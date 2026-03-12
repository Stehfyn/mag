#include "winuserx.h"

BOOL SetWindowAlwaysOnTop(HWND hWnd, BOOL fEnable)
{
    HWND hWndInsertAfter = fEnable ? HWND_TOPMOST : HWND_NOTOPMOST;
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
    SetThreadPriorityBoost(GetCurrentThread(), FALSE);
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
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

HMENU LoadPopupMenu(HINSTANCE hInstance, LPCTSTR lpMenuName)
{
    HMENU hMenu;
    HMENU hSubMenu = NULL;

    if (hMenu = LoadMenu(hInstance, lpMenuName))
    {
        hSubMenu = GetSubMenu(hMenu, 0);
    }

    return hSubMenu;
}

void ForceTimerMessagesToBeCreatedIfNecessary(LPMSG lpMsg)
{
    PeekMessage(lpMsg, NULL, WM_TIMER, WM_TIMER, PM_NOREMOVE);
}

BOOL PumpMessageQueue(HWND hwndPump)
{
    MSG  msg;
    BOOL fQuit = FALSE;
    SecureZeroMemory(&msg, sizeof(msg));

    if (hwndPump && IsWindow(hwndPump))
    {
      while (PeekMessage(&msg, hwndPump, 0, 0, PM_REMOVE | PM_NOYIELD))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        fQuit |= WM_QUIT == msg.message;
      }

      fQuit |= !IsWindow(hwndPump);
    }

    while (PeekMessage(&msg, 0, 0, 0, PM_REMOVE | PM_NOYIELD))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    
    return !fQuit;
}