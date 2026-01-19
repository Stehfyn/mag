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
