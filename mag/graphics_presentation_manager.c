#define COBJMACROS

#include "graphics_presentation_manager.h"
#include "graphics_dcomp.h"
#include "dcompabi.h"

#include <Presentation.h>

#define MAG_PRESENTATION_MAX_BUFFERS 16

static const IID IID_MAG_IPresentationFactory =
  { 0x8fb37b58, 0x1d74, 0x4f64, { 0xa4, 0x9c, 0x1f, 0x97, 0xa8, 0x0a, 0x2e, 0xc0 } };
static const IID IID_MAG_ICompositionFramePresentStatistics =
  { 0xab41d127, 0xc101, 0x4c0a, { 0x91, 0x1d, 0xf9, 0xf2, 0xe9, 0xd0, 0x8e, 0x64 } };

typedef HRESULT (WINAPI *MAGCREATEPRESENTATIONFACTORY)(
  IUnknown* d3dDevice,
  REFIID riid,
  void** presentationFactory);

typedef struct MAGPRESENTATIONBUFFERENTRY
{
  ID3D11Texture2D*   texture;
  IPresentationBuffer* presentationBuffer;
  HANDLE             availableEvent;
} MAGPRESENTATIONBUFFERENTRY;

struct MAGPRESENTATIONMANAGERPRESENTER
{
  HMODULE                    dcompModule;
  IPresentationFactory*      factory;
  IPresentationManager*      manager;
  IPresentationSurface*      surface;
  MAGPRESENTATIONBUFFERENTRY buffers[MAG_PRESENTATION_MAX_BUFFERS];
  UINT                       bufferCount;
  MAGDCOMPPRESENTER*         composition;
  HANDLE                     compositionSurfaceHandle;
  HANDLE                     lostEvent;
  HANDLE                     statisticsEvent;
  SIZE                       reservoirSize;
  SIZE                       clientSize;
  MAGPRESENTATIONSETTINGS    settings;
  UINT                       nextBuffer;
  UINT64                     resourceGeneration;
  UINT64                     lastSubmittedPresentId;
  MAGPRESENTATIONTARGET      observedTarget;
  BOOL                       haveObservedTarget;
};

static void magPresentationManagerSetReason(
  LPTSTR reason,
  UINT reasonCount,
  LPCTSTR value)
{
    if (reason && reasonCount)
    {
      lstrcpyn(reason, value, reasonCount);
    }
}

static BOOL magPresentationManagerRequiresIndependentFlip(
  MAGPRESENTATIONTARGET target)
{
    return MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP == target ||
      MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP == target;
}

static DXGI_ALPHA_MODE magPresentationManagerAlphaMode(
  const MAGPRESENTATIONSETTINGS* settings)
{
    return MAG_LAYER_ALPHA_PER_PIXEL_PREMULTIPLIED == settings->alphaMode
      ? DXGI_ALPHA_MODE_PREMULTIPLIED
      : DXGI_ALPHA_MODE_IGNORE;
}

static void magPresentationManagerDrainStatistics(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    if (!presenter || !presenter->manager || !presenter->statisticsEvent ||
        WAIT_OBJECT_0 != WaitForSingleObject(presenter->statisticsEvent, 0))
    {
      return;
    }

    for (;;)
    {
      IPresentStatistics* statistics = NULL;
      HRESULT hr = IPresentationManager_GetNextPresentStatistics(
        presenter->manager,
        &statistics);
      if (FAILED(hr) || !statistics)
      {
        break;
      }

      if (PresentStatisticsKind_IndependentFlipFrame ==
          IPresentStatistics_GetKind(statistics))
      {
        presenter->observedTarget = MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP;
        presenter->haveObservedTarget = TRUE;
      }
      else if (PresentStatisticsKind_CompositionFrame ==
               IPresentStatistics_GetKind(statistics))
      {
        ICompositionFramePresentStatistics* compositionStatistics = NULL;
        if (SUCCEEDED(IPresentStatistics_QueryInterface(
              statistics,
              &IID_MAG_ICompositionFramePresentStatistics,
              (void**)&compositionStatistics)))
        {
          UINT instanceCount = 0;
          const CompositionFrameDisplayInstance* instances = NULL;
          UINT i;

          ICompositionFramePresentStatistics_GetDisplayInstanceArray(
            compositionStatistics,
            &instanceCount,
            &instances);
          presenter->observedTarget = MAG_PRESENT_COMPOSED_FLIP;
          for (i = 0; i < instanceCount; ++i)
          {
            if (CompositionFrameInstanceKind_ScanoutOnScreen ==
                instances[i].instanceKind)
            {
              presenter->observedTarget =
                MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP;
              break;
            }
          }
          presenter->haveObservedTarget = TRUE;
          ICompositionFramePresentStatistics_Release(compositionStatistics);
        }
      }
      IPresentStatistics_Release(statistics);
    }
}

BOOL magPresentationManagerPresenterCreate(
  HWND hWnd,
  ID3D11Device* device,
  SIZE reservoirSize,
  const MAGPRESENTATIONSETTINGS* settings,
  MAGPRESENTATIONMANAGERPRESENTER** presenterOut,
  LPTSTR reason,
  UINT reasonCount)
{
    MAGPRESENTATIONMANAGERPRESENTER* presenter;
    MAGCREATEPRESENTATIONFACTORY createFactory;
    HRESULT hr;
    UINT i;

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!hWnd || !device || !settings || !presenterOut ||
        reservoirSize.cx < 1 || reservoirSize.cy < 1 ||
        settings->bufferCount < 2 ||
        settings->bufferCount > MAG_PRESENTATION_MAX_BUFFERS)
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("Invalid Presentation Manager parameters."));
      return FALSE;
    }
    *presenterOut = NULL;

    presenter = (MAGPRESENTATIONMANAGERPRESENTER*)HeapAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      sizeof(*presenter));
    if (!presenter)
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("Presentation Manager state allocation failed."));
      return FALSE;
    }
    presenter->reservoirSize = reservoirSize;
    presenter->clientSize = reservoirSize;
    presenter->settings = *settings;
    presenter->bufferCount = settings->bufferCount;
    presenter->observedTarget = MAG_PRESENT_AUTO;

    presenter->dcompModule = LoadLibrary(TEXT("dcomp.dll"));
    createFactory = presenter->dcompModule
      ? (MAGCREATEPRESENTATIONFACTORY)GetProcAddress(
          presenter->dcompModule,
          "CreatePresentationFactory")
      : NULL;
    if (!createFactory)
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("CreatePresentationFactory is unavailable."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    hr = createFactory(
      (IUnknown*)device,
      &IID_MAG_IPresentationFactory,
      (void**)&presenter->factory);
    if (FAILED(hr) || !presenter->factory)
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("The selected Direct3D device cannot create a presentation factory."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }
    if (magPresentationManagerRequiresIndependentFlip(settings->target)
          ? !IPresentationFactory_IsPresentationSupportedWithIndependentFlip(
              presenter->factory)
          : !IPresentationFactory_IsPresentationSupported(presenter->factory))
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        magPresentationManagerRequiresIndependentFlip(settings->target)
          ? TEXT("The selected adapter cannot allocate displayable Presentation Manager surfaces for Independent Flip.")
          : TEXT("The selected adapter does not support Presentation Manager surfaces."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    hr = IPresentationFactory_CreatePresentationManager(
      presenter->factory,
      &presenter->manager);
    if (SUCCEEDED(hr))
    {
      hr = DCompositionCreateSurfaceHandle(
        MAG_COMPOSITIONOBJECT_ALL_ACCESS,
        NULL,
        &presenter->compositionSurfaceHandle);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_CreatePresentationSurface(
        presenter->manager,
        presenter->compositionSurfaceHandle,
        &presenter->surface);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationSurface_SetAlphaMode(
        presenter->surface,
        magPresentationManagerAlphaMode(settings));
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationSurface_SetColorSpace(
        presenter->surface,
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    }
    if (SUCCEEDED(hr))
    {
      IPresentationSurface_SetTag(presenter->surface, (UINT_PTR)presenter);
      hr = IPresentationManager_EnablePresentStatisticsKind(
        presenter->manager,
        PresentStatisticsKind_PresentStatus,
        (boolean)TRUE);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_EnablePresentStatisticsKind(
        presenter->manager,
        PresentStatisticsKind_CompositionFrame,
        (boolean)TRUE);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_EnablePresentStatisticsKind(
        presenter->manager,
        PresentStatisticsKind_IndependentFlipFrame,
        (boolean)TRUE);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_GetLostEvent(
        presenter->manager,
        &presenter->lostEvent);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_GetPresentStatisticsAvailableEvent(
        presenter->manager,
        &presenter->statisticsEvent);
    }
    if (FAILED(hr))
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("Presentation Manager queue/statistics initialization failed."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    for (i = 0; i < presenter->bufferCount; ++i)
    {
      D3D11_TEXTURE2D_DESC textureDesc;
      MAGPRESENTATIONBUFFERENTRY* entry = &presenter->buffers[i];

      ZeroMemory(&textureDesc, sizeof(textureDesc));
      textureDesc.Width = (UINT)reservoirSize.cx;
      textureDesc.Height = (UINT)reservoirSize.cy;
      textureDesc.MipLevels = 1;
      textureDesc.ArraySize = 1;
      textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      textureDesc.SampleDesc.Count = 1;
      textureDesc.Usage = D3D11_USAGE_DEFAULT;
      textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE |
        D3D11_BIND_RENDER_TARGET;
      textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED |
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
        D3D11_RESOURCE_MISC_SHARED_DISPLAYABLE;
      hr = ID3D11Device_CreateTexture2D(
        device,
        &textureDesc,
        NULL,
        &entry->texture);
      if (SUCCEEDED(hr))
      {
        hr = IPresentationManager_AddBufferFromResource(
          presenter->manager,
          (IUnknown*)entry->texture,
          &entry->presentationBuffer);
      }
      if (SUCCEEDED(hr))
      {
        hr = IPresentationBuffer_GetAvailableEvent(
          entry->presentationBuffer,
          &entry->availableEvent);
      }
      if (FAILED(hr))
      {
        magPresentationManagerSetReason(
          reason,
          reasonCount,
          TEXT("A displayable Presentation Manager buffer could not be allocated or registered."));
        magPresentationManagerPresenterDestroy(presenter);
        return FALSE;
      }
    }

    if (!magDCompPresenterCreateFromSurfaceHandle(
          hWnd,
          presenter->compositionSurfaceHandle,
          &presenter->composition))
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("The Presentation Manager surface could not be attached to the DirectComposition tree."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }
    if (MAG_LAYER_ALPHA_CONSTANT == settings->alphaMode &&
        !magDCompPresenterSetOpacity(
          presenter->composition,
          (FLOAT)settings->constantAlpha / 255.0f))
    {
      magPresentationManagerSetReason(
        reason,
        reasonCount,
        TEXT("DirectComposition rejected the requested constant opacity."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    presenter->resourceGeneration = 1;
    *presenterOut = presenter;
    return TRUE;
}

void magPresentationManagerPresenterDestroy(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    UINT i;

    if (!presenter)
    {
      return;
    }
    if (presenter->manager &&
        IPresentationManager_GetNextPresentId(presenter->manager) > 1)
    {
      IPresentationManager_CancelPresentsFrom(presenter->manager, 1);
    }
    if (presenter->surface)
    {
      IPresentationSurface_SetBuffer(presenter->surface, NULL);
      if (presenter->manager)
      {
        IPresentationManager_Present(presenter->manager);
      }
    }
    magDCompPresenterDestroy(presenter->composition);
    presenter->composition = NULL;
    for (i = 0; i < presenter->bufferCount; ++i)
    {
      MAGPRESENTATIONBUFFERENTRY* entry = &presenter->buffers[i];
      if (entry->availableEvent)
      {
        CloseHandle(entry->availableEvent);
      }
      if (entry->presentationBuffer)
      {
        IPresentationBuffer_Release(entry->presentationBuffer);
      }
      if (entry->texture)
      {
        ID3D11Texture2D_Release(entry->texture);
      }
    }
    if (presenter->surface)
    {
      IPresentationSurface_Release(presenter->surface);
    }
    if (presenter->manager)
    {
      IPresentationManager_Release(presenter->manager);
    }
    if (presenter->factory)
    {
      IPresentationFactory_Release(presenter->factory);
    }
    if (presenter->statisticsEvent)
    {
      CloseHandle(presenter->statisticsEvent);
    }
    if (presenter->lostEvent)
    {
      CloseHandle(presenter->lostEvent);
    }
    if (presenter->compositionSurfaceHandle)
    {
      CloseHandle(presenter->compositionSurfaceHandle);
    }
    if (presenter->dcompModule)
    {
      FreeLibrary(presenter->dcompModule);
    }
    HeapFree(GetProcessHeap(), 0, presenter);
}

BOOL magPresentationManagerPresenterSetEnabled(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  BOOL enabled)
{
    return presenter && magDCompPresenterSetEnabled(
      presenter->composition,
      enabled);
}

BOOL magPresentationManagerPresenterResize(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  SIZE clientSize)
{
    if (!presenter || clientSize.cx < 1 || clientSize.cy < 1 ||
        clientSize.cx > presenter->reservoirSize.cx ||
        clientSize.cy > presenter->reservoirSize.cy)
    {
      return FALSE;
    }
    presenter->clientSize = clientSize;
    return TRUE;
}

BOOL magPresentationManagerPresenterAcquire(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  BOOL allowWait,
  ID3D11Texture2D** textureOut,
  UINT* bufferIndexOut)
{
    UINT attempt;

    if (!presenter || !textureOut || !bufferIndexOut || !presenter->bufferCount)
    {
      return FALSE;
    }
    *textureOut = NULL;
    *bufferIndexOut = 0;
    magPresentationManagerDrainStatistics(presenter);
    if (presenter->lostEvent &&
        WAIT_OBJECT_0 == WaitForSingleObject(presenter->lostEvent, 0))
    {
      return FALSE;
    }

    for (attempt = 0; attempt < (allowWait ? 2U : 1U); ++attempt)
    {
      UINT offset;
      for (offset = 0; offset < presenter->bufferCount; ++offset)
      {
        UINT index = (presenter->nextBuffer + offset) % presenter->bufferCount;
        boolean available = (boolean)FALSE;
        if (SUCCEEDED(IPresentationBuffer_IsAvailable(
              presenter->buffers[index].presentationBuffer,
              &available)) && available)
        {
          *textureOut = presenter->buffers[index].texture;
          ID3D11Texture2D_AddRef(*textureOut);
          *bufferIndexOut = index;
          presenter->nextBuffer = (index + 1U) % presenter->bufferCount;
          return TRUE;
        }
      }
      if (allowWait)
      {
        HANDLE events[MAG_PRESENTATION_MAX_BUFFERS + 1];
        DWORD eventCount = 0;
        DWORD wait;
        UINT i;

        if (presenter->lostEvent)
        {
          events[eventCount++] = presenter->lostEvent;
        }
        for (i = 0; i < presenter->bufferCount; ++i)
        {
          events[eventCount++] = presenter->buffers[i].availableEvent;
        }
        wait = WaitForMultipleObjects(eventCount, events, FALSE, 1000);
        if (WAIT_FAILED == wait || WAIT_TIMEOUT == wait ||
            (presenter->lostEvent && WAIT_OBJECT_0 == wait))
        {
          return FALSE;
        }
      }
    }
    return FALSE;
}

BOOL magPresentationManagerPresenterPresent(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  UINT bufferIndex,
  SIZE contentSize,
  const MAGPRESENTINTENT* intent)
{
    RECT sourceRect;
    SystemInterruptTime targetTime;
    HRESULT hr;

    if (!presenter || bufferIndex >= presenter->bufferCount ||
        contentSize.cx < 1 || contentSize.cy < 1 ||
        contentSize.cx > presenter->reservoirSize.cx ||
        contentSize.cy > presenter->reservoirSize.cy)
    {
      return FALSE;
    }
    if (intent && intent->restartSequence)
    {
      IPresentationManager_CancelPresentsFrom(presenter->manager, 1);
    }

    SetRect(&sourceRect, 0, 0, contentSize.cx, contentSize.cy);
    presenter->lastSubmittedPresentId =
      IPresentationManager_GetNextPresentId(presenter->manager);
    hr = IPresentationSurface_SetSourceRect(presenter->surface, &sourceRect);
    if (SUCCEEDED(hr))
    {
      hr = IPresentationSurface_SetBuffer(
        presenter->surface,
        presenter->buffers[bufferIndex].presentationBuffer);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_ForceVSyncInterrupt(
        presenter->manager,
        (boolean)(intent && intent->synchronize));
    }
    if (SUCCEEDED(hr))
    {
      ZeroMemory(&targetTime, sizeof(targetTime));
      hr = IPresentationManager_SetTargetTime(presenter->manager, targetTime);
    }
    if (SUCCEEDED(hr))
    {
      hr = IPresentationManager_Present(presenter->manager);
    }
    magPresentationManagerDrainStatistics(presenter);
    return SUCCEEDED(hr);
}

HANDLE magPresentationManagerPresenterGetFrameWaitHandle(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    if (!presenter || !presenter->bufferCount)
    {
      return NULL;
    }
    return presenter->buffers[presenter->nextBuffer].availableEvent;
}

BOOL magPresentationManagerPresenterGetNextEstimatedFrameTime(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  LONGLONG* frameTime)
{
    return presenter && magDCompPresenterGetNextEstimatedFrameTime(
      presenter->composition,
      frameTime);
}

BOOL magPresentationManagerPresenterGetObservedTarget(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  UINT* target)
{
    if (!presenter || !target)
    {
      return FALSE;
    }
    magPresentationManagerDrainStatistics(presenter);
    if (!presenter->haveObservedTarget)
    {
      return FALSE;
    }
    *target = (UINT)presenter->observedTarget;
    return TRUE;
}

UINT64 magPresentationManagerPresenterGetResourceGeneration(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    return presenter ? presenter->resourceGeneration : 0;
}
