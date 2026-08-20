#include "framework.h"
#include "graphics.h"

#include <d3d11.h>
#include <dxgi.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")

typedef struct MAGD3D11STATE
{
  ID3D11Device*        device;
  ID3D11DeviceContext* context;
  IDXGISwapChain*      swapChain;
  ID3D11Texture2D*     backBuffer;
  ID3D11Texture2D*     uploadTexture;
  MAGCPUCOMPOSITOR     compositor;
  UINT                 width;
  UINT                 height;
  BOOL                 warp;
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

static BOOL magGraphicsD3D11CreateFrameResources(MAGD3D11STATE* state, SIZE clientSize)
{
    D3D11_TEXTURE2D_DESC textureDesc = { 0 };
    HRESULT hr;

    if (clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }

    magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
    magGraphicsD3D11ReleaseTexture(&state->backBuffer);

    hr = IDXGISwapChain_GetBuffer(
      state->swapChain,
      0,
      &IID_ID3D11Texture2D,
      (void**)&state->backBuffer);
    if (FAILED(hr))
    {
      return FALSE;
    }

    textureDesc.Width = (UINT)clientSize.cx;
    textureDesc.Height = (UINT)clientSize.cy;
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

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    return TRUE;
}

static HRESULT magGraphicsD3D11CreateDevice(
  HWND hWnd,
  SIZE clientSize,
  D3D_DRIVER_TYPE driverType,
  MAGD3D11STATE* state)
{
    DXGI_SWAP_CHAIN_DESC swapDesc = { 0 };
    const D3D_FEATURE_LEVEL featureLevels[] =
    {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
    };
    D3D_FEATURE_LEVEL selectedFeatureLevel;

    swapDesc.BufferDesc.Width = (UINT)clientSize.cx;
    swapDesc.BufferDesc.Height = (UINT)clientSize.cy;
    swapDesc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = 2;
    swapDesc.OutputWindow = hWnd;
    swapDesc.Windowed = TRUE;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
      NULL,
      driverType,
      NULL,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      featureLevels,
      ARRAYSIZE(featureLevels),
      D3D11_SDK_VERSION,
      &swapDesc,
      &state->swapChain,
      &state->device,
      &selectedFeatureLevel,
      &state->context);

    if (E_INVALIDARG == hr)
    {
      hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        driverType,
        NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        featureLevels + 1,
        ARRAYSIZE(featureLevels) - 1,
        D3D11_SDK_VERSION,
        &swapDesc,
        &state->swapChain,
        &state->device,
        &selectedFeatureLevel,
        &state->context);
    }
    return hr;
}

static void magGraphicsD3D11Destroy(HWND hWnd, void* opaqueState);

static BOOL magGraphicsD3D11Create(HWND hWnd, SIZE clientSize, void** stateOut)
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

    hr = magGraphicsD3D11CreateDevice(hWnd, clientSize, D3D_DRIVER_TYPE_HARDWARE, state);
    if (FAILED(hr))
    {
      hr = magGraphicsD3D11CreateDevice(hWnd, clientSize, D3D_DRIVER_TYPE_WARP, state);
      state->warp = SUCCEEDED(hr);
    }

    if (FAILED(hr) || !magGraphicsD3D11CreateFrameResources(state, clientSize))
    {
      magGraphicsD3D11Destroy(hWnd, state);
      return FALSE;
    }

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
    magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
    magGraphicsD3D11ReleaseTexture(&state->backBuffer);
    if (state->context)
    {
      /* D3D11 defers object destruction. Flush after dropping every buffer
         reference before releasing the flip-model swap chain for this HWND. */
      ID3D11DeviceContext_Flush(state->context);
    }
    if (state->swapChain)
    {
      IDXGISwapChain_Release(state->swapChain);
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

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }

    if ((UINT)clientSize.cx == state->width && (UINT)clientSize.cy == state->height)
    {
      return TRUE;
    }

    magGraphicsD3D11ReleaseTexture(&state->uploadTexture);
    magGraphicsD3D11ReleaseTexture(&state->backBuffer);
    hr = IDXGISwapChain_ResizeBuffers(
      state->swapChain,
      0,
      (UINT)clientSize.cx,
      (UINT)clientSize.cy,
      DXGI_FORMAT_UNKNOWN,
      0);
    return SUCCEEDED(hr) && magGraphicsD3D11CreateFrameResources(state, clientSize);
}

static BOOL magGraphicsD3D11Render(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui)
{
    MAGD3D11STATE* state = (MAGD3D11STATE*)opaqueState;
    MAGPIXELBUFFER composed;
    HRESULT hr;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !frame || frame->width != state->width || frame->height != state->height ||
        !magGraphicsComposeFrame(&state->compositor, frame, ui, &composed))
    {
      return FALSE;
    }

    ID3D11DeviceContext_UpdateSubresource(
      state->context,
      (ID3D11Resource*)state->uploadTexture,
      0,
      NULL,
      composed.pixels,
      composed.stride,
      0);
    ID3D11DeviceContext_CopyResource(
      state->context,
      (ID3D11Resource*)state->backBuffer,
      (ID3D11Resource*)state->uploadTexture);

    hr = IDXGISwapChain_Present(state->swapChain, 1, 0);
    return SUCCEEDED(hr);
}

static HANDLE magGraphicsD3D11GetFrameWaitHandle(void* state)
{
    UNREFERENCED_PARAMETER(state);
    return NULL;
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
  magGraphicsSetPresentationEnabledNoop,
  magGraphicsD3D11Render,
  magGraphicsD3D11GetFrameWaitHandle,
};
