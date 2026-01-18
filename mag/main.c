#include "framework.h"

int 
WINAPI 
_tWinMain(
  _In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE hPrevInstance,
  _In_ LPSTR lpCmdLine,
  _In_ int nShowCmd)
{
    MSG msg;

    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

    if (!magInitInstance(hInstance, nShowCmd))
    {
      return FALSE;
    }

    while (GetMessage(&msg, NULL, 0, 0))
    {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }

    return (int)msg.wParam;
}
