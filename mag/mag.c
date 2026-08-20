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

#pragma comment(lib, "Advapi32")

#define MAG_SETTINGS_REGISTRY_KEY TEXT("Software\\mag")

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

void mag_LoadSettings(LPMAGSTATE lpsd)
{
    DWORD value;

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

      RegSetValueEx(key, TEXT("GraphicsApi"), 0, REG_DWORD, (const BYTE*)&graphicsApi, sizeof(graphicsApi));
      RegSetValueEx(key, TEXT("CaptureApi"), 0, REG_DWORD, (const BYTE*)&captureApi, sizeof(captureApi));
      RegSetValueEx(key, TEXT("UiGraphicsApi"), 0, REG_DWORD, (const BYTE*)&uiApi, sizeof(uiApi));
      RegSetValueEx(key, TEXT("TextRenderer"), 0, REG_DWORD, (const BYTE*)&textRenderer, sizeof(textRenderer));
      RegSetValueEx(key, TEXT("MouseRelativeZoom"), 0, REG_DWORD, (const BYTE*)&mouseRelativeZoom, sizeof(mouseRelativeZoom));
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
    DWORD dwExStyle;

    if (!lpsd)
    {
      return;
    }

    dwExStyle = GetWindowExStyle(hWnd);
    dwExStyle &= ~WS_EX_LAYERED;
    if (MAG_VIEW_LENS == lpsd->viewMode)
    {
      dwExStyle |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
    }
    else
    {
      dwExStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
    }

    SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);
    SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_NOACTIVATE);
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

void mag_UpdateSettingsDialogState(HWND hDlg)
{
    UINT graphicsId = GRAPHICS_API_OPENGL;
    UINT captureId = CAPTURE_API_GDI_BITBLT;
    UINT uiId = UI_GRAPHICS_API_NATIVE;
    UINT textId = TEXT_RENDERER_DIRECTWRITE;
    BOOL fGraphicsImplemented = FALSE;
    BOOL fCaptureImplemented = FALSE;
    BOOL fUiImplemented = FALSE;
    BOOL fTextImplemented = FALSE;
    BOOL fValid;

    fValid =
      mag_GetSelectedGraphicsOption(hDlg, IDC_SETTINGS_GRAPHICS_API, &graphicsId, &fGraphicsImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), &captureId, &fCaptureImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_UI_API, g_uiGraphicsApiOptions, ARRAYSIZE(g_uiGraphicsApiOptions), &uiId, &fUiImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_TEXT_RENDERER, g_textRendererOptions, ARRAYSIZE(g_textRendererOptions), &textId, &fTextImplemented);

    EnableWindow(
      GetDlgItem(hDlg, IDOK),
      fValid && fGraphicsImplemented && fCaptureImplemented && fUiImplemented && fTextImplemented);

    if (!fValid)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("Select graphics, capture, UI, and text renderers."));
    }
    else if (!fGraphicsImplemented || !fCaptureImplemented || !fUiImplemented || !fTextImplemented)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT("The selected renderer is unavailable on this system."));
    }
    else if (CAPTURE_API_DWM_THUMBNAIL == captureId ||
             CAPTURE_API_DWM_PRIVATE_VISUAL == captureId)
    {
      SetDlgItemText(
        hDlg,
        IDC_SETTINGS_STATUS,
        TEXT("DWM owns this visual; Graphics, UI, and text selections are its pixel fallback."));
    }
    else
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, TEXT(""));
    }
}

static BOOL mag_Settings_OnInitDialog(HWND hDlg, HWND hwndFocus, LPARAM lParam)
{
    HWND hOwner = (HWND)lParam;
    LPMAGSTATE lpsd;
    UINT graphicsApi;
    UINT captureApi;
    UINT uiApi;
    UINT textRenderer;

    UNREFERENCED_PARAMETER(hwndFocus);

    SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)hOwner);
    lpsd = (LPMAGSTATE)GetWindowLongPtr(hOwner, GWLP_USERDATA);
    graphicsApi = (lpsd && lpsd->graphicsApi < GRAPHICS_API_COUNT) ? lpsd->graphicsApi : GRAPHICS_API_OPENGL;
    captureApi = (lpsd && lpsd->captureApi < CAPTURE_API_COUNT) ? lpsd->captureApi : CAPTURE_API_GDI_BITBLT;
    uiApi = (lpsd && lpsd->uiGraphicsApi < UI_GRAPHICS_API_COUNT) ? lpsd->uiGraphicsApi : UI_GRAPHICS_API_NATIVE;
    textRenderer = (lpsd && lpsd->textRenderer < TEXT_RENDERER_COUNT) ? lpsd->textRenderer : TEXT_RENDERER_DIRECTWRITE;

    mag_AddGraphicsOptions(hDlg, IDC_SETTINGS_GRAPHICS_API, (GRAPHICSAPI)graphicsApi);
    mag_AddSettingsOptions(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), captureApi);
    mag_AddSettingsOptions(hDlg, IDC_SETTINGS_UI_API, g_uiGraphicsApiOptions, ARRAYSIZE(g_uiGraphicsApiOptions), uiApi);
    mag_AddSettingsOptions(hDlg, IDC_SETTINGS_TEXT_RENDERER, g_textRendererOptions, ARRAYSIZE(g_textRendererOptions), textRenderer);
    SendDlgItemMessage(
      hDlg,
      IDC_SETTINGS_MOUSE_RELATIVE_ZOOM,
      BM_SETCHECK,
      (lpsd && lpsd->fMouseRelativeZoom) ? BST_CHECKED : BST_UNCHECKED,
      0);
    mag_UpdateSettingsDialogState(hDlg);

    return TRUE;
}

static void mag_Settings_OnCommand(HWND hDlg, int id, HWND hwndCtl, UINT codeNotify)
{
    UNREFERENCED_PARAMETER(hwndCtl);

    switch (id)
    {
    case IDOK:
      {
        HWND hOwner = (HWND)GetWindowLongPtr(hDlg, DWLP_USER);
        LPMAGSTATE lpsd;
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

            if (!renderApplySettings(
                  hOwner,
                  (GRAPHICSAPI)graphicsApi,
                  (CAPTUREAPI)captureApi,
                  (UIGRAPHICSAPI)uiApi,
                  (TEXTRENDERER)textRenderer,
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
      {
        if (CBN_SELCHANGE == codeNotify)
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
    }
    return FALSE;
}

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(lpCreateStruct);

    SetCurrentProcessEfficiencyQoS();
    mag_SetTaskbarIcon(hWnd);
    mag_AddTrayIcon(hWnd);
    mag_UpdateWindowOutlineColor(hWnd);

    lpsd->graphicsApi = GRAPHICS_API_OPENGL;
    lpsd->uiGraphicsApi = UI_GRAPHICS_API_NATIVE;
    lpsd->textRenderer = TEXT_RENDERER_DIRECTWRITE;
    lpsd->captureApi = CAPTURE_API_GDI_BITBLT;
    lpsd->viewMode = MAG_VIEW_WINDOW;
    lpsd->fMouseRelativeZoom = FALSE;
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
    UNREFERENCED_PARAMETER(state);
    UNREFERENCED_PARAMETER(hWndActDeact);

    if (!fMinimized)
    {
      const MARGINS margins = { 1, 1, 1, 1 };
      DwmExtendFrameIntoClientArea(hWnd, &margins);

      SetWindowLongPtr(hWnd, GWL_EXSTYLE, WS_EX_TOPMOST | GetWindowExStyle(hWnd));
      SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
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
    if (fCalcValidRects)
    {
      UINT result = WVR_VALIDRECTS;
      SIZE proposedClientSize;

      if (IsMaximized(hWnd))
      {
        MONITORINFO mi = { sizeof(MONITORINFO) };

        if(GetMonitorInfo(
            MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), (LPMONITORINFO)&mi))
        {
          lpcsp->rgrc[0] = mi.rcWork;
          result = 0;
        }
      }
      else
      {
        lpcsp->rgrc[0].bottom += 1;
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
      return result;
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

    if (!lpsd || !IsWindowEnabled(hWnd))
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

    SetWindowAlwaysOnTop(hWnd, TRUE);

    renderSubmit(hWnd);
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
      DOUBLE anchorX = 0.0;
      DOUBLE anchorY = 0.0;

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

        if (fMouseRelativeZoom)
        {
          anchorX = (DOUBLE)rcSourceOld.left + ((DOUBLE)ptClient.x * (DOUBLE)RECTWIDTH(rcSourceOld) / (DOUBLE)clientWidth);
          anchorY = (DOUBLE)rcSourceOld.top + ((DOUBLE)ptClient.y * (DOUBLE)RECTHEIGHT(rcSourceOld) / (DOUBLE)clientHeight);
        }
        else
        {
          anchorX = (DOUBLE)rcSourceOld.left + ((DOUBLE)RECTWIDTH(rcSourceOld) / 2.0);
          anchorY = (DOUBLE)rcSourceOld.top + ((DOUBLE)RECTHEIGHT(rcSourceOld) / 2.0);
        }
      }

      lpsd->fScale = fScaleNew;
      lpsd->fTexScaler = fTexScalerNew;

      if (fAnchorZoom && (lpsd->fTexScaler > 1.0001f || lpsd->fSourceOriginPinned))
      {
        const LONG srcW = (lpsd->fTexScaler > 1.0001f) ? max(1, (LONG)(clientWidth / lpsd->fTexScaler)) : clientWidth;
        const LONG srcH = (lpsd->fTexScaler > 1.0001f) ? max(1, (LONG)(clientHeight / lpsd->fTexScaler)) : clientHeight;
        LONG srcX;
        LONG srcY;

        mag_GetCaptureRect(lpsd, &rcCapture);

        if (fMouseRelativeZoom)
        {
          srcX = (LONG)(anchorX - ((DOUBLE)ptClient.x * (DOUBLE)srcW / (DOUBLE)clientWidth));
          srcY = (LONG)(anchorY - ((DOUBLE)ptClient.y * (DOUBLE)srcH / (DOUBLE)clientHeight));
        }
        else
        {
          srcX = (LONG)(anchorX - ((DOUBLE)srcW / 2.0));
          srcY = (LONG)(anchorY - ((DOUBLE)srcH / 2.0));
        }

        lpsd->ptSourceOrigin.x = render_clipSourceOrigin(srcX, srcW, rcCapture.left, rcCapture.right);
        lpsd->ptSourceOrigin.y = render_clipSourceOrigin(srcY, srcH, rcCapture.top, rcCapture.bottom);
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

      renderRender(hWnd);
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
    UNREFERENCED_PARAMETER(cx);
    UNREFERENCED_PARAMETER(cy);

    if (SIZE_MINIMIZED == state)
    {
      LPMAGSTATE lpsd = (LPMAGSTATE)GetWindowLongPtr(hWnd, GWLP_USERDATA);

      if (lpsd)
      {
        lpsd->fPresentedContentValid = FALSE;
      }
      return;
    }

    renderResizeCapture(hWnd);
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
    DOUBLE anchorX = 0.0;
    DOUBLE anchorY = 0.0;

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
        anchorX = (DOUBLE)rcSourceOld.left + ((DOUBLE)RECTWIDTH(rcSourceOld) / 2.0);
        anchorY = (DOUBLE)rcSourceOld.top + ((DOUBLE)RECTHEIGHT(rcSourceOld) / 2.0);
        fPreservePinnedCenter = TRUE;
      }
    }

    DefWindowProc(hWnd, WM_WINDOWPOSCHANGED, 0, (LPARAM)lpwndpos);
    
    if (lpsd &&
        lpsd->graphicsBackend &&
        lpsd->hCaptureDC &&
        (fMoved || fSized))
    {
      render_minimapNotifyActivity(hWnd);

      if (fActualMove)
      {
        lpsd->fUseSourceOrigin = FALSE;
        lpsd->fSourceOriginPinned = FALSE;
      }
      else if (fPreservePinnedCenter &&
               lpsd->bi.biWidth > 0 &&
               lpsd->bi.biHeight > 0)
      {
        const LONG srcW = (lpsd->fTexScaler > 1.0001f) ? max(1, (LONG)(lpsd->bi.biWidth / lpsd->fTexScaler)) : lpsd->bi.biWidth;
        const LONG srcH = (lpsd->fTexScaler > 1.0001f) ? max(1, (LONG)(lpsd->bi.biHeight / lpsd->fTexScaler)) : lpsd->bi.biHeight;
        const LONG srcX = (LONG)(anchorX - ((DOUBLE)srcW / 2.0));
        const LONG srcY = (LONG)(anchorY - ((DOUBLE)srcH / 2.0));

        mag_GetCaptureRect(lpsd, &rcCapture);
        lpsd->ptSourceOrigin.x = render_clipSourceOrigin(srcX, srcW, rcCapture.left, rcCapture.right);
        lpsd->ptSourceOrigin.y = render_clipSourceOrigin(srcY, srcH, rcCapture.top, rcCapture.bottom);
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
      }
      else
      {
        renderRender(hWnd);
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

    wcex.style = CS_OWNDC | CS_SAVEBITS | CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW;
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
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.hbrBackground = GetStockBrush(BLACK_BRUSH);
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

    hWnd = CreateWindowEx(
      WS_EX_TRANSPARENT | 
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

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    //SetWindowLongPtr(hWnd, GWL_EXSTYLE, (WS_EX_COMPOSITED|WS_EX_APPWINDOW) | GetWindowExStyle(hWnd));
    return hWnd;
}
