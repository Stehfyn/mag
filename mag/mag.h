#pragma once

#include "framework.h"

#define WM_MAG_RENDER (WM_APP + 2)

HWND magInitInstance(HINSTANCE, int);
void mag_OnTimer(HWND hWnd, UINT_PTR idEvent);
