#include "mag.h"

#define MAIN_RENDER_INTERVAL_MS USER_TIMER_MINIMUM

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
    ULONGLONG ullNextRenderTime;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    hWnd = magInitInstance(hInstance, nShowCmd);
    if (!hWnd)
    {
      return FALSE;
    }

    ullNextRenderTime = GetTickCount64();
    while (main_PumpMessages(hWnd, &exitCode))
    {
      DWORD dwTimeout;
      DWORD dwWait;
      ULONGLONG ullNow = GetTickCount64();

      if (ullNow >= ullNextRenderTime)
      {
        if (IsWindow(hWnd))
        {
          mag_OnTimer(hWnd, 0);
          ullNextRenderTime = ullNow + MAIN_RENDER_INTERVAL_MS;
        }
        else
        {
          break;
        }
      }

      ullNow = GetTickCount64();
      dwTimeout = (ullNow >= ullNextRenderTime) ? 0 : (DWORD)(ullNextRenderTime - ullNow);
      dwWait = MsgWaitForMultipleObjects(0, NULL, FALSE, dwTimeout, QS_ALLINPUT);

      if (WAIT_FAILED == dwWait)
      {
        break;
      }
    }

    return exitCode;
}
