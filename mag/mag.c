#include "mag.h"
#include "render.h"
#include "help.h"

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
  TCHAR catalogReason[MAG_PRESENTATION_REASON_LENGTH];
} MAGSETTINGSDIALOGSTATE;

#define MAG_SETTINGS_EXPLICIT_ITEM_BASE 0x10000U

#pragma comment(lib, "Advapi32")

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
static void mag_Settings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify);
static void mag_Settings_OnDestroy(HWND hDlg);
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

    if (!lpsd || ERROR_SUCCESS != RegCreateKeyEx(
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
      TCHAR label[320];

      if (!adapter)
      {
        continue;
      }
      _sntprintf_s(
        label,
        ARRAYSIZE(label),
        _TRUNCATE,
        TEXT("%s | %s | %ld,%ld %ldx%ld | %u Hz%s"),
        adapter->description,
        output->deviceName,
        output->desktopCoordinates.left,
        output->desktopCoordinates.top,
        RECTWIDTH(output->desktopCoordinates),
        RECTHEIGHT(output->desktopCoordinates),
        output->refreshDenominator
          ? output->refreshNumerator / output->refreshDenominator
          : 0,
        output->hdr ? TEXT(" | HDR") : TEXT(""));
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
    BOOL translated;
    TCHAR colorText[32];
    TCHAR* end = NULL;
    ULONG color;

    if (!settings)
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
    settings->bufferCount = GetDlgItemInt(hDlg, IDC_SETTINGS_BUFFER_COUNT, &translated, FALSE);
    if (!translated || settings->bufferCount < 2 || settings->bufferCount > 16)
    {
      return FALSE;
    }
    settings->maximumFrameLatency = GetDlgItemInt(hDlg, IDC_SETTINGS_FRAME_LATENCY, &translated, FALSE);
    if (!translated || settings->maximumFrameLatency < 1 || settings->maximumFrameLatency > 16)
    {
      return FALSE;
    }
    settings->syncInterval = GetDlgItemInt(hDlg, IDC_SETTINGS_SYNC_INTERVAL, &translated, FALSE);
    if (!translated || settings->syncInterval > 4)
    {
      return FALSE;
    }
    value = GetDlgItemInt(hDlg, IDC_SETTINGS_CONSTANT_ALPHA, &translated, FALSE);
    if (!translated || value > 255)
    {
      return FALSE;
    }
    settings->constantAlpha = (BYTE)value;

    GetDlgItemText(hDlg, IDC_SETTINGS_COLOR_KEY, colorText, ARRAYSIZE(colorText));
    color = _tcstoul(colorText, &end, 16);
    if (!colorText[0] || !end || *end || color > 0xFFFFFFUL)
    {
      return FALSE;
    }
    settings->colorKey = RGB(
      (color >> 16) & 0xFF,
      (color >> 8) & 0xFF,
      color & 0xFF);
    return TRUE;
}

void mag_UpdateSettingsDialogState(HWND hDlg)
{
    MAGSETTINGSDIALOGSTATE* dialog = (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
    UINT graphicsId = GRAPHICS_API_OPENGL;
    UINT captureId = CAPTURE_API_GDI_BITBLT;
    UINT uiId = UI_GRAPHICS_API_NATIVE;
    UINT textId = TEXT_RENDERER_DIRECTWRITE;
    MAGPRESENTATIONSETTINGS requested;
    MAGPRESENTATIONSETTINGS resolved;
    MAGPRESENTATIONSTATUS status;
    BOOL fGraphicsImplemented = FALSE;
    BOOL fCaptureImplemented = FALSE;
    BOOL fUiImplemented = FALSE;
    BOOL fTextImplemented = FALSE;
    BOOL fPresentationValid;
    BOOL fPresentationSupported = FALSE;
    BOOL fValid;

    fValid =
      mag_GetSelectedGraphicsOption(hDlg, IDC_SETTINGS_GRAPHICS_API, &graphicsId, &fGraphicsImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), &captureId, &fCaptureImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_UI_API, g_uiGraphicsApiOptions, ARRAYSIZE(g_uiGraphicsApiOptions), &uiId, &fUiImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_TEXT_RENDERER, g_textRendererOptions, ARRAYSIZE(g_textRendererOptions), &textId, &fTextImplemented);
    fPresentationValid = mag_GetPresentationDialogSettings(hDlg, dialog, &requested);
    if (fValid && fPresentationValid && dialog && dialog->catalogValid &&
        fGraphicsImplemented && fCaptureImplemented && fUiImplemented && fTextImplemented)
    {
      fPresentationSupported = magPresentationResolve(
        dialog->owner,
        (GRAPHICSAPI)graphicsId,
        (CAPTUREAPI)captureId,
        (UIGRAPHICSAPI)uiId,
        (TEXTRENDERER)textId,
        &requested,
        &dialog->catalog,
        &resolved,
        &status) && status.configurationSupported && status.flickerFree;
    }

    EnableWindow(
      GetDlgItem(hDlg, IDOK),
      fValid && fPresentationValid && fGraphicsImplemented && fCaptureImplemented &&
        fUiImplemented && fTextImplemented && fPresentationSupported);

    if (!fValid)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("Select graphics, capture, UI, and text renderers."));
    }
    else if (!fPresentationValid)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("Select valid presentation values. Buffers 2-16, latency 1-16, sync 0-4, alpha 0-255, color RRGGBB."));
    }
    else if (!dialog || !dialog->catalogValid)
    {
      SetDlgItemText(
        hDlg,
        IDC_SETTINGS_STATUS,
        dialog && dialog->catalogReason[0]
          ? dialog->catalogReason
          : TEXT("Display and hardware adapters could not be enumerated."));
    }
    else if (!fGraphicsImplemented || !fCaptureImplemented || !fUiImplemented || !fTextImplemented)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("The selected renderer is unavailable on this system."));
    }
    else if (!fPresentationSupported)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, status.reason[0]
        ? status.reason
        : TEXT("This combination cannot guarantee flicker-free presentation and is unavailable."));
    }
    else
    {
      TCHAR summary[768];

      _sntprintf_s(
        summary,
        ARRAYSIZE(summary),
        _TRUNCATE,
        TEXT("%s Copy class: %s. Flicker-free resize contract: required and supported."),
        status.reason,
        magCopyClassName(status.copyClass));
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, summary);
    }
}

static BOOL mag_Settings_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
    HWND hOwner = (HWND)lParam;
    LPMAGSTATE lpsd;
    MAGSETTINGSDIALOGSTATE* dialog;
    MAGPRESENTATIONSETTINGS defaults;
    const MAGPRESENTATIONSETTINGS* presentation;
    UINT graphicsApi;
    UINT captureApi;
    UINT uiApi;
    UINT textRenderer;
    TCHAR colorKey[16];

    UNREFERENCED_PARAMETER(hwndFocus);

    dialog = (MAGSETTINGSDIALOGSTATE*)HeapAlloc(
      GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*dialog));
    if (!dialog)
    {
      EndDialog(hDlg, IDCANCEL);
      return TRUE;
    }
    dialog->owner = hOwner;
    dialog->catalogValid = magAdapterCatalogEnumerate(
      &dialog->catalog,
      dialog->catalogReason,
      ARRAYSIZE(dialog->catalogReason));
    SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)dialog);
    lpsd = (LPMAGSTATE)GetWindowLongPtr(hOwner, GWLP_USERDATA);
    graphicsApi = (lpsd && lpsd->graphicsApi < GRAPHICS_API_COUNT) ? lpsd->graphicsApi : GRAPHICS_API_OPENGL;
    captureApi = (lpsd && lpsd->captureApi < CAPTURE_API_COUNT) ? lpsd->captureApi : CAPTURE_API_GDI_BITBLT;
    uiApi = (lpsd && lpsd->uiGraphicsApi < UI_GRAPHICS_API_COUNT) ? lpsd->uiGraphicsApi : UI_GRAPHICS_API_NATIVE;
    textRenderer = (lpsd && lpsd->textRenderer < TEXT_RENDERER_COUNT) ? lpsd->textRenderer : TEXT_RENDERER_DIRECTWRITE;
    magPresentationSettingsSetDefaults(&defaults);
    presentation = lpsd ? &lpsd->presentationSettings : &defaults;

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
    mag_AddDisplayAdapterOptions(hDlg, dialog, &presentation->display);
    mag_AddHardwareAdapterOptions(hDlg, dialog, &presentation->hardware);
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
    SetDlgItemInt(hDlg, IDC_SETTINGS_BUFFER_COUNT, presentation->bufferCount, FALSE);
    SetDlgItemInt(hDlg, IDC_SETTINGS_FRAME_LATENCY, presentation->maximumFrameLatency, FALSE);
    SetDlgItemInt(hDlg, IDC_SETTINGS_SYNC_INTERVAL, presentation->syncInterval, FALSE);
    SetDlgItemInt(hDlg, IDC_SETTINGS_CONSTANT_ALPHA, presentation->constantAlpha, FALSE);
    _sntprintf_s(
      colorKey,
      ARRAYSIZE(colorKey),
      _TRUNCATE,
      TEXT("%02X%02X%02X"),
      GetRValue(presentation->colorKey),
      GetGValue(presentation->colorKey),
      GetBValue(presentation->colorKey));
    SetDlgItemText(hDlg, IDC_SETTINGS_COLOR_KEY, colorKey);
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_MOUSE_RELATIVE_ZOOM,
      BM_SETCHECK,
      (lpsd && lpsd->fMouseRelativeZoom) ? BST_CHECKED : BST_UNCHECKED,
      0);
    mag_UpdateSettingsDialogState(hDlg);

    return TRUE;
}

static void mag_Settings_OnDestroy(HWND hDlg)
{
    MAGSETTINGSDIALOGSTATE* dialog = (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);

    SetWindowLongPtr(hDlg, DWLP_USER, 0);
    if (dialog)
    {
      HeapFree(GetProcessHeap(), 0, dialog);
    }
}

static void mag_Settings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);

    switch (id)
    {
    case IDOK:
      {
        MAGSETTINGSDIALOGSTATE* dialog = (MAGSETTINGSDIALOGSTATE*)GetWindowLongPtr(hDlg, DWLP_USER);
        HWND hOwner = dialog ? dialog->owner : NULL;
        LPMAGSTATE lpsd;
        MAGPRESENTATIONSETTINGS presentation;
        UINT graphicsApi = GRAPHICS_API_OPENGL;
        UINT captureApi = CAPTURE_API_GDI_BITBLT;
        UINT uiApi = UI_GRAPHICS_API_NATIVE;
        UINT textRenderer = TEXT_RENDERER_DIRECTWRITE;
        BOOL fGraphicsImplemented = FALSE;
        BOOL fCaptureImplemented = FALSE;
        BOOL fUiImplemented = FALSE;
        BOOL fTextImplemented = FALSE;
        BOOL fMouseRelativeZoom = BST_CHECKED == SendDlgItemMessage(hDlg, IDC_SETTINGS_MOUSE_RELATIVE_ZOOM, BM_GETCHECK, 0, 0);

        if (!mag_GetSelectedGraphicsOption(hDlg, IDC_SETTINGS_GRAPHICS_API, &graphicsApi, &fGraphicsImplemented) ||
          !mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), &captureApi, &fCaptureImplemented) ||
          !mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_UI_API, g_uiGraphicsApiOptions, ARRAYSIZE(g_uiGraphicsApiOptions), &uiApi, &fUiImplemented) ||
          !mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_TEXT_RENDERER, g_textRendererOptions, ARRAYSIZE(g_textRendererOptions), &textRenderer, &fTextImplemented) ||
          !mag_GetPresentationDialogSettings(hDlg, dialog, &presentation) ||
          !fGraphicsImplemented ||
          !fCaptureImplemented ||
          !fUiImplemented ||
          !fTextImplemented)
        {
          MessageBox(hDlg, TEXT("The selected renderer is unavailable on this system."), TEXT("Settings"), MB_OK | MB_ICONINFORMATION);
          mag_UpdateSettingsDialogState(hDlg);
          return;
        }

        lpsd = (LPMAGSTATE)GetWindowLongPtr(hOwner, GWLP_USERDATA);
        if (lpsd)
        {
          if (graphicsApi < GRAPHICS_API_COUNT &&
              uiApi < UI_GRAPHICS_API_COUNT &&
              textRenderer < TEXT_RENDERER_COUNT)
          {
            TCHAR reason[256];

            if (!renderApplyFullSettings(
                  hOwner,
                  (GRAPHICSAPI)graphicsApi,
                  (CAPTUREAPI)captureApi,
                  (UIGRAPHICSAPI)uiApi,
                  (TEXTRENDERER)textRenderer,
                  &presentation,
                  reason,
                  ARRAYSIZE(reason)))
            {
              MessageBox(
                hDlg,
                reason[0] ? reason : TEXT("The selected graphics API could not be applied."),
                TEXT("Settings"),
                MB_OK | MB_ICONERROR);
              return;
            }
          }

          lpsd->fMouseRelativeZoom = fMouseRelativeZoom;
          if (!lpsd->fMouseRelativeZoom && !lpsd->fSourceOriginPinned)
          {
            lpsd->fUseSourceOrigin = FALSE;
          }

          mag_SaveSettings(lpsd);
          renderRender(hOwner);
        }

        EndDialog(hDlg, IDOK);
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
    case IDC_SETTINGS_STRICT_TARGET:
    case IDC_SETTINGS_ALLOW_TEARING:
    case IDC_SETTINGS_BUFFER_COUNT:
    case IDC_SETTINGS_FRAME_LATENCY:
    case IDC_SETTINGS_SYNC_INTERVAL:
    case IDC_SETTINGS_CONSTANT_ALPHA:
    case IDC_SETTINGS_COLOR_KEY:
      {
        if (CBN_SELCHANGE == codeNotify || BN_CLICKED == codeNotify || EN_CHANGE == codeNotify)
        {
          mag_UpdateSettingsDialogState(hDlg);
        }
        return;
      }
    case IDCANCEL:
      {
        EndDialog(hDlg, IDCANCEL);
        return;
      }
    default:
      return;
    }
}

static INT_PTR CALLBACK mag_SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    HANDLE_DIALOG_MSG(hDlg, WM_INITDIALOG, mag_Settings_OnInitDialog);
    HANDLE_DIALOG_MSG(hDlg, WM_COMMAND,    mag_Settings_OnCommand);
    HANDLE_DIALOG_MSG(hDlg, WM_DESTROY,    mag_Settings_OnDestroy);
    }
    return FALSE;
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
    mag_LoadSettings(lpsd);

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
