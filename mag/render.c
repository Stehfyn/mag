#include "render.h"

#include <roapi.h>
#include <windows.graphics.capture.h>
#include <windows.graphics.directx.direct3d11.interop.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")
#pragma comment(lib, "runtimeobject")

#define WGLCHECK(Func) do { if (!(Func)) { __debugbreak(); } } while (0)
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

#define DWM_ORD_CREATE_SHARED_DESKTOP_VISUAL 163
#define DWM_ORD_UPDATE_SHARED_DESKTOP_VISUAL 164
#define DWM_PRIVATE_MULTIWINDOW_BUILD 20000

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

static const IID IID_MAG_IDCompositionDevice =
  { 0xC37EA93A, 0xE7AA, 0x450D, { 0xB1, 0x6F, 0x97, 0x46, 0xCB, 0x04, 0x07, 0xF3 } };

static const IID IID_MAG_IDCompositionDesktopDevice =
  { 0x5F4633FE, 0x1E08, 0x4CB8, { 0x8C, 0x75, 0xCE, 0x24, 0x33, 0x3F, 0x56, 0x02 } };

typedef struct MAG_IDCompositionDevice MAG_IDCompositionDevice;
typedef struct MAG_IDCompositionDesktopDevice MAG_IDCompositionDesktopDevice;
typedef struct MAG_IDCompositionTarget MAG_IDCompositionTarget;

typedef struct MAG_IDCompositionDeviceVtbl
{
  BEGIN_INTERFACE
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionDevice* This, REFIID riid, void** ppvObject);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionDevice* This);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionDevice* This);
  HRESULT (STDMETHODCALLTYPE* Commit)(MAG_IDCompositionDevice* This);
  HRESULT (STDMETHODCALLTYPE* WaitForCommitCompletion)(MAG_IDCompositionDevice* This);
  HRESULT (STDMETHODCALLTYPE* GetFrameStatistics)(MAG_IDCompositionDevice* This, void* statistics);
  HRESULT (STDMETHODCALLTYPE* CreateTargetForHwnd)(MAG_IDCompositionDevice* This, HWND hwnd, BOOL topmost, MAG_IDCompositionTarget** target);
  END_INTERFACE
} MAG_IDCompositionDeviceVtbl;

struct MAG_IDCompositionDevice
{
  CONST_VTBL struct MAG_IDCompositionDeviceVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionDesktopDeviceVtbl
{
  BEGIN_INTERFACE
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionDesktopDevice* This, REFIID riid, void** ppvObject);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionDesktopDevice* This);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionDesktopDevice* This);
  HRESULT (STDMETHODCALLTYPE* Commit)(MAG_IDCompositionDesktopDevice* This);
  HRESULT (STDMETHODCALLTYPE* WaitForCommitCompletion)(MAG_IDCompositionDesktopDevice* This);
  HRESULT (STDMETHODCALLTYPE* GetFrameStatistics)(MAG_IDCompositionDesktopDevice* This, void* statistics);
  HRESULT (STDMETHODCALLTYPE* CreateVisual)(MAG_IDCompositionDesktopDevice* This, void** visual);
  HRESULT (STDMETHODCALLTYPE* CreateSurfaceFactory)(MAG_IDCompositionDesktopDevice* This, IUnknown* renderingDevice, void** surfaceFactory);
  HRESULT (STDMETHODCALLTYPE* CreateSurface)(MAG_IDCompositionDesktopDevice* This, UINT width, UINT height, DXGI_FORMAT pixelFormat, DXGI_ALPHA_MODE alphaMode, void** surface);
  HRESULT (STDMETHODCALLTYPE* CreateVirtualSurface)(MAG_IDCompositionDesktopDevice* This, UINT initialWidth, UINT initialHeight, DXGI_FORMAT pixelFormat, DXGI_ALPHA_MODE alphaMode, void** virtualSurface);
  HRESULT (STDMETHODCALLTYPE* CreateTranslateTransform)(MAG_IDCompositionDesktopDevice* This, void** translateTransform);
  HRESULT (STDMETHODCALLTYPE* CreateScaleTransform)(MAG_IDCompositionDesktopDevice* This, void** scaleTransform);
  HRESULT (STDMETHODCALLTYPE* CreateRotateTransform)(MAG_IDCompositionDesktopDevice* This, void** rotateTransform);
  HRESULT (STDMETHODCALLTYPE* CreateSkewTransform)(MAG_IDCompositionDesktopDevice* This, void** skewTransform);
  HRESULT (STDMETHODCALLTYPE* CreateMatrixTransform)(MAG_IDCompositionDesktopDevice* This, void** matrixTransform);
  HRESULT (STDMETHODCALLTYPE* CreateTransformGroup)(MAG_IDCompositionDesktopDevice* This, void** transforms, UINT elements, void** transformGroup);
  HRESULT (STDMETHODCALLTYPE* CreateTranslateTransform3D)(MAG_IDCompositionDesktopDevice* This, void** translateTransform3D);
  HRESULT (STDMETHODCALLTYPE* CreateScaleTransform3D)(MAG_IDCompositionDesktopDevice* This, void** scaleTransform3D);
  HRESULT (STDMETHODCALLTYPE* CreateRotateTransform3D)(MAG_IDCompositionDesktopDevice* This, void** rotateTransform3D);
  HRESULT (STDMETHODCALLTYPE* CreateMatrixTransform3D)(MAG_IDCompositionDesktopDevice* This, void** matrixTransform3D);
  HRESULT (STDMETHODCALLTYPE* CreateTransform3DGroup)(MAG_IDCompositionDesktopDevice* This, void** transforms3D, UINT elements, void** transform3DGroup);
  HRESULT (STDMETHODCALLTYPE* CreateEffectGroup)(MAG_IDCompositionDesktopDevice* This, void** effectGroup);
  HRESULT (STDMETHODCALLTYPE* CreateRectangleClip)(MAG_IDCompositionDesktopDevice* This, void** clip);
  HRESULT (STDMETHODCALLTYPE* CreateAnimation)(MAG_IDCompositionDesktopDevice* This, void** animation);
  HRESULT (STDMETHODCALLTYPE* CreateTargetForHwnd)(MAG_IDCompositionDesktopDevice* This, HWND hwnd, BOOL topmost, MAG_IDCompositionTarget** target);
  END_INTERFACE
} MAG_IDCompositionDesktopDeviceVtbl;

struct MAG_IDCompositionDesktopDevice
{
  CONST_VTBL struct MAG_IDCompositionDesktopDeviceVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionTargetVtbl
{
  BEGIN_INTERFACE
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionTarget* This, REFIID riid, void** ppvObject);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionTarget* This);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionTarget* This);
  HRESULT (STDMETHODCALLTYPE* SetRoot)(MAG_IDCompositionTarget* This, IUnknown* visual);
  END_INTERFACE
} MAG_IDCompositionTargetVtbl;

struct MAG_IDCompositionTarget
{
  CONST_VTBL struct MAG_IDCompositionTargetVtbl* lpVtbl;
};

typedef HRESULT (WINAPI* PFN_DCOMPOSITIONCREATEDEVICE)(IDXGIDevice* dxgiDevice, REFIID iid, void** dcompositionDevice);
typedef HRESULT (WINAPI* PFN_DCOMPOSITIONCREATEDEVICE3)(IUnknown* renderingDevice, REFIID iid, void** dcompositionDevice);
typedef HRESULT (WINAPI* PFN_DWMPCREATESHAREDDESKTOPVISUAL)(HWND hwndDestination, VOID* pDCompDevice, VOID** ppVisual, PHTHUMBNAIL phThumbnailId);
typedef HRESULT (WINAPI* PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL)(HTHUMBNAIL hThumbnailId, HWND* phwndsInclude, DWORD chwndsInclude, HWND* phwndsExclude, DWORD chwndsExclude, RECT* prcSource, SIZE* pDestinationSize);
typedef HRESULT (WINAPI* PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL)(HTHUMBNAIL hThumbnailId, HWND* phwndsInclude, DWORD chwndsInclude, HWND* phwndsExclude, DWORD chwndsExclude, RECT* prcSource, SIZE* pDestinationSize, DWORD dwFlags);

typedef enum MAG_WINDOWCOMPOSITIONATTRIB
{
  MAG_WCA_EXCLUDED_FROM_LIVEPREVIEW = 0x0D
} MAG_WINDOWCOMPOSITIONATTRIB;

typedef struct MAG_WINDOWCOMPOSITIONATTRIBDATA
{
  MAG_WINDOWCOMPOSITIONATTRIB Attrib;
  PVOID pvData;
  DWORD cbData;
} MAG_WINDOWCOMPOSITIONATTRIBDATA, *LPMAG_WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL (WINAPI* PFN_SETWINDOWCOMPOSITIONATTRIBUTE)(HWND hwnd, LPMAG_WINDOWCOMPOSITIONATTRIBDATA pwcad);

typedef struct MAG_RTL_OSVERSIONINFOW
{
  ULONG dwOSVersionInfoSize;
  ULONG dwMajorVersion;
  ULONG dwMinorVersion;
  ULONG dwBuildNumber;
  ULONG dwPlatformId;
  WCHAR szCSDVersion[128];
} MAG_RTL_OSVERSIONINFOW, *LPMAG_RTL_OSVERSIONINFOW;

typedef LONG (WINAPI* PFN_RTLGETVERSION)(LPMAG_RTL_OSVERSIONINFOW lpVersionInformation);

#define IDirect3DDxgiInterfaceAccess_GetInterface(This, iid, p) ((This)->lpVtbl->GetInterface((This), (iid), (p)))

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax);
void render_wglInit(HWND hWnd);
void render_wglInitPBuffer(HWND hWnd);
void render_wglCreateResources(HWND hWnd);
void render_gdiCreateResources(HWND hWnd);
void render_wglResizeSurface(HWND hWnd);
void render_gdiResizeSurface(HWND hWnd);
void render_wglRender(HWND hWnd);
void render_gdiCaptureScreen(HWND hWnd);
void render_updateSurfaceInfo(HWND hWnd);
BOOL render_gdiCreateCaptureBitmap(HWND hWnd);
void render_gdiDeleteCaptureBitmap(HWND hWnd);
void render_dxgiDeleteResources(HWND hWnd);
void render_dxgiDeleteOutputResources(LPDXGIOUTPUTCAPTURE lpOutput);
LPDXGIOUTPUTCAPTURE render_dxgiFindOutputCapture(LPSHAREDWGLDATA lpsd, HMONITOR hMonitor);
LPDXGIOUTPUTCAPTURE render_dxgiFindFreeOutputCapture(LPSHAREDWGLDATA lpsd);
BOOL render_dxgiCreateDuplicationForMonitor(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput);
BOOL render_dxgiEnsureDuplication(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput);
BOOL render_dxgiEnsureStagingTexture(LPDXGIOUTPUTCAPTURE lpOutput, UINT width, UINT height);
BOOL render_dxgiUpdateFrame(LPDXGIOUTPUTCAPTURE lpOutput);
void render_dxgiCopyMappedPixelsToRect(LPSHAREDWGLDATA lpsd, const BYTE* src, UINT srcWidth, UINT srcHeight, UINT srcPitch, const RECT* lprcDst);
BOOL render_dxgiCaptureIntersection(LPSHAREDWGLDATA lpsd, LPDXGIOUTPUTCAPTURE lpOutput, const RECT* lprcSource, const RECT* lprcIntersection);
void render_dxgiCaptureScreen(HWND hWnd);
void render_wgcCloseObject(IUnknown* object);
HRESULT render_wgcGetActivationFactory(PCWSTR pszRuntimeClass, REFIID riid, void** ppv);
BOOL render_wgcEnsureWinRt(HWND hWnd);
void render_wgcDeleteMonitorResources(LPWGCMONITORCAPTURE lpCapture);
void render_wgcDeleteResources(HWND hWnd);
LPWGCMONITORCAPTURE render_wgcFindMonitorCapture(LPSHAREDWGLDATA lpsd, HMONITOR hMonitor);
LPWGCMONITORCAPTURE render_wgcFindFreeMonitorCapture(LPSHAREDWGLDATA lpsd);
BOOL render_wgcCreateItemForMonitor(HMONITOR hMonitor, WGCITEM** lplpItem);
BOOL render_wgcCreateCaptureForMonitor(HWND hWnd, HMONITOR hMonitor, LPWGCMONITORCAPTURE* lplpCapture);
BOOL render_wgcEnsureCapture(HWND hWnd, HMONITOR hMonitor, LPWGCMONITORCAPTURE* lplpCapture);
BOOL render_wgcEnsureStagingTexture(LPWGCMONITORCAPTURE lpCapture, UINT width, UINT height);
BOOL render_wgcUpdateFrame(LPWGCMONITORCAPTURE lpCapture);
BOOL render_wgcCaptureIntersection(LPSHAREDWGLDATA lpsd, LPWGCMONITORCAPTURE lpCapture, const RECT* lprcSource, const RECT* lprcIntersection);
void render_wgcCaptureScreen(HWND hWnd);
void render_computeSourceRect(HWND hWnd, RECT* lprcSource);
void render_dwmThumbnailDeleteResources(HWND hWnd);
BOOL render_dwmThumbnailEnsureResources(HWND hWnd);
void render_dwmThumbnailCaptureScreen(HWND hWnd);
BOOL render_dwmPrivateIsMultiWindowBuild(void);
LRESULT CALLBACK render_dwmPrivateHostWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL render_dwmPrivateRegisterHostClass(void);
void render_dwmPrivateSetExcludedFromLivePreview(HWND hWnd, BOOL fExcluded);
BOOL render_dwmPrivateUpdateHostWindow(HWND hWnd);
void render_dwmPrivateDeleteResources(HWND hWnd);
BOOL render_dwmPrivateEnsureResources(HWND hWnd);
void render_dwmPrivateCaptureScreen(HWND hWnd);

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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

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
    lpsd->bi.biSizeImage = width * height * CHANNELS;
}

BOOL render_gdiCreateCaptureBitmap(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BITMAPINFO bmi = { 0 };
    VOID* bits = NULL;

    if (!lpsd->hDesktopDC || !lpsd->hCaptureDC)
    {
      return FALSE;
    }

    bmi.bmiHeader = lpsd->bi;
    lpsd->hBitmapBg = CreateDIBSection(lpsd->hDesktopDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

    if (!lpsd->hBitmapBg || !bits)
    {
      if (lpsd->hBitmapBg)
      {
        DeleteBitmap(lpsd->hBitmapBg);
        lpsd->hBitmapBg = NULL;
      }

      lpsd->glScreenData = NULL;
      return FALSE;
    }

    lpsd->hBitmapOld = SelectBitmap(lpsd->hCaptureDC, lpsd->hBitmapBg);
    if (!lpsd->hBitmapOld)
    {
      DeleteBitmap(lpsd->hBitmapBg);
      lpsd->hBitmapBg = NULL;
      lpsd->glScreenData = NULL;
      return FALSE;
    }

    lpsd->glScreenData = (GLubyte*)bits;
    return TRUE;
}

void render_gdiDeleteCaptureBitmap(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

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

    lpsd->glScreenData = NULL;
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      render_dxgiDeleteOutputResources(&lpsd->dxgiOutputs[i]);
    }
}

LPDXGIOUTPUTCAPTURE render_dxgiFindOutputCapture(LPSHAREDWGLDATA lpsd, HMONITOR hMonitor)
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

LPDXGIOUTPUTCAPTURE render_dxgiFindFreeOutputCapture(LPSHAREDWGLDATA lpsd)
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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

    if (lpOutput->dxgiStagingTexture &&
        lpOutput->dxgiStagingWidth == width &&
        lpOutput->dxgiStagingHeight == height)
    {
      return TRUE;
    }

    SAFERELEASE(lpOutput->dxgiStagingTexture);
    lpOutput->dxgiStagingWidth = 0;
    lpOutput->dxgiStagingHeight = 0;

    desc.Width = width;
    desc.Height = height;
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

    lpOutput->dxgiStagingWidth = width;
    lpOutput->dxgiStagingHeight = height;
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

void render_dxgiCopyMappedPixelsToRect(LPSHAREDWGLDATA lpsd, const BYTE* src, UINT srcWidth, UINT srcHeight, UINT srcPitch, const RECT* lprcDst)
{
    const UINT dstWidth = (UINT)RECTWIDTH((*lprcDst));
    const UINT dstHeight = (UINT)RECTHEIGHT((*lprcDst));
    const UINT screenWidth = (UINT)lpsd->bi.biWidth;
    const UINT screenHeight = (UINT)lpsd->bi.biHeight;
    const UINT screenPitch = screenWidth * CHANNELS;
    UINT y;

    if (!dstWidth || !dstHeight || !srcWidth || !srcHeight)
    {
      return;
    }

    for (y = 0; y < dstHeight; ++y)
    {
      const UINT srcY = min((UINT)(((ULONGLONG)y * srcHeight) / dstHeight), srcHeight - 1);
      const UINT dstY = (UINT)lprcDst->top + y;
      BYTE* dstRow = ((BYTE*)lpsd->glScreenData) + ((screenHeight - 1 - dstY) * screenPitch) + ((UINT)lprcDst->left * CHANNELS);
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

BOOL render_dxgiCaptureIntersection(LPSHAREDWGLDATA lpsd, LPDXGIOUTPUTCAPTURE lpOutput, const RECT* lprcSource, const RECT* lprcIntersection)
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

    rcDst.left = MulDiv(lprcIntersection->left - lprcSource->left, lpsd->bi.biWidth, RECTWIDTH((*lprcSource)));
    rcDst.top = MulDiv(lprcIntersection->top - lprcSource->top, lpsd->bi.biHeight, RECTHEIGHT((*lprcSource)));
    rcDst.right = MulDiv(lprcIntersection->right - lprcSource->left, lpsd->bi.biWidth, RECTWIDTH((*lprcSource)));
    rcDst.bottom = MulDiv(lprcIntersection->bottom - lprcSource->top, lpsd->bi.biHeight, RECTHEIGHT((*lprcSource)));

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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->wgcMonitors); ++i)
    {
      render_wgcDeleteMonitorResources(&lpsd->wgcMonitors[i]);
    }
}

LPWGCMONITORCAPTURE render_wgcFindMonitorCapture(LPSHAREDWGLDATA lpsd, HMONITOR hMonitor)
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

LPWGCMONITORCAPTURE render_wgcFindFreeMonitorCapture(LPSHAREDWGLDATA lpsd)
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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
        lpCapture->wgcStagingWidth == width &&
        lpCapture->wgcStagingHeight == height)
    {
      return TRUE;
    }

    SAFERELEASE(lpCapture->wgcStagingTexture);
    lpCapture->wgcStagingWidth = 0;
    lpCapture->wgcStagingHeight = 0;

    desc.Width = width;
    desc.Height = height;
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

    lpCapture->wgcStagingWidth = width;
    lpCapture->wgcStagingHeight = height;
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

BOOL render_wgcCaptureIntersection(LPSHAREDWGLDATA lpsd, LPWGCMONITORCAPTURE lpCapture, const RECT* lprcSource, const RECT* lprcIntersection)
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

    rcDst.left = MulDiv(lprcIntersection->left - lprcSource->left, lpsd->bi.biWidth, RECTWIDTH((*lprcSource)));
    rcDst.top = MulDiv(lprcIntersection->top - lprcSource->top, lpsd->bi.biHeight, RECTHEIGHT((*lprcSource)));
    rcDst.right = MulDiv(lprcIntersection->right - lprcSource->left, lpsd->bi.biWidth, RECTWIDTH((*lprcSource)));
    rcDst.bottom = MulDiv(lprcIntersection->bottom - lprcSource->top, lpsd->bi.biHeight, RECTHEIGHT((*lprcSource)));

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

void render_computeSourceRect(HWND hWnd, RECT* lprcSource)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const LONG cw = lpsd->bi.biWidth;
    const LONG ch = lpsd->bi.biHeight;
    POINT tl = { 0, 0 };
    POINT center = { 0, 0 };
    RECT rcVirtual = lpsd->di.rc;
    const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
    BOOL fCenterOnCursor;
    LONG srcW;
    LONG srcH;
    LONG srcX;
    LONG srcY;

    if (!ClientToScreen(hWnd, &tl))
    {
      SetRectEmpty(lprcSource);
      return;
    }

    fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
    if (!fCenterOnCursor)
    {
      center.x = tl.x + cw / 2;
      center.y = tl.y + ch / 2;
    }

    if (m <= 1.0001f)
    {
      srcW = cw;
      srcH = ch;
      srcX = fCenterOnCursor ? center.x - srcW / 2 : tl.x;
      srcY = fCenterOnCursor ? center.y - srcH / 2 : tl.y;
      lpsd->pt = fCenterOnCursor ? center : tl;
    }
    else
    {
      srcW = (LONG)(cw / m);
      srcH = (LONG)(ch / m);

      if (srcW < 1) srcW = 1;
      if (srcH < 1) srcH = 1;

      srcX = center.x - srcW / 2;
      srcY = center.y - srcH / 2;
      lpsd->pt = center;
    }

    if (IsRectEmpty(&rcVirtual))
    {
      rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
      rcVirtual.right = rcVirtual.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
      rcVirtual.bottom = rcVirtual.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }

    srcX = render_clipSourceOrigin(srcX, srcW, rcVirtual.left, rcVirtual.right);
    srcY = render_clipSourceOrigin(srcY, srcH, rcVirtual.top, rcVirtual.bottom);
    SetRect(lprcSource, srcX, srcY, srcX + srcW, srcY + srcH);
}

void render_dwmThumbnailDeleteResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->dwmThumbnail.hThumbnail)
    {
      DwmUnregisterThumbnail(lpsd->dwmThumbnail.hThumbnail);
      lpsd->dwmThumbnail.hThumbnail = NULL;
    }

    lpsd->dwmThumbnail.hwndSource = NULL;
}

BOOL render_dwmThumbnailEnsureResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
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

void render_dwmThumbnailCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    DWM_THUMBNAIL_PROPERTIES props = { 0 };
    RECT rcSource;
    RECT rcVirtual = lpsd->di.rc;

    if (!render_dwmThumbnailEnsureResources(hWnd))
    {
      render_gdiCaptureScreen(hWnd);
      render_wglRender(hWnd);
      return;
    }

    render_computeSourceRect(hWnd, &rcSource);
    if (IsRectEmpty(&rcSource))
    {
      return;
    }

    if (IsRectEmpty(&rcVirtual))
    {
      rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    }

    OffsetRect(&rcSource, -rcVirtual.left, -rcVirtual.top);

    props.dwFlags = DWM_TNP_RECTDESTINATION | DWM_TNP_RECTSOURCE | DWM_TNP_VISIBLE | DWM_TNP_OPACITY | DWM_TNP_SOURCECLIENTAREAONLY;
    SetRect(&props.rcDestination, 0, 0, lpsd->bi.biWidth, lpsd->bi.biHeight);
    props.rcSource = rcSource;
    props.opacity = 255;
    props.fVisible = TRUE;
    props.fSourceClientAreaOnly = FALSE;

    if (FAILED(DwmUpdateThumbnailProperties(lpsd->dwmThumbnail.hThumbnail, &props)))
    {
      render_dwmThumbnailDeleteResources(hWnd);
      render_gdiCaptureScreen(hWnd);
      render_wglRender(hWnd);
      return;
    }

    DwmFlush();
}

BOOL render_dwmPrivateIsMultiWindowBuild(void)
{
    HMODULE hNtdll = GetModuleHandle(TEXT("ntdll.dll"));
    PFN_RTLGETVERSION pRtlGetVersion = hNtdll ? (PFN_RTLGETVERSION)GetProcAddress(hNtdll, "RtlGetVersion") : NULL;
    MAG_RTL_OSVERSIONINFOW version = { 0 };

    version.dwOSVersionInfoSize = sizeof(version);

    if (pRtlGetVersion && pRtlGetVersion(&version) >= 0)
    {
      return version.dwBuildNumber >= DWM_PRIVATE_MULTIWINDOW_BUILD;
    }

#if defined(_WIN64)
    return TRUE;
#else
    return FALSE;
#endif
}

LRESULT CALLBACK render_dwmPrivateHostWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCHITTEST:
      return HTTRANSPARENT;
    case WM_MOUSEACTIVATE:
      return MA_NOACTIVATE;
    case WM_ERASEBKGND:
      return 1;
    default:
      return DefWindowProc(hwnd, message, wParam, lParam);
    }
}

BOOL render_dwmPrivateRegisterHostClass(void)
{
    static BOOL fRegistered = FALSE;
    WNDCLASS wc = { 0 };

    if (fRegistered)
    {
      return TRUE;
    }

    wc.lpfnWndProc = render_dwmPrivateHostWndProc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.hbrBackground = GetStockBrush(BLACK_BRUSH);
    wc.lpszClassName = TEXT("magDwmPrivateHostClass");

    if (!RegisterClass(&wc) && ERROR_CLASS_ALREADY_EXISTS != GetLastError())
    {
      return FALSE;
    }

    fRegistered = TRUE;
    return TRUE;
}

void render_dwmPrivateSetExcludedFromLivePreview(HWND hWnd, BOOL fExcluded)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDWMPRIVATEVISUALCAPTURE lpCapture;
    PFN_SETWINDOWCOMPOSITIONATTRIBUTE pSetWindowCompositionAttribute;
    MAG_WINDOWCOMPOSITIONATTRIBDATA data = { 0 };
    BOOL fEnable = fExcluded;

    if (!lpsd || !lpsd->dwmPrivate.pSetWindowCompositionAttribute)
    {
      return;
    }

    lpCapture = &lpsd->dwmPrivate;
    pSetWindowCompositionAttribute = (PFN_SETWINDOWCOMPOSITIONATTRIBUTE)lpsd->dwmPrivate.pSetWindowCompositionAttribute;
    data.Attrib = MAG_WCA_EXCLUDED_FROM_LIVEPREVIEW;
    data.pvData = &fEnable;
    data.cbData = sizeof(fEnable);
    pSetWindowCompositionAttribute(lpCapture->hwndHost ? lpCapture->hwndHost : hWnd, &data);
}

BOOL render_dwmPrivateUpdateHostWindow(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    POINT pt = { 0, 0 };
    RECT rc;

    if (!lpsd->dwmPrivate.hwndHost ||
        !GetClientRect(hWnd, &rc) ||
        !ClientToScreen(hWnd, &pt))
    {
      return FALSE;
    }

    return SetWindowPos(
      lpsd->dwmPrivate.hwndHost,
      HWND_TOPMOST,
      pt.x,
      pt.y,
      RECTWIDTH(rc),
      RECTHEIGHT(rc),
      SWP_NOACTIVATE | SWP_SHOWWINDOW);
}

void render_dwmPrivateDeleteResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDWMPRIVATEVISUALCAPTURE lpCapture = &lpsd->dwmPrivate;

    if (lpCapture->dcompTarget)
    {
      ((MAG_IDCompositionTarget*)lpCapture->dcompTarget)->lpVtbl->SetRoot((MAG_IDCompositionTarget*)lpCapture->dcompTarget, NULL);
    }

    if (lpCapture->dcompDevice)
    {
      ((MAG_IDCompositionDevice*)lpCapture->dcompDevice)->lpVtbl->Commit((MAG_IDCompositionDevice*)lpCapture->dcompDevice);
    }

    if (lpCapture->pSetWindowCompositionAttribute)
    {
      render_dwmPrivateSetExcludedFromLivePreview(hWnd, FALSE);
    }

    if (lpCapture->hThumbnail)
    {
      DwmUnregisterThumbnail(lpCapture->hThumbnail);
      lpCapture->hThumbnail = NULL;
    }

    SAFERELEASE(lpCapture->dcompVisual);
    SAFERELEASE(lpCapture->dcompTarget);
    SAFERELEASE(lpCapture->dcompDevice);
    SAFERELEASE(lpCapture->dxgiDevice);
    SAFERELEASE(lpCapture->d3dDevice);

    if (lpCapture->hwndHost)
    {
      DestroyWindow(lpCapture->hwndHost);
    }

    if (lpCapture->hUser32)
    {
      FreeLibrary(lpCapture->hUser32);
    }

    if (lpCapture->hDwmApi)
    {
      FreeLibrary(lpCapture->hDwmApi);
    }

    if (lpCapture->hDComp)
    {
      FreeLibrary(lpCapture->hDComp);
    }

    ZeroMemory(lpCapture, sizeof(*lpCapture));
}

BOOL render_dwmPrivateEnsureResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDWMPRIVATEVISUALCAPTURE lpCapture = &lpsd->dwmPrivate;
    PFN_DCOMPOSITIONCREATEDEVICE3 pDCompositionCreateDevice3;
    PFN_DWMPCREATESHAREDDESKTOPVISUAL pCreateSharedVisual;
    D3D_FEATURE_LEVEL featureLevel;
    HRESULT hr;

    if (lpCapture->fInitialized &&
        lpCapture->dcompDevice &&
        lpCapture->dcompTarget &&
        lpCapture->dcompVisual &&
        lpCapture->hThumbnail)
    {
      return TRUE;
    }

    render_dwmPrivateDeleteResources(hWnd);

    lpCapture->hDComp = LoadLibrary(TEXT("dcomp.dll"));
    lpCapture->hDwmApi = LoadLibrary(TEXT("dwmapi.dll"));
    lpCapture->hUser32 = LoadLibrary(TEXT("user32.dll"));

    if (!lpCapture->hDComp || !lpCapture->hDwmApi)
    {
      render_dwmPrivateDeleteResources(hWnd);
      return FALSE;
    }

    lpCapture->pDCompositionCreateDevice = GetProcAddress(lpCapture->hDComp, "DCompositionCreateDevice");
    lpCapture->pDCompositionCreateDevice3 = GetProcAddress(lpCapture->hDComp, "DCompositionCreateDevice3");
    lpCapture->pCreateSharedVisual = GetProcAddress(lpCapture->hDwmApi, MAKEINTRESOURCEA(DWM_ORD_CREATE_SHARED_DESKTOP_VISUAL));
    lpCapture->pUpdateSharedVisual = GetProcAddress(lpCapture->hDwmApi, MAKEINTRESOURCEA(DWM_ORD_UPDATE_SHARED_DESKTOP_VISUAL));

    if (lpCapture->hUser32)
    {
      lpCapture->pSetWindowCompositionAttribute = GetProcAddress(lpCapture->hUser32, "SetWindowCompositionAttribute");
    }

    if (!lpCapture->pDCompositionCreateDevice3 ||
        !lpCapture->pCreateSharedVisual ||
        !lpCapture->pUpdateSharedVisual)
    {
      render_dwmPrivateDeleteResources(hWnd);
      return FALSE;
    }

    if (!render_dwmPrivateRegisterHostClass())
    {
      render_dwmPrivateDeleteResources(hWnd);
      return FALSE;
    }

    lpCapture->hwndHost = CreateWindowEx(
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_NOREDIRECTIONBITMAP,
      TEXT("magDwmPrivateHostClass"),
      TEXT("magDwmPrivateHost"),
      WS_POPUP,
      0,
      0,
      1,
      1,
      hWnd,
      NULL,
      GetModuleHandle(NULL),
      NULL);

    if (!lpCapture->hwndHost ||
        !render_dwmPrivateUpdateHostWindow(hWnd))
    {
      render_dwmPrivateDeleteResources(hWnd);
      return FALSE;
    }

    render_dwmPrivateSetExcludedFromLivePreview(hWnd, TRUE);

    hr = D3D11CreateDevice(
      NULL,
      D3D_DRIVER_TYPE_HARDWARE,
      NULL,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      NULL,
      0,
      D3D11_SDK_VERSION,
      &lpCapture->d3dDevice,
      &featureLevel,
      NULL);

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
        &lpCapture->d3dDevice,
        &featureLevel,
        NULL);
    }

    if (SUCCEEDED(hr))
    {
      hr = ID3D11Device_QueryInterface(lpCapture->d3dDevice, &IID_IDXGIDevice, (void**)&lpCapture->dxgiDevice);
    }

    pDCompositionCreateDevice3 = (PFN_DCOMPOSITIONCREATEDEVICE3)lpCapture->pDCompositionCreateDevice3;
    if (SUCCEEDED(hr))
    {
      hr = pDCompositionCreateDevice3((IUnknown*)lpCapture->dxgiDevice, &IID_MAG_IDCompositionDesktopDevice, (void**)&lpCapture->dcompDevice);
      if (SUCCEEDED(hr))
      {
        lpCapture->fDesktopDevice = TRUE;
      }
    }

    pCreateSharedVisual = (PFN_DWMPCREATESHAREDDESKTOPVISUAL)lpCapture->pCreateSharedVisual;
    if (SUCCEEDED(hr))
    {
      hr = pCreateSharedVisual(lpCapture->hwndHost, lpCapture->dcompDevice, (void**)&lpCapture->dcompVisual, &lpCapture->hThumbnail);
    }

    if (SUCCEEDED(hr))
    {
      hr = ((MAG_IDCompositionDesktopDevice*)lpCapture->dcompDevice)->lpVtbl->CreateTargetForHwnd(
        (MAG_IDCompositionDesktopDevice*)lpCapture->dcompDevice,
        lpCapture->hwndHost,
        FALSE,
        (MAG_IDCompositionTarget**)&lpCapture->dcompTarget);
    }

    if (SUCCEEDED(hr))
    {
      hr = ((MAG_IDCompositionTarget*)lpCapture->dcompTarget)->lpVtbl->SetRoot(
        (MAG_IDCompositionTarget*)lpCapture->dcompTarget,
        lpCapture->dcompVisual);
    }

    if (SUCCEEDED(hr))
    {
      hr = ((MAG_IDCompositionDevice*)lpCapture->dcompDevice)->lpVtbl->Commit((MAG_IDCompositionDevice*)lpCapture->dcompDevice);
    }

    if (FAILED(hr))
    {
      render_dwmPrivateDeleteResources(hWnd);
      return FALSE;
    }

    lpCapture->fUseMultiWindow = render_dwmPrivateIsMultiWindowBuild();
    lpCapture->fInitialized = TRUE;
    return TRUE;
}

void render_dwmPrivateCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDWMPRIVATEVISUALCAPTURE lpCapture = &lpsd->dwmPrivate;
    RECT rcSource;
    SIZE destinationSize;
    HWND exclude[2];
    DWORD excludeCount = 0;
    HRESULT hr;

    if (!render_dwmPrivateEnsureResources(hWnd))
    {
      render_gdiCaptureScreen(hWnd);
      render_wglRender(hWnd);
      return;
    }

    if (!render_dwmPrivateUpdateHostWindow(hWnd))
    {
      render_dwmPrivateDeleteResources(hWnd);
      render_gdiCaptureScreen(hWnd);
      render_wglRender(hWnd);
      return;
    }

    render_computeSourceRect(hWnd, &rcSource);
    if (IsRectEmpty(&rcSource))
    {
      return;
    }

    destinationSize.cx = lpsd->bi.biWidth;
    destinationSize.cy = lpsd->bi.biHeight;
    exclude[excludeCount++] = hWnd;
    exclude[excludeCount++] = lpCapture->hwndHost;

    if (lpCapture->fUseMultiWindow)
    {
      PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL pUpdate =
        (PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL)lpCapture->pUpdateSharedVisual;

      hr = pUpdate(lpCapture->hThumbnail, NULL, 0, exclude, excludeCount, &rcSource, &destinationSize, 1);
    }
    else
    {
      PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL pUpdate =
        (PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL)lpCapture->pUpdateSharedVisual;

      hr = pUpdate(lpCapture->hThumbnail, NULL, 0, exclude, excludeCount, &rcSource, &destinationSize);
    }

    if (FAILED(hr))
    {
      render_dwmPrivateDeleteResources(hWnd);
      render_gdiCaptureScreen(hWnd);
      render_wglRender(hWnd);
      return;
    }

    ((MAG_IDCompositionDevice*)lpCapture->dcompDevice)->lpVtbl->Commit((MAG_IDCompositionDevice*)lpCapture->dcompDevice);
    DwmFlush();
}

void render_wglInit(HWND hWnd)
{
    const PIXELFORMATDESCRIPTOR pfd =
    {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,
      PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      PFD_TYPE_RGBA,
      32,
      0, 0, 0, 0, 0, 0,
      8,
      0,
      0,
      0, 0, 0, 0,
      24,
      8,
      0,
      PFD_MAIN_PLANE,
      0,
      0, 0, 0
    };

    const int iAttribs[] =
    {
      WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
      WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
      WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
      WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
      WGL_COLOR_BITS_ARB, 32,
      WGL_DEPTH_BITS_ARB, 24,
      WGL_ALPHA_BITS_ARB, 8,
      WGL_SWAP_METHOD_ARB,WGL_SWAP_COPY_ARB,
      WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
      0
    };

    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    wglInit();
    lpsd->hDC = GetDC(hWnd);
    SetPixelFormat(lpsd->hDC, wglFindPixelFormat(lpsd->hDC, iAttribs, NULL), &pfd);
    lpsd->hRC = wglCreateContextAttribsARB(lpsd->hDC, NULL, NULL);
    wglMakeCurrent(lpsd->hDC, lpsd->hRC);

    render_updateSurfaceInfo(hWnd);
    render_gdiCreateResources(hWnd);
    render_wglCreateResources(hWnd);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    wglSwapIntervalEXT(0);
}

void render_wglInitPBuffer(HWND hWnd)
{
    // basic pixel format
    const PIXELFORMATDESCRIPTOR pfd =
    {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,
      PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      PFD_TYPE_RGBA,
      32,
      0, 0, 0, 0, 0, 0,
      8,
      0,
      0,
      0, 0, 0, 0,
      24,
      8,
      0,
      PFD_MAIN_PLANE,
      0,
      0, 0, 0
    };

    const int iAttribs[] =
    {
      WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
      WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
      WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
      WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
      WGL_COLOR_BITS_ARB, 32,
      WGL_DEPTH_BITS_ARB, 24,
      WGL_ALPHA_BITS_ARB, 8,
      WGL_SWAP_METHOD_ARB,WGL_SWAP_COPY_ARB,
      WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
      0
    };

    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    wglInit();
    lpsd->hDC = GetDC(0);

    int pf = ChoosePixelFormat(lpsd->hDC, &pfd);
    WGLCHECK(SetPixelFormat(lpsd->hDC, pf, &pfd));

    // dummy GL context
    HGLRC rc = wglCreateContext(lpsd->hDC);
    WGLCHECK(wglMakeCurrent(lpsd->hDC, rc));

    // choose pbuffer format
    int iattribs[] = {
        WGL_DRAW_TO_PBUFFER_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,  GL_TRUE,
        WGL_BIND_TO_TEXTURE_RGBA_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        0
    };

    int formats[1];
    UINT count;
    if (!wglChoosePixelFormatARB(lpsd->hDC, iattribs, NULL, 1, formats, &count))
      __debugbreak();

    // create pbuffer
    int pattribs[] = {
        WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGBA_ARB,
        WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_2D_ARB,
        0
    };

    lpsd->pb = wglCreatePbufferARB(lpsd->hDC, formats[0], 256, 256, pattribs);
    HDC pbdc = wglGetPbufferDCARB(lpsd->pb);

    // render context for pbuffer
    HGLRC pbrc = wglCreateContext(pbdc);
    wglMakeCurrent(pbdc, pbrc);

    render_updateSurfaceInfo(hWnd);
    render_gdiCreateResources(hWnd);
    render_wglCreateResources(hWnd);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    wglSwapIntervalEXT(0);
}

void render_wglCreateResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    glGenTextures(1, &lpsd->glScreenTexture);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, lpsd->bi.biWidth, lpsd->bi.biHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    lpsd->fScale = 1.0f;
    lpsd->fTexScaler = 1.0f;
}

void render_gdiCreateResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    gdiGetDisplayInfo(&lpsd->di);
    lpsd->hDesktopDC = GetDC(NULL); // whole virtual screen (handles negative/secondary monitor coords)
    lpsd->hCaptureDC = CreateCompatibleDC(lpsd->hDesktopDC);
    render_updateSurfaceInfo(hWnd);
    render_gdiCreateCaptureBitmap(hWnd);
}

void render_wglResizeSurface(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd->hRC || !lpsd->glScreenTexture)
    {
      return;
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &lpsd->glScreenTexture);

    glGenTextures(1, &lpsd->glScreenTexture);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, lpsd->bi.biWidth, lpsd->bi.biHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
}

void render_gdiResizeSurface(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    render_updateSurfaceInfo(hWnd);

    if (!lpsd->hCaptureDC)
    {
      return;
    }

    render_gdiDeleteCaptureBitmap(hWnd);
    render_gdiCreateCaptureBitmap(hWnd);
}

void render_wglRender(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd->glScreenData || !lpsd->glScreenTexture)
    {
      return;
    }

    //glClearColor(
    //  lpsd->cfClearColor[0],
    //  lpsd->cfClearColor[1],
    //  lpsd->cfClearColor[2],
    //  lpsd->cfClearColor[3]);
    //glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, lpsd->bi.biWidth, lpsd->bi.biHeight);
    glDisable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lpsd->bi.biWidth, lpsd->bi.biHeight, GL_BGRA, GL_UNSIGNED_BYTE, lpsd->glScreenData);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    //glFlush();
    SwapBuffers(lpsd->hDC);
    glFinish();
}

void render_gdiCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hBitmapBg && lpsd->glScreenData)
    {
      const LONG cw = lpsd->bi.biWidth;
      const LONG ch = lpsd->bi.biHeight;
      POINT tl = { 0, 0 };
      POINT center = { 0, 0 };
      RECT rcSource = lpsd->di.rc;
      const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
      BOOL fCenterOnCursor;

      if (!ClientToScreen(hWnd, &tl))
      {
        return;
      }

      fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
      if (!fCenterOnCursor)
      {
        center.x = tl.x + cw / 2;
        center.y = tl.y + ch / 2;
      }

      if (IsRectEmpty(&rcSource))
      {
        rcSource.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        rcSource.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        rcSource.right = rcSource.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        rcSource.bottom = rcSource.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
      }

      PatBlt(lpsd->hCaptureDC, 0, 0, cw, ch, BLACKNESS);

      if (m <= 1.0001f)
      {
        LONG srcX = tl.x;
        LONG srcY = tl.y;

        if (fCenterOnCursor)
        {
          srcX = center.x - cw / 2;
          srcY = center.y - ch / 2;
        }

        if (fCenterOnCursor)
        {
          srcX = render_clipSourceOrigin(srcX, cw, rcSource.left, rcSource.right);
          srcY = render_clipSourceOrigin(srcY, ch, rcSource.top, rcSource.bottom);
        }

        lpsd->pt = fCenterOnCursor ? center : tl;
        BitBlt(
          lpsd->hCaptureDC,
          0,
          0,
          cw,
          ch,
          lpsd->hDesktopDC,
          srcX,
          srcY,
          SRCCOPY | CAPTUREBLT);
      }
      else
      {
        LONG srcW = (LONG)(cw / m);
        LONG srcH = (LONG)(ch / m);
        LONG srcX;
        LONG srcY;

        if (srcW < 1) srcW = 1;
        if (srcH < 1) srcH = 1;

        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
        if (fCenterOnCursor)
        {
          srcX = render_clipSourceOrigin(srcX, srcW, rcSource.left, rcSource.right);
          srcY = render_clipSourceOrigin(srcY, srcH, rcSource.top, rcSource.bottom);
        }
        lpsd->pt = center;

        SetStretchBltMode(lpsd->hCaptureDC, COLORONCOLOR);
        StretchBlt(
          lpsd->hCaptureDC,
          0,
          0,
          cw,
          ch,
          lpsd->hDesktopDC,
          srcX,
          srcY,
          srcW,
          srcH,
          SRCCOPY | CAPTUREBLT);
      }

      GdiFlush();
    }
}

void render_dxgiCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->glScreenData)
    {
      const LONG cw = lpsd->bi.biWidth;
      const LONG ch = lpsd->bi.biHeight;
      POINT tl = { 0, 0 };
      POINT center = { 0, 0 };
      RECT rcVirtual = lpsd->di.rc;
      RECT rcSource;
      const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
      BOOL fCenterOnCursor;
      LONG srcW;
      LONG srcH;
      LONG srcX;
      LONG srcY;
      UINT i;
      BOOL fCapturedAny = FALSE;

      if (!ClientToScreen(hWnd, &tl))
      {
        return;
      }

      fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
      if (!fCenterOnCursor)
      {
        center.x = tl.x + cw / 2;
        center.y = tl.y + ch / 2;
      }

      if (m <= 1.0001f)
      {
        srcW = cw;
        srcH = ch;
        srcX = fCenterOnCursor ? center.x - srcW / 2 : tl.x;
        srcY = fCenterOnCursor ? center.y - srcH / 2 : tl.y;
        lpsd->pt = fCenterOnCursor ? center : tl;
      }
      else
      {
        srcW = (LONG)(cw / m);
        srcH = (LONG)(ch / m);

        if (srcW < 1) srcW = 1;
        if (srcH < 1) srcH = 1;

        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
        lpsd->pt = center;
      }

      if (IsRectEmpty(&rcVirtual))
      {
        rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        rcVirtual.right = rcVirtual.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        rcVirtual.bottom = rcVirtual.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
      }

      srcX = render_clipSourceOrigin(srcX, srcW, rcVirtual.left, rcVirtual.right);
      srcY = render_clipSourceOrigin(srcY, srcH, rcVirtual.top, rcVirtual.bottom);
      SetRect(&rcSource, srcX, srcY, srcX + srcW, srcY + srcH);

      ZeroMemory(lpsd->glScreenData, lpsd->bi.biSizeImage);

      for (i = 0; i < lpsd->di.numMonitors; ++i)
      {
        RECT rcMonitor = lpsd->di.monitors[i].monitorInfoEx.rcMonitor;
        RECT rcIntersection;
        POINT ptIntersection;
        HMONITOR hMonitor;
        LPDXGIOUTPUTCAPTURE lpOutput = NULL;

        if (!IntersectRect(&rcIntersection, &rcSource, &rcMonitor))
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->glScreenData)
    {
      const LONG cw = lpsd->bi.biWidth;
      const LONG ch = lpsd->bi.biHeight;
      POINT tl = { 0, 0 };
      POINT center = { 0, 0 };
      RECT rcVirtual = lpsd->di.rc;
      RECT rcSource;
      const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
      BOOL fCenterOnCursor;
      LONG srcW;
      LONG srcH;
      LONG srcX;
      LONG srcY;
      UINT i;
      BOOL fCapturedAny = FALSE;

      if (!ClientToScreen(hWnd, &tl))
      {
        return;
      }

      fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
      if (!fCenterOnCursor)
      {
        center.x = tl.x + cw / 2;
        center.y = tl.y + ch / 2;
      }

      if (m <= 1.0001f)
      {
        srcW = cw;
        srcH = ch;
        srcX = fCenterOnCursor ? center.x - srcW / 2 : tl.x;
        srcY = fCenterOnCursor ? center.y - srcH / 2 : tl.y;
        lpsd->pt = fCenterOnCursor ? center : tl;
      }
      else
      {
        srcW = (LONG)(cw / m);
        srcH = (LONG)(ch / m);

        if (srcW < 1) srcW = 1;
        if (srcH < 1) srcH = 1;

        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
        lpsd->pt = center;
      }

      if (IsRectEmpty(&rcVirtual))
      {
        rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        rcVirtual.right = rcVirtual.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        rcVirtual.bottom = rcVirtual.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
      }

      srcX = render_clipSourceOrigin(srcX, srcW, rcVirtual.left, rcVirtual.right);
      srcY = render_clipSourceOrigin(srcY, srcH, rcVirtual.top, rcVirtual.bottom);
      SetRect(&rcSource, srcX, srcY, srcX + srcW, srcY + srcH);

      ZeroMemory(lpsd->glScreenData, lpsd->bi.biSizeImage);

      for (i = 0; i < lpsd->di.numMonitors; ++i)
      {
        RECT rcMonitor = lpsd->di.monitors[i].monitorInfoEx.rcMonitor;
        RECT rcIntersection;
        POINT ptIntersection;
        HMONITOR hMonitor;
        LPWGCMONITORCAPTURE lpCapture = NULL;

        if (!IntersectRect(&rcIntersection, &rcSource, &rcMonitor))
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

void renderInit(HWND hWnd)
{
    render_wglInit(hWnd);
    //render_wglInitPBuffer(hWnd);
}

void renderCleanup(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    render_dwmPrivateDeleteResources(hWnd);
    render_dwmThumbnailDeleteResources(hWnd);
    render_wgcDeleteResources(hWnd);
    render_dxgiDeleteResources(hWnd);

    if (lpsd->fWinRtInitialized)
    {
      RoUninitialize();
      lpsd->fWinRtInitialized = FALSE;
    }
}

void renderResizeCapture(HWND hWnd)
{
    render_gdiResizeSurface(hWnd);
    render_wglResizeSurface(hWnd);
}

void renderRender(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (lpsd->captureApi)
    {
    case CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE:
      render_dxgiDeleteResources(hWnd);
      render_dwmThumbnailDeleteResources(hWnd);
      render_dwmPrivateDeleteResources(hWnd);
      render_wgcCaptureScreen(hWnd);
      break;
    case CAPTURE_API_DXGI_DESKTOP_DUPLICATION:
      render_wgcDeleteResources(hWnd);
      render_dwmThumbnailDeleteResources(hWnd);
      render_dwmPrivateDeleteResources(hWnd);
      render_dxgiCaptureScreen(hWnd);
      break;
    case CAPTURE_API_DWM_THUMBNAIL:
      render_wgcDeleteResources(hWnd);
      render_dxgiDeleteResources(hWnd);
      render_dwmPrivateDeleteResources(hWnd);
      render_dwmThumbnailCaptureScreen(hWnd);
      return;
    case CAPTURE_API_DWM_PRIVATE_VISUAL:
      render_wgcDeleteResources(hWnd);
      render_dxgiDeleteResources(hWnd);
      render_dwmThumbnailDeleteResources(hWnd);
      render_dwmPrivateCaptureScreen(hWnd);
      return;
    case CAPTURE_API_GDI_BITBLT:
    default:
      render_wgcDeleteResources(hWnd);
      render_dxgiDeleteResources(hWnd);
      render_dwmThumbnailDeleteResources(hWnd);
      render_dwmPrivateDeleteResources(hWnd);
      render_gdiCaptureScreen(hWnd);
      break;
    }

    render_wglRender(hWnd);
}
