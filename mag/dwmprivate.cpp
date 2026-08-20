#define NOMINMAX

#include "dwmprivate.h"

#include <d3d11.h>
#include <dcomp.h>
#include <dwmapi.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>
#include <vector>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "dxgi")

#define DWM_ORD_CREATE_SHARED_DESKTOP_VISUAL 163
#define DWM_ORD_UPDATE_SHARED_DESKTOP_VISUAL 164
#define DWM_PRIVATE_MULTIWINDOW_BUILD 20000

using Microsoft::WRL::ComPtr;

typedef HRESULT (WINAPI* PFN_DCOMPOSITIONCREATEDEVICE3)(IUnknown* renderingDevice, REFIID iid, void** dcompositionDevice);
typedef HRESULT (WINAPI* PFN_DWMPCREATESHAREDDESKTOPVISUAL)(HWND hwndDestination, void* dcompDevice, void** visual, PHTHUMBNAIL thumbnail);
typedef HRESULT (WINAPI* PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL)(HTHUMBNAIL thumbnail, HWND* includeWindows, DWORD includeCount, HWND* excludeWindows, DWORD excludeCount, RECT* source, SIZE* destinationSize);
typedef HRESULT (WINAPI* PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL)(HTHUMBNAIL thumbnail, HWND* includeWindows, DWORD includeCount, HWND* excludeWindows, DWORD excludeCount, RECT* source, SIZE* destinationSize, DWORD flags);

typedef enum MAG_WINDOWCOMPOSITIONATTRIB
{
  MAG_WCA_EXCLUDED_FROM_LIVEPREVIEW = 0x0D
} MAG_WINDOWCOMPOSITIONATTRIB;

typedef struct MAG_WINDOWCOMPOSITIONATTRIBDATA
{
  MAG_WINDOWCOMPOSITIONATTRIB attrib;
  void*                       data;
  DWORD                       dataSize;
} MAG_WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL (WINAPI* PFN_SETWINDOWCOMPOSITIONATTRIBUTE)(HWND hwnd, MAG_WINDOWCOMPOSITIONATTRIBDATA* data);

typedef struct MAG_RTL_OSVERSIONINFOW
{
  ULONG dwOSVersionInfoSize;
  ULONG dwMajorVersion;
  ULONG dwMinorVersion;
  ULONG dwBuildNumber;
  ULONG dwPlatformId;
  WCHAR szCSDVersion[128];
} MAG_RTL_OSVERSIONINFOW;

typedef LONG (WINAPI* PFN_RTLGETVERSION)(MAG_RTL_OSVERSIONINFOW* versionInformation);

struct DWMPRIVATECAPTURESTATE
{
  HWND                                      hwndDestination = NULL;
  HMODULE                                   hDcomp = NULL;
  HMODULE                                   hDwmApi = NULL;
  HMODULE                                   hUser32 = NULL;
  PFN_DCOMPOSITIONCREATEDEVICE3             createDcompDevice = NULL;
  PFN_DWMPCREATESHAREDDESKTOPVISUAL         createSharedVisual = NULL;
  FARPROC                                   updateSharedVisual = NULL;
  PFN_SETWINDOWCOMPOSITIONATTRIBUTE         setWindowCompositionAttribute = NULL;
  ComPtr<ID3D11Device>                      d3dDevice;
  ComPtr<ID3D11DeviceContext>               d3dContext;
  ComPtr<IDXGIDevice>                       dxgiDevice;
  ComPtr<IDCompositionDesktopDevice>        dcompDevice;
  ComPtr<IDCompositionTarget>               dcompTarget;
  ComPtr<IDCompositionVisual2>              rootVisual;
  ComPtr<IDCompositionVisual2>              captureVisual;
  ComPtr<IDCompositionVisual2>              viewVisual;
  ComPtr<IDCompositionVisual>               sharedVisual;
  ComPtr<IDCompositionVisual2>              overlayVisual;
  ComPtr<IDXGISwapChain1>                   overlaySwapChain;
  HTHUMBNAIL                                thumbnail = NULL;
  UINT                                      overlayWidth = 0;
  UINT                                      overlayHeight = 0;
  BOOL                                      useMultiWindow = FALSE;
  BOOL                                      livePreviewExcluded = FALSE;
  BOOL                                      desktopConfigured = FALSE;
  RECT                                      desktopSource = {};
  std::vector<std::uint32_t>                overlayPixels;
};

static LONG DwmPrivateClampLong(LONG value, LONG minimum, LONG maximum)
{
  if (value < minimum)
  {
    return minimum;
  }

  if (value > maximum)
  {
    return maximum;
  }

  return value;
}

static BYTE DwmPrivateFloatToByte(FLOAT value)
{
  if (value <= 0.0f)
  {
    return 0;
  }

  if (value >= 1.0f)
  {
    return 255;
  }

  return static_cast<BYTE>(std::lround(value * 255.0f));
}

static std::uint32_t DwmPrivatePremultiplyColor(const FLOAT color[4])
{
  const UINT alpha = DwmPrivateFloatToByte(color[3]);
  const UINT red = (DwmPrivateFloatToByte(color[0]) * alpha + 127U) / 255U;
  const UINT green = (DwmPrivateFloatToByte(color[1]) * alpha + 127U) / 255U;
  const UINT blue = (DwmPrivateFloatToByte(color[2]) * alpha + 127U) / 255U;

  return blue | (green << 8) | (red << 16) | (alpha << 24);
}

static std::uint32_t DwmPrivateBlendPixel(std::uint32_t destination, std::uint32_t source)
{
  const UINT sourceAlpha = (source >> 24) & 0xffU;
  const UINT inverseAlpha = 255U - sourceAlpha;
  const UINT destinationBlue = destination & 0xffU;
  const UINT destinationGreen = (destination >> 8) & 0xffU;
  const UINT destinationRed = (destination >> 16) & 0xffU;
  const UINT destinationAlpha = (destination >> 24) & 0xffU;
  const UINT blue = (source & 0xffU) + ((destinationBlue * inverseAlpha + 127U) / 255U);
  const UINT green = ((source >> 8) & 0xffU) + ((destinationGreen * inverseAlpha + 127U) / 255U);
  const UINT red = ((source >> 16) & 0xffU) + ((destinationRed * inverseAlpha + 127U) / 255U);
  const UINT alpha = sourceAlpha + ((destinationAlpha * inverseAlpha + 127U) / 255U);

  return blue | (green << 8) | (red << 16) | (alpha << 24);
}

static RECT DwmPrivateClipRect(const DWMPRIVATECAPTURESTATE* state, const RECT& rect)
{
  RECT clipped = rect;

  clipped.left = DwmPrivateClampLong(clipped.left, 0, static_cast<LONG>(state->overlayWidth));
  clipped.top = DwmPrivateClampLong(clipped.top, 0, static_cast<LONG>(state->overlayHeight));
  clipped.right = DwmPrivateClampLong(clipped.right, 0, static_cast<LONG>(state->overlayWidth));
  clipped.bottom = DwmPrivateClampLong(clipped.bottom, 0, static_cast<LONG>(state->overlayHeight));
  return clipped;
}

static void DwmPrivateFillRect(DWMPRIVATECAPTURESTATE* state, const RECT& rect, std::uint32_t color)
{
  const RECT clipped = DwmPrivateClipRect(state, rect);

  if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
  {
    return;
  }

  for (LONG y = clipped.top; y < clipped.bottom; ++y)
  {
    std::uint32_t* row = state->overlayPixels.data() + (static_cast<std::size_t>(y) * state->overlayWidth);

    for (LONG x = clipped.left; x < clipped.right; ++x)
    {
      row[x] = DwmPrivateBlendPixel(row[x], color);
    }
  }
}

static void DwmPrivateClearRect(DWMPRIVATECAPTURESTATE* state, const RECT& rect)
{
  const RECT clipped = DwmPrivateClipRect(state, rect);

  if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
  {
    return;
  }

  for (LONG y = clipped.top; y < clipped.bottom; ++y)
  {
    std::uint32_t* row = state->overlayPixels.data() + (static_cast<std::size_t>(y) * state->overlayWidth);
    std::fill(row + clipped.left, row + clipped.right, 0U);
  }
}

static void DwmPrivateStrokeRect(DWMPRIVATECAPTURESTATE* state, const RECT& rect, UINT thickness, std::uint32_t color)
{
  const RECT clipped = DwmPrivateClipRect(state, rect);
  const LONG stroke = static_cast<LONG>(thickness ? thickness : 1U);

  if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
  {
    return;
  }

  for (LONG y = clipped.top; y < clipped.bottom; ++y)
  {
    std::uint32_t* row = state->overlayPixels.data() + (static_cast<std::size_t>(y) * state->overlayWidth);

    for (LONG x = clipped.left; x < clipped.right; ++x)
    {
      if (x < rect.left + stroke || x >= rect.right - stroke ||
          y < rect.top + stroke || y >= rect.bottom - stroke)
      {
        row[x] = DwmPrivateBlendPixel(row[x], color);
      }
    }
  }
}

static BOOL DwmPrivateIsMultiWindowBuild(void)
{
  HMODULE hNtdll = GetModuleHandle(TEXT("ntdll.dll"));
  PFN_RTLGETVERSION rtlGetVersion = hNtdll
    ? reinterpret_cast<PFN_RTLGETVERSION>(GetProcAddress(hNtdll, "RtlGetVersion"))
    : NULL;
  MAG_RTL_OSVERSIONINFOW version = {};

  version.dwOSVersionInfoSize = sizeof(version);
  if (rtlGetVersion && rtlGetVersion(&version) >= 0)
  {
    return version.dwBuildNumber >= DWM_PRIVATE_MULTIWINDOW_BUILD;
  }

  return sizeof(void*) == 8 ? TRUE : FALSE;
}

static BOOL DwmPrivateSetExcludedFromLivePreview(DWMPRIVATECAPTURESTATE* state, BOOL excluded)
{
  MAG_WINDOWCOMPOSITIONATTRIBDATA data = {};
  BOOL enabled = excluded;

  if (!state->setWindowCompositionAttribute)
  {
    return FALSE;
  }

  data.attrib = MAG_WCA_EXCLUDED_FROM_LIVEPREVIEW;
  data.data = &enabled;
  data.dataSize = sizeof(enabled);
  return state->setWindowCompositionAttribute(state->hwndDestination, &data);
}

static HRESULT DwmPrivateConfigureDesktop(
  DWMPRIVATECAPTURESTATE* state,
  const RECT& desktopSource)
{
  if (state->desktopConfigured && EqualRect(&state->desktopSource, &desktopSource))
  {
    return S_OK;
  }

  RECT sourceCopy = desktopSource;
  SIZE destinationSize =
  {
    sourceCopy.right - sourceCopy.left,
    sourceCopy.bottom - sourceCopy.top
  };
  HWND excludeWindow = state->hwndDestination;
  HRESULT hr;

  if (destinationSize.cx < 1 || destinationSize.cy < 1)
  {
    return E_INVALIDARG;
  }

  if (state->useMultiWindow)
  {
    PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL update =
      reinterpret_cast<PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL>(state->updateSharedVisual);

    hr = update(
      state->thumbnail,
      NULL,
      0,
      &excludeWindow,
      1,
      &sourceCopy,
      &destinationSize,
      1);
  }
  else
  {
    PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL update =
      reinterpret_cast<PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL>(state->updateSharedVisual);

    hr = update(
      state->thumbnail,
      NULL,
      0,
      &excludeWindow,
      1,
      &sourceCopy,
      &destinationSize);
  }

  if (SUCCEEDED(hr))
  {
    state->desktopSource = desktopSource;
    state->desktopConfigured = TRUE;
  }

  return hr;
}

static HRESULT DwmPrivateEnsureOverlay(DWMPRIVATECAPTURESTATE* state, UINT width, UINT height)
{
  ComPtr<IDXGIAdapter> adapter;
  ComPtr<IDXGIFactory2> factory;
  ComPtr<IDXGISwapChain1> swapChain;
  DXGI_SWAP_CHAIN_DESC1 desc = {};
  std::vector<std::uint32_t> pixels;
  const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  HRESULT hr;

  if (state->overlaySwapChain && state->overlayWidth == width && state->overlayHeight == height)
  {
    return S_OK;
  }

  if (!width || !height ||
      width > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      height > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      pixelCount > std::numeric_limits<std::size_t>::max() / sizeof(std::uint32_t))
  {
    return E_INVALIDARG;
  }

  try
  {
    pixels.resize(pixelCount);
  }
  catch (const std::bad_alloc&)
  {
    return E_OUTOFMEMORY;
  }

  hr = state->dxgiDevice->GetAdapter(&adapter);
  if (SUCCEEDED(hr))
  {
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
  }

  desc.Width = width;
  desc.Height = height;
  desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  desc.Stereo = FALSE;
  desc.SampleDesc.Count = 1;
  desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  desc.BufferCount = 2;
  desc.Scaling = DXGI_SCALING_STRETCH;
  desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
  desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

  if (SUCCEEDED(hr))
  {
    hr = factory->CreateSwapChainForComposition(state->d3dDevice.Get(), &desc, NULL, &swapChain);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->overlayVisual->SetContent(swapChain.Get());
  }

  if (FAILED(hr))
  {
    return hr;
  }

  state->overlaySwapChain = std::move(swapChain);
  state->overlayPixels = std::move(pixels);
  state->overlayWidth = width;
  state->overlayHeight = height;
  return S_OK;
}

static HRESULT DwmPrivateUpdateOverlay(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* captureDestination,
  const DWMPRIVATEDRAWCOMMAND* drawCommands,
  UINT drawCommandCount)
{
  ComPtr<ID3D11Texture2D> backBuffer;
  const std::uint32_t opaqueBlack = 0xff000000U;
  HRESULT hr;

  std::fill(state->overlayPixels.begin(), state->overlayPixels.end(), opaqueBlack);
  if (captureDestination)
  {
    DwmPrivateClearRect(state, *captureDestination);
  }

  for (UINT i = 0; i < drawCommandCount; ++i)
  {
    const DWMPRIVATEDRAWCOMMAND& command = drawCommands[i];
    const std::uint32_t color = DwmPrivatePremultiplyColor(command.color);

    if (DWM_PRIVATE_DRAW_FILL == command.type)
    {
      DwmPrivateFillRect(state, command.rc, color);
    }
    else if (DWM_PRIVATE_DRAW_STROKE == command.type)
    {
      DwmPrivateStrokeRect(state, command.rc, command.thickness, color);
    }
    else
    {
      return E_INVALIDARG;
    }
  }

  hr = state->overlaySwapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
  if (FAILED(hr))
  {
    return hr;
  }

  state->d3dContext->UpdateSubresource(
    backBuffer.Get(),
    0,
    NULL,
    state->overlayPixels.data(),
    state->overlayWidth * sizeof(std::uint32_t),
    0);

  return state->overlaySwapChain->Present(0, 0);
}

extern "C" void DwmPrivateCaptureDestroy(DWMPRIVATECAPTURESTATE* state)
{
  if (!state)
  {
    return;
  }

  if (state->dcompTarget)
  {
    state->dcompTarget->SetRoot(NULL);
  }

  if (state->dcompDevice)
  {
    state->dcompDevice->Commit();
    DwmFlush();
  }

  if (state->livePreviewExcluded)
  {
    DwmPrivateSetExcludedFromLivePreview(state, FALSE);
  }

  if (state->thumbnail)
  {
    DwmUnregisterThumbnail(state->thumbnail);
    state->thumbnail = NULL;
  }

  state->overlaySwapChain.Reset();
  state->overlayVisual.Reset();
  state->sharedVisual.Reset();
  state->viewVisual.Reset();
  state->captureVisual.Reset();
  state->rootVisual.Reset();
  state->dcompTarget.Reset();
  state->dcompDevice.Reset();
  state->dxgiDevice.Reset();
  state->d3dContext.Reset();
  state->d3dDevice.Reset();

  if (state->hUser32)
  {
    FreeLibrary(state->hUser32);
  }

  if (state->hDwmApi)
  {
    FreeLibrary(state->hDwmApi);
  }

  if (state->hDcomp)
  {
    FreeLibrary(state->hDcomp);
  }

  delete state;
}

extern "C" BOOL DwmPrivateCaptureCreate(HWND hWnd, DWMPRIVATECAPTURESTATE** stateOut)
{
  DWMPRIVATECAPTURESTATE* state;
  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr;
  IDCompositionVisual* sharedVisual = NULL;

  if (!hWnd || !stateOut)
  {
    return FALSE;
  }

  *stateOut = NULL;
  state = new (std::nothrow) DWMPRIVATECAPTURESTATE();
  if (!state)
  {
    return FALSE;
  }

  state->hwndDestination = hWnd;
  state->hDcomp = LoadLibrary(TEXT("dcomp.dll"));
  state->hDwmApi = LoadLibrary(TEXT("dwmapi.dll"));
  state->hUser32 = LoadLibrary(TEXT("user32.dll"));

  if (state->hDcomp)
  {
    state->createDcompDevice = reinterpret_cast<PFN_DCOMPOSITIONCREATEDEVICE3>(
      GetProcAddress(state->hDcomp, "DCompositionCreateDevice3"));
  }

  if (state->hDwmApi)
  {
    state->createSharedVisual = reinterpret_cast<PFN_DWMPCREATESHAREDDESKTOPVISUAL>(
      GetProcAddress(state->hDwmApi, MAKEINTRESOURCEA(DWM_ORD_CREATE_SHARED_DESKTOP_VISUAL)));
    state->updateSharedVisual = GetProcAddress(
      state->hDwmApi,
      MAKEINTRESOURCEA(DWM_ORD_UPDATE_SHARED_DESKTOP_VISUAL));
  }

  if (state->hUser32)
  {
    state->setWindowCompositionAttribute = reinterpret_cast<PFN_SETWINDOWCOMPOSITIONATTRIBUTE>(
      GetProcAddress(state->hUser32, "SetWindowCompositionAttribute"));
  }

  if (!state->createDcompDevice || !state->createSharedVisual || !state->updateSharedVisual)
  {
    DwmPrivateCaptureDestroy(state);
    return FALSE;
  }

  hr = D3D11CreateDevice(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    NULL,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
    NULL,
    0,
    D3D11_SDK_VERSION,
    &state->d3dDevice,
    &featureLevel,
    &state->d3dContext);

  if (FAILED(hr))
  {
    hr = D3D11CreateDevice(
      NULL,
      D3D_DRIVER_TYPE_WARP,
      NULL,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      NULL,
      0,
      D3D11_SDK_VERSION,
      &state->d3dDevice,
      &featureLevel,
      &state->d3dContext);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->d3dDevice.As(&state->dxgiDevice);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->createDcompDevice(
      state->dxgiDevice.Get(),
      __uuidof(IDCompositionDesktopDevice),
      reinterpret_cast<void**>(state->dcompDevice.GetAddressOf()));
  }

  if (SUCCEEDED(hr))
  {
    hr = state->createSharedVisual(
      hWnd,
      state->dcompDevice.Get(),
      reinterpret_cast<void**>(&sharedVisual),
      &state->thumbnail);
  }

  if (SUCCEEDED(hr))
  {
    state->sharedVisual.Attach(sharedVisual);
    sharedVisual = NULL;
    hr = state->dcompDevice->CreateVisual(&state->rootVisual);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->CreateVisual(&state->captureVisual);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->CreateVisual(&state->viewVisual);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->CreateVisual(&state->overlayVisual);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->CreateTargetForHwnd(hWnd, FALSE, &state->dcompTarget);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->viewVisual->AddVisual(state->sharedVisual.Get(), TRUE, NULL);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->captureVisual->AddVisual(state->viewVisual.Get(), TRUE, NULL);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->rootVisual->AddVisual(state->captureVisual.Get(), TRUE, NULL);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->rootVisual->AddVisual(state->overlayVisual.Get(), TRUE, state->captureVisual.Get());
  }

  if (SUCCEEDED(hr))
  {
    hr = state->dcompTarget->SetRoot(state->rootVisual.Get());
  }

  if (SUCCEEDED(hr))
  {
    state->livePreviewExcluded = DwmPrivateSetExcludedFromLivePreview(state, TRUE);
    state->useMultiWindow = DwmPrivateIsMultiWindowBuild();
    hr = state->dcompDevice->Commit();
  }

  if (FAILED(hr))
  {
    if (sharedVisual)
    {
      sharedVisual->Release();
    }
    DwmPrivateCaptureDestroy(state);
    return FALSE;
  }

  *stateOut = state;
  return TRUE;
}

extern "C" BOOL DwmPrivateCaptureUpdate(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* desktop,
  const RECT* viewSource,
  const RECT* viewDestination,
  SIZE targetSize,
  const DWMPRIVATEDRAWCOMMAND* drawCommands,
  UINT drawCommandCount)
{
  HRESULT hr;

  if (!state || !desktop || IsRectEmpty(desktop) ||
      targetSize.cx < 1 || targetSize.cy < 1 ||
      (drawCommandCount && !drawCommands) ||
      drawCommandCount > DWM_PRIVATE_MAX_DRAW_COMMANDS ||
      (!!viewSource != !!viewDestination))
  {
    return FALSE;
  }

  hr = DwmPrivateEnsureOverlay(
    state,
    static_cast<UINT>(targetSize.cx),
    static_cast<UINT>(targetSize.cy));

  if (SUCCEEDED(hr))
  {
    hr = DwmPrivateConfigureDesktop(state, *desktop);
  }

  if (SUCCEEDED(hr) && viewSource && viewDestination)
  {
    const LONG sourceWidth = viewSource->right - viewSource->left;
    const LONG sourceHeight = viewSource->bottom - viewSource->top;
    const LONG destinationWidth = viewDestination->right - viewDestination->left;
    const LONG destinationHeight = viewDestination->bottom - viewDestination->top;

    if (sourceWidth < 1 || sourceHeight < 1 ||
        destinationWidth < 1 || destinationHeight < 1 ||
        viewSource->left < desktop->left || viewSource->top < desktop->top ||
        viewSource->right > desktop->right || viewSource->bottom > desktop->bottom)
    {
      hr = E_INVALIDARG;
    }
    else
    {
      const FLOAT scaleX = static_cast<FLOAT>(destinationWidth) / static_cast<FLOAT>(sourceWidth);
      const FLOAT scaleY = static_cast<FLOAT>(destinationHeight) / static_cast<FLOAT>(sourceHeight);
      const FLOAT sourceLeft = static_cast<FLOAT>(viewSource->left - desktop->left);
      const FLOAT sourceTop = static_cast<FLOAT>(viewSource->top - desktop->top);
      const D2D_MATRIX_3X2_F transform =
      {
        scaleX,
        0.0f,
        0.0f,
        scaleY,
        static_cast<FLOAT>(viewDestination->left) - sourceLeft * scaleX,
        static_cast<FLOAT>(viewDestination->top) - sourceTop * scaleY
      };
      const D2D_RECT_F clip =
      {
        static_cast<FLOAT>(viewDestination->left),
        static_cast<FLOAT>(viewDestination->top),
        static_cast<FLOAT>(viewDestination->right),
        static_cast<FLOAT>(viewDestination->bottom)
      };

      hr = state->viewVisual->SetTransform(transform);
      if (SUCCEEDED(hr))
      {
        hr = state->captureVisual->SetClip(clip);
      }
    }
  }
  else if (SUCCEEDED(hr))
  {
    const D2D_RECT_F emptyClip = {};
    hr = state->captureVisual->SetClip(emptyClip);
  }

  if (SUCCEEDED(hr))
  {
    hr = DwmPrivateUpdateOverlay(state, viewDestination, drawCommands, drawCommandCount);
  }

  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->Commit();
  }

  if (SUCCEEDED(hr))
  {
    hr = DwmFlush();
  }

  return SUCCEEDED(hr);
}
