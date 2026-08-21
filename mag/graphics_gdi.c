#include "framework.h"
#include "graphics.h"
#include "graphics_presentation_manager.h"
#include "presentation.h"

#include <d3d11.h>

#pragma comment(lib, "Msimg32")
#pragma comment(lib, "d3d11")

typedef struct MAGGDISTATE
{
  HDC      hColorDC;
  HBITMAP  hColorBitmap;
  HBITMAP  hColorBitmapOld;
  DWORD*   colorPixel;
  ID3D11Device* d3dDevice;
  ID3D11DeviceContext* d3dContext;
  MAGPRESENTATIONMANAGERPRESENTER* presentationManager;
  MAGCPUCOMPOSITOR compositor;
  UINT     width;
  UINT     height;
  UINT     capacityWidth;
  UINT     capacityHeight;
  BOOL     presentationManagerHost;
  BOOL     presentationEnabled;
  UINT64   resourceGeneration;
} MAGGDISTATE;

static void magGraphicsGdiReleaseState(MAGGDISTATE* state)
{
    if (!state)
    {
      return;
    }
    if (state->hColorDC && state->hColorBitmapOld)
    {
      SelectBitmap(state->hColorDC, state->hColorBitmapOld);
    }
    if (state->hColorBitmap)
    {
      DeleteBitmap(state->hColorBitmap);
    }
    if (state->hColorDC)
    {
      DeleteDC(state->hColorDC);
    }
    magPresentationManagerPresenterDestroy(state->presentationManager);
    if (state->d3dContext)
    {
      ID3D11DeviceContext_Release(state->d3dContext);
    }
    if (state->d3dDevice)
    {
      ID3D11Device_Release(state->d3dDevice);
    }
    magGraphicsDestroyCpuCompositor(&state->compositor);
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsGdiCreatePresentationManager(
  HWND hWnd,
  MAGGDISTATE* state,
  SIZE clientSize,
  const MAGPRESENTATIONSETTINGS* presentation)
{
    IDXGIAdapter1* adapter = NULL;
    D3D_FEATURE_LEVEL featureLevel;
    D3D_DRIVER_TYPE driverType;
    SIZE reservoirSize;
    UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT |
      D3D11_CREATE_DEVICE_SINGLETHREADED |
      D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
    HRESULT hr;

    if (!presentation || MAG_HOST_PRESENTATION_MANAGER != presentation->host)
    {
      return TRUE;
    }
    driverType = MAG_HARDWARE_ADAPTER_WARP == presentation->hardware.mode
      ? D3D_DRIVER_TYPE_WARP
      : D3D_DRIVER_TYPE_UNKNOWN;
    if (D3D_DRIVER_TYPE_WARP != driverType &&
        !magAdapterOpenDxgi(presentation->hardware.adapterLuid, &adapter))
    {
      return FALSE;
    }

    hr = D3D11CreateDevice(
      (IDXGIAdapter*)adapter,
      driverType,
      NULL,
      deviceFlags,
      NULL,
      0,
      D3D11_SDK_VERSION,
      &state->d3dDevice,
      &featureLevel,
      &state->d3dContext);
    if (adapter)
    {
      IDXGIAdapter1_Release(adapter);
    }
    if (FAILED(hr))
    {
      return FALSE;
    }

    reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
    if (!magPresentationManagerPresenterCreate(
          hWnd,
          state->d3dDevice,
          reservoirSize,
          presentation,
          &state->presentationManager,
          NULL,
          0) ||
        !magGraphicsReserveCpuCompositor(
          &state->compositor,
          (UINT)reservoirSize.cx,
          (UINT)reservoirSize.cy))
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    state->capacityWidth = (UINT)reservoirSize.cx;
    state->capacityHeight = (UINT)reservoirSize.cy;
    state->presentationManagerHost = TRUE;
    state->presentationEnabled = TRUE;
    return TRUE;
}

static BOOL magGraphicsGdiIsAvailable(LPTSTR reason, UINT reasonCount)
{
    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    return TRUE;
}

static BOOL magGraphicsGdiCreate(
  HWND hWnd,
  SIZE clientSize,
  const struct MAGPRESENTATIONSETTINGS* presentation,
  void** stateOut)
{
    MAGGDISTATE* state;
    BITMAPINFO bmi = { 0 };

    if (!stateOut || !presentation || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *stateOut = NULL;

    state = (MAGGDISTATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }

    state->hColorDC = CreateCompatibleDC(NULL);
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = 1;
    bmi.bmiHeader.biHeight = -1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    state->hColorBitmap = CreateDIBSection(
      state->hColorDC,
      &bmi,
      DIB_RGB_COLORS,
      (void**)&state->colorPixel,
      NULL,
      0);

    if (!state->hColorDC || !state->hColorBitmap || !state->colorPixel ||
        !magGraphicsGdiCreatePresentationManager(
          hWnd,
          state,
          clientSize,
          presentation))
    {
      magGraphicsGdiReleaseState(state);
      return FALSE;
    }

    state->hColorBitmapOld = SelectBitmap(state->hColorDC, state->hColorBitmap);
    if (!state->hColorBitmapOld)
    {
      magGraphicsGdiReleaseState(state);
      return FALSE;
    }

    state->resourceGeneration = 1;
    *stateOut = state;
    return TRUE;
}

static void magGraphicsGdiDestroy(HWND hWnd, void* opaqueState)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);

    if (!state)
    {
      return;
    }

    magGraphicsGdiReleaseState(state);
}

static BOOL magGraphicsGdiResize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    if (!state->presentationManagerHost)
    {
      return TRUE;
    }
    if ((UINT)clientSize.cx > state->capacityWidth ||
        (UINT)clientSize.cy > state->capacityHeight ||
        !magPresentationManagerPresenterResize(
          state->presentationManager,
          clientSize))
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    return TRUE;
}

static BOOL magGraphicsGdiSetPresentationEnabled(
  HWND hWnd,
  void* opaqueState,
  BOOL enabled)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;

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
    state->presentationEnabled = enabled;
    return TRUE;
}

static BYTE magGraphicsGdiColorByte(FLOAT value)
{
    value = CLAMP(value, 0.0f, 1.0f);
    return (BYTE)(value * 255.0f + 0.5f);
}

static void magGraphicsGdiAlphaFill(MAGGDISTATE* state, HDC hDC, const RECT* rect, MAGCOLORF color)
{
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    const BYTE a = magGraphicsGdiColorByte(color.a);
    const BYTE r = (BYTE)((magGraphicsGdiColorByte(color.r) * a + 127) / 255);
    const BYTE g = (BYTE)((magGraphicsGdiColorByte(color.g) * a + 127) / 255);
    const BYTE b = (BYTE)((magGraphicsGdiColorByte(color.b) * a + 127) / 255);
    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;

    if (width <= 0 || height <= 0 || !a)
    {
      return;
    }

    *state->colorPixel = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | b;
    AlphaBlend(
      hDC,
      rect->left,
      rect->top,
      width,
      height,
      state->hColorDC,
      0,
      0,
      1,
      1,
      blend);
}

static void magGraphicsGdiStroke(MAGGDISTATE* state, HDC hDC, const MAGUIDRAWCOMMAND* command)
{
    const LONG thickness = max(1, (LONG)(command->thickness + 0.5f));
    RECT edges[4] =
    {
      { command->rect.left, command->rect.top, command->rect.right, command->rect.top + thickness },
      { command->rect.left, command->rect.bottom - thickness, command->rect.right, command->rect.bottom },
      { command->rect.left, command->rect.top + thickness, command->rect.left + thickness, command->rect.bottom - thickness },
      { command->rect.right - thickness, command->rect.top + thickness, command->rect.right, command->rect.bottom - thickness },
    };
    UINT i;

    for (i = 0; i < ARRAYSIZE(edges); ++i)
    {
      magGraphicsGdiAlphaFill(state, hDC, &edges[i], command->color);
    }
}

static BOOL magGraphicsGdiRender(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  const MAGPRESENTINTENT* intent)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;
    MAGPIXELBUFFER composed;
    BITMAPINFO bmi = { 0 };
    HDC hDC;
    UINT i;
    int copied;

    if (!state || !frame || !frame->pixels || !frame->width || !frame->height)
    {
      return FALSE;
    }

    if (state->presentationManagerHost)
    {
      ID3D11Texture2D* texture = NULL;
      UINT bufferIndex;
      D3D11_BOX box;

      if (!state->presentationEnabled || frame->width != state->width ||
          frame->height != state->height ||
          !magGraphicsComposeFrame(&state->compositor, frame, ui, &composed) ||
          !magPresentationManagerPresenterAcquire(
            state->presentationManager,
            !intent || intent->synchronize,
            &texture,
            &bufferIndex) ||
          !texture)
      {
        return FALSE;
      }
      box.left = 0;
      box.top = 0;
      box.front = 0;
      box.right = frame->width;
      box.bottom = frame->height;
      box.back = 1;
      ID3D11DeviceContext_UpdateSubresource(
        state->d3dContext,
        (ID3D11Resource*)texture,
        0,
        &box,
        composed.pixels,
        composed.stride,
        0);
      ID3D11Texture2D_Release(texture);
      return magPresentationManagerPresenterPresent(
        state->presentationManager,
        bufferIndex,
        (SIZE){ (LONG)frame->width, (LONG)frame->height },
        intent);
    }

    hDC = GetDC(hWnd);
    if (!hDC)
    {
      return FALSE;
    }

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = (LONG)(frame->stride / 4U);
    bmi.bmiHeader.biHeight = MAG_ROW_ORDER_TOP_DOWN == frame->rowOrder ? -(LONG)frame->height : (LONG)frame->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    copied = SetDIBitsToDevice(
      hDC,
      0,
      0,
      frame->width,
      frame->height,
      0,
      0,
      0,
      frame->height,
      frame->pixels,
      &bmi,
      DIB_RGB_COLORS);

    if (ui)
    {
      for (i = 0; i < ui->count; ++i)
      {
        if (MAG_UI_DRAW_FILL_RECT == ui->commands[i].type)
        {
          magGraphicsGdiAlphaFill(state, hDC, &ui->commands[i].rect, ui->commands[i].color);
        }
        else if (MAG_UI_DRAW_STROKE_RECT == ui->commands[i].type)
        {
          magGraphicsGdiStroke(state, hDC, &ui->commands[i]);
        }
      }
    }

    GdiFlush();
    ReleaseDC(hWnd, hDC);
    return 0 != copied;
}

static HANDLE magGraphicsGdiGetFrameWaitHandle(void* state)
{
    MAGGDISTATE* gdiState = (MAGGDISTATE*)state;
    return gdiState && gdiState->presentationManagerHost
      ? magPresentationManagerPresenterGetFrameWaitHandle(
          gdiState->presentationManager)
      : NULL;
}

static UINT64 magGraphicsGdiGetResourceGeneration(void* opaqueState)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;
    return state
      ? state->resourceGeneration + state->compositor.generation +
        magPresentationManagerPresenterGetResourceGeneration(
          state->presentationManager)
      : 0;
}

static BOOL magGraphicsGdiGetNextEstimatedFrameTime(
  void* opaqueState,
  LONGLONG* frameTime)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;
    return state && state->presentationManagerHost &&
      magPresentationManagerPresenterGetNextEstimatedFrameTime(
        state->presentationManager,
        frameTime);
}

static BOOL magGraphicsGdiGetObservedPresentationTarget(
  void* opaqueState,
  UINT* target)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;
    return state && state->presentationManagerHost &&
      magPresentationManagerPresenterGetObservedTarget(
        state->presentationManager,
        target);
}

const MAGGRAPHICSBACKEND g_magGraphicsGdiBackend =
{
  GRAPHICS_API_GDI,
  TEXT("GDI"),
  TRUE,
  magGraphicsGdiIsAvailable,
  magGraphicsGdiCreate,
  magGraphicsGdiDestroy,
  magGraphicsGdiResize,
  magGraphicsGdiSetPresentationEnabled,
  magGraphicsGdiRender,
  magGraphicsGdiGetFrameWaitHandle,
  magGraphicsGdiGetResourceGeneration,
  magGraphicsGdiGetNextEstimatedFrameTime,
  magGraphicsGdiGetObservedPresentationTarget,
};
