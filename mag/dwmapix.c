#include "dwmapix.h"
#include "cdcomp.h"
#include "render.h"
//#include "cd2d.h"

#include <Unknwn.h>
#include <ntstatus.h>
#include <winternl.h>
#include <dwmapi.h>
#include <dxgi1_3.h>
#include <d3d11_2.h>
#include <d2d1_2.h>
#include <d2d1_2helper.h>
#include <assert.h>

// Either use this or add in your linker configuration
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dcomp")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "ole32")
#pragma comment(lib, "dxguid")

//-------------------- Definitions
typedef enum THUMBNAIL_TYPE
{
    TT_DEFAULT = 0x0,
    TT_SNAPSHOT = 0x1,
    TT_ICONIC = 0x2,
    TT_BITMAPPENDING = 0x3,
    TT_BITMAP = 0x4
} THUMBNAIL_TYPE;

typedef HRESULT(WINAPI* DwmpCreateSharedThumbnailVisual)(
    IN HWND hwndDestination,
    IN HWND hwndSource,
    IN DWORD dwThumbnailFlags,
    IN DWM_THUMBNAIL_PROPERTIES* pThumbnailProperties,
    IN VOID* pDCompDevice,
    OUT VOID** ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

typedef HRESULT(WINAPI* DwmpQueryWindowThumbnailSourceSize)(
    IN HWND hwndSource,
    IN BOOL fSourceClientAreaOnly,
    OUT SIZE* pSize);

typedef HRESULT(WINAPI* DwmpQueryThumbnailType)(
    IN HTHUMBNAIL hThumbnailId,
    OUT THUMBNAIL_TYPE* thumbType);

//pre-cobalt/pre-iron
typedef HRESULT(WINAPI* DwmpCreateSharedVirtualDesktopVisual)(
    IN HWND hwndDestination,
    IN VOID* pDCompDevice,
    OUT VOID** ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

//cobalt/iron (20xxx+)
//No changes except for the function name.
typedef HRESULT(WINAPI* DwmpCreateSharedMultiWindowVisual)(
    IN HWND hwndDestination,
    IN VOID* pDCompDevice,
    OUT VOID** ppVisual,
    OUT PHTHUMBNAIL phThumbnailId);

//pre-cobalt/pre-iron
typedef HRESULT(WINAPI* DwmpUpdateSharedVirtualDesktopVisual)(
    IN HTHUMBNAIL hThumbnailId,
    IN HWND* phwndsInclude,
    IN DWORD chwndsInclude,
    IN HWND* phwndsExclude,
    IN DWORD chwndsExclude,
    OUT RECT* prcSource,
    OUT SIZE* pDestinationSize);

//cobalt/iron (20xxx+)
//Change: function name + new DWORD parameter.
//Pass "1" in dwFlags. Feel free to explore other flags.
typedef HRESULT(WINAPI* DwmpUpdateSharedMultiWindowVisual)(
    IN HTHUMBNAIL hThumbnailId,
    IN HWND* phwndsInclude,
    IN DWORD chwndsInclude,
    IN HWND* phwndsExclude,
    IN DWORD chwndsExclude,
    OUT RECT* prcSource,
    OUT SIZE* pDestinationSize,
    IN DWORD dwFlags);

#define DWM_TNP_FREEZE            0x100000
#define DWM_TNP_ENABLE3D          0x4000000
#define DWM_TNP_DISABLE3D         0x8000000
#define DWM_TNP_FORCECVI          0x40000000
#define DWM_TNP_DISABLEFORCECVI   0x80000000
typedef enum WINDOWCOMPOSITIONATTRIB
{
    WCA_UNDEFINED = 0x0,
    WCA_NCRENDERING_ENABLED = 0x1,
    WCA_NCRENDERING_POLICY = 0x2,
    WCA_TRANSITIONS_FORCEDISABLED = 0x3,
    WCA_ALLOW_NCPAINT = 0x4,
    WCA_CAPTION_BUTTON_BOUNDS = 0x5,
    WCA_NONCLIENT_RTL_LAYOUT = 0x6,
    WCA_FORCE_ICONIC_REPRESENTATION = 0x7,
    WCA_EXTENDED_FRAME_BOUNDS = 0x8,
    WCA_HAS_ICONIC_BITMAP = 0x9,
    WCA_THEME_ATTRIBUTES = 0xA,
    WCA_NCRENDERING_EXILED = 0xB,
    WCA_NCADORNMENTINFO = 0xC,
    WCA_EXCLUDED_FROM_LIVEPREVIEW = 0xD,
    WCA_VIDEO_OVERLAY_ACTIVE = 0xE,
    WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 0xF,
    WCA_DISALLOW_PEEK = 0x10,
    WCA_CLOAK = 0x11,
    WCA_CLOAKED = 0x12,
    WCA_ACCENT_POLICY = 0x13,
    WCA_FREEZE_REPRESENTATION = 0x14,
    WCA_EVER_UNCLOAKED = 0x15,
    WCA_VISUAL_OWNER = 0x16,
    WCA_HOLOGRAPHIC = 0x17,
    WCA_EXCLUDED_FROM_DDA = 0x18,
    WCA_PASSIVEUPDATEMODE = 0x19,
    WCA_LAST = 0x1A,
}WINDOWCOMPOSITIONATTRIB;
 
typedef struct WINDOWCOMPOSITIONATTRIBDATA
{
    WINDOWCOMPOSITIONATTRIB Attrib;
    void* pvData;
    DWORD cbData;
}WINDOWCOMPOSITIONATTRIBDATA;
 
typedef BOOL(WINAPI* SetWindowCompositionAttribute)(
    IN HWND hwnd,
    IN WINDOWCOMPOSITIONATTRIBDATA* pwcad);
//------------------------- Getting functions
DwmpQueryThumbnailType lDwmpQueryThumbnailType;
DwmpCreateSharedThumbnailVisual lDwmpCreateSharedThumbnailVisual;
DwmpQueryWindowThumbnailSourceSize lDwmpQueryWindowThumbnailSourceSize;

//PRE-IRON
DwmpCreateSharedVirtualDesktopVisual lDwmpCreateSharedVirtualDesktopVisual;
DwmpUpdateSharedVirtualDesktopVisual lDwmpUpdateSharedVirtualDesktopVisual;

//20xxx+
DwmpCreateSharedMultiWindowVisual lDwmpCreateSharedMultiWindowVisual;
DwmpUpdateSharedMultiWindowVisual lDwmpUpdateSharedMultiWindowVisual;

SetWindowCompositionAttribute lSetWindowCompositionAttribute;

typedef struct ID2D1Factory2 { struct { void* tbl[]; }*v; } ID2D1Factory2;
ID3D11Device*  pD3D11Device;
IDXGIDevice2*  pDXGIDevice2;
ID2D1Factory2* pD2D1Factory;
IDCompositionDesktopDevice* pDCompositionDesktopDevice;
IDCompositionVisual2* pVirtualDesktopVisual;
IDCompositionTarget* pDCompositionTarget;
HTHUMBNAIL hThumbVirtualDesktop;

EXTERN_GUID(IID_IDCompositionDesktopDevice,
  0x5F4633FE, 0x1E08, 0x4CB8, 0x8C, 0x75, 0xCE, 0x24, 0x33, 0x3F, 0x56, 0x02);
BOOL DwmInitPrivate(void)
{
    HMODULE __dwmapi;

    if (!(__dwmapi = LoadLibrary(TEXT("dwmapi.dll"))))
    {
      return FALSE;
    }

    lDwmpQueryThumbnailType = (DwmpQueryThumbnailType)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(114));
    lDwmpCreateSharedThumbnailVisual = (DwmpCreateSharedThumbnailVisual)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(147));
    lDwmpQueryWindowThumbnailSourceSize = (DwmpQueryWindowThumbnailSourceSize)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(162));

    //PRE-IRON
    lDwmpCreateSharedVirtualDesktopVisual = (DwmpCreateSharedVirtualDesktopVisual)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(163));
    lDwmpUpdateSharedVirtualDesktopVisual = (DwmpUpdateSharedVirtualDesktopVisual)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(164));

    //20xxx+
    lDwmpCreateSharedMultiWindowVisual = (DwmpCreateSharedMultiWindowVisual)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(163));
    lDwmpUpdateSharedMultiWindowVisual = (DwmpUpdateSharedMultiWindowVisual)GetProcAddress(__dwmapi, MAKEINTRESOURCEA(164));

    HMODULE __user32;
    
    if (!(__user32 = LoadLibrary(L"user32.dll")))
      return FALSE;

    lSetWindowCompositionAttribute = (SetWindowCompositionAttribute)GetProcAddress(__user32, "SetWindowCompositionAttribute");

    return TRUE;
}

BOOL DwmCreateDevice(void)
{
    HRESULT hr;
    D2D1_FACTORY_OPTIONS options;

    hr = D3D11CreateDevice(
        0,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,
        NULL,
        0,
        D3D11_SDK_VERSION,
        &pD3D11Device,
        NULL,
        NULL);
    assert(SUCCEEDED(hr));

    hr = ID3D11Device_QueryInterface(pD3D11Device, &IID_ID3D11Device, &pDXGIDevice2);
    assert(SUCCEEDED(hr));

    options.debugLevel = D2D1_DEBUG_LEVEL_NONE;
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, &options, &pD2D1Factory);
    assert(SUCCEEDED(hr));

    hr = DCompositionCreateDevice3(pDXGIDevice2, &IID_IDCompositionDesktopDevice, &pDCompositionDesktopDevice);
    assert(SUCCEEDED(hr));

    return S_OK == hr;
}

BOOL DwmCreateThumbnail(HWND hWnd)
{
    HRESULT hr;

    BOOL enable = TRUE;
    WINDOWCOMPOSITIONATTRIBDATA wData = { 0 };
    wData.Attrib = WCA_EXCLUDED_FROM_LIVEPREVIEW;
    wData.pvData = &enable;
    wData.cbData = sizeof(BOOL);
    lSetWindowCompositionAttribute(hWnd, &wData);

    hr = lDwmpCreateSharedMultiWindowVisual(hWnd, pDCompositionDesktopDevice,
      (void**)&pVirtualDesktopVisual, &hThumbVirtualDesktop);
    assert(SUCCEEDED(hr));

    DwmUpdateVisual(hWnd);
}

BOOL DwmUpdateVisual(HWND hWnd)
{
    RECT rc;
    POINT pt = { 0, 0 };
    GetWindowRect(hWnd, &rc);
    GetCursorPos(&pt);
    HRESULT hr;
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    gdiGetDisplayInfo(&lpsd->di);
    RECT monitorSize = { lpsd->di.rc.left, lpsd->di.rc.top, lpsd->di.rc.right, lpsd->di.rc.bottom };
    //RECT monitorSize = { pt.x - 128, pt.y - 128, pt.x + 128, pt.y + 128 };
    SIZE targetSize = { RECTWIDTH(rc), RECTHEIGHT(rc) };
    //SIZE targetSize = { 256, 256 };
    HWND hwndTest = (HWND)0x1; //Exclude from the list what you want to exclude
    //HWND* includeArray = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HWND));
    HWND* excludeArray = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(HWND));
    excludeArray[0] = hWnd;

    hr = lDwmpUpdateSharedMultiWindowVisual(hThumbVirtualDesktop, NULL, 0,
      //excludeArray, 1,
      NULL, 0,
      &monitorSize, &targetSize, 1);
    assert(SUCCEEDED(hr));

    return S_OK == hr;
}

BOOL DwmBindVisual(HWND hWnd)
{
    HRESULT hr;

    hr = IDCompositionDesktopDevice_CreateTargetForHwnd(pDCompositionDesktopDevice, hWnd, FALSE, &pDCompositionTarget);
    //hr = IDCompositionDesktopDevice_CreateTargetForHwnd(pDCompositionDesktopDevice, hWnd, TRUE, &pDCompositionTarget);
    assert(SUCCEEDED(hr));

    hr = IDCompositionTarget_SetRoot(pDCompositionTarget, (IDCompositionVisual*)pVirtualDesktopVisual);
    assert(SUCCEEDED(hr));

    hr = IDCompositionDevice_Commit(pDCompositionDesktopDevice);
    assert(SUCCEEDED(hr));
    
    return S_OK == hr;
}

BOOL DwmEnableWindowComposition(HWND hWnd, BOOL fEnable)
{
    DWM_BLURBEHIND bb = { 0 };

    if (fEnable)
    {
      bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
      bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    }
    else
    {
      bb.dwFlags = DWM_BB_ENABLE;
    }

    bb.fEnable = fEnable;

    return S_OK == DwmEnableBlurBehindWindow(hWnd, &bb);
}