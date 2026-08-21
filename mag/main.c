#include "mag.h"
#include "render.h"

#define MAIN_RENDER_INTERVAL_MS USER_TIMER_MINIMUM
#define MAIN_TEST_DESKTOP_PREFIX TEXT("mag-test-")
#define MAIN_TEST_DESKTOP_CLASS L"MagSmokeDesktopWindow"
#define MAIN_TEST_PEER_CLASS L"MagSmokePeerWindow"
#define MAIN_TEST_TASKBAR_CLASS L"MagSmokeTaskbarWindow"

typedef struct MAINTESTWINDOWS
{
  HINSTANCE instance;
  ATOM      desktopClass;
  ATOM      peerClass;
  ATOM      taskbarClass;
  HWND      desktop;
  HWND      peer;
  HWND      taskbar;
} MAINTESTWINDOWS, *LPMAINTESTWINDOWS;

typedef struct MAINVBLANKTHREAD
{
  HWND   hWnd;
  HANDLE hStopEvent;
  HANDLE hThread;
} MAINVBLANKTHREAD, *LPMAINVBLANKTHREAD;

DWORD WINAPI main_VBlankThreadProc(LPVOID lpParameter);
BOOL main_StartVBlankThread(HWND hWnd, LPMAINVBLANKTHREAD lpThread);
void main_StopVBlankThread(LPMAINVBLANKTHREAD lpThread);
BOOL main_BeginIsolatedTestDesktop(HDESK* originalDesktop, HDESK* testDesktop);
void main_EndIsolatedTestDesktop(HDESK originalDesktop, HDESK testDesktop);
LRESULT CALLBACK main_TestWindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
BOOL main_CreateIsolatedTestWindows(HINSTANCE hInstance, LPMAINTESTWINDOWS windows);
void main_DestroyIsolatedTestWindows(LPMAINTESTWINDOWS windows);
BOOL main_InitializeAndRevealWindow(HWND hWnd, int nCmdShow, BOOL synchronize);

LRESULT CALLBACK main_TestWindowProc(
  HWND hWnd,
  UINT message,
  WPARAM wParam,
  LPARAM lParam)
{
    if (WM_NCCREATE == message)
    {
      const CREATESTRUCTW* create = (const CREATESTRUCTW*)lParam;
      SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)create->lpCreateParams);
      return TRUE;
    }
    if (WM_ERASEBKGND == message)
    {
      return 1;
    }
    if (WM_PAINT == message)
    {
      PAINTSTRUCT paint;
      HDC dc = BeginPaint(hWnd, &paint);
      HBRUSH brush = CreateSolidBrush(
        (COLORREF)(ULONG_PTR)GetWindowLongPtrW(hWnd, GWLP_USERDATA));

      if (dc && brush)
      {
        FillRect(dc, &paint.rcPaint, brush);
      }
      if (brush)
      {
        DeleteObject(brush);
      }
      EndPaint(hWnd, &paint);
      return 0;
    }
    return DefWindowProcW(hWnd, message, wParam, lParam);
}

static ATOM main_RegisterTestWindowClass(
  HINSTANCE hInstance,
  LPCWSTR className)
{
    WNDCLASSEXW windowClass = { sizeof(windowClass) };

    windowClass.lpfnWndProc = main_TestWindowProc;
    windowClass.hInstance = hInstance;
    windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = className;
    return RegisterClassExW(&windowClass);
}

void main_DestroyIsolatedTestWindows(LPMAINTESTWINDOWS windows)
{
    if (!windows)
    {
      return;
    }
    if (windows->taskbar)
    {
      DestroyWindow(windows->taskbar);
    }
    if (windows->peer)
    {
      DestroyWindow(windows->peer);
    }
    if (windows->desktop)
    {
      DestroyWindow(windows->desktop);
    }
    if (windows->taskbarClass)
    {
      UnregisterClassW(MAKEINTATOM(windows->taskbarClass), windows->instance);
    }
    if (windows->peerClass)
    {
      UnregisterClassW(MAKEINTATOM(windows->peerClass), windows->instance);
    }
    if (windows->desktopClass)
    {
      UnregisterClassW(MAKEINTATOM(windows->desktopClass), windows->instance);
    }
    ZeroMemory(windows, sizeof(*windows));
}

BOOL main_CreateIsolatedTestWindows(
  HINSTANCE hInstance,
  LPMAINTESTWINDOWS windows)
{
    const int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    const int top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    const int width = max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN));
    const int height = max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN));
    const int taskbarHeight = min(48, height);

    if (!windows)
    {
      return FALSE;
    }
    ZeroMemory(windows, sizeof(*windows));
    windows->instance = hInstance;
    /* Explorer's special shell surfaces cannot be manufactured on a private
       WinStation desktop: the private DWM thumbnail ordinal rejects synthetic
       Progman/Shell_TrayWnd sources.  These semantic fixtures still prove the
       multi-window visual includes independent top-level DWM surfaces without
       ever touching the user's input desktop. */
    windows->desktopClass = main_RegisterTestWindowClass(
      hInstance, MAIN_TEST_DESKTOP_CLASS);
    windows->peerClass = main_RegisterTestWindowClass(
      hInstance, MAIN_TEST_PEER_CLASS);
    windows->taskbarClass = main_RegisterTestWindowClass(
      hInstance, MAIN_TEST_TASKBAR_CLASS);
    if (!windows->desktopClass || !windows->peerClass ||
        !windows->taskbarClass)
    {
      main_DestroyIsolatedTestWindows(windows);
      return FALSE;
    }

    windows->desktop = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      MAKEINTATOM(windows->desktopClass),
      L"MAG smoke desktop",
      WS_POPUP,
      left,
      top,
      width,
      height,
      NULL,
      NULL,
      hInstance,
      (LPVOID)(ULONG_PTR)RGB(22, 48, 80));
    windows->peer = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      MAKEINTATOM(windows->peerClass),
      L"MAG smoke peer",
      WS_POPUP,
      left + min(160, width / 8),
      top + min(120, height / 8),
      min(480, width),
      min(320, height),
      NULL,
      NULL,
      hInstance,
      (LPVOID)(ULONG_PTR)RGB(190, 45, 35));
    windows->taskbar = CreateWindowExW(
      WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
      MAKEINTATOM(windows->taskbarClass),
      L"MAG smoke taskbar",
      WS_POPUP,
      left,
      top + height - taskbarHeight,
      width,
      taskbarHeight,
      NULL,
      NULL,
      hInstance,
      (LPVOID)(ULONG_PTR)RGB(32, 120, 64));
    if (!windows->desktop || !windows->peer || !windows->taskbar)
    {
      main_DestroyIsolatedTestWindows(windows);
      return FALSE;
    }

    ShowWindow(windows->desktop, SW_SHOWNOACTIVATE);
    ShowWindow(windows->peer, SW_SHOWNOACTIVATE);
    ShowWindow(windows->taskbar, SW_SHOWNOACTIVATE);
    SetWindowPos(
      windows->desktop,
      HWND_BOTTOM,
      0,
      0,
      0,
      0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(
      windows->peer,
      HWND_TOP,
      0,
      0,
      0,
      0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    SetWindowPos(
      windows->taskbar,
      HWND_TOP,
      0,
      0,
      0,
      0,
      SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    UpdateWindow(windows->desktop);
    UpdateWindow(windows->peer);
    UpdateWindow(windows->taskbar);
    return TRUE;
}

BOOL main_InitializeAndRevealWindow(HWND hWnd, int nCmdShow, BOOL synchronize)
{
    if (!hWnd)
    {
      return FALSE;
    }

    /* No messages are pumped between ShowWindow and the first submission.
       DWM therefore receives initialized content before the next composition
       pass instead of ever painting the class background. */
    ShowWindow(hWnd, nCmdShow);
    return renderSubmit(hWnd) &&
      (!synchronize || SUCCEEDED(DwmFlush()));
}

BOOL main_BeginIsolatedTestDesktop(HDESK* originalDesktop, HDESK* testDesktop)
{
    TCHAR desktopName[64];
    HDESK createdDesktop;

    if (!originalDesktop || !testDesktop)
    {
      return FALSE;
    }
    *originalDesktop = GetThreadDesktop(GetCurrentThreadId());
    *testDesktop = NULL;
    if (!*originalDesktop)
    {
      return FALSE;
    }

    _sntprintf_s(
      desktopName,
      ARRAYSIZE(desktopName),
      _TRUNCATE,
      TEXT("%s%lu-%I64u"),
      MAIN_TEST_DESKTOP_PREFIX,
      GetCurrentProcessId(),
      GetTickCount64());
    createdDesktop = CreateDesktop(
      desktopName,
      NULL,
      NULL,
      0,
      DESKTOP_CREATEWINDOW | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS,
      NULL);
    if (!createdDesktop)
    {
      return FALSE;
    }
    if (!SetThreadDesktop(createdDesktop))
    {
      CloseDesktop(createdDesktop);
      return FALSE;
    }
    *testDesktop = createdDesktop;
    return TRUE;
}

void main_EndIsolatedTestDesktop(HDESK originalDesktop, HDESK testDesktop)
{
    if (originalDesktop)
    {
      SetThreadDesktop(originalDesktop);
    }
    if (testDesktop)
    {
      CloseDesktop(testDesktop);
    }
}

BOOL main_PumpMessages(HWND hWnd, int* lpExitCode)
{
    MSG msg;

    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
    {
      if (WM_QUIT == msg.message)
      {
        if (lpExitCode)
        {
          *lpExitCode = (int)msg.wParam;
        }

        return FALSE;
      }

      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    return IsWindow(hWnd);
}

DWORD WINAPI main_VBlankThreadProc(LPVOID lpParameter)
{
    LPMAINVBLANKTHREAD lpThread = (LPMAINVBLANKTHREAD)lpParameter;

    while (WAIT_OBJECT_0 != WaitForSingleObject(lpThread->hStopEvent, 0))
    {
      HANDLE frameWaitHandle;

      if (!IsWindow(lpThread->hWnd))
      {
        break;
      }

      frameWaitHandle = renderDuplicateFrameWaitHandle(lpThread->hWnd);
      if (frameWaitHandle)
      {
        HANDLE handles[] = { lpThread->hStopEvent, frameWaitHandle };
        const DWORD waitResult = WaitForMultipleObjects(ARRAYSIZE(handles), handles, FALSE, 1000);

        CloseHandle(frameWaitHandle);

        if (WAIT_OBJECT_0 == waitResult)
        {
          break;
        }
        if (WAIT_OBJECT_0 + 1 != waitResult)
        {
          continue;
        }
      }
      else if (!D3DKMTWaitForVerticalBlank(lpThread->hWnd))
      {
        if (WAIT_OBJECT_0 == WaitForSingleObject(lpThread->hStopEvent, MAIN_RENDER_INTERVAL_MS))
        {
          break;
        }
      }

      if (WAIT_OBJECT_0 == WaitForSingleObject(lpThread->hStopEvent, 0) || !IsWindow(lpThread->hWnd))
      {
        break;
      }

      SendMessage(lpThread->hWnd, WM_MAG_RENDER, 0, 0);
    }

    return 0;
}

BOOL main_StartVBlankThread(HWND hWnd, LPMAINVBLANKTHREAD lpThread)
{
    ZeroMemory(lpThread, sizeof(*lpThread));
    lpThread->hWnd = hWnd;
    lpThread->hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!lpThread->hStopEvent)
    {
      main_StopVBlankThread(lpThread);
      return FALSE;
    }

    lpThread->hThread = CreateThread(NULL, 0, main_VBlankThreadProc, lpThread, 0, NULL);
    if (!lpThread->hThread)
    {
      main_StopVBlankThread(lpThread);
      return FALSE;
    }

    return TRUE;
}

void main_StopVBlankThread(LPMAINVBLANKTHREAD lpThread)
{
    if (lpThread->hStopEvent)
    {
      SetEvent(lpThread->hStopEvent);
    }

    if (lpThread->hThread)
    {
      for (;;)
      {
        DWORD dwWait = MsgWaitForMultipleObjects(1, &lpThread->hThread, FALSE, INFINITE, QS_ALLINPUT);

        if (WAIT_OBJECT_0 == dwWait)
        {
          break;
        }

        if (WAIT_OBJECT_0 + 1 == dwWait)
        {
          MSG msg;

          while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
          {
            if (WM_QUIT == msg.message)
            {
              PostQuitMessage((int)msg.wParam);
            }
            else
            {
              TranslateMessage(&msg);
              DispatchMessage(&msg);
            }
          }
        }
        else
        {
          break;
        }
      }

      CloseHandle(lpThread->hThread);
    }

    if (lpThread->hStopEvent)
    {
      CloseHandle(lpThread->hStopEvent);
    }

    ZeroMemory(lpThread, sizeof(*lpThread));
}

int 
WINAPI 
_tWinMain(
  _In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPTSTR lpCmdLine,
  _In_ int nShowCmd)
{
    HWND hWnd;
    int exitCode = 0;
    MAINVBLANKTHREAD vblankThread;
    BOOL fMessageDrivenRender = FALSE;
    const BOOL fGraphicsSmoke = NULL != _tcsstr(lpCmdLine, TEXT("--graphics-smoke"));
    const BOOL fDwmPrivateSmoke =
      NULL != _tcsstr(lpCmdLine, TEXT("--dwm-private-smoke"));
    const BOOL fIsolatedSmoke = fGraphicsSmoke || fDwmPrivateSmoke;
    HDESK originalDesktop = NULL;
    HDESK testDesktop = NULL;
    MAINTESTWINDOWS testWindows = { 0 };

    UNREFERENCED_PARAMETER(hPrevInstance);

    if (fIsolatedSmoke && !main_BeginIsolatedTestDesktop(&originalDesktop, &testDesktop))
    {
      return ERROR_ACCESS_DENIED;
    }
    if (fIsolatedSmoke &&
        !main_CreateIsolatedTestWindows(hInstance, &testWindows))
    {
      main_EndIsolatedTestDesktop(originalDesktop, testDesktop);
      return ERROR_OPEN_FAILED;
    }

    hWnd = magInitInstance(hInstance, fIsolatedSmoke ? SW_HIDE : nShowCmd);
    if (!hWnd)
    {
      if (fIsolatedSmoke)
      {
        main_DestroyIsolatedTestWindows(&testWindows);
        main_EndIsolatedTestDesktop(originalDesktop, testDesktop);
      }
      return FALSE;
    }

    if (fIsolatedSmoke)
    {
      /* Mirror normal startup exactly, but reveal the initialized window only
         on the private, non-input desktop. */
      if (!main_InitializeAndRevealWindow(
            hWnd,
            SW_SHOWNOACTIVATE,
            FALSE))
      {
        DestroyWindow(hWnd);
        main_DestroyIsolatedTestWindows(&testWindows);
        main_EndIsolatedTestDesktop(originalDesktop, testDesktop);
        return ERROR_OPEN_FAILED;
      }
      exitCode = fDwmPrivateSmoke
        ? renderRunDwmPrivateSmoke(
            hWnd,
            testWindows.desktop,
            testWindows.taskbar,
            testWindows.peer)
        : renderRunGraphicsSmoke(
            hWnd,
            testWindows.desktop,
            testWindows.taskbar,
            testWindows.peer);
      DestroyWindow(hWnd);
      main_DestroyIsolatedTestWindows(&testWindows);
      main_EndIsolatedTestDesktop(originalDesktop, testDesktop);
      return exitCode;
    }

    /* Give DWM a real target while keeping it invisible.  The first complete
       frame is committed before uncloaking, so no default white frame or busy
       cursor is ever exposed. */
    if (!main_InitializeAndRevealWindow(hWnd, nShowCmd, TRUE))
    {
      DestroyWindow(hWnd);
      return ERROR_OPEN_FAILED;
    }

    if (main_StartVBlankThread(hWnd, &vblankThread))
    {
      fMessageDrivenRender = TRUE;
      renderSetMessageDriven(hWnd, TRUE);
    }

    while (main_PumpMessages(hWnd, &exitCode))
    {
      DWORD dwWait;

      dwWait = MsgWaitForMultipleObjects(0, NULL, FALSE, fMessageDrivenRender ? INFINITE : MAIN_RENDER_INTERVAL_MS, QS_ALLINPUT);

      if (WAIT_FAILED == dwWait)
      {
        break;
      }

      if (!fMessageDrivenRender && WAIT_TIMEOUT == dwWait)
      {
        if (IsWindow(hWnd))
        {
          mag_OnTimer(hWnd, 0);
        }
        else
        {
          break;
        }
      }
    }

    main_StopVBlankThread(&vblankThread);
    return exitCode;
}
