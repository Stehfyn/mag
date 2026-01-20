#pragma once

#include "framework.h"

BOOL SetWindowAlwaysOnTop(HWND hWnd, BOOL fEnable);

BOOL SetWindowOwner(HWND hWnd, HWND hWndOwner);

BOOL SetCurrentProcessEfficiencyQoS(void);

BOOL InMenu(void);