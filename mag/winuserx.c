#include "winuserx.h"

BOOL SetWindowAlwaysOnTop(HWND hWnd, BOOL fEnable)
{
    DWORD dwExStyle = GetWindowExStyle(hWnd);
    HWND hWndInsertAfter = fEnable ? HWND_TOPMOST : HWND_NOTOPMOST;

    if (fEnable)
    {
        dwExStyle |= WS_EX_TOPMOST;
    }
    else
    {
        dwExStyle &= ~WS_EX_TOPMOST;
    }

    return SetWindowPos(hWnd, hWndInsertAfter, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

BOOL SetWindowOwner(HWND hWnd, HWND hWndOwner)
{
    LONG_PTR offset;

    SetLastError(NO_ERROR);

    offset = SetWindowLongPtr(hWnd, GWLP_HWNDPARENT, hWndOwner);

    return (0 == offset) && (NO_ERROR != GetLastError());
}

BOOL SetCurrentProcessEfficiencyQoS(void)
{
    PROCESS_POWER_THROTTLING_STATE state = { 0 };

    state.Version = PROCESS_POWER_THROTTLING_CURRENT_VERSION;
    state.ControlMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;
    state.StateMask = PROCESS_POWER_THROTTLING_EXECUTION_SPEED;

    SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);

    return SetProcessInformation(GetCurrentProcess(), ProcessPowerThrottling, &state, sizeof(state));

}

BOOL InMenu(void)
{
    GUITHREADINFO gti = { sizeof(gti) };

    if (GetGUIThreadInfo(GetCurrentThreadId(), &gti))
    {
      return (GUI_INMENUMODE | GUI_SYSTEMMENUMODE | GUI_POPUPMENUMODE) & gti.flags;
    }

    return FALSE;
}
