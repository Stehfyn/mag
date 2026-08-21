#include "framework.h"
#include "graphics.h"
#include "graphics_dcomp.h"
#include "graphics_presentation_manager.h"
#include "presentation.h"

#include <d3d11.h>
#include <dxgi1_3.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")

typedef struct MAGD3D11STATE
{
  ID3D11Device*        device;
  ID3D11DeviceContext* context;
  IDXGISwapChain2*     swapChain;
  MAGDCOMPPRESENTER*   composition;
  MAGPRESENTATIONMANAGERPRESENTER* presentationManager;
  HANDLE               frameWaitHandle;
  ID3D11Texture2D*     backBuffer;
  ID3D11Texture2D*     uploadTexture;
  MAGCPUCOMPOSITOR     compositor;
  UINT                 width;
  UINT                 height;
  UINT                 capacityWidth;
  UINT                 capacityHeight;
  UINT64               resourceGeneration;
  UINT                 bufferCount;
  UINT                 syncInterval;
  UINT                 presentFlags;
  UINT                 swapChainFlags;
  MAGPRESENTATIONTARGET configuredTarget;
  BOOL                 warp;
  BOOL                 flipModel;
  BOOL                 waitableSwapChain;
  BOOL                 compositionHost;
  BOOL                 presentationManagerHost;
  BOOL                 presentationEnabled;
} MAGD3D11STATE;

static void magGraphicsD3D11ReleaseTexture(ID3D11Texture2D** texture)
{
    if (*texture)
    {
      ID3D11Texture2D_Release(*texture);
      *texture = NULL;
    }
}

static BOOL magGraphicsD3D11IsAvailable(LPTSTR reason, UINT reasonCount)
{
    HMODULE module = LoadLibrary(TEXT("d3d11.dll"));

    if (module)
    {
      FreeLibrary(module);
      if (reason && reasonCount)
      {
        reason[0] = TEXT('\0');
      }
      return TRUE;
    }

    if (reason && reasonCount)
    {
      lstrcpyn(reason, TEXT("Direct3D 11 is not installed."), reasonCount);
    }
    return FALSE;
}

static BOOL magGraphicsD3D11CreateFrameResources(MAGD3D11STATE* state, SIZE reservoirSize)
{
    D3D11_TEXTURE2D_DESC textureDesc = { 0 };
    HRESULT hr;

    if (reservoirSize.cx < 1 || reservoirSize.cy < 1)
    {
      return FALSE;
    }

    magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
    magGraphicsD3D11ReleaseTexture(&state->backBuffer);

    hr = IDXGISwapChain2_GetBuffer(
      state->swapChain,
      0,
      &IID_ID3D11Texture2D,
      (void**)&state->backBuffer);
    if (FAILED(hr))
    {
      return FALSE;
    }

    textureDesc.Width = (UINT)reservoirSize.cx;
    textureDesc.Height = (UINT)reservoirSize.cy;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    hr = ID3D11Device_CreateTexture2D(
      state->device,
      &textureDesc,
      NULL,
      &state->uploadTexture);
    if (FAILED(hr))
    {
      magGraphicsD3D11ReleaseTexture(&state->backBuffer);
      return FALSE;
    }

    state->capacityWidth = (UINT)reservoirSize.cx;
    state->capacityHeight = (UINT)reservoirSize.cy;
    ++state->resourceGeneration;
    return TRUE;
}

static HRESULT magGraphicsD3D11CreateDevice(
  HWND hWnd,
  SIZE clientSize,
  const MAGPRESENTATIONSETTINGS* presentation,
  MAGD3D11STATE* state)
{
    DXGI_SWAP_CHAIN_DESC1 swapDesc = { 0 };
    IDXGIDevice* dxgiDevice = NULL;
    IDXGIDevice1* dxgiDevice1 = NULL;
    IDXGIAdapter1* selectedAdapter = NULL;
    IDXGIAdapter* deviceAdapter = NULL;
    IDXGIFactory2* factory = NULL;
    IDXGIFactory5* factory5 = NULL;
    IDXGISwapChain1* swapChain1 = NULL;
    SIZE reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
    const D3D_FEATURE_LEVEL featureLevels[] =
    {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selectedFeatureLevel;
    D3D_DRIVER_TYPE driverType;
    BOOL allowTearing = FALSE;
    BOOL flipModel;
    BOOL waitableSwapChain;
    BOOL compositionHost;
    BOOL presentationManagerHost;
    UINT deviceFlags;
    HRESULT hr;

    if (!presentation)
    {
      return E_INVALIDARG;
    }
    state->warp = MAG_HARDWARE_ADAPTER_WARP == presentation->hardware.mode;
    state->configuredTarget = presentation->target;
    flipModel = MAG_PRESENT_COMPOSED_COPY_GPU_GDI != presentation->target;
    waitableSwapChain = flipModel &&
      MAG_WAITABLE_SWAP_CHAIN_ENABLED == presentation->waitableSwapChainMode;
    state->flipModel = flipModel;
    state->waitableSwapChain = waitableSwapChain;
    if (!flipModel)
    {
      reservoirSize = clientSize;
    }
    compositionHost = MAG_HOST_DIRECTCOMPOSITION == presentation->host;
    presentationManagerHost = MAG_HOST_PRESENTATION_MANAGER == presentation->host;
    state->compositionHost = compositionHost;
    state->presentationManagerHost = presentationManagerHost;
    deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (presentationManagerHost)
    {
      deviceFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED |
        D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
    }
    driverType = state->warp ? D3D_DRIVER_TYPE_WARP : D3D_DRIVER_TYPE_UNKNOWN;
    if (!state->warp &&
        !magAdapterOpenDxgi(presentation->hardware.adapterLuid, &selectedAdapter))
    {
      return DXGI_ERROR_NOT_FOUND;
    }

    swapDesc.Width = (UINT)reservoirSize.cx;
    swapDesc.Height = (UINT)reservoirSize.cy;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = presentation->bufferCount;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.SwapEffect = flipModel
      ? DXGI_SWAP_EFFECT_FLIP_DISCARD
      : DXGI_SWAP_EFFECT_DISCARD;
    swapDesc.AlphaMode = compositionHost &&
      MAG_LAYER_ALPHA_OPAQUE != presentation->alphaMode
      ? DXGI_ALPHA_MODE_PREMULTIPLIED
      : DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = waitableSwapChain
      ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
      : 0;

    hr = D3D11CreateDevice(
      (IDXGIAdapter*)selectedAdapter,
      driverType,
      NULL,
      deviceFlags,
      featureLevels,
      ARRAYSIZE(featureLevels),
      D3D11_SDK_VERSION,
      &state->device,
      &selectedFeatureLevel,
      &state->context);

    if (E_INVALIDARG == hr)
    {
      hr = D3D11CreateDevice(
        (IDXGIAdapter*)selectedAdapter,
        driverType,
        NULL,
        deviceFlags,
        featureLevels + 1,
        ARRAYSIZE(featureLevels) - 1,
        D3D11_SDK_VERSION,
        &state->device,
        &selectedFeatureLevel,
        &state->context);
    }

    if (SUCCEEDED(hr) && presentationManagerHost)
    {
      TCHAR managerReason[256];

      if (!magPresentationManagerPresenterCreate(
            hWnd,
            state->device,
            reservoirSize,
            presentation,
            &state->presentationManager,
            managerReason,
            ARRAYSIZE(managerReason)))
      {
        hr = E_FAIL;
      }
      else
      {
        state->capacityWidth = (UINT)reservoirSize.cx;
        state->capacityHeight = (UINT)reservoirSize.cy;
        state->bufferCount = presentation->bufferCount;
        state->syncInterval = presentation->syncInterval;
        state->presentFlags = 0;
        state->presentationEnabled = TRUE;
      }
      if (selectedAdapter)
      {
        IDXGIAdapter1_Release(selectedAdapter);
      }
      return hr;
    }

    if (SUCCEEDED(hr))
    {
      hr = ID3D11Device_QueryInterface(state->device, &IID_IDXGIDevice, (void**)&dxgiDevice);
    }
    if (SUCCEEDED(hr) && !waitableSwapChain)
    {
      hr = IDXGIDevice_QueryInterface(
        dxgiDevice,
        &IID_IDXGIDevice1,
        (void**)&dxgiDevice1);
      if (SUCCEEDED(hr))
      {
        hr = IDXGIDevice1_SetMaximumFrameLatency(
          dxgiDevice1,
          presentation->maximumFrameLatency);
      }
    }
    if (SUCCEEDED(hr))
    {
      hr = IDXGIDevice_GetAdapter(dxgiDevice, &deviceAdapter);
    }
    if (SUCCEEDED(hr))
    {
      hr = IDXGIAdapter_GetParent(deviceAdapter, &IID_IDXGIFactory2, (void**)&factory);
    }
    if (SUCCEEDED(hr) && flipModel && presentation->allowTearing &&
        0 == presentation->syncInterval &&
        SUCCEEDED(IDXGIFactory2_QueryInterface(factory, &IID_IDXGIFactory5, (void**)&factory5)))
    {
      UINT featureSize = sizeof(allowTearing);
      if (FAILED(IDXGIFactory5_CheckFeatureSupport(
            factory5,
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &allowTearing,
            featureSize)))
      {
        allowTearing = FALSE;
      }
      if (allowTearing)
      {
        swapDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      }
    }
    if (SUCCEEDED(hr) && compositionHost)
    {
      hr = IDXGIFactory2_CreateSwapChainForComposition(
        factory,
        (IUnknown*)state->device,
        &swapDesc,
        NULL,
        &swapChain1);
    }
    else if (SUCCEEDED(hr))
    {
      hr = IDXGIFactory2_CreateSwapChainForHwnd(
        factory,
        (IUnknown*)state->device,
        hWnd,
        &swapDesc,
        NULL,
        NULL,
        &swapChain1);
    }
    if (SUCCEEDED(hr))
    {
      hr = IDXGISwapChain1_QueryInterface(swapChain1, &IID_IDXGISwapChain2, (void**)&state->swapChain);
    }
    if (SUCCEEDED(hr) && waitableSwapChain)
    {
      hr = IDXGISwapChain2_SetMaximumFrameLatency(
        state->swapChain,
        presentation->maximumFrameLatency);
    }
    if (SUCCEEDED(hr))
    {
      if (waitableSwapChain)
      {
        state->frameWaitHandle =
          IDXGISwapChain2_GetFrameLatencyWaitableObject(state->swapChain);
      }
      if (waitableSwapChain && !state->frameWaitHandle)
      {
        hr = E_FAIL;
      }
    }
    if (SUCCEEDED(hr) && flipModel)
    {
      hr = IDXGISwapChain2_SetSourceSize(
        state->swapChain,
        (UINT)clientSize.cx,
        (UINT)clientSize.cy);
    }
    if (SUCCEEDED(hr) && compositionHost &&
        !magDCompPresenterCreate(
          hWnd,
          (IUnknown*)state->swapChain,
          &state->composition))
    {
      hr = E_FAIL;
    }

    if (swapChain1)
    {
      IDXGISwapChain1_Release(swapChain1);
    }
    if (factory)
    {
      IDXGIFactory2_Release(factory);
    }
    if (factory5)
    {
      IDXGIFactory5_Release(factory5);
    }
    if (deviceAdapter)
    {
      IDXGIAdapter_Release(deviceAdapter);
    }
    if (dxgiDevice)
    {
      IDXGIDevice_Release(dxgiDevice);
    }
    if (dxgiDevice1)
    {
      IDXGIDevice1_Release(dxgiDevice1);
    }
    if (SUCCEEDED(hr))
    {
      state->capacityWidth = (UINT)reservoirSize.cx;
      state->capacityHeight = (UINT)reservoirSize.cy;
      state->bufferCount = presentation->bufferCount;
      state->syncInterval = presentation->syncInterval;
      state->presentFlags = allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
      state->swapChainFlags = swapDesc.Flags;
      state->presentationEnabled = TRUE;
    }
    if (selectedAdapter)
    {
      IDXGIAdapter1_Release(selectedAdapter);
    }
    return hr;
}

static void magGraphicsD3D11Destroy(HWND hWnd, void* opaqueState);

static BOOL magGraphicsD3D11Create(
  HWND hWnd,
  SIZE clientSize,
  const struct MAGPRESENTATIONSETTINGS* presentation,
  void** stateOut)
{
    MAGD3D11STATE* state;
    HRESULT hr;

    if (!stateOut || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *stateOut = NULL;

    state = (MAGD3D11STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }

    hr = magGraphicsD3D11CreateDevice(hWnd, clientSize, presentation, state);

    if (FAILED(hr) ||
        (!state->presentationManagerHost && !magGraphicsD3D11CreateFrameResources(
          state,
          (SIZE){ (LONG)state->capacityWidth, (LONG)state->capacityHeight })) ||
        !magGraphicsReserveCpuCompositor(
          &state->compositor,
          state->capacityWidth,
          state->capacityHeight))
    {
      const DWORD failure = FAILED(hr) ? (DWORD)hr : ERROR_NOT_ENOUGH_MEMORY;

      magGraphicsD3D11Destroy(hWnd, state);
      SetLastError(failure);
      return FALSE;
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    *stateOut = state;
    return TRUE;
}

static void magGraphicsD3D11Destroy(HWND hWnd, void* opaqueState)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return;
    }

    if (state->context)
    {
      ID3D11DeviceContext_ClearState(state->context);
      ID3D11DeviceContext_Flush(state->context);
    }
    magPresentationManagerPresenterDestroy(state->presentationManager);
    state->presentationManager = NULL;
    magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
    magGraphicsD3D11ReleaseTexture(&state->backBuffer);
    magDCompPresenterDestroy(state->composition);
    if (state->context)
    {
      /* D3D11 defers object destruction. Flush after dropping every buffer
         reference before releasing the flip-model swap chain for this HWND. */
      ID3D11DeviceContext_Flush(state->context);
    }
    if (state->swapChain)
    {
      IDXGISwapChain2_Release(state->swapChain);
    }
    if (state->frameWaitHandle)
    {
      CloseHandle(state->frameWaitHandle);
    }
    if (state->context)
    {
      ID3D11DeviceContext_Release(state->context);
    }
    if (state->device)
    {
      ID3D11Device_Release(state->device);
    }
    magGraphicsDestroyCpuCompositor(&state->compositor);
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsD3D11Resize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;
    HRESULT hr;

    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    if (state->presentationManagerHost)
    {
      return magPresentationManagerPresenterResize(
        state->presentationManager,
        clientSize);
    }
    if (!state->flipModel)
    {
      magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
      magGraphicsD3D11ReleaseTexture(&state->backBuffer);
      ID3D11DeviceContext_ClearState(state->context);
      ID3D11DeviceContext_Flush(state->context);
      hr = IDXGISwapChain2_ResizeBuffers(
        state->swapChain,
        state->bufferCount,
        state->width,
        state->height,
        DXGI_FORMAT_B8G8R8A8_UNORM,
        state->swapChainFlags);
      return SUCCEEDED(hr) &&
        magGraphicsD3D11CreateFrameResources(state, clientSize) &&
        magGraphicsReserveCpuCompositor(
          &state->compositor,
          state->width,
          state->height);
    }
    if (state->width <= state->capacityWidth && state->height <= state->capacityHeight)
    {
      return SUCCEEDED(IDXGISwapChain2_SetSourceSize(
        state->swapChain,
        state->width,
        state->height));
    }

    {
      SIZE reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
      reservoirSize.cx = max(reservoirSize.cx, (LONG)state->capacityWidth);
      reservoirSize.cy = max(reservoirSize.cy, (LONG)state->capacityHeight);

    magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
    magGraphicsD3D11ReleaseTexture(&state->backBuffer);
    hr = IDXGISwapChain2_ResizeBuffers(
      state->swapChain,
      state->bufferCount,
      (UINT)reservoirSize.cx,
      (UINT)reservoirSize.cy,
      DXGI_FORMAT_B8G8R8A8_UNORM,
      state->swapChainFlags);
    return SUCCEEDED(hr) &&
      magGraphicsD3D11CreateFrameResources(state, reservoirSize) &&
      SUCCEEDED(IDXGISwapChain2_SetSourceSize(
        state->swapChain,
        state->width,
        state->height));
    }
}

static BOOL magGraphicsD3D11SetPresentationEnabled(HWND hWnd, void* opaqueState, BOOL enabled)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return FALSE;
    }
    if (state->presentationManagerHost &&
        !magPresentationManagerPresenterSetEnabled(
          state->presentationManager,
          enabled))
    {
      return FALSE;
    }
    if (state->compositionHost &&
        !magDCompPresenterSetEnabled(state->composition, enabled))
    {
      return FALSE;
    }
    state->presentationEnabled = enabled;
    return TRUE;
}

static BOOL magGraphicsD3D11Render(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  const MAGPRESENTINTENT* intent)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;
    MAGPIXELBUFFER composed;
    D3D11_BOX box;
    HRESULT hr;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !frame || frame->width != state->width || frame->height != state->height ||
        !magGraphicsComposeFrame(&state->compositor, frame, ui, &composed))
    {
      return FALSE;
    }

    box.left = 0;
    box.top = 0;
    box.front = 0;
    box.right = state->width;
    box.bottom = state->height;
    box.back = 1;
    if (state->presentationManagerHost)
    {
      ID3D11Texture2D* presentationTexture = NULL;
      UINT bufferIndex;
      BOOL acquired = magPresentationManagerPresenterAcquire(
        state->presentationManager,
        !intent || intent->synchronize,
        &presentationTexture,
        &bufferIndex);

      if (!acquired || !presentationTexture)
      {
        return FALSE;
      }
      ID3D11DeviceContext_UpdateSubresource(
        state->context,
        (ID3D11Resource*)presentationTexture,
        0,
        &box,
        composed.pixels,
        composed.stride,
        0);
      ID3D11Texture2D_Release(presentationTexture);
      return magPresentationManagerPresenterPresent(
        state->presentationManager,
        bufferIndex,
        (SIZE){ (LONG)state->width, (LONG)state->height },
        intent);
    }
    ID3D11DeviceContext_UpdateSubresource(
      state->context,
      (ID3D11Resource*)state->uploadTexture,
      0,
      &box,
      composed.pixels,
      composed.stride,
      0);
    ID3D11DeviceContext_CopyResource(
      state->context,
      (ID3D11Resource*)state->backBuffer,
      (ID3D11Resource*)state->uploadTexture);

    if (state->flipModel && intent && intent->restartSequence)
    {
      HRESULT first = IDXGISwapChain2_Present(
        state->swapChain,
        0,
        state->presentFlags | DXGI_PRESENT_RESTART |
          (state->waitableSwapChain ? DXGI_PRESENT_DO_NOT_WAIT : 0));

      if (!intent->synchronize && !state->waitableSwapChain)
      {
        return SUCCEEDED(first);
      }
      HRESULT second = IDXGISwapChain2_Present(
        state->swapChain,
        intent->synchronize ? 1 : 0,
        intent->synchronize
          ? DXGI_PRESENT_DO_NOT_SEQUENCE
          : state->presentFlags | DXGI_PRESENT_DO_NOT_WAIT);

      return (SUCCEEDED(first) || DXGI_ERROR_WAS_STILL_DRAWING == first) &&
        SUCCEEDED(second);
    }

    if (intent && !intent->synchronize)
    {
      hr = IDXGISwapChain2_Present(
        state->swapChain,
        0,
        state->presentFlags |
          (state->waitableSwapChain ? DXGI_PRESENT_DO_NOT_WAIT : 0));
      return SUCCEEDED(hr);
    }

    hr = IDXGISwapChain2_Present(
      state->swapChain,
      state->syncInterval,
      state->presentFlags);
    return SUCCEEDED(hr);
}

static HANDLE magGraphicsD3D11GetFrameWaitHandle(void* state)
{
    MAGD3D11STATE* d3dState = (MAGD3D11STATE*)state;

    if (!d3dState)
    {
      return NULL;
    }
    return d3dState->presentationManagerHost
      ? magPresentationManagerPresenterGetFrameWaitHandle(
          d3dState->presentationManager)
      : d3dState->frameWaitHandle;
}

static UINT64 magGraphicsD3D11GetResourceGeneration(void* opaqueState)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;
    return state
      ? state->resourceGeneration + state->compositor.generation +
        magPresentationManagerPresenterGetResourceGeneration(
          state->presentationManager)
      : 0;
}

static BOOL magGraphicsD3D11GetNextEstimatedFrameTime(
  void* opaqueState,
  LONGLONG* frameTime)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;
    if (!state)
    {
      return FALSE;
    }
    return state->presentationManagerHost
      ? magPresentationManagerPresenterGetNextEstimatedFrameTime(
          state->presentationManager,
          frameTime)
      : magDCompPresenterGetNextEstimatedFrameTime(
          state->composition,
          frameTime);
}

static BOOL magGraphicsD3D11GetObservedPresentationTarget(
  void* opaqueState,
  UINT* target)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;

    if (state && state->presentationManagerHost)
    {
      return magPresentationManagerPresenterGetObservedTarget(
        state->presentationManager,
        target);
    }
    return FALSE;
}

const MAGGRAPHICSBACKEND g_magGraphicsD3D11Backend =
{
  GRAPHICS_API_D3D11,
  TEXT("Direct3D 11"),
  TRUE,
  magGraphicsD3D11IsAvailable,
  magGraphicsD3D11Create,
  magGraphicsD3D11Destroy,
  magGraphicsD3D11Resize,
  magGraphicsD3D11SetPresentationEnabled,
  magGraphicsD3D11Render,
  magGraphicsD3D11GetFrameWaitHandle,
  magGraphicsD3D11GetResourceGeneration,
  magGraphicsD3D11GetNextEstimatedFrameTime,
  magGraphicsD3D11GetObservedPresentationTarget,
};
