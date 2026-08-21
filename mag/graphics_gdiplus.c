#include "framework.h"
#include "gdiplusabi.h"
#include "graphics.h"
#include "graphics_presentation_manager.h"
#include "presentation.h"

#include <d3d11.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "gdiplus")

typedef struct MAGGDIPLUSSTATE
{
  ULONG_PTR gdiplusToken;
  HDC hSurfaceDC;
  HBITMAP hSurfaceBitmap;
  HBITMAP hSurfaceBitmapOld;
  BYTE* surfacePixels;
  BYTE* sourcePixels;
  GpBitmap* sourceBitmap;
  GpBitmap* surfaceBitmap;
  GpGraphics* surfaceGraphics;
  GpSolidFill* solidBrush;
  ID3D11Device* d3dDevice;
  ID3D11DeviceContext* d3dContext;
  MAGPRESENTATIONMANAGERPRESENTER* presentationManager;
  UINT width;
  UINT height;
  UINT capacityWidth;
  UINT capacityHeight;
  UINT stride;
  BOOL presentationManagerHost;
  BOOL presentationEnabled;
  UINT64 resourceGeneration;
} MAGGDIPLUSSTATE;

static void magGraphicsGdiPlusReleaseState(MAGGDIPLUSSTATE* state)
{
    if (!state)
    {
      return;
    }
    if (state->surfaceGraphics)
    {
      GdipDeleteGraphics(state->surfaceGraphics);
    }
    if (state->solidBrush)
    {
      GdipDeleteBrush((GpBrush*)state->solidBrush);
    }
    if (state->surfaceBitmap)
    {
      GdipDisposeImage((GpImage*)state->surfaceBitmap);
    }
    if (state->sourceBitmap)
    {
      GdipDisposeImage((GpImage*)state->sourceBitmap);
    }
    if (state->sourcePixels)
    {
      HeapFree(GetProcessHeap(), 0, state->sourcePixels);
    }
    if (state->hSurfaceDC && state->hSurfaceBitmapOld)
    {
      SelectBitmap(state->hSurfaceDC, state->hSurfaceBitmapOld);
    }
    if (state->hSurfaceBitmap)
    {
      DeleteBitmap(state->hSurfaceBitmap);
    }
    if (state->hSurfaceDC)
    {
      DeleteDC(state->hSurfaceDC);
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
    if (state->gdiplusToken)
    {
      GdiplusShutdown(state->gdiplusToken);
    }
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsGdiPlusCreatePresentationManager(
  HWND hWnd,
  MAGGDIPLUSSTATE* state,
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

    reservoirSize.cx = (LONG)state->capacityWidth;
    reservoirSize.cy = (LONG)state->capacityHeight;
    if (!magPresentationManagerPresenterCreate(
          hWnd,
          state->d3dDevice,
          reservoirSize,
          presentation,
          &state->presentationManager,
          NULL,
          0))
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    state->presentationManagerHost = TRUE;
    return TRUE;
}

static BOOL magGraphicsGdiPlusIsAvailable(LPTSTR reason, UINT reasonCount)
{
    HMODULE module = LoadLibraryEx(
      TEXT("gdiplus.dll"),
      NULL,
      LOAD_LIBRARY_SEARCH_SYSTEM32);

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!module)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("Gdiplus.dll is unavailable."), reasonCount);
      }
      return FALSE;
    }
    FreeLibrary(module);
    return TRUE;
}

static BOOL magGraphicsGdiPlusCreate(
  HWND hWnd,
  SIZE clientSize,
  const struct MAGPRESENTATIONSETTINGS* presentation,
  void** stateOut)
{
    MAGGDIPLUSSTATE* state;
    MAGGDIPLUSSTARTUPINPUT startupInput = { 1, NULL, FALSE, TRUE };
    BITMAPINFO bitmapInfo = { 0 };
    SIZE reservoirSize;
    SIZE_T pixelBytes;

    if (!stateOut || !presentation || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *stateOut = NULL;
    reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
    if (reservoirSize.cx < 1 || reservoirSize.cy < 1 ||
        (UINT)reservoirSize.cx > (UINT)MAXINT / 4U)
    {
      return FALSE;
    }
    pixelBytes = (SIZE_T)(UINT)reservoirSize.cx * 4U * (UINT)reservoirSize.cy;
    if (pixelBytes / (UINT)reservoirSize.cy != (SIZE_T)(UINT)reservoirSize.cx * 4U)
    {
      return FALSE;
    }

    state = (MAGGDIPLUSSTATE*)HeapAlloc(
      GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }
    if (MAG_GDIP_STATUS_OK != GdiplusStartup(
          &state->gdiplusToken, &startupInput, NULL))
    {
      magGraphicsGdiPlusReleaseState(state);
      return FALSE;
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    state->capacityWidth = (UINT)reservoirSize.cx;
    state->capacityHeight = (UINT)reservoirSize.cy;
    state->stride = state->capacityWidth * 4U;
    state->hSurfaceDC = CreateCompatibleDC(NULL);
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = reservoirSize.cx;
    bitmapInfo.bmiHeader.biHeight = -reservoirSize.cy;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    state->hSurfaceBitmap = CreateDIBSection(
      state->hSurfaceDC,
      &bitmapInfo,
      DIB_RGB_COLORS,
      (void**)&state->surfacePixels,
      NULL,
      0);
    if (state->hSurfaceDC && state->hSurfaceBitmap)
    {
      state->hSurfaceBitmapOld = SelectBitmap(
        state->hSurfaceDC, state->hSurfaceBitmap);
    }
    state->sourcePixels = (BYTE*)HeapAlloc(
      GetProcessHeap(), HEAP_ZERO_MEMORY, pixelBytes);

    if (!state->hSurfaceDC || !state->hSurfaceBitmap ||
        !state->hSurfaceBitmapOld || !state->surfacePixels ||
        !state->sourcePixels ||
        MAG_GDIP_STATUS_OK != GdipCreateBitmapFromScan0(
          (INT)state->capacityWidth,
          (INT)state->capacityHeight,
          (INT)state->stride,
          MAG_GDIP_PIXEL_FORMAT_32BPP_RGB,
          state->sourcePixels,
          &state->sourceBitmap) ||
        MAG_GDIP_STATUS_OK != GdipCreateBitmapFromScan0(
          (INT)state->capacityWidth,
          (INT)state->capacityHeight,
          (INT)state->stride,
          MAG_GDIP_PIXEL_FORMAT_32BPP_RGB,
          state->surfacePixels,
          &state->surfaceBitmap) ||
        MAG_GDIP_STATUS_OK != GdipGetImageGraphicsContext(
          (GpImage*)state->surfaceBitmap,
          &state->surfaceGraphics) ||
        MAG_GDIP_STATUS_OK != GdipCreateSolidFill(
          0xFFFFFFFFUL, &state->solidBrush) ||
        MAG_GDIP_STATUS_OK != GdipSetPageUnit(
          state->surfaceGraphics, MAG_GDIP_UNIT_PIXEL) ||
        MAG_GDIP_STATUS_OK != GdipSetCompositingQuality(
          state->surfaceGraphics,
          MAG_GDIP_COMPOSITING_QUALITY_HIGH_SPEED) ||
        MAG_GDIP_STATUS_OK != GdipSetInterpolationMode(
          state->surfaceGraphics,
          MAG_GDIP_INTERPOLATION_NEAREST_NEIGHBOR) ||
        MAG_GDIP_STATUS_OK != GdipSetPixelOffsetMode(
          state->surfaceGraphics,
          MAG_GDIP_PIXEL_OFFSET_HIGH_SPEED) ||
        !magGraphicsGdiPlusCreatePresentationManager(
          hWnd, state, clientSize, presentation))
    {
      magGraphicsGdiPlusReleaseState(state);
      return FALSE;
    }

    state->presentationEnabled = TRUE;
    state->resourceGeneration = 1;
    *stateOut = state;
    return TRUE;
}

static void magGraphicsGdiPlusDestroy(HWND hWnd, void* opaqueState)
{
    UNREFERENCED_PARAMETER(hWnd);
    magGraphicsGdiPlusReleaseState((MAGGDIPLUSSTATE*)opaqueState);
}

static BOOL magGraphicsGdiPlusResize(
  HWND hWnd,
  void* opaqueState,
  SIZE clientSize)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || clientSize.cx < 1 || clientSize.cy < 1 ||
        (UINT)clientSize.cx > state->capacityWidth ||
        (UINT)clientSize.cy > state->capacityHeight)
    {
      return FALSE;
    }
    if (state->presentationManagerHost &&
        !magPresentationManagerPresenterResize(
          state->presentationManager, clientSize))
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    return TRUE;
}

static BOOL magGraphicsGdiPlusSetPresentationEnabled(
  HWND hWnd,
  void* opaqueState,
  BOOL enabled)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return FALSE;
    }
    if (state->presentationManagerHost &&
        !magPresentationManagerPresenterSetEnabled(
          state->presentationManager, enabled))
    {
      return FALSE;
    }
    state->presentationEnabled = enabled;
    return TRUE;
}

static BYTE magGraphicsGdiPlusColorByte(FLOAT value)
{
    return (BYTE)(CLAMP(value, 0.0f, 1.0f) * 255.0f + 0.5f);
}

static MAGGDIPARGB magGraphicsGdiPlusArgb(MAGCOLORF color)
{
    return ((MAGGDIPARGB)magGraphicsGdiPlusColorByte(color.a) << 24) |
      ((MAGGDIPARGB)magGraphicsGdiPlusColorByte(color.r) << 16) |
      ((MAGGDIPARGB)magGraphicsGdiPlusColorByte(color.g) << 8) |
      (MAGGDIPARGB)magGraphicsGdiPlusColorByte(color.b);
}

static BOOL magGraphicsGdiPlusFill(
  MAGGDIPLUSSTATE* state,
  const RECT* sourceRect,
  MAGCOLORF color)
{
    RECT bounds = { 0, 0, (LONG)state->width, (LONG)state->height };
    RECT rect;

    if (!IntersectRect(&rect, sourceRect, &bounds))
    {
      return TRUE;
    }
    return MAG_GDIP_STATUS_OK == GdipSetSolidFillColor(
        state->solidBrush, magGraphicsGdiPlusArgb(color)) &&
      MAG_GDIP_STATUS_OK == GdipFillRectangleI(
        state->surfaceGraphics,
        (GpBrush*)state->solidBrush,
        rect.left,
        rect.top,
        rect.right - rect.left,
        rect.bottom - rect.top);
}

static BOOL magGraphicsGdiPlusDrawUi(
  MAGGDIPLUSSTATE* state,
  const MAGUIDRAWLIST* ui)
{
    UINT i;

    if (!ui)
    {
      return TRUE;
    }
    for (i = 0; i < ui->count; ++i)
    {
      const MAGUIDRAWCOMMAND* command = &ui->commands[i];

      if (MAG_UI_DRAW_FILL_RECT == command->type)
      {
        if (!magGraphicsGdiPlusFill(state, &command->rect, command->color))
        {
          return FALSE;
        }
      }
      else if (MAG_UI_DRAW_STROKE_RECT == command->type)
      {
        const LONG thickness = max(1, (LONG)(command->thickness + 0.5f));
        const RECT edges[] =
        {
          { command->rect.left, command->rect.top,
            command->rect.right, command->rect.top + thickness },
          { command->rect.left, command->rect.bottom - thickness,
            command->rect.right, command->rect.bottom },
          { command->rect.left, command->rect.top + thickness,
            command->rect.left + thickness, command->rect.bottom - thickness },
          { command->rect.right - thickness, command->rect.top + thickness,
            command->rect.right, command->rect.bottom - thickness },
        };
        UINT edge;

        for (edge = 0; edge < ARRAYSIZE(edges); ++edge)
        {
          if (!magGraphicsGdiPlusFill(state, &edges[edge], command->color))
          {
            return FALSE;
          }
        }
      }
    }
    return TRUE;
}

static BOOL magGraphicsGdiPlusRender(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  const MAGPRESENTINTENT* intent)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;
    UINT y;

    if (!state || !state->presentationEnabled || !frame || !frame->pixels ||
        !frame->width || !frame->height || frame->width != state->width ||
        frame->height != state->height || frame->width > state->capacityWidth ||
        frame->height > state->capacityHeight || frame->stride < frame->width * 4U)
    {
      return FALSE;
    }

    for (y = 0; y < frame->height; ++y)
    {
      const UINT sourceY = MAG_ROW_ORDER_TOP_DOWN == frame->rowOrder
        ? y
        : frame->height - 1U - y;
      CopyMemory(
        state->sourcePixels + (SIZE_T)y * state->stride,
        frame->pixels + (SIZE_T)sourceY * frame->stride,
        (SIZE_T)frame->width * 4U);
    }

    if (MAG_GDIP_STATUS_OK != GdipSetCompositingMode(
          state->surfaceGraphics, MAG_GDIP_COMPOSITING_SOURCE_COPY) ||
        MAG_GDIP_STATUS_OK != GdipDrawImageRectRectI(
          state->surfaceGraphics,
          (GpImage*)state->sourceBitmap,
          0,
          0,
          (INT)frame->width,
          (INT)frame->height,
          0,
          0,
          (INT)frame->width,
          (INT)frame->height,
          MAG_GDIP_UNIT_PIXEL,
          NULL,
          NULL,
          NULL) ||
        MAG_GDIP_STATUS_OK != GdipSetCompositingMode(
          state->surfaceGraphics, MAG_GDIP_COMPOSITING_SOURCE_OVER) ||
        !magGraphicsGdiPlusDrawUi(state, ui) ||
        MAG_GDIP_STATUS_OK != GdipFlush(
          state->surfaceGraphics, MAG_GDIP_FLUSH_SYNC))
    {
      return FALSE;
    }

    if (state->presentationManagerHost)
    {
      ID3D11Texture2D* texture = NULL;
      UINT bufferIndex;
      D3D11_BOX box;

      if (!magPresentationManagerPresenterAcquire(
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
        state->surfacePixels,
        state->stride,
        0);
      ID3D11Texture2D_Release(texture);
      return magPresentationManagerPresenterPresent(
        state->presentationManager,
        bufferIndex,
        (SIZE){ (LONG)frame->width, (LONG)frame->height },
        intent);
    }
    else
    {
      HDC windowDC = GetDC(hWnd);
      BOOL presented;

      if (!windowDC)
      {
        return FALSE;
      }
      presented = BitBlt(
        windowDC,
        0,
        0,
        (INT)frame->width,
        (INT)frame->height,
        state->hSurfaceDC,
        0,
        0,
        SRCCOPY);
      GdiFlush();
      ReleaseDC(hWnd, windowDC);
      return presented;
    }
}

static HANDLE magGraphicsGdiPlusGetFrameWaitHandle(void* opaqueState)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;
    return state && state->presentationManagerHost
      ? magPresentationManagerPresenterGetFrameWaitHandle(
          state->presentationManager)
      : NULL;
}

static UINT64 magGraphicsGdiPlusGetResourceGeneration(void* opaqueState)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;
    return state
      ? state->resourceGeneration +
        magPresentationManagerPresenterGetResourceGeneration(
          state->presentationManager)
      : 0;
}

static BOOL magGraphicsGdiPlusGetNextEstimatedFrameTime(
  void* opaqueState,
  LONGLONG* frameTime)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;
    return state && state->presentationManagerHost &&
      magPresentationManagerPresenterGetNextEstimatedFrameTime(
        state->presentationManager, frameTime);
}

static BOOL magGraphicsGdiPlusGetObservedPresentationTarget(
  void* opaqueState,
  UINT* target)
{
    MAGGDIPLUSSTATE* state = (MAGGDIPLUSSTATE*)opaqueState;
    return state && state->presentationManagerHost &&
      magPresentationManagerPresenterGetObservedTarget(
        state->presentationManager, target);
}

const MAGGRAPHICSBACKEND g_magGraphicsGdiPlusBackend =
{
  GRAPHICS_API_GDIPLUS,
  TEXT("GDI+"),
  TRUE,
  magGraphicsGdiPlusIsAvailable,
  magGraphicsGdiPlusCreate,
  magGraphicsGdiPlusDestroy,
  magGraphicsGdiPlusResize,
  magGraphicsGdiPlusSetPresentationEnabled,
  magGraphicsGdiPlusRender,
  magGraphicsGdiPlusGetFrameWaitHandle,
  magGraphicsGdiPlusGetResourceGeneration,
  magGraphicsGdiPlusGetNextEstimatedFrameTime,
  magGraphicsGdiPlusGetObservedPresentationTarget,
};
