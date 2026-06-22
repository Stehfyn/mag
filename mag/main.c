#include "mag.h"

#define MAIN_RENDER_INTERVAL_MS USER_TIMER_MINIMUM

typedef struct MAINVBLANKTHREAD
{
  HWND   hWnd;
  HANDLE hVBlankEvent;
  HANDLE hStopEvent;
  HANDLE hThread;
} MAINVBLANKTHREAD, *LPMAINVBLANKTHREAD;

DWORD WINAPI main_VBlankThreadProc(LPVOID lpParameter);
BOOL main_StartVBlankThread(HWND hWnd, LPMAINVBLANKTHREAD lpThread);
void main_StopVBlankThread(LPMAINVBLANKTHREAD lpThread);

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
      if (!IsWindow(lpThread->hWnd))
      {
        break;
      }

      if (D3DKMTWaitForVerticalBlank(lpThread->hWnd))
      {
        SetEvent(lpThread->hVBlankEvent);
      }
      else if (WAIT_OBJECT_0 == WaitForSingleObject(lpThread->hStopEvent, MAIN_RENDER_INTERVAL_MS))
      {
        break;
      }
    }

    return 0;
}

BOOL main_StartVBlankThread(HWND hWnd, LPMAINVBLANKTHREAD lpThread)
{
    ZeroMemory(lpThread, sizeof(*lpThread));
    lpThread->hWnd = hWnd;
    lpThread->hVBlankEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    lpThread->hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!lpThread->hVBlankEvent || !lpThread->hStopEvent)
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
      WaitForSingleObject(lpThread->hThread, INFINITE);
      CloseHandle(lpThread->hThread);
    }

    if (lpThread->hVBlankEvent)
    {
      CloseHandle(lpThread->hVBlankEvent);
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
    HANDLE waitHandles[1];
    DWORD waitHandleCount = 0;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    hWnd = magInitInstance(hInstance, nShowCmd);
    if (!hWnd)
    {
      return FALSE;
    }

    if (main_StartVBlankThread(hWnd, &vblankThread))
    {
      waitHandles[waitHandleCount++] = vblankThread.hVBlankEvent;
    }

    while (main_PumpMessages(hWnd, &exitCode))
    {
      DWORD dwTimeout;
      DWORD dwWait;

      dwTimeout = waitHandleCount ? INFINITE : MAIN_RENDER_INTERVAL_MS;
      dwWait = MsgWaitForMultipleObjects(waitHandleCount, waitHandles, FALSE, dwTimeout, QS_ALLINPUT);

      if (WAIT_FAILED == dwWait)
      {
        break;
      }

      if ((waitHandleCount && WAIT_OBJECT_0 == dwWait) || WAIT_TIMEOUT == dwWait)
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
