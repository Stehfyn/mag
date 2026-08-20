#include "graphics_presentation_manager.h"
#include "graphics_dcomp.h"

#include <Presentation.h>
#include <dcomp.h>
#include <new>
#include <vector>
#include <wrl/client.h>

#pragma comment(lib, "dcomp")

using Microsoft::WRL::ComPtr;

typedef HRESULT (WINAPI *MAGCREATEPRESENTATIONFACTORY)(
  IUnknown* d3dDevice,
  REFIID riid,
  void** presentationFactory);

struct MAGPRESENTATIONBUFFERENTRY
{
  ComPtr<ID3D11Texture2D> texture;
  ComPtr<IPresentationBuffer> presentationBuffer;
  HANDLE availableEvent = nullptr;
};

struct MAGPRESENTATIONMANAGERPRESENTER
{
  HMODULE dcompModule = nullptr;
  ComPtr<IPresentationFactory> factory;
  ComPtr<IPresentationManager> manager;
  ComPtr<IPresentationSurface> surface;
  std::vector<MAGPRESENTATIONBUFFERENTRY> buffers;
  MAGDCOMPPRESENTER* composition = nullptr;
  HANDLE compositionSurfaceHandle = nullptr;
  HANDLE lostEvent = nullptr;
  HANDLE statisticsEvent = nullptr;
  SIZE reservoirSize = {};
  SIZE clientSize = {};
  MAGPRESENTATIONSETTINGS settings = {};
  UINT nextBuffer = 0;
  UINT64 resourceGeneration = 0;
  UINT64 lastSubmittedPresentId = 0;
  MAGPRESENTATIONTARGET observedTarget = MAG_PRESENT_AUTO;
  bool haveObservedTarget = false;
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

static bool magPresentationManagerRequiresIndependentFlip(
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
      ComPtr<IPresentStatistics> statistics;
      HRESULT hr = presenter->manager->GetNextPresentStatistics(&statistics);
      if (FAILED(hr) || !statistics)
      {
        break;
      }

      if (PresentStatisticsKind_IndependentFlipFrame == statistics->GetKind())
      {
        presenter->observedTarget = MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP;
        presenter->haveObservedTarget = true;
      }
      else if (PresentStatisticsKind_CompositionFrame == statistics->GetKind())
      {
        ComPtr<ICompositionFramePresentStatistics> compositionStatistics;
        if (SUCCEEDED(statistics.As(&compositionStatistics)))
        {
          UINT instanceCount = 0;
          const CompositionFrameDisplayInstance* instances = nullptr;
          compositionStatistics->GetDisplayInstanceArray(&instanceCount, &instances);
          presenter->observedTarget = MAG_PRESENT_COMPOSED_FLIP;
          for (UINT i = 0; i < instanceCount; ++i)
          {
            if (CompositionFrameInstanceKind_ScanoutOnScreen == instances[i].instanceKind)
            {
              presenter->observedTarget =
                MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP;
              break;
            }
          }
          presenter->haveObservedTarget = true;
        }
      }
    }
}

extern "C" BOOL magPresentationManagerPresenterCreate(
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

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!hWnd || !device || !settings || !presenterOut ||
        reservoirSize.cx < 1 || reservoirSize.cy < 1 ||
        settings->bufferCount < 2 || settings->bufferCount > 16)
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("Invalid Presentation Manager parameters."));
      return FALSE;
    }
    *presenterOut = nullptr;

    presenter = new (std::nothrow) MAGPRESENTATIONMANAGERPRESENTER;
    if (!presenter)
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("Presentation Manager state allocation failed."));
      return FALSE;
    }
    presenter->reservoirSize = reservoirSize;
    presenter->clientSize = reservoirSize;
    presenter->settings = *settings;

    presenter->dcompModule = LoadLibrary(TEXT("dcomp.dll"));
    createFactory = presenter->dcompModule
      ? reinterpret_cast<MAGCREATEPRESENTATIONFACTORY>(
          GetProcAddress(presenter->dcompModule, "CreatePresentationFactory"))
      : nullptr;
    if (!createFactory)
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("CreatePresentationFactory is unavailable."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    hr = createFactory(
      device,
      __uuidof(IPresentationFactory),
      reinterpret_cast<void**>(presenter->factory.GetAddressOf()));
    if (FAILED(hr) || !presenter->factory)
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("The selected Direct3D device cannot create a presentation factory."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }
    if (magPresentationManagerRequiresIndependentFlip(settings->target)
          ? !presenter->factory->IsPresentationSupportedWithIndependentFlip()
          : !presenter->factory->IsPresentationSupported())
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

    hr = presenter->factory->CreatePresentationManager(&presenter->manager);
    if (SUCCEEDED(hr))
    {
      hr = DCompositionCreateSurfaceHandle(
        COMPOSITIONOBJECT_ALL_ACCESS,
        nullptr,
        &presenter->compositionSurfaceHandle);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->CreatePresentationSurface(
        presenter->compositionSurfaceHandle,
        &presenter->surface);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->surface->SetAlphaMode(
        magPresentationManagerAlphaMode(settings));
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->surface->SetColorSpace(
        DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709);
    }
    if (SUCCEEDED(hr))
    {
      presenter->surface->SetTag(reinterpret_cast<UINT_PTR>(presenter));
      hr = presenter->manager->EnablePresentStatisticsKind(
        PresentStatisticsKind_PresentStatus,
        true);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->EnablePresentStatisticsKind(
        PresentStatisticsKind_CompositionFrame,
        true);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->EnablePresentStatisticsKind(
        PresentStatisticsKind_IndependentFlipFrame,
        true);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->GetLostEvent(&presenter->lostEvent);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->GetPresentStatisticsAvailableEvent(
        &presenter->statisticsEvent);
    }
    if (FAILED(hr))
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("Presentation Manager queue/statistics initialization failed."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    presenter->buffers.resize(settings->bufferCount);
    for (UINT i = 0; i < settings->bufferCount; ++i)
    {
      D3D11_TEXTURE2D_DESC textureDesc = {};
      MAGPRESENTATIONBUFFERENTRY& entry = presenter->buffers[i];

      textureDesc.Width = static_cast<UINT>(reservoirSize.cx);
      textureDesc.Height = static_cast<UINT>(reservoirSize.cy);
      textureDesc.MipLevels = 1;
      textureDesc.ArraySize = 1;
      textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      textureDesc.SampleDesc.Count = 1;
      textureDesc.Usage = D3D11_USAGE_DEFAULT;
      textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
      textureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED |
        D3D11_RESOURCE_MISC_SHARED_NTHANDLE |
        D3D11_RESOURCE_MISC_SHARED_DISPLAYABLE;
      hr = device->CreateTexture2D(&textureDesc, nullptr, &entry.texture);
      if (SUCCEEDED(hr))
      {
        hr = presenter->manager->AddBufferFromResource(
          entry.texture.Get(),
          &entry.presentationBuffer);
      }
      if (SUCCEEDED(hr))
      {
        hr = entry.presentationBuffer->GetAvailableEvent(&entry.availableEvent);
      }
      if (FAILED(hr))
      {
        magPresentationManagerSetReason(reason, reasonCount, TEXT("A displayable Presentation Manager buffer could not be allocated or registered."));
        magPresentationManagerPresenterDestroy(presenter);
        return FALSE;
      }
    }

    if (!magDCompPresenterCreateFromSurfaceHandle(
          hWnd,
          presenter->compositionSurfaceHandle,
          &presenter->composition))
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("The Presentation Manager surface could not be attached to the DirectComposition tree."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }
    if (MAG_LAYER_ALPHA_CONSTANT == settings->alphaMode &&
        !magDCompPresenterSetOpacity(
          presenter->composition,
          static_cast<FLOAT>(settings->constantAlpha) / 255.0f))
    {
      magPresentationManagerSetReason(reason, reasonCount, TEXT("DirectComposition rejected the requested constant opacity."));
      magPresentationManagerPresenterDestroy(presenter);
      return FALSE;
    }

    presenter->resourceGeneration = 1;
    *presenterOut = presenter;
    return TRUE;
}

extern "C" void magPresentationManagerPresenterDestroy(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    if (!presenter)
    {
      return;
    }
    if (presenter->manager && presenter->manager->GetNextPresentId() > 1)
    {
      presenter->manager->CancelPresentsFrom(1);
    }
    if (presenter->surface)
    {
      presenter->surface->SetBuffer(nullptr);
      if (presenter->manager)
      {
        presenter->manager->Present();
      }
    }
    magDCompPresenterDestroy(presenter->composition);
    presenter->composition = nullptr;
    for (MAGPRESENTATIONBUFFERENTRY& entry : presenter->buffers)
    {
      if (entry.availableEvent)
      {
        CloseHandle(entry.availableEvent);
        entry.availableEvent = nullptr;
      }
      entry.presentationBuffer.Reset();
      entry.texture.Reset();
    }
    presenter->buffers.clear();
    presenter->surface.Reset();
    presenter->manager.Reset();
    presenter->factory.Reset();
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
    delete presenter;
}

extern "C" BOOL magPresentationManagerPresenterSetEnabled(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  BOOL enabled)
{
    return presenter && magDCompPresenterSetEnabled(
      presenter->composition,
      enabled);
}

extern "C" BOOL magPresentationManagerPresenterResize(
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

extern "C" BOOL magPresentationManagerPresenterAcquire(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  BOOL allowWait,
  ID3D11Texture2D** textureOut,
  UINT* bufferIndexOut)
{
    if (!presenter || !textureOut || !bufferIndexOut || presenter->buffers.empty())
    {
      return FALSE;
    }
    *textureOut = nullptr;
    *bufferIndexOut = 0;
    magPresentationManagerDrainStatistics(presenter);
    if (presenter->lostEvent &&
        WAIT_OBJECT_0 == WaitForSingleObject(presenter->lostEvent, 0))
    {
      return FALSE;
    }

    for (UINT attempt = 0; attempt < (allowWait ? 2U : 1U); ++attempt)
    {
      for (UINT offset = 0; offset < presenter->buffers.size(); ++offset)
      {
        UINT index = (presenter->nextBuffer + offset) %
          static_cast<UINT>(presenter->buffers.size());
        boolean available = false;
        if (SUCCEEDED(presenter->buffers[index].presentationBuffer->IsAvailable(&available)) &&
            available)
        {
          *textureOut = presenter->buffers[index].texture.Get();
          (*textureOut)->AddRef();
          *bufferIndexOut = index;
          presenter->nextBuffer = (index + 1U) %
            static_cast<UINT>(presenter->buffers.size());
          return TRUE;
        }
      }
      if (allowWait)
      {
        std::vector<HANDLE> events;
        if (presenter->lostEvent)
        {
          events.push_back(presenter->lostEvent);
        }
        for (const MAGPRESENTATIONBUFFERENTRY& entry : presenter->buffers)
        {
          events.push_back(entry.availableEvent);
        }
        DWORD wait = WaitForMultipleObjects(
          static_cast<DWORD>(events.size()),
          events.data(),
          FALSE,
          1000);
        if (WAIT_FAILED == wait || WAIT_TIMEOUT == wait ||
            (presenter->lostEvent && WAIT_OBJECT_0 == wait))
        {
          return FALSE;
        }
      }
    }
    return FALSE;
}

extern "C" BOOL magPresentationManagerPresenterPresent(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  UINT bufferIndex,
  SIZE contentSize,
  const MAGPRESENTINTENT* intent)
{
    RECT sourceRect;
    HRESULT hr;

    if (!presenter || bufferIndex >= presenter->buffers.size() ||
        contentSize.cx < 1 || contentSize.cy < 1 ||
        contentSize.cx > presenter->reservoirSize.cx ||
        contentSize.cy > presenter->reservoirSize.cy)
    {
      return FALSE;
    }
    if (intent && intent->restartSequence)
    {
      presenter->manager->CancelPresentsFrom(1);
    }

    sourceRect = { 0, 0, contentSize.cx, contentSize.cy };
    presenter->lastSubmittedPresentId = presenter->manager->GetNextPresentId();
    hr = presenter->surface->SetSourceRect(&sourceRect);
    if (SUCCEEDED(hr))
    {
      hr = presenter->surface->SetBuffer(
        presenter->buffers[bufferIndex].presentationBuffer.Get());
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->ForceVSyncInterrupt(
        intent && intent->synchronize);
    }
    if (SUCCEEDED(hr))
    {
      SystemInterruptTime targetTime = {};
      hr = presenter->manager->SetTargetTime(targetTime);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->manager->Present();
    }
    magPresentationManagerDrainStatistics(presenter);
    return SUCCEEDED(hr);
}

extern "C" HANDLE magPresentationManagerPresenterGetFrameWaitHandle(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    if (!presenter || presenter->buffers.empty())
    {
      return nullptr;
    }
    return presenter->buffers[presenter->nextBuffer].availableEvent;
}

extern "C" BOOL magPresentationManagerPresenterGetNextEstimatedFrameTime(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  LONGLONG* frameTime)
{
    return presenter && magDCompPresenterGetNextEstimatedFrameTime(
      presenter->composition,
      frameTime);
}

extern "C" BOOL magPresentationManagerPresenterGetObservedTarget(
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
    *target = static_cast<UINT>(presenter->observedTarget);
    return TRUE;
}

extern "C" UINT64 magPresentationManagerPresenterGetResourceGeneration(
  MAGPRESENTATIONMANAGERPRESENTER* presenter)
{
    return presenter ? presenter->resourceGeneration : 0;
}
