#include "mag.h"

#include <windowsx.h>
#include <gl/GL.h>

#define ABS(x)         ((x < 0) ? -x : x)
#define RECTWIDTH(rc)  (ABS(rc.right - rc.left))
#define RECTHEIGHT(rc) (ABS(rc.bottom - rc.top))
#define CLAMP(x,lo,hi) (max(min(x, hi),lo))

/* void Cls_OnEnterSizeMove(HWND hwnd) */
#define HANDLE_WM_ENTERSIZEMOVE(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)

/* void Cls_OnExitSizeMove(HWND hwnd) */
#define HANDLE_WM_EXITSIZEMOVE(hwnd, wParam, lParam, fn) \
        ((fn)((hwnd)), 0L)

#define FORWARD_MSG(hwnd, message, fn)    \
    default: return (fn)((hwnd), (message), (wParam), (lParam))

typedef struct SHAREDWGLDATA
{
  POINT            pt;
  RECT             rc;
  DISPLAYINFO      di;
  BITMAPINFOHEADER bi;
  HDC              hDC;
  HGLRC            hRC;
  HDC              hCaptureDC;
  HDC              hDesktopDC;
  HBITMAP          hBitmapBg;
  GLclampf         cfClearColor[4];
  GLsizei          glScreenWidth;
  GLsizei          glScreenHeight;
  GLuint           glScreenTexture;
  GLubyte          glScreenData[3840*2160*4];

} SHAREDWGLDATA, *LPSHAREDWGLDATA;

void mag_wglInit(HWND hWnd);
void mag_wglRender(HWND hWnd);
void mag_CaptureScreen(HWND hWnd);

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
void mag_OnDestroy(HWND hWnd);
void mag_OnActivate(HWND hWnd, UINT state, HWND hWndActDeact, BOOL fMinimized);
void mag_OnPaint(HWND hWnd);
UINT mag_OnEraseBkgnd(HWND hWnd, HDC hDC);
BOOL mag_OnNCCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct);
UINT mag_OnNCHittest(HWND hWnd, int x, int y);
UINT mag_OnNCCalcSize(HWND hWnd, BOOL fCalcValidRects, NCCALCSIZE_PARAMS* lpcsp);
void mag_OnKeyUp(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags);
void mag_OnEnterSizeMove(HWND hWnd);
void mag_OnExitSizeMove(HWND hWnd);
void mag_OnTimer(HWND hWnd, UINT_PTR idEvent);
LRESULT CALLBACK mag_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
ATOM mag_RegisterClassEx(HINSTANCE hInstance);

void mag_wglInit(HWND hWnd)
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
      WGL_SWAP_METHOD_ARB,WGL_SWAP_EXCHANGE_ARB,
      WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
      WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
      WGL_SAMPLES_ARB, 4,
      0
    };

    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    int pf;

    wglInit();

    lpsd->hDC = GetDC(hWnd);
    pf = wglFindPixelFormat(lpsd->hDC, iAttribs, NULL);

    SetPixelFormat(lpsd->hDC, pf, &pfd);

    lpsd->hRC = wglCreateContextAttribsARB(lpsd->hDC, NULL, NULL);
    wglMakeCurrent(lpsd->hDC, lpsd->hRC);

    glGenTextures(1, &lpsd->glScreenTexture);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);

    // Upload texture data
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, 256, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, lpsd->glScreenData);

    gdiGetDisplayInfo(&lpsd->di);
    lpsd->hDesktopDC = GetDC(GetDesktopWindow());
    lpsd->hCaptureDC = CreateCompatibleDC(lpsd->hDesktopDC);
    lpsd->hBitmapBg = CreateCompatibleBitmap(lpsd->hDesktopDC, RECTWIDTH(lpsd->di.rc), RECTHEIGHT(lpsd->di.rc));
    SelectObject(lpsd->hCaptureDC, lpsd->hBitmapBg);

    lpsd->bi.biSize = sizeof(lpsd->bi);
    lpsd->bi.biWidth = 256;
    lpsd->bi.biHeight = 256;
    lpsd->bi.biPlanes = 1;
    lpsd->bi.biBitCount = 32; // RGBA
    lpsd->bi.biCompression = BI_RGB;

    wglSwapIntervalEXT(0);
}

void mag_wglRender(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!GetClientRect(hWnd, &lpsd->rc))
    {
      return;
    }

    glClearColor(
      lpsd->cfClearColor[0],
      lpsd->cfClearColor[1],
      lpsd->cfClearColor[2],
      lpsd->cfClearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, RECTWIDTH(lpsd->rc), RECTHEIGHT(lpsd->rc));

    mag_CaptureScreen(hWnd);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lpsd->glScreenWidth, lpsd->glScreenHeight, GL_BGRA, GL_UNSIGNED_BYTE, lpsd->glScreenData);
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
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    SwapBuffers(lpsd->hDC);

    glFinish();
}

void mag_CaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hBitmapBg && GetCursorPos(&lpsd->pt))
    {
      BitBlt(
        lpsd->hCaptureDC,
        0,
        0,
        256,
        256,
        lpsd->hDesktopDC,
        CLAMP(lpsd->pt.x - (LONG)(0.5f * 256), lpsd->di.rc.left, lpsd->di.rc.right - 256),
        CLAMP(lpsd->pt.y - (LONG)(0.5f * 256), lpsd->di.rc.top, lpsd->di.rc.bottom - 256),
        SRCCOPY | CAPTUREBLT);

      GetDIBits(lpsd->hCaptureDC, lpsd->hBitmapBg, 0, 256, lpsd->glScreenData, (BITMAPINFO*)&lpsd->bi, DIB_RGB_COLORS);
    }

    // BitBlt(hDC, 0, 0, nScreenWidth, nScreenHeight, hCaptureDC, 0, 0, SRCCOPY);
    // here to save the captured image to disk

    for (int i = 0; i < 256; ++i)
      for (int j = 0; j < 256; ++j)
        lpsd->glScreenData[((j + (i * 256)) * 4) + 3] = 225;   // Alpha is at offset 3

    lpsd->glScreenWidth = 256;
    lpsd->glScreenHeight = 256;
}

LRESULT mag_OnCreate(HWND hWnd, LPCREATESTRUCT lpCreateStruct)
{
    UNREFERENCED_PARAMETER(lpCreateStruct);

    mag_wglInit(hWnd);

    SetTimer(hWnd, 1, USER_TIMER_MINIMUM, NULL);

    //SetWindowDisplayAffinity(hWnd, WDA_EXCLUDEFROMCAPTURE);
    
    DwmEnableWindowComposition(hWnd, TRUE);

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

      SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED);
    }
}

void mag_OnPaint(HWND hWnd)
{
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hWnd, &ps);
    mag_wglRender(hWnd);
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

void mag_OnKeyUp(HWND hWnd, UINT vk, BOOL fDown, int cRepeat, UINT flags)
{
    UNREFERENCED_PARAMETER(fDown);
    UNREFERENCED_PARAMETER(cRepeat);
    UNREFERENCED_PARAMETER(flags);

    switch (vk) {
    case VK_ESCAPE:
    {
      if (GetForegroundWindow() == hWnd)
      {
        PostMessage(hWnd, WM_DESTROY, 0, 0);
      }
      break;
    }
    default:
      break;
    }
}

void mag_OnTimer(HWND hWnd, UINT_PTR idEvent)
{
    UNREFERENCED_PARAMETER(idEvent);

    mag_wglRender(hWnd);

    InvalidateRect(hWnd, NULL, FALSE);
    UpdateWindow(hWnd);
}

void mag_OnEnterSizeMove(HWND hWnd)
{
    SetTimer(hWnd, 0, USER_TIMER_MINIMUM, NULL);
}

void mag_OnExitSizeMove(HWND hWnd)
{
    KillTimer(hWnd, 0);
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
    HANDLE_MSG(hWnd,  WM_KEYUP,         mag_OnKeyUp);
    HANDLE_MSG(hWnd,  WM_TIMER,         mag_OnTimer);
    HANDLE_MSG(hWnd,  WM_ENTERSIZEMOVE, mag_OnEnterSizeMove);
    HANDLE_MSG(hWnd,  WM_EXITSIZEMOVE,  mag_OnExitSizeMove);
    FORWARD_MSG(hWnd, message,          DefWindowProc);
    }
}

ATOM mag_RegisterClassEx(HINSTANCE hInstance)
{
    WNDCLASSEX wcex = { sizeof(wcex) };

    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
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
      0,
      (LPTSTR)(atm = mag_RegisterClassEx(hInstance)),
      TEXT("magWindow"),
      WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT,
      0,
      CW_USEDEFAULT,
      0,
      NULL,
      NULL,
      hInstance,
      HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(SHAREDWGLDATA)));

    if (!hWnd)
    {
      return FALSE;
    }

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    return TRUE;
}