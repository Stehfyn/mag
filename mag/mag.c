#include "mag.h"
#include "render.h"
#include "help.h"
#include "d2dwriteabi.h"

#include <commdlg.h>

#pragma comment(lib, "ntdll")

#define FORWARD_MSG(hwnd, message, fn)    \
    default: return (fn)((hwnd), (message), (wParam), (lParam))

#define HANDLE_DIALOG_MSG(hwnd, message, fn) \
    case (message): return SetDlgMsgResult((hwnd), (message), HANDLE_##message((hwnd), wParam, lParam, (fn)))

#define MOUSE_WHEEL_ZOOM_STEP_SCALE 0.25f
#define WM_MAG_TRAYICON (WM_APP + 1)
#define MAG_TRAY_ICON_ID 1

/* void Cls_OnCaptureChanged(HWND hwnd, HWND hwndNewCapture) */
#define HANDLE_WM_CAPTURECHANGED(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (HWND)(lParam)), 0L)
#define FORWARD_WM_CAPTURECHANGED(hwnd, hwndNewCapture, fn) \
        (void)(fn)((hwnd), WM_CAPTURECHANGED, 0L, (LPARAM)(HWND)(hwndNewCapture))

/* void Cls_OnDwmColorizationColorChanged(HWND hwnd, DWORD colorizationColor, BOOL fOpaqueBlend) */
#define HANDLE_WM_DWMCOLORIZATIONCOLORCHANGED(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (DWORD)(wParam), (BOOL)(lParam)), 0L)
#define FORWARD_WM_DWMCOLORIZATIONCOLORCHANGED(hwnd, colorizationColor, fOpaqueBlend, fn) \
        (void)(fn)((hwnd), WM_DWMCOLORIZATIONCOLORCHANGED, (WPARAM)(DWORD)(colorizationColor), (LPARAM)(BOOL)(fOpaqueBlend))

/* void Cls_OnEnterMenuLoop(HWND hwnd, BOOL fIsTrackPopupMenu) */
#define HANDLE_WM_ENTERMENULOOP(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (BOOL)(wParam)), 0L)
#define FORWARD_WM_ENTERMENULOOP(hwnd, fIsTrackPopupMenu, fn) \
        (void)(fn)((hwnd), WM_ENTERMENULOOP, (WPARAM)(BOOL)(fIsTrackPopupMenu), 0L)

/* void Cls_OnEnterSizeMove(HWND hwnd) */
#define HANDLE_WM_ENTERSIZEMOVE(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)
#define FORWARD_WM_ENTERSIZEMOVE(hwnd, fn) \
        (void)(fn)((hwnd), WM_ENTERSIZEMOVE, 0L, 0L)

/* void Cls_OnExitMenuLoop(HWND hwnd, BOOL fIsTrackPopupMenu) */
#define HANDLE_WM_EXITMENULOOP(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (BOOL)(wParam)), 0L)
#define FORWARD_WM_EXITMENULOOP(hwnd, fIsTrackPopupMenu, fn) \
        (void)(fn)((hwnd), WM_EXITMENULOOP, (WPARAM)(BOOL)(fIsTrackPopupMenu), 0L)

/* void Cls_OnExitSizeMove(HWND hwnd) */
#define HANDLE_WM_EXITSIZEMOVE(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)
#define FORWARD_WM_EXITSIZEMOVE(hwnd, fn) \
        (void)(fn)((hwnd), WM_EXITSIZEMOVE, 0L, 0L)

/* void Cls_OnMagRender(HWND hwnd) */
#define HANDLE_WM_MAG_RENDER(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)
#define FORWARD_WM_MAG_RENDER(hwnd, fn) \
        (void)(fn)((hwnd), WM_MAG_RENDER, 0L, 0L)

/* void Cls_OnMagPresentationStatus(HWND hwnd) */
#define HANDLE_WM_MAG_PRESENTATION_STATUS(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)
#define FORWARD_WM_MAG_PRESENTATION_STATUS(hwnd, fn) \
        (void)(fn)((hwnd), WM_MAG_PRESENTATION_STATUS, 0L, 0L)

/* void Cls_OnMagTrayIcon(HWND hwnd, UINT id, UINT notification) */
#define HANDLE_WM_MAG_TRAYICON(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (UINT)(wParam), (UINT)(lParam)), 0L)
#define FORWARD_WM_MAG_TRAYICON(hwnd, id, notification, fn) \
        (void)(fn)((hwnd), WM_MAG_TRAYICON, (WPARAM)(UINT)(id), (LPARAM)(UINT)(notification))

typedef struct SETTINGSOPTION
{
  UINT    id;
  LPCTSTR pszName;
  BOOL    fImplemented;
} SETTINGSOPTION;

typedef const MAGNAMEDOPTION* (*MAGOPTIONATPROC)(UINT index);

typedef struct MAGSETTINGSDIALOGSTATE
{
  HWND owner;
  MAGADAPTERCATALOG catalog;
  BOOL catalogValid;
  BOOL updatingControls;
  BOOL autoPreset;
  BOOL pendingFieldsValid;
  BOOL pendingCompatible;
  BOOL pendingApplied;
  COLORREF colorKey;
  COLORREF customColors[16];
  HFONT routeFont;
  ID2D1Factory* routeD2dFactory;
  ID2D1DCRenderTarget* routeD2dTarget;
  ID2D1SolidColorBrush* routeD2dBrush;
  IDWriteFactory* routeDwriteFactory;
  IDWriteTextFormat* routeNodeFormat;
  IDWriteTextFormat* routeValueFormat;
  IDWriteTextFormat* routeHeaderFormat;
  GRAPHICSAPI requestedGraphicsApi;
  CAPTUREAPI requestedCaptureApi;
  UIGRAPHICSAPI requestedUiApi;
  TEXTRENDERER requestedTextRenderer;
  MAGPRESENTATIONSETTINGS requestedPresentation;
  MAGPRESENTATIONSETTINGS resolvedPresentation;
  MAGPRESENTATIONSTATUS pendingStatus;
  UINT availableCounts[12];
  UINT optionCounts[12];
  TCHAR catalogReason[MAG_PRESENTATION_REASON_LENGTH];
  TCHAR pendingReason[MAG_PRESENTATION_REASON_LENGTH];
} MAGSETTINGSDIALOGSTATE;

#define MAG_SETTINGS_EXPLICIT_ITEM_BASE 0x10000U

#pragma comment(lib, "Advapi32")
#pragma comment(lib, "Comdlg32")
#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

static const IID IID_MAG_SETTINGS_ID2D1Factory =
  { 0x06152247, 0x6f50, 0x465a, { 0x92, 0x45, 0x11, 0x8b, 0xfd, 0x3b, 0x60, 0x07 } };
static const IID IID_MAG_SETTINGS_IDWriteFactory =
  { 0xb859ee5a, 0xd838, 0x4b5b, { 0xa2, 0xe8, 0x1a, 0xdc, 0x7d, 0x93, 0xdb, 0x48 } };

#define MAG_SETTINGS_REGISTRY_KEY TEXT("Software\\mag")

#ifndef WS_EX_NOREDIRECTIONBITMAP
#define WS_EX_NOREDIRECTIONBITMAP 0x00200000L
#endif

void mag_ShowPopupMenu(HWND hWnd, int x, int y);
void mag_ShowHelpMenu(HWND hWnd, int x, int y);
void mag_ShowSettingsDialog(HWND hWnd);
void mag_SetViewMode(HWND hWnd, MAGVIEWMODE viewMode);
void mag_UpdateViewWindowStyle(HWND hWnd);
void mag_UpdateWindowOutlineColor(HWND hWnd);
void mag_UpdateLensWindowPosition(HWND hWnd);
BOOL mag_IsLensMode(HWND hWnd);
void mag_AddTrayIcon(HWND hWnd);
void mag_DeleteTrayIcon(HWND hWnd);
void mag_AddGraphicsOptions(HWND hDlg, int idCtl, GRAPHICSAPI selectedApi);
BOOL mag_GetSelectedGraphicsOption(HWND hDlg, int idCtl, UINT* selectedId, BOOL* fImplemented);
void mag_AddSettingsOptions(HWND hDlg, int idCtl, const SETTINGSOPTION* options, UINT count, UINT selectedId);
BOOL mag_GetSelectedSettingsOption(HWND hDlg, int idCtl, const SETTINGSOPTION* options, UINT count, UINT* selectedId, BOOL* fImplemented);
void mag_UpdateSettingsDialogState(HWND hDlg);
void mag_LoadSettings(LPMAGSTATE lpsd);
void mag_SaveSettings(const LPMAGSTATE lpsd);
void mag_GetCaptureRect(LPMAGSTATE lpsd, RECT* lprcCapture);
static BOOL mag_Settings_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam);
static LPCTSTR mag_SettingsShortWaitable(
  MAGWAITABLESWAPCHAINMODE mode);
static void mag_Settings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify);
static void mag_Settings_OnHScroll(HWND hDlg, HWND hwndCtl, UINT code, int position);
static LRESULT mag_Settings_OnNotify(HWND hDlg, int idFrom, NMHDR* header);
static void mag_Settings_OnDrawItem(HWND hDlg, const DRAWITEMSTRUCT* drawItem);
static void mag_Settings_OnMeasureItem(HWND hDlg, MEASUREITEMSTRUCT* measureItem);
static void mag_SettingsUpdateAvailabilitySummary(
  HWND hDlg,
  MAGSETTINGSDIALOGSTATE* dialog);
static void mag_SettingsRedrawRoute(HWND hDlg);
static HBRUSH mag_Settings_OnCtlColorStatic(HWND hDlg, HDC hdc, HWND hwndCtl, int type);
static HBRUSH mag_Settings_OnCtlColorBtn(HWND hDlg, HDC hdc, HWND hwndCtl, int type);
static void mag_Settings_OnDestroy(HWND hDlg);
static void mag_Settings_OnPresentationStatus(HWND hDlg);
static INT_PTR CALLBACK mag_SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
void mag_OnDestroy(HWND hWnd);
void mag_SetTaskbarIcon(HWND hWnd);
void mag_OnActivate(HWND hWnd, UINT state, HWND hWndActDeact, BOOL fMinimized);
void mag_OnPaint(HWND hWnd);
UINT mag_OnEraseBkgnd(HWND hWnd, HDC hDC);
BOOL mag_OnNCCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
UINT mag_OnNCHittest(HWND hWnd, int x, int y);
UINT mag_OnNCCalcSize(HWND hWnd, BOOL fCalcValidRects, NCCALCSIZE_PARAMS* lpcsp);
BOOL mag_OnNCActivate(HWND hWnd, BOOL fActive, HWND hwndActDeact, BOOL fMinimized);
void mag_OnNCRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT codeHitTest);
void mag_OnTrayIcon(HWND hWnd, UINT id, UINT notification);
void mag_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify);
void mag_OnKeyUp(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
void mag_OnTimer(HWND hWnd, UINT_PTR idEvent);
void mag_OnRender(HWND hWnd);
void mag_OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys);
void mag_OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags);
void mag_OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags);
void mag_OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags);
void mag_OnNCMouseMove(HWND hWnd, int x, int y, UINT codeHitTest);
void mag_OnDwmColorizationColorChanged(HWND hWnd, DWORD colorizationColor, BOOL fOpaqueBlend);
void mag_OnSysColorChange(HWND hWnd);
void mag_NotifyMiniMapCursorActivity(HWND hWnd, POINT ptScreen);
void mag_OnCaptureChanged(HWND hWnd, HWND hwndNewCapture);
void mag_OnSize(HWND hWnd, UINT state, int cx, int cy);
void mag_OnEnterMenuLoop(HWND hWnd, BOOL fIsTrackPopupMenu);
void mag_OnExitMenuLoop(HWND hWnd, BOOL fIsTrackPopupMenu);
void mag_OnEnterSizeMove(HWND hWnd);
void mag_OnExitSizeMove(HWND hWnd);
void mag_OnWindowPosChanged(HWND hWnd, const WINDOWPOS* lpwndpos);

LRESULT CALLBACK mag_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ATOM mag_RegisterClassEx(HINSTANCE hInstance);

static const SETTINGSOPTION g_captureApiOptions[] =
{
  { CAPTURE_API_GDI_BITBLT, TEXT("GDI BitBlt"), TRUE },
  { CAPTURE_API_DXGI_DESKTOP_DUPLICATION, TEXT("DXGI Desktop Duplication"), TRUE },
  { CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE, TEXT("Windows Graphics Capture"), TRUE },
  { CAPTURE_API_DWM_THUMBNAIL, TEXT("DWM Thumbnail"), TRUE },
  { CAPTURE_API_DWM_PRIVATE_VISUAL, TEXT("DWM Private Visual"), TRUE },
};

static const SETTINGSOPTION g_uiGraphicsApiOptions[] =
{
  { UI_GRAPHICS_API_NATIVE, TEXT("Native draw list"), TRUE },
  { UI_GRAPHICS_API_DIRECT2D, TEXT("Direct2D"), TRUE },
};

static const SETTINGSOPTION g_textRendererOptions[] =
{
  { TEXT_RENDERER_DIRECTWRITE, TEXT("DirectWrite"), TRUE },
  { TEXT_RENDERER_GPU_GLYPH_ATLAS, TEXT("Glyph atlas (OpenGL GPU / CPU)"), TRUE },
  { TEXT_RENDERER_GDI, TEXT("GDI"), TRUE },
};

static BOOL mag_ReadSettingDword(LPCTSTR valueName, DWORD* value)
{
    DWORD size = sizeof(*value);

    return ERROR_SUCCESS == RegGetValue(
      HKEY_CURRENT_USER,
      MAG_SETTINGS_REGISTRY_KEY,
      valueName,
      RRF_RT_REG_DWORD,
      NULL,
      value,
      &size);
}

static BOOL mag_ReadSettingString(
  LPCTSTR valueName,
  LPTSTR value,
  DWORD valueCount)
{
    DWORD size = valueCount * sizeof(*value);

    if (!value || !valueCount)
    {
      return FALSE;
    }
    value[0] = TEXT('\0');
    return ERROR_SUCCESS == RegGetValue(
      HKEY_CURRENT_USER,
      MAG_SETTINGS_REGISTRY_KEY,
      valueName,
      RRF_RT_REG_SZ,
      NULL,
      value,
      &size);
}

void mag_LoadSettings(LPMAGSTATE lpsd)
{
    DWORD value;
    const DWORD presentationVersion = lpsd->presentationSettings.version;

    if (mag_ReadSettingDword(TEXT("GraphicsApi"), &value) && value < GRAPHICS_API_COUNT)
    {
      lpsd->graphicsApi = (GRAPHICSAPI)value;
    }
    if (mag_ReadSettingDword(TEXT("CaptureApi"), &value) && value < CAPTURE_API_COUNT)
    {
      lpsd->captureApi = (CAPTUREAPI)value;
    }
    if (mag_ReadSettingDword(TEXT("UiGraphicsApi"), &value) && value < UI_GRAPHICS_API_COUNT)
    {
      lpsd->uiGraphicsApi = (UIGRAPHICSAPI)value;
    }
    if (mag_ReadSettingDword(TEXT("TextRenderer"), &value) && value < TEXT_RENDERER_COUNT)
    {
      lpsd->textRenderer = (TEXTRENDERER)value;
    }
    if (mag_ReadSettingDword(TEXT("MouseRelativeZoom"), &value))
    {
      lpsd->fMouseRelativeZoom = 0 != value;
    }
    if (mag_ReadSettingDword(TEXT("AutoSettingsPreset"), &value))
    {
      lpsd->fAutoSettingsPreset = 0 != value;
    }

    if (mag_ReadSettingDword(TEXT("PresentationVersion"), &value) &&
        value == presentationVersion)
    {
#define MAG_READ_PRESENT_ENUM(Name, Field, Count, Type) \
      if (mag_ReadSettingDword(TEXT(Name), &value) && value < (Count)) \
      { \
        lpsd->presentationSettings.Field = (Type)value; \
      }
      MAG_READ_PRESENT_ENUM("PresentationTarget", target, MAG_PRESENT_COUNT, MAGPRESENTATIONTARGET)
      MAG_READ_PRESENT_ENUM("SurfaceOwnership", surfaceOwnership, MAG_SURFACE_COUNT, MAGSURFACEOWNERSHIP)
      MAG_READ_PRESENT_ENUM("CompositionHost", host, MAG_HOST_COUNT, MAGCOMPOSITIONHOST)
      MAG_READ_PRESENT_ENUM("CopyRequirement", copyRequirement, MAG_COPY_COUNT, MAGCOPYREQUIREMENT)
      MAG_READ_PRESENT_ENUM("LayeredAlphaMode", alphaMode, MAG_LAYER_ALPHA_COUNT, MAGLAYEREDALPHAMODE)
      MAG_READ_PRESENT_ENUM("WaitableSwapChainMode", waitableSwapChainMode, MAG_WAITABLE_SWAP_CHAIN_MODE_COUNT, MAGWAITABLESWAPCHAINMODE)
      MAG_READ_PRESENT_ENUM("DisplayAdapterMode", display.mode, MAG_DISPLAY_ADAPTER_MODE_COUNT, MAGDISPLAYADAPTERMODE)
      MAG_READ_PRESENT_ENUM("HardwareAdapterMode", hardware.mode, MAG_HARDWARE_ADAPTER_MODE_COUNT, MAGHARDWAREADAPTERMODE)
#undef MAG_READ_PRESENT_ENUM

      if (mag_ReadSettingDword(TEXT("ConstantAlpha"), &value) && value <= 255)
      {
        lpsd->presentationSettings.constantAlpha = (BYTE)value;
      }
      if (mag_ReadSettingDword(TEXT("LayerColorKey"), &value))
      {
        lpsd->presentationSettings.colorKey = (COLORREF)value;
      }
      if (mag_ReadSettingDword(TEXT("StrictPresentationTarget"), &value))
      {
        lpsd->presentationSettings.strictTarget = 0 != value;
      }
      if (mag_ReadSettingDword(TEXT("AllowTearing"), &value))
      {
        lpsd->presentationSettings.allowTearing = 0 != value;
      }
      if (mag_ReadSettingDword(TEXT("PresentationBufferCount"), &value) && value >= 2 && value <= 16)
      {
        lpsd->presentationSettings.bufferCount = value;
      }
      if (mag_ReadSettingDword(TEXT("MaximumFrameLatency"), &value) && value >= 1 && value <= 16)
      {
        lpsd->presentationSettings.maximumFrameLatency = value;
      }
      if (mag_ReadSettingDword(TEXT("SyncInterval"), &value) && value <= 4)
      {
        lpsd->presentationSettings.syncInterval = value;
      }
      if (mag_ReadSettingDword(TEXT("DisplayAdapterLuidLow"), &value))
      {
        lpsd->presentationSettings.display.adapterLuid.LowPart = value;
      }
      if (mag_ReadSettingDword(TEXT("DisplayAdapterLuidHigh"), &value))
      {
        lpsd->presentationSettings.display.adapterLuid.HighPart = (LONG)value;
      }
      mag_ReadSettingString(
        TEXT("DisplayDeviceName"),
        lpsd->presentationSettings.display.deviceName,
        ARRAYSIZE(lpsd->presentationSettings.display.deviceName));
      if (mag_ReadSettingDword(TEXT("HardwareAdapterLuidLow"), &value))
      {
        lpsd->presentationSettings.hardware.adapterLuid.LowPart = value;
      }
      if (mag_ReadSettingDword(TEXT("HardwareAdapterLuidHigh"), &value))
      {
        lpsd->presentationSettings.hardware.adapterLuid.HighPart = (LONG)value;
      }
    }
}

void mag_SaveSettings(const LPMAGSTATE lpsd)
{
    HKEY key;
    DWORD disposition;

    if (!lpsd || !magGraphicsIsInputDesktop() || ERROR_SUCCESS != RegCreateKeyEx(
          HKEY_CURRENT_USER,
          MAG_SETTINGS_REGISTRY_KEY,
          0,
          NULL,
          REG_OPTION_NON_VOLATILE,
          KEY_SET_VALUE,
          NULL,
          &key,
          &disposition))
    {
      return;
    }

    {
      const DWORD graphicsApi = lpsd->graphicsApi;
      const DWORD captureApi = lpsd->captureApi;
      const DWORD uiApi = lpsd->uiGraphicsApi;
      const DWORD textRenderer = lpsd->textRenderer;
      const DWORD mouseRelativeZoom = lpsd->fMouseRelativeZoom;
      const DWORD autoSettingsPreset = lpsd->fAutoSettingsPreset;
      const MAGPRESENTATIONSETTINGS* presentation = &lpsd->presentationSettings;

#define MAG_WRITE_DWORD(Name, Value) \
      do \
      { \
        const DWORD savedValue = (DWORD)(Value); \
        RegSetValueEx(key, TEXT(Name), 0, REG_DWORD, (const BYTE*)&savedValue, sizeof(savedValue)); \
      } while (0)

      RegSetValueEx(key, TEXT("GraphicsApi"), 0, REG_DWORD, (const BYTE*)&graphicsApi, sizeof(graphicsApi));
      RegSetValueEx(key, TEXT("CaptureApi"), 0, REG_DWORD, (const BYTE*)&captureApi, sizeof(captureApi));
      RegSetValueEx(key, TEXT("UiGraphicsApi"), 0, REG_DWORD, (const BYTE*)&uiApi, sizeof(uiApi));
      RegSetValueEx(key, TEXT("TextRenderer"), 0, REG_DWORD, (const BYTE*)&textRenderer, sizeof(textRenderer));
      RegSetValueEx(key, TEXT("MouseRelativeZoom"), 0, REG_DWORD, (const BYTE*)&mouseRelativeZoom, sizeof(mouseRelativeZoom));
      RegSetValueEx(key, TEXT("AutoSettingsPreset"), 0, REG_DWORD, (const BYTE*)&autoSettingsPreset, sizeof(autoSettingsPreset));
      MAG_WRITE_DWORD("PresentationVersion", presentation->version);
      MAG_WRITE_DWORD("PresentationTarget", presentation->target);
      MAG_WRITE_DWORD("SurfaceOwnership", presentation->surfaceOwnership);
      MAG_WRITE_DWORD("CompositionHost", presentation->host);
      MAG_WRITE_DWORD("CopyRequirement", presentation->copyRequirement);
      MAG_WRITE_DWORD("LayeredAlphaMode", presentation->alphaMode);
      MAG_WRITE_DWORD("ConstantAlpha", presentation->constantAlpha);
      MAG_WRITE_DWORD("LayerColorKey", presentation->colorKey);
      MAG_WRITE_DWORD("StrictPresentationTarget", presentation->strictTarget);
      MAG_WRITE_DWORD("AllowTearing", presentation->allowTearing);
      MAG_WRITE_DWORD("WaitableSwapChainMode", presentation->waitableSwapChainMode);
      MAG_WRITE_DWORD("PresentationBufferCount", presentation->bufferCount);
      MAG_WRITE_DWORD("MaximumFrameLatency", presentation->maximumFrameLatency);
      MAG_WRITE_DWORD("SyncInterval", presentation->syncInterval);
      MAG_WRITE_DWORD("DisplayAdapterMode", presentation->display.mode);
      MAG_WRITE_DWORD("DisplayAdapterLuidLow", presentation->display.adapterLuid.LowPart);
      MAG_WRITE_DWORD("DisplayAdapterLuidHigh", presentation->display.adapterLuid.HighPart);
      MAG_WRITE_DWORD("HardwareAdapterMode", presentation->hardware.mode);
      MAG_WRITE_DWORD("HardwareAdapterLuidLow", presentation->hardware.adapterLuid.LowPart);
      MAG_WRITE_DWORD("HardwareAdapterLuidHigh", presentation->hardware.adapterLuid.HighPart);
      RegSetValueEx(
        key,
        TEXT("DisplayDeviceName"),
        0,
        REG_SZ,
        (const BYTE*)presentation->display.deviceName,
        (lstrlen(presentation->display.deviceName) + 1) * sizeof(TCHAR));
#undef MAG_WRITE_DWORD
    }
    RegCloseKey(key);
    UNREFERENCED_PARAMETER(disposition);
}

void mag_GetCaptureRect(LPMAGSTATE lpsd, RECT* lprcCapture)
{
    *lprcCapture = lpsd->di.rc;

    if (IsRectEmpty(lprcCapture))
    {
      lprcCapture->left = GetSystemMetrics(SM_XVIRTUALSCREEN);
      lprcCapture->top = GetSystemMetrics(SM_YVIRTUALSCREEN);
      lprcCapture->right = lprcCapture->left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
      lprcCapture->bottom = lprcCapture->top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
}

void mag_ShowPopupMenu(HWND hWnd, int x, int y)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const MAGVIEWMODE viewMode = lpsd ? lpsd->viewMode : MAG_VIEW_WINDOW;
    UINT checkedPosition = 0;
    
    HMENU hMenu = CreatePopupMenu();
    //HMENU hMenu = LoadPopupMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));

    switch (viewMode)
    {
    case MAG_VIEW_FOLLOW_MOUSE:
      checkedPosition = 1;
      break;
    case MAG_VIEW_LENS:
      checkedPosition = 2;
      break;
    case MAG_VIEW_WINDOW:
    default:
      checkedPosition = 0;
      break;
    }

    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_WINDOW_MODE, TEXT("Window"));
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_FOLLOW_MOUSE, TEXT("Follow Mouse"));
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_LENS_MODE, TEXT("Lens"));
    CheckMenuRadioItem(hMenu, 0, 2, checkedPosition, MF_BYPOSITION);
    AppendMenu(hMenu, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_SETTINGS, TEXT("Settings..."));
    AppendMenu(hMenu, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_HELP, TEXT("Help"));
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_CLOSE, TEXT("Exit"));

    TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_WORKAREA, x, y, hWnd, NULL);

    DestroyMenu(hMenu);
}

void mag_SetViewMode(HWND hWnd, MAGVIEWMODE viewMode)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd || viewMode >= MAG_VIEW_COUNT)
    {
      return;
    }

    lpsd->viewMode = viewMode;
    lpsd->fTrackCursor = (MAG_VIEW_FOLLOW_MOUSE == viewMode || MAG_VIEW_LENS == viewMode);
    lpsd->fUseSourceOrigin = FALSE;
    lpsd->fSourceOriginPinned = FALSE;
    lpsd->fMiniMapDragging = FALSE;

    mag_UpdateViewWindowStyle(hWnd);

    if (MAG_VIEW_LENS == viewMode)
    {
      mag_UpdateLensWindowPosition(hWnd);
    }

    render_minimapNotifyActivity(hWnd);
    renderRender(hWnd);
}

void mag_UpdateViewWindowStyle(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    DWORD oldExStyle;
    DWORD dwExStyle;

    if (!lpsd)
    {
      return;
    }

    oldExStyle = GetWindowExStyle(hWnd);
    dwExStyle = oldExStyle;
    dwExStyle &= ~(WS_EX_LAYERED | WS_EX_NOREDIRECTIONBITMAP);
    if (MAG_HOST_TRADITIONAL_LAYERED == lpsd->resolvedPresentation.host)
    {
      dwExStyle |= WS_EX_LAYERED;
    }
    else if (MAG_SURFACE_NO_REDIRECTION == lpsd->resolvedPresentation.surfaceOwnership)
    {
      dwExStyle |= WS_EX_NOREDIRECTIONBITMAP;
    }
    if (MAG_VIEW_LENS == lpsd->viewMode)
    {
      dwExStyle |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    }
    else
    {
      dwExStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }

    if (dwExStyle != oldExStyle)
    {
      SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);
      SetWindowPos(
        hWnd,
        HWND_TOPMOST,
        0,
        0,
        0,
        0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    }
}

void mag_UpdateWindowOutlineColor(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    COLORREF accentColor;

    if (!lpsd)
    {
      return;
    }

    accentColor = GetSysColor(COLOR_HIGHLIGHT);
    lpsd->outlineColor[0] = GetRValue(accentColor) / 255.0f;
    lpsd->outlineColor[1] = GetGValue(accentColor) / 255.0f;
    lpsd->outlineColor[2] = GetBValue(accentColor) / 255.0f;
    lpsd->outlineColor[3] = 1.0f;
}

void mag_UpdateLensWindowPosition(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    POINT cursor;
    POINT ptClientOrigin = { 0, 0 };
    RECT rcWindow;
    RECT rcClient;
    RECT rcMonitor;
    LONG clientWidth;
    LONG clientHeight;
    LONG clientOffsetX;
    LONG clientOffsetY;
    LONG srcW;
    LONG srcH;
    LONG srcX;
    LONG srcY;
    LONG clientCursorX;
    LONG clientCursorY;
    LONG x;
    LONG y;
    FLOAT m;
    HMONITOR hMonitor;
    MONITORINFO mi = { sizeof(mi) };

    if (!lpsd || MAG_VIEW_LENS != lpsd->viewMode)
    {
      return;
    }

    if (!GetCursorPos(&cursor) ||
        !GetWindowRect(hWnd, &rcWindow) ||
        !GetClientRect(hWnd, &rcClient) ||
        !ClientToScreen(hWnd, &ptClientOrigin))
    {
      return;
    }

    clientWidth = RECTWIDTH(rcClient);
    clientHeight = RECTHEIGHT(rcClient);
    if (clientWidth < 1 || clientHeight < 1)
    {
      return;
    }

    clientOffsetX = ptClientOrigin.x - rcWindow.left;
    clientOffsetY = ptClientOrigin.y - rcWindow.top;
    hMonitor = MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST);
    if (hMonitor && GetMonitorInfo(hMonitor, &mi))
    {
      rcMonitor = mi.rcMonitor;
    }
    else
    {
      mag_GetCaptureRect(lpsd, &rcMonitor);
    }

    m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
    if (m <= 1.0001f)
    {
      srcW = clientWidth;
      srcH = clientHeight;
    }
    else
    {
      srcW = max(1, (LONG)(clientWidth / m));
      srcH = max(1, (LONG)(clientHeight / m));
    }

    srcX = render_clipSourceOrigin(cursor.x - srcW / 2, srcW, rcMonitor.left, rcMonitor.right);
    srcY = render_clipSourceOrigin(cursor.y - srcH / 2, srcH, rcMonitor.top, rcMonitor.bottom);
    clientCursorX = CLAMP(MulDiv(cursor.x - srcX, clientWidth, srcW), 0, clientWidth);
    clientCursorY = CLAMP(MulDiv(cursor.y - srcY, clientHeight, srcH), 0, clientHeight);
    x = cursor.x - clientCursorX - clientOffsetX;
    y = cursor.y - clientCursorY - clientOffsetY;

    if (rcWindow.left != x || rcWindow.top != y)
    {
      SetWindowPos(hWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

BOOL mag_IsLensMode(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    return lpsd && MAG_VIEW_LENS == lpsd->viewMode;
}

void mag_AddTrayIcon(HWND hWnd)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    NOTIFYICONDATA nid = { sizeof(nid) };

    nid.hWnd = hWnd;
    nid.uID = MAG_TRAY_ICON_ID;
    nid.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid.uCallbackMessage = WM_MAG_TRAYICON;
    nid.hIcon = (HICON)LoadImage(
      hInstance,
      MAKEINTRESOURCE(IDI_APPICON),
      IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON),
      GetSystemMetrics(SM_CYSMICON),
      LR_DEFAULTCOLOR | LR_SHARED);
    lstrcpyn(nid.szTip, TEXT("mag"), ARRAYSIZE(nid.szTip));

    Shell_NotifyIcon(NIM_ADD, &nid);
}

void mag_DeleteTrayIcon(HWND hWnd)
{
    NOTIFYICONDATA nid = { sizeof(nid) };

    nid.hWnd = hWnd;
    nid.uID = MAG_TRAY_ICON_ID;
    Shell_NotifyIcon(NIM_DELETE, &nid);
}

void mag_ShowHelpMenu(HWND hWnd, int x, int y)
{
    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);

    help_Show(hWnd);
}

void mag_ShowSettingsDialog(HWND hWnd)
{
    DialogBoxParam(
      GetModuleHandle(NULL),
      MAKEINTRESOURCE(IDD_SETTINGS),
      hWnd,
      mag_SettingsDlgProc,
      (LPARAM)hWnd);
}

void mag_AddSettingsOptions(HWND hDlg, int idCtl, const SETTINGSOPTION* options, UINT count, UINT selectedId)
{
    HWND hCtl = GetDlgItem(hDlg, idCtl);
    UINT i;

    for (i = 0; i < count; ++i)
    {
      const LRESULT item = SendMessage(hCtl, CB_ADDSTRING, 0, (LPARAM)options[i].pszName);

      if (CB_ERR != item && CB_ERRSPACE != item)
      {
        SendMessage(hCtl, CB_SETITEMDATA, (WPARAM)item, (LPARAM)i);

        if (options[i].id == selectedId)
        {
          SendMessage(hCtl, CB_SETCURSEL, (WPARAM)item, 0);
        }
      }
    }

    if (CB_ERR == SendMessage(hCtl, CB_GETCURSEL, 0, 0) && 0 < count)
    {
      SendMessage(hCtl, CB_SETCURSEL, 0, 0);
    }
}

void mag_AddGraphicsOptions(HWND hDlg, int idCtl, GRAPHICSAPI selectedApi)
{
    HWND hCtl = GetDlgItem(hDlg, idCtl);
    UINT i;

    for (i = 0; i < magGraphicsGetBackendCount(); ++i)
    {
      const MAGGRAPHICSBACKEND* backend = magGraphicsGetBackendAt(i);
      const LRESULT item = SendMessage(hCtl, CB_ADDSTRING, 0, (LPARAM)backend->name);

      if (CB_ERR != item && CB_ERRSPACE != item)
      {
        SendMessage(hCtl, CB_SETITEMDATA, (WPARAM)item, (LPARAM)backend->api);
        if (backend->api == selectedApi)
        {
          SendMessage(hCtl, CB_SETCURSEL, (WPARAM)item, 0);
        }
      }
    }

    if (CB_ERR == SendMessage(hCtl, CB_GETCURSEL, 0, 0) && magGraphicsGetBackendCount())
    {
      SendMessage(hCtl, CB_SETCURSEL, 0, 0);
    }
}

BOOL mag_GetSelectedGraphicsOption(HWND hDlg, int idCtl, UINT* selectedId, BOOL* fImplemented)
{
    const LRESULT selectedItem = SendDlgItemMessage(hDlg, idCtl, CB_GETCURSEL, 0, 0);
    LRESULT api;
    const MAGGRAPHICSBACKEND* backend;
    TCHAR reason[2];

    if (CB_ERR == selectedItem)
    {
      return FALSE;
    }

    api = SendDlgItemMessage(hDlg, idCtl, CB_GETITEMDATA, (WPARAM)selectedItem, 0);
    if (CB_ERR == api || api < 0 || (UINT)api >= GRAPHICS_API_COUNT)
    {
      return FALSE;
    }

    backend = magGraphicsGetBackend((GRAPHICSAPI)api);
    if (!backend)
    {
      return FALSE;
    }

    *selectedId = (UINT)api;
    *fImplemented = backend->implemented && backend->IsAvailable(reason, ARRAYSIZE(reason));
    return TRUE;
}

BOOL mag_GetSelectedSettingsOption(HWND hDlg, int idCtl, const SETTINGSOPTION* options, UINT count, UINT* selectedId, BOOL* fImplemented)
{
    const LRESULT selectedItem = SendDlgItemMessage(hDlg, idCtl, CB_GETCURSEL, 0, 0);
    LRESULT optionIndex;

    if (CB_ERR == selectedItem)
    {
      return FALSE;
    }

    optionIndex = SendDlgItemMessage(hDlg, idCtl, CB_GETITEMDATA, (WPARAM)selectedItem, 0);
    if (CB_ERR == optionIndex || optionIndex < 0 || (UINT)optionIndex >= count)
    {
      return FALSE;
    }

    *selectedId = options[optionIndex].id;
    *fImplemented = options[optionIndex].fImplemented;
    return TRUE;
}

static void mag_AddNamedOptions(
  HWND hDlg,
  int idCtl,
  UINT count,
  MAGOPTIONATPROC optionAt,
  UINT selectedId)
{
    HWND hCtl = GetDlgItem(hDlg, idCtl);
    UINT i;

    SendMessage(hCtl, CB_RESETCONTENT, 0, 0);
    for (i = 0; i < count; ++i)
    {
      const MAGNAMEDOPTION* option = optionAt(i);
      LRESULT item;

      if (!option)
      {
        continue;
      }
      item = SendMessage(hCtl, CB_ADDSTRING, 0, (LPARAM)option->name);
      if (CB_ERR != item && CB_ERRSPACE != item)
      {
        SendMessage(hCtl, CB_SETITEMDATA, (WPARAM)item, (LPARAM)option->id);
        if (option->id == selectedId)
        {
          SendMessage(hCtl, CB_SETCURSEL, (WPARAM)item, 0);
        }
      }
    }
    if (CB_ERR == SendMessage(hCtl, CB_GETCURSEL, 0, 0) && count)
    {
      SendMessage(hCtl, CB_SETCURSEL, 0, 0);
    }
}

static BOOL mag_GetSelectedNamedOption(
  HWND hDlg,
  int idCtl,
  UINT count,
  UINT* selectedId)
{
    LRESULT item = SendDlgItemMessage(hDlg, idCtl, CB_GETCURSEL, 0, 0);
    LRESULT value;

    if (CB_ERR == item || !selectedId)
    {
      return FALSE;
    }
    value = SendDlgItemMessage(hDlg, idCtl, CB_GETITEMDATA, (WPARAM)item, 0);
    if (CB_ERR == value || value < 0 || (UINT)value >= count)
    {
      return FALSE;
    }
    *selectedId = (UINT)value;
    return TRUE;
}

static void mag_SettingsFitComboDropDown(HWND hDlg, UINT controlId)
{
    HWND hCombo = GetDlgItem(hDlg, controlId);
    RECT comboRect;
    HDC hDC;
    HFONT hFont;
    HFONT hOldFont = NULL;
    SIZE statusTextSize = { 0 };
    int desiredWidth;
    int auxiliaryWidth;
    int itemCount;
    int itemIndex;

    if (!hCombo || !GetClientRect(hCombo, &comboRect))
    {
      return;
    }
    desiredWidth = RECTWIDTH(comboRect);
    hDC = GetDC(hCombo);
    hFont = (HFONT)SendMessage(hCombo, WM_GETFONT, 0, 0);
    if (hDC && hFont)
    {
      hOldFont = SelectFont(hDC, hFont);
    }
    if (hDC && IDC_SETTINGS_PRESET != controlId)
    {
      GetTextExtentPoint32(
        hDC,
        TEXT("Unavailable"),
        (int)lstrlen(TEXT("Unavailable")),
        &statusTextSize);
    }
    if (IDC_SETTINGS_PRESET != controlId && statusTextSize.cx <= 0)
    {
      statusTextSize.cx = MulDiv(
        80,
        (int)GetDpiForWindow(hCombo),
        USER_DEFAULT_SCREEN_DPI);
    }
    auxiliaryWidth = GetSystemMetrics(SM_CXVSCROLL) +
      (IDC_SETTINGS_PRESET == controlId
        ? MulDiv(
            16,
            (int)GetDpiForWindow(hCombo),
            USER_DEFAULT_SCREEN_DPI)
        : statusTextSize.cx + MulDiv(
            80,
            (int)GetDpiForWindow(hCombo),
            USER_DEFAULT_SCREEN_DPI));
    itemCount = (int)SendMessage(hCombo, CB_GETCOUNT, 0, 0);
    for (itemIndex = 0; hDC && itemIndex < itemCount; ++itemIndex)
    {
      const LRESULT textLength = SendMessage(
        hCombo, CB_GETLBTEXTLEN, (WPARAM)itemIndex, 0);
      TCHAR text[512];
      SIZE textSize;

      if (textLength < 0 || textLength >= ARRAYSIZE(text))
      {
        continue;
      }
      if (CB_ERR == SendMessage(
            hCombo, CB_GETLBTEXT, (WPARAM)itemIndex, (LPARAM)text) ||
          !GetTextExtentPoint32(hDC, text, (int)textLength, &textSize))
      {
        continue;
      }
      desiredWidth = max(
        desiredWidth,
        textSize.cx + auxiliaryWidth);
    }
    if (hOldFont)
    {
      SelectFont(hDC, hOldFont);
    }
    if (hDC)
    {
      ReleaseDC(hCombo, hDC);
    }
    {
      RECT screenRect;
      MONITORINFO monitorInfo = { sizeof(monitorInfo) };
      HMONITOR monitor = MonitorFromWindow(
        hCombo,
        MONITOR_DEFAULTTONEAREST);

      if (GetWindowRect(hCombo, &screenRect) &&
          GetMonitorInfo(monitor, &monitorInfo))
      {
        desiredWidth = min(
          desiredWidth,
          max(
            RECTWIDTH(comboRect),
            monitorInfo.rcWork.right - screenRect.left));
      }
    }
    SendMessage(hCombo, CB_SETDROPPEDWIDTH, (WPARAM)max(1, desiredWidth), 0);
}

static void mag_SettingsUpdateSliderValue(
  HWND hDlg,
  UINT sliderId,
  UINT valueId)
{
    SetDlgItemInt(
      hDlg,
      valueId,
      (UINT)SendDlgItemMessage(hDlg, sliderId, TBM_GETPOS, 0, 0),
      FALSE);
}

static void mag_SettingsSetSlider(
  HWND hDlg,
  UINT sliderId,
  UINT valueId,
  UINT minimum,
  UINT maximum,
  UINT value)
{
    SendDlgItemMessage(
      hDlg,
      sliderId,
      TBM_SETRANGE,
      TRUE,
      MAKELPARAM(minimum, maximum));
    SendDlgItemMessage(
      hDlg,
      sliderId,
      TBM_SETPAGESIZE,
      0,
      max(1U, (maximum - minimum) / 8U));
    SendDlgItemMessage(hDlg, sliderId, TBM_SETPOS, TRUE, value);
    mag_SettingsUpdateSliderValue(hDlg, sliderId, valueId);
}

static void mag_SettingsUpdateColorKeyButton(HWND hDlg, COLORREF colorKey)
{
    TCHAR label[16];

    _sntprintf_s(
      label,
      ARRAYSIZE(label),
      _TRUNCATE,
      TEXT("#%02X%02X%02X..."),
      GetRValue(colorKey),
      GetGValue(colorKey),
      GetBValue(colorKey));
    SetDlgItemText(hDlg, IDC_SETTINGS_COLOR_KEY, label);
}

enum
{
  MAG_SETTINGS_PRESET_CUSTOM = 0,
  MAG_SETTINGS_PRESET_AUTO = 1,
};

static BOOL mag_SettingsSelectComboData(
  HWND hDlg,
  UINT controlId,
  UINT_PTR value)
{
    HWND hCombo = GetDlgItem(hDlg, controlId);
    const LRESULT count = SendMessage(hCombo, CB_GETCOUNT, 0, 0);
    LRESULT item;

    for (item = 0; item < count; ++item)
    {
      if ((UINT_PTR)SendMessage(
            hCombo,
            CB_GETITEMDATA,
            (WPARAM)item,
            0) == value)
      {
        SendMessage(hCombo, CB_SETCURSEL, (WPARAM)item, 0);
        return TRUE;
      }
    }
    return FALSE;
}

static void mag_SettingsSetPresetSelection(
  HWND hDlg,
  MAGSETTINGSDIALOGSTATE* dialog,
  BOOL automatic)
{
    if (!dialog)
    {
      return;
    }
    dialog->autoPreset = automatic;
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_PRESET,
      automatic ? MAG_SETTINGS_PRESET_AUTO : MAG_SETTINGS_PRESET_CUSTOM);
}

static void mag_SettingsSetAutoPresentationControls(
  HWND hDlg,
  MAGSETTINGSDIALOGSTATE* dialog)
{
    MAGPRESENTATIONSETTINGS automatic;

    if (!dialog)
    {
      return;
    }
    magPresentationSettingsSetDefaults(&automatic);
    dialog->updatingControls = TRUE;
    mag_SettingsSetPresetSelection(hDlg, dialog, TRUE);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_PRESENT_TARGET,
      automatic.target);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      automatic.surfaceOwnership);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_COMPOSITION_HOST,
      automatic.host);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_COPY_REQUIREMENT,
      automatic.copyRequirement);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_ALPHA_MODE,
      automatic.alphaMode);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
      automatic.waitableSwapChainMode);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_DISPLAY_ADAPTER,
      automatic.display.mode);
    mag_SettingsSelectComboData(
      hDlg,
      IDC_SETTINGS_HARDWARE_ADAPTER,
      automatic.hardware.mode);
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_STRICT_TARGET,
      BM_SETCHECK,
      automatic.strictTarget ? BST_CHECKED : BST_UNCHECKED,
      0);
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_ALLOW_TEARING,
      BM_SETCHECK,
      automatic.allowTearing ? BST_CHECKED : BST_UNCHECKED,
      0);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_BUFFER_COUNT,
      IDC_SETTINGS_VALUE_BUFFER_COUNT,
      2,
      16,
      automatic.bufferCount);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_FRAME_LATENCY,
      IDC_SETTINGS_VALUE_FRAME_LATENCY,
      1,
      16,
      automatic.maximumFrameLatency);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_SYNC_INTERVAL,
      IDC_SETTINGS_VALUE_SYNC_INTERVAL,
      0,
      4,
      automatic.syncInterval);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_CONSTANT_ALPHA,
      IDC_SETTINGS_VALUE_CONSTANT_ALPHA,
      0,
      255,
      automatic.constantAlpha);
    dialog->colorKey = automatic.colorKey;
    mag_SettingsUpdateColorKeyButton(hDlg, dialog->colorKey);
    dialog->updatingControls = FALSE;
}

static void mag_SettingsMarkCustom(HWND hDlg)
{
    MAGSETTINGSDIALOGSTATE* dialog =
      (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);

    if (!dialog || dialog->updatingControls)
    {
      return;
    }
    dialog->updatingControls = TRUE;
    mag_SettingsSetPresetSelection(hDlg, dialog, FALSE);
    dialog->updatingControls = FALSE;
}

static LRESULT mag_AddComboItem(HWND hCtl, LPCTSTR text, UINT_PTR data)
{
    LRESULT item = SendMessage(hCtl, CB_ADDSTRING, 0, (LPARAM)text);

    if (CB_ERR != item && CB_ERRSPACE != item)
    {
      SendMessage(hCtl, CB_SETITEMDATA, (WPARAM)item, (LPARAM)data);
    }
    return item;
}

static void mag_AddDisplayAdapterOptions(
  HWND hDlg,
  const MAGSETTINGSDIALOGSTATE* dialog,
  const MAGDISPLAYSELECTION* selected)
{
    HWND hCtl = GetDlgItem(hDlg, IDC_SETTINGS_DISPLAY_ADAPTER);
    LRESULT selectedItem = CB_ERR;
    LRESULT item;
    UINT i;

    SendMessage(hCtl, CB_RESETCONTENT, 0, 0);
    item = mag_AddComboItem(hCtl, TEXT("Auto (window output)"), MAG_DISPLAY_ADAPTER_AUTO);
    if (selected && MAG_DISPLAY_ADAPTER_AUTO == selected->mode)
    {
      selectedItem = item;
    }
    item = mag_AddComboItem(hCtl, TEXT("Follow captured display"), MAG_DISPLAY_ADAPTER_FOLLOW_CAPTURE);
    if (selected && MAG_DISPLAY_ADAPTER_FOLLOW_CAPTURE == selected->mode)
    {
      selectedItem = item;
    }

    for (i = 0; dialog && dialog->catalogValid && i < dialog->catalog.outputCount; ++i)
    {
      const MAGOUTPUTINFO* output = &dialog->catalog.outputs[i];
      const MAGADAPTERINFO* adapter = output->adapterIndex < dialog->catalog.adapterCount
        ? &dialog->catalog.adapters[output->adapterIndex]
        : NULL;
      TCHAR label[512];
      TCHAR planes[192] = TEXT("");

      if (!adapter)
      {
        continue;
      }
      if (output->overlayCapsKnown)
      {
        _sntprintf_s(
          planes,
          ARRAYSIZE(planes),
          _TRUNCATE,
          TEXT(" | DF %s | IF %s | MPO %u (%u RGB/%u YUV)"),
          output->directFlipCapable ? TEXT("yes") : TEXT("no"),
          output->independentFlipCapable ? TEXT("yes") : TEXT("no"),
          output->maxPlanes,
          output->maxRgbPlanes,
          output->maxYuvPlanes);
      }
      else if (output->displayPlaneCapsKnown)
      {
        _sntprintf_s(
          planes,
          ARRAYSIZE(planes),
          _TRUNCATE,
          TEXT(" | DF %s | IF %s | MPO %s (plane count unavailable)"),
          output->directFlipCapable ? TEXT("yes") : TEXT("no"),
          output->independentFlipCapable ? TEXT("yes") : TEXT("no"),
          output->multiPlaneOverlayCapable ? TEXT("yes") : TEXT("no"));
      }
      _sntprintf_s(
        label,
        ARRAYSIZE(label),
        _TRUNCATE,
        TEXT("%s | %s | %ld,%ld %ldx%ld | %u Hz%s%s"),
        adapter->description,
        output->deviceName,
        output->desktopCoordinates.left,
        output->desktopCoordinates.top,
        RECTWIDTH(output->desktopCoordinates),
        RECTHEIGHT(output->desktopCoordinates),
        output->refreshDenominator
          ? output->refreshNumerator / output->refreshDenominator
          : 0,
        output->hdr ? TEXT(" | HDR") : TEXT(""),
        planes);
      item = mag_AddComboItem(hCtl, label, MAG_SETTINGS_EXPLICIT_ITEM_BASE + i);
      if (selected && MAG_DISPLAY_ADAPTER_EXPLICIT == selected->mode &&
          magAdapterLuidEqual(selected->adapterLuid, adapter->luid) &&
          0 == lstrcmpi(selected->deviceName, output->deviceName))
      {
        selectedItem = item;
      }
    }

    SendMessage(hCtl, CB_SETCURSEL, CB_ERR == selectedItem ? 0 : selectedItem, 0);
}

static void mag_AddHardwareAdapterOptions(
  HWND hDlg,
  const MAGSETTINGSDIALOGSTATE* dialog,
  const MAGHARDWARESELECTION* selected)
{
    HWND hCtl = GetDlgItem(hDlg, IDC_SETTINGS_HARDWARE_ADAPTER);
    LRESULT selectedItem = CB_ERR;
    LRESULT item;
    UINT i;

    SendMessage(hCtl, CB_RESETCONTENT, 0, 0);
    item = mag_AddComboItem(hCtl, TEXT("Auto (highest-performance compatible)"), MAG_HARDWARE_ADAPTER_AUTO);
    if (selected && MAG_HARDWARE_ADAPTER_AUTO == selected->mode)
    {
      selectedItem = item;
    }
    item = mag_AddComboItem(hCtl, TEXT("Same as Display Adapter"), MAG_HARDWARE_ADAPTER_SAME_AS_DISPLAY);
    if (selected && MAG_HARDWARE_ADAPTER_SAME_AS_DISPLAY == selected->mode)
    {
      selectedItem = item;
    }
    item = mag_AddComboItem(hCtl, TEXT("Same as Capture Adapter"), MAG_HARDWARE_ADAPTER_SAME_AS_CAPTURE);
    if (selected && MAG_HARDWARE_ADAPTER_SAME_AS_CAPTURE == selected->mode)
    {
      selectedItem = item;
    }
    item = mag_AddComboItem(hCtl, TEXT("WARP (software, explicit fallback)"), MAG_HARDWARE_ADAPTER_WARP);
    if (selected && MAG_HARDWARE_ADAPTER_WARP == selected->mode)
    {
      selectedItem = item;
    }

    for (i = 0; dialog && dialog->catalogValid && i < dialog->catalog.adapterCount; ++i)
    {
      const MAGADAPTERINFO* adapter = &dialog->catalog.adapters[i];
      TCHAR label[256];

      if (adapter->software)
      {
        continue;
      }
      _sntprintf_s(
        label,
        ARRAYSIZE(label),
        _TRUNCATE,
        TEXT("%s | LUID %08lX:%08lX%s"),
        adapter->description,
        (DWORD)adapter->luid.HighPart,
        adapter->luid.LowPart,
        adapter->remote ? TEXT(" | remote") : TEXT(""));
      item = mag_AddComboItem(hCtl, label, MAG_SETTINGS_EXPLICIT_ITEM_BASE + i);
      if (selected && MAG_HARDWARE_ADAPTER_EXPLICIT == selected->mode &&
          magAdapterLuidEqual(selected->adapterLuid, adapter->luid))
      {
        selectedItem = item;
      }
    }

    SendMessage(hCtl, CB_SETCURSEL, CB_ERR == selectedItem ? 0 : selectedItem, 0);
}

static BOOL mag_GetPresentationDialogSettings(
  HWND hDlg,
  const MAGSETTINGSDIALOGSTATE* dialog,
  MAGPRESENTATIONSETTINGS* settings)
{
    UINT value;
    LRESULT item;
    LRESULT itemData;

    if (!settings || !dialog)
    {
      return FALSE;
    }
    magPresentationSettingsSetDefaults(settings);
    if (!mag_GetSelectedNamedOption(hDlg, IDC_SETTINGS_PRESENT_TARGET, MAG_PRESENT_COUNT, &value))
    {
      return FALSE;
    }
    settings->target = (MAGPRESENTATIONTARGET)value;
    if (!mag_GetSelectedNamedOption(hDlg, IDC_SETTINGS_SURFACE_OWNERSHIP, MAG_SURFACE_COUNT, &value))
    {
      return FALSE;
    }
    settings->surfaceOwnership = (MAGSURFACEOWNERSHIP)value;
    if (!mag_GetSelectedNamedOption(hDlg, IDC_SETTINGS_COMPOSITION_HOST, MAG_HOST_COUNT, &value))
    {
      return FALSE;
    }
    settings->host = (MAGCOMPOSITIONHOST)value;
    if (!mag_GetSelectedNamedOption(hDlg, IDC_SETTINGS_COPY_REQUIREMENT, MAG_COPY_COUNT, &value))
    {
      return FALSE;
    }
    settings->copyRequirement = (MAGCOPYREQUIREMENT)value;
    if (!mag_GetSelectedNamedOption(hDlg, IDC_SETTINGS_ALPHA_MODE, MAG_LAYER_ALPHA_COUNT, &value))
    {
      return FALSE;
    }
    settings->alphaMode = (MAGLAYEREDALPHAMODE)value;
    if (!mag_GetSelectedNamedOption(
          hDlg,
          IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
          MAG_WAITABLE_SWAP_CHAIN_MODE_COUNT,
          &value))
    {
      return FALSE;
    }
    settings->waitableSwapChainMode = (MAGWAITABLESWAPCHAINMODE)value;

    item = SendDlgItemMessage(hDlg, IDC_SETTINGS_DISPLAY_ADAPTER, CB_GETCURSEL, 0, 0);
    itemData = CB_ERR == item ? CB_ERR : SendDlgItemMessage(
      hDlg, IDC_SETTINGS_DISPLAY_ADAPTER, CB_GETITEMDATA, (WPARAM)item, 0);
    if (CB_ERR == itemData || itemData < 0)
    {
      return FALSE;
    }
    if ((UINT_PTR)itemData >= MAG_SETTINGS_EXPLICIT_ITEM_BASE)
    {
      UINT outputIndex = (UINT)((UINT_PTR)itemData - MAG_SETTINGS_EXPLICIT_ITEM_BASE);
      const MAGOUTPUTINFO* output;

      if (!dialog || !dialog->catalogValid || outputIndex >= dialog->catalog.outputCount)
      {
        return FALSE;
      }
      output = &dialog->catalog.outputs[outputIndex];
      if (output->adapterIndex >= dialog->catalog.adapterCount)
      {
        return FALSE;
      }
      settings->display.mode = MAG_DISPLAY_ADAPTER_EXPLICIT;
      settings->display.adapterLuid = dialog->catalog.adapters[output->adapterIndex].luid;
      lstrcpyn(settings->display.deviceName, output->deviceName, ARRAYSIZE(settings->display.deviceName));
    }
    else if ((UINT)itemData < MAG_DISPLAY_ADAPTER_MODE_COUNT)
    {
      settings->display.mode = (MAGDISPLAYADAPTERMODE)itemData;
    }
    else
    {
      return FALSE;
    }

    item = SendDlgItemMessage(hDlg, IDC_SETTINGS_HARDWARE_ADAPTER, CB_GETCURSEL, 0, 0);
    itemData = CB_ERR == item ? CB_ERR : SendDlgItemMessage(
      hDlg, IDC_SETTINGS_HARDWARE_ADAPTER, CB_GETITEMDATA, (WPARAM)item, 0);
    if (CB_ERR == itemData || itemData < 0)
    {
      return FALSE;
    }
    if ((UINT_PTR)itemData >= MAG_SETTINGS_EXPLICIT_ITEM_BASE)
    {
      UINT adapterIndex = (UINT)((UINT_PTR)itemData - MAG_SETTINGS_EXPLICIT_ITEM_BASE);

      if (!dialog || !dialog->catalogValid || adapterIndex >= dialog->catalog.adapterCount)
      {
        return FALSE;
      }
      settings->hardware.mode = MAG_HARDWARE_ADAPTER_EXPLICIT;
      settings->hardware.adapterLuid = dialog->catalog.adapters[adapterIndex].luid;
    }
    else if ((UINT)itemData < MAG_HARDWARE_ADAPTER_MODE_COUNT)
    {
      settings->hardware.mode = (MAGHARDWAREADAPTERMODE)itemData;
    }
    else
    {
      return FALSE;
    }

    settings->strictTarget = BST_CHECKED == SendDlgItemMessage(
      hDlg, IDC_SETTINGS_STRICT_TARGET, BM_GETCHECK, 0, 0);
    settings->allowTearing = BST_CHECKED == SendDlgItemMessage(
      hDlg, IDC_SETTINGS_ALLOW_TEARING, BM_GETCHECK, 0, 0);
    settings->bufferCount = (UINT)SendDlgItemMessage(
      hDlg, IDC_SETTINGS_BUFFER_COUNT, TBM_GETPOS, 0, 0);
    if (settings->bufferCount < 2 || settings->bufferCount > 16)
    {
      return FALSE;
    }
    settings->maximumFrameLatency = (UINT)SendDlgItemMessage(
      hDlg, IDC_SETTINGS_FRAME_LATENCY, TBM_GETPOS, 0, 0);
    if (settings->maximumFrameLatency < 1 || settings->maximumFrameLatency > 16)
    {
      return FALSE;
    }
    settings->syncInterval = (UINT)SendDlgItemMessage(
      hDlg, IDC_SETTINGS_SYNC_INTERVAL, TBM_GETPOS, 0, 0);
    if (settings->syncInterval > 4)
    {
      return FALSE;
    }
    value = (UINT)SendDlgItemMessage(
      hDlg, IDC_SETTINGS_CONSTANT_ALPHA, TBM_GETPOS, 0, 0);
    if (value > 255)
    {
      return FALSE;
    }
    settings->constantAlpha = (BYTE)value;

    settings->colorKey = dialog->colorKey;
    return TRUE;
}

static GRAPHICSAPI mag_SettingsActiveGraphicsApi(const MAGSTATE* state)
{
    return state && state->graphicsBackend
      ? state->graphicsBackend->api
      : (state ? state->graphicsApi : GRAPHICS_API_OPENGL);
}

static CAPTUREAPI mag_SettingsActiveCaptureApi(const MAGSTATE* state)
{
    return state && state->activeCaptureApi < CAPTURE_API_COUNT
      ? state->activeCaptureApi
      : (state ? state->captureApi : CAPTURE_API_GDI_BITBLT);
}

static BOOL mag_SettingsPendingMatchesActive(
  const MAGSETTINGSDIALOGSTATE* dialog,
  const MAGSTATE* state)
{
    return dialog && state &&
      dialog->requestedGraphicsApi == mag_SettingsActiveGraphicsApi(state) &&
      dialog->requestedCaptureApi == mag_SettingsActiveCaptureApi(state) &&
      dialog->requestedUiApi == state->uiGraphicsApi &&
      dialog->requestedTextRenderer == state->textRenderer &&
      magPresentationSettingsEqual(
        &dialog->requestedPresentation,
        &state->presentationSettings);
}

static void mag_SettingsSetPendingReason(
  MAGSETTINGSDIALOGSTATE* dialog,
  LPCTSTR reason)
{
    if (dialog)
    {
      lstrcpyn(
        dialog->pendingReason,
        reason ? reason : TEXT(""),
        ARRAYSIZE(dialog->pendingReason));
    }
}

static void mag_SettingsAppendCapability(
  LPTSTR text,
  UINT textCount,
  LPCTSTR capability)
{
    if (!text || !textCount || !capability || !capability[0])
    {
      return;
    }
    if (text[0])
    {
      _tcsncat_s(text, textCount, TEXT(", "), _TRUNCATE);
    }
    _tcsncat_s(text, textCount, capability, _TRUNCATE);
}

static void mag_SettingsFormatMpoFeatures(
  const MAGPRESENTATIONSTATUS* status,
  LPTSTR text,
  UINT textCount)
{
    if (!text || !textCount)
    {
      return;
    }
    text[0] = TEXT('\0');
    if (!status || !status->overlayCapsKnown)
    {
      lstrcpyn(text, TEXT("not reported"), textCount);
      return;
    }

    if (status->overlayRotationCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("rotation"));
    if (status->overlayRotationWithoutIndependentFlip)
      mag_SettingsAppendCapability(text, textCount, TEXT("rotation without IF"));
    if (status->overlayVerticalFlipCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("vertical flip"));
    if (status->overlayHorizontalFlipCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("horizontal flip"));
    if (status->overlayStretchRgbCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("RGB stretch"));
    if (status->overlayStretchYuvCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("YUV stretch"));
    if (status->overlayBilinearFilterCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("bilinear"));
    if (status->overlayHighFilterCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("high filter"));
    if (status->overlaySharedAcrossOutputs)
      mag_SettingsAppendCapability(text, textCount, TEXT("shared across outputs"));
    if (status->overlayImmediateCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("immediate"));
    if (status->overlayPlane0ForVirtualModeOnly)
      mag_SettingsAppendCapability(text, textCount, TEXT("plane 0 virtual-only"));
    if (status->overlayVersion3DdiCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("MPO3 DDI"));
    if (status->mpoKernelCapsCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("kernel caps"));
    if (status->mpoHudKernelCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("kernel HUD"));
    if (status->mpoHudCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("HUD"));
    if (status->mpoStretchCapable)
      mag_SettingsAppendCapability(text, textCount, TEXT("stretch query"));
    if (status->displayPreferPhysicallyContiguous)
      mag_SettingsAppendCapability(text, textCount, TEXT("physical-contiguous preference"));
    if (status->displayCursorScaledWithMpoPlane0)
      mag_SettingsAppendCapability(text, textCount, TEXT("cursor scales with plane 0"));
    if (status->displayCursorNoXorWithMpo)
      mag_SettingsAppendCapability(text, textCount, TEXT("cursor XOR unavailable"));
    if (!text[0])
    {
      lstrcpyn(text, TEXT("none"), textCount);
    }
}

static LPCTSTR mag_SettingsHardwareFlipQueueName(
  const MAGPRESENTATIONSTATUS* status)
{
    if (!status || !status->wddm3CapsKnown)
    {
      return TEXT("unknown");
    }
    if (status->hardwareFlipQueueEnabled)
    {
      return TEXT("enabled");
    }
    switch (status->hardwareFlipQueueSupportState)
    {
    case 1:
      return TEXT("experimental/off");
    case 2:
      return TEXT("stable/off");
    case 3:
      return TEXT("always-on/off");
    case 0:
    default:
      return TEXT("unsupported");
    }
}

static BOOL mag_SettingsEvaluate(HWND hDlg, MAGSETTINGSDIALOGSTATE* dialog)
{
    UINT graphicsId = GRAPHICS_API_OPENGL;
    UINT captureId = CAPTURE_API_GDI_BITBLT;
    UINT uiId = UI_GRAPHICS_API_NATIVE;
    UINT textId = TEXT_RENDERER_DIRECTWRITE;
    BOOL graphicsAvailable = FALSE;
    BOOL captureAvailable = FALSE;
    BOOL uiAvailable = FALSE;
    BOOL textAvailable = FALSE;
    BOOL fieldsValid;
    BOOL presentationValid;
    BOOL resolved;

    if (!dialog)
    {
      return FALSE;
    }
    dialog->pendingFieldsValid = FALSE;
    dialog->pendingCompatible = FALSE;
    dialog->pendingApplied = FALSE;
    ZeroMemory(&dialog->pendingStatus, sizeof(dialog->pendingStatus));
    magPresentationSettingsSetDefaults(&dialog->requestedPresentation);
    dialog->resolvedPresentation = dialog->requestedPresentation;
    mag_SettingsSetPendingReason(dialog, TEXT(""));

    fieldsValid =
      mag_GetSelectedGraphicsOption(
        hDlg,
        IDC_SETTINGS_GRAPHICS_API,
        &graphicsId,
        &graphicsAvailable) &&
      mag_GetSelectedSettingsOption(
        hDlg,
        IDC_SETTINGS_CAPTURE_API,
        g_captureApiOptions,
        ARRAYSIZE(g_captureApiOptions),
        &captureId,
        &captureAvailable) &&
      mag_GetSelectedSettingsOption(
        hDlg,
        IDC_SETTINGS_UI_API,
        g_uiGraphicsApiOptions,
        ARRAYSIZE(g_uiGraphicsApiOptions),
        &uiId,
        &uiAvailable) &&
      mag_GetSelectedSettingsOption(
        hDlg,
        IDC_SETTINGS_TEXT_RENDERER,
        g_textRendererOptions,
        ARRAYSIZE(g_textRendererOptions),
        &textId,
        &textAvailable);
    presentationValid = mag_GetPresentationDialogSettings(
      hDlg,
      dialog,
      &dialog->requestedPresentation);
    if (!fieldsValid || !presentationValid)
    {
      mag_SettingsSetPendingReason(
        dialog,
        TEXT("One or more setting values could not be read."));
      return FALSE;
    }

    dialog->requestedGraphicsApi = (GRAPHICSAPI)graphicsId;
    dialog->requestedCaptureApi = (CAPTUREAPI)captureId;
    dialog->requestedUiApi = (UIGRAPHICSAPI)uiId;
    dialog->requestedTextRenderer = (TEXTRENDERER)textId;
    dialog->pendingFieldsValid = TRUE;

    if (!dialog->catalogValid)
    {
      mag_SettingsSetPendingReason(
        dialog,
        dialog->catalogReason[0]
          ? dialog->catalogReason
          : TEXT("Display and hardware adapters could not be enumerated."));
      return FALSE;
    }

    resolved = magPresentationResolve(
      dialog->owner,
      dialog->requestedGraphicsApi,
      dialog->requestedCaptureApi,
      dialog->requestedUiApi,
      dialog->requestedTextRenderer,
      &dialog->requestedPresentation,
      &dialog->catalog,
      &dialog->resolvedPresentation,
      &dialog->pendingStatus);

    if (!graphicsAvailable)
    {
      const MAGGRAPHICSBACKEND* backend = magGraphicsGetBackend(
        dialog->requestedGraphicsApi);
      TCHAR availabilityReason[MAG_PRESENTATION_REASON_LENGTH] = TEXT("");

      if (backend && backend->IsAvailable)
      {
        backend->IsAvailable(
          availabilityReason,
          ARRAYSIZE(availabilityReason));
      }
      mag_SettingsSetPendingReason(
        dialog,
        availabilityReason[0]
          ? availabilityReason
          : TEXT("The selected graphics renderer is unavailable on this system."));
      return FALSE;
    }
    if (!captureAvailable || !uiAvailable || !textAvailable)
    {
      mag_SettingsSetPendingReason(
        dialog,
        TEXT("The selected capture, UI, or text renderer is unavailable on this system."));
      return FALSE;
    }
    if (!resolved || !dialog->pendingStatus.configurationSupported ||
        !dialog->pendingStatus.flickerFree)
    {
      mag_SettingsSetPendingReason(
        dialog,
        dialog->pendingStatus.reason[0]
          ? dialog->pendingStatus.reason
          : TEXT("This route cannot guarantee flicker-free presentation."));
      return FALSE;
    }

    dialog->pendingCompatible = TRUE;
    mag_SettingsSetPendingReason(dialog, dialog->pendingStatus.reason);
    return TRUE;
}

static BOOL mag_SettingsRefresh(HWND hDlg, BOOL applyIfCompatible)
{
    MAGSETTINGSDIALOGSTATE* dialog =
      (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
    LPMAGSTATE state = dialog
      ? (LPMAGSTATE)GetWindowLongPtr(dialog->owner, GWLP_USERDATA)
      : NULL;
    BOOL compatible = mag_SettingsEvaluate(hDlg, dialog);
    BOOL routeChanged = FALSE;
    BOOL mouseChanged = FALSE;

    if (compatible && state)
    {
      routeChanged = !mag_SettingsPendingMatchesActive(dialog, state);
      mouseChanged = state->fMouseRelativeZoom !=
        (BST_CHECKED == SendDlgItemMessage(
          hDlg,
          IDC_SETTINGS_MOUSE_RELATIVE_ZOOM,
          BM_GETCHECK,
          0,
          0));
      if (applyIfCompatible && routeChanged)
      {
        TCHAR applyReason[MAG_PRESENTATION_REASON_LENGTH] = TEXT("");

        if (!renderApplyFullSettings(
              dialog->owner,
              dialog->requestedGraphicsApi,
              dialog->requestedCaptureApi,
              dialog->requestedUiApi,
              dialog->requestedTextRenderer,
              &dialog->requestedPresentation,
              applyReason,
              ARRAYSIZE(applyReason)))
        {
          dialog->pendingCompatible = FALSE;
          compatible = FALSE;
          mag_SettingsSetPendingReason(
            dialog,
            applyReason[0]
              ? applyReason
              : TEXT("The compatible route could not be activated; the active route was retained."));
        }
      }
      if (compatible && applyIfCompatible)
      {
        state->fMouseRelativeZoom = BST_CHECKED == SendDlgItemMessage(
          hDlg,
          IDC_SETTINGS_MOUSE_RELATIVE_ZOOM,
          BM_GETCHECK,
          0,
          0);
        if (!state->fMouseRelativeZoom && !state->fSourceOriginPinned)
        {
          state->fUseSourceOrigin = FALSE;
        }
        state->fAutoSettingsPreset = dialog->autoPreset;
        mag_SaveSettings(state);
        if (routeChanged || mouseChanged)
        {
          renderRender(dialog->owner);
        }
      }
      dialog->pendingApplied = compatible &&
        mag_SettingsPendingMatchesActive(dialog, state);
    }

    if (!dialog)
    {
      return FALSE;
    }
    if (dialog->pendingApplied)
    {
      TCHAR summary[1800];
      const MAGPRESENTATIONSTATUS* activeStatus = state
        ? &state->presentationStatus
        : &dialog->pendingStatus;
      const MAGPRESENTATIONSETTINGS* activeResolved = state
        ? &state->resolvedPresentation
        : &dialog->resolvedPresentation;
      const MAGPRESENTATIONSETTINGS* activeRequested = state
        ? &state->presentationSettings
        : &dialog->requestedPresentation;
      TCHAR mode[360];
      TCHAR planes[420];
      TCHAR planeDetails[520];
      TCHAR mpoFeatures[320];
      TCHAR mpoScale[80];
      TCHAR postScale[80];
      TCHAR scanout[48];
      TCHAR pacing[192];

      if (activeStatus->observedTargetValid)
      {
        _sntprintf_s(
          mode,
          ARRAYSIZE(mode),
          _TRUNCATE,
          TEXT("Observed: %s (source: %s)%s."),
          magPresentationTargetName(activeStatus->observedTarget),
          magPresentationObservationSourceName(
            activeStatus->observationSource),
          activeStatus->strictTargetSatisfied
            ? TEXT("")
            : TEXT(" (requested exact result not active)"));
      }
      else if (MAG_PRESENT_OBSERVATION_PRESENTATION_MANAGER ==
               activeStatus->observationSource)
      {
        _sntprintf_s(
          mode,
          ARRAYSIZE(mode),
          _TRUNCATE,
          TEXT("Configured: %s; observed: pending (Presentation Manager statistics)."),
          magPresentationTargetName(activeResolved->target));
      }
      else if (activeStatus->observationAvailable)
      {
        _sntprintf_s(
          mode,
          ARRAYSIZE(mode),
          _TRUNCATE,
          TEXT("Configured: %s; observed: pending (%s ETW: %s%s)."),
          magPresentationTargetName(activeResolved->target),
          activeStatus->observationFullFidelity
            ? TEXT("full-fidelity")
            : TEXT("partial"),
          activeStatus->observationDxgKrnl ? TEXT("DxgKrnl") : TEXT(""),
          activeStatus->observationWin32k
            ? (activeStatus->observationDxgKrnl
                ? TEXT(" + Win32k")
                : TEXT("Win32k"))
            : TEXT(""));
      }
      else
      {
        _sntprintf_s(
          mode,
          ARRAYSIZE(mode),
          _TRUNCATE,
          TEXT("Configured: %s; observed: unavailable (StartTrace %lu). Eligibility is not an active-plane claim."),
          magPresentationTargetName(activeResolved->target),
          activeStatus->observationError);
      }
      if (activeStatus->displayPlaneCapsKnown)
      {
        mag_SettingsFormatMpoFeatures(
          activeStatus,
          mpoFeatures,
          ARRAYSIZE(mpoFeatures));
        if (activeStatus->overlayCapsKnown)
        {
          _sntprintf_s(
            mpoScale,
            ARRAYSIZE(mpoScale),
            _TRUNCATE,
            TEXT("shrink %.2f / stretch %.2f"),
            activeStatus->maxShrinkFactor,
            activeStatus->maxStretchFactor);
        }
        else
        {
          lstrcpyn(mpoScale, TEXT("not reported"), ARRAYSIZE(mpoScale));
        }
        if (activeStatus->postCompositionCapsKnown)
        {
          _sntprintf_s(
            postScale,
            ARRAYSIZE(postScale),
            _TRUNCATE,
            TEXT("shrink %.2f / stretch %.2f"),
            activeStatus->postCompositionMaxShrinkFactor,
            activeStatus->postCompositionMaxStretchFactor);
        }
        else
        {
          lstrcpyn(postScale, TEXT("not reported"), ARRAYSIZE(postScale));
        }
        if (activeStatus->scanoutCapsKnown)
        {
          _sntprintf_s(
            scanout,
            ARRAYSIZE(scanout),
            _TRUNCATE,
            TEXT("0x%08X"),
            activeStatus->scanoutCaps);
        }
        else
        {
          lstrcpyn(scanout, TEXT("not reported"), ARRAYSIZE(scanout));
        }
        _sntprintf_s(
          planes,
          ARRAYSIZE(planes),
          _TRUNCATE,
          TEXT("Output: DirectFlip %s; IF %s (secondary %s); MPO %s, %u planes (%u RGB/%u YUV; decode %s; secondary %s); panel fitter %s; geometry %s%s."),
          activeStatus->directFlipCapable ? TEXT("capable") : TEXT("unavailable"),
          activeStatus->independentFlipCapable ? TEXT("capable") : TEXT("unavailable"),
          activeStatus->independentFlipSecondaryCapable
            ? TEXT("capable")
            : TEXT("unavailable"),
          activeStatus->multiPlaneOverlayCapable
            ? TEXT("capable")
            : TEXT("unavailable"),
          activeStatus->overlayCapsKnown ? activeStatus->maxPlanes : 0,
          activeStatus->maxRgbPlanes,
          activeStatus->maxYuvPlanes,
          activeStatus->multiPlaneOverlayDecodeCapable
            ? TEXT("yes")
            : TEXT("no"),
          activeStatus->mpoSecondaryCapable ? TEXT("capable") : TEXT("unavailable"),
          activeStatus->panelFitterCapable ? TEXT("capable") : TEXT("unavailable"),
          activeStatus->directFlipGeometryEligible
            ? TEXT("covers output")
            : TEXT("windowed/subregion"),
          activeStatus->hardwareCompositionCursorStretchRisk
            ? TEXT("; hardware scaling may stretch the cursor")
            : TEXT(""));
        _sntprintf_s(
          planeDetails,
          ARRAYSIZE(planeDetails),
          _TRUNCATE,
          TEXT("MPO features: %s; MPO scale %s; post-composition %s; displayable %s; HW flip queue %s; cross-adapter %s; scanout caps %s."),
          mpoFeatures,
          mpoScale,
          postScale,
          activeStatus->wddm3CapsKnown
            ? (activeStatus->displayableSupported ? TEXT("yes") : TEXT("no"))
            : TEXT("unknown"),
          mag_SettingsHardwareFlipQueueName(activeStatus),
          activeStatus->crossAdapterSupportKnown
            ? magCrossAdapterSupportTierName(
                activeStatus->crossAdapterSupportTier)
            : TEXT("unknown"),
          scanout);
      }
      else
      {
        lstrcpyn(
          planes,
          TEXT("Output plane capabilities were not reported by the display stack."),
          ARRAYSIZE(planes));
        lstrcpyn(
          planeDetails,
          TEXT("DirectFlip, IF, MPO, scanout, and WDDM displayable eligibility remain unknown."),
          ARRAYSIZE(planeDetails));
      }
      _sntprintf_s(
        pacing,
        ARRAYSIZE(pacing),
        _TRUNCATE,
        TEXT("Pacing: waitable %s%s; %u buffers; %s %u; sync %u; tearing %s."),
        MAG_WAITABLE_SWAP_CHAIN_AUTO ==
            activeRequested->waitableSwapChainMode
          ? TEXT("Auto > ")
          : TEXT(""),
        mag_SettingsShortWaitable(activeResolved->waitableSwapChainMode),
        activeRequested->bufferCount,
        GRAPHICS_API_D3D12 == mag_SettingsActiveGraphicsApi(state) &&
          MAG_WAITABLE_SWAP_CHAIN_DISABLED ==
            activeResolved->waitableSwapChainMode
          ? TEXT("driver-managed latency; request")
          : TEXT("maximum latency"),
        activeRequested->maximumFrameLatency,
        activeRequested->syncInterval,
        activeRequested->allowTearing ? TEXT("allowed") : TEXT("off"));
      _sntprintf_s(
        summary,
        ARRAYSIZE(summary),
        _TRUNCATE,
        TEXT("%s\r\n%s\r\n%s\r\n%s\r\n%s"),
        mode,
        planes,
        planeDetails,
        pacing,
        activeStatus->reason);
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, summary);
    }
    else if (dialog->pendingCompatible)
    {
      SetDlgItemText(
        hDlg,
        IDC_SETTINGS_STATUS,
        TEXT("Compatible request; the active route has not changed."));
    }
    else
    {
      TCHAR unavailable[MAG_PRESENTATION_REASON_LENGTH];

      _sntprintf_s(
        unavailable,
        ARRAYSIZE(unavailable),
        _TRUNCATE,
        TEXT("Unavailable because: %s"),
        dialog->pendingReason[0]
          ? dialog->pendingReason
          : TEXT("The requested route is incompatible."));
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, unavailable);
    }
    if (applyIfCompatible || 0 == dialog->optionCounts[0])
    {
      mag_SettingsUpdateAvailabilitySummary(hDlg, dialog);
    }
    mag_SettingsRedrawRoute(hDlg);
    return dialog->pendingApplied;
}

void mag_UpdateSettingsDialogState(HWND hDlg)
{
    mag_SettingsRefresh(hDlg, FALSE);
}

typedef struct MAGSETTINGSPAGECONTROLS
{
  const UINT* ids;
  UINT count;
} MAGSETTINGSPAGECONTROLS;

enum
{
  MAG_SETTINGS_PAGE_ROUTE,
  MAG_SETTINGS_PAGE_RENDERING,
  MAG_SETTINGS_PAGE_PRESENTATION,
  MAG_SETTINGS_PAGE_ADAPTERS,
  MAG_SETTINGS_PAGE_LAYERING,
  MAG_SETTINGS_PAGE_PACING,
  MAG_SETTINGS_PAGE_COUNT,
};

static const UINT g_settingsRouteControls[] =
{
  IDC_SETTINGS_LABEL_PRESET,
  IDC_SETTINGS_PRESET,
  IDC_SETTINGS_ROUTE_DIAGRAM,
  IDC_SETTINGS_ROUTE_PACING,
  IDC_SETTINGS_ROUTE_LAYERING,
};

static const UINT g_settingsRouteSharedControls[] =
{
  IDC_SETTINGS_CAPTURE_API,
  IDC_SETTINGS_COPY_REQUIREMENT,
  IDC_SETTINGS_GRAPHICS_API,
  IDC_SETTINGS_HARDWARE_ADAPTER,
  IDC_SETTINGS_SURFACE_OWNERSHIP,
  IDC_SETTINGS_TEXT_RENDERER,
  IDC_SETTINGS_UI_API,
  IDC_SETTINGS_COMPOSITION_HOST,
  IDC_SETTINGS_PRESENT_TARGET,
  IDC_SETTINGS_DISPLAY_ADAPTER,
};

enum
{
  MAG_SETTINGS_ROUTE_WIDTH = 497,
  MAG_SETTINGS_ROUTE_HEIGHT = 158,
  MAG_SETTINGS_ROUTE_NODE_WIDTH = 73,
  MAG_SETTINGS_ROUTE_NODE_HEIGHT = 30,
};

static const int g_settingsRouteNodeX[12] =
{
  5, 87, 169, 87, 169, 251, 333, 415, 5, 87, 251, 169,
};

static const int g_settingsRouteNodeY[12] =
{
  4, 4, 4, 48, 48, 4, 4, 4, 92, 92, 124, 124,
};

static const UINT g_settingsRouteNodeControls[12] =
{
  IDC_SETTINGS_CAPTURE_API,
  IDC_SETTINGS_COPY_REQUIREMENT,
  IDC_SETTINGS_GRAPHICS_API,
  IDC_SETTINGS_HARDWARE_ADAPTER,
  IDC_SETTINGS_SURFACE_OWNERSHIP,
  IDC_SETTINGS_COMPOSITION_HOST,
  IDC_SETTINGS_PRESENT_TARGET,
  IDC_SETTINGS_DISPLAY_ADAPTER,
  IDC_SETTINGS_TEXT_RENDERER,
  IDC_SETTINGS_UI_API,
  IDC_SETTINGS_ROUTE_PACING,
  IDC_SETTINGS_ROUTE_LAYERING,
};

static void mag_SettingsActivateRouteNode(HWND hDlg)
{
    HWND diagram = GetDlgItem(hDlg, IDC_SETTINGS_ROUTE_DIAGRAM);
    RECT client;
    POINT point;
    int x;
    int y;
    UINT node;

    if (!diagram || !GetClientRect(diagram, &client) ||
        RECTWIDTH(client) <= 0 || RECTHEIGHT(client) <= 0 ||
        !GetCursorPos(&point) || !ScreenToClient(diagram, &point))
    {
      return;
    }
    x = MulDiv(point.x, MAG_SETTINGS_ROUTE_WIDTH, RECTWIDTH(client));
    y = MulDiv(point.y, MAG_SETTINGS_ROUTE_HEIGHT, RECTHEIGHT(client));
    for (node = 0; node < ARRAYSIZE(g_settingsRouteNodeControls); ++node)
    {
      if (x >= g_settingsRouteNodeX[node] &&
          x <= g_settingsRouteNodeX[node] + MAG_SETTINGS_ROUTE_NODE_WIDTH &&
          y >= g_settingsRouteNodeY[node] &&
          y <= g_settingsRouteNodeY[node] + MAG_SETTINGS_ROUTE_NODE_HEIGHT)
      {
        HWND control = GetDlgItem(hDlg, g_settingsRouteNodeControls[node]);

        if (!control)
        {
          return;
        }
        if (IDC_SETTINGS_ROUTE_PACING == g_settingsRouteNodeControls[node] ||
            IDC_SETTINGS_ROUTE_LAYERING == g_settingsRouteNodeControls[node])
        {
          SendMessage(control, BM_CLICK, 0, 0);
        }
        else
        {
          SetFocus(control);
          SendMessage(control, CB_SHOWDROPDOWN, TRUE, 0);
        }
        return;
      }
    }
}

static void mag_SettingsRedrawRoute(HWND hDlg)
{
    static const UINT foregroundIds[] =
    {
      IDC_SETTINGS_CAPTURE_API,
      IDC_SETTINGS_COPY_REQUIREMENT,
      IDC_SETTINGS_GRAPHICS_API,
      IDC_SETTINGS_HARDWARE_ADAPTER,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      IDC_SETTINGS_TEXT_RENDERER,
      IDC_SETTINGS_UI_API,
      IDC_SETTINGS_COMPOSITION_HOST,
      IDC_SETTINGS_PRESENT_TARGET,
      IDC_SETTINGS_DISPLAY_ADAPTER,
      IDC_SETTINGS_ROUTE_LAYERING,
      IDC_SETTINGS_ROUTE_PACING,
    };
    HWND diagram = GetDlgItem(hDlg, IDC_SETTINGS_ROUTE_DIAGRAM);
    UINT control;

    if (!diagram || !IsWindowVisible(diagram))
    {
      return;
    }
    /* The graph is an owner-drawn sibling behind native, keyboard-accessible
       setting controls. Paint it first, then the controls that act as nodes. */
    RedrawWindow(
      diagram,
      NULL,
      NULL,
      RDW_INVALIDATE | RDW_ERASE | RDW_UPDATENOW);
    for (control = 0; control < ARRAYSIZE(foregroundIds); ++control)
    {
      HWND foreground = GetDlgItem(hDlg, foregroundIds[control]);

      if (foreground && IsWindowVisible(foreground))
      {
        RedrawWindow(
          foreground,
          NULL,
          NULL,
          RDW_INVALIDATE | RDW_ERASE | RDW_FRAME | RDW_UPDATENOW);
      }
    }
}

static const UINT g_settingsRenderingControls[] =
{
  IDC_SETTINGS_LABEL_GRAPHICS_API,
  IDC_SETTINGS_GRAPHICS_API,
  IDC_SETTINGS_LABEL_CAPTURE_API,
  IDC_SETTINGS_CAPTURE_API,
  IDC_SETTINGS_LABEL_UI_API,
  IDC_SETTINGS_UI_API,
  IDC_SETTINGS_LABEL_TEXT_RENDERER,
  IDC_SETTINGS_TEXT_RENDERER,
};

static const UINT g_settingsPresentationControls[] =
{
  IDC_SETTINGS_LABEL_PRESENT_TARGET,
  IDC_SETTINGS_PRESENT_TARGET,
  IDC_SETTINGS_LABEL_SURFACE_OWNERSHIP,
  IDC_SETTINGS_SURFACE_OWNERSHIP,
  IDC_SETTINGS_LABEL_COMPOSITION_HOST,
  IDC_SETTINGS_COMPOSITION_HOST,
  IDC_SETTINGS_LABEL_COPY_REQUIREMENT,
  IDC_SETTINGS_COPY_REQUIREMENT,
  IDC_SETTINGS_STRICT_TARGET,
  IDC_SETTINGS_LABEL_STRICT_TARGET,
  IDC_SETTINGS_ALLOW_TEARING,
  IDC_SETTINGS_LABEL_ALLOW_TEARING,
};

static const UINT g_settingsAdapterControls[] =
{
  IDC_SETTINGS_LABEL_DISPLAY_ADAPTER,
  IDC_SETTINGS_DISPLAY_ADAPTER,
  IDC_SETTINGS_LABEL_HARDWARE_ADAPTER,
  IDC_SETTINGS_HARDWARE_ADAPTER,
};

static const UINT g_settingsLayeringControls[] =
{
  IDC_SETTINGS_LABEL_ALPHA_MODE,
  IDC_SETTINGS_ALPHA_MODE,
  IDC_SETTINGS_LABEL_CONSTANT_ALPHA,
  IDC_SETTINGS_CONSTANT_ALPHA,
  IDC_SETTINGS_VALUE_CONSTANT_ALPHA,
  IDC_SETTINGS_LABEL_COLOR_KEY,
  IDC_SETTINGS_COLOR_KEY,
};

static const UINT g_settingsPacingControls[] =
{
  IDC_SETTINGS_LABEL_BUFFER_COUNT,
  IDC_SETTINGS_BUFFER_COUNT,
  IDC_SETTINGS_VALUE_BUFFER_COUNT,
  IDC_SETTINGS_LABEL_FRAME_LATENCY,
  IDC_SETTINGS_FRAME_LATENCY,
  IDC_SETTINGS_VALUE_FRAME_LATENCY,
  IDC_SETTINGS_LABEL_SYNC_INTERVAL,
  IDC_SETTINGS_SYNC_INTERVAL,
  IDC_SETTINGS_VALUE_SYNC_INTERVAL,
  IDC_SETTINGS_LABEL_WAITABLE_SWAP_CHAIN,
  IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
  IDC_SETTINGS_MOUSE_RELATIVE_ZOOM,
  IDC_SETTINGS_LABEL_MOUSE_RELATIVE_ZOOM,
};

static const MAGSETTINGSPAGECONTROLS g_settingsPages[] =
{
  { g_settingsRouteControls, ARRAYSIZE(g_settingsRouteControls) },
  { g_settingsRenderingControls, ARRAYSIZE(g_settingsRenderingControls) },
  { g_settingsPresentationControls, ARRAYSIZE(g_settingsPresentationControls) },
  { g_settingsAdapterControls, ARRAYSIZE(g_settingsAdapterControls) },
  { g_settingsLayeringControls, ARRAYSIZE(g_settingsLayeringControls) },
  { g_settingsPacingControls, ARRAYSIZE(g_settingsPacingControls) },
};

static void mag_SettingsMoveControlDlu(
  HWND hDlg,
  UINT controlId,
  int x,
  int y,
  int width,
  int height)
{
    RECT bounds = { x, y, x + width, y + height };
    HWND hControl = GetDlgItem(hDlg, controlId);

    if (hControl && MapDialogRect(hDlg, &bounds))
    {
      SetWindowPos(
        hControl,
        NULL,
        bounds.left,
        bounds.top,
        RECTWIDTH(bounds),
        RECTHEIGHT(bounds),
        SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOZORDER);
    }
}

static void mag_SettingsLayoutPage(HWND hDlg, UINT selectedPage)
{
    static const UINT comboIds[] =
    {
      IDC_SETTINGS_PRESET,
      IDC_SETTINGS_GRAPHICS_API,
      IDC_SETTINGS_CAPTURE_API,
      IDC_SETTINGS_UI_API,
      IDC_SETTINGS_TEXT_RENDERER,
      IDC_SETTINGS_PRESENT_TARGET,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      IDC_SETTINGS_COMPOSITION_HOST,
      IDC_SETTINGS_COPY_REQUIREMENT,
      IDC_SETTINGS_DISPLAY_ADAPTER,
      IDC_SETTINGS_HARDWARE_ADAPTER,
      IDC_SETTINGS_ALPHA_MODE,
      IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
    };
    UINT combo;

    switch (selectedPage)
    {
    case MAG_SETTINGS_PAGE_ROUTE:
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_CAPTURE_API, 16, 50, 71, 68);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_COPY_REQUIREMENT, 98, 50, 71, 60);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_GRAPHICS_API, 180, 50, 71, 60);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_COMPOSITION_HOST, 262, 50, 71, 68);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_PRESENT_TARGET, 344, 50, 71, 84);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_DISPLAY_ADAPTER, 426, 50, 71, 100);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_HARDWARE_ADAPTER, 98, 94, 71, 100);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_SURFACE_OWNERSHIP, 180, 94, 71, 52);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_TEXT_RENDERER, 16, 138, 71, 60);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_UI_API, 98, 138, 71, 52);
      break;
    case MAG_SETTINGS_PAGE_RENDERING:
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_GRAPHICS_API, 66, 17, 442, 60);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_CAPTURE_API, 66, 33, 442, 68);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_UI_API, 66, 49, 442, 52);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_TEXT_RENDERER, 66, 65, 442, 60);
      break;
    case MAG_SETTINGS_PAGE_PRESENTATION:
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_PRESENT_TARGET, 66, 17, 442, 84);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_SURFACE_OWNERSHIP, 66, 33, 442, 52);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_COMPOSITION_HOST, 66, 49, 442, 68);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_COPY_REQUIREMENT, 66, 65, 442, 60);
      break;
    case MAG_SETTINGS_PAGE_ADAPTERS:
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_DISPLAY_ADAPTER, 66, 28, 442, 100);
      mag_SettingsMoveControlDlu(hDlg, IDC_SETTINGS_HARDWARE_ADAPTER, 66, 52, 442, 100);
      break;
    default:
      break;
    }
    for (combo = 0; combo < ARRAYSIZE(comboIds); ++combo)
    {
      mag_SettingsFitComboDropDown(hDlg, comboIds[combo]);
    }
}

static void mag_SettingsShowPage(HWND hDlg, UINT selectedPage)
{
    UINT page;

    if (selectedPage >= ARRAYSIZE(g_settingsPages))
    {
      selectedPage = MAG_SETTINGS_PAGE_RENDERING;
    }
    SendMessage(hDlg, WM_SETREDRAW, FALSE, 0);
    mag_SettingsLayoutPage(hDlg, selectedPage);
    for (page = 0; page < ARRAYSIZE(g_settingsPages); ++page)
    {
      UINT control;
      const MAGSETTINGSPAGECONTROLS* controls = &g_settingsPages[page];

      for (control = 0; control < controls->count; ++control)
      {
        HWND hControl = GetDlgItem(hDlg, controls->ids[control]);

        if (hControl)
        {
          ShowWindow(hControl, selectedPage == page ? SW_SHOW : SW_HIDE);
        }
      }
    }
    if (MAG_SETTINGS_PAGE_ROUTE == selectedPage)
    {
      UINT control;

      for (control = 0;
           control < ARRAYSIZE(g_settingsRouteSharedControls);
           ++control)
      {
        HWND hControl = GetDlgItem(
          hDlg,
          g_settingsRouteSharedControls[control]);

        if (hControl)
        {
          ShowWindow(hControl, SW_SHOW);
        }
      }
    }
    SendMessage(hDlg, WM_SETREDRAW, TRUE, 0);
    RedrawWindow(
      hDlg,
      NULL,
      NULL,
      RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    if (MAG_SETTINGS_PAGE_ROUTE == selectedPage)
    {
      mag_SettingsRedrawRoute(hDlg);
    }
}

static void mag_SettingsReleaseRouteDrawingResources(
  MAGSETTINGSDIALOGSTATE* dialog)
{
    if (!dialog)
    {
      return;
    }
    if (dialog->routeHeaderFormat)
    {
      dialog->routeHeaderFormat->lpVtbl->Release(dialog->routeHeaderFormat);
      dialog->routeHeaderFormat = NULL;
    }
    if (dialog->routeNodeFormat)
    {
      dialog->routeNodeFormat->lpVtbl->Release(dialog->routeNodeFormat);
      dialog->routeNodeFormat = NULL;
    }
    if (dialog->routeValueFormat)
    {
      dialog->routeValueFormat->lpVtbl->Release(dialog->routeValueFormat);
      dialog->routeValueFormat = NULL;
    }
    if (dialog->routeDwriteFactory)
    {
      dialog->routeDwriteFactory->lpVtbl->Release(dialog->routeDwriteFactory);
      dialog->routeDwriteFactory = NULL;
    }
    if (dialog->routeD2dBrush)
    {
      dialog->routeD2dBrush->lpVtbl->Release(dialog->routeD2dBrush);
      dialog->routeD2dBrush = NULL;
    }
    if (dialog->routeD2dTarget)
    {
      dialog->routeD2dTarget->lpVtbl->Release(dialog->routeD2dTarget);
      dialog->routeD2dTarget = NULL;
    }
    if (dialog->routeD2dFactory)
    {
      dialog->routeD2dFactory->lpVtbl->Release(dialog->routeD2dFactory);
      dialog->routeD2dFactory = NULL;
    }
}

static BOOL mag_SettingsCreateRouteDrawingResources(
  HWND hDlg,
  MAGSETTINGSDIALOGSTATE* dialog)
{
    D2D1_RENDER_TARGET_PROPERTIES properties;
    D2D1_COLOR_F color = { 0.0f, 0.0f, 0.0f, 1.0f };
    const FLOAT textScale =
      GetDpiForWindow(hDlg) / (FLOAT)USER_DEFAULT_SCREEN_DPI;
    HRESULT hr;

    if (!dialog)
    {
      return FALSE;
    }
    ZeroMemory(&properties, sizeof(properties));
    properties.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
    properties.pixelFormat.format = MAG_DXGI_FORMAT_B8G8R8A8_UNORM;
    properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;
    properties.dpiX = 96.0f;
    properties.dpiY = 96.0f;
    hr = D2D1CreateFactory(
      D2D1_FACTORY_TYPE_SINGLE_THREADED,
      &IID_MAG_SETTINGS_ID2D1Factory,
      NULL,
      (void**)&dialog->routeD2dFactory);
    if (SUCCEEDED(hr))
    {
      hr = dialog->routeD2dFactory->lpVtbl->CreateDCRenderTarget(
        dialog->routeD2dFactory,
        &properties,
        &dialog->routeD2dTarget);
    }
    if (SUCCEEDED(hr))
    {
      hr = dialog->routeD2dTarget->lpVtbl->CreateSolidColorBrush(
        dialog->routeD2dTarget,
        &color,
        NULL,
        &dialog->routeD2dBrush);
    }
    if (SUCCEEDED(hr))
    {
      hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        &IID_MAG_SETTINGS_IDWriteFactory,
        (IUnknown**)&dialog->routeDwriteFactory);
    }
    if (SUCCEEDED(hr))
    {
      hr = dialog->routeDwriteFactory->lpVtbl->CreateTextFormat(
        dialog->routeDwriteFactory,
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        9.5f * textScale,
        L"en-us",
        &dialog->routeNodeFormat);
    }
    if (SUCCEEDED(hr))
    {
      hr = dialog->routeDwriteFactory->lpVtbl->CreateTextFormat(
        dialog->routeDwriteFactory,
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        8.5f * textScale,
        L"en-us",
        &dialog->routeValueFormat);
    }
    if (SUCCEEDED(hr))
    {
      hr = dialog->routeDwriteFactory->lpVtbl->CreateTextFormat(
        dialog->routeDwriteFactory,
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_SEMI_BOLD,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f * textScale,
        L"en-us",
        &dialog->routeHeaderFormat);
    }
    if (SUCCEEDED(hr))
    {
      dialog->routeNodeFormat->lpVtbl->SetWordWrapping(
        dialog->routeNodeFormat,
        DWRITE_WORD_WRAPPING_NO_WRAP);
      dialog->routeNodeFormat->lpVtbl->SetTextAlignment(
        dialog->routeNodeFormat,
        DWRITE_TEXT_ALIGNMENT_LEADING);
      dialog->routeNodeFormat->lpVtbl->SetParagraphAlignment(
        dialog->routeNodeFormat,
        DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      dialog->routeValueFormat->lpVtbl->SetWordWrapping(
        dialog->routeValueFormat,
        DWRITE_WORD_WRAPPING_WRAP);
      dialog->routeValueFormat->lpVtbl->SetTextAlignment(
        dialog->routeValueFormat,
        DWRITE_TEXT_ALIGNMENT_LEADING);
      dialog->routeValueFormat->lpVtbl->SetParagraphAlignment(
        dialog->routeValueFormat,
        DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
      dialog->routeHeaderFormat->lpVtbl->SetWordWrapping(
        dialog->routeHeaderFormat,
        DWRITE_WORD_WRAPPING_NO_WRAP);
      dialog->routeHeaderFormat->lpVtbl->SetTextAlignment(
        dialog->routeHeaderFormat,
        DWRITE_TEXT_ALIGNMENT_LEADING);
      dialog->routeHeaderFormat->lpVtbl->SetParagraphAlignment(
        dialog->routeHeaderFormat,
        DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
      return TRUE;
    }
    mag_SettingsReleaseRouteDrawingResources(dialog);
    return FALSE;
}

static BOOL mag_Settings_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
    static const LPCTSTR pageTitles[MAG_SETTINGS_PAGE_COUNT] =
    {
      TEXT("Route"),
      TEXT("Rendering"),
      TEXT("Present"),
      TEXT("Adapters"),
      TEXT("Layering"),
      TEXT("Pacing"),
    };
    static const UINT comboIds[] =
    {
      IDC_SETTINGS_PRESET,
      IDC_SETTINGS_GRAPHICS_API,
      IDC_SETTINGS_CAPTURE_API,
      IDC_SETTINGS_UI_API,
      IDC_SETTINGS_TEXT_RENDERER,
      IDC_SETTINGS_PRESENT_TARGET,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      IDC_SETTINGS_COMPOSITION_HOST,
      IDC_SETTINGS_COPY_REQUIREMENT,
      IDC_SETTINGS_DISPLAY_ADAPTER,
      IDC_SETTINGS_HARDWARE_ADAPTER,
      IDC_SETTINGS_ALPHA_MODE,
      IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
    };
    HWND hOwner = (HWND)lParam;
    HWND hTab = GetDlgItem(hDlg, IDC_SETTINGS_TAB);
    LPMAGSTATE lpsd;
    MAGSETTINGSDIALOGSTATE* dialog;
    MAGPRESENTATIONSETTINGS defaults;
    const MAGPRESENTATIONSETTINGS* presentation;
    UINT graphicsApi;
    UINT captureApi;
    UINT uiApi;
    UINT textRenderer;
    UINT page;

    UNREFERENCED_PARAMETER(hwndFocus);

    dialog = (MAGSETTINGSDIALOGSTATE*)HeapAlloc(
      GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*dialog));
    if (!dialog)
    {
      EndDialog(hDlg, IDCANCEL);
      return TRUE;
    }
    dialog->owner = hOwner;
    dialog->updatingControls = TRUE;
    {
      HFONT dialogFont = (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0);
      LOGFONT fontDescription;

      if (dialogFont && sizeof(fontDescription) ==
            GetObject(dialogFont, sizeof(fontDescription), &fontDescription))
      {
        fontDescription.lfHeight = -MulDiv(
          7,
          (int)GetDpiForWindow(hDlg),
          72);
        fontDescription.lfWeight = FW_NORMAL;
        dialog->routeFont = CreateFontIndirect(&fontDescription);
      }
    }
    mag_SettingsCreateRouteDrawingResources(hDlg, dialog);
    dialog->catalogValid = magAdapterCatalogEnumerate(
      &dialog->catalog,
      dialog->catalogReason,
      ARRAYSIZE(dialog->catalogReason));
    SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)dialog);
    for (page = 0; hTab && page < ARRAYSIZE(pageTitles); ++page)
    {
      TCITEM item = { 0 };

      item.mask = TCIF_TEXT;
      item.pszText = (LPTSTR)pageTitles[page];
      TabCtrl_InsertItem(hTab, page, &item);
    }
    if (hTab)
    {
      TabCtrl_SetCurSel(hTab, MAG_SETTINGS_PAGE_ROUTE);
    }
    lpsd = (LPMAGSTATE)GetWindowLongPtr(hOwner, GWLP_USERDATA);
    {
      HWND hPreset = GetDlgItem(hDlg, IDC_SETTINGS_PRESET);
      LRESULT item;

      item = mag_AddComboItem(
        hPreset,
        TEXT("Auto (presentation preset)"),
        MAG_SETTINGS_PRESET_AUTO);
      UNREFERENCED_PARAMETER(item);
      mag_AddComboItem(
        hPreset,
        TEXT("Custom"),
        MAG_SETTINGS_PRESET_CUSTOM);
      mag_SettingsSetPresetSelection(
        hDlg,
        dialog,
        lpsd && lpsd->fAutoSettingsPreset);
    }
    graphicsApi = lpsd
      ? mag_SettingsActiveGraphicsApi(lpsd)
      : GRAPHICS_API_OPENGL;
    captureApi = lpsd
      ? mag_SettingsActiveCaptureApi(lpsd)
      : CAPTURE_API_GDI_BITBLT;
    uiApi = (lpsd && lpsd->uiGraphicsApi < UI_GRAPHICS_API_COUNT) ? lpsd->uiGraphicsApi : UI_GRAPHICS_API_NATIVE;
    textRenderer = (lpsd && lpsd->textRenderer < TEXT_RENDERER_COUNT) ? lpsd->textRenderer : TEXT_RENDERER_DIRECTWRITE;
    magPresentationSettingsSetDefaults(&defaults);
    presentation = lpsd ? &lpsd->presentationSettings : &defaults;
    dialog->colorKey = presentation->colorKey;

    mag_AddGraphicsOptions(hDlg, IDC_SETTINGS_GRAPHICS_API, (GRAPHICSAPI)graphicsApi);
    mag_AddSettingsOptions(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), captureApi);
    mag_AddSettingsOptions(hDlg, IDC_SETTINGS_UI_API, g_uiGraphicsApiOptions, ARRAYSIZE(g_uiGraphicsApiOptions), uiApi);
    mag_AddSettingsOptions(hDlg, IDC_SETTINGS_TEXT_RENDERER, g_textRendererOptions, ARRAYSIZE(g_textRendererOptions), textRenderer);
    mag_AddNamedOptions(
      hDlg,
      IDC_SETTINGS_PRESENT_TARGET,
      magPresentationTargetCount(),
      magPresentationTargetAt,
      presentation->target);
    mag_AddNamedOptions(
      hDlg,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      magSurfaceOwnershipCount(),
      magSurfaceOwnershipAt,
      presentation->surfaceOwnership);
    mag_AddNamedOptions(
      hDlg,
      IDC_SETTINGS_COMPOSITION_HOST,
      magCompositionHostCount(),
      magCompositionHostAt,
      presentation->host);
    mag_AddNamedOptions(
      hDlg,
      IDC_SETTINGS_COPY_REQUIREMENT,
      magCopyRequirementCount(),
      magCopyRequirementAt,
      presentation->copyRequirement);
    mag_AddNamedOptions(
      hDlg,
      IDC_SETTINGS_ALPHA_MODE,
      magLayeredAlphaModeCount(),
      magLayeredAlphaModeAt,
      presentation->alphaMode);
    mag_AddNamedOptions(
      hDlg,
      IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
      magWaitableSwapChainModeCount(),
      magWaitableSwapChainModeAt,
      presentation->waitableSwapChainMode);
    mag_AddDisplayAdapterOptions(hDlg, dialog, &presentation->display);
    mag_AddHardwareAdapterOptions(hDlg, dialog, &presentation->hardware);
    for (page = 0; page < ARRAYSIZE(comboIds); ++page)
    {
      mag_SettingsFitComboDropDown(hDlg, comboIds[page]);
    }
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_STRICT_TARGET,
      BM_SETCHECK,
      presentation->strictTarget ? BST_CHECKED : BST_UNCHECKED,
      0);
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_ALLOW_TEARING,
      BM_SETCHECK,
      presentation->allowTearing ? BST_CHECKED : BST_UNCHECKED,
      0);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_BUFFER_COUNT,
      IDC_SETTINGS_VALUE_BUFFER_COUNT,
      2,
      16,
      presentation->bufferCount);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_FRAME_LATENCY,
      IDC_SETTINGS_VALUE_FRAME_LATENCY,
      1,
      16,
      presentation->maximumFrameLatency);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_SYNC_INTERVAL,
      IDC_SETTINGS_VALUE_SYNC_INTERVAL,
      0,
      4,
      presentation->syncInterval);
    mag_SettingsSetSlider(
      hDlg,
      IDC_SETTINGS_CONSTANT_ALPHA,
      IDC_SETTINGS_VALUE_CONSTANT_ALPHA,
      0,
      255,
      presentation->constantAlpha);
    mag_SettingsUpdateColorKeyButton(hDlg, dialog->colorKey);
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_MOUSE_RELATIVE_ZOOM,
      BM_SETCHECK,
      (lpsd && lpsd->fMouseRelativeZoom) ? BST_CHECKED : BST_UNCHECKED,
      0);
    dialog->updatingControls = FALSE;
    mag_SettingsShowPage(hDlg, MAG_SETTINGS_PAGE_ROUTE);
    mag_UpdateSettingsDialogState(hDlg);

    return TRUE;
}

static LRESULT mag_Settings_OnNotify(
  HWND hDlg,
  int idFrom,
  NMHDR* header)
{
    if (IDC_SETTINGS_TAB == idFrom && header && TCN_SELCHANGE == header->code)
    {
      const int selectedPage = TabCtrl_GetCurSel(header->hwndFrom);

      if (selectedPage >= 0)
      {
        mag_SettingsShowPage(hDlg, (UINT)selectedPage);
      }
    }
    return 0;
}

static void mag_Settings_OnDestroy(HWND hDlg)
{
    MAGSETTINGSDIALOGSTATE* dialog = (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);

    SetWindowLongPtr(hDlg, DWLP_USER, 0);
    if (dialog)
    {
      mag_SettingsReleaseRouteDrawingResources(dialog);
      if (dialog->routeFont)
      {
        DeleteFont(dialog->routeFont);
      }
      HeapFree(GetProcessHeap(), 0, dialog);
    }
}

static void mag_Settings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id)
    {
    case IDC_SETTINGS_ROUTE_PACING:
    case IDC_SETTINGS_ROUTE_LAYERING:
      if (BN_CLICKED == codeNotify)
      {
        const UINT page = IDC_SETTINGS_ROUTE_PACING == id
          ? MAG_SETTINGS_PAGE_PACING
          : MAG_SETTINGS_PAGE_LAYERING;
        HWND hTab = GetDlgItem(hDlg, IDC_SETTINGS_TAB);

        if (hTab)
        {
          TabCtrl_SetCurSel(hTab, (int)page);
        }
        mag_SettingsShowPage(hDlg, page);
      }
      return;
    case IDC_SETTINGS_ROUTE_DIAGRAM:
      if (STN_CLICKED == codeNotify)
      {
        mag_SettingsActivateRouteNode(hDlg);
      }
      return;
    case IDC_SETTINGS_LABEL_STRICT_TARGET:
    case IDC_SETTINGS_LABEL_ALLOW_TEARING:
    case IDC_SETTINGS_LABEL_MOUSE_RELATIVE_ZOOM:
      if (STN_CLICKED == codeNotify)
      {
        const UINT checkId = IDC_SETTINGS_LABEL_STRICT_TARGET == id
          ? IDC_SETTINGS_STRICT_TARGET
          : (IDC_SETTINGS_LABEL_ALLOW_TEARING == id
              ? IDC_SETTINGS_ALLOW_TEARING
              : IDC_SETTINGS_MOUSE_RELATIVE_ZOOM);

        SendDlgItemMessage(hDlg, checkId, BM_CLICK, 0, 0);
      }
      return;
    case IDC_SETTINGS_PRESET:
      {
        MAGSETTINGSDIALOGSTATE* dialog =
          (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
        LRESULT selected;
        LRESULT preset;

        if (!dialog || dialog->updatingControls || CBN_SELCHANGE != codeNotify)
        {
          return;
        }
        selected = SendDlgItemMessage(
          hDlg,
          IDC_SETTINGS_PRESET,
          CB_GETCURSEL,
          0,
          0);
        preset = CB_ERR == selected
          ? CB_ERR
          : SendDlgItemMessage(
              hDlg,
              IDC_SETTINGS_PRESET,
              CB_GETITEMDATA,
              (WPARAM)selected,
              0);
        if (MAG_SETTINGS_PRESET_AUTO == preset)
        {
          mag_SettingsSetAutoPresentationControls(hDlg, dialog);
        }
        else
        {
          dialog->autoPreset = FALSE;
        }
        mag_SettingsRefresh(hDlg, TRUE);
        return;
      }
    case IDC_SETTINGS_COLOR_KEY:
      {
        MAGSETTINGSDIALOGSTATE* dialog =
          (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
        CHOOSECOLOR choice = { sizeof(choice) };

        if (BN_CLICKED != codeNotify || !dialog)
        {
          return;
        }
        choice.hwndOwner = hDlg;
        choice.rgbResult = dialog->colorKey;
        choice.lpCustColors = dialog->customColors;
        choice.Flags = CC_FULLOPEN | CC_RGBINIT;
        if (ChooseColor(&choice))
        {
          dialog->colorKey = choice.rgbResult;
          mag_SettingsUpdateColorKeyButton(hDlg, dialog->colorKey);
          mag_SettingsMarkCustom(hDlg);
          mag_SettingsRefresh(hDlg, TRUE);
        }
        return;
      }
    case IDC_SETTINGS_GRAPHICS_API:
    case IDC_SETTINGS_CAPTURE_API:
    case IDC_SETTINGS_UI_API:
    case IDC_SETTINGS_TEXT_RENDERER:
    case IDC_SETTINGS_PRESENT_TARGET:
    case IDC_SETTINGS_SURFACE_OWNERSHIP:
    case IDC_SETTINGS_COMPOSITION_HOST:
    case IDC_SETTINGS_COPY_REQUIREMENT:
    case IDC_SETTINGS_ALPHA_MODE:
    case IDC_SETTINGS_DISPLAY_ADAPTER:
    case IDC_SETTINGS_HARDWARE_ADAPTER:
    case IDC_SETTINGS_WAITABLE_SWAP_CHAIN:
    case IDC_SETTINGS_STRICT_TARGET:
    case IDC_SETTINGS_ALLOW_TEARING:
    case IDC_SETTINGS_MOUSE_RELATIVE_ZOOM:
      {
        if (CBN_SELCHANGE == codeNotify || BN_CLICKED == codeNotify || EN_CHANGE == codeNotify)
        {
          mag_SettingsMarkCustom(hDlg);
          mag_SettingsRefresh(hDlg, TRUE);
        }
        return;
      }
    case IDCANCEL:
      {
        EndDialog(hDlg, IDCANCEL);
        return;
      }
    default:
      UNREFERENCED_PARAMETER(hwndCtl);
      return;
    }
}

static void mag_Settings_OnHScroll(
  HWND hDlg,
  HWND hwndCtl,
  UINT code,
  int position)
{
    UINT valueId;

    UNREFERENCED_PARAMETER(position);
    if (!hwndCtl)
    {
      return;
    }
    switch (GetDlgCtrlID(hwndCtl))
    {
    case IDC_SETTINGS_CONSTANT_ALPHA:
      valueId = IDC_SETTINGS_VALUE_CONSTANT_ALPHA;
      break;
    case IDC_SETTINGS_BUFFER_COUNT:
      valueId = IDC_SETTINGS_VALUE_BUFFER_COUNT;
      break;
    case IDC_SETTINGS_FRAME_LATENCY:
      valueId = IDC_SETTINGS_VALUE_FRAME_LATENCY;
      break;
    case IDC_SETTINGS_SYNC_INTERVAL:
      valueId = IDC_SETTINGS_VALUE_SYNC_INTERVAL;
      break;
    default:
      return;
    }
    mag_SettingsUpdateSliderValue(
      hDlg,
      (UINT)GetDlgCtrlID(hwndCtl),
      valueId);
    mag_SettingsMarkCustom(hDlg);
    mag_SettingsRefresh(hDlg, TB_THUMBTRACK != code);
}

static BOOL mag_SettingsOptionIsImplemented(
  const SETTINGSOPTION* options,
  UINT count,
  UINT id)
{
    UINT index;

    for (index = 0; index < count; ++index)
    {
      if (options[index].id == id)
      {
        return options[index].fImplemented;
      }
    }
    return FALSE;
}

static BOOL mag_SettingsResolveCandidate(
  const MAGSETTINGSDIALOGSTATE* dialog,
  GRAPHICSAPI graphicsApi,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  const MAGPRESENTATIONSETTINGS* requested,
  LPTSTR reason,
  UINT reasonCount)
{
    const MAGGRAPHICSBACKEND* backend = magGraphicsGetBackend(graphicsApi);
    MAGPRESENTATIONSETTINGS resolved;
    MAGPRESENTATIONSTATUS status = { 0 };

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!dialog || !requested || !dialog->catalogValid)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(
          reason,
          dialog && dialog->catalogReason[0]
            ? dialog->catalogReason
            : TEXT("Display and hardware adapters are unavailable."),
          reasonCount);
      }
      return FALSE;
    }
    if (!backend || !backend->implemented ||
        !backend->IsAvailable(reason, reasonCount))
    {
      if (reason && reasonCount && !reason[0])
      {
        lstrcpyn(reason, TEXT("The graphics renderer is unavailable."), reasonCount);
      }
      return FALSE;
    }
    if (!magPresentationResolve(
          dialog->owner,
          graphicsApi,
          captureApi,
          uiApi,
          textRenderer,
          requested,
          &dialog->catalog,
          &resolved,
          &status) ||
        !status.configurationSupported ||
        !status.flickerFree)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(
          reason,
          status.reason[0]
            ? status.reason
            : TEXT("The route is incompatible with the current settings."),
          reasonCount);
      }
      return FALSE;
    }
    return TRUE;
}

static BOOL mag_SettingsReadCandidate(
  HWND hDlg,
  const MAGSETTINGSDIALOGSTATE* dialog,
  GRAPHICSAPI* graphicsApi,
  CAPTUREAPI* captureApi,
  UIGRAPHICSAPI* uiApi,
  TEXTRENDERER* textRenderer,
  MAGPRESENTATIONSETTINGS* presentation)
{
    const MAGSTATE* activeState = dialog
      ? (const MAGSTATE*)GetWindowLongPtr(dialog->owner, GWLP_USERDATA)
      : NULL;
    UINT graphicsId;
    UINT captureId;
    UINT uiId;
    UINT textId;
    BOOL available;

    if (!graphicsApi || !captureApi || !uiApi || !textRenderer || !presentation)
    {
      return FALSE;
    }
    if (activeState)
    {
      *graphicsApi = mag_SettingsActiveGraphicsApi(activeState);
      *captureApi = mag_SettingsActiveCaptureApi(activeState);
      *uiApi = activeState->uiGraphicsApi;
      *textRenderer = activeState->textRenderer;
      *presentation = activeState->presentationSettings;
      return TRUE;
    }
    if (
        !mag_GetSelectedGraphicsOption(
          hDlg,
          IDC_SETTINGS_GRAPHICS_API,
          &graphicsId,
          &available) ||
        !mag_GetSelectedSettingsOption(
          hDlg,
          IDC_SETTINGS_CAPTURE_API,
          g_captureApiOptions,
          ARRAYSIZE(g_captureApiOptions),
          &captureId,
          &available) ||
        !mag_GetSelectedSettingsOption(
          hDlg,
          IDC_SETTINGS_UI_API,
          g_uiGraphicsApiOptions,
          ARRAYSIZE(g_uiGraphicsApiOptions),
          &uiId,
          &available) ||
        !mag_GetSelectedSettingsOption(
          hDlg,
          IDC_SETTINGS_TEXT_RENDERER,
          g_textRendererOptions,
          ARRAYSIZE(g_textRendererOptions),
          &textId,
          &available) ||
        !mag_GetPresentationDialogSettings(hDlg, dialog, presentation))
    {
      return FALSE;
    }
    *graphicsApi = (GRAPHICSAPI)graphicsId;
    *captureApi = (CAPTUREAPI)captureId;
    *uiApi = (UIGRAPHICSAPI)uiId;
    *textRenderer = (TEXTRENDERER)textId;
    return TRUE;
}

static BOOL mag_SettingsComboItemAvailable(
  HWND hDlg,
  MAGSETTINGSDIALOGSTATE* dialog,
  UINT controlId,
  UINT_PTR itemData,
  LPTSTR reason,
  UINT reasonCount)
{
    GRAPHICSAPI graphicsApi;
    CAPTUREAPI captureApi;
    UIGRAPHICSAPI uiApi;
    TEXTRENDERER textRenderer;
    MAGPRESENTATIONSETTINGS presentation;

    if (IDC_SETTINGS_PRESET == controlId)
    {
      return TRUE;
    }
    if (!dialog || dialog->updatingControls ||
        !mag_SettingsReadCandidate(
          hDlg,
          dialog,
          &graphicsApi,
          &captureApi,
          &uiApi,
          &textRenderer,
          &presentation))
    {
      return TRUE;
    }
    switch (controlId)
    {
    case IDC_SETTINGS_GRAPHICS_API:
      if (itemData >= GRAPHICS_API_COUNT)
      {
        return FALSE;
      }
      graphicsApi = (GRAPHICSAPI)itemData;
      break;
    case IDC_SETTINGS_CAPTURE_API:
      if (itemData >= ARRAYSIZE(g_captureApiOptions))
      {
        return FALSE;
      }
      captureApi = (CAPTUREAPI)g_captureApiOptions[itemData].id;
      break;
    case IDC_SETTINGS_UI_API:
      if (itemData >= ARRAYSIZE(g_uiGraphicsApiOptions))
      {
        return FALSE;
      }
      uiApi = (UIGRAPHICSAPI)g_uiGraphicsApiOptions[itemData].id;
      break;
    case IDC_SETTINGS_TEXT_RENDERER:
      if (itemData >= ARRAYSIZE(g_textRendererOptions))
      {
        return FALSE;
      }
      textRenderer = (TEXTRENDERER)g_textRendererOptions[itemData].id;
      break;
    case IDC_SETTINGS_PRESENT_TARGET:
      if (itemData >= MAG_PRESENT_COUNT)
      {
        return FALSE;
      }
      presentation.target = (MAGPRESENTATIONTARGET)itemData;
      break;
    case IDC_SETTINGS_SURFACE_OWNERSHIP:
      if (itemData >= MAG_SURFACE_COUNT)
      {
        return FALSE;
      }
      presentation.surfaceOwnership = (MAGSURFACEOWNERSHIP)itemData;
      break;
    case IDC_SETTINGS_COMPOSITION_HOST:
      if (itemData >= MAG_HOST_COUNT)
      {
        return FALSE;
      }
      presentation.host = (MAGCOMPOSITIONHOST)itemData;
      break;
    case IDC_SETTINGS_COPY_REQUIREMENT:
      if (itemData >= MAG_COPY_COUNT)
      {
        return FALSE;
      }
      presentation.copyRequirement = (MAGCOPYREQUIREMENT)itemData;
      break;
    case IDC_SETTINGS_ALPHA_MODE:
      if (itemData >= MAG_LAYER_ALPHA_COUNT)
      {
        return FALSE;
      }
      presentation.alphaMode = (MAGLAYEREDALPHAMODE)itemData;
      break;
    case IDC_SETTINGS_WAITABLE_SWAP_CHAIN:
      if (itemData >= MAG_WAITABLE_SWAP_CHAIN_MODE_COUNT)
      {
        return FALSE;
      }
      presentation.waitableSwapChainMode =
        (MAGWAITABLESWAPCHAINMODE)itemData;
      break;
    case IDC_SETTINGS_DISPLAY_ADAPTER:
      if (itemData >= MAG_SETTINGS_EXPLICIT_ITEM_BASE)
      {
        const UINT outputIndex =
          (UINT)(itemData - MAG_SETTINGS_EXPLICIT_ITEM_BASE);
        const MAGOUTPUTINFO* output;

        if (outputIndex >= dialog->catalog.outputCount)
        {
          return FALSE;
        }
        output = &dialog->catalog.outputs[outputIndex];
        if (output->adapterIndex >= dialog->catalog.adapterCount)
        {
          return FALSE;
        }
        presentation.display.mode = MAG_DISPLAY_ADAPTER_EXPLICIT;
        presentation.display.adapterLuid =
          dialog->catalog.adapters[output->adapterIndex].luid;
        lstrcpyn(
          presentation.display.deviceName,
          output->deviceName,
          ARRAYSIZE(presentation.display.deviceName));
      }
      else if (itemData < MAG_DISPLAY_ADAPTER_MODE_COUNT)
      {
        presentation.display.mode = (MAGDISPLAYADAPTERMODE)itemData;
        ZeroMemory(
          &presentation.display.adapterLuid,
          sizeof(presentation.display.adapterLuid));
        presentation.display.deviceName[0] = TEXT('\0');
      }
      else
      {
        return FALSE;
      }
      break;
    case IDC_SETTINGS_HARDWARE_ADAPTER:
      if (itemData >= MAG_SETTINGS_EXPLICIT_ITEM_BASE)
      {
        const UINT adapterIndex =
          (UINT)(itemData - MAG_SETTINGS_EXPLICIT_ITEM_BASE);

        if (adapterIndex >= dialog->catalog.adapterCount)
        {
          return FALSE;
        }
        presentation.hardware.mode = MAG_HARDWARE_ADAPTER_EXPLICIT;
        presentation.hardware.adapterLuid =
          dialog->catalog.adapters[adapterIndex].luid;
      }
      else if (itemData < MAG_HARDWARE_ADAPTER_MODE_COUNT)
      {
        presentation.hardware.mode = (MAGHARDWAREADAPTERMODE)itemData;
        ZeroMemory(
          &presentation.hardware.adapterLuid,
          sizeof(presentation.hardware.adapterLuid));
      }
      else
      {
        return FALSE;
      }
      break;
    default:
      return TRUE;
    }
    return mag_SettingsResolveCandidate(
      dialog,
      graphicsApi,
      captureApi,
      uiApi,
      textRenderer,
      &presentation,
      reason,
      reasonCount);
}

static void mag_SettingsUpdateAvailabilitySummary(
  HWND hDlg,
  MAGSETTINGSDIALOGSTATE* dialog)
{
    static const UINT nodeComboIds[12] =
    {
      IDC_SETTINGS_CAPTURE_API,
      IDC_SETTINGS_COPY_REQUIREMENT,
      IDC_SETTINGS_GRAPHICS_API,
      IDC_SETTINGS_HARDWARE_ADAPTER,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      IDC_SETTINGS_COMPOSITION_HOST,
      IDC_SETTINGS_PRESENT_TARGET,
      IDC_SETTINGS_DISPLAY_ADAPTER,
      IDC_SETTINGS_TEXT_RENDERER,
      IDC_SETTINGS_UI_API,
      IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
      IDC_SETTINGS_ALPHA_MODE,
    };
    UINT node;

    if (!dialog)
    {
      return;
    }
    ZeroMemory(dialog->availableCounts, sizeof(dialog->availableCounts));
    ZeroMemory(dialog->optionCounts, sizeof(dialog->optionCounts));
    for (node = 0; node < ARRAYSIZE(nodeComboIds); ++node)
    {
      HWND hCombo;
      LRESULT count;
      LRESULT item;

      if (!nodeComboIds[node])
      {
        dialog->availableCounts[node] = 3;
        dialog->optionCounts[node] = 3;
        continue;
      }
      hCombo = GetDlgItem(hDlg, nodeComboIds[node]);
      count = SendMessage(hCombo, CB_GETCOUNT, 0, 0);
      if (count <= 0)
      {
        continue;
      }
      dialog->optionCounts[node] = (UINT)count;
      for (item = 0; item < count; ++item)
      {
        const LRESULT itemData = SendMessage(
          hCombo,
          CB_GETITEMDATA,
          (WPARAM)item,
          0);

        if (CB_ERR != itemData && mag_SettingsComboItemAvailable(
              hDlg,
              dialog,
              nodeComboIds[node],
              (UINT_PTR)itemData,
              NULL,
              0))
        {
          ++dialog->availableCounts[node];
        }
      }
      InvalidateRect(hCombo, NULL, TRUE);
    }
}

static LPCTSTR mag_SettingsOptionName(
  const SETTINGSOPTION* options,
  UINT count,
  UINT id)
{
    UINT index;

    for (index = 0; index < count; ++index)
    {
      if (options[index].id == id)
      {
        return options[index].pszName;
      }
    }
    return TEXT("Unknown");
}

static LPCTSTR mag_SettingsNamedOptionName(
  UINT count,
  MAGOPTIONATPROC optionAt,
  UINT id)
{
    UINT index;

    for (index = 0; index < count; ++index)
    {
      const MAGNAMEDOPTION* option = optionAt(index);

      if (option && option->id == id)
      {
        return option->name;
      }
    }
    return TEXT("Unknown");
}

static LPCTSTR mag_SettingsShortHost(MAGCOMPOSITIONHOST host)
{
    switch (host)
    {
    case MAG_HOST_REDIRECTED_HWND:
      return TEXT("Redirected HWND");
    case MAG_HOST_TRADITIONAL_LAYERED:
      return TEXT("Layered HWND");
    case MAG_HOST_DIRECTCOMPOSITION:
      return TEXT("DirectComposition");
    case MAG_HOST_PRESENTATION_MANAGER:
      return TEXT("Presentation Manager");
    default:
      return TEXT("Auto");
    }
}

static LPCTSTR mag_SettingsShortSurface(MAGSURFACEOWNERSHIP surface)
{
    switch (surface)
    {
    case MAG_SURFACE_REDIRECTION:
      return TEXT("redirected");
    case MAG_SURFACE_NO_REDIRECTION:
      return TEXT("no-redirection");
    default:
      return TEXT("Auto");
    }
}

static LPCTSTR mag_SettingsShortTarget(MAGPRESENTATIONTARGET target)
{
    switch (target)
    {
    case MAG_PRESENT_HARDWARE_LEGACY_FLIP:
      return TEXT("Legacy Flip");
    case MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER:
      return TEXT("Legacy front copy");
    case MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP:
      return TEXT("Independent Flip");
    case MAG_PRESENT_COMPOSED_FLIP:
      return TEXT("Composed Flip");
    case MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP:
      return TEXT("HW Composed Flip");
    case MAG_PRESENT_COMPOSED_COPY_GPU_GDI:
      return TEXT("GPU GDI copy");
    case MAG_PRESENT_COMPOSED_COPY_CPU_GDI:
      return TEXT("CPU GDI copy");
    default:
      return TEXT("Auto");
    }
}

static LPCTSTR mag_SettingsShortWaitable(MAGWAITABLESWAPCHAINMODE mode)
{
    switch (mode)
    {
    case MAG_WAITABLE_SWAP_CHAIN_ENABLED:
      return TEXT("enabled");
    case MAG_WAITABLE_SWAP_CHAIN_DISABLED:
      return TEXT("disabled");
    default:
      return TEXT("Auto");
    }
}

static void mag_SettingsCompactComboText(
  const MAGSETTINGSDIALOGSTATE* dialog,
  UINT controlId,
  UINT_PTR itemData,
  LPCTSTR originalText,
  LPTSTR compactText,
  UINT compactTextCount)
{
    LPCTSTR value = originalText;

    switch (controlId)
    {
    case IDC_SETTINGS_CAPTURE_API:
      if (itemData < ARRAYSIZE(g_captureApiOptions))
      {
        switch (g_captureApiOptions[itemData].id)
        {
        case CAPTURE_API_DXGI_DESKTOP_DUPLICATION:
          value = TEXT("DXGI Duplication");
          break;
        case CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE:
          value = TEXT("Windows Capture");
          break;
        case CAPTURE_API_DWM_PRIVATE_VISUAL:
          value = TEXT("DWM Private");
          break;
        default:
          value = g_captureApiOptions[itemData].pszName;
          break;
        }
      }
      break;
    case IDC_SETTINGS_UI_API:
      if (itemData < ARRAYSIZE(g_uiGraphicsApiOptions) &&
          UI_GRAPHICS_API_NATIVE == g_uiGraphicsApiOptions[itemData].id)
      {
        value = TEXT("Native");
      }
      break;
    case IDC_SETTINGS_TEXT_RENDERER:
      if (itemData < ARRAYSIZE(g_textRendererOptions) &&
          TEXT_RENDERER_GPU_GLYPH_ATLAS == g_textRendererOptions[itemData].id)
      {
        value = TEXT("Glyph atlas");
      }
      break;
    case IDC_SETTINGS_PRESENT_TARGET:
      if (itemData < MAG_PRESENT_COUNT)
      {
        value = mag_SettingsShortTarget((MAGPRESENTATIONTARGET)itemData);
      }
      break;
    case IDC_SETTINGS_SURFACE_OWNERSHIP:
      if (itemData < MAG_SURFACE_COUNT)
      {
        value = mag_SettingsShortSurface((MAGSURFACEOWNERSHIP)itemData);
      }
      break;
    case IDC_SETTINGS_COMPOSITION_HOST:
      if (itemData < MAG_HOST_COUNT)
      {
        value = mag_SettingsShortHost((MAGCOMPOSITIONHOST)itemData);
      }
      break;
    case IDC_SETTINGS_COPY_REQUIREMENT:
      switch ((MAGCOPYREQUIREMENT)itemData)
      {
      case MAG_COPY_STRICT_ZERO_COPY:
        value = TEXT("Zero-copy");
        break;
      case MAG_COPY_ALLOW_GPU_LOCAL:
        value = TEXT("GPU copy");
        break;
      case MAG_COPY_ALLOW_CPU_ROUND_TRIP:
        value = TEXT("CPU copy");
        break;
      default:
        value = TEXT("Auto");
        break;
      }
      break;
    case IDC_SETTINGS_DISPLAY_ADAPTER:
      if (MAG_DISPLAY_ADAPTER_AUTO == itemData)
      {
        value = TEXT("Window output");
      }
      else if (MAG_DISPLAY_ADAPTER_FOLLOW_CAPTURE == itemData)
      {
        value = TEXT("Captured display");
      }
      else if (dialog && itemData >= MAG_SETTINGS_EXPLICIT_ITEM_BASE)
      {
        const UINT outputIndex =
          (UINT)(itemData - MAG_SETTINGS_EXPLICIT_ITEM_BASE);

        if (outputIndex < dialog->catalog.outputCount)
        {
          value = dialog->catalog.outputs[outputIndex].deviceName;
        }
      }
      break;
    case IDC_SETTINGS_HARDWARE_ADAPTER:
      switch ((UINT)itemData)
      {
      case MAG_HARDWARE_ADAPTER_AUTO:
        value = TEXT("Auto");
        break;
      case MAG_HARDWARE_ADAPTER_SAME_AS_DISPLAY:
        value = TEXT("Display adapter");
        break;
      case MAG_HARDWARE_ADAPTER_SAME_AS_CAPTURE:
        value = TEXT("Capture adapter");
        break;
      case MAG_HARDWARE_ADAPTER_WARP:
        value = TEXT("WARP");
        break;
      default:
        break;
      }
      break;
    case IDC_SETTINGS_WAITABLE_SWAP_CHAIN:
      value = mag_SettingsShortWaitable((MAGWAITABLESWAPCHAINMODE)itemData);
      break;
    default:
      break;
    }
    lstrcpyn(compactText, value, compactTextCount);
}

static void mag_SettingsDrawComboItem(
  HWND hDlg,
  const DRAWITEMSTRUCT* drawItem)
{
    MAGSETTINGSDIALOGSTATE* dialog =
      (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
    TCHAR text[512] = TEXT("");
    TCHAR compactText[512] = TEXT("");
    TCHAR reason[MAG_PRESENTATION_REASON_LENGTH] = TEXT("");
    TCHAR displayText[1024] = TEXT("");
    RECT statusRect;
    RECT textRect;
    HFONT controlFont;
    HFONT oldFont = NULL;
    COLORREF background;
    COLORREF foreground;
    BOOL available = TRUE;
    BOOL selected;
    BOOL compact;

    if (!drawItem || ODT_COMBOBOX != drawItem->CtlType ||
        (UINT)-1 == drawItem->itemID)
    {
      return;
    }
    SendMessage(
      drawItem->hwndItem,
      CB_GETLBTEXT,
      drawItem->itemID,
      (LPARAM)text);
    available = mag_SettingsComboItemAvailable(
      hDlg,
      dialog,
      drawItem->CtlID,
      drawItem->itemData,
      reason,
      ARRAYSIZE(reason));
    selected = 0 != (drawItem->itemState & ODS_SELECTED);
    compact = 0 != (drawItem->itemState & ODS_COMBOBOXEDIT);
    if (compact && MAG_SETTINGS_PAGE_ROUTE ==
          TabCtrl_GetCurSel(GetDlgItem(hDlg, IDC_SETTINGS_TAB)))
    {
      mag_SettingsCompactComboText(
        dialog,
        drawItem->CtlID,
        drawItem->itemData,
        text,
        compactText,
        ARRAYSIZE(compactText));
      lstrcpyn(text, compactText, ARRAYSIZE(text));
    }
    background = selected
      ? GetSysColor(COLOR_HIGHLIGHT)
      : GetSysColor(COLOR_WINDOW);
    foreground = selected
      ? GetSysColor(COLOR_HIGHLIGHTTEXT)
      : (available ? GetSysColor(COLOR_WINDOWTEXT) : GetSysColor(COLOR_GRAYTEXT));
    SetBkColor(drawItem->hDC, background);
    SetTextColor(drawItem->hDC, foreground);
    ExtTextOut(
      drawItem->hDC,
      0,
      0,
      ETO_OPAQUE,
      &drawItem->rcItem,
      NULL,
      0,
      NULL);
    controlFont = (HFONT)SendMessage(drawItem->hwndItem, WM_GETFONT, 0, 0);
    if (controlFont)
    {
      oldFont = SelectFont(drawItem->hDC, controlFont);
    }
    SetBkMode(drawItem->hDC, TRANSPARENT);
    textRect = drawItem->rcItem;
    textRect.left += 4;
    textRect.right -= 4;
    if (IDC_SETTINGS_PRESET != drawItem->CtlID)
    {
      const int statusWidth = compact
        ? max(3, MulDiv(4, (int)GetDpiForWindow(hDlg), USER_DEFAULT_SCREEN_DPI))
        : MulDiv(68, (int)GetDpiForWindow(hDlg), USER_DEFAULT_SCREEN_DPI);

      statusRect = textRect;
      statusRect.right = min(statusRect.right, statusRect.left + statusWidth);
      textRect.left = min(textRect.right, statusRect.right + 3);
      SetTextColor(
        drawItem->hDC,
        selected
          ? GetSysColor(COLOR_HIGHLIGHTTEXT)
          : (available ? RGB(0, 110, 45) : RGB(190, 35, 35)));
      if (compact)
      {
        HBRUSH oldBrush = SelectBrush(
          drawItem->hDC,
          GetStockBrush(DC_BRUSH));

        SetDCBrushColor(
          drawItem->hDC,
          available ? RGB(0, 120, 55) : RGB(190, 35, 35));
        FillRect(
          drawItem->hDC,
          &statusRect,
          GetStockBrush(DC_BRUSH));
        SelectBrush(drawItem->hDC, oldBrush);
      }
      else
      {
        DrawText(
          drawItem->hDC,
          available ? TEXT("Available") : TEXT("Unavailable"),
          -1,
          &statusRect,
          DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
      }
    }
    SetTextColor(drawItem->hDC, foreground);
    if (!compact && !available && reason[0])
    {
      _sntprintf_s(
        displayText,
        ARRAYSIZE(displayText),
        _TRUNCATE,
        TEXT("%s  -  %s"),
        text,
        reason);
    }
    else
    {
      lstrcpyn(displayText, text, ARRAYSIZE(displayText));
    }
    DrawText(
      drawItem->hDC,
      displayText,
      -1,
      &textRect,
      DT_LEFT | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
    if (drawItem->itemState & ODS_FOCUS)
    {
      DrawFocusRect(drawItem->hDC, &drawItem->rcItem);
    }
    if (oldFont)
    {
      SelectFont(drawItem->hDC, oldFont);
    }
}

static void mag_Settings_OnMeasureItem(
  HWND hDlg,
  MEASUREITEMSTRUCT* measureItem)
{
    HDC hdc;
    HWND hControl;
    HFONT font;
    HFONT oldFont = NULL;
    TEXTMETRIC metrics;

    if (!measureItem || ODT_COMBOBOX != measureItem->CtlType)
    {
      return;
    }
    hControl = GetDlgItem(hDlg, measureItem->CtlID);
    hdc = GetDC(hControl ? hControl : hDlg);
    font = hControl
      ? (HFONT)SendMessage(hControl, WM_GETFONT, 0, 0)
      : NULL;
    if (hdc && font)
    {
      oldFont = SelectFont(hdc, font);
    }
    if (hdc && GetTextMetrics(hdc, &metrics))
    {
      measureItem->itemHeight = max(
        measureItem->itemHeight,
        (UINT)metrics.tmHeight + 4U);
    }
    if (oldFont)
    {
      SelectFont(hdc, oldFont);
    }
    if (hdc)
    {
      ReleaseDC(hControl ? hControl : hDlg, hdc);
    }
}

static D2D1_COLOR_F mag_SettingsD2dColor(
  BYTE red,
  BYTE green,
  BYTE blue,
  FLOAT alpha)
{
    D2D1_COLOR_F color =
    {
      red / 255.0f,
      green / 255.0f,
      blue / 255.0f,
      alpha,
    };

    return color;
}

static void mag_SettingsD2dSetBrush(
  MAGSETTINGSDIALOGSTATE* dialog,
  D2D1_COLOR_F color)
{
    dialog->routeD2dBrush->lpVtbl->SetColor(
      dialog->routeD2dBrush,
      &color);
}

static void mag_SettingsD2dDrawPolyline(
  MAGSETTINGSDIALOGSTATE* dialog,
  const D2D1_POINT_2F* points,
  UINT count,
  D2D1_COLOR_F color,
  FLOAT width)
{
    ID2D1DCRenderTarget* target = dialog->routeD2dTarget;
    UINT index;

    if (!points || count < 2)
    {
      return;
    }
    mag_SettingsD2dSetBrush(dialog, color);
    for (index = 1; index < count; ++index)
    {
      target->lpVtbl->DrawLine(
        target,
        points[index - 1],
        points[index],
        dialog->routeD2dBrush,
        width,
        NULL);
    }
    {
      const D2D1_POINT_2F end = points[count - 1];
      const D2D1_POINT_2F previous = points[count - 2];
      D2D1_POINT_2F arrow1 = end;
      D2D1_POINT_2F arrow2 = end;

      if (fabsf(end.x - previous.x) >= fabsf(end.y - previous.y))
      {
        const FLOAT direction = end.x >= previous.x ? 1.0f : -1.0f;
        arrow1.x -= 5.0f * direction;
        arrow1.y -= 3.0f;
        arrow2.x -= 5.0f * direction;
        arrow2.y += 3.0f;
      }
      else
      {
        const FLOAT direction = end.y >= previous.y ? 1.0f : -1.0f;
        arrow1.x -= 3.0f;
        arrow1.y -= 5.0f * direction;
        arrow2.x += 3.0f;
        arrow2.y -= 5.0f * direction;
      }
      target->lpVtbl->DrawLine(
        target,
        end,
        arrow1,
        dialog->routeD2dBrush,
        width,
        NULL);
      target->lpVtbl->DrawLine(
        target,
        end,
        arrow2,
        dialog->routeD2dBrush,
        width,
        NULL);
    }
}

static void mag_SettingsD2dDrawConnection(
  MAGSETTINGSDIALOGSTATE* dialog,
  const D2D1_RECT_F* source,
  const D2D1_RECT_F* destination,
  D2D1_COLOR_F color,
  FLOAT width)
{
    D2D1_POINT_2F points[2] =
    {
      { source->right, (source->top + source->bottom) * 0.5f },
      { destination->left, (destination->top + destination->bottom) * 0.5f },
    };

    mag_SettingsD2dDrawPolyline(dialog, points, ARRAYSIZE(points), color, width);
}

static BOOL mag_SettingsDrawRouteD2d(
  HWND hDlg,
  const DRAWITEMSTRUCT* drawItem,
  MAGSETTINGSDIALOGSTATE* dialog)
{
    static const WCHAR* const nodeNames[12] =
    {
      L"Capture", L"Transfer", L"Renderer", L"Hardware",
      L"Surface", L"Host", L"Plane target", L"Output planes",
      L"Text", L"UI", L"Pacing", L"Layering",
    };
    LPMAGSTATE activeState;
    GRAPHICSAPI activeGraphicsApi;
    CAPTUREAPI activeCaptureApi;
    UIGRAPHICSAPI activeUiApi;
    TEXTRENDERER activeTextRenderer;
    const MAGPRESENTATIONSETTINGS* activeRequested;
    const MAGPRESENTATIONSETTINGS* activeResolved;
    const MAGPRESENTATIONSTATUS* activeStatus;
    const MAGGRAPHICSBACKEND* backend;
    const MAGADAPTERINFO* adapter;
    D2D1_RECT_F nodes[12];
    WCHAR nodeValues[12][160] = { { 0 } };
    BOOL changed[12] = { FALSE };
    BOOL unavailableRequest;
    BOOL compositorBypass;
    const FLOAT scaleX =
      RECTWIDTH(drawItem->rcItem) / (FLOAT)MAG_SETTINGS_ROUTE_WIDTH;
    const FLOAT scaleY =
      RECTHEIGHT(drawItem->rcItem) / (FLOAT)MAG_SETTINGS_ROUTE_HEIGHT;
    const D2D1_COLOR_F background = mag_SettingsD2dColor(255, 255, 255, 1.0f);
    const D2D1_COLOR_F active = mag_SettingsD2dColor(0, 102, 180, 1.0f);
    const D2D1_COLOR_F inactive = mag_SettingsD2dColor(150, 156, 162, 1.0f);
    const D2D1_COLOR_F unavailable = mag_SettingsD2dColor(190, 35, 35, 1.0f);
    const D2D1_COLOR_F available = mag_SettingsD2dColor(0, 120, 55, 1.0f);
    const D2D1_COLOR_F activeFill = mag_SettingsD2dColor(245, 250, 255, 1.0f);
    const D2D1_COLOR_F unavailableFill = mag_SettingsD2dColor(255, 242, 242, 1.0f);
    const D2D1_COLOR_F text = mag_SettingsD2dColor(36, 36, 36, 1.0f);
    RECT bindRect = drawItem->rcItem;
    UINT node;
    HRESULT hr;

    UNREFERENCED_PARAMETER(hDlg);
    if (!dialog->routeD2dTarget || !dialog->routeD2dBrush ||
        !dialog->routeNodeFormat || !dialog->routeValueFormat ||
        !dialog->routeHeaderFormat ||
        FAILED(dialog->routeD2dTarget->lpVtbl->BindDC(
          dialog->routeD2dTarget,
          drawItem->hDC,
          &bindRect)))
    {
      return FALSE;
    }
    activeState = (LPMAGSTATE)GetWindowLongPtr(dialog->owner, GWLP_USERDATA);
    activeGraphicsApi = mag_SettingsActiveGraphicsApi(activeState);
    activeCaptureApi = mag_SettingsActiveCaptureApi(activeState);
    activeUiApi = activeState
      ? activeState->uiGraphicsApi
      : dialog->requestedUiApi;
    activeTextRenderer = activeState
      ? activeState->textRenderer
      : dialog->requestedTextRenderer;
    activeRequested = activeState
      ? &activeState->presentationSettings
      : &dialog->requestedPresentation;
    activeResolved = activeState && activeState->resolvedPresentation.version
      ? &activeState->resolvedPresentation
      : &dialog->resolvedPresentation;
    activeStatus = activeState &&
      activeState->presentationStatus.configurationSupported
        ? &activeState->presentationStatus
        : &dialog->pendingStatus;
    backend = magGraphicsGetBackend(activeGraphicsApi);
    adapter = activeStatus->hardwareAdapterResolved
      ? magAdapterCatalogFindAdapter(
          &dialog->catalog,
          activeStatus->hardwareAdapterLuid)
      : NULL;
    unavailableRequest = !dialog->pendingApplied && dialog->pendingFieldsValid;
    compositorBypass = activeState &&
      (CAPTURE_API_DWM_THUMBNAIL == activeCaptureApi ||
       CAPTURE_API_DWM_PRIVATE_VISUAL == activeCaptureApi);

    lstrcpynW(
      nodeValues[0],
      mag_SettingsOptionName(
        g_captureApiOptions,
        ARRAYSIZE(g_captureApiOptions),
        activeCaptureApi),
      ARRAYSIZE(nodeValues[0]));
    if (MAG_COPY_CLASS_UNKNOWN != activeStatus->copyClass)
    {
      _snwprintf_s(
        nodeValues[1],
        ARRAYSIZE(nodeValues[1]),
        _TRUNCATE,
        L"%s%s",
        MAG_COPY_AUTO_FASTEST == activeRequested->copyRequirement
          ? L"Auto > "
          : L"",
        magCopyClassName(activeStatus->copyClass));
    }
    else
    {
      lstrcpynW(
        nodeValues[1],
        mag_SettingsNamedOptionName(
          magCopyRequirementCount(),
          magCopyRequirementAt,
          activeRequested->copyRequirement),
        ARRAYSIZE(nodeValues[1]));
    }
    _snwprintf_s(
      nodeValues[2],
      ARRAYSIZE(nodeValues[2]),
      _TRUNCATE,
      L"%s%s",
      backend ? backend->name : L"Unknown",
      compositorBypass ? L" (capture bypass)" : L"");
    if (MAG_HARDWARE_ADAPTER_WARP == activeRequested->hardware.mode)
    {
      lstrcpynW(nodeValues[3], L"WARP software", ARRAYSIZE(nodeValues[3]));
    }
    else if (adapter)
    {
      _snwprintf_s(
        nodeValues[3],
        ARRAYSIZE(nodeValues[3]),
        _TRUNCATE,
        L"%s%s",
        MAG_HARDWARE_ADAPTER_AUTO == activeRequested->hardware.mode
          ? L"Auto > "
          : L"",
        adapter->description);
    }
    else
    {
      lstrcpynW(nodeValues[3], L"Unresolved", ARRAYSIZE(nodeValues[3]));
    }
    _snwprintf_s(
      nodeValues[4],
      ARRAYSIZE(nodeValues[4]),
      _TRUNCATE,
      L"%s%s",
      MAG_SURFACE_AUTO == activeRequested->surfaceOwnership
        ? L"Auto > "
        : L"",
      mag_SettingsShortSurface(activeResolved->surfaceOwnership));
    _snwprintf_s(
      nodeValues[5],
      ARRAYSIZE(nodeValues[5]),
      _TRUNCATE,
      L"%s%s",
      MAG_HOST_AUTO == activeRequested->host ? L"Auto > " : L"",
      mag_SettingsShortHost(activeResolved->host));
    if (activeStatus->observedTargetValid)
    {
      _snwprintf_s(
        nodeValues[6],
        ARRAYSIZE(nodeValues[6]),
        _TRUNCATE,
        L"Observed > %s",
        mag_SettingsShortTarget(activeStatus->observedTarget));
    }
    else
    {
      _snwprintf_s(
        nodeValues[6],
        ARRAYSIZE(nodeValues[6]),
        _TRUNCATE,
        L"%s%s > %s",
        MAG_PRESENT_AUTO == activeRequested->target ? L"Auto > " : L"",
        mag_SettingsShortTarget(activeResolved->target),
        activeStatus->observationAvailable ? L"pending" : L"no observer");
    }
    _snwprintf_s(
      nodeValues[7],
      ARRAYSIZE(nodeValues[7]),
      _TRUNCATE,
      L"DF:%s IF:%s MPO:%s %u/%uR; DISP:%s CA:%s %s",
      activeStatus->directFlipCapable ? L"yes" : L"no",
      activeStatus->independentFlipCapable ? L"yes" : L"no",
      activeStatus->multiPlaneOverlayCapable ? L"yes" : L"no",
      activeStatus->overlayCapsKnown ? activeStatus->maxPlanes : 0,
      activeStatus->maxRgbPlanes,
      activeStatus->wddm3CapsKnown
        ? (activeStatus->displayableSupported ? L"yes" : L"no")
        : L"?",
      activeStatus->crossAdapterSupportKnown
        ? magCrossAdapterSupportTierName(activeStatus->crossAdapterSupportTier)
        : L"?",
      activeResolved->display.deviceName[0]
        ? activeResolved->display.deviceName
        : L"unresolved");
    lstrcpynW(
      nodeValues[8],
      mag_SettingsOptionName(
        g_textRendererOptions,
        ARRAYSIZE(g_textRendererOptions),
        activeTextRenderer),
      ARRAYSIZE(nodeValues[8]));
    lstrcpynW(
      nodeValues[9],
      mag_SettingsOptionName(
        g_uiGraphicsApiOptions,
        ARRAYSIZE(g_uiGraphicsApiOptions),
        activeUiApi),
      ARRAYSIZE(nodeValues[9]));
    _snwprintf_s(
      nodeValues[10],
      ARRAYSIZE(nodeValues[10]),
      _TRUNCATE,
      L"%u buf; L%u; S%u; wait %s%s%s",
      activeRequested->bufferCount,
      activeRequested->maximumFrameLatency,
      activeRequested->syncInterval,
      MAG_WAITABLE_SWAP_CHAIN_AUTO ==
          activeRequested->waitableSwapChainMode
        ? L"Auto>"
        : L"",
      mag_SettingsShortWaitable(activeResolved->waitableSwapChainMode),
      activeRequested->allowTearing ? L"; tearing" : L"");
    lstrcpynW(
      nodeValues[11],
      mag_SettingsNamedOptionName(
        magLayeredAlphaModeCount(),
        magLayeredAlphaModeAt,
        activeResolved->alphaMode),
      ARRAYSIZE(nodeValues[11]));
    if (unavailableRequest && activeState)
    {
      changed[0] = dialog->requestedCaptureApi != activeCaptureApi;
      changed[1] = dialog->requestedPresentation.copyRequirement !=
        activeState->presentationSettings.copyRequirement;
      changed[2] = dialog->requestedGraphicsApi != activeGraphicsApi;
      changed[3] = dialog->requestedPresentation.hardware.mode !=
          activeState->presentationSettings.hardware.mode ||
        !magAdapterLuidEqual(
          dialog->requestedPresentation.hardware.adapterLuid,
          activeState->presentationSettings.hardware.adapterLuid);
      changed[4] = dialog->requestedPresentation.surfaceOwnership !=
        activeState->presentationSettings.surfaceOwnership;
      changed[5] = dialog->requestedPresentation.host !=
        activeState->presentationSettings.host;
      changed[6] = dialog->requestedPresentation.target !=
          activeState->presentationSettings.target ||
        dialog->requestedPresentation.strictTarget !=
          activeState->presentationSettings.strictTarget;
      changed[7] = dialog->requestedPresentation.display.mode !=
          activeState->presentationSettings.display.mode ||
        !magAdapterLuidEqual(
          dialog->requestedPresentation.display.adapterLuid,
          activeState->presentationSettings.display.adapterLuid) ||
        0 != lstrcmpi(
          dialog->requestedPresentation.display.deviceName,
          activeState->presentationSettings.display.deviceName);
      changed[8] = dialog->requestedTextRenderer != activeState->textRenderer;
      changed[9] = dialog->requestedUiApi != activeState->uiGraphicsApi;
      changed[10] = dialog->requestedPresentation.bufferCount !=
          activeState->presentationSettings.bufferCount ||
        dialog->requestedPresentation.maximumFrameLatency !=
          activeState->presentationSettings.maximumFrameLatency ||
        dialog->requestedPresentation.syncInterval !=
          activeState->presentationSettings.syncInterval ||
        dialog->requestedPresentation.waitableSwapChainMode !=
          activeState->presentationSettings.waitableSwapChainMode ||
        dialog->requestedPresentation.allowTearing !=
          activeState->presentationSettings.allowTearing;
      changed[11] = dialog->requestedPresentation.alphaMode !=
        activeState->presentationSettings.alphaMode;
    }
    for (node = 0; node < ARRAYSIZE(nodes); ++node)
    {
      nodes[node].left = g_settingsRouteNodeX[node] * scaleX;
      nodes[node].top = g_settingsRouteNodeY[node] * scaleY;
      nodes[node].right =
        (g_settingsRouteNodeX[node] + MAG_SETTINGS_ROUTE_NODE_WIDTH) * scaleX;
      nodes[node].bottom =
        (g_settingsRouteNodeY[node] + MAG_SETTINGS_ROUTE_NODE_HEIGHT) * scaleY;
    }

    dialog->routeD2dTarget->lpVtbl->SetTextAntialiasMode(
      dialog->routeD2dTarget,
      D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    dialog->routeD2dTarget->lpVtbl->BeginDraw(dialog->routeD2dTarget);
    dialog->routeD2dTarget->lpVtbl->Clear(dialog->routeD2dTarget, &background);

    mag_SettingsD2dDrawConnection(dialog, &nodes[0], &nodes[1], active, 1.5f);
    mag_SettingsD2dDrawConnection(
      dialog,
      &nodes[1],
      &nodes[2],
      compositorBypass ? inactive : active,
      compositorBypass ? 1.0f : 1.5f);
    {
      D2D1_POINT_2F points[4] =
      {
        { (nodes[1].left + nodes[1].right) * 0.5f, nodes[1].top },
        { (nodes[1].left + nodes[1].right) * 0.5f, 1.0f * scaleY },
        { (nodes[5].left + nodes[5].right) * 0.5f, 1.0f * scaleY },
        { (nodes[5].left + nodes[5].right) * 0.5f, nodes[5].top },
      };
      mag_SettingsD2dDrawPolyline(
        dialog,
        points,
        ARRAYSIZE(points),
        compositorBypass ? active : inactive,
        compositorBypass ? 1.5f : 1.0f);
    }
    {
      D2D1_POINT_2F points[4] =
      {
        { (nodes[3].left + nodes[3].right) * 0.5f, nodes[3].top },
        { (nodes[3].left + nodes[3].right) * 0.5f, 41.0f * scaleY },
        { (nodes[2].left + nodes[2].right) * 0.5f, 41.0f * scaleY },
        { (nodes[2].left + nodes[2].right) * 0.5f, nodes[2].bottom },
      };
      mag_SettingsD2dDrawPolyline(dialog, points, ARRAYSIZE(points), active, 1.5f);
    }
    mag_SettingsD2dDrawConnection(dialog, &nodes[8], &nodes[9], active, 1.5f);
    {
      D2D1_POINT_2F points[5] =
      {
        { nodes[9].right, (nodes[9].top + nodes[9].bottom) * 0.5f },
        { 164.0f * scaleX, (nodes[9].top + nodes[9].bottom) * 0.5f },
        { 164.0f * scaleX, 41.0f * scaleY },
        { (nodes[2].left + nodes[2].right) * 0.5f, 41.0f * scaleY },
        { (nodes[2].left + nodes[2].right) * 0.5f, nodes[2].bottom },
      };
      mag_SettingsD2dDrawPolyline(dialog, points, ARRAYSIZE(points), active, 1.5f);
    }
    {
      D2D1_POINT_2F points[4] =
      {
        { (nodes[4].left + nodes[4].right) * 0.5f, nodes[4].top },
        { (nodes[4].left + nodes[4].right) * 0.5f, 41.0f * scaleY },
        { (nodes[5].left + nodes[5].right) * 0.5f, 41.0f * scaleY },
        { (nodes[5].left + nodes[5].right) * 0.5f, nodes[5].bottom },
      };
      mag_SettingsD2dDrawPolyline(dialog, points, ARRAYSIZE(points), active, 1.5f);
    }
    {
      D2D1_POINT_2F points[5] =
      {
        { nodes[11].right, (nodes[11].top + nodes[11].bottom) * 0.5f },
        { 247.0f * scaleX, (nodes[11].top + nodes[11].bottom) * 0.5f },
        { 247.0f * scaleX, 41.0f * scaleY },
        { (nodes[5].left + nodes[5].right) * 0.5f, 41.0f * scaleY },
        { (nodes[5].left + nodes[5].right) * 0.5f, nodes[5].bottom },
      };
      mag_SettingsD2dDrawPolyline(dialog, points, ARRAYSIZE(points), active, 1.5f);
    }
    {
      D2D1_POINT_2F points[4] =
      {
        { nodes[10].right, (nodes[10].top + nodes[10].bottom) * 0.5f },
        { 329.0f * scaleX, (nodes[10].top + nodes[10].bottom) * 0.5f },
        { 329.0f * scaleX, (nodes[6].top + nodes[6].bottom) * 0.5f },
        { nodes[6].left, (nodes[6].top + nodes[6].bottom) * 0.5f },
      };
      mag_SettingsD2dDrawPolyline(dialog, points, ARRAYSIZE(points), active, 1.5f);
    }
    mag_SettingsD2dDrawConnection(dialog, &nodes[2], &nodes[5], active, 1.5f);
    mag_SettingsD2dDrawConnection(dialog, &nodes[5], &nodes[6], active, 1.5f);
    mag_SettingsD2dDrawConnection(dialog, &nodes[6], &nodes[7], active, 1.5f);

    for (node = 0; node < ARRAYSIZE(nodes); ++node)
    {
      D2D1_ROUNDED_RECT rounded =
      {
        nodes[node],
        4.0f,
        4.0f,
      };
      D2D1_RECT_F label = nodes[node];
      D2D1_RECT_F value = nodes[node];
      WCHAR nodeLabel[64];
      const BOOL nodeUnavailable = unavailableRequest && changed[node];

      mag_SettingsD2dSetBrush(
        dialog,
        nodeUnavailable ? unavailableFill : activeFill);
      dialog->routeD2dTarget->lpVtbl->FillRoundedRectangle(
        dialog->routeD2dTarget,
        &rounded,
        dialog->routeD2dBrush);
      mag_SettingsD2dSetBrush(
        dialog,
        nodeUnavailable ? unavailable : active);
      dialog->routeD2dTarget->lpVtbl->DrawRoundedRectangle(
        dialog->routeD2dTarget,
        &rounded,
        dialog->routeD2dBrush,
        nodeUnavailable ? 2.0f : 1.0f,
        NULL);
      _snwprintf_s(
        nodeLabel,
        ARRAYSIZE(nodeLabel),
        _TRUNCATE,
        L"%s  %u/%u",
        nodeNames[node],
        dialog->availableCounts[node],
        dialog->optionCounts[node]);
      label.left += 4.0f;
      label.right -= 4.0f;
      label.bottom = label.top + 11.0f * scaleY;
      value.left += 4.0f;
      value.right -= 4.0f;
      value.top += 11.0f * scaleY;
      value.bottom -= 2.0f * scaleY;
      mag_SettingsD2dSetBrush(
        dialog,
        nodeUnavailable
          ? unavailable
          : (dialog->availableCounts[node] ? text : unavailable));
      dialog->routeD2dTarget->lpVtbl->DrawText(
        dialog->routeD2dTarget,
        nodeLabel,
        (UINT32)lstrlenW(nodeLabel),
        dialog->routeNodeFormat,
        &label,
        dialog->routeD2dBrush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);
      mag_SettingsD2dSetBrush(
        dialog,
        nodeUnavailable ? unavailable : text);
      dialog->routeD2dTarget->lpVtbl->DrawText(
        dialog->routeD2dTarget,
        nodeValues[node],
        (UINT32)lstrlenW(nodeValues[node]),
        dialog->routeValueFormat,
        &value,
        dialog->routeD2dBrush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);
    }
    {
      D2D1_RECT_F header =
      {
        342.0f * scaleX,
        88.0f * scaleY,
        493.0f * scaleX,
        103.0f * scaleY,
      };
      D2D1_RECT_F legend =
      {
        342.0f * scaleX,
        105.0f * scaleY,
        493.0f * scaleX,
        154.0f * scaleY,
      };
      WCHAR headerText[96];

      _snwprintf_s(
        headerText,
        ARRAYSIZE(headerText),
        _TRUNCATE,
        unavailableRequest
          ? L"ACTIVE %s - REQUEST UNAVAILABLE"
          : L"ACTIVE PIPELINE - %s",
        activeState && activeState->fAutoSettingsPreset ? L"AUTO" : L"CUSTOM");
      mag_SettingsD2dSetBrush(dialog, unavailableRequest ? unavailable : active);
      dialog->routeD2dTarget->lpVtbl->DrawText(
        dialog->routeD2dTarget,
        headerText,
        (UINT32)lstrlenW(headerText),
        dialog->routeHeaderFormat,
        &header,
        dialog->routeD2dBrush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);
      mag_SettingsD2dSetBrush(dialog, available);
      dialog->routeD2dTarget->lpVtbl->DrawText(
        dialog->routeD2dTarget,
        L"Each node shows active value and available / total\nRed: unavailable request",
        (UINT32)lstrlenW(
          L"Each node shows active value and available / total\nRed: unavailable request"),
        dialog->routeValueFormat,
        &legend,
        dialog->routeD2dBrush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);
    }
    hr = dialog->routeD2dTarget->lpVtbl->EndDraw(
      dialog->routeD2dTarget,
      NULL,
      NULL);
    return SUCCEEDED(hr);
}

static void mag_SettingsDrawRouteArrow(
  HDC hdc,
  POINT start,
  POINT end,
  COLORREF color)
{
    const LONG horizontalDirection = end.x >= start.x ? 1 : -1;
    const LONG verticalDirection = end.y >= start.y ? 1 : -1;

    SetDCPenColor(hdc, color);
    MoveToEx(hdc, start.x, start.y, NULL);
    LineTo(hdc, end.x, end.y);
    if (labs(end.x - start.x) >= labs(end.y - start.y))
    {
      MoveToEx(hdc, end.x, end.y, NULL);
      LineTo(hdc, end.x - 4 * horizontalDirection, end.y - 3);
      MoveToEx(hdc, end.x, end.y, NULL);
      LineTo(hdc, end.x - 4 * horizontalDirection, end.y + 3);
    }
    else
    {
      MoveToEx(hdc, end.x, end.y, NULL);
      LineTo(hdc, end.x - 3, end.y - 4 * verticalDirection);
      MoveToEx(hdc, end.x, end.y, NULL);
      LineTo(hdc, end.x + 3, end.y - 4 * verticalDirection);
    }
}

static void mag_SettingsDrawRouteConnection(
  HDC hdc,
  const RECT* source,
  const RECT* destination,
  COLORREF color)
{
    POINT start;
    POINT end;

    if (destination->left >= source->right)
    {
      start.x = source->right;
      start.y = (source->top + source->bottom) / 2;
      end.x = destination->left;
      end.y = (destination->top + destination->bottom) / 2;
    }
    else
    {
      start.x = (source->left + source->right) / 2;
      start.y = source->bottom;
      end.x = (destination->left + destination->right) / 2;
      end.y = destination->top;
    }
    mag_SettingsDrawRouteArrow(hdc, start, end, color);
}

static void mag_SettingsDrawRouteNode(
  HDC hdc,
  const RECT* bounds,
  LPCTSTR title,
  LPCTSTR value,
  BOOL blocked,
  BOOL bypassed,
  const TEXTMETRIC* metrics)
{
    RECT titleRect = *bounds;
    RECT valueRect = *bounds;
    const COLORREF border = blocked ? RGB(190, 35, 35) : GetSysColor(COLOR_HIGHLIGHT);
    const COLORREF fill = bypassed
      ? GetSysColor(COLOR_BTNFACE)
      : GetSysColor(COLOR_WINDOW);

    SetDCPenColor(hdc, border);
    SetDCBrushColor(hdc, fill);
    RoundRect(
      hdc,
      bounds->left,
      bounds->top,
      bounds->right,
      bounds->bottom,
      5,
      5);
    titleRect.left += 3;
    titleRect.right -= 3;
    titleRect.top += 1;
    titleRect.bottom = min(
      titleRect.bottom,
      titleRect.top + metrics->tmHeight);
    valueRect.left += 3;
    valueRect.right -= 3;
    valueRect.top = titleRect.bottom - 1;
    valueRect.bottom -= 1;
    SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
    DrawText(
      hdc,
      title,
      -1,
      &titleRect,
      DT_CENTER | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
    SetTextColor(
      hdc,
      bypassed ? GetSysColor(COLOR_GRAYTEXT) : GetSysColor(COLOR_WINDOWTEXT));
    DrawText(
      hdc,
      value,
      -1,
      &valueRect,
      DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS | DT_NOPREFIX);
}

static void mag_Settings_OnDrawItem(
  HWND hDlg,
  const DRAWITEMSTRUCT* drawItem)
{
    MAGSETTINGSDIALOGSTATE* dialog =
      (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
    LPMAGSTATE activeState;
    GRAPHICSAPI activeGraphicsApi;
    CAPTUREAPI activeCaptureApi;
    UIGRAPHICSAPI activeUiApi;
    TEXTRENDERER activeTextRenderer;
    const MAGPRESENTATIONSETTINGS* activeRequested;
    const MAGPRESENTATIONSETTINGS* activeResolved;
    const MAGPRESENTATIONSTATUS* activeStatus;
    HDC hdc;
    RECT client;
    RECT nodes[9];
    HFONT oldFont;
    HPEN oldPen;
    HBRUSH oldBrush;
    TEXTMETRIC metrics;
    TCHAR source[96];
    TCHAR transfer[96];
    TCHAR renderer[96];
    TCHAR uiText[96];
    TCHAR hardware[128];
    TCHAR pacing[96];
    TCHAR hostSurface[128];
    TCHAR target[160];
    TCHAR display[160];
    const MAGGRAPHICSBACKEND* backend;
    const MAGADAPTERINFO* adapter;
    BOOL blocked;
    BOOL bypassed;
    BOOL changedNodes[9] = { FALSE };
    COLORREF routeColor;
    int margin;
    int bannerHeight;
    int graphTop;
    int columnGap;
    int rowGap;
    int nodeWidth;
    int nodeHeight;

    if (drawItem && ODT_COMBOBOX == drawItem->CtlType)
    {
      mag_SettingsDrawComboItem(hDlg, drawItem);
      return;
    }
    if (!drawItem || IDC_SETTINGS_ROUTE_DIAGRAM != drawItem->CtlID || !dialog)
    {
      return;
    }
    if (mag_SettingsDrawRouteD2d(hDlg, drawItem, dialog))
    {
      return;
    }
    activeState = (LPMAGSTATE)GetWindowLongPtr(dialog->owner, GWLP_USERDATA);
    activeGraphicsApi = activeState
      ? mag_SettingsActiveGraphicsApi(activeState)
      : dialog->requestedGraphicsApi;
    activeCaptureApi = activeState
      ? mag_SettingsActiveCaptureApi(activeState)
      : dialog->requestedCaptureApi;
    activeUiApi = activeState
      ? activeState->uiGraphicsApi
      : dialog->requestedUiApi;
    activeTextRenderer = activeState
      ? activeState->textRenderer
      : dialog->requestedTextRenderer;
    activeRequested = activeState
      ? &activeState->presentationSettings
      : &dialog->requestedPresentation;
    activeResolved = activeState && activeState->resolvedPresentation.version
      ? &activeState->resolvedPresentation
      : &dialog->resolvedPresentation;
    activeStatus = activeState && activeState->presentationStatus.configurationSupported
      ? &activeState->presentationStatus
      : &dialog->pendingStatus;
    blocked = !dialog->pendingApplied;
    bypassed = CAPTURE_API_DWM_THUMBNAIL == activeCaptureApi ||
      CAPTURE_API_DWM_PRIVATE_VISUAL == activeCaptureApi;
    if (blocked && dialog->pendingFieldsValid)
    {
      changedNodes[0] = dialog->requestedCaptureApi != activeCaptureApi;
      changedNodes[1] = dialog->requestedPresentation.hardware.mode !=
          activeRequested->hardware.mode ||
        !magAdapterLuidEqual(
          dialog->requestedPresentation.hardware.adapterLuid,
          activeRequested->hardware.adapterLuid);
      changedNodes[2] = dialog->requestedPresentation.bufferCount !=
          activeRequested->bufferCount ||
        dialog->requestedPresentation.maximumFrameLatency !=
          activeRequested->maximumFrameLatency ||
        dialog->requestedPresentation.syncInterval !=
          activeRequested->syncInterval ||
        dialog->requestedPresentation.waitableSwapChainMode !=
          activeRequested->waitableSwapChainMode ||
        dialog->requestedPresentation.allowTearing !=
          activeRequested->allowTearing;
      changedNodes[3] = dialog->requestedPresentation.copyRequirement !=
        activeRequested->copyRequirement;
      changedNodes[4] = dialog->requestedGraphicsApi != activeGraphicsApi;
      changedNodes[5] = dialog->requestedUiApi != activeUiApi ||
        dialog->requestedTextRenderer != activeTextRenderer;
      changedNodes[6] = dialog->requestedPresentation.host !=
          activeRequested->host ||
        dialog->requestedPresentation.surfaceOwnership !=
          activeRequested->surfaceOwnership ||
        dialog->requestedPresentation.alphaMode != activeRequested->alphaMode;
      changedNodes[7] = dialog->requestedPresentation.target !=
          activeRequested->target ||
        dialog->requestedPresentation.strictTarget !=
          activeRequested->strictTarget;
      changedNodes[8] = dialog->requestedPresentation.display.mode !=
          activeRequested->display.mode ||
        !magAdapterLuidEqual(
          dialog->requestedPresentation.display.adapterLuid,
          activeRequested->display.adapterLuid) ||
        0 != lstrcmpi(
          dialog->requestedPresentation.display.deviceName,
          activeRequested->display.deviceName);
      if (!changedNodes[0] && !changedNodes[1] && !changedNodes[2] &&
          !changedNodes[3] && !changedNodes[4] && !changedNodes[5] &&
          !changedNodes[6] && !changedNodes[7] && !changedNodes[8])
      {
        UINT changedIndex;

        for (changedIndex = 0; changedIndex < ARRAYSIZE(changedNodes); ++changedIndex)
        {
          changedNodes[changedIndex] = TRUE;
        }
      }
    }
    hdc = drawItem->hDC;
    client = drawItem->rcItem;
    FillRect(hdc, &client, GetSysColorBrush(COLOR_WINDOW));
    oldFont = SelectFont(
      hdc,
      dialog->routeFont
        ? dialog->routeFont
        : (HFONT)SendMessage(hDlg, WM_GETFONT, 0, 0));
    oldPen = SelectPen(hdc, GetStockPen(DC_PEN));
    oldBrush = SelectBrush(hdc, GetStockBrush(DC_BRUSH));
    SetBkMode(hdc, TRANSPARENT);
    GetTextMetrics(hdc, &metrics);

    routeColor = GetSysColor(COLOR_HIGHLIGHT);
    {
      RECT activeBanner = client;

      activeBanner.left += 6;
      activeBanner.right -= 6;
      activeBanner.top += 2;
      activeBanner.bottom = activeBanner.top + metrics.tmHeight;
      SetTextColor(hdc, routeColor);
      DrawText(
        hdc,
        activeState && activeState->fAutoSettingsPreset
          ? TEXT("ACTIVE PIPELINE - AUTO PRESET")
          : TEXT("ACTIVE PIPELINE - CUSTOM"),
        -1,
        &activeBanner,
        DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
      if (blocked)
      {
        SetTextColor(hdc, RGB(190, 35, 35));
        DrawText(
          hdc,
          TEXT("UNAVAILABLE REQUEST: RED NODE"),
          -1,
          &activeBanner,
          DT_RIGHT | DT_SINGLELINE | DT_END_ELLIPSIS | DT_NOPREFIX);
      }
    }

    margin = 6;
    bannerHeight = metrics.tmHeight + 7;
    graphTop = client.top + bannerHeight;
    columnGap = max(8, RECTWIDTH(client) / 48);
    nodeWidth = max(
      metrics.tmAveCharWidth * 9,
      (RECTWIDTH(client) - margin * 2 - columnGap * 4) / 5);
    nodeHeight = max(
      metrics.tmHeight * 2 + 5,
      (RECTHEIGHT(client) - bannerHeight - margin) / 5);
    rowGap = max(
      4,
      (RECTHEIGHT(client) - bannerHeight - margin - nodeHeight * 3) / 2);
    {
      int x[5];
      int y[3];
      int index;

      for (index = 0; index < 5; ++index)
      {
        x[index] = client.left + margin + index * (nodeWidth + columnGap);
      }
      for (index = 0; index < 3; ++index)
      {
        y[index] = graphTop + index * (nodeHeight + rowGap);
      }
      SetRect(&nodes[0], x[0], y[0], x[0] + nodeWidth, y[0] + nodeHeight);
      SetRect(&nodes[1], x[0], y[1], x[0] + nodeWidth, y[1] + nodeHeight);
      SetRect(&nodes[2], x[0], y[2], x[0] + nodeWidth, y[2] + nodeHeight);
      SetRect(&nodes[3], x[1], y[0], x[1] + nodeWidth, y[0] + nodeHeight);
      SetRect(&nodes[4], x[1], y[1], x[1] + nodeWidth, y[1] + nodeHeight);
      SetRect(&nodes[5], x[2], y[0], x[2] + nodeWidth, y[0] + nodeHeight);
      SetRect(&nodes[6], x[2], y[1], x[2] + nodeWidth, y[1] + nodeHeight);
      SetRect(&nodes[7], x[3], y[1], x[3] + nodeWidth, y[1] + nodeHeight);
      SetRect(
        &nodes[8],
        x[4],
        y[1],
        client.right - margin,
        y[1] + nodeHeight);
    }

    backend = magGraphicsGetBackend(activeGraphicsApi);
    adapter = activeStatus->hardwareAdapterResolved
      ? magAdapterCatalogFindAdapter(
          &dialog->catalog,
          activeStatus->hardwareAdapterLuid)
      : NULL;
    lstrcpyn(
      source,
      mag_SettingsOptionName(
        g_captureApiOptions,
        ARRAYSIZE(g_captureApiOptions),
        activeCaptureApi),
      ARRAYSIZE(source));
    if (MAG_COPY_CLASS_UNKNOWN != activeStatus->copyClass)
    {
      _sntprintf_s(
        transfer,
        ARRAYSIZE(transfer),
        _TRUNCATE,
        TEXT("%s%s"),
        MAG_COPY_AUTO_FASTEST == activeRequested->copyRequirement
          ? TEXT("Auto > ")
          : TEXT(""),
        magCopyClassName(activeStatus->copyClass));
    }
    else
    {
      lstrcpyn(
        transfer,
        mag_SettingsNamedOptionName(
          magCopyRequirementCount(),
          magCopyRequirementAt,
          activeRequested->copyRequirement),
        ARRAYSIZE(transfer));
    }
    _sntprintf_s(
      renderer,
      ARRAYSIZE(renderer),
      _TRUNCATE,
      TEXT("%s%s"),
      backend ? backend->name : TEXT("Unknown"),
      bypassed ? TEXT(" (capture bypass)") : TEXT(""));
    _sntprintf_s(
      uiText,
      ARRAYSIZE(uiText),
      _TRUNCATE,
      TEXT("%s / %s"),
      mag_SettingsOptionName(
        g_uiGraphicsApiOptions,
        ARRAYSIZE(g_uiGraphicsApiOptions),
        activeUiApi),
      mag_SettingsOptionName(
        g_textRendererOptions,
        ARRAYSIZE(g_textRendererOptions),
        activeTextRenderer));
    if (MAG_HARDWARE_ADAPTER_WARP ==
          activeRequested->hardware.mode)
    {
      lstrcpyn(hardware, TEXT("WARP software"), ARRAYSIZE(hardware));
    }
    else if (adapter)
    {
      _sntprintf_s(
        hardware,
        ARRAYSIZE(hardware),
        _TRUNCATE,
        TEXT("%s%s"),
        MAG_HARDWARE_ADAPTER_AUTO ==
          activeRequested->hardware.mode
          ? TEXT("Auto > ")
          : TEXT(""),
        adapter->description);
    }
    else
    {
      lstrcpyn(hardware, TEXT("unresolved"), ARRAYSIZE(hardware));
    }
    _sntprintf_s(
      pacing,
      ARRAYSIZE(pacing),
      _TRUNCATE,
      TEXT("%u buffers / latency %u / sync %u / wait %s%s"),
      activeRequested->bufferCount,
      activeRequested->maximumFrameLatency,
      activeRequested->syncInterval,
      MAG_WAITABLE_SWAP_CHAIN_AUTO ==
          activeRequested->waitableSwapChainMode
        ? TEXT("Auto > ")
        : TEXT(""),
      mag_SettingsShortWaitable(activeResolved->waitableSwapChainMode));
    _sntprintf_s(
      hostSurface,
      ARRAYSIZE(hostSurface),
      _TRUNCATE,
      TEXT("%s%s / %s%s"),
      MAG_HOST_AUTO == activeRequested->host
        ? TEXT("Auto > ")
        : TEXT(""),
      mag_SettingsShortHost(activeResolved->host),
      MAG_SURFACE_AUTO == activeRequested->surfaceOwnership
        ? TEXT("Auto > ")
        : TEXT(""),
      mag_SettingsShortSurface(activeResolved->surfaceOwnership));
    if (activeStatus->observedTargetValid)
    {
      _sntprintf_s(
        target,
        ARRAYSIZE(target),
        _TRUNCATE,
        TEXT("Observed > %s"),
        mag_SettingsShortTarget(activeStatus->observedTarget));
    }
    else
    {
      _sntprintf_s(
        target,
        ARRAYSIZE(target),
        _TRUNCATE,
        TEXT("%s%s > %s"),
        MAG_PRESENT_AUTO == activeRequested->target
          ? TEXT("Auto > ")
          : TEXT(""),
        mag_SettingsShortTarget(activeResolved->target),
        activeStatus->observationAvailable
          ? TEXT("pending")
          : TEXT("no observer"));
    }
    _sntprintf_s(
      display,
      ARRAYSIZE(display),
      _TRUNCATE,
      TEXT("DF:%s IF:%s MPO:%s %u/%uR DISP:%s CA:%s; %s%s"),
      activeStatus->directFlipCapable ? TEXT("yes") : TEXT("no"),
      activeStatus->independentFlipCapable ? TEXT("yes") : TEXT("no"),
      activeStatus->multiPlaneOverlayCapable ? TEXT("yes") : TEXT("no"),
      activeStatus->overlayCapsKnown ? activeStatus->maxPlanes : 0,
      activeStatus->maxRgbPlanes,
      activeStatus->wddm3CapsKnown
        ? (activeStatus->displayableSupported ? TEXT("yes") : TEXT("no"))
        : TEXT("?"),
      activeStatus->crossAdapterSupportKnown
        ? magCrossAdapterSupportTierName(activeStatus->crossAdapterSupportTier)
        : TEXT("?"),
      MAG_DISPLAY_ADAPTER_AUTO == activeRequested->display.mode
        ? TEXT("Auto > ")
        : TEXT(""),
      activeResolved->display.deviceName[0]
        ? activeResolved->display.deviceName
        : TEXT("unresolved"));

    mag_SettingsDrawRouteConnection(hdc, &nodes[0], &nodes[3], routeColor);
    mag_SettingsDrawRouteConnection(
      hdc,
      &nodes[3],
      &nodes[4],
      bypassed ? GetSysColor(COLOR_GRAYTEXT) : routeColor);
    mag_SettingsDrawRouteConnection(
      hdc,
      &nodes[3],
      &nodes[6],
      bypassed ? routeColor : GetSysColor(COLOR_GRAYTEXT));
    mag_SettingsDrawRouteConnection(hdc, &nodes[1], &nodes[4], routeColor);
    mag_SettingsDrawRouteConnection(hdc, &nodes[2], &nodes[6], routeColor);
    mag_SettingsDrawRouteConnection(hdc, &nodes[4], &nodes[5], routeColor);
    mag_SettingsDrawRouteConnection(hdc, &nodes[5], &nodes[6], routeColor);
    mag_SettingsDrawRouteConnection(hdc, &nodes[6], &nodes[7], routeColor);
    mag_SettingsDrawRouteConnection(hdc, &nodes[7], &nodes[8], routeColor);

    mag_SettingsDrawRouteNode(hdc, &nodes[0], TEXT("Capture"), source, blocked && changedNodes[0], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[1], TEXT("Hardware"), hardware, blocked && changedNodes[1], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[2], TEXT("Pacing"), pacing, blocked && changedNodes[2], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[3], TEXT("Transfer"), transfer, blocked && changedNodes[3], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[4], TEXT("Renderer"), renderer, blocked && changedNodes[4], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[5], TEXT("UI / text"), uiText, blocked && changedNodes[5], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[6], TEXT("Host / surface"), hostSurface, blocked && changedNodes[6], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[7], TEXT("Plane target"), target, blocked && changedNodes[7], FALSE, &metrics);
    mag_SettingsDrawRouteNode(hdc, &nodes[8], TEXT("Output / planes"), display, blocked && changedNodes[8], FALSE, &metrics);

    SelectBrush(hdc, oldBrush);
    SelectPen(hdc, oldPen);
    SelectFont(hdc, oldFont);
}

static BOOL mag_SettingsControlUsesTabBackground(UINT id)
{
    switch (id)
    {
    case IDC_SETTINGS_LABEL_PRESET:
    case IDC_SETTINGS_LABEL_GRAPHICS_API:
    case IDC_SETTINGS_LABEL_CAPTURE_API:
    case IDC_SETTINGS_LABEL_UI_API:
    case IDC_SETTINGS_LABEL_TEXT_RENDERER:
    case IDC_SETTINGS_LABEL_PRESENT_TARGET:
    case IDC_SETTINGS_LABEL_SURFACE_OWNERSHIP:
    case IDC_SETTINGS_LABEL_COMPOSITION_HOST:
    case IDC_SETTINGS_LABEL_COPY_REQUIREMENT:
    case IDC_SETTINGS_LABEL_DISPLAY_ADAPTER:
    case IDC_SETTINGS_LABEL_HARDWARE_ADAPTER:
    case IDC_SETTINGS_LABEL_ALPHA_MODE:
    case IDC_SETTINGS_LABEL_CONSTANT_ALPHA:
    case IDC_SETTINGS_LABEL_COLOR_KEY:
    case IDC_SETTINGS_LABEL_BUFFER_COUNT:
    case IDC_SETTINGS_LABEL_FRAME_LATENCY:
    case IDC_SETTINGS_LABEL_SYNC_INTERVAL:
    case IDC_SETTINGS_VALUE_CONSTANT_ALPHA:
    case IDC_SETTINGS_VALUE_BUFFER_COUNT:
    case IDC_SETTINGS_VALUE_FRAME_LATENCY:
    case IDC_SETTINGS_VALUE_SYNC_INTERVAL:
    case IDC_SETTINGS_LABEL_STRICT_TARGET:
    case IDC_SETTINGS_LABEL_ALLOW_TEARING:
    case IDC_SETTINGS_LABEL_MOUSE_RELATIVE_ZOOM:
      return TRUE;
    default:
      return FALSE;
    }
}

static HBRUSH mag_Settings_OnCtlColorStatic(
  HWND hDlg,
  HDC hdc,
  HWND hwndCtl,
  int type)
{
    UNREFERENCED_PARAMETER(hDlg);
    UNREFERENCED_PARAMETER(type);
    if (hwndCtl && mag_SettingsControlUsesTabBackground((UINT)GetDlgCtrlID(hwndCtl)))
    {
      SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
      SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
      return GetSysColorBrush(COLOR_WINDOW);
    }
    return NULL;
}

static HBRUSH mag_Settings_OnCtlColorBtn(
  HWND hDlg,
  HDC hdc,
  HWND hwndCtl,
  int type)
{
    const UINT id = hwndCtl ? (UINT)GetDlgCtrlID(hwndCtl) : 0;

    UNREFERENCED_PARAMETER(hDlg);
    UNREFERENCED_PARAMETER(type);
    if (IDC_SETTINGS_STRICT_TARGET == id ||
        IDC_SETTINGS_ALLOW_TEARING == id ||
        IDC_SETTINGS_MOUSE_RELATIVE_ZOOM == id)
    {
      SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
      SetBkColor(hdc, GetSysColor(COLOR_WINDOW));
      return GetSysColorBrush(COLOR_WINDOW);
    }
    return NULL;
}

static INT_PTR CALLBACK mag_SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    HANDLE_DIALOG_MSG(hDlg, WM_INITDIALOG, mag_Settings_OnInitDialog);
    HANDLE_DIALOG_MSG(hDlg, WM_COMMAND,    mag_Settings_OnCommand);
    HANDLE_DIALOG_MSG(hDlg, WM_HSCROLL,    mag_Settings_OnHScroll);
    HANDLE_DIALOG_MSG(hDlg, WM_NOTIFY,     mag_Settings_OnNotify);
    HANDLE_DIALOG_MSG(hDlg, WM_DRAWITEM,   mag_Settings_OnDrawItem);
    HANDLE_DIALOG_MSG(hDlg, WM_MEASUREITEM, mag_Settings_OnMeasureItem);
    HANDLE_DIALOG_MSG(hDlg, WM_CTLCOLORSTATIC, mag_Settings_OnCtlColorStatic);
    HANDLE_DIALOG_MSG(hDlg, WM_CTLCOLORBTN,    mag_Settings_OnCtlColorBtn);
    HANDLE_DIALOG_MSG(hDlg, WM_MAG_PRESENTATION_STATUS, mag_Settings_OnPresentationStatus);
    HANDLE_DIALOG_MSG(hDlg, WM_DESTROY,    mag_Settings_OnDestroy);
    }
    return FALSE;
}

static BOOL mag_SettingsWriteWindowBmp(HWND hWnd, LPCTSTR path)
{
    RECT windowRect;
    BITMAPINFO bitmapInfo = { 0 };
    BITMAPFILEHEADER fileHeader = { 0 };
    HDC memoryDC = NULL;
    HBITMAP bitmap = NULL;
    HBITMAP oldBitmap = NULL;
    BYTE* pixels = NULL;
    HANDLE file = INVALID_HANDLE_VALUE;
    DWORD pixelsSize;
    DWORD written;
    BOOL result = FALSE;

    if (!hWnd || !path || !path[0] || magGraphicsIsInputDesktop() ||
        !GetWindowRect(hWnd, &windowRect) ||
        RECTWIDTH(windowRect) < 1 || RECTHEIGHT(windowRect) < 1)
    {
      return FALSE;
    }
    memoryDC = CreateCompatibleDC(NULL);
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = RECTWIDTH(windowRect);
    bitmapInfo.bmiHeader.biHeight = -RECTHEIGHT(windowRect);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(
      memoryDC,
      &bitmapInfo,
      DIB_RGB_COLORS,
      (void**)&pixels,
      NULL,
      0);
    if (!memoryDC || !bitmap || !pixels)
    {
      goto cleanup;
    }
    oldBitmap = SelectBitmap(memoryDC, bitmap);
    PatBlt(
      memoryDC,
      0,
      0,
      RECTWIDTH(windowRect),
      RECTHEIGHT(windowRect),
      WHITENESS);
    if (!PrintWindow(hWnd, memoryDC, PW_RENDERFULLCONTENT))
    {
      SendMessage(
        hWnd,
        WM_PRINT,
        (WPARAM)memoryDC,
        PRF_CHECKVISIBLE | PRF_NONCLIENT | PRF_CLIENT | PRF_ERASEBKGND |
          PRF_CHILDREN | PRF_OWNED);
    }
    GdiFlush();

    pixelsSize = (DWORD)((SIZE_T)RECTWIDTH(windowRect) *
      (SIZE_T)RECTHEIGHT(windowRect) * 4U);
    fileHeader.bfType = 0x4D42;
    fileHeader.bfOffBits = sizeof(fileHeader) + sizeof(bitmapInfo.bmiHeader);
    fileHeader.bfSize = fileHeader.bfOffBits + pixelsSize;
    file = CreateFile(
      path,
      GENERIC_WRITE,
      0,
      NULL,
      CREATE_ALWAYS,
      FILE_ATTRIBUTE_NORMAL,
      NULL);
    if (INVALID_HANDLE_VALUE == file)
    {
      goto cleanup;
    }
    result = WriteFile(
        file,
        &fileHeader,
        sizeof(fileHeader),
        &written,
        NULL) && written == sizeof(fileHeader) &&
      WriteFile(
        file,
        &bitmapInfo.bmiHeader,
        sizeof(bitmapInfo.bmiHeader),
        &written,
        NULL) && written == sizeof(bitmapInfo.bmiHeader) &&
      WriteFile(file, pixels, pixelsSize, &written, NULL) &&
        written == pixelsSize;

cleanup:
    if (INVALID_HANDLE_VALUE != file)
    {
      CloseHandle(file);
    }
    if (oldBitmap)
    {
      SelectBitmap(memoryDC, oldBitmap);
    }
    if (bitmap)
    {
      DeleteBitmap(bitmap);
    }
    if (memoryDC)
    {
      DeleteDC(memoryDC);
    }
    return result;
}

BOOL mag_RunSettingsDialogSmoke(
  HWND hWnd,
  LPTSTR reason,
  UINT reasonCount)
{
    static const UINT comboIds[] =
    {
      IDC_SETTINGS_PRESET,
      IDC_SETTINGS_GRAPHICS_API,
      IDC_SETTINGS_CAPTURE_API,
      IDC_SETTINGS_UI_API,
      IDC_SETTINGS_TEXT_RENDERER,
      IDC_SETTINGS_PRESENT_TARGET,
      IDC_SETTINGS_SURFACE_OWNERSHIP,
      IDC_SETTINGS_COMPOSITION_HOST,
      IDC_SETTINGS_COPY_REQUIREMENT,
      IDC_SETTINGS_DISPLAY_ADAPTER,
      IDC_SETTINGS_HARDWARE_ADAPTER,
      IDC_SETTINGS_ALPHA_MODE,
      IDC_SETTINGS_WAITABLE_SWAP_CHAIN,
    };
    HWND hDlg = NULL;
    HWND hDiagram;
    MAGSETTINGSDIALOGSTATE* dialog;
    HDC memoryDC = NULL;
    HBITMAP bitmap = NULL;
    HBITMAP oldBitmap = NULL;
    BYTE* pixels = NULL;
    BITMAPINFO bitmapInfo = { 0 };
    DRAWITEMSTRUCT drawItem = { 0 };
    RECT diagramRect;
    TCHAR status[MAG_PRESENTATION_REASON_LENGTH];
    UINT activePixelCount = 0;
    UINT textPixelCount = 0;
    UINT pixelIndex;
    UINT index;
    BOOL result = FALSE;

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!hWnd || magGraphicsIsInputDesktop())
    {
      if (reason && reasonCount)
      {
        lstrcpyn(
          reason,
          TEXT("The settings smoke refused to create a dialog on the input desktop."),
          reasonCount);
      }
      return FALSE;
    }
    hDlg = CreateDialogParam(
      GetModuleHandle(NULL),
      MAKEINTRESOURCE(IDD_SETTINGS),
      hWnd,
      mag_SettingsDlgProc,
      (LPARAM)hWnd);
    if (!hDlg)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The settings dialog could not be created."), reasonCount);
      }
      goto cleanup;
    }
    ShowWindow(hDlg, SW_SHOWNOACTIVATE);
    UpdateWindow(hDlg);
    dialog = (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
    hDiagram = GetDlgItem(hDlg, IDC_SETTINGS_ROUTE_DIAGRAM);
    if (!dialog || !hDiagram || !dialog->routeD2dTarget ||
        !dialog->routeD2dBrush || !dialog->routeNodeFormat ||
        !dialog->routeValueFormat || !dialog->routeHeaderFormat ||
        !GetClientRect(hDiagram, &diagramRect))
    {
      if (reason && reasonCount)
      {
        lstrcpyn(
          reason,
          TEXT("The Direct2D/DirectWrite route diagram did not initialize."),
          reasonCount);
      }
      goto cleanup;
    }
    for (index = 0; index < ARRAYSIZE(g_settingsRouteNodeControls); ++index)
    {
      if (!dialog->optionCounts[index] ||
          !GetDlgItem(hDlg, g_settingsRouteNodeControls[index]))
      {
        if (reason && reasonCount)
        {
          _sntprintf_s(
            reason,
            reasonCount,
            _TRUNCATE,
            TEXT("Route node %u has no option registry or interactive control."),
            index);
        }
        goto cleanup;
      }
    }
    if (!GetDlgItemText(
          hDlg,
          IDC_SETTINGS_STATUS,
          status,
          ARRAYSIZE(status)) ||
        _tcsstr(status, TEXT("Not implemented")) ||
        _tcsstr(status, TEXT("Blocked")))
    {
      if (reason && reasonCount)
      {
        lstrcpyn(
          reason,
          TEXT("The settings status is empty or contains a legacy placeholder."),
          reasonCount);
      }
      goto cleanup;
    }

    memoryDC = CreateCompatibleDC(NULL);
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = RECTWIDTH(diagramRect);
    bitmapInfo.bmiHeader.biHeight = -RECTHEIGHT(diagramRect);
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    bitmap = CreateDIBSection(
      memoryDC,
      &bitmapInfo,
      DIB_RGB_COLORS,
      (void**)&pixels,
      NULL,
      0);
    if (!memoryDC || !bitmap || !pixels)
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The route-diagram test surface could not be created."), reasonCount);
      }
      goto cleanup;
    }
    oldBitmap = SelectBitmap(memoryDC, bitmap);
    drawItem.CtlType = ODT_STATIC;
    drawItem.CtlID = IDC_SETTINGS_ROUTE_DIAGRAM;
    drawItem.hwndItem = hDiagram;
    drawItem.hDC = memoryDC;
    drawItem.rcItem = diagramRect;
    if (!mag_SettingsDrawRouteD2d(hDlg, &drawItem, dialog))
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The route DAG could not render to an isolated DIB."), reasonCount);
      }
      goto cleanup;
    }
    GdiFlush();
    for (pixelIndex = 0;
         pixelIndex < (UINT)(RECTWIDTH(diagramRect) * RECTHEIGHT(diagramRect));
         ++pixelIndex)
    {
      const BYTE blue = pixels[(SIZE_T)pixelIndex * 4U + 0U];
      const BYTE green = pixels[(SIZE_T)pixelIndex * 4U + 1U];
      const BYTE red = pixels[(SIZE_T)pixelIndex * 4U + 2U];

      if (red < 45 && green >= 70 && green <= 145 && blue >= 135)
      {
        ++activePixelCount;
      }
      if (red < 100 && green < 100 && blue < 100)
      {
        ++textPixelCount;
      }
    }
    if (activePixelCount < 64 || textPixelCount < 64)
    {
      if (reason && reasonCount)
      {
        _sntprintf_s(
          reason,
          reasonCount,
          _TRUNCATE,
          TEXT("The route DAG lacks visible active edges/nodes or DirectWrite values (active=%u text=%u)."),
          activePixelCount,
          textPixelCount);
      }
      goto cleanup;
    }

    for (index = 0; index < MAG_SETTINGS_PAGE_COUNT; ++index)
    {
      HWND hTab = GetDlgItem(hDlg, IDC_SETTINGS_TAB);
      TCHAR capturePath[1024];

      if (hTab)
      {
        TabCtrl_SetCurSel(hTab, (int)index);
      }
      mag_SettingsShowPage(hDlg, index);
      UpdateWindow(hDlg);
      if (MAG_SETTINGS_PAGE_PACING == index &&
          GetEnvironmentVariable(
            TEXT("MAG_SETTINGS_PACING_BMP"),
            capturePath,
            ARRAYSIZE(capturePath)) > 0 &&
          !mag_SettingsWriteWindowBmp(hDlg, capturePath))
      {
        if (reason && reasonCount)
        {
          lstrcpyn(
            reason,
            TEXT("The private-desktop Pacing help illustration could not be captured."),
            reasonCount);
        }
        goto cleanup;
      }
    }
    mag_SettingsShowPage(hDlg, MAG_SETTINGS_PAGE_ROUTE);

    for (index = 0; index < ARRAYSIZE(comboIds); ++index)
    {
      HWND combo = GetDlgItem(hDlg, comboIds[index]);
      COMBOBOXINFO comboInfo = { sizeof(comboInfo) };
      RECT listRect;
      MONITORINFO monitorInfo = { sizeof(monitorInfo) };
      HMONITOR monitor;

      if (!combo)
      {
        continue;
      }
      ShowWindow(combo, SW_SHOWNA);
      SendMessage(combo, CB_SHOWDROPDOWN, TRUE, 0);
      monitor = MonitorFromWindow(hDlg, MONITOR_DEFAULTTONEAREST);
      if (!GetComboBoxInfo(combo, &comboInfo) || !comboInfo.hwndList ||
          !GetWindowRect(comboInfo.hwndList, &listRect) ||
          !GetMonitorInfo(monitor, &monitorInfo) ||
          listRect.left < monitorInfo.rcWork.left ||
          listRect.top < monitorInfo.rcWork.top ||
          listRect.right > monitorInfo.rcWork.right ||
          listRect.bottom > monitorInfo.rcWork.bottom)
      {
        SendMessage(combo, CB_SHOWDROPDOWN, FALSE, 0);
        if (reason && reasonCount)
        {
          _sntprintf_s(
            reason,
            reasonCount,
            _TRUNCATE,
            TEXT("Settings combo %u opens outside the monitor work area: list=(%ld,%ld)-(%ld,%ld), work=(%ld,%ld)-(%ld,%ld)."),
            comboIds[index],
            listRect.left,
            listRect.top,
            listRect.right,
            listRect.bottom,
            monitorInfo.rcWork.left,
            monitorInfo.rcWork.top,
            monitorInfo.rcWork.right,
            monitorInfo.rcWork.bottom);
        }
        goto cleanup;
      }
      SendMessage(combo, CB_SHOWDROPDOWN, FALSE, 0);
    }
    result = TRUE;

cleanup:
    if (oldBitmap && memoryDC)
    {
      SelectBitmap(memoryDC, oldBitmap);
    }
    if (bitmap)
    {
      DeleteBitmap(bitmap);
    }
    if (memoryDC)
    {
      DeleteDC(memoryDC);
    }
    if (hDlg)
    {
      DestroyWindow(hDlg);
    }
    return result;
}

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const MARGINS margins = { 1, 1, 1, 1 };

    UNREFERENCED_PARAMETER(lpCreateStruct);

    SetCurrentProcessEfficiencyQoS();
    mag_SetTaskbarIcon(hWnd);
    mag_AddTrayIcon(hWnd);
    mag_UpdateWindowOutlineColor(hWnd);
    DwmExtendFrameIntoClientArea(hWnd, &margins);

    lpsd->graphicsApi = GRAPHICS_API_OPENGL;
    lpsd->uiGraphicsApi = UI_GRAPHICS_API_NATIVE;
    lpsd->textRenderer = TEXT_RENDERER_DIRECTWRITE;
    lpsd->captureApi = CAPTURE_API_GDI_BITBLT;
    lpsd->viewMode = MAG_VIEW_WINDOW;
    lpsd->fMouseRelativeZoom = FALSE;
    magPresentationSettingsSetDefaults(&lpsd->presentationSettings);
    if (magGraphicsIsInputDesktop())
    {
      mag_LoadSettings(lpsd);
    }
    else
    {
      lpsd->graphicsApi = GRAPHICS_API_GDI;
      lpsd->uiGraphicsApi = UI_GRAPHICS_API_DIRECT2D;
      lpsd->textRenderer = TEXT_RENDERER_DIRECTWRITE;
      lpsd->fAutoSettingsPreset = TRUE;
    }

    renderInit(hWnd);
    render_minimapNotifyActivity(hWnd);

    //SetTimer(hWnd, 1, 13, NULL);
    SetWindowDisplayAffinity(hWnd, WDA_EXCLUDEFROMCAPTURE);
    DwmEnableWindowComposition(hWnd, TRUE);

    return TRUE;
}

void mag_SetTaskbarIcon(HWND hWnd)
{
    HINSTANCE hInstance = GetModuleHandle(NULL);
    HICON hIcon = (HICON)LoadImage(
      hInstance,
      MAKEINTRESOURCE(IDI_APPICON),
      IMAGE_ICON,
      GetSystemMetrics(SM_CXICON),
      GetSystemMetrics(SM_CYICON),
      LR_DEFAULTCOLOR | LR_SHARED);
    HICON hIconSm = (HICON)LoadImage(
      hInstance,
      MAKEINTRESOURCE(IDI_APPICON),
      IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON),
      GetSystemMetrics(SM_CYSMICON),
      LR_DEFAULTCOLOR | LR_SHARED);

    if (hIcon)
    {
      SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
    }

    if (hIconSm)
    {
      SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSm);
    }
}

void mag_OnDestroy(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    mag_DeleteTrayIcon(hWnd);
    renderCleanup(hWnd);
    help_Cleanup();
    VirtualFree(lpsd, 0, MEM_RELEASE);
    PostQuitMessage(0);
}

void mag_OnActivate(HWND hWnd, UINT state, HWND hWndActDeact, BOOL fMinimized)
{
    UNREFERENCED_PARAMETER(hWndActDeact);

    if (!fMinimized &&
        (WA_ACTIVE == state || WA_CLICKACTIVE == state || WA_INACTIVE == state))
    {
      /* Activation is a content transition, not a reason to rebuild the
         non-client frame.  Restarting the presenter prevents an older queued
         frame from resurfacing as activation changes composition state. */
      renderSubmitStateTransitionFrame(hWnd, TRUE);
    }
}

void mag_OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    BeginPaint(hWnd, &ps);
    EndPaint(hWnd, &ps);
}

UINT mag_OnEraseBkgnd(HWND hWnd, HDC hDC)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(hDC);

    return 1;
}

BOOL mag_OnNCCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    LONG_PTR offset;

    SetLastError(NO_ERROR);
    
    offset = SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)lpCreateStruct->lpCreateParams);
    
    if ((0 == offset) && (NO_ERROR != GetLastError()))
    {
      return FALSE;
    }

    return FORWARD_WM_NCCREATE(hWnd, lpCreateStruct, DefWindowProc);
}

UINT mag_OnNCCalcSize(HWND hWnd, BOOL fCalcValidRects, NCCALCSIZE_PARAMS* lpcsp)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (fCalcValidRects)
    {
      SIZE proposedClientSize;

      if (IsMaximized(hWnd))
      {
        MONITORINFO mi = { sizeof(MONITORINFO) };

        if(GetMonitorInfo(
            MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), (LPMONITORINFO)&mi))
        {
          lpcsp->rgrc[0] = mi.rcWork;
        }
      }
      /*
       * The proposed rectangle is not committed yet.  Latch content for the
       * current client size before DWM switches geometry; the replacement
       * frame is submitted from WM_WINDOWPOSCHANGED after WM_SIZE has resized
       * all renderer resources.
       */
      proposedClientSize.cx = RECTWIDTH(lpcsp->rgrc[0]);
      proposedClientSize.cy = RECTHEIGHT(lpcsp->rgrc[0]);
      renderPrepareWindowResize(hWnd, proposedClientSize);
      return 0;
    }

    return 0;
}

UINT mag_OnNCHittest(HWND hWnd, int x, int y)
{
    RECT        rc = { 0 };
    const POINT cursor = { (LONG)x, (LONG)y };
    const SIZE  border =
    {
      (LONG)(GetSystemMetrics(SM_CXFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER)),
      (LONG)(GetSystemMetrics(SM_CYFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER))  // Padded border is symmetric for both x, y
    };

    if (mag_IsLensMode(hWnd))
    {
      return HTTRANSPARENT;
    }

    GetWindowRect(hWnd, &rc);

    enum region_mask
    {
      client = 0b0000,
      left = 0b0001,
      right = 0b0010,
      top = 0b0100,
      bottom = 0b1000,
    };

    CONST INT result =
      left * (cursor.x < (rc.left + border.cx)) |
      right * (cursor.x >= (rc.right - border.cx)) |
      top * (cursor.y < (rc.top + border.cy)) |
      bottom * (cursor.y >= (rc.bottom - border.cy));

    switch (result) {
    case left:           return HTLEFT;
    case right:          return HTRIGHT;
    case top:            return HTTOP;
    case bottom:         return HTBOTTOM;
    case top | left:     return HTTOPLEFT;
    case top | right:    return HTTOPRIGHT;
    case bottom | left:  return HTBOTTOMLEFT;
    case bottom | right: return HTBOTTOMRIGHT;
    case client:
    {
      POINT ptClient = cursor;

      if (ScreenToClient(hWnd, &ptClient) && render_minimapHitTest(hWnd, ptClient))
      {
        return HTCLIENT;
      }

      return HTCAPTION;
    }
    default:             return FORWARD_WM_NCHITTEST(hWnd, x, y, DefWindowProc);
    }
}

BOOL mag_OnNCActivate(HWND hWnd, BOOL fActive, HWND hwndActDeact, BOOL fMinimized)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(fActive);
    UNREFERENCED_PARAMETER(hwndActDeact);
    UNREFERENCED_PARAMETER(fMinimized);

    return TRUE;
}

void mag_OnNCRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT codeHitTest)
{
    UNREFERENCED_PARAMETER(fDoubleClick);
    UNREFERENCED_PARAMETER(codeHitTest);

    mag_ShowPopupMenu(hWnd, x, y);
}

void mag_OnTrayIcon(HWND hWnd, UINT id, UINT notification)
{
    POINT pt;

    if (MAG_TRAY_ICON_ID != id)
    {
      return;
    }

    if (WM_RBUTTONUP == notification || WM_CONTEXTMENU == notification)
    {
      if (GetCursorPos(&pt))
      {
        SetForegroundWindow(hWnd);
        mag_ShowPopupMenu(hWnd, pt.x, pt.y);
        PostMessage(hWnd, WM_NULL, 0, 0);
      }
    }
}

void mag_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch(id){
    case ID_CONTEXTMENU_WINDOW_MODE:
    {
      mag_SetViewMode(hWnd, MAG_VIEW_WINDOW);
      break;
    }
    case ID_CONTEXTMENU_FOLLOW_MOUSE:
    {
      mag_SetViewMode(hWnd, MAG_VIEW_FOLLOW_MOUSE);
      break;
    }
    case ID_CONTEXTMENU_LENS_MODE:
    {
      mag_SetViewMode(hWnd, MAG_VIEW_LENS);
      break;
    }
    case ID_CONTEXTMENU_SETTINGS:
    {
      mag_ShowSettingsDialog(hWnd);
      break;
    }
    case ID_CONTEXTMENU_HELP:
    {
      mag_ShowHelpMenu(hWnd, 0, 0);
      break;
    }
    case ID_CONTEXTMENU_CLOSE:
    {
      DestroyWindow(hWnd);
      break;
    }
    default:
      break;
    }

    FORWARD_WM_COMMAND(hWnd, id, hwndCtl, codeNotify, DefWindowProc);
}

void mag_OnKeyUp(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(fDown);

    if (lpsd && MAG_VIEW_LENS == lpsd->viewMode)
    {
      FORWARD_WM_KEYUP(hWnd, vk, cRepeat, flags, DefWindowProc);
      return;
    }

    if (!lpsd)
    {
      return;
    }

    switch (vk) {
    case VK_SPACE:
    {
      if (MAG_VIEW_FOLLOW_MOUSE == lpsd->viewMode)
      {
        mag_SetViewMode(hWnd, MAG_VIEW_WINDOW);
      }
      break;
    }
    case VK_ESCAPE:
    {
      if (GetForegroundWindow() == hWnd)
      {
        DestroyWindow(hWnd);
      }
      break;
    }
    default:
      break;
    }
}

void mag_OnTimer(HWND hWnd, UINT_PTR idEvent)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(idEvent);

    if (lpsd && !lpsd->fRenderMessageDriven)
    {
      mag_OnRender(hWnd);
    }
}

void mag_OnRender(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd || !IsWindowEnabled(hWnd) || lpsd->fGeometryTransition)
    {
      return;
    }

    if (InMenu() && !lpsd->fInSizeMove)
    {
      return;
    }

    if (lpsd->fTrackCursor)
    {
      mag_UpdateViewWindowStyle(hWnd);
    }

    if (MAG_VIEW_LENS == lpsd->viewMode)
    {
      mag_UpdateLensWindowPosition(hWnd);
    }

    if (lpsd->fInSizeMove)
    {
      renderSubmitLiveFrame(hWnd);
    }
    else
    {
      renderSubmit(hWnd);
    }
}

void mag_OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    POINT ptClient = { xPos, yPos };

    UNREFERENCED_PARAMETER(fwKeys);

    if (ScreenToClient(hWnd, &ptClient) && PtInRect(&lpsd->rc, ptClient))
    {
      RECT rcSourceOld;
      RECT rcCapture;
      const LONG clientWidth = lpsd->bi.biWidth;
      const LONG clientHeight = lpsd->bi.biHeight;
      const BOOL fMouseRelativeZoom = lpsd->fMouseRelativeZoom && !lpsd->fTrackCursor;
      const BOOL fKeepSourceOrigin = !lpsd->fTrackCursor && lpsd->fUseSourceOrigin;
      const BOOL fAnchorZoom = fMouseRelativeZoom || fKeepSourceOrigin;
      const FLOAT fScaleScale = powf(-logf(0.001f * (1.0f - lpsd->fScale) + .575f), 6);
      const FLOAT fWheelSteps = (FLOAT)zDelta / (FLOAT)WHEEL_DELTA;
      FLOAT fScaleNew;
      FLOAT fTexScalerNew;

      render_minimapNotifyActivity(hWnd);

      if (clientWidth < 1 || clientHeight < 1)
      {
        return;
      }

      fScaleNew = CLAMP(
        lpsd->fScale + ((0.0f < fScaleScale) ? fScaleScale : -1.0f + fScaleScale) * -fWheelSteps * MOUSE_WHEEL_ZOOM_STEP_SCALE,
        0.001f,
        1.0f);
      fTexScalerNew = 1.0f + (15.0f * (1.0f - fScaleNew));

      if (fAnchorZoom)
      {
        render_computeSourceRect(hWnd, &rcSourceOld);
        if (IsRectEmpty(&rcSourceOld))
        {
          return;
        }

      }

      lpsd->fScale = fScaleNew;
      lpsd->fTexScaler = fTexScalerNew;

      if (fAnchorZoom && (lpsd->fTexScaler > 1.0001f || lpsd->fSourceOriginPinned))
      {
        const SIZE clientSize = { clientWidth, clientHeight };
        const DOUBLE anchorU = fMouseRelativeZoom
          ? (DOUBLE)ptClient.x / (DOUBLE)clientWidth
          : 0.5;
        const DOUBLE anchorV = fMouseRelativeZoom
          ? (DOUBLE)ptClient.y / (DOUBLE)clientHeight
          : 0.5;
        POINT sourceOrigin;

        mag_GetCaptureRect(lpsd, &rcCapture);
        if (!render_calculateZoomedSourceOrigin(
              &rcSourceOld,
              clientSize,
              lpsd->fTexScaler,
              anchorU,
              anchorV,
              &rcCapture,
              &sourceOrigin))
        {
          return;
        }

        lpsd->ptSourceOrigin = sourceOrigin;
        lpsd->fUseSourceOrigin = TRUE;
        if (!fKeepSourceOrigin)
        {
          lpsd->fSourceOriginPinned = FALSE;
        }
      }
      else
      {
        lpsd->fUseSourceOrigin = FALSE;
        lpsd->fSourceOriginPinned = FALSE;
      }

      if (MAG_VIEW_LENS == lpsd->viewMode)
      {
        mag_UpdateLensWindowPosition(hWnd);
      }
      renderSubmitStateTransitionFrame(hWnd, TRUE);
    }
}

static void mag_Settings_OnPresentationStatus(HWND hDlg)
{
    mag_SettingsRefresh(hDlg, FALSE);
}

void mag_OnLButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT keyFlags)
{
    POINT ptClient = { x, y };

    UNREFERENCED_PARAMETER(fDoubleClick);
    UNREFERENCED_PARAMETER(keyFlags);

    render_minimapNotifyActivity(hWnd);

    if (render_minimapBeginDrag(hWnd, ptClient))
    {
      SetCapture(hWnd);
      renderRender(hWnd);
    }
}

void mag_OnLButtonUp(HWND hWnd, int x, int y, UINT keyFlags)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);
    UNREFERENCED_PARAMETER(keyFlags);

    render_minimapNotifyActivity(hWnd);

    if (lpsd && lpsd->fMiniMapDragging)
    {
      render_minimapEndDrag(hWnd);
      if (GetCapture() == hWnd)
      {
        ReleaseCapture();
      }
      renderRender(hWnd);
    }
}

void mag_OnMouseMove(HWND hWnd, int x, int y, UINT keyFlags)
{
    POINT ptClient = { x, y };
    POINT ptScreen = ptClient;

    UNREFERENCED_PARAMETER(keyFlags);

    if (ClientToScreen(hWnd, &ptScreen))
    {
      mag_NotifyMiniMapCursorActivity(hWnd, ptScreen);
    }

    if (render_minimapDrag(hWnd, ptClient))
    {
      renderRender(hWnd);
    }
}

void mag_OnNCMouseMove(HWND hWnd, int x, int y, UINT codeHitTest)
{
    POINT ptScreen = { x, y };

    UNREFERENCED_PARAMETER(codeHitTest);

    mag_NotifyMiniMapCursorActivity(hWnd, ptScreen);
}

void mag_OnDwmColorizationColorChanged(HWND hWnd, DWORD colorizationColor, BOOL fOpaqueBlend)
{
    UNREFERENCED_PARAMETER(colorizationColor);
    UNREFERENCED_PARAMETER(fOpaqueBlend);

    mag_UpdateWindowOutlineColor(hWnd);
    renderRender(hWnd);
}

void mag_OnSysColorChange(HWND hWnd)
{
    mag_UpdateWindowOutlineColor(hWnd);
    renderRender(hWnd);
}

void mag_NotifyMiniMapCursorActivity(HWND hWnd, POINT ptScreen)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd)
    {
      return;
    }

    if (!lpsd->fMiniMapHaveLastCursor ||
        lpsd->ptMiniMapLastCursor.x != ptScreen.x ||
        lpsd->ptMiniMapLastCursor.y != ptScreen.y)
    {
      lpsd->fMiniMapHaveLastCursor = TRUE;
      lpsd->ptMiniMapLastCursor = ptScreen;
      render_minimapNotifyActivity(hWnd);
    }
}

void mag_OnCaptureChanged(HWND hWnd, HWND hwndNewCapture)
{
    UNREFERENCED_PARAMETER(hwndNewCapture);

    render_minimapEndDrag(hWnd);
}

void mag_OnSize(HWND hWnd, UINT state, int cx, int cy)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (SIZE_MINIMIZED == state)
    {
      if (lpsd)
      {
        lpsd->fPresentedContentValid = FALSE;
      }
      return;
    }

    if (lpsd && cx > 0 && cy > 0)
    {
      lpsd->deferredClientSize.cx = cx;
      lpsd->deferredClientSize.cy = cy;
      lpsd->fDeferredResize = TRUE;
    }
}

void mag_OnEnterMenuLoop(HWND hWnd, BOOL fIsTrackPopupMenu)
{
    UNREFERENCED_PARAMETER(fIsTrackPopupMenu);

    SetTimer(hWnd, 0x69, USER_TIMER_MINIMUM, 0);
}

void mag_OnExitMenuLoop(HWND hWnd,BOOL fIsShortcutMenu)
{
    UNREFERENCED_PARAMETER(fIsShortcutMenu);

    KillTimer(hWnd, 0x69);
}

void mag_OnEnterSizeMove(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd)
    {
      lpsd->fMiniMapHoldVisible = TRUE;
      lpsd->fInSizeMove = TRUE;
    }
    render_minimapNotifyActivity(hWnd);
    SetTimer(hWnd, 0x69, USER_TIMER_MINIMUM, 0);
}

void mag_OnExitSizeMove(HWND hWnd)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd)
    {
      lpsd->fMiniMapHoldVisible = FALSE;
      lpsd->fInSizeMove = FALSE;
      /*
       * WM_EXITSIZEMOVE is not a rendering checkpoint.  Every committed
       * geometry epoch must already have been resized and presented from its
       * WM_WINDOWPOSCHANGED.  Doing that work here masks stale modal-loop
       * frames and is exactly the delayed-update failure this contract is
       * intended to prevent.
       */
      if (lpsd->fDeferredResize || lpsd->fGeometryTransition)
      {
        lpsd->fResizeContractViolation = TRUE;
      }
      lpsd->fGeometryTransition = FALSE;
    }
    render_minimapNotifyActivity(hWnd);
    KillTimer(hWnd, 0x69);
}

void mag_OnWindowPosChanged(HWND hWnd, const WINDOWPOS* lpwndpos)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    const BOOL fMoved = !(lpwndpos->flags & SWP_NOMOVE);
    const BOOL fSized = !(lpwndpos->flags & SWP_NOSIZE);
    const BOOL fActualMove = fMoved && !fSized;
    RECT rcSourceOld;
    RECT rcCapture;
    BOOL fPreservePinnedCenter = FALSE;

    if (lpsd &&
        fSized &&
        !fActualMove &&
        lpsd->fSourceOriginPinned &&
        lpsd->fUseSourceOrigin &&
        lpsd->bi.biWidth > 0 &&
        lpsd->bi.biHeight > 0)
    {
      render_computeSourceRect(hWnd, &rcSourceOld);
      if (!IsRectEmpty(&rcSourceOld))
      {
        fPreservePinnedCenter = TRUE;
      }
    }

    DefWindowProc(hWnd, WM_WINDOWPOSCHANGED, 0, (LPARAM)lpwndpos);
    
    if (lpsd &&
        lpsd->graphicsBackend &&
        (fMoved || fSized))
    {
      render_minimapNotifyActivity(hWnd);

      if (fSized && !renderResizeCapture(hWnd))
      {
        lpsd->fResizeContractViolation = TRUE;
        lpsd->fGeometryTransition = FALSE;
        return;
      }

      if (fActualMove)
      {
        lpsd->fUseSourceOrigin = FALSE;
        lpsd->fSourceOriginPinned = FALSE;
      }
      else if (fPreservePinnedCenter &&
               lpsd->bi.biWidth > 0 &&
               lpsd->bi.biHeight > 0)
      {
        const SIZE clientSize =
        {
          lpsd->bi.biWidth,
          lpsd->bi.biHeight,
        };
        POINT sourceOrigin;

        mag_GetCaptureRect(lpsd, &rcCapture);
        if (!render_calculateZoomedSourceOrigin(
              &rcSourceOld,
              clientSize,
              lpsd->fTexScaler,
              0.5,
              0.5,
              &rcCapture,
              &sourceOrigin))
        {
          lpsd->fResizeContractViolation = TRUE;
          lpsd->fGeometryTransition = FALSE;
          return;
        }
        lpsd->ptSourceOrigin = sourceOrigin;
        lpsd->fUseSourceOrigin = TRUE;
        lpsd->fSourceOriginPinned = TRUE;
      }
      else if (!lpsd->fSourceOriginPinned)
      {
        lpsd->fUseSourceOrigin = FALSE;
      }
      if (fSized)
      {
        renderPresentCommittedGeometry(hWnd);
        lpsd->fGeometryTransition = FALSE;
      }
      else
      {
        /* A pure move carries the visual with the HWND, but a magnifier's
           source and exclusion relationship also move.  Submit immediately
           with restart semantics so no queued pre-move frame can reappear. */
        renderSubmitStateTransitionFrame(hWnd, TRUE);
      }
    }
}

LRESULT CALLBACK mag_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    HANDLE_MSG(hWnd,  WM_CREATE,            mag_OnCreate);
    HANDLE_MSG(hWnd,  WM_DESTROY,           mag_OnDestroy);
    HANDLE_MSG(hWnd,  WM_SIZE,              mag_OnSize);
    HANDLE_MSG(hWnd,  WM_ACTIVATE,          mag_OnActivate);
    HANDLE_MSG(hWnd,  WM_PAINT,             mag_OnPaint);
    HANDLE_MSG(hWnd,  WM_ERASEBKGND,        mag_OnEraseBkgnd);
    HANDLE_MSG(hWnd,  WM_NCCREATE,          mag_OnNCCreate);
    HANDLE_MSG(hWnd,  WM_NCCALCSIZE,        mag_OnNCCalcSize);
    HANDLE_MSG(hWnd,  WM_NCHITTEST,         mag_OnNCHittest);
    HANDLE_MSG(hWnd,  WM_NCACTIVATE,        mag_OnNCActivate);
    HANDLE_MSG(hWnd,  WM_NCRBUTTONDOWN,     mag_OnNCRButtonDown);
    HANDLE_MSG(hWnd,  WM_COMMAND,           mag_OnCommand);
    HANDLE_MSG(hWnd,  WM_KEYUP,             mag_OnKeyUp);
    HANDLE_MSG(hWnd,  WM_TIMER,             mag_OnTimer);
    HANDLE_MSG(hWnd,  WM_MAG_RENDER,        mag_OnRender);
    HANDLE_MSG(hWnd,  WM_MOUSEWHEEL,        mag_OnMouseWheel);
    HANDLE_MSG(hWnd,  WM_LBUTTONDOWN,       mag_OnLButtonDown);
    HANDLE_MSG(hWnd,  WM_LBUTTONUP,         mag_OnLButtonUp);
    HANDLE_MSG(hWnd,  WM_MOUSEMOVE,         mag_OnMouseMove);
    HANDLE_MSG(hWnd,  WM_NCMOUSEMOVE,       mag_OnNCMouseMove);
    HANDLE_MSG(hWnd,  WM_MAG_TRAYICON,      mag_OnTrayIcon);
    HANDLE_MSG(hWnd,  WM_DWMCOLORIZATIONCOLORCHANGED, mag_OnDwmColorizationColorChanged);
    HANDLE_MSG(hWnd,  WM_SYSCOLORCHANGE,    mag_OnSysColorChange);
    HANDLE_MSG(hWnd,  WM_CAPTURECHANGED,    mag_OnCaptureChanged);
    HANDLE_MSG(hWnd,  WM_ENTERMENULOOP,     mag_OnEnterMenuLoop);
    HANDLE_MSG(hWnd,  WM_EXITMENULOOP,      mag_OnExitMenuLoop);
    HANDLE_MSG(hWnd,  WM_ENTERSIZEMOVE,     mag_OnEnterSizeMove);
    HANDLE_MSG(hWnd,  WM_EXITSIZEMOVE,      mag_OnExitSizeMove);
    HANDLE_MSG(hWnd,  WM_WINDOWPOSCHANGED,  mag_OnWindowPosChanged);
    FORWARD_MSG(hWnd, message,              DefWindowProc);
    }
}

ATOM mag_RegisterClassEx(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = { sizeof(wcex) };

    wcex.style = 0;
    wcex.lpfnWndProc = mag_WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = (HICON)LoadImage(
      hInstance,
      MAKEINTRESOURCE(IDI_APPICON),
      IMAGE_ICON,
      GetSystemMetrics(SM_CXICON),
      GetSystemMetrics(SM_CYICON),
      LR_DEFAULTCOLOR | LR_SHARED);
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = NULL;
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszClassName = TEXT("magWindowClass");
    //wcex.lpszClassName  = szWindowClass;
    wcex.hIconSm = (HICON)LoadImage(
      hInstance,
      MAKEINTRESOURCE(IDI_APPICON),
      IMAGE_ICON,
      GetSystemMetrics(SM_CXSMICON),
      GetSystemMetrics(SM_CYSMICON),
      LR_DEFAULTCOLOR | LR_SHARED);

    return RegisterClassEx(&wcex);
}

HWND magInitInstance(HINSTANCE hInstance, int nCmdShow)
{
    ATOM atm;
    HWND hWnd;

    UNREFERENCED_PARAMETER(nCmdShow);

    hWnd = CreateWindowEx(
      WS_EX_TOPMOST |
      //WS_EX_PALETTEWINDOW |
      WS_EX_APPWINDOW |
      WS_EX_CONTEXTHELP |
      WS_EX_DLGMODALFRAME,
      (LPTSTR)(atm = mag_RegisterClassEx(hInstance)),
      TEXT("magWindow"),
      //WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_BORDER,
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      //0,
      0,
      CW_USEDEFAULT,
      //0,
      100,
      NULL,
      NULL,
      hInstance,
      VirtualAlloc(NULL, sizeof(MAGSTATE), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!hWnd)
    {
      return NULL;
    }

    return hWnd;
}
