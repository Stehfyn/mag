#include "mag.h"
#include "render.h"

#pragma comment(lib, "htmlhelp")
#pragma comment(lib, "Shlwapi")

#define FORWARD_MSG(hwnd, message, fn)    \
    default: return (fn)((hwnd), (message), (wParam), (lParam))

void mag_ShowPopupMenu(HWND hWnd, int x, int y);
void mag_ShowHelpMenu(HWND hWnd, int x, int y);

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
LRESULT CALLBACK mag_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ATOM mag_RegisterClassEx(HINSTANCE hInstance);

void mag_ShowPopupMenu(HWND hWnd, int x, int y)
{
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    HMENU hPopupMenu = CreatePopupMenu();

    SetWindowDisplayAffinity(hPopupMenu, WDA_EXCLUDEFROMCAPTURE);

    AppendMenu(hPopupMenu, ((lpsd->fTrackCursor) ? MF_CHECKED : 0) | MF_BYPOSITION | MF_STRING, 1001, _T("Follow Mouse"));
    AppendMenu(hPopupMenu, MF_BYPOSITION | MF_STRING, 1002, _T("Help"));
    AppendMenu(hPopupMenu, MF_BYPOSITION | MF_STRING, 1003, _T("Exit"));

    TrackPopupMenuEx(hPopupMenu, TPM_LEFTALIGN | TPM_RIGHTBUTTON | TPM_WORKAREA, x, y, hWnd, NULL);

    DestroyMenu(hPopupMenu);
}

void mag_ShowHelpMenu(HWND hWnd, int x, int y)
{
    TCHAR szFileName[MAX_PATH + 1];

    UNREFERENCED_PARAMETER(x);
    UNREFERENCED_PARAMETER(y);

    if (GetWindowModuleFileName(hWnd, szFileName, MAX_PATH + 1) && PathRemoveFileSpec(szFileName))
    {
      PathAppend(szFileName, TEXT("mag.chm"));

      SetWindowOwner(HtmlHelp(hWnd, szFileName, HH_HELP_FINDER, NULL), GetDesktopWindow());
    }
}

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    UNREFERENCED_PARAMETER(lpCreateStruct);

    SetTimer(hWnd, 1, USER_TIMER_MINIMUM, NULL);
    SetWindowDisplayAffinity(hWnd, WDA_EXCLUDEFROMCAPTURE);
    DwmEnableWindowComposition(hWnd, TRUE);

    SetCurrentProcessEfficiencyQoS();

    return TRUE;
}

void mag_OnDestroy(HWND hWnd)
{
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
    HDC hdc = BeginPaint(hWnd, &ps);
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
    case 1001:
    {
      LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);
      lpsd->fTrackCursor = !lpsd->fTrackCursor;

      break;
    }
    case 1002:
    {
      mag_ShowHelpMenu(hWnd, 0, 0);
      break;
    }
    case 1003:
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
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

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
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    UNREFERENCED_PARAMETER(idEvent);

    if (!InMenu())
    {
      if (lpsd->fTrackCursor)
      {
        POINT pt;
      
        if (GetCursorPos(&pt))
        {
          pt.x -= 0.5f * RECTWIDTH(lpsd->rc);
          pt.y -= 0.5f * RECTHEIGHT(lpsd->rc);

          SetWindowPos(hWnd, 0, pt.x, pt.y, 0, 0, SWP_NOSIZE | SWP_ASYNCWINDOWPOS | SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        }
      }

      SetWindowAlwaysOnTop(hWnd, TRUE);

      renderRender(hWnd);
    }
}

void mag_OnMouseWheel(HWND hWnd, int xPos, int yPos, int zDelta, UINT fwKeys)
{
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

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

LRESULT CALLBACK mag_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message) {
    HANDLE_MSG(hWnd,  WM_CREATE,        mag_OnCreate);
    HANDLE_MSG(hWnd,  WM_DESTROY,       mag_OnDestroy);
    HANDLE_MSG(hWnd,  WM_ACTIVATE,      mag_OnActivate);
    HANDLE_MSG(hWnd,  WM_PAINT,         mag_OnPaint);
    HANDLE_MSG(hWnd,  WM_ERASEBKGND,    mag_OnEraseBkgnd);
    HANDLE_MSG(hWnd,  WM_NCCREATE,      mag_OnNCCreate);
    HANDLE_MSG(hWnd,  WM_NCCALCSIZE,    mag_OnNCCalcSize);
    HANDLE_MSG(hWnd,  WM_NCHITTEST,     mag_OnNCHittest);
    HANDLE_MSG(hWnd,  WM_NCACTIVATE,    mag_OnNCActivate);
    HANDLE_MSG(hWnd,  WM_NCRBUTTONDOWN, mag_OnNCRButtonDown);
    HANDLE_MSG(hWnd,  WM_COMMAND,       mag_OnCommand);
    HANDLE_MSG(hWnd,  WM_KEYUP,         mag_OnKeyUp);
    HANDLE_MSG(hWnd,  WM_TIMER,         mag_OnTimer);
    HANDLE_MSG(hWnd,  WM_MOUSEWHEEL,    mag_OnMouseWheel);
    FORWARD_MSG(hWnd, message,          DefWindowProc);
    }
}

ATOM mag_RegisterClassEx(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = { sizeof(wcex) };

    wcex.style = CS_OWNDC | CS_BYTEALIGNCLIENT | CS_BYTEALIGNWINDOW;
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
      WS_EX_TRANSPARENT | WS_EX_DLGMODALFRAME,
      (LPTSTR)(atm = mag_RegisterClassEx(hInstance)),
      TEXT("magWindow"),
      WS_OVERLAPPEDWINDOW,
      //CW_USEDEFAULT,
      0,
      0,
      //CW_USEDEFAULT,
      100,
      100,
      NULL,
      NULL,
      hInstance,
      HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SHAREDWGLDATA)));

    if (!hWnd)
    {
      return FALSE;
    }

    SetParent(hWnd, GetDesktopWindow());

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    renderInit(hWnd);
    renderCreateResources(hWnd);

    return TRUE;
}