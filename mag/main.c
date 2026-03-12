#include "framework.h"

typedef long NTSTATUS;

NTSYSCALLAPI
NTSTATUS
NTAPI
NtDelayExecution(
    _In_ BOOLEAN Alertable,
    _In_ PLARGE_INTEGER DelayInterval
    );

NTSYSCALLAPI
NTSTATUS
NTAPI
NtYieldExecution(
    VOID
    );

#define MILLISECONDS_FROM_100NANOSECONDS(durationNanoS) ((durationNanoS) / (1000.0 * 10.0))
#define MILLISECONDS_TO_100NANOSECONDS(durationMs)      ((durationMs) * 1000 * 10)

static
VOID PFORCEINLINE WINAPI 
TimerAPCProc(
    LPVOID lpArg,               // Data value
    DWORD  dwTimerLowValue,     // Timer low value
    DWORD  dwTimerHighValue)    // Timer high value

{
    HWND hwnd;

    hwnd = lpArg;

    mag_OnTimer(hwnd, 0);
    NtYieldExecution();

    UNREFERENCED_PARAMETER(dwTimerLowValue);
    UNREFERENCED_PARAMETER(dwTimerHighValue);
}

int 
WINAPI 
_tWinMain(
  _In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPSTR lpCmdLine,
  _In_ int nShowCmd)
{
    MSG msg;
    HANDLE hTimer;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!magInitInstance(hInstance, nShowCmd))
    {
      return FALSE;
    }

    if (hTimer = CreateWaitableTimer(NULL, FALSE, NULL))
    {
      LARGE_INTEGER liDueTime = { 0 };
      GUITHREADINFO gti = { sizeof(gti) };

      if (GetGUIThreadInfo(GetCurrentThreadId(), &gti) &&
          SetWaitableTimer(hTimer, &liDueTime, 2 *USER_TIMER_MINIMUM, TimerAPCProc, gti.hwndActive, FALSE))
      {
        LARGE_INTEGER liPumpTime;

        liPumpTime.QuadPart = -MILLISECONDS_TO_100NANOSECONDS(1.0);

        while (PumpMessageQueue(gti.hwndActive))
        {
          GetInputState();
          NtDelayExecution(TRUE, &liPumpTime);
        }
      }
    }

    return 0;
}
