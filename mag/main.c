#include "mag.h"
#include "render.h"

#define MAIN_RENDER_INTERVAL_MS USER_TIMER_MINIMUM

typedef struct MAINVBLANKTHREAD
{
  HWND   hWnd;
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

      if (!D3DKMTWaitForVerticalBlank(lpThread->hWnd))
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

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    hWnd = magInitInstance(hInstance, nShowCmd);
    if (!hWnd)
    {
      return FALSE;
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
