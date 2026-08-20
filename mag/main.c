#include "mag.h"
#include "render.h"

#define MAIN_RENDER_INTERVAL_MS USER_TIMER_MINIMUM
#define MAIN_TEST_DESKTOP_PREFIX TEXT("mag-test-")

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
    HDESK originalDesktop = NULL;
    HDESK testDesktop = NULL;

    UNREFERENCED_PARAMETER(hPrevInstance);

    if (fGraphicsSmoke && !main_BeginIsolatedTestDesktop(&originalDesktop, &testDesktop))
    {
      return ERROR_ACCESS_DENIED;
    }

    hWnd = magInitInstance(hInstance, fGraphicsSmoke ? SW_HIDE : nShowCmd);
    if (!hWnd)
    {
      if (fGraphicsSmoke)
      {
        main_EndIsolatedTestDesktop(originalDesktop, testDesktop);
      }
      return FALSE;
    }

    if (fGraphicsSmoke)
    {
      exitCode = renderRunGraphicsSmoke(hWnd);
      DestroyWindow(hWnd);
      main_EndIsolatedTestDesktop(originalDesktop, testDesktop);
      return exitCode;
    }

    /* Never expose an uninitialized redirection surface.  The first content
       frame is prepared while the HWND is hidden, then the already-populated
       surface is revealed and refreshed for the visible geometry. */
    if (!renderSubmit(hWnd))
    {
      DestroyWindow(hWnd);
      return ERROR_OPEN_FAILED;
    }
    ShowWindow(hWnd, nShowCmd);
    if (!renderSubmit(hWnd))
    {
      ShowWindow(hWnd, SW_HIDE);
      DestroyWindow(hWnd);
      return ERROR_WRITE_FAULT;
    }
    DwmFlush();

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
