#include "dwmapix.h"

#pragma comment (lib, "dwmapi")

BOOL DwmEnableWindowComposition(HWND hWnd, BOOL fEnable)
{
    DWM_BLURBEHIND bb = { 0 };

    if (fEnable)
    {
      bb.dwFlags  = DWM_BB_ENABLE | DWM_BB_BLURREGION;
      bb.hRgnBlur = CreateRectRgn(0, 0, -1, -1);
    }
    else
    {
      bb.dwFlags = DWM_BB_ENABLE;
    }

    bb.fEnable = fEnable;

    return S_OK == DwmEnableBlurBehindWindow(hWnd, &bb);
}