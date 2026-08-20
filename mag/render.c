#include "render.h"
#include "mag.h"

#include <roapi.h>
#include <strsafe.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <stdio.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "runtimeobject")

#define SAFERELEASE(Obj) do { if ((Obj)) { IUnknown_Release((IUnknown*)(Obj)); (Obj) = NULL; } } while (0)

typedef __x_ABI_CWindows_CFoundation_CIClosable WGCCLOSABLE;
typedef __x_ABI_CWindows_CGraphics_CSizeInt32 WGCSIZE;
typedef __x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureItem WGCITEM;
typedef __x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureSession WGCSESSION;
typedef __x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureSession3 WGCSESSION3;
typedef __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFrame WGCFRAME;
typedef __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFramePool WGCFRAMEPOOL;
typedef __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFramePoolStatics2 WGCFRAMEPOOLSTATICS2;
typedef __x_ABI_CWindows_CGraphics_CDirectX_CDirect3D11_CIDirect3DDevice WGCD3DDEVICE;
typedef __x_ABI_CWindows_CGraphics_CDirectX_CDirect3D11_CIDirect3DSurface WGCSURFACE;

typedef struct IGraphicsCaptureItemInterop IGraphicsCaptureItemInterop;
typedef struct IGraphicsCaptureItemInteropVtbl
{
  BEGIN_INTERFACE
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(IGraphicsCaptureItemInterop* This, REFIID riid, void** ppvObject);
  ULONG (STDMETHODCALLTYPE* AddRef)(IGraphicsCaptureItemInterop* This);
  ULONG (STDMETHODCALLTYPE* Release)(IGraphicsCaptureItemInterop* This);
  HRESULT (STDMETHODCALLTYPE* CreateForWindow)(IGraphicsCaptureItemInterop* This, HWND window, REFIID riid, void** result);
  HRESULT (STDMETHODCALLTYPE* CreateForMonitor)(IGraphicsCaptureItemInterop* This, HMONITOR monitor, REFIID riid, void** result);
  END_INTERFACE
} IGraphicsCaptureItemInteropVtbl;

struct IGraphicsCaptureItemInterop
{
  CONST_VTBL struct IGraphicsCaptureItemInteropVtbl* lpVtbl;
};

static const IID IID_IGraphicsCaptureItemInterop =
  { 0x3628E81B, 0x3CAC, 0x4C60, { 0xB7, 0xF4, 0x23, 0xCE, 0x0E, 0x0C, 0x33, 0x56 } };

static const IID IID_WGC_IClosable =
  { 0x30D5A829, 0x7FA4, 0x4026, { 0x83, 0xBB, 0xD7, 0x5B, 0xAE, 0x4E, 0xA9, 0x9E } };

static const IID IID_WGC_IDirect3DDevice =
  { 0xA37624AB, 0x8D5F, 0x4650, { 0x9D, 0x3E, 0x9E, 0xAE, 0x3D, 0x9B, 0xC6, 0x70 } };

static const IID IID_WGC_IGraphicsCaptureItem =
  { 0x79C3F95B, 0x31F7, 0x4EC2, { 0xA4, 0x64, 0x63, 0x2E, 0xF5, 0xD3, 0x07, 0x60 } };

static const IID IID_WGC_IGraphicsCaptureSession3 =
  { 0xF2CDD966, 0x22AE, 0x5EA1, { 0x95, 0x96, 0x3A, 0x28, 0x93, 0x44, 0xC3, 0xBE } };

static const IID IID_WGC_IDirect3D11CaptureFramePoolStatics2 =
  { 0x589B103F, 0x6BBC, 0x5DF5, { 0xA9, 0x91, 0x02, 0xE2, 0x8B, 0x3B, 0x66, 0xD5 } };

typedef struct IDirect3DDxgiInterfaceAccess IDirect3DDxgiInterfaceAccess;
typedef struct IDirect3DDxgiInterfaceAccessVtbl
{
  BEGIN_INTERFACE
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(IDirect3DDxgiInterfaceAccess* This, REFIID riid, void** ppvObject);
  ULONG (STDMETHODCALLTYPE* AddRef)(IDirect3DDxgiInterfaceAccess* This);
  ULONG (STDMETHODCALLTYPE* Release)(IDirect3DDxgiInterfaceAccess* This);
  HRESULT (STDMETHODCALLTYPE* GetInterface)(IDirect3DDxgiInterfaceAccess* This, REFIID iid, void** p);
  END_INTERFACE
} IDirect3DDxgiInterfaceAccessVtbl;

struct IDirect3DDxgiInterfaceAccess
{
  CONST_VTBL struct IDirect3DDxgiInterfaceAccessVtbl* lpVtbl;
};

static const IID IID_IDirect3DDxgiInterfaceAccess =
  { 0xA9B3D012, 0x3DF2, 0x4EE3, { 0xB8, 0xD1, 0x86, 0x95, 0xF4, 0x57, 0xD3, 0xC1 } };

#define MINIMAP_MARGIN 12
#define MINIMAP_LABEL_HEIGHT 20
#define MINIMAP_LABEL_GAP 4
#define MINIMAP_TOP (MINIMAP_MARGIN + MINIMAP_LABEL_HEIGHT + MINIMAP_LABEL_GAP)
#define MINIMAP_MAX_WIDTH 220
#define MINIMAP_MAX_HEIGHT 140
#define MINIMAP_MIN_WIDTH 48
#define MINIMAP_MIN_HEIGHT 36
#define MINIMAP_VISIBLE_MS 1200U
#define MINIMAP_FADE_MS 450U
#define MINIMAP_MIN_ALPHA 0.01f

typedef struct MINIMAPLAYOUT
{
  RECT rcCapture;
  RECT rcMap;
} MINIMAPLAYOUT, *LPMINIMAPLAYOUT;

#define IDirect3DDxgiInterfaceAccess_GetInterface(This, iid, p) ((This)->lpVtbl->GetInterface((This), (iid), (p)))

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax);
void render_gdiCreateResources(HWND hWnd);
void render_gdiDeleteResources(HWND hWnd);
void render_gdiResizeSurface(HWND hWnd);
void render_buildUiDrawList(HWND hWnd, LPMAGUIDRAWLIST list);
BOOL render_presentPixelFrame(HWND hWnd);
BOOL render_tryPresentPixelFrame(HWND hWnd);
BOOL render_recreateGraphicsBackend(HWND hWnd);
BOOL render_stampPresentedContent(HWND hWnd);
static BOOL renderSubmitGeometryFrame(HWND hWnd, BOOL restartSequence, BOOL synchronize);
void render_gdiCaptureScreen(HWND hWnd);
void render_updateSurfaceInfo(HWND hWnd);
BOOL render_gdiCreateCaptureBitmap(HWND hWnd);
void render_gdiDeleteCaptureBitmap(HWND hWnd);
void render_dxgiDeleteResources(HWND hWnd);
void render_dxgiDeleteOutputResources(LPDXGIOUTPUTCAPTURE lpOutput);
LPDXGIOUTPUTCAPTURE render_dxgiFindOutputCapture(LPMAGSTATE lpsd, HMONITOR hMonitor);
LPDXGIOUTPUTCAPTURE render_dxgiFindFreeOutputCapture(LPMAGSTATE lpsd);
BOOL render_dxgiCreateDuplicationForMonitor(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput);
BOOL render_dxgiEnsureDuplication(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput);
BOOL render_dxgiEnsureStagingTexture(LPDXGIOUTPUTCAPTURE lpOutput, UINT width, UINT height);
BOOL render_dxgiUpdateFrame(LPDXGIOUTPUTCAPTURE lpOutput);
void render_dxgiCopyMappedPixelsToRect(LPMAGSTATE lpsd, const BYTE* src, UINT srcWidth, UINT srcHeight, UINT srcPitch, const RECT* lprcDst);
BOOL render_mapSourceRectToDestination(LPMAGSTATE lpsd, const RECT* lprcSource, const RECT* lprcPart, RECT* lprcDst);
BOOL render_sourceRectIsClipped(const RECT* lprcSource, const RECT* lprcClippedSource);
BOOL render_minimapGetCaptureRect(LPMAGSTATE lpsd, RECT* lprcCapture);
FLOAT render_minimapGetOpacity(LPMAGSTATE lpsd);
BOOL render_minimapHasVisibleState(LPMAGSTATE lpsd);
BOOL render_minimapComputeLayout(HWND hWnd, MINIMAPLAYOUT* lpLayout);
void render_minimapMapSourceRectToClient(const MINIMAPLAYOUT* lpLayout, const RECT* lprcSource, RECT* lprcClient);
POINT render_minimapClientPointToSource(const MINIMAPLAYOUT* lpLayout, POINT ptClient);
BOOL render_minimapSetSourceFromPoint(HWND hWnd, POINT ptClient);
BOOL render_dxgiCaptureIntersection(LPMAGSTATE lpsd, LPDXGIOUTPUTCAPTURE lpOutput, const RECT* lprcSource, const RECT* lprcIntersection);
void render_dxgiCaptureScreen(HWND hWnd);
void render_wgcCloseObject(IUnknown* object);
HRESULT render_wgcGetActivationFactory(PCWSTR pszRuntimeClass, REFIID riid, void** ppv);
BOOL render_wgcEnsureWinRt(HWND hWnd);
void render_wgcDeleteMonitorResources(LPWGCMONITORCAPTURE lpCapture);
void render_wgcDeleteResources(HWND hWnd);
LPWGCMONITORCAPTURE render_wgcFindMonitorCapture(LPMAGSTATE lpsd, HMONITOR hMonitor);
LPWGCMONITORCAPTURE render_wgcFindFreeMonitorCapture(LPMAGSTATE lpsd);
BOOL render_wgcCreateItemForMonitor(HMONITOR hMonitor, WGCITEM** lplpItem);
BOOL render_wgcCreateCaptureForMonitor(HWND hWnd, HMONITOR hMonitor, LPWGCMONITORCAPTURE* lplpCapture);
BOOL render_wgcEnsureCapture(HWND hWnd, HMONITOR hMonitor, LPWGCMONITORCAPTURE* lplpCapture);
BOOL render_wgcEnsureStagingTexture(LPWGCMONITORCAPTURE lpCapture, UINT width, UINT height);
BOOL render_wgcUpdateFrame(LPWGCMONITORCAPTURE lpCapture);
BOOL render_wgcCaptureIntersection(LPMAGSTATE lpsd, LPWGCMONITORCAPTURE lpCapture, const RECT* lprcSource, const RECT* lprcIntersection);
void render_wgcCaptureScreen(HWND hWnd);
void render_computeSourceRects(HWND hWnd, RECT* lprcSource, RECT* lprcClippedSource);
void render_computeSourceRect(HWND hWnd, RECT* lprcSource);
void render_dwmThumbnailDeleteResources(HWND hWnd);
BOOL render_dwmThumbnailEnsureResources(HWND hWnd);
BOOL render_dwmThumbnailCaptureScreen(HWND hWnd);
void render_dwmPrivateDeleteResources(HWND hWnd);
BOOL render_dwmPrivateEnsureResources(HWND hWnd);
UINT render_dwmPrivateTranslateDrawCommands(
  const MAGUIDRAWLIST* ui,
  DWMPRIVATEDRAWCOMMAND* lpCommands,
  UINT commandCapacity);
BOOL render_dwmPrivateCaptureScreen(HWND hWnd);
BOOL render_transitionCaptureApi(HWND hWnd, CAPTUREAPI captureApi);
BOOL render_captureUsesCompositor(CAPTUREAPI captureApi);
BOOL render_setGraphicsPresentationEnabled(HWND hWnd, BOOL enabled);
BOOL render_presentDwmFallback(HWND hWnd);
static int render_smokeFailure(int code, LPCTSTR reason);

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax)
{
    const LONG maxOrigin = clipMax - sourceExtent;

    if (maxOrigin < clipMin)
    {
      return clipMin;
    }

    return CLAMP(origin, clipMin, maxOrigin);
}

void render_updateSurfaceInfo(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    RECT rc = { 0 };
    LONG width;
    LONG height;

    GetClientRect(hWnd, &rc);
    width = RECTWIDTH(rc);
    height = RECTHEIGHT(rc);

    if (width < 1) width = 1;
    if (height < 1) height = 1;

    lpsd->rc = rc;
    lpsd->bi.biSize = sizeof(lpsd->bi);
    lpsd->bi.biWidth = width;
    lpsd->bi.biHeight = height;
    lpsd->bi.biPlanes = 1;
    lpsd->bi.biBitCount = BITS_PER_PIXEL;
    lpsd->bi.biCompression = BI_RGB;
    lpsd->bi.biSizeImage =
      (lpsd->frameCapacityWidth && lpsd->frameCapacityHeight)
        ? lpsd->frameCapacityWidth * lpsd->frameCapacityHeight * CHANNELS
        : width * height * CHANNELS;
    lpsd->frame.width = (UINT)width;
    lpsd->frame.height = (UINT)height;
    lpsd->frame.stride =
      (lpsd->frameCapacityWidth ? lpsd->frameCapacityWidth : (UINT)width) * CHANNELS;
    lpsd->frame.rowOrder = MAG_ROW_ORDER_TOP_DOWN;
    lpsd->frame.alphaMode = MAG_ALPHA_MODE_IGNORE;
}

BOOL render_gdiCreateCaptureBitmap(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BITMAPINFO bmi = { 0 };
    VOID* bits = NULL;
    SIZE minimumSize;
    SIZE reservoirSize;

    if (!lpsd->hDesktopDC || !lpsd->hCaptureDC)
    {
      return FALSE;
    }

    minimumSize.cx = max(1, lpsd->bi.biWidth);
    minimumSize.cy = max(1, lpsd->bi.biHeight);
    reservoirSize = magGraphicsChooseReservoirSize(hWnd, minimumSize);
    lpsd->frameCapacityWidth = (UINT)reservoirSize.cx;
    lpsd->frameCapacityHeight = (UINT)reservoirSize.cy;

    bmi.bmiHeader = lpsd->bi;
    bmi.bmiHeader.biWidth = reservoirSize.cx;
    bmi.bmiHeader.biHeight = -reservoirSize.cy;
    bmi.bmiHeader.biSizeImage =
      (DWORD)((SIZE_T)reservoirSize.cx * (SIZE_T)reservoirSize.cy * CHANNELS);
    lpsd->hBitmapBg = CreateDIBSection(lpsd->hDesktopDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

    if (!lpsd->hBitmapBg || !bits)
    {
      if (lpsd->hBitmapBg)
      {
        DeleteBitmap(lpsd->hBitmapBg);
        lpsd->hBitmapBg = NULL;
      }

      lpsd->frame.pixels = NULL;
      return FALSE;
    }

    lpsd->hBitmapOld = SelectBitmap(lpsd->hCaptureDC, lpsd->hBitmapBg);
    if (!lpsd->hBitmapOld)
    {
      DeleteBitmap(lpsd->hBitmapBg);
      lpsd->hBitmapBg = NULL;
      lpsd->frame.pixels = NULL;
      return FALSE;
    }

    lpsd->frame.pixels = (BYTE*)bits;
    ++lpsd->captureSurfaceGeneration;
    render_updateSurfaceInfo(hWnd);
    return TRUE;
}

void render_gdiDeleteCaptureBitmap(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hCaptureDC && lpsd->hBitmapOld)
    {
      SelectBitmap(lpsd->hCaptureDC, lpsd->hBitmapOld);
      lpsd->hBitmapOld = NULL;
    }

    if (lpsd->hBitmapBg)
    {
      DeleteBitmap(lpsd->hBitmapBg);
      lpsd->hBitmapBg = NULL;
    }

    lpsd->frame.pixels = NULL;
}

void render_dxgiDeleteOutputResources(LPDXGIOUTPUTCAPTURE lpOutput)
{
    SAFERELEASE(lpOutput->dxgiStagingTexture);
    SAFERELEASE(lpOutput->dxgiFrameTexture);
    SAFERELEASE(lpOutput->dxgiDuplication);
    SAFERELEASE(lpOutput->d3dContext);
    SAFERELEASE(lpOutput->d3dDevice);
    ZeroMemory(lpOutput, sizeof(*lpOutput));
}

void render_dxgiDeleteResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      render_dxgiDeleteOutputResources(&lpsd->dxgiOutputs[i]);
    }
}

LPDXGIOUTPUTCAPTURE render_dxgiFindOutputCapture(LPMAGSTATE lpsd, HMONITOR hMonitor)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      if (lpsd->dxgiOutputs[i].hMonitor == hMonitor)
      {
        return &lpsd->dxgiOutputs[i];
      }
    }

    return NULL;
}

LPDXGIOUTPUTCAPTURE render_dxgiFindFreeOutputCapture(LPMAGSTATE lpsd)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      if (!lpsd->dxgiOutputs[i].hMonitor)
      {
        return &lpsd->dxgiOutputs[i];
      }
    }

    return NULL;
}

BOOL render_dxgiCreateDuplicationForMonitor(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDXGIOUTPUTCAPTURE lpOutput = render_dxgiFindFreeOutputCapture(lpsd);
    IDXGIFactory1* factory = NULL;
    IDXGIAdapter1* adapter = NULL;
    IDXGIOutput* output = NULL;
    IDXGIOutput1* output1 = NULL;
    HRESULT hr;
    UINT adapterIndex;
    BOOL fResult = FALSE;

    if (!lpOutput)
    {
      return FALSE;
    }

    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory);
    if (FAILED(hr))
    {
      return FALSE;
    }

    for (adapterIndex = 0; SUCCEEDED(IDXGIFactory1_EnumAdapters1(factory, adapterIndex, &adapter)); ++adapterIndex)
    {
      UINT outputIndex;

      for (outputIndex = 0; SUCCEEDED(IDXGIAdapter1_EnumOutputs(adapter, outputIndex, &output)); ++outputIndex)
      {
        DXGI_OUTPUT_DESC outputDesc;

        if (SUCCEEDED(IDXGIOutput_GetDesc(output, &outputDesc)) && outputDesc.Monitor == hMonitor)
        {
          D3D11_TEXTURE2D_DESC frameDesc = { 0 };
          D3D_FEATURE_LEVEL featureLevel;

          hr = D3D11CreateDevice(
            (IDXGIAdapter*)adapter,
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,
            0,
            D3D11_SDK_VERSION,
            &lpOutput->d3dDevice,
            &featureLevel,
            &lpOutput->d3dContext);

          if (SUCCEEDED(hr))
          {
            hr = IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput1, (void**)&output1);
          }

          if (SUCCEEDED(hr))
          {
            hr = IDXGIOutput1_DuplicateOutput(output1, (IUnknown*)lpOutput->d3dDevice, &lpOutput->dxgiDuplication);
          }

          if (SUCCEEDED(hr))
          {
            frameDesc.Width = RECTWIDTH(outputDesc.DesktopCoordinates);
            frameDesc.Height = RECTHEIGHT(outputDesc.DesktopCoordinates);
            frameDesc.MipLevels = 1;
            frameDesc.ArraySize = 1;
            frameDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            frameDesc.SampleDesc.Count = 1;
            frameDesc.Usage = D3D11_USAGE_DEFAULT;

            hr = ID3D11Device_CreateTexture2D(lpOutput->d3dDevice, &frameDesc, NULL, &lpOutput->dxgiFrameTexture);
          }

          if (SUCCEEDED(hr))
          {
            lpOutput->hMonitor = hMonitor;
            lpOutput->rcOutput = outputDesc.DesktopCoordinates;
            *lplpOutput = lpOutput;
            fResult = TRUE;
          }

          SAFERELEASE(output1);
          SAFERELEASE(output);
          SAFERELEASE(adapter);
          SAFERELEASE(factory);

          if (!fResult)
          {
            render_dxgiDeleteOutputResources(lpOutput);
          }

          return fResult;
        }

        SAFERELEASE(output);
      }

      SAFERELEASE(adapter);
    }

    SAFERELEASE(factory);
    return FALSE;
}

BOOL render_dxgiEnsureDuplication(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDXGIOUTPUTCAPTURE lpOutput = render_dxgiFindOutputCapture(lpsd, hMonitor);

    if (lpOutput && lpOutput->dxgiDuplication)
    {
      *lplpOutput = lpOutput;
      return TRUE;
    }

    return render_dxgiCreateDuplicationForMonitor(hWnd, hMonitor, lplpOutput);
}

BOOL render_dxgiEnsureStagingTexture(LPDXGIOUTPUTCAPTURE lpOutput, UINT width, UINT height)
{
    D3D11_TEXTURE2D_DESC desc = { 0 };
    D3D11_TEXTURE2D_DESC frameDesc = { 0 };

    if (lpOutput->dxgiStagingTexture &&
        lpOutput->dxgiStagingWidth >= width &&
        lpOutput->dxgiStagingHeight >= height)
    {
      return TRUE;
    }

    SAFERELEASE(lpOutput->dxgiStagingTexture);
    lpOutput->dxgiStagingWidth = 0;
    lpOutput->dxgiStagingHeight = 0;

    if (lpOutput->dxgiFrameTexture)
    {
      ID3D11Texture2D_GetDesc(lpOutput->dxgiFrameTexture, &frameDesc);
    }
    desc.Width = max(width, frameDesc.Width);
    desc.Height = max(height, frameDesc.Height);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    if (FAILED(ID3D11Device_CreateTexture2D(lpOutput->d3dDevice, &desc, NULL, &lpOutput->dxgiStagingTexture)))
    {
      return FALSE;
    }

    lpOutput->dxgiStagingWidth = desc.Width;
    lpOutput->dxgiStagingHeight = desc.Height;
    return TRUE;
}

BOOL render_dxgiUpdateFrame(LPDXGIOUTPUTCAPTURE lpOutput)
{
    IDXGIResource* frameResource = NULL;
    ID3D11Texture2D* frameTexture = NULL;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = { 0 };
    HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(lpOutput->dxgiDuplication, 0, &frameInfo, &frameResource);

    if (DXGI_ERROR_WAIT_TIMEOUT == hr)
    {
      return lpOutput->fHasFrame;
    }

    if (DXGI_ERROR_ACCESS_LOST == hr)
    {
      render_dxgiDeleteOutputResources(lpOutput);
      return FALSE;
    }

    if (FAILED(hr))
    {
      return FALSE;
    }

    hr = IDXGIResource_QueryInterface(frameResource, &IID_ID3D11Texture2D, (void**)&frameTexture);
    if (SUCCEEDED(hr))
    {
      ID3D11DeviceContext_CopyResource(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiFrameTexture, (ID3D11Resource*)frameTexture);
      lpOutput->fHasFrame = TRUE;
    }

    SAFERELEASE(frameTexture);
    SAFERELEASE(frameResource);
    IDXGIOutputDuplication_ReleaseFrame(lpOutput->dxgiDuplication);

    return lpOutput->fHasFrame;
}

void render_dxgiCopyMappedPixelsToRect(LPMAGSTATE lpsd, const BYTE* src, UINT srcWidth, UINT srcHeight, UINT srcPitch, const RECT* lprcDst)
{
    const UINT dstWidth = (UINT)RECTWIDTH((*lprcDst));
    const UINT dstHeight = (UINT)RECTHEIGHT((*lprcDst));
    const UINT screenPitch = lpsd->frame.stride;
    UINT y;

    if (!dstWidth || !dstHeight || !srcWidth || !srcHeight)
    {
      return;
    }

    for (y = 0; y < dstHeight; ++y)
    {
      const UINT srcY = min((UINT)(((ULONGLONG)y * srcHeight) / dstHeight), srcHeight - 1);
      const UINT dstY = (UINT)lprcDst->top + y;
      BYTE* dstRow = lpsd->frame.pixels + (dstY * screenPitch) + ((UINT)lprcDst->left * CHANNELS);
      const BYTE* srcRow = src + (srcY * srcPitch);

      if (srcWidth == dstWidth)
      {
        CopyMemory(dstRow, srcRow, dstWidth * CHANNELS);
      }
      else
      {
        UINT x;

        for (x = 0; x < dstWidth; ++x)
        {
          const UINT srcX = min((UINT)(((ULONGLONG)x * srcWidth) / dstWidth), srcWidth - 1);
          CopyMemory(dstRow + (x * CHANNELS), srcRow + (srcX * CHANNELS), CHANNELS);
        }
      }
    }
}

BOOL render_mapSourceRectToDestination(LPMAGSTATE lpsd, const RECT* lprcSource, const RECT* lprcPart, RECT* lprcDst)
{
    const LONG sourceWidth = RECTWIDTH((*lprcSource));
    const LONG sourceHeight = RECTHEIGHT((*lprcSource));

    if (sourceWidth < 1 || sourceHeight < 1 || IsRectEmpty(lprcPart))
    {
      SetRectEmpty(lprcDst);
      return FALSE;
    }

    lprcDst->left = MulDiv(lprcPart->left - lprcSource->left, lpsd->bi.biWidth, sourceWidth);
    lprcDst->top = MulDiv(lprcPart->top - lprcSource->top, lpsd->bi.biHeight, sourceHeight);
    lprcDst->right = MulDiv(lprcPart->right - lprcSource->left, lpsd->bi.biWidth, sourceWidth);
    lprcDst->bottom = MulDiv(lprcPart->bottom - lprcSource->top, lpsd->bi.biHeight, sourceHeight);

    lprcDst->left = CLAMP(lprcDst->left, 0, lpsd->bi.biWidth);
    lprcDst->top = CLAMP(lprcDst->top, 0, lpsd->bi.biHeight);
    lprcDst->right = CLAMP(lprcDst->right, 0, lpsd->bi.biWidth);
    lprcDst->bottom = CLAMP(lprcDst->bottom, 0, lpsd->bi.biHeight);

    if (IsRectEmpty(lprcDst))
    {
      return FALSE;
    }

    return TRUE;
}

BOOL render_sourceRectIsClipped(const RECT* lprcSource, const RECT* lprcClippedSource)
{
    return lprcSource->left != lprcClippedSource->left ||
           lprcSource->top != lprcClippedSource->top ||
           lprcSource->right != lprcClippedSource->right ||
           lprcSource->bottom != lprcClippedSource->bottom;
}

BOOL render_minimapGetCaptureRect(LPMAGSTATE lpsd, RECT* lprcCapture)
{
    *lprcCapture = lpsd->di.rc;

    if (IsRectEmpty(lprcCapture))
    {
      lprcCapture->left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      lprcCapture->top = GetSystemMetrics(SM_YVIRTUALSCREEN);
      lprcCapture->right = lprcCapture->left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
      lprcCapture->bottom = lprcCapture->top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }

    return RECTWIDTH((*lprcCapture)) > 0 && RECTHEIGHT((*lprcCapture)) > 0;
}

void render_minimapNotifyActivity(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd)
    {
      lpsd->dwMiniMapLastActivity = GetTickCount();
    }
}

BOOL render_minimapHasVisibleState(LPMAGSTATE lpsd)
{
    if (MAG_VIEW_LENS == lpsd->viewMode)
    {
      return FALSE;
    }

    return lpsd->fTexScaler > 1.0001f ||
           (!lpsd->fTrackCursor && lpsd->fUseSourceOrigin && lpsd->fSourceOriginPinned);
}

FLOAT render_minimapGetOpacity(LPMAGSTATE lpsd)
{
    const DWORD now = GetTickCount();
    const DWORD elapsed = now - lpsd->dwMiniMapLastActivity;

    if (!render_minimapHasVisibleState(lpsd))
    {
      return 0.0f;
    }

    if (lpsd->fMiniMapDragging || lpsd->fMiniMapHoldVisible)
    {
      return 1.0f;
    }

    if (!lpsd->dwMiniMapLastActivity)
    {
      return 1.0f;
    }

    if (elapsed <= MINIMAP_VISIBLE_MS)
    {
      return 1.0f;
    }

    if (elapsed >= MINIMAP_VISIBLE_MS + MINIMAP_FADE_MS)
    {
      return 0.0f;
    }

    return 1.0f - ((FLOAT)(elapsed - MINIMAP_VISIBLE_MS) / (FLOAT)MINIMAP_FADE_MS);
}

BOOL render_minimapComputeLayout(HWND hWnd, MINIMAPLAYOUT* lpLayout)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const LONG clientWidth = lpsd->bi.biWidth;
    const LONG clientHeight = lpsd->bi.biHeight;
    const LONG maxWidth = min(MINIMAP_MAX_WIDTH, clientWidth - (MINIMAP_MARGIN * 2));
    const LONG maxHeight = min(
      MINIMAP_MAX_HEIGHT,
      clientHeight - MINIMAP_TOP - MINIMAP_MARGIN);
    const LONG captureWidth = render_minimapGetCaptureRect(lpsd, &lpLayout->rcCapture) ? RECTWIDTH(lpLayout->rcCapture) : 0;
    const LONG captureHeight = RECTHEIGHT(lpLayout->rcCapture);
    LONG mapWidth;
    LONG mapHeight;

    if (!render_minimapHasVisibleState(lpsd) ||
        maxWidth < MINIMAP_MIN_WIDTH ||
        maxHeight < MINIMAP_MIN_HEIGHT ||
        captureWidth < 1 ||
        captureHeight < 1)
    {
      SetRectEmpty(&lpLayout->rcMap);
      return FALSE;
    }

    if (MulDiv(maxWidth, captureHeight, captureWidth) <= maxHeight)
    {
      mapWidth = maxWidth;
      mapHeight = max(1, MulDiv(maxWidth, captureHeight, captureWidth));
    }
    else
    {
      mapHeight = maxHeight;
      mapWidth = max(1, MulDiv(maxHeight, captureWidth, captureHeight));
    }

    SetRect(
      &lpLayout->rcMap,
      MINIMAP_MARGIN,
      MINIMAP_TOP,
      MINIMAP_MARGIN + mapWidth,
      MINIMAP_TOP + mapHeight);

    return TRUE;
}

void render_minimapMapSourceRectToClient(const MINIMAPLAYOUT* lpLayout, const RECT* lprcSource, RECT* lprcClient)
{
    const LONG captureWidth = RECTWIDTH(lpLayout->rcCapture);
    const LONG captureHeight = RECTHEIGHT(lpLayout->rcCapture);
    const LONG mapWidth = RECTWIDTH(lpLayout->rcMap);
    const LONG mapHeight = RECTHEIGHT(lpLayout->rcMap);

    lprcClient->left = lpLayout->rcMap.left + MulDiv(lprcSource->left - lpLayout->rcCapture.left, mapWidth, captureWidth);
    lprcClient->top = lpLayout->rcMap.top + MulDiv(lprcSource->top - lpLayout->rcCapture.top, mapHeight, captureHeight);
    lprcClient->right = lpLayout->rcMap.left + MulDiv(lprcSource->right - lpLayout->rcCapture.left, mapWidth, captureWidth);
    lprcClient->bottom = lpLayout->rcMap.top + MulDiv(lprcSource->bottom - lpLayout->rcCapture.top, mapHeight, captureHeight);
}

POINT render_minimapClientPointToSource(const MINIMAPLAYOUT* lpLayout, POINT ptClient)
{
    POINT ptSource;
    const LONG captureWidth = RECTWIDTH(lpLayout->rcCapture);
    const LONG captureHeight = RECTHEIGHT(lpLayout->rcCapture);
    const LONG mapWidth = RECTWIDTH(lpLayout->rcMap);
    const LONG mapHeight = RECTHEIGHT(lpLayout->rcMap);

    ptClient.x = CLAMP(ptClient.x, lpLayout->rcMap.left, lpLayout->rcMap.right);
    ptClient.y = CLAMP(ptClient.y, lpLayout->rcMap.top, lpLayout->rcMap.bottom);
    ptSource.x = lpLayout->rcCapture.left + MulDiv(ptClient.x - lpLayout->rcMap.left, captureWidth, mapWidth);
    ptSource.y = lpLayout->rcCapture.top + MulDiv(ptClient.y - lpLayout->rcMap.top, captureHeight, mapHeight);
    ptSource.x = CLAMP(ptSource.x, lpLayout->rcCapture.left, lpLayout->rcCapture.right);
    ptSource.y = CLAMP(ptSource.y, lpLayout->rcCapture.top, lpLayout->rcCapture.bottom);
    return ptSource;
}

static MAGCOLORF render_uiColor(FLOAT r, FLOAT g, FLOAT b, FLOAT a)
{
    MAGCOLORF color = { r, g, b, a };
    return color;
}

void render_buildUiDrawList(HWND hWnd, LPMAGUIDRAWLIST list)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    MINIMAPLAYOUT layout;
    RECT rcSource;
    RECT rcClippedSource;
    RECT rcWindow;
    RECT rcClippedWindow;
    RECT rcWindowClient;
    RECT rcPanel;
    RECT rcVisibleClient;
    RECT rcLabel;
    TCHAR label[MAG_UI_MAX_TEXT_LENGTH];
    FLOAT opacity;
    UINT i;

    magUiDrawListReset(list);
    if (!lpsd || !list)
    {
      return;
    }

    if (render_minimapComputeLayout(hWnd, &layout))
    {
      opacity = render_minimapGetOpacity(lpsd);
      if (opacity > MINIMAP_MIN_ALPHA)
      {
        render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
        rcPanel = layout.rcMap;
        InflateRect(&rcPanel, 4, 4);
        rcLabel = layout.rcMap;
        rcLabel.top -= MINIMAP_LABEL_HEIGHT;
        rcLabel.bottom = layout.rcMap.top - MINIMAP_LABEL_GAP;
        rcPanel.top = rcLabel.top - 4;

        magUiDrawListAppendFill(list, &rcPanel, render_uiColor(0.02f, 0.025f, 0.03f, 0.72f * opacity));
        magUiDrawListAppendFill(list, &layout.rcMap, render_uiColor(0.11f, 0.12f, 0.13f, 0.78f * opacity));

        for (i = 0; i < lpsd->di.numMonitors; ++i)
        {
          RECT rcMonitor;

          render_minimapMapSourceRectToClient(&layout, &lpsd->di.monitors[i].monitorInfoEx.rcMonitor, &rcMonitor);
          magUiDrawListAppendFill(list, &rcMonitor, render_uiColor(0.18f, 0.19f, 0.20f, 0.58f * opacity));
        }

        for (i = 0; i < lpsd->di.numMonitors; ++i)
        {
          RECT rcMonitor;

          render_minimapMapSourceRectToClient(&layout, &lpsd->di.monitors[i].monitorInfoEx.rcMonitor, &rcMonitor);
          magUiDrawListAppendStroke(list, &rcMonitor, render_uiColor(0.55f, 0.58f, 0.62f, 0.70f * opacity), 1.0f);
        }

        magUiDrawListAppendStroke(list, &layout.rcMap, render_uiColor(0.78f, 0.82f, 0.88f, 0.88f * opacity), 1.0f);

        if (GetWindowRect(hWnd, &rcWindow) &&
            IntersectRect(&rcClippedWindow, &rcWindow, &layout.rcCapture))
        {
          render_minimapMapSourceRectToClient(&layout, &rcClippedWindow, &rcWindowClient);
          magUiDrawListAppendFill(list, &rcWindowClient, render_uiColor(0.38f, 0.40f, 0.43f, 0.10f * opacity));
          magUiDrawListAppendStroke(list, &rcWindowClient, render_uiColor(0.48f, 0.50f, 0.54f, 0.92f * opacity), 1.5f);
        }

        if (!IsRectEmpty(&rcClippedSource))
        {
          render_minimapMapSourceRectToClient(&layout, &rcClippedSource, &rcVisibleClient);
          magUiDrawListAppendFill(list, &rcVisibleClient, render_uiColor(0.86f, 0.92f, 1.0f, 0.12f * opacity));
          magUiDrawListAppendStroke(list, &rcVisibleClient, render_uiColor(0.90f, 0.96f, 1.0f, 0.96f * opacity), 2.0f);
        }

        StringCchPrintf(
          label,
          ARRAYSIZE(label),
          TEXT("%.2fx  %s"),
          lpsd->fTexScaler,
          lpsd->graphicsBackend ? lpsd->graphicsBackend->name : TEXT("No renderer"));
        magUiDrawListAppendText(
          list,
          &rcLabel,
          render_uiColor(0.90f, 0.94f, 1.0f, 0.96f * opacity),
          12.0f,
          label);
      }
    }

    if (lpsd->bi.biWidth >= 2 && lpsd->bi.biHeight >= 2)
    {
      const RECT edges[] =
      {
        { 0, 0, lpsd->bi.biWidth, 1 },
        { 0, lpsd->bi.biHeight - 1, lpsd->bi.biWidth, lpsd->bi.biHeight },
        { 0, 1, 1, lpsd->bi.biHeight - 1 },
        { lpsd->bi.biWidth - 1, 1, lpsd->bi.biWidth, lpsd->bi.biHeight - 1 },
      };
      const MAGCOLORF outline =
      {
        lpsd->outlineColor[0],
        lpsd->outlineColor[1],
        lpsd->outlineColor[2],
        lpsd->outlineColor[3],
      };

      for (i = 0; i < ARRAYSIZE(edges); ++i)
      {
        magUiDrawListAppendFill(list, &edges[i], outline);
      }
    }
}

BOOL render_minimapHitTest(HWND hWnd, POINT ptClient)
{
    MINIMAPLAYOUT layout;

    return render_minimapComputeLayout(hWnd, &layout) && PtInRect(&layout.rcMap, ptClient);
}

BOOL render_minimapSetSourceFromPoint(HWND hWnd, POINT ptClient)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    MINIMAPLAYOUT layout;
    RECT rcSource;
    RECT rcClippedSource;
    POINT ptSource;
    LONG srcW;
    LONG srcH;
    LONG srcX;
    LONG srcY;

    if (!render_minimapComputeLayout(hWnd, &layout))
    {
      return FALSE;
    }

    render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
    srcW = RECTWIDTH(rcSource);
    srcH = RECTHEIGHT(rcSource);
    if (srcW < 1 || srcH < 1)
    {
      return FALSE;
    }

    ptSource = render_minimapClientPointToSource(&layout, ptClient);
    srcX = ptSource.x - lpsd->ptMiniMapDragOffset.x;
    srcY = ptSource.y - lpsd->ptMiniMapDragOffset.y;
    srcX = render_clipSourceOrigin(srcX, srcW, layout.rcCapture.left, layout.rcCapture.right);
    srcY = render_clipSourceOrigin(srcY, srcH, layout.rcCapture.top, layout.rcCapture.bottom);

    if (lpsd->fTrackCursor)
    {
      return TRUE;
    }

    lpsd->fUseSourceOrigin = TRUE;
    lpsd->fSourceOriginPinned = TRUE;
    lpsd->ptSourceOrigin.x = srcX;
    lpsd->ptSourceOrigin.y = srcY;
    return TRUE;
}

BOOL render_minimapBeginDrag(HWND hWnd, POINT ptClient)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    MINIMAPLAYOUT layout;
    RECT rcSource;
    RECT rcClippedSource;
    POINT ptSource;

    if (!render_minimapComputeLayout(hWnd, &layout) || !PtInRect(&layout.rcMap, ptClient))
    {
      return FALSE;
    }

    render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
    if (IsRectEmpty(&rcSource))
    {
      return FALSE;
    }

    ptSource = render_minimapClientPointToSource(&layout, ptClient);
    if (!IsRectEmpty(&rcClippedSource) && PtInRect(&rcClippedSource, ptSource))
    {
      lpsd->ptMiniMapDragOffset.x = ptSource.x - rcSource.left;
      lpsd->ptMiniMapDragOffset.y = ptSource.y - rcSource.top;
    }
    else
    {
      lpsd->ptMiniMapDragOffset.x = RECTWIDTH(rcSource) / 2;
      lpsd->ptMiniMapDragOffset.y = RECTHEIGHT(rcSource) / 2;
    }

    lpsd->fMiniMapDragging = TRUE;
    return render_minimapSetSourceFromPoint(hWnd, ptClient);
}

BOOL render_minimapDrag(HWND hWnd, POINT ptClient)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd->fMiniMapDragging)
    {
      return FALSE;
    }

    return render_minimapSetSourceFromPoint(hWnd, ptClient);
}

void render_minimapEndDrag(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    lpsd->fMiniMapDragging = FALSE;
}

BOOL render_dxgiCaptureIntersection(LPMAGSTATE lpsd, LPDXGIOUTPUTCAPTURE lpOutput, const RECT* lprcSource, const RECT* lprcIntersection)
{
    const UINT srcPartWidth = (UINT)RECTWIDTH((*lprcIntersection));
    const UINT srcPartHeight = (UINT)RECTHEIGHT((*lprcIntersection));
    RECT rcDst;
    D3D11_BOX box;
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;

    if (!render_dxgiUpdateFrame(lpOutput) ||
        !render_dxgiEnsureStagingTexture(lpOutput, srcPartWidth, srcPartHeight))
    {
      return FALSE;
    }

    box.left = (UINT)(lprcIntersection->left - lpOutput->rcOutput.left);
    box.top = (UINT)(lprcIntersection->top - lpOutput->rcOutput.top);
    box.front = 0;
    box.right = box.left + srcPartWidth;
    box.bottom = box.top + srcPartHeight;
    box.back = 1;

    ID3D11DeviceContext_CopySubresourceRegion(
      lpOutput->d3dContext,
      (ID3D11Resource*)lpOutput->dxgiStagingTexture,
      0,
      0,
      0,
      0,
      (ID3D11Resource*)lpOutput->dxgiFrameTexture,
      0,
      &box);

    hr = ID3D11DeviceContext_Map(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
      return FALSE;
    }

    if (!render_mapSourceRectToDestination(lpsd, lprcSource, lprcIntersection, &rcDst))
    {
      ID3D11DeviceContext_Unmap(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiStagingTexture, 0);
      return TRUE;
    }

    render_dxgiCopyMappedPixelsToRect(lpsd, (const BYTE*)mapped.pData, srcPartWidth, srcPartHeight, mapped.RowPitch, &rcDst);
    ID3D11DeviceContext_Unmap(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiStagingTexture, 0);

    return TRUE;
}

void render_wgcCloseObject(IUnknown* object)
{
    WGCCLOSABLE* closable = NULL;

    if (object &&
        SUCCEEDED(IUnknown_QueryInterface(object, &IID_WGC_IClosable, (void**)&closable)))
    {
      __x_ABI_CWindows_CFoundation_CIClosable_Close(closable);
      SAFERELEASE(closable);
    }
}

HRESULT render_wgcGetActivationFactory(PCWSTR pszRuntimeClass, REFIID riid, void** ppv)
{
    HSTRING runtimeClass = NULL;
    HRESULT hr;

    *ppv = NULL;

    hr = WindowsCreateString(pszRuntimeClass, (UINT32)lstrlenW(pszRuntimeClass), &runtimeClass);
    if (SUCCEEDED(hr))
    {
      hr = RoGetActivationFactory(runtimeClass, riid, ppv);
      WindowsDeleteString(runtimeClass);
    }

    return hr;
}

BOOL render_wgcEnsureWinRt(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    HRESULT hr;

    if (lpsd->fWinRtInitialized)
    {
      return TRUE;
    }

    hr = RoInitialize(RO_INIT_MULTITHREADED);
    if (SUCCEEDED(hr))
    {
      lpsd->fWinRtInitialized = TRUE;
      return TRUE;
    }

    return RPC_E_CHANGED_MODE == hr;
}

void render_wgcDeleteMonitorResources(LPWGCMONITORCAPTURE lpCapture)
{
    render_wgcCloseObject(lpCapture->wgcSession);
    render_wgcCloseObject(lpCapture->wgcFramePool);

    SAFERELEASE(lpCapture->wgcStagingTexture);
    SAFERELEASE(lpCapture->wgcFrameTexture);
    SAFERELEASE(lpCapture->wgcSession);
    SAFERELEASE(lpCapture->wgcFramePool);
    SAFERELEASE(lpCapture->wgcItem);
    SAFERELEASE(lpCapture->wgcDevice);
    SAFERELEASE(lpCapture->d3dContext);
    SAFERELEASE(lpCapture->d3dDevice);
    ZeroMemory(lpCapture, sizeof(*lpCapture));
}

void render_wgcDeleteResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->wgcMonitors); ++i)
    {
      render_wgcDeleteMonitorResources(&lpsd->wgcMonitors[i]);
    }
}

LPWGCMONITORCAPTURE render_wgcFindMonitorCapture(LPMAGSTATE lpsd, HMONITOR hMonitor)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->wgcMonitors); ++i)
    {
      if (lpsd->wgcMonitors[i].hMonitor == hMonitor)
      {
        return &lpsd->wgcMonitors[i];
      }
    }

    return NULL;
}

LPWGCMONITORCAPTURE render_wgcFindFreeMonitorCapture(LPMAGSTATE lpsd)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->wgcMonitors); ++i)
    {
      if (!lpsd->wgcMonitors[i].hMonitor)
      {
        return &lpsd->wgcMonitors[i];
      }
    }

    return NULL;
}

BOOL render_wgcCreateItemForMonitor(HMONITOR hMonitor, WGCITEM** lplpItem)
{
    IGraphicsCaptureItemInterop* interop = NULL;
    HRESULT hr;

    *lplpItem = NULL;

    hr = render_wgcGetActivationFactory(
      RuntimeClass_Windows_Graphics_Capture_GraphicsCaptureItem,
      &IID_IGraphicsCaptureItemInterop,
      (void**)&interop);

    if (SUCCEEDED(hr))
    {
      hr = interop->lpVtbl->CreateForMonitor(
        interop,
        hMonitor,
        &IID_WGC_IGraphicsCaptureItem,
        (void**)lplpItem);
    }

    SAFERELEASE(interop);
    return SUCCEEDED(hr) && *lplpItem;
}

BOOL render_wgcCreateCaptureForMonitor(HWND hWnd, HMONITOR hMonitor, LPWGCMONITORCAPTURE* lplpCapture)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPWGCMONITORCAPTURE lpCapture = render_wgcFindFreeMonitorCapture(lpsd);
    IDXGIFactory1* factory = NULL;
    IDXGIAdapter1* adapter = NULL;
    IDXGIOutput* output = NULL;
    IDXGIDevice* dxgiDevice = NULL;
    IInspectable* inspectableDevice = NULL;
    WGCITEM* item = NULL;
    WGCFRAMEPOOLSTATICS2* framePoolStatics = NULL;
    WGCSESSION3* session3 = NULL;
    HRESULT hr;
    UINT adapterIndex;
    BOOL fResult = FALSE;

    if (!lpCapture || !render_wgcEnsureWinRt(hWnd))
    {
      return FALSE;
    }

    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory);
    if (FAILED(hr))
    {
      return FALSE;
    }

    for (adapterIndex = 0; SUCCEEDED(IDXGIFactory1_EnumAdapters1(factory, adapterIndex, &adapter)); ++adapterIndex)
    {
      UINT outputIndex;

      for (outputIndex = 0; SUCCEEDED(IDXGIAdapter1_EnumOutputs(adapter, outputIndex, &output)); ++outputIndex)
      {
        DXGI_OUTPUT_DESC outputDesc;

        if (SUCCEEDED(IDXGIOutput_GetDesc(output, &outputDesc)) && outputDesc.Monitor == hMonitor)
        {
          D3D11_TEXTURE2D_DESC frameDesc = { 0 };
          D3D_FEATURE_LEVEL featureLevel;
          WGCSIZE itemSize = { 0 };

          hr = D3D11CreateDevice(
            (IDXGIAdapter*)adapter,
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,
            0,
            D3D11_SDK_VERSION,
            &lpCapture->d3dDevice,
            &featureLevel,
            &lpCapture->d3dContext);

          if (SUCCEEDED(hr))
          {
            hr = ID3D11Device_QueryInterface(lpCapture->d3dDevice, &IID_IDXGIDevice, (void**)&dxgiDevice);
          }

          if (SUCCEEDED(hr))
          {
            hr = CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice, &inspectableDevice);
          }

          if (SUCCEEDED(hr))
          {
            hr = IUnknown_QueryInterface(
              (IUnknown*)inspectableDevice,
              &IID_WGC_IDirect3DDevice,
              (void**)&lpCapture->wgcDevice);
          }

          if (SUCCEEDED(hr))
          {
            if (render_wgcCreateItemForMonitor(hMonitor, &item))
            {
              lpCapture->wgcItem = (IUnknown*)item;
              item = NULL;
              hr = __x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureItem_get_Size((WGCITEM*)lpCapture->wgcItem, &itemSize);
            }
            else
            {
              hr = E_FAIL;
            }
          }

          if (SUCCEEDED(hr) && (itemSize.Width < 1 || itemSize.Height < 1))
          {
            itemSize.Width = RECTWIDTH(outputDesc.DesktopCoordinates);
            itemSize.Height = RECTHEIGHT(outputDesc.DesktopCoordinates);
          }

          if (SUCCEEDED(hr))
          {
            hr = render_wgcGetActivationFactory(
              RuntimeClass_Windows_Graphics_Capture_Direct3D11CaptureFramePool,
              &IID_WGC_IDirect3D11CaptureFramePoolStatics2,
              (void**)&framePoolStatics);
          }

          if (SUCCEEDED(hr))
          {
            hr = __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFramePoolStatics2_CreateFreeThreaded(
              framePoolStatics,
              (WGCD3DDEVICE*)lpCapture->wgcDevice,
              DirectXPixelFormat_B8G8R8A8UIntNormalized,
              2,
              itemSize,
              (WGCFRAMEPOOL**)&lpCapture->wgcFramePool);
          }

          if (SUCCEEDED(hr))
          {
            hr = __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFramePool_CreateCaptureSession(
              (WGCFRAMEPOOL*)lpCapture->wgcFramePool,
              (WGCITEM*)lpCapture->wgcItem,
              (WGCSESSION**)&lpCapture->wgcSession);
          }

          if (SUCCEEDED(hr) &&
              SUCCEEDED(IUnknown_QueryInterface(
                lpCapture->wgcSession,
                &IID_WGC_IGraphicsCaptureSession3,
                (void**)&session3)))
          {
            __x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureSession3_put_IsBorderRequired(session3, FALSE);
          }

          if (SUCCEEDED(hr))
          {
            hr = __x_ABI_CWindows_CGraphics_CCapture_CIGraphicsCaptureSession_StartCapture((WGCSESSION*)lpCapture->wgcSession);
          }

          if (SUCCEEDED(hr))
          {
            frameDesc.Width = (UINT)itemSize.Width;
            frameDesc.Height = (UINT)itemSize.Height;
            frameDesc.MipLevels = 1;
            frameDesc.ArraySize = 1;
            frameDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            frameDesc.SampleDesc.Count = 1;
            frameDesc.Usage = D3D11_USAGE_DEFAULT;

            hr = ID3D11Device_CreateTexture2D(lpCapture->d3dDevice, &frameDesc, NULL, &lpCapture->wgcFrameTexture);
          }

          if (SUCCEEDED(hr))
          {
            lpCapture->hMonitor = hMonitor;
            lpCapture->rcOutput = outputDesc.DesktopCoordinates;
            lpCapture->wgcFrameWidth = (UINT)itemSize.Width;
            lpCapture->wgcFrameHeight = (UINT)itemSize.Height;
            *lplpCapture = lpCapture;
            fResult = TRUE;
          }

          SAFERELEASE(session3);
          SAFERELEASE(framePoolStatics);
          SAFERELEASE(item);
          SAFERELEASE(inspectableDevice);
          SAFERELEASE(dxgiDevice);
          SAFERELEASE(output);
          SAFERELEASE(adapter);
          SAFERELEASE(factory);

          if (!fResult)
          {
            render_wgcDeleteMonitorResources(lpCapture);
          }

          return fResult;
        }

        SAFERELEASE(output);
      }

      SAFERELEASE(adapter);
    }

    SAFERELEASE(factory);
    return FALSE;
}

BOOL render_wgcEnsureCapture(HWND hWnd, HMONITOR hMonitor, LPWGCMONITORCAPTURE* lplpCapture)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPWGCMONITORCAPTURE lpCapture = render_wgcFindMonitorCapture(lpsd, hMonitor);

    if (lpCapture && lpCapture->wgcFramePool && lpCapture->wgcSession)
    {
      *lplpCapture = lpCapture;
      return TRUE;
    }

    return render_wgcCreateCaptureForMonitor(hWnd, hMonitor, lplpCapture);
}

BOOL render_wgcEnsureStagingTexture(LPWGCMONITORCAPTURE lpCapture, UINT width, UINT height)
{
    D3D11_TEXTURE2D_DESC desc = { 0 };

    if (lpCapture->wgcStagingTexture &&
        lpCapture->wgcStagingWidth >= width &&
        lpCapture->wgcStagingHeight >= height)
    {
      return TRUE;
    }

    SAFERELEASE(lpCapture->wgcStagingTexture);
    lpCapture->wgcStagingWidth = 0;
    lpCapture->wgcStagingHeight = 0;

    desc.Width = max(width, lpCapture->wgcFrameWidth);
    desc.Height = max(height, lpCapture->wgcFrameHeight);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    if (FAILED(ID3D11Device_CreateTexture2D(lpCapture->d3dDevice, &desc, NULL, &lpCapture->wgcStagingTexture)))
    {
      return FALSE;
    }

    lpCapture->wgcStagingWidth = desc.Width;
    lpCapture->wgcStagingHeight = desc.Height;
    return TRUE;
}

BOOL render_wgcUpdateFrame(LPWGCMONITORCAPTURE lpCapture)
{
    WGCFRAME* frame = NULL;
    WGCFRAME* latestFrame = NULL;
    WGCSURFACE* surface = NULL;
    IDirect3DDxgiInterfaceAccess* access = NULL;
    ID3D11Texture2D* frameTexture = NULL;
    WGCSIZE contentSize = { 0 };
    HRESULT hr;

    while (SUCCEEDED(__x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFramePool_TryGetNextFrame(
      (WGCFRAMEPOOL*)lpCapture->wgcFramePool,
      &frame)) && frame)
    {
      SAFERELEASE(latestFrame);
      latestFrame = frame;
      frame = NULL;
    }

    if (!latestFrame)
    {
      return lpCapture->fHasFrame;
    }

    hr = __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFrame_get_ContentSize(latestFrame, &contentSize);
    if (FAILED(hr) ||
        contentSize.Width < 1 ||
        contentSize.Height < 1 ||
        (UINT)contentSize.Width != lpCapture->wgcFrameWidth ||
        (UINT)contentSize.Height != lpCapture->wgcFrameHeight)
    {
      SAFERELEASE(latestFrame);
      render_wgcDeleteMonitorResources(lpCapture);
      return FALSE;
    }

    hr = __x_ABI_CWindows_CGraphics_CCapture_CIDirect3D11CaptureFrame_get_Surface(latestFrame, &surface);
    if (SUCCEEDED(hr))
    {
      hr = IUnknown_QueryInterface((IUnknown*)surface, &IID_IDirect3DDxgiInterfaceAccess, (void**)&access);
    }

    if (SUCCEEDED(hr))
    {
      hr = IDirect3DDxgiInterfaceAccess_GetInterface(access, &IID_ID3D11Texture2D, (void**)&frameTexture);
    }

    if (SUCCEEDED(hr))
    {
      ID3D11DeviceContext_CopyResource(
        lpCapture->d3dContext,
        (ID3D11Resource*)lpCapture->wgcFrameTexture,
        (ID3D11Resource*)frameTexture);
      lpCapture->fHasFrame = TRUE;
    }

    SAFERELEASE(frameTexture);
    SAFERELEASE(access);
    SAFERELEASE(surface);
    SAFERELEASE(latestFrame);

    return lpCapture->fHasFrame;
}

BOOL render_wgcCaptureIntersection(LPMAGSTATE lpsd, LPWGCMONITORCAPTURE lpCapture, const RECT* lprcSource, const RECT* lprcIntersection)
{
    RECT rcDst;
    D3D11_BOX box;
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;

    if (!render_wgcUpdateFrame(lpCapture))
    {
      return FALSE;
    }

    box.left = (UINT)MulDiv(lprcIntersection->left - lpCapture->rcOutput.left, lpCapture->wgcFrameWidth, RECTWIDTH(lpCapture->rcOutput));
    box.top = (UINT)MulDiv(lprcIntersection->top - lpCapture->rcOutput.top, lpCapture->wgcFrameHeight, RECTHEIGHT(lpCapture->rcOutput));
    box.front = 0;
    box.right = (UINT)MulDiv(lprcIntersection->right - lpCapture->rcOutput.left, lpCapture->wgcFrameWidth, RECTWIDTH(lpCapture->rcOutput));
    box.bottom = (UINT)MulDiv(lprcIntersection->bottom - lpCapture->rcOutput.top, lpCapture->wgcFrameHeight, RECTHEIGHT(lpCapture->rcOutput));
    box.back = 1;

    if (box.right <= box.left || box.bottom <= box.top)
    {
      return FALSE;
    }

    if (!render_wgcEnsureStagingTexture(lpCapture, box.right - box.left, box.bottom - box.top))
    {
      return FALSE;
    }

    ID3D11DeviceContext_CopySubresourceRegion(
      lpCapture->d3dContext,
      (ID3D11Resource*)lpCapture->wgcStagingTexture,
      0,
      0,
      0,
      0,
      (ID3D11Resource*)lpCapture->wgcFrameTexture,
      0,
      &box);

    hr = ID3D11DeviceContext_Map(lpCapture->d3dContext, (ID3D11Resource*)lpCapture->wgcStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
      return FALSE;
    }

    if (!render_mapSourceRectToDestination(lpsd, lprcSource, lprcIntersection, &rcDst))
    {
      ID3D11DeviceContext_Unmap(lpCapture->d3dContext, (ID3D11Resource*)lpCapture->wgcStagingTexture, 0);
      return TRUE;
    }

    render_dxgiCopyMappedPixelsToRect(
      lpsd,
      (const BYTE*)mapped.pData,
      box.right - box.left,
      box.bottom - box.top,
      mapped.RowPitch,
      &rcDst);

    ID3D11DeviceContext_Unmap(lpCapture->d3dContext, (ID3D11Resource*)lpCapture->wgcStagingTexture, 0);
    return TRUE;
}

void render_computeSourceRects(HWND hWnd, RECT* lprcSource, RECT* lprcClippedSource)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const LONG cw = lpsd->bi.biWidth;
    const LONG ch = lpsd->bi.biHeight;
    POINT tl = { 0, 0 };
    POINT center = { 0, 0 };
    RECT rcClip = lpsd->di.rc;
    const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
    BOOL fCenterOnCursor;
    BOOL fUseSourceOrigin;
    LONG srcW;
    LONG srcH;
    LONG srcX;
    LONG srcY;

    if (!ClientToScreen(hWnd, &tl))
    {
      SetRectEmpty(lprcSource);
      SetRectEmpty(lprcClippedSource);
      return;
    }

    fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
    if (!fCenterOnCursor)
    {
      center.x = tl.x + cw / 2;
      center.y = tl.y + ch / 2;
    }

    if (fCenterOnCursor)
    {
      const HMONITOR hMonitor = MonitorFromPoint(center, MONITOR_DEFAULTTONEAREST);
      MONITORINFO mi = { sizeof(mi) };

      if (hMonitor && GetMonitorInfo(hMonitor, &mi))
      {
        rcClip = mi.rcMonitor;
      }
    }

    if (IsRectEmpty(&rcClip))
    {
      rcClip.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      rcClip.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
      rcClip.right = rcClip.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
      rcClip.bottom = rcClip.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }

    fUseSourceOrigin = !fCenterOnCursor && lpsd->fUseSourceOrigin;

    if (m <= 1.0001f)
    {
      srcW = cw;
      srcH = ch;
      if (fUseSourceOrigin)
      {
        srcX = lpsd->ptSourceOrigin.x;
        srcY = lpsd->ptSourceOrigin.y;
      }
      else
      {
        srcX = fCenterOnCursor ? center.x - srcW / 2 : tl.x;
        srcY = fCenterOnCursor ? center.y - srcH / 2 : tl.y;
      }
      if (fCenterOnCursor)
      {
        lpsd->pt = center;
      }
      else
      {
        lpsd->pt.x = srcX;
        lpsd->pt.y = srcY;
      }
      if (!fCenterOnCursor && !fUseSourceOrigin)
      {
        lpsd->fUseSourceOrigin = FALSE;
        lpsd->fSourceOriginPinned = FALSE;
      }
    }
    else
    {
      srcW = (LONG)(cw / m);
      srcH = (LONG)(ch / m);

      if (srcW < 1) srcW = 1;
      if (srcH < 1) srcH = 1;

      if (fUseSourceOrigin)
      {
        srcX = lpsd->ptSourceOrigin.x;
        srcY = lpsd->ptSourceOrigin.y;
      }
      else
      {
        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
      }

      lpsd->pt = center;
    }

    if (fCenterOnCursor)
    {
      srcX = render_clipSourceOrigin(srcX, srcW, rcClip.left, rcClip.right);
      srcY = render_clipSourceOrigin(srcY, srcH, rcClip.top, rcClip.bottom);
    }

    SetRect(lprcSource, srcX, srcY, srcX + srcW, srcY + srcH);
    if (!IntersectRect(lprcClippedSource, lprcSource, &rcClip))
    {
      SetRectEmpty(lprcClippedSource);
    }
}

void render_computeSourceRect(HWND hWnd, RECT* lprcSource)
{
    RECT rcClippedSource;

    render_computeSourceRects(hWnd, lprcSource, &rcClippedSource);
}

BOOL render_captureUsesCompositor(CAPTUREAPI captureApi)
{
    return CAPTURE_API_DWM_THUMBNAIL == captureApi ||
      CAPTURE_API_DWM_PRIVATE_VISUAL == captureApi;
}

BOOL render_setGraphicsPresentationEnabled(HWND hWnd, BOOL enabled)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BOOL result = FALSE;

    if (!lpsd)
    {
      return FALSE;
    }

    AcquireSRWLockExclusive(&lpsd->graphicsLock);
    if (lpsd->graphicsBackend && lpsd->graphicsState &&
        lpsd->graphicsBackend->SetPresentationEnabled)
    {
      result = lpsd->graphicsBackend->SetPresentationEnabled(
        hWnd,
        lpsd->graphicsState,
        enabled);
      if (result)
      {
        lpsd->fGraphicsPresentationEnabled = enabled;
      }
    }
    else if (!enabled)
    {
      /* A compositor-owned capture route may intentionally defer creation of
         the pixel presenter.  An absent presenter is already detached. */
      lpsd->fGraphicsPresentationEnabled = FALSE;
      result = TRUE;
    }
    ReleaseSRWLockExclusive(&lpsd->graphicsLock);
    return result;
}

BOOL render_presentDwmFallback(HWND hWnd)
{
    render_setGraphicsPresentationEnabled(hWnd, TRUE);
    render_gdiCaptureScreen(hWnd);
    return render_presentPixelFrame(hWnd);
}

void render_dwmThumbnailDeleteResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->dwmThumbnail.hThumbnail)
    {
      DwmUnregisterThumbnail(lpsd->dwmThumbnail.hThumbnail);
      lpsd->dwmThumbnail.hThumbnail = NULL;
    }

    lpsd->dwmThumbnail.hwndSource = NULL;
}

BOOL render_dwmThumbnailEnsureResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    HWND hwndSource = GetDesktopWindow();

    if (lpsd->dwmThumbnail.hThumbnail && lpsd->dwmThumbnail.hwndSource == hwndSource)
    {
      return TRUE;
    }

    render_dwmThumbnailDeleteResources(hWnd);

    if (!hwndSource ||
        FAILED(DwmRegisterThumbnail(hWnd, hwndSource, &lpsd->dwmThumbnail.hThumbnail)))
    {
      lpsd->dwmThumbnail.hThumbnail = NULL;
      return FALSE;
    }

    lpsd->dwmThumbnail.hwndSource = hwndSource;
    return TRUE;
}

BOOL render_dwmThumbnailCaptureScreen(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    DWM_THUMBNAIL_PROPERTIES props = { 0 };
    RECT rcSource;
    RECT rcClippedSource;
    RECT rcVirtual = lpsd->di.rc;

    if (!render_setGraphicsPresentationEnabled(hWnd, FALSE) ||
        !render_dwmThumbnailEnsureResources(hWnd))
    {
      return render_presentDwmFallback(hWnd);
    }

    render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
    if (IsRectEmpty(&rcClippedSource) || render_sourceRectIsClipped(&rcSource, &rcClippedSource))
    {
      render_dwmThumbnailDeleteResources(hWnd);
      return render_presentDwmFallback(hWnd);
    }

    if (IsRectEmpty(&rcVirtual))
    {
      rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    }

    OffsetRect(&rcClippedSource, -rcVirtual.left, -rcVirtual.top);

    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
    SetRect(&props.rcDestination, 0, 0, lpsd->bi.biWidth, lpsd->bi.biHeight);
    props.rcSource = rcClippedSource;
    props.opacity = 255;
    props.fVisible = TRUE;
    props.fSourceClientAreaOnly = FALSE;

    if (FAILED(DwmUpdateThumbnailProperties(lpsd->dwmThumbnail.hThumbnail, &props)))
    {
      render_dwmThumbnailDeleteResources(hWnd);
      return render_presentDwmFallback(hWnd);
    }

    return SUCCEEDED(DwmFlush());
}

void render_dwmPrivateDeleteResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    DwmPrivateCaptureDestroy(lpsd->dwmPrivate.state);
    lpsd->dwmPrivate.state = NULL;
}

BOOL render_dwmPrivateEnsureResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    return lpsd->dwmPrivate.state ||
      DwmPrivateCaptureCreate(hWnd, &lpsd->dwmPrivate.state);
}

UINT render_dwmPrivateTranslateDrawCommands(
  const MAGUIDRAWLIST* ui,
  DWMPRIVATEDRAWCOMMAND* lpCommands,
  UINT commandCapacity)
{
    UINT i;
    UINT count = 0;

    for (i = 0; ui && i < ui->count && count < commandCapacity; ++i)
    {
      const MAGUIDRAWCOMMAND* source = &ui->commands[i];
      DWMPRIVATEDRAWCOMMAND* destination;

      if (MAG_UI_DRAW_TEXT == source->type)
      {
        continue;
      }

      destination = &lpCommands[count++];

      destination->type = MAG_UI_DRAW_STROKE_RECT == source->type
        ? DWM_PRIVATE_DRAW_STROKE
        : DWM_PRIVATE_DRAW_FILL;
      destination->rc = source->rect;
      destination->color[0] = source->color.r;
      destination->color[1] = source->color.g;
      destination->color[2] = source->color.b;
      destination->color[3] = source->color.a;
      destination->thickness = max(1U, (UINT)(source->thickness + 0.5f));
    }

    return count;
}

BOOL render_dwmPrivateCaptureScreen(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    DWMPRIVATEDRAWCOMMAND drawCommands[DWM_PRIVATE_MAX_DRAW_COMMANDS];
    RECT rcSource;
    RECT rcClippedSource;
    RECT rcDestination;
    RECT rcDesktop;
    const RECT* lprcPrivateSource = NULL;
    const RECT* lprcPrivateDestination = NULL;
    const SIZE targetSize = { lpsd->bi.biWidth, lpsd->bi.biHeight };
    UINT drawCommandCount;

    if (!render_setGraphicsPresentationEnabled(hWnd, FALSE))
    {
      return render_presentDwmFallback(hWnd);
    }

    render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
    if (!IsRectEmpty(&rcClippedSource) &&
        render_mapSourceRectToDestination(lpsd, &rcSource, &rcClippedSource, &rcDestination))
    {
      lprcPrivateSource = &rcClippedSource;
      lprcPrivateDestination = &rcDestination;
    }

    render_buildUiDrawList(hWnd, &lpsd->uiDrawList);
    drawCommandCount = render_dwmPrivateTranslateDrawCommands(
      &lpsd->uiDrawList,
      drawCommands,
      ARRAYSIZE(drawCommands));
    if (!render_minimapGetCaptureRect(lpsd, &rcDesktop) ||
        !render_dwmPrivateEnsureResources(hWnd) ||
        !DwmPrivateCaptureUpdate(
          lpsd->dwmPrivate.state,
          &rcDesktop,
          lprcPrivateSource,
          lprcPrivateDestination,
          targetSize,
          drawCommands,
          drawCommandCount))
    {
      render_dwmPrivateDeleteResources(hWnd);
      return render_presentDwmFallback(hWnd);
    }

    return TRUE;
}

void render_gdiCreateResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    gdiGetDisplayInfo(&lpsd->di);
    lpsd->hDesktopDC = GetDC(NULL); // whole virtual screen (handles negative/secondary monitor coords)
    lpsd->hCaptureDC = CreateCompatibleDC(lpsd->hDesktopDC);
    render_updateSurfaceInfo(hWnd);
    render_gdiCreateCaptureBitmap(hWnd);
}

void render_gdiDeleteResources(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    render_gdiDeleteCaptureBitmap(hWnd);
    if (lpsd->hCaptureDC)
    {
      DeleteDC(lpsd->hCaptureDC);
      lpsd->hCaptureDC = NULL;
    }
    if (lpsd->hDesktopDC)
    {
      ReleaseDC(NULL, lpsd->hDesktopDC);
      lpsd->hDesktopDC = NULL;
    }
}

void render_gdiResizeSurface(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    render_updateSurfaceInfo(hWnd);

    if (!lpsd->hCaptureDC)
    {
      return;
    }

    if (lpsd->hBitmapBg &&
        (UINT)lpsd->bi.biWidth <= lpsd->frameCapacityWidth &&
        (UINT)lpsd->bi.biHeight <= lpsd->frameCapacityHeight)
    {
      return;
    }

    render_gdiDeleteCaptureBitmap(hWnd);
    render_gdiCreateCaptureBitmap(hWnd);
}

void render_gdiCaptureScreen(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hBitmapBg && lpsd->frame.pixels)
    {
      const LONG cw = lpsd->bi.biWidth;
      const LONG ch = lpsd->bi.biHeight;
      RECT rcSource;
      RECT rcClippedSource;
      RECT rcDst;

      render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
      PatBlt(lpsd->hCaptureDC, 0, 0, cw, ch, BLACKNESS);

      if (!IsRectEmpty(&rcClippedSource) &&
          render_mapSourceRectToDestination(lpsd, &rcSource, &rcClippedSource, &rcDst))
      {
        if (RECTWIDTH(rcDst) == RECTWIDTH(rcClippedSource) &&
            RECTHEIGHT(rcDst) == RECTHEIGHT(rcClippedSource))
        {
          BitBlt(
            lpsd->hCaptureDC,
            rcDst.left,
            rcDst.top,
            RECTWIDTH(rcDst),
            RECTHEIGHT(rcDst),
            lpsd->hDesktopDC,
            rcClippedSource.left,
            rcClippedSource.top,
            SRCCOPY | CAPTUREBLT);
        }
        else
        {
          SetStretchBltMode(lpsd->hCaptureDC, COLORONCOLOR);
          StretchBlt(
            lpsd->hCaptureDC,
            rcDst.left,
            rcDst.top,
            RECTWIDTH(rcDst),
            RECTHEIGHT(rcDst),
            lpsd->hDesktopDC,
            rcClippedSource.left,
            rcClippedSource.top,
            RECTWIDTH(rcClippedSource),
            RECTHEIGHT(rcClippedSource),
            SRCCOPY | CAPTUREBLT);
        }
      }

      GdiFlush();
    }
}

void render_dxgiCaptureScreen(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->frame.pixels)
    {
      RECT rcSource;
      RECT rcClippedSource;
      UINT i;
      BOOL fCapturedAny = FALSE;

      render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
      ZeroMemory(lpsd->frame.pixels, lpsd->bi.biSizeImage);
      if (IsRectEmpty(&rcClippedSource))
      {
        return;
      }

      for (i = 0; i < lpsd->di.numMonitors; ++i)
      {
        RECT rcMonitor = lpsd->di.monitors[i].monitorInfoEx.rcMonitor;
        RECT rcIntersection;
        POINT ptIntersection;
        HMONITOR hMonitor;
        LPDXGIOUTPUTCAPTURE lpOutput = NULL;

        if (!IntersectRect(&rcIntersection, &rcClippedSource, &rcMonitor))
        {
          continue;
        }

        ptIntersection.x = rcIntersection.left + RECTWIDTH(rcIntersection) / 2;
        ptIntersection.y = rcIntersection.top + RECTHEIGHT(rcIntersection) / 2;
        hMonitor = MonitorFromPoint(ptIntersection, MONITOR_DEFAULTTONULL);

        if (!hMonitor ||
            !render_dxgiEnsureDuplication(hWnd, hMonitor, &lpOutput) ||
            !render_dxgiCaptureIntersection(lpsd, lpOutput, &rcSource, &rcIntersection))
        {
          render_gdiCaptureScreen(hWnd);
          return;
        }

        fCapturedAny = TRUE;
      }

      if (!fCapturedAny)
      {
        render_gdiCaptureScreen(hWnd);
      }
    }
}

void render_wgcCaptureScreen(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->frame.pixels)
    {
      RECT rcSource;
      RECT rcClippedSource;
      UINT i;
      BOOL fCapturedAny = FALSE;

      render_computeSourceRects(hWnd, &rcSource, &rcClippedSource);
      ZeroMemory(lpsd->frame.pixels, lpsd->bi.biSizeImage);
      if (IsRectEmpty(&rcClippedSource))
      {
        return;
      }

      for (i = 0; i < lpsd->di.numMonitors; ++i)
      {
        RECT rcMonitor = lpsd->di.monitors[i].monitorInfoEx.rcMonitor;
        RECT rcIntersection;
        POINT ptIntersection;
        HMONITOR hMonitor;
        LPWGCMONITORCAPTURE lpCapture = NULL;

        if (!IntersectRect(&rcIntersection, &rcClippedSource, &rcMonitor))
        {
          continue;
        }

        ptIntersection.x = rcIntersection.left + RECTWIDTH(rcIntersection) / 2;
        ptIntersection.y = rcIntersection.top + RECTHEIGHT(rcIntersection) / 2;
        hMonitor = MonitorFromPoint(ptIntersection, MONITOR_DEFAULTTONULL);

        if (!hMonitor ||
            !render_wgcEnsureCapture(hWnd, hMonitor, &lpCapture) ||
            !render_wgcCaptureIntersection(lpsd, lpCapture, &rcSource, &rcIntersection))
        {
          render_gdiCaptureScreen(hWnd);
          return;
        }

        fCapturedAny = TRUE;
      }

      if (!fCapturedAny)
      {
        render_gdiCaptureScreen(hWnd);
      }
    }
}

BOOL renderApplyPresentationSettings(
  HWND hWnd,
  GRAPHICSAPI api,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  LPTSTR reason,
  UINT reasonCount)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const MAGGRAPHICSBACKEND* backend = magGraphicsGetBackend(api);
    void* candidateGraphicsState = NULL;
    MAGUIRENDERER* candidateUiRenderer = NULL;
    BOOL graphicsChanged;
    BOOL uiChanged;
    BOOL candidateGraphicsIsParked = FALSE;
    BOOL usedCompatibilityBridge = FALSE;
    BOOL candidatePresentationEnabled;
    BOOL requiresPixelResources;
    const MAGGRAPHICSBACKEND* suspendedGraphicsBackend = NULL;
    void* suspendedGraphicsState = NULL;
    BOOL suspendedPresentationEnabled = TRUE;
    SIZE clientSize;
    const MAGGRAPHICSBACKEND* oldGraphicsBackend = NULL;
    void* oldGraphicsState = NULL;
    GRAPHICSAPI bridgePreviousApi = GRAPHICS_API_OPENGL;
    UIGRAPHICSAPI bridgePreviousUiApi = UI_GRAPHICS_API_NATIVE;
    TEXTRENDERER bridgePreviousTextRenderer = TEXT_RENDERER_DIRECTWRITE;

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }

    if (!lpsd || !backend || !backend->implemented ||
        uiApi >= UI_GRAPHICS_API_COUNT || textRenderer >= TEXT_RENDERER_COUNT)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The selected graphics, UI, or text renderer is invalid."), reasonCount);
      }
      return FALSE;
    }

    requiresPixelResources = !render_captureUsesCompositor(lpsd->captureApi);

    if (requiresPixelResources && lpsd->graphicsBackend && lpsd->graphicsApi != api &&
        GRAPHICS_API_GDI != lpsd->graphicsApi && GRAPHICS_API_GDI != api &&
        (GRAPHICS_API_VULKAN == lpsd->graphicsApi || GRAPHICS_API_VULKAN == api))
    {
      TCHAR bridgeReason[256];

      bridgePreviousApi = lpsd->graphicsApi;
      bridgePreviousUiApi = lpsd->uiGraphicsApi;
      bridgePreviousTextRenderer = lpsd->textRenderer;
      if (!renderApplyPresentationSettings(
            hWnd,
            GRAPHICS_API_GDI,
            bridgePreviousUiApi,
            bridgePreviousTextRenderer,
            bridgeReason,
            ARRAYSIZE(bridgeReason)))
      {
        if (reason && reasonCount)
        {
          lstrcpyn(
            reason,
            bridgeReason[0]
              ? bridgeReason
              : TEXT("The compatibility presenter could not prepare the Vulkan transition."),
            reasonCount);
        }
        return FALSE;
      }
      usedCompatibilityBridge = TRUE;
    }

    graphicsChanged = lpsd->graphicsBackend != backend ||
      (requiresPixelResources && !lpsd->graphicsState) ||
      (!requiresPixelResources && lpsd->graphicsState) ||
      lpsd->fForceGraphicsRecreate;
    uiChanged = (requiresPixelResources && !lpsd->uiRenderer) ||
      (!requiresPixelResources && lpsd->uiRenderer) ||
      lpsd->uiGraphicsApi != uiApi ||
      lpsd->textRenderer != textRenderer;
    if (!graphicsChanged && !uiChanged)
    {
      return TRUE;
    }

    if (graphicsChanged && requiresPixelResources &&
        !backend->IsAvailable(reason, reasonCount))
    {
      if (usedCompatibilityBridge)
      {
        TCHAR rollbackReason[256];
        renderApplyPresentationSettings(
          hWnd,
          bridgePreviousApi,
          bridgePreviousUiApi,
          bridgePreviousTextRenderer,
          rollbackReason,
          ARRAYSIZE(rollbackReason));
      }
      return FALSE;
    }

    if (graphicsChanged && lpsd->graphicsBackend && lpsd->graphicsState)
    {
      if (!lpsd->graphicsBackend->SetPresentationEnabled(
            hWnd,
            lpsd->graphicsState,
            FALSE))
      {
        if (reason && reasonCount)
        {
          lstrcpyn(reason, TEXT("The current graphics API could not release its presentation target for the switch."), reasonCount);
        }
        return FALSE;
      }
      suspendedGraphicsBackend = lpsd->graphicsBackend;
      suspendedGraphicsState = lpsd->graphicsState;
      suspendedPresentationEnabled = lpsd->fGraphicsPresentationEnabled;
    }

    clientSize.cx = max(1, lpsd->bi.biWidth);
    clientSize.cy = max(1, lpsd->bi.biHeight);
    candidatePresentationEnabled = requiresPixelResources &&
      MAG_HOST_TRADITIONAL_LAYERED != lpsd->resolvedPresentation.host;
    if (graphicsChanged && requiresPixelResources && !lpsd->fForceGraphicsRecreate &&
        GRAPHICS_API_VULKAN == api && lpsd->parkedVulkanState)
    {
      candidateGraphicsState = lpsd->parkedVulkanState;
      if (backend->Resize(hWnd, candidateGraphicsState, clientSize))
      {
        candidateGraphicsIsParked = TRUE;
      }
      else
      {
        backend->Destroy(hWnd, candidateGraphicsState);
        lpsd->parkedVulkanState = NULL;
        candidateGraphicsState = NULL;
      }
    }
    if (graphicsChanged && requiresPixelResources && !lpsd->fForceGraphicsRecreate &&
        GRAPHICS_API_OPENGL == api && lpsd->parkedOpenGlState)
    {
      candidateGraphicsState = lpsd->parkedOpenGlState;
      if (backend->Resize(hWnd, candidateGraphicsState, clientSize))
      {
        candidateGraphicsIsParked = TRUE;
      }
      else
      {
        backend->Destroy(hWnd, candidateGraphicsState);
        lpsd->parkedOpenGlState = NULL;
        candidateGraphicsState = NULL;
      }
    }

    if (graphicsChanged && requiresPixelResources && !candidateGraphicsState &&
        (!backend->Create(
            hWnd,
            clientSize,
            &lpsd->resolvedPresentation,
            &candidateGraphicsState) || !candidateGraphicsState))
    {
      if (reason && reasonCount && !reason[0])
      {
        _sntprintf_s(
          reason,
          reasonCount,
          _TRUNCATE,
          TEXT("The selected graphics API could not create its presentation device (error 0x%08lX)."),
          GetLastError());
      }
      if (suspendedGraphicsBackend && suspendedGraphicsState)
      {
        suspendedGraphicsBackend->SetPresentationEnabled(
          hWnd,
          suspendedGraphicsState,
          suspendedPresentationEnabled);
      }
      if (usedCompatibilityBridge)
      {
        TCHAR rollbackReason[256];
        renderApplyPresentationSettings(
          hWnd,
          bridgePreviousApi,
          bridgePreviousUiApi,
          bridgePreviousTextRenderer,
          rollbackReason,
          ARRAYSIZE(rollbackReason));
      }
      return FALSE;
    }
    if (graphicsChanged && candidateGraphicsState &&
        !backend->SetPresentationEnabled(
          hWnd,
          candidateGraphicsState,
          candidatePresentationEnabled))
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The selected graphics API could not attach its presentation target."), reasonCount);
      }
      if (candidateGraphicsState && !candidateGraphicsIsParked)
      {
        backend->Destroy(hWnd, candidateGraphicsState);
      }
      if (suspendedGraphicsBackend && suspendedGraphicsState)
      {
        suspendedGraphicsBackend->SetPresentationEnabled(
          hWnd,
          suspendedGraphicsState,
          suspendedPresentationEnabled);
      }
      if (usedCompatibilityBridge)
      {
        TCHAR rollbackReason[256];
        renderApplyPresentationSettings(
          hWnd,
          bridgePreviousApi,
          bridgePreviousUiApi,
          bridgePreviousTextRenderer,
          rollbackReason,
          ARRAYSIZE(rollbackReason));
      }
      return FALSE;
    }
    if (uiChanged && requiresPixelResources && !magUiRendererCreate(
          uiApi,
          textRenderer,
          clientSize,
          &candidateUiRenderer,
          reason,
          reasonCount))
    {
      if (candidateGraphicsState && !candidateGraphicsIsParked)
      {
        backend->Destroy(hWnd, candidateGraphicsState);
      }
      if (suspendedGraphicsBackend && suspendedGraphicsState)
      {
        suspendedGraphicsBackend->SetPresentationEnabled(
          hWnd,
          suspendedGraphicsState,
          suspendedPresentationEnabled);
      }
      if (usedCompatibilityBridge)
      {
        TCHAR rollbackReason[256];
        renderApplyPresentationSettings(
          hWnd,
          bridgePreviousApi,
          bridgePreviousUiApi,
          bridgePreviousTextRenderer,
          rollbackReason,
          ARRAYSIZE(rollbackReason));
      }
      return FALSE;
    }

    if (graphicsChanged)
    {
      AcquireSRWLockExclusive(&lpsd->graphicsLock);
      oldGraphicsBackend = lpsd->graphicsBackend;
      oldGraphicsState = lpsd->graphicsState;
      if (candidateGraphicsIsParked)
      {
        if (GRAPHICS_API_OPENGL == api)
        {
          lpsd->parkedOpenGlState = NULL;
        }
        else if (GRAPHICS_API_VULKAN == api)
        {
          lpsd->parkedVulkanState = NULL;
        }
      }
      if (!lpsd->fForceGraphicsRecreate &&
          oldGraphicsBackend && GRAPHICS_API_OPENGL == oldGraphicsBackend->api)
      {
        lpsd->parkedOpenGlState = oldGraphicsState;
      }
      else if (!lpsd->fForceGraphicsRecreate &&
               oldGraphicsBackend && GRAPHICS_API_VULKAN == oldGraphicsBackend->api)
      {
        lpsd->parkedVulkanState = oldGraphicsState;
      }
      lpsd->graphicsApi = api;
      lpsd->graphicsBackend = backend;
      lpsd->graphicsState = candidateGraphicsState;
      lpsd->fGraphicsPresentationEnabled = candidatePresentationEnabled;
      ReleaseSRWLockExclusive(&lpsd->graphicsLock);

      if (oldGraphicsBackend && oldGraphicsState &&
          (lpsd->fForceGraphicsRecreate ||
           (GRAPHICS_API_OPENGL != oldGraphicsBackend->api &&
            GRAPHICS_API_VULKAN != oldGraphicsBackend->api)))
      {
        oldGraphicsBackend->Destroy(hWnd, oldGraphicsState);
        DwmFlush();
      }
    }
    if (uiChanged)
    {
      magUiRendererDestroy(lpsd->uiRenderer);
      lpsd->uiRenderer = candidateUiRenderer;
      lpsd->uiGraphicsApi = uiApi;
      lpsd->textRenderer = textRenderer;
    }
    lpsd->fPresentedContentValid = FALSE;
    return TRUE;
}

BOOL renderApplySettings(
  HWND hWnd,
  GRAPHICSAPI api,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  LPTSTR reason,
  UINT reasonCount)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    CAPTUREAPI oldCaptureApi;
    GRAPHICSAPI oldGraphicsApi;
    UIGRAPHICSAPI oldUiApi;
    TEXTRENDERER oldTextRenderer;
    BOOL detachCompositor;
    BOOL createdPixelResources = FALSE;

    if (!lpsd || captureApi >= CAPTURE_API_COUNT)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The selected capture API is invalid."), reasonCount);
      }
      return FALSE;
    }

    oldCaptureApi = lpsd->captureApi;
    oldGraphicsApi = lpsd->graphicsApi;
    oldUiApi = lpsd->uiGraphicsApi;
    oldTextRenderer = lpsd->textRenderer;
    if (!render_captureUsesCompositor(captureApi) && !lpsd->hCaptureDC)
    {
      render_gdiCreateResources(hWnd);
      if (!lpsd->hCaptureDC || !lpsd->frame.pixels)
      {
        if (reason && reasonCount)
        {
          lstrcpyn(reason, TEXT("The pixel capture reservoir could not be initialized."), reasonCount);
        }
        return FALSE;
      }
      createdPixelResources = TRUE;
    }
    detachCompositor = (lpsd->graphicsBackend != magGraphicsGetBackend(api) ||
                        !lpsd->graphicsState) &&
      (render_captureUsesCompositor(oldCaptureApi) ||
       render_captureUsesCompositor(captureApi));
    if (detachCompositor)
    {
      if (!render_transitionCaptureApi(hWnd, CAPTURE_API_GDI_BITBLT))
      {
        if (reason && reasonCount)
        {
          lstrcpyn(reason, TEXT("The current compositor target could not be released."), reasonCount);
        }
        return FALSE;
      }
    }

    lpsd->captureApi = captureApi;
    if (!renderApplyPresentationSettings(
          hWnd,
          api,
          uiApi,
          textRenderer,
          reason,
          reasonCount))
    {
      lpsd->captureApi = oldCaptureApi;
      render_transitionCaptureApi(hWnd, oldCaptureApi);
      if (createdPixelResources && render_captureUsesCompositor(oldCaptureApi))
      {
        render_gdiDeleteResources(hWnd);
        render_updateSurfaceInfo(hWnd);
      }
      return FALSE;
    }

    if (!render_transitionCaptureApi(hWnd, captureApi))
    {
      TCHAR rollbackReason[256];

      lpsd->captureApi = oldCaptureApi;
      renderApplyPresentationSettings(
        hWnd,
        oldGraphicsApi,
        oldUiApi,
        oldTextRenderer,
        rollbackReason,
        ARRAYSIZE(rollbackReason));
      render_transitionCaptureApi(hWnd, oldCaptureApi);
      if (createdPixelResources && render_captureUsesCompositor(oldCaptureApi))
      {
        render_gdiDeleteResources(hWnd);
        render_updateSurfaceInfo(hWnd);
      }
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The selected capture API could not acquire presentation ownership."), reasonCount);
      }
      return FALSE;
    }
    return TRUE;
}

static BOOL render_isModernFlipTarget(MAGPRESENTATIONTARGET target)
{
    return MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP == target ||
      MAG_PRESENT_COMPOSED_FLIP == target ||
      MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP == target;
}

static BOOL render_presentationResourcesEqual(
  GRAPHICSAPI api,
  const MAGPRESENTATIONSETTINGS* oldSettings,
  const MAGPRESENTATIONSETTINGS* newSettings)
{
    MAGPRESENTATIONSETTINGS left;
    MAGPRESENTATIONSETTINGS right;

    if (!oldSettings || !newSettings)
    {
      return FALSE;
    }
    left = *oldSettings;
    right = *newSettings;

    /* These fields constrain admission/observation, not device resources. */
    left.copyRequirement = right.copyRequirement;
    left.strictTarget = right.strictTarget;

    /* All modern HWND flip outcomes use the same flip-discard chain.  Windows
       chooses composed, DirectFlip, or MPO promotion at runtime; changing the
       observation target must not tear down that chain. */
    if ((GRAPHICS_API_D3D11 == api || GRAPHICS_API_D3D12 == api) &&
        MAG_HOST_REDIRECTED_HWND == left.host &&
        MAG_HOST_REDIRECTED_HWND == right.host &&
        render_isModernFlipTarget(left.target) &&
        render_isModernFlipTarget(right.target))
    {
      left.target = right.target;
    }
    return magPresentationSettingsEqual(&left, &right);
}

BOOL renderApplyFullSettings(
  HWND hWnd,
  GRAPHICSAPI api,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  const MAGPRESENTATIONSETTINGS* presentation,
  LPTSTR reason,
  UINT reasonCount)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    MAGADAPTERCATALOG catalog;
    MAGPRESENTATIONSETTINGS resolved;
    MAGPRESENTATIONSETTINGS oldResolved;
    MAGPRESENTATIONSTATUS status = { 0 };
    MAGLAYEREDPRESENTER* candidateLayeredPresenter = NULL;
    MAGLAYEREDPRESENTER* oldLayeredPresenter;
    UINT64 nextGeneration;
    BOOL oldForceGraphicsRecreate;

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!lpsd || !presentation)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The complete presentation settings are invalid."), reasonCount);
      }
      return FALSE;
    }
    if (!magAdapterCatalogEnumerate(&catalog, reason, reasonCount) ||
        !magPresentationResolve(
          hWnd,
          api,
          captureApi,
          uiApi,
          textRenderer,
          presentation,
          &catalog,
          &resolved,
          &status))
    {
      if (reason && reasonCount && !reason[0] && status.reason[0])
      {
        lstrcpyn(reason, status.reason, reasonCount);
      }
      return FALSE;
    }
    if (!status.configurationSupported || !status.flickerFree)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(
          reason,
          status.reason[0]
            ? status.reason
            : TEXT("The combination cannot guarantee flicker-free presentation."),
          reasonCount);
      }
      return FALSE;
    }

    if (MAG_HOST_TRADITIONAL_LAYERED == resolved.host)
    {
      RECT clientRect;
      SIZE clientSize;

      if (!GetClientRect(hWnd, &clientRect))
      {
        if (reason && reasonCount)
        {
          lstrcpyn(reason, TEXT("The layered presenter could not read the client geometry."), reasonCount);
        }
        return FALSE;
      }
      clientSize.cx = max(1L, RECTWIDTH(clientRect));
      clientSize.cy = max(1L, RECTHEIGHT(clientRect));
      if (!magLayeredPresenterCreate(hWnd, clientSize, &candidateLayeredPresenter))
      {
        if (reason && reasonCount)
        {
          lstrcpyn(reason, TEXT("The reservoir-backed layered presenter could not initialize."), reasonCount);
        }
        return FALSE;
      }
    }

    nextGeneration = lpsd->presentationStatus.configurationGeneration + 1;
    oldResolved = lpsd->resolvedPresentation;
    oldForceGraphicsRecreate = lpsd->fForceGraphicsRecreate;
    lpsd->resolvedPresentation = resolved;
    lpsd->fForceGraphicsRecreate = lpsd->graphicsState &&
      !render_presentationResourcesEqual(api, &oldResolved, &resolved);
    mag_UpdateViewWindowStyle(hWnd);
    if (!renderApplySettings(
          hWnd,
          api,
          captureApi,
          uiApi,
          textRenderer,
          reason,
          reasonCount))
    {
      magLayeredPresenterDestroy(candidateLayeredPresenter);
      lpsd->resolvedPresentation = oldResolved;
      lpsd->fForceGraphicsRecreate = oldForceGraphicsRecreate;
      mag_UpdateViewWindowStyle(hWnd);
      return FALSE;
    }
    lpsd->fForceGraphicsRecreate = oldForceGraphicsRecreate;

    oldLayeredPresenter = lpsd->layeredPresenter;
    lpsd->layeredPresenter = candidateLayeredPresenter;
    magLayeredPresenterDestroy(oldLayeredPresenter);

    status.configurationGeneration = nextGeneration;
    status.geometryEpoch = lpsd->geometryEpoch;
    lpsd->presentationSettings = *presentation;
    lpsd->resolvedPresentation = resolved;
    lpsd->presentationStatus = status;
    return TRUE;
}

BOOL renderSetGraphicsApi(HWND hWnd, GRAPHICSAPI api, LPTSTR reason, UINT reasonCount)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    return lpsd && renderApplySettings(
      hWnd,
      api,
      lpsd->captureApi,
      lpsd->uiGraphicsApi,
      lpsd->textRenderer,
      reason,
      reasonCount);
}

BOOL renderSetUiRendering(
  HWND hWnd,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  LPTSTR reason,
  UINT reasonCount)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    return lpsd && renderApplySettings(
      hWnd,
      lpsd->graphicsApi,
      lpsd->captureApi,
      uiApi,
      textRenderer,
      reason,
      reasonCount);
}

void renderInit(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    TCHAR reason[256];
    MAGPRESENTATIONSETTINGS fallbackPresentation;

    InitializeSRWLock(&lpsd->graphicsLock);
    if (render_captureUsesCompositor(lpsd->captureApi))
    {
      render_updateSurfaceInfo(hWnd);
    }
    else
    {
      render_gdiCreateResources(hWnd);
    }
    lpsd->activeCaptureApi = CAPTURE_API_COUNT;
    lpsd->fScale = 1.0f;
    lpsd->fTexScaler = 1.0f;

    if (!renderApplyFullSettings(
          hWnd,
          lpsd->graphicsApi,
          lpsd->captureApi,
          lpsd->uiGraphicsApi,
          lpsd->textRenderer,
          &lpsd->presentationSettings,
          reason,
          ARRAYSIZE(reason)))
    {
      magPresentationSettingsSetDefaults(&fallbackPresentation);
      renderApplyFullSettings(
        hWnd,
        GRAPHICS_API_GDI,
        CAPTURE_API_GDI_BITBLT,
        UI_GRAPHICS_API_NATIVE,
        TEXT_RENDERER_GDI,
        &fallbackPresentation,
        reason,
        ARRAYSIZE(reason));
    }
}

void renderCleanup(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const MAGGRAPHICSBACKEND* graphicsBackend;
    void* graphicsState;
    void* parkedOpenGlState;
    void* parkedVulkanState;
    void* openGlState;
    void* vulkanState;

    render_dwmPrivateDeleteResources(hWnd);
    render_dwmThumbnailDeleteResources(hWnd);
    render_wgcDeleteResources(hWnd);
    render_dxgiDeleteResources(hWnd);
    magLayeredPresenterDestroy(lpsd->layeredPresenter);
    lpsd->layeredPresenter = NULL;

    magUiRendererDestroy(lpsd->uiRenderer);
    lpsd->uiRenderer = NULL;

    AcquireSRWLockExclusive(&lpsd->graphicsLock);
    graphicsBackend = lpsd->graphicsBackend;
    graphicsState = lpsd->graphicsState;
    parkedOpenGlState = lpsd->parkedOpenGlState;
    parkedVulkanState = lpsd->parkedVulkanState;
    lpsd->graphicsState = NULL;
    lpsd->graphicsBackend = NULL;
    lpsd->parkedOpenGlState = NULL;
    lpsd->parkedVulkanState = NULL;
    ReleaseSRWLockExclusive(&lpsd->graphicsLock);

    openGlState = graphicsBackend && GRAPHICS_API_OPENGL == graphicsBackend->api
      ? graphicsState
      : parkedOpenGlState;
    vulkanState = graphicsBackend && GRAPHICS_API_VULKAN == graphicsBackend->api
      ? graphicsState
      : parkedVulkanState;
    if (graphicsBackend && graphicsState &&
        GRAPHICS_API_OPENGL != graphicsBackend->api &&
        GRAPHICS_API_VULKAN != graphicsBackend->api)
    {
      graphicsBackend->Destroy(hWnd, graphicsState);
    }
    if (vulkanState)
    {
      g_magGraphicsVulkanBackend.Destroy(hWnd, vulkanState);
    }
    if (openGlState)
    {
      g_magGraphicsOpenGLBackend.Destroy(hWnd, openGlState);
    }

    render_gdiDeleteResources(hWnd);

    if (lpsd->fWinRtInitialized)
    {
      RoUninitialize();
      lpsd->fWinRtInitialized = FALSE;
    }
}

BOOL renderResizeCapture(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    RECT rcClient;
    SIZE clientSize;
    BOOL graphicsResized = TRUE;
    BOOL uiResized = TRUE;

    if (!lpsd || !GetClientRect(hWnd, &rcClient))
    {
      return FALSE;
    }

    if (RECTWIDTH(rcClient) < 1 || RECTHEIGHT(rcClient) < 1)
    {
      lpsd->fPresentedContentValid = FALSE;
      return TRUE;
    }

    if (render_captureUsesCompositor(lpsd->captureApi))
    {
      render_updateSurfaceInfo(hWnd);
      clientSize.cx = max(1, lpsd->bi.biWidth);
      clientSize.cy = max(1, lpsd->bi.biHeight);
      /* Keep a parked pixel presenter geometry-compatible without allocating
         or presenting it.  If the compositor route is lost, its already
         resident reservoir can publish the same committed epoch instead of a
         stale old-size frame. */
      AcquireSRWLockExclusive(&lpsd->graphicsLock);
      if (lpsd->graphicsBackend && lpsd->graphicsState)
      {
        graphicsResized = lpsd->graphicsBackend->Resize(
          hWnd,
          lpsd->graphicsState,
          clientSize);
      }
      ReleaseSRWLockExclusive(&lpsd->graphicsLock);
      if (lpsd->uiRenderer)
      {
        uiResized = magUiRendererResize(lpsd->uiRenderer, clientSize);
      }
      lpsd->fDeferredResize = FALSE;
      lpsd->committedGeometryEpoch = lpsd->geometryEpoch;
      lpsd->fPresentedContentValid = FALSE;
      return graphicsResized && uiResized;
    }

    render_gdiResizeSurface(hWnd);
    if (!lpsd->frame.pixels)
    {
      lpsd->fPresentedContentValid = FALSE;
      return FALSE;
    }
    clientSize.cx = max(1, lpsd->bi.biWidth);
    clientSize.cy = max(1, lpsd->bi.biHeight);

    AcquireSRWLockExclusive(&lpsd->graphicsLock);
    if (lpsd->graphicsBackend && lpsd->graphicsState)
    {
      graphicsResized = lpsd->graphicsBackend->Resize(hWnd, lpsd->graphicsState, clientSize);
    }
    ReleaseSRWLockExclusive(&lpsd->graphicsLock);

    if (!graphicsResized)
    {
      graphicsResized = render_recreateGraphicsBackend(hWnd);
    }
    if (lpsd->uiRenderer)
    {
      uiResized = magUiRendererResize(lpsd->uiRenderer, clientSize);
      if (!uiResized)
      {
        MAGUIRENDERER* replacement = NULL;
        TCHAR reason[256];

        if (magUiRendererCreate(
              lpsd->uiGraphicsApi,
              lpsd->textRenderer,
              clientSize,
              &replacement,
              reason,
              ARRAYSIZE(reason)))
        {
          magUiRendererDestroy(lpsd->uiRenderer);
          lpsd->uiRenderer = replacement;
          uiResized = TRUE;
        }
      }
    }
    lpsd->fPresentedContentValid = FALSE;
    if (graphicsResized && uiResized)
    {
      lpsd->fDeferredResize = FALSE;
      lpsd->committedGeometryEpoch = lpsd->geometryEpoch;
    }
    return graphicsResized && uiResized;
}

static BOOL render_contentSizeMatches(const LPMAGSTATE lpsd, SIZE size)
{
    return lpsd &&
      lpsd->fPresentedContentValid &&
      lpsd->presentedContentSize.cx == size.cx &&
      lpsd->presentedContentSize.cy == size.cy;
}

static BOOL render_contentTargetsCurrentFrame(
  const LPMAGSTATE lpsd,
  SIZE size)
{
    LONGLONG nextFrame;

    if (!render_contentSizeMatches(lpsd, size))
    {
      return FALSE;
    }
    if (!lpsd->graphicsBackend ||
        !lpsd->graphicsBackend->GetNextEstimatedFrameTime)
    {
      return TRUE;
    }
    return lpsd->graphicsBackend->GetNextEstimatedFrameTime(
      lpsd->graphicsState,
      &nextFrame) && lpsd->presentedTargetFrame == nextFrame;
}

BOOL renderPrepareWindowResize(HWND hWnd, SIZE proposedClientSize)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    RECT rcClient;
    SIZE currentClientSize;
    BOOL fPresented = TRUE;

    if (!lpsd || proposedClientSize.cx < 1 || proposedClientSize.cy < 1 ||
        !GetClientRect(hWnd, &rcClient))
    {
      return FALSE;
    }

    currentClientSize.cx = RECTWIDTH(rcClient);
    currentClientSize.cy = RECTHEIGHT(rcClient);
    if (currentClientSize.cx < 1 || currentClientSize.cy < 1 ||
        (currentClientSize.cx == proposedClientSize.cx &&
         currentClientSize.cy == proposedClientSize.cy) ||
        lpsd->fInResizePresent)
    {
      return TRUE;
    }

    if (!render_contentTargetsCurrentFrame(lpsd, currentClientSize))
    {
      lpsd->fInResizePresent = TRUE;
      fPresented = renderSubmitGeometryFrame(hWnd, TRUE, FALSE);
      lpsd->fInResizePresent = FALSE;
    }

    if (fPresented && render_contentSizeMatches(lpsd, currentClientSize) &&
        SUCCEEDED(DwmFlush()))
    {
      ++lpsd->geometryEpoch;
      lpsd->fGeometryTransition = TRUE;
      lpsd->deferredClientSize = proposedClientSize;
      lpsd->fDeferredResize = TRUE;
      ++lpsd->resizePrecommitCount;
      return TRUE;
    }

    lpsd->fResizeContractViolation = TRUE;
    return FALSE;
}

BOOL renderPresentCommittedGeometry(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    RECT rcClient;
    SIZE clientSize;
    BOOL fPresented;

    if (!lpsd || !GetClientRect(hWnd, &rcClient))
    {
      return FALSE;
    }

    clientSize.cx = RECTWIDTH(rcClient);
    clientSize.cy = RECTHEIGHT(rcClient);
    if (clientSize.cx < 1 || clientSize.cy < 1 ||
        render_contentSizeMatches(lpsd, clientSize) ||
        lpsd->fInResizePresent)
    {
      return TRUE;
    }

    lpsd->fInResizePresent = TRUE;
    fPresented = renderSubmitGeometryFrame(hWnd, TRUE, TRUE);
    lpsd->fInResizePresent = FALSE;
    if (fPresented && render_contentSizeMatches(lpsd, clientSize))
    {
      lpsd->presentedGeometryEpoch = lpsd->committedGeometryEpoch;
      ++lpsd->resizeCommitCount;
      return TRUE;
    }

    lpsd->fResizeContractViolation = TRUE;
    return FALSE;
}

void renderSetMessageDriven(HWND hWnd, BOOL fMessageDriven)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd)
    {
      lpsd->fRenderMessageDriven = fMessageDriven;
    }
}

void renderRender(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd)
    {
      return;
    }

    if (!lpsd->fRenderMessageDriven)
    {
      renderSubmit(hWnd);
    }
}

BOOL render_stampPresentedContent(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    RECT rcClient;
    UINT observedTarget;

    if (!lpsd || !GetClientRect(hWnd, &rcClient) ||
        RECTWIDTH(rcClient) < 1 || RECTHEIGHT(rcClient) < 1)
    {
      return FALSE;
    }

    lpsd->presentedContentSize.cx = RECTWIDTH(rcClient);
    lpsd->presentedContentSize.cy = RECTHEIGHT(rcClient);
    lpsd->presentedGeometryEpoch = lpsd->committedGeometryEpoch;
    lpsd->presentedStateTransitionEpoch = lpsd->stateTransitionEpoch;
    lpsd->presentedTargetFrame = 0;
    if (lpsd->graphicsBackend && lpsd->graphicsState &&
        lpsd->graphicsBackend->GetNextEstimatedFrameTime)
    {
      lpsd->graphicsBackend->GetNextEstimatedFrameTime(
        lpsd->graphicsState,
        &lpsd->presentedTargetFrame);
    }
    if (lpsd->graphicsBackend && lpsd->graphicsState &&
        lpsd->graphicsBackend->GetObservedPresentationTarget &&
        lpsd->graphicsBackend->GetObservedPresentationTarget(
          lpsd->graphicsState,
          &observedTarget) &&
        observedTarget < MAG_PRESENT_COUNT)
    {
      lpsd->presentationStatus.observedTarget =
        (MAGPRESENTATIONTARGET)observedTarget;
      if (lpsd->presentationSettings.strictTarget &&
          MAG_PRESENT_AUTO != lpsd->presentationSettings.target &&
          lpsd->presentationSettings.target != observedTarget)
      {
        lpsd->presentationStatus.configurationSupported = FALSE;
        _sntprintf_s(
          lpsd->presentationStatus.reason,
          ARRAYSIZE(lpsd->presentationStatus.reason),
          _TRUNCATE,
          TEXT("The configured flip presenter is active, but Windows observed presentation target %u instead of strict target %u."),
          observedTarget,
          (UINT)lpsd->presentationSettings.target);
      }
    }
    lpsd->fPresentedContentValid = TRUE;
    return TRUE;
}

static BOOL renderSubmitGeometryFrame(
  HWND hWnd,
  BOOL restartSequence,
  BOOL synchronize)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    MAGPRESENTINTENT oldIntent;
    BOOL oldIntentActive;
    BOOL result;

    if (!lpsd)
    {
      return FALSE;
    }
    oldIntent = lpsd->presentIntent;
    oldIntentActive = lpsd->fPresentIntentActive;
    lpsd->presentIntent.restartSequence = restartSequence;
    lpsd->presentIntent.synchronize = synchronize;
    lpsd->fPresentIntentActive = TRUE;
    result = renderSubmit(hWnd);
    lpsd->presentIntent = oldIntent;
    lpsd->fPresentIntentActive = oldIntentActive;
    return result;
}

BOOL renderSubmitLiveFrame(HWND hWnd)
{
    return renderSubmitGeometryFrame(hWnd, FALSE, FALSE);
}

BOOL renderSubmitStateTransitionFrame(HWND hWnd, BOOL synchronize)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BOOL presented;

    if (!lpsd ||
        ((!lpsd->graphicsBackend || !lpsd->graphicsState) &&
         !render_captureUsesCompositor(lpsd->captureApi)))
    {
      return FALSE;
    }

    ++lpsd->stateTransitionEpoch;
    presented = renderSubmitGeometryFrame(hWnd, TRUE, synchronize);
    if (!presented ||
        lpsd->presentedStateTransitionEpoch != lpsd->stateTransitionEpoch)
    {
      lpsd->fResizeContractViolation = TRUE;
      return FALSE;
    }
    return TRUE;
}

BOOL renderSubmit(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BOOL fPresented;

    if (!lpsd)
    {
      return FALSE;
    }

    (void)render_transitionCaptureApi(hWnd, lpsd->captureApi);
    switch (lpsd->captureApi)
    {
    case CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE:
      render_wgcCaptureScreen(hWnd);
      break;
    case CAPTURE_API_DXGI_DESKTOP_DUPLICATION:
      render_dxgiCaptureScreen(hWnd);
      break;
    case CAPTURE_API_DWM_THUMBNAIL:
      fPresented = render_dwmThumbnailCaptureScreen(hWnd);
      break;
    case CAPTURE_API_DWM_PRIVATE_VISUAL:
      fPresented = render_dwmPrivateCaptureScreen(hWnd);
      break;
    case CAPTURE_API_GDI_BITBLT:
    default:
      render_gdiCaptureScreen(hWnd);
      break;
    }

    if (CAPTURE_API_DWM_THUMBNAIL != lpsd->captureApi &&
        CAPTURE_API_DWM_PRIVATE_VISUAL != lpsd->captureApi)
    {
      fPresented = render_presentPixelFrame(hWnd);
    }

    if (fPresented)
    {
      render_stampPresentedContent(hWnd);
    }
    return fPresented;
}

HANDLE renderDuplicateFrameWaitHandle(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    HANDLE sourceHandle = NULL;
    HANDLE duplicateHandle = NULL;

    if (!lpsd)
    {
      return NULL;
    }

    AcquireSRWLockShared(&lpsd->graphicsLock);
    if (lpsd->graphicsBackend && lpsd->graphicsState)
    {
      sourceHandle = lpsd->graphicsBackend->GetFrameWaitHandle(lpsd->graphicsState);
      if (sourceHandle)
      {
        DuplicateHandle(
          GetCurrentProcess(),
          sourceHandle,
          GetCurrentProcess(),
          &duplicateHandle,
          SYNCHRONIZE,
          FALSE,
          0);
      }
    }
    ReleaseSRWLockShared(&lpsd->graphicsLock);
    return duplicateHandle;
}

BOOL render_presentPixelFrame(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd)
    {
      return FALSE;
    }

    if (render_tryPresentPixelFrame(hWnd))
    {
      render_stampPresentedContent(hWnd);
      return TRUE;
    }

    if (render_recreateGraphicsBackend(hWnd) && render_tryPresentPixelFrame(hWnd))
    {
      render_stampPresentedContent(hWnd);
      return TRUE;
    }

    if (GRAPHICS_API_GDI != lpsd->graphicsApi)
    {
      TCHAR reason[256];

      if (renderApplyPresentationSettings(
            hWnd,
            GRAPHICS_API_GDI,
            lpsd->uiGraphicsApi,
            lpsd->textRenderer,
            reason,
            ARRAYSIZE(reason)))
      {
        if (render_tryPresentPixelFrame(hWnd))
        {
          render_stampPresentedContent(hWnd);
          return TRUE;
        }
      }
    }
    return FALSE;
}

BOOL render_recreateGraphicsBackend(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const MAGGRAPHICSBACKEND* backend;
    void* candidateState = NULL;
    void* oldState;
    SIZE clientSize;

    if (!lpsd || !(backend = lpsd->graphicsBackend))
    {
      return FALSE;
    }

    clientSize.cx = max(1, lpsd->bi.biWidth);
    clientSize.cy = max(1, lpsd->bi.biHeight);
    if (!backend->Create(
          hWnd,
          clientSize,
          &lpsd->resolvedPresentation,
          &candidateState) || !candidateState)
    {
      return FALSE;
    }
    if (!backend->SetPresentationEnabled(
          hWnd,
          candidateState,
          lpsd->fGraphicsPresentationEnabled))
    {
      backend->Destroy(hWnd, candidateState);
      return FALSE;
    }

    AcquireSRWLockExclusive(&lpsd->graphicsLock);
    oldState = lpsd->graphicsState;
    lpsd->graphicsState = candidateState;
    ReleaseSRWLockExclusive(&lpsd->graphicsLock);

    if (oldState)
    {
      backend->Destroy(hWnd, oldState);
    }
    return TRUE;
}

BOOL render_tryPresentPixelFrame(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    MAGUIDRAWLIST backendUi;

    if (!lpsd)
    {
      return FALSE;
    }

    render_buildUiDrawList(hWnd, &lpsd->uiDrawList);
    if (lpsd->graphicsBackend && lpsd->graphicsState)
    {
      const MAGPIXELBUFFER* presentationFrame = &lpsd->frame;
      const MAGUIDRAWLIST* presentationUi = &lpsd->uiDrawList;
      const BOOL useOpenGlGlyphAtlas =
        GRAPHICS_API_OPENGL == lpsd->graphicsApi &&
        MAG_HOST_TRADITIONAL_LAYERED != lpsd->resolvedPresentation.host &&
        TEXT_RENDERER_GPU_GLYPH_ATLAS == lpsd->textRenderer;

      if (lpsd->uiRenderer)
      {
        BOOL composed;

        if (useOpenGlGlyphAtlas)
        {
          UINT i;

          composed = magUiRendererComposeWithoutText(
            lpsd->uiRenderer,
            &lpsd->frame,
            &lpsd->uiDrawList,
            &lpsd->presentationFrame);
          magUiDrawListReset(&backendUi);
          for (i = 0; i < lpsd->uiDrawList.count; ++i)
          {
            if (MAG_UI_DRAW_TEXT == lpsd->uiDrawList.commands[i].type &&
                backendUi.count < ARRAYSIZE(backendUi.commands))
            {
              backendUi.commands[backendUi.count++] = lpsd->uiDrawList.commands[i];
            }
          }
          backendUi.glyphAtlas = magUiRendererGetGlyphAtlas(lpsd->uiRenderer);
          presentationUi = &backendUi;
        }
        else
        {
          composed = magUiRendererCompose(
            lpsd->uiRenderer,
            &lpsd->frame,
            &lpsd->uiDrawList,
            &lpsd->presentationFrame);
          presentationUi = NULL;
        }

        if (!composed)
        {
          return FALSE;
        }
        presentationFrame = &lpsd->presentationFrame;
      }
      if (MAG_HOST_TRADITIONAL_LAYERED == lpsd->resolvedPresentation.host)
      {
        return lpsd->layeredPresenter && magLayeredPresenterPresent(
          lpsd->layeredPresenter,
          hWnd,
          presentationFrame,
          &lpsd->resolvedPresentation);
      }
      return lpsd->graphicsBackend->Render(
        hWnd,
        lpsd->graphicsState,
        presentationFrame,
        presentationUi,
        lpsd->fPresentIntentActive ? &lpsd->presentIntent : NULL);
    }
    return FALSE;
}

static void render_fillSmokeFrame(LPMAGSTATE lpsd)
{
    UINT y;

    for (y = 0; y < lpsd->frame.height; ++y)
    {
      BYTE* row = lpsd->frame.pixels + (SIZE_T)y * lpsd->frame.stride;
      UINT x;

      for (x = 0; x < lpsd->frame.width; ++x)
      {
        row[x * 4U + 0] = (BYTE)((x * 255U) / max(1U, lpsd->frame.width - 1U));
        row[x * 4U + 1] = (BYTE)((y * 255U) / max(1U, lpsd->frame.height - 1U));
        row[x * 4U + 2] = (BYTE)(255U - row[x * 4U + 0]);
        row[x * 4U + 3] = 255;
      }
    }
}

static BOOL render_testCpuCompositor(void)
{
    BYTE pixels[4U * 4U * 4U];
    MAGPIXELBUFFER frame =
    {
      pixels,
      4,
      4,
      4U * 4U,
      MAG_ROW_ORDER_BOTTOM_UP,
      MAG_ALPHA_MODE_IGNORE,
    };
    MAGPIXELBUFFER output;
    MAGCPUCOMPOSITOR compositor = { 0 };
    MAGUIDRAWLIST ui;
    RECT rect = { 0, 0, 4, 4 };
    MAGCOLORF white = { 1.0f, 1.0f, 1.0f, 1.0f };
    MAGCOLORF halfRed = { 1.0f, 0.0f, 0.0f, 0.5f };
    BOOL success = FALSE;
    UINT x;

    ZeroMemory(pixels, sizeof(pixels));
    for (x = 0; x < 4; ++x)
    {
      pixels[x * 4U + 0] = 255;
      pixels[(3U * frame.stride) + x * 4U + 2] = 255;
    }
    magUiDrawListReset(&ui);
    if (!magGraphicsComposeFrame(&compositor, &frame, &ui, &output) ||
        255 != output.pixels[2] ||
        255 != output.pixels[(3U * output.stride) + 0])
    {
      goto cleanup;
    }

    magUiDrawListReset(&ui);
    if (!magUiDrawListAppendStroke(&ui, &rect, white, 1.0f) ||
        !magGraphicsComposeFrame(&compositor, &frame, &ui, &output) ||
        255 != output.pixels[(0U * output.stride) + (2U * 4U) + 2] ||
        255 != output.pixels[(3U * output.stride) + (2U * 4U) + 2] ||
        255 != output.pixels[(2U * output.stride) + (0U * 4U) + 2] ||
        255 != output.pixels[(2U * output.stride) + (3U * 4U) + 2] ||
        255 == output.pixels[(1U * output.stride) + (1U * 4U) + 2])
    {
      goto cleanup;
    }

    SetRect(&rect, 1, 1, 3, 3);
    magUiDrawListReset(&ui);
    if (!magUiDrawListAppendFill(&ui, &rect, halfRed) ||
        !magGraphicsComposeFrame(&compositor, &frame, &ui, &output) ||
        128 != output.pixels[(1U * output.stride) + (1U * 4U) + 2])
    {
      goto cleanup;
    }
    success = TRUE;

cleanup:
    magGraphicsDestroyCpuCompositor(&compositor);
    return success;
}

static BOOL render_smokeResizeWindow(HWND hWnd, GRAPHICSAPI expectedApi, SIZE desiredClientSize)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    RECT rcWindow;
    RECT rcClient;
    SIZE initialSize;
    UINT step;
    UINT precommitCount;
    UINT commitCount;
    UINT64 graphicsGeneration;
    UINT64 captureGeneration;
    UINT64 uiGeneration;
    UINT64 layeredGeneration;
    UINT64 dwmPrivateGeneration;
    ULONGLONG resizeStart;
    ULONGLONG resizeElapsed;
    ULONGLONG epochLatencyLimit;

    if (!lpsd)
    {
      return FALSE;
    }
    precommitCount = lpsd->resizePrecommitCount;
    commitCount = lpsd->resizeCommitCount;
    graphicsGeneration = lpsd->graphicsBackend->GetResourceGeneration(lpsd->graphicsState);
    captureGeneration = lpsd->captureSurfaceGeneration;
    uiGeneration = magUiRendererGetSurfaceGeneration(lpsd->uiRenderer);
    layeredGeneration = magLayeredPresenterGetResourceGeneration(lpsd->layeredPresenter);
    dwmPrivateGeneration = DwmPrivateCaptureGetResourceGeneration(lpsd->dwmPrivate.state);
    epochLatencyLimit = CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE == lpsd->captureApi
      ? 500U
      : (CAPTURE_API_DXGI_DESKTOP_DUPLICATION == lpsd->captureApi ? 250U : 100U);
    resizeStart = GetTickCount64();
    SendMessage(hWnd, WM_ENTERSIZEMOVE, 0, 0);

    if (!GetClientRect(hWnd, &rcClient))
    {
      SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
      return FALSE;
    }
    initialSize.cx = RECTWIDTH(rcClient);
    initialSize.cy = RECTHEIGHT(rcClient);

    /*
     * Exercise several distinct geometry commits while the modal size loop is
     * still active.  Each synchronous SetWindowPos must return only after its
     * WM_WINDOWPOSCHANGED has submitted the matching frame.  There is
     * deliberately no manual render here and no exit-loop rescue present.
     */
    for (step = 1; step <= 4; ++step)
    {
      const SIZE stepSize =
      {
        initialSize.cx + MulDiv(desiredClientSize.cx - initialSize.cx, (INT)step, 4),
        initialSize.cy + MulDiv(desiredClientSize.cy - initialSize.cy, (INT)step, 4)
      };
      UINT attempt;

      for (attempt = 0; attempt < 3; ++attempt)
      {
        LONG targetWidth;
        LONG targetHeight;
        SIZE actualSize;
        UINT commitBefore = lpsd->resizeCommitCount;
        UINT64 epochBefore = lpsd->committedGeometryEpoch;
        ULONGLONG epochStart = GetTickCount64();

        if (!GetWindowRect(hWnd, &rcWindow) || !GetClientRect(hWnd, &rcClient))
        {
          SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
          return FALSE;
        }
        if (RECTWIDTH(rcClient) == stepSize.cx && RECTHEIGHT(rcClient) == stepSize.cy)
        {
          break;
        }

        targetWidth = stepSize.cx + RECTWIDTH(rcWindow) - RECTWIDTH(rcClient);
        targetHeight = stepSize.cy + RECTHEIGHT(rcWindow) - RECTHEIGHT(rcClient);
        if (!SetWindowPos(
              hWnd,
              NULL,
              0,
              0,
              targetWidth,
              targetHeight,
              SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE) ||
            !GetClientRect(hWnd, &rcClient))
        {
          SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
          return FALSE;
        }
        actualSize.cx = RECTWIDTH(rcClient);
        actualSize.cy = RECTHEIGHT(rcClient);

        if (actualSize.cx < 1 || actualSize.cy < 1 ||
            lpsd->resizeCommitCount <= commitBefore ||
            lpsd->committedGeometryEpoch <= epochBefore ||
            lpsd->presentedGeometryEpoch != lpsd->committedGeometryEpoch ||
            !lpsd->fPresentedContentValid ||
            lpsd->presentedContentSize.cx != actualSize.cx ||
            lpsd->presentedContentSize.cy != actualSize.cy ||
            lpsd->frame.width != (UINT)actualSize.cx ||
            lpsd->frame.height != (UINT)actualSize.cy ||
            lpsd->fDeferredResize ||
            lpsd->fGeometryTransition ||
            GetTickCount64() - epochStart > epochLatencyLimit)
        {
          TCHAR detail[768];

          lpsd->fResizeContractViolation = TRUE;
          _sntprintf_s(
            detail,
            ARRAYSIZE(detail),
            _TRUNCATE,
            TEXT("Live resize epoch %u.%u: requested=%ldx%ld actual=%ldx%ld frame=%ux%u commit=%u/%u geometry=%llu/%llu presented=%llu valid=%u presented-size=%ldx%ld deferred=%u transition=%u elapsed=%llums"),
            step,
            attempt + 1,
            stepSize.cx,
            stepSize.cy,
            actualSize.cx,
            actualSize.cy,
            lpsd->frame.width,
            lpsd->frame.height,
            lpsd->resizeCommitCount,
            commitBefore,
            lpsd->committedGeometryEpoch,
            epochBefore,
            lpsd->presentedGeometryEpoch,
            lpsd->fPresentedContentValid,
            lpsd->presentedContentSize.cx,
            lpsd->presentedContentSize.cy,
            lpsd->fDeferredResize,
            lpsd->fGeometryTransition,
            GetTickCount64() - epochStart);
          render_smokeFailure(2, detail);
          SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
          return FALSE;
        }

        {
          MAGUIDRAWLIST ui;
          UINT commandIndex;

          render_buildUiDrawList(hWnd, &ui);
          if (!EqualRect(&lpsd->rc, &rcClient))
          {
            TCHAR detail[256];

            _sntprintf_s(
              detail,
              ARRAYSIZE(detail),
              _TRUNCATE,
              TEXT("Committed UI bounds are stale: state=(%ld,%ld)-(%ld,%ld), client=(%ld,%ld)-(%ld,%ld)."),
              lpsd->rc.left,
              lpsd->rc.top,
              lpsd->rc.right,
              lpsd->rc.bottom,
              rcClient.left,
              rcClient.top,
              rcClient.right,
              rcClient.bottom);
            render_smokeFailure(3, detail);
            lpsd->fResizeContractViolation = TRUE;
            SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
            return FALSE;
          }
          for (commandIndex = 0; commandIndex < ui.count; ++commandIndex)
          {
            const RECT* rect = &ui.commands[commandIndex].rect;

            if (rect->left < rcClient.left || rect->top < rcClient.top ||
                rect->right > rcClient.right || rect->bottom > rcClient.bottom ||
                IsRectEmpty(rect))
            {
              TCHAR detail[256];

              _sntprintf_s(
                detail,
                ARRAYSIZE(detail),
                _TRUNCATE,
                TEXT("UI behavior rectangle %u is outside committed client geometry: rect=(%ld,%ld)-(%ld,%ld), client=(%ld,%ld)-(%ld,%ld)."),
                commandIndex,
                rect->left,
                rect->top,
                rect->right,
                rect->bottom,
                rcClient.left,
                rcClient.top,
                rcClient.right,
                rcClient.bottom);
              render_smokeFailure(4, detail);
              lpsd->fResizeContractViolation = TRUE;
              SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
              return FALSE;
            }
          }
        }
      }
      if (!GetClientRect(hWnd, &rcClient) ||
          RECTWIDTH(rcClient) != stepSize.cx ||
          RECTHEIGHT(rcClient) != stepSize.cy)
      {
        TCHAR detail[256];

        _sntprintf_s(
          detail,
          ARRAYSIZE(detail),
          _TRUNCATE,
          TEXT("Resize target did not converge in three committed epochs: requested=%ldx%ld actual=%ldx%ld."),
          stepSize.cx,
          stepSize.cy,
          RECTWIDTH(rcClient),
          RECTHEIGHT(rcClient));
        render_smokeFailure(5, detail);
        lpsd->fResizeContractViolation = TRUE;
        SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
        return FALSE;
      }
    }

    /* Exiting the loop must not be the event that advances presentation. */
    {
      const UINT commitBeforeExit = lpsd->resizeCommitCount;
      const UINT64 presentedEpochBeforeExit = lpsd->presentedGeometryEpoch;

      SendMessage(hWnd, WM_EXITSIZEMOVE, 0, 0);
      if (lpsd->resizeCommitCount != commitBeforeExit ||
          lpsd->presentedGeometryEpoch != presentedEpochBeforeExit)
      {
        render_smokeFailure(
          6,
          TEXT("WM_EXITSIZEMOVE advanced presentation; the last modal-loop epoch was not final."));
        lpsd->fResizeContractViolation = TRUE;
        return FALSE;
      }
    }
    resizeElapsed = GetTickCount64() - resizeStart;

    if (!GetClientRect(hWnd, &rcClient) ||
        RECTWIDTH(rcClient) != desiredClientSize.cx ||
        RECTHEIGHT(rcClient) != desiredClientSize.cy ||
        lpsd->graphicsApi != expectedApi ||
        lpsd->frame.width != (UINT)desiredClientSize.cx ||
        lpsd->frame.height != (UINT)desiredClientSize.cy ||
        lpsd->graphicsBackend->GetResourceGeneration(lpsd->graphicsState) != graphicsGeneration ||
        lpsd->captureSurfaceGeneration != captureGeneration ||
        magUiRendererGetSurfaceGeneration(lpsd->uiRenderer) != uiGeneration ||
        magLayeredPresenterGetResourceGeneration(lpsd->layeredPresenter) != layeredGeneration ||
        DwmPrivateCaptureGetResourceGeneration(lpsd->dwmPrivate.state) != dwmPrivateGeneration ||
        lpsd->fResizeContractViolation ||
        resizeElapsed > max(400U, epochLatencyLimit * 6U) ||
        lpsd->resizePrecommitCount <= precommitCount ||
        lpsd->resizeCommitCount <= commitCount)
    {
      TCHAR detail[768];

      _sntprintf_s(
        detail,
        ARRAYSIZE(detail),
        _TRUNCATE,
        TEXT("Resize contract: requested=%ldx%ld actual=%ldx%ld frame=%ux%u api=%u/%u generation=%llu/%llu capture=%llu/%llu ui=%llu/%llu layered=%llu/%llu dwm-private=%llu/%llu violation=%u elapsed=%llums precommit=%u/%u commit=%u/%u"),
        desiredClientSize.cx,
        desiredClientSize.cy,
        RECTWIDTH(rcClient),
        RECTHEIGHT(rcClient),
        lpsd->frame.width,
        lpsd->frame.height,
        (UINT)lpsd->graphicsApi,
        (UINT)expectedApi,
        lpsd->graphicsBackend->GetResourceGeneration(lpsd->graphicsState),
        graphicsGeneration,
        lpsd->captureSurfaceGeneration,
        captureGeneration,
        magUiRendererGetSurfaceGeneration(lpsd->uiRenderer),
        uiGeneration,
        magLayeredPresenterGetResourceGeneration(lpsd->layeredPresenter),
        layeredGeneration,
        DwmPrivateCaptureGetResourceGeneration(lpsd->dwmPrivate.state),
        dwmPrivateGeneration,
        lpsd->fResizeContractViolation,
        resizeElapsed,
        lpsd->resizePrecommitCount,
        precommitCount,
        lpsd->resizeCommitCount,
        commitCount);
      render_smokeFailure(1, detail);
      return FALSE;
    }

    if (render_captureUsesCompositor(lpsd->captureApi))
    {
      return TRUE;
    }

    render_fillSmokeFrame(lpsd);
    if (!render_tryPresentPixelFrame(hWnd))
    {
      return FALSE;
    }
    return render_stampPresentedContent(hWnd);
}

static BOOL render_smokeWindowTransitions(HWND hWnd, GRAPHICSAPI expectedApi)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    RECT originalWindow;
    RECT clientRect;
    DWORD_PTR originalStyle;
    UINT64 graphicsGeneration;
    UINT64 captureGeneration;
    UINT64 uiGeneration;
    UINT64 layeredGeneration;
    UINT64 dwmPrivateGeneration;
    UINT64 geometryEpoch;
    UINT64 transitionEpoch;
    const UINT activationStates[] = { WA_INACTIVE, WA_ACTIVE };
    UINT i;

    if (!lpsd || !GetWindowRect(hWnd, &originalWindow) ||
        !GetClientRect(hWnd, &clientRect))
    {
      render_smokeFailure(7, TEXT("Transition test could not read initial window geometry."));
      return FALSE;
    }
    originalStyle = (DWORD_PTR)GetWindowLongPtr(hWnd, GWL_EXSTYLE);
    graphicsGeneration = lpsd->graphicsBackend->GetResourceGeneration(lpsd->graphicsState);
    captureGeneration = lpsd->captureSurfaceGeneration;
    uiGeneration = magUiRendererGetSurfaceGeneration(lpsd->uiRenderer);
    layeredGeneration = magLayeredPresenterGetResourceGeneration(lpsd->layeredPresenter);
    dwmPrivateGeneration = DwmPrivateCaptureGetResourceGeneration(lpsd->dwmPrivate.state);
    geometryEpoch = lpsd->committedGeometryEpoch;
    transitionEpoch = lpsd->stateTransitionEpoch;

    if (!SetWindowPos(
          hWnd,
          NULL,
          originalWindow.left + 13,
          originalWindow.top + 7,
          0,
          0,
          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) ||
        lpsd->stateTransitionEpoch <= transitionEpoch ||
        lpsd->presentedStateTransitionEpoch != lpsd->stateTransitionEpoch ||
        !lpsd->fPresentedContentValid ||
        lpsd->graphicsApi != expectedApi ||
        lpsd->committedGeometryEpoch != geometryEpoch)
    {
      TCHAR detail[384];
      RECT actualWindow = { 0 };

      GetWindowRect(hWnd, &actualWindow);

      _sntprintf_s(
        detail,
        ARRAYSIZE(detail),
        _TRUNCATE,
        TEXT("Pure move transition: transition=%llu/%llu presented=%llu valid=%u api=%u/%u geometry=%llu/%llu violation=%u original=(%ld,%ld)-(%ld,%ld) actual=(%ld,%ld)-(%ld,%ld) visible=%u zoomed=%u."),
        lpsd->stateTransitionEpoch,
        transitionEpoch,
        lpsd->presentedStateTransitionEpoch,
        lpsd->fPresentedContentValid,
        (UINT)lpsd->graphicsApi,
        (UINT)expectedApi,
        lpsd->committedGeometryEpoch,
        geometryEpoch,
        lpsd->fResizeContractViolation,
        originalWindow.left,
        originalWindow.top,
        originalWindow.right,
        originalWindow.bottom,
        actualWindow.left,
        actualWindow.top,
        actualWindow.right,
        actualWindow.bottom,
        IsWindowVisible(hWnd),
        IsZoomed(hWnd));
      render_smokeFailure(8, detail);
      return FALSE;
    }
    transitionEpoch = lpsd->stateTransitionEpoch;
    if (!SetWindowPos(
          hWnd,
          NULL,
          originalWindow.left,
          originalWindow.top,
          0,
          0,
          SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE) ||
        lpsd->stateTransitionEpoch <= transitionEpoch ||
        lpsd->presentedStateTransitionEpoch != lpsd->stateTransitionEpoch ||
        lpsd->committedGeometryEpoch != geometryEpoch)
    {
      render_smokeFailure(9, TEXT("Restoring the window position did not synchronously publish its restarted transition epoch."));
      return FALSE;
    }

    for (i = 0; i < ARRAYSIZE(activationStates); ++i)
    {
      transitionEpoch = lpsd->stateTransitionEpoch;
      SendMessage(hWnd, WM_ACTIVATE, MAKEWPARAM(activationStates[i], FALSE), 0);
      if (lpsd->stateTransitionEpoch <= transitionEpoch ||
          lpsd->presentedStateTransitionEpoch != lpsd->stateTransitionEpoch ||
          !lpsd->fPresentedContentValid ||
          lpsd->committedGeometryEpoch != geometryEpoch)
      {
        render_smokeFailure(
          10,
          activationStates[i] == WA_INACTIVE
            ? TEXT("Deactivation did not synchronously publish its restarted transition epoch.")
            : TEXT("Activation did not synchronously publish its restarted transition epoch."));
        return FALSE;
      }
    }

    if ((DWORD_PTR)GetWindowLongPtr(hWnd, GWL_EXSTYLE) != originalStyle ||
        lpsd->graphicsBackend->GetResourceGeneration(lpsd->graphicsState) != graphicsGeneration ||
        lpsd->captureSurfaceGeneration != captureGeneration ||
        magUiRendererGetSurfaceGeneration(lpsd->uiRenderer) != uiGeneration ||
        magLayeredPresenterGetResourceGeneration(lpsd->layeredPresenter) != layeredGeneration ||
        DwmPrivateCaptureGetResourceGeneration(lpsd->dwmPrivate.state) != dwmPrivateGeneration)
    {
      TCHAR detail[512];

      _sntprintf_s(
        detail,
        ARRAYSIZE(detail),
        _TRUNCATE,
        TEXT("Move/activation changed stable resources or window style: style=%p/%p graphics=%llu/%llu capture=%llu/%llu ui=%llu/%llu layered=%llu/%llu dwm-private=%llu/%llu."),
        (void*)(DWORD_PTR)GetWindowLongPtr(hWnd, GWL_EXSTYLE),
        (void*)originalStyle,
        lpsd->graphicsBackend->GetResourceGeneration(lpsd->graphicsState),
        graphicsGeneration,
        lpsd->captureSurfaceGeneration,
        captureGeneration,
        magUiRendererGetSurfaceGeneration(lpsd->uiRenderer),
        uiGeneration,
        magLayeredPresenterGetResourceGeneration(lpsd->layeredPresenter),
        layeredGeneration,
        DwmPrivateCaptureGetResourceGeneration(lpsd->dwmPrivate.state),
        dwmPrivateGeneration);
      render_smokeFailure(11, detail);
      return FALSE;
    }
    return TRUE;
}

static int render_smokeFailure(int code, LPCTSTR reason)
{
    TCHAR message[768];
    DWORD written;
    HANDLE errorHandle = GetStdHandle(STD_ERROR_HANDLE);

    _sntprintf_s(
      message,
      ARRAYSIZE(message),
      _TRUNCATE,
      TEXT("MAG isolated smoke failure %d: %s\r\n"),
      code,
      (reason && reason[0]) ? reason : TEXT("No detail was reported."));
    OutputDebugString(message);
    if (errorHandle && INVALID_HANDLE_VALUE != errorHandle)
    {
      WriteFile(
        errorHandle,
        message,
        (DWORD)(lstrlen(message) * sizeof(TCHAR)),
        &written,
        NULL);
    }
    return code;
}

int renderRunGraphicsSmoke(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const MAGGRAPHICSBACKEND* availableBackends[GRAPHICS_API_COUNT];
    UINT availableBackendCount = 0;
    RECT initialClientRect;
    SIZE initialClientSize;
    GRAPHICSAPI initialGraphicsApi;
    CAPTUREAPI initialCaptureApi;
    UIGRAPHICSAPI initialUiApi;
    TEXTRENDERER initialTextRenderer;
    MAGPRESENTATIONSETTINGS initialPresentation;
    UINT backendIndex;

    if (lpsd && !lpsd->frame.pixels)
    {
      render_gdiCreateResources(hWnd);
    }
    if (!lpsd || !lpsd->frame.pixels || !GetClientRect(hWnd, &initialClientRect) ||
        !render_testCpuCompositor())
    {
      return 1;
    }
    initialClientSize.cx = RECTWIDTH(initialClientRect);
    initialClientSize.cy = RECTHEIGHT(initialClientRect);
    initialGraphicsApi = lpsd->graphicsApi;
    initialCaptureApi = lpsd->captureApi;
    initialUiApi = lpsd->uiGraphicsApi;
    initialTextRenderer = lpsd->textRenderer;
    initialPresentation = lpsd->presentationSettings;

    for (backendIndex = 0; backendIndex < magGraphicsGetBackendCount(); ++backendIndex)
    {
      const MAGGRAPHICSBACKEND* backend = magGraphicsGetBackendAt(backendIndex);
      TCHAR reason[256];
      UINT uiApi;

      if (!backend->implemented || !backend->IsAvailable(reason, ARRAYSIZE(reason)))
      {
        continue;
      }
      availableBackends[availableBackendCount++] = backend;

      for (uiApi = 0; uiApi < UI_GRAPHICS_API_COUNT; ++uiApi)
      {
        UINT textRenderer;

        for (textRenderer = 0; textRenderer < TEXT_RENDERER_COUNT; ++textRenderer)
        {
          MAGPRESENTATIONSETTINGS smokePresentation;
          UINT frameIndex;

          magPresentationSettingsSetDefaults(&smokePresentation);
          if (!renderApplyFullSettings(
                hWnd,
                backend->api,
                CAPTURE_API_GDI_BITBLT,
                (UIGRAPHICSAPI)uiApi,
                (TEXTRENDERER)textRenderer,
                &smokePresentation,
                reason,
                ARRAYSIZE(reason)))
          {
            return render_smokeFailure(
              10 + (int)backend->api * 10 + (int)uiApi * 3 + (int)textRenderer,
              reason);
          }
          render_fillSmokeFrame(lpsd);
          render_minimapNotifyActivity(hWnd);

          for (frameIndex = 0; frameIndex < 3; ++frameIndex)
          {
            if (!render_tryPresentPixelFrame(hWnd))
            {
              return 100 + (int)backend->api * 10 + (int)uiApi * 3 + (int)textRenderer;
            }
          }
        }
      }

      {
        const SIZE resizeSizes[] = { { 337, 251 }, { 509, 347 } };
        UINT resizeIndex;

      for (resizeIndex = 0; resizeIndex < ARRAYSIZE(resizeSizes); ++resizeIndex)
      {
        if (!render_smokeResizeWindow(hWnd, backend->api, resizeSizes[resizeIndex]))
        {
          return 160 + (int)backend->api;
        }
      }
      if (!render_smokeWindowTransitions(hWnd, backend->api))
      {
        return 170 + (int)backend->api;
      }
      }
    }
    for (backendIndex = 0; backendIndex < availableBackendCount; ++backendIndex)
    {
      UINT targetIndex;

      for (targetIndex = 0; targetIndex < availableBackendCount; ++targetIndex)
      {
        TCHAR reason[256];

        if (!renderApplyPresentationSettings(
              hWnd,
              availableBackends[backendIndex]->api,
              UI_GRAPHICS_API_NATIVE,
              TEXT_RENDERER_DIRECTWRITE,
              reason,
              ARRAYSIZE(reason)))
        {
          return 180 + (int)backendIndex * GRAPHICS_API_COUNT + (int)targetIndex;
        }
        if (!renderApplyPresentationSettings(
              hWnd,
              availableBackends[targetIndex]->api,
              UI_GRAPHICS_API_NATIVE,
              TEXT_RENDERER_DIRECTWRITE,
              reason,
              ARRAYSIZE(reason)))
        {
          return 400 + (int)backendIndex * GRAPHICS_API_COUNT + (int)targetIndex;
        }
        render_fillSmokeFrame(lpsd);
        if (!render_tryPresentPixelFrame(hWnd))
        {
          return 220 + (int)backendIndex * GRAPHICS_API_COUNT + (int)targetIndex;
        }
      }
    }

    for (backendIndex = 0; backendIndex < availableBackendCount; ++backendIndex)
    {
      static const MAGPRESENTATIONTARGET flipTargets[] =
      {
        MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP,
        MAG_PRESENT_COMPOSED_FLIP,
        MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP,
      };
      UINT targetIndex;

      if (GRAPHICS_API_D3D11 != availableBackends[backendIndex]->api)
      {
        continue;
      }
      for (targetIndex = 0; targetIndex < ARRAYSIZE(flipTargets); ++targetIndex)
      {
        const SIZE resizeSizes[] = { { 337, 251 }, { 509, 347 } };
        MAGPRESENTATIONSETTINGS flipPresentation;
        TCHAR reason[256];
        UINT resizeIndex;

        magPresentationSettingsSetDefaults(&flipPresentation);
        flipPresentation.target = flipTargets[targetIndex];
        flipPresentation.host = MAG_HOST_REDIRECTED_HWND;
        flipPresentation.surfaceOwnership = MAG_SURFACE_REDIRECTION;
        flipPresentation.copyRequirement = MAG_COPY_ALLOW_CPU_ROUND_TRIP;
        if (!renderApplyFullSettings(
              hWnd,
              GRAPHICS_API_D3D11,
              CAPTURE_API_GDI_BITBLT,
              UI_GRAPHICS_API_NATIVE,
              TEXT_RENDERER_DIRECTWRITE,
              &flipPresentation,
              reason,
              ARRAYSIZE(reason)))
        {
          return render_smokeFailure(560 + (int)targetIndex, reason);
        }
        render_fillSmokeFrame(lpsd);
        if (!render_tryPresentPixelFrame(hWnd) || !render_stampPresentedContent(hWnd))
        {
          return 570 + (int)targetIndex;
        }
        for (resizeIndex = 0; resizeIndex < ARRAYSIZE(resizeSizes); ++resizeIndex)
        {
          if (!render_smokeResizeWindow(
                hWnd,
                GRAPHICS_API_D3D11,
                resizeSizes[resizeIndex]))
          {
            return 580 + (int)targetIndex;
          }
        }
      }
    }

    for (backendIndex = 0; backendIndex < availableBackendCount; ++backendIndex)
    {
      TCHAR reason[256];
      UINT captureApi;

      if (!renderApplySettings(
            hWnd,
            availableBackends[backendIndex]->api,
            CAPTURE_API_GDI_BITBLT,
            UI_GRAPHICS_API_DIRECT2D,
            TEXT_RENDERER_DIRECTWRITE,
            reason,
            ARRAYSIZE(reason)))
      {
        return 500 + (int)backendIndex;
      }

      for (captureApi = 0; captureApi < CAPTURE_API_COUNT; ++captureApi)
      {
        UINT frameIndex;

        if (!renderApplySettings(
              hWnd,
              availableBackends[backendIndex]->api,
              (CAPTUREAPI)captureApi,
              UI_GRAPHICS_API_DIRECT2D,
              TEXT_RENDERER_DIRECTWRITE,
              reason,
              ARRAYSIZE(reason)))
        {
          return 510 + (int)backendIndex * CAPTURE_API_COUNT + (int)captureApi;
        }
        render_fillSmokeFrame(lpsd);
        render_minimapNotifyActivity(hWnd);
        for (frameIndex = 0; frameIndex < 3; ++frameIndex)
        {
          if (!renderSubmit(hWnd))
          {
            return 520 + (int)backendIndex * CAPTURE_API_COUNT + (int)captureApi;
          }
        }
        {
          const SIZE compositorResize =
          {
            421 + (LONG)captureApi * 3,
            287 + (LONG)captureApi * 2
          };

          if (!render_smokeResizeWindow(
                hWnd,
                availableBackends[backendIndex]->api,
                compositorResize))
          {
            return 540 + (int)backendIndex * CAPTURE_API_COUNT + (int)captureApi;
          }
          if (!render_smokeWindowTransitions(
                hWnd,
                availableBackends[backendIndex]->api))
          {
            return 640 + (int)backendIndex * CAPTURE_API_COUNT + (int)captureApi;
          }
        }
      }
    }

    {
      const SIZE resizeSizes[] = { { 337, 251 }, { 509, 347 } };
      UINT alphaMode;

      for (alphaMode = 0; alphaMode < MAG_LAYER_ALPHA_COUNT; ++alphaMode)
      {
        UINT uiApi;

        for (uiApi = 0; uiApi < UI_GRAPHICS_API_COUNT; ++uiApi)
        {
          UINT textRenderer;

          for (textRenderer = 0; textRenderer < TEXT_RENDERER_COUNT; ++textRenderer)
          {
            MAGPRESENTATIONSETTINGS layeredPresentation;
            TCHAR reason[256];
            UINT resizeIndex;

            magPresentationSettingsSetDefaults(&layeredPresentation);
            layeredPresentation.host = MAG_HOST_TRADITIONAL_LAYERED;
            layeredPresentation.surfaceOwnership = MAG_SURFACE_REDIRECTION;
            layeredPresentation.target = MAG_PRESENT_COMPOSED_COPY_CPU_GDI;
            layeredPresentation.copyRequirement = MAG_COPY_ALLOW_CPU_ROUND_TRIP;
            layeredPresentation.alphaMode = (MAGLAYEREDALPHAMODE)alphaMode;
            layeredPresentation.constantAlpha = 191;
            layeredPresentation.colorKey = RGB(255, 0, 255);
            if (!renderApplyFullSettings(
                  hWnd,
                  GRAPHICS_API_GDI,
                  CAPTURE_API_GDI_BITBLT,
                  (UIGRAPHICSAPI)uiApi,
                  (TEXTRENDERER)textRenderer,
                  &layeredPresentation,
                  reason,
                  ARRAYSIZE(reason)))
            {
              return render_smokeFailure(
                600 + (int)alphaMode * 20 + (int)uiApi * 3 + (int)textRenderer,
                reason);
            }
            render_fillSmokeFrame(lpsd);
            if (!render_tryPresentPixelFrame(hWnd) || !render_stampPresentedContent(hWnd))
            {
              return 700 + (int)alphaMode * 20 + (int)uiApi * 3 + (int)textRenderer;
            }
            for (resizeIndex = 0; resizeIndex < ARRAYSIZE(resizeSizes); ++resizeIndex)
            {
              if (!render_smokeResizeWindow(
                    hWnd,
                    GRAPHICS_API_GDI,
                    resizeSizes[resizeIndex]))
              {
                return 800 + (int)alphaMode * 20 +
                  (int)uiApi * 3 + (int)textRenderer;
              }
            }
          }
        }
      }
    }
    {
      TCHAR reason[256];

      if (!renderApplyFullSettings(
            hWnd,
            initialGraphicsApi,
            initialCaptureApi,
            initialUiApi,
            initialTextRenderer,
            &initialPresentation,
            reason,
            ARRAYSIZE(reason)) ||
          (!render_captureUsesCompositor(initialCaptureApi) &&
           !render_smokeResizeWindow(hWnd, initialGraphicsApi, initialClientSize)))
      {
        return 300;
      }
    }

    return 0;
}

BOOL render_transitionCaptureApi(HWND hWnd, CAPTUREAPI captureApi)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd || lpsd->activeCaptureApi == captureApi)
    {
      return NULL != lpsd;
    }

    render_dwmPrivateDeleteResources(hWnd);
    render_dwmThumbnailDeleteResources(hWnd);
    render_wgcDeleteResources(hWnd);
    render_dxgiDeleteResources(hWnd);
    if (lpsd->graphicsBackend && lpsd->graphicsState &&
        !render_setGraphicsPresentationEnabled(
          hWnd,
          !render_captureUsesCompositor(captureApi)))
    {
      lpsd->activeCaptureApi = CAPTURE_API_COUNT;
      return FALSE;
    }
    lpsd->activeCaptureApi = captureApi;
    return TRUE;
}
