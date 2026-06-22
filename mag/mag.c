#include "mag.h"
#include "render.h"
#include "help.h"

#pragma comment(lib, "ntdll")

#define FORWARD_MSG(hwnd, message, fn)    \
    default: return (fn)((hwnd), (message), (wParam), (lParam))

/* void Cls_OnEnterSizeMove(HWND hwnd) */
#define HANDLE_WM_ENTERSIZEMOVE(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)

/* void Cls_OnExitSizeMove(HWND hwnd */
#define HANDLE_WM_EXITSIZEMOVE(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)

/* void Cls_OnExitSizeMove(HWND hwnd */
#define HANDLE_WM_ENTERMENULOOP(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (BOOL)(wParam)), 0L)

/* void Cls_OnEnterSizeMove(HWND hwnd) */
#define HANDLE_WM_EXITMENULOOP(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd), (BOOL)(wParam)), 0L)

typedef struct SETTINGSOPTION
{
  UINT    id;
  LPCTSTR pszName;
  BOOL    fImplemented;
} SETTINGSOPTION;

void mag_ShowPopupMenu(HWND hWnd, int x, int y);
void mag_ShowHelpMenu(HWND hWnd, int x, int y);
void mag_ShowSettingsDialog(HWND hWnd);
void mag_AddSettingsOptions(HWND hDlg, int idCtl, const SETTINGSOPTION* options, UINT count, UINT selectedId);
BOOL mag_GetSelectedSettingsOption(HWND hDlg, int idCtl, const SETTINGSOPTION* options, UINT count, UINT* selectedId, BOOL* fImplemented);
void mag_UpdateSettingsDialogState(HWND hDlg);
INT_PTR CALLBACK mag_SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
void mag_OnDestroy(HWND hWnd);
void mag_OnActivate(HWND hWnd, UINT state, HWND hWndActDeact, BOOL fMinimized);
void mag_OnPaint(HWND hWnd);
UINT mag_OnEraseBkgnd(HWND hWnd, HDC hDC);
BOOL mag_OnNCCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
UINT mag_OnNCHittest(HWND hWnd, int x, int y);
UINT mag_OnNCCalcSize(HWND hWnd, BOOL fCalcValidRects, NCCALCSIZE_PARAMS* lpcsp);
BOOL mag_OnNCActivate(HWND hWnd, BOOL fActive, HWND hwndActDeact, BOOL fMinimized);
void mag_OnNCRButtonDown(HWND hWnd, BOOL fDoubleClick, int x, int y, UINT codeHitTest);
void mag_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify);
void mag_OnKeyUp(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
void mag_OnTimer(HWND hWnd, UINT_PTR idEvent);
void mag_OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys);
void mag_OnSize(HWND hWnd, UINT state, int cx, int cy);
void mag_OnEnterMenuLoop(HWND hWnd, BOOL fIsTrackPopupMenu);
void mag_OnExitMenuLoop(HWND hWnd, BOOL fIsTrackPopupMenu);
void mag_OnEnterSizeMove(HWND hWnd);
void mag_OnExitSizeMove(HWND hWnd);
void mag_OnWindowPosChanged(HWND hWnd, const WINDOWPOS* lpwndpos);

LRESULT CALLBACK mag_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ATOM mag_RegisterClassEx(HINSTANCE hInstance);

static const SETTINGSOPTION g_graphicsApiOptions[] =
{
  { GRAPHICS_API_OPENGL, _T("OpenGL"), TRUE },
};

static const SETTINGSOPTION g_captureApiOptions[] =
{
  { CAPTURE_API_GDI_BITBLT, _T("GDI BitBlt"), TRUE },
  { CAPTURE_API_DXGI_DESKTOP_DUPLICATION, _T("DXGI Desktop Duplication"), TRUE },
  { CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE, _T("Windows Graphics Capture"), TRUE },
  { CAPTURE_API_MAGNIFICATION, _T("Magnification API (planned)"), FALSE },
};

void mag_ShowPopupMenu(HWND hWnd, int x, int y)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    HMENU hMenu = CreatePopupMenu();
    //HMENU hMenu = LoadPopupMenu(GetModuleHandle(NULL), MAKEINTRESOURCE(IDR_MENU1));


    AppendMenu(hMenu, ((lpsd->fTrackCursor) ? MF_CHECKED : 0) | MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_FOLLOW_MOUSE, _T("Follow Mouse"));
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_SETTINGS, _T("Settings..."));
    AppendMenu(hMenu, MF_BYPOSITION | MF_SEPARATOR, 0, NULL);
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_HELP, _T("Help"));
    AppendMenu(hMenu, MF_BYPOSITION | MF_STRING, ID_CONTEXTMENU_CLOSE, _T("Exit"));

    TrackPopupMenuEx(hMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_WORKAREA, x, y, hWnd, NULL);

    DestroyMenu(hMenu);
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
    UINT selectedId;
    BOOL fGraphicsImplemented = FALSE;
    BOOL fCaptureImplemented = FALSE;
    BOOL fValid;

    fValid =
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_GRAPHICS_API, g_graphicsApiOptions, ARRAYSIZE(g_graphicsApiOptions), &selectedId, &fGraphicsImplemented) &&
      mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), &selectedId, &fCaptureImplemented);

    EnableWindow(GetDlgItem(hDlg, IDOK), fValid && fGraphicsImplemented && fCaptureImplemented);

    if (!fValid)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, _T("Select a graphics API and capture API."));
    }
    else if (!fGraphicsImplemented || !fCaptureImplemented)
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, _T("Planned APIs are listed but cannot be applied yet."));
    }
    else
    {
      SetDlgItemText(hDlg, IDC_SETTINGS_STATUS, _T(""));
    }
}

INT_PTR CALLBACK mag_SettingsDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    LPSHAREDWGLDATA lpsd;

    switch (message)
    {
    case WM_INITDIALOG:
    {
      HWND hOwner = (HWND)lParam;
      UINT graphicsApi;
      UINT captureApi;

      SetWindowLongPtr(hDlg, DWLP_USER, (LONG_PTR)hOwner);
      lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hOwner, GWLP_USERDATA);
      graphicsApi = (lpsd && lpsd->graphicsApi < GRAPHICS_API_COUNT) ? lpsd->graphicsApi : GRAPHICS_API_OPENGL;
      captureApi = (lpsd && lpsd->captureApi < CAPTURE_API_COUNT) ? lpsd->captureApi : CAPTURE_API_GDI_BITBLT;

      mag_AddSettingsOptions(hDlg, IDC_SETTINGS_GRAPHICS_API, g_graphicsApiOptions, ARRAYSIZE(g_graphicsApiOptions), graphicsApi);
      mag_AddSettingsOptions(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), captureApi);
      mag_UpdateSettingsDialogState(hDlg);

      return TRUE;
    }
    case WM_COMMAND:
    {
      switch (LOWORD(wParam))
      {
      case IDOK:
      {
        HWND hOwner = (HWND)GetWindowLongPtr(hDlg, DWLP_USER);
        UINT graphicsApi = GRAPHICS_API_OPENGL;
        UINT captureApi = CAPTURE_API_GDI_BITBLT;
        BOOL fGraphicsImplemented = FALSE;
        BOOL fCaptureImplemented = FALSE;

        if (!mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_GRAPHICS_API, g_graphicsApiOptions, ARRAYSIZE(g_graphicsApiOptions), &graphicsApi, &fGraphicsImplemented) ||
          !mag_GetSelectedSettingsOption(hDlg, IDC_SETTINGS_CAPTURE_API, g_captureApiOptions, ARRAYSIZE(g_captureApiOptions), &captureApi, &fCaptureImplemented) ||
          !fGraphicsImplemented ||
          !fCaptureImplemented)
        {
          MessageBox(hDlg, _T("That backend is listed for planning but is not implemented yet."), _T("Settings"), MB_OK | MB_ICONINFORMATION);
          mag_UpdateSettingsDialogState(hDlg);
          return TRUE;
        }

        lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hOwner, GWLP_USERDATA);
        if (lpsd)
        {
          if (graphicsApi < GRAPHICS_API_COUNT)
          {
            lpsd->graphicsApi = (GRAPHICSAPI)graphicsApi;
          }

          if (captureApi < CAPTURE_API_COUNT)
          {
            lpsd->captureApi = (CAPTUREAPI)captureApi;
          }
        }

        EndDialog(hDlg, IDOK);
        return TRUE;
      }
      case IDC_SETTINGS_GRAPHICS_API:
      case IDC_SETTINGS_CAPTURE_API:
      {
        if (CBN_SELCHANGE == HIWORD(wParam))
        {
          mag_UpdateSettingsDialogState(hDlg);
          return TRUE;
        }
        break;
      }
      case IDCANCEL:
      {
        EndDialog(hDlg, IDCANCEL);
        return TRUE;
      }
      default:
        break;
      }
      break;
    }
    default:
      break;
    }

    return FALSE;
}

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(lpCreateStruct);

    SetCurrentProcessEfficiencyQoS();

    lpsd->graphicsApi = GRAPHICS_API_OPENGL;
    lpsd->captureApi = CAPTURE_API_GDI_BITBLT;

    renderInit(hWnd);

    //SetTimer(hWnd, 1, 13, NULL);
    SetWindowDisplayAffinity(hWnd, WDA_EXCLUDEFROMCAPTURE);
    DwmEnableWindowComposition(hWnd, TRUE);

    return TRUE;
}

void mag_OnDestroy(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

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
      if (IsMaximized(hWnd))
      {
        MONITORINFO mi = { sizeof(MONITORINFO) };

        if(GetMonitorInfo(
            MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST), (LPMONITORINFO)&mi))
        {
          lpcsp->rgrc[0] = mi.rcWork;

          return 0;
        }
      }

      mag_OnTimer(hWnd, 0);
      
      lpcsp->rgrc[0].bottom += 1;

      return WVR_VALIDRECTS;
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
    case client:         return HTCAPTION;
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

void mag_OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch(id){
    case ID_CONTEXTMENU_FOLLOW_MOUSE:
    {
      LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
      lpsd->fTrackCursor = !lpsd->fTrackCursor;

      if (lpsd->fTrackCursor)
      {
        DWORD dwExStyle = GetWindowExStyle(hWnd);
        dwExStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        dwExStyle |= WS_EX_LAYERED;
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);
      }
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
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(fDown);
    UNREFERENCED_PARAMETER(cRepeat);
    UNREFERENCED_PARAMETER(flags);

    switch (vk) {
    if (lpsd->fTrackCursor)
    { 
      case VK_SPACE:
      {   
        lpsd->fTrackCursor = FALSE;
        break;
      }
    }
    if (GetForegroundWindow() == hWnd)
    {
      case VK_ESCAPE:
      {
        PostMessage(hWnd, WM_DESTROY, 0, 0);
        break;
      }
    }
    default:
      break;
    }
}

void mag_OnTimer(HWND hWnd, UINT_PTR idEvent)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(idEvent);

    if (!InMenu())
    {
      if (lpsd->fTrackCursor)
      {
        DWORD dwExStyle = GetWindowExStyle(hWnd);

        dwExStyle &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
        dwExStyle |= WS_EX_LAYERED;// | WS_EX_COMPOSITED;
        SetWindowLongPtr(hWnd, GWL_EXSTYLE, dwExStyle);
      }

      SetWindowAlwaysOnTop(hWnd, TRUE);

      renderRender(hWnd);
    }
}

void mag_OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    POINT pt = { xPos, yPos };

    UNREFERENCED_PARAMETER(fwKeys);

    if (ScreenToClient(hWnd, &pt) && PtInRect(&lpsd->rc, pt))
    {
      const FLOAT fScaleScale = powf(-logf(0.001f * (1.0f - lpsd->fScale) + .575f), 6);

      lpsd->fScale += ((0.0f < fScaleScale) ? fScaleScale : -1.0f + fScaleScale) * ((!zDelta) ? 0.0f : (0 < zDelta) ? -1.0f : 1.0f);
      lpsd->fScale = CLAMP(lpsd->fScale, 0.001f, 1.0f);
      lpsd->fTexScaler = 1.0f + (15.0f * (1.0f - lpsd->fScale));
    }
}

void mag_OnSize(HWND hWnd, UINT state, int cx, int cy)
{
    UNREFERENCED_PARAMETER(state);
    UNREFERENCED_PARAMETER(cx);
    UNREFERENCED_PARAMETER(cy);

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
    SetTimer(hWnd, 0x69, USER_TIMER_MINIMUM, 0);
}

void mag_OnExitSizeMove(HWND hWnd)
{
    KillTimer(hWnd, 0x69);
}

void mag_OnWindowPosChanged(HWND hWnd, const WINDOWPOS* lpwndpos)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    DefWindowProc(hWnd, WM_WINDOWPOSCHANGED, 0, (LPARAM)lpwndpos);
    
    if (lpsd &&
        lpsd->hDC &&
        lpsd->hCaptureDC &&
        (!(lpwndpos->flags & SWP_NOMOVE) || !(lpwndpos->flags & SWP_NOSIZE)))
    {
      renderRender(hWnd);
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
    HANDLE_MSG(hWnd,  WM_MOUSEWHEEL,        mag_OnMouseWheel);
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
    //wcex.hIcon          = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_WINDOWSPROJECT1));
    wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.hbrBackground = GetStockBrush(BLACK_BRUSH);
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_WINDOWSPROJECT1);
    wcex.lpszClassName = TEXT("magWindowClass");
    //wcex.lpszClassName  = szWindowClass;
    //wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassEx(&wcex);
}

BOOL magInitInstance(HINSTANCE hInstance, int nCmdShow)
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
      VirtualAlloc(NULL, sizeof(SHAREDWGLDATA), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!hWnd)
    {
      return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);
    //SetWindowLongPtr(hWnd, GWL_EXSTYLE, (WS_EX_COMPOSITED|WS_EX_APPWINDOW) | GetWindowExStyle(hWnd));
    return TRUE;
}
