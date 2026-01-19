#pragma once

#include "framework.h"

BOOL DwmInitPrivate(void);

BOOL DwmCreateDevice(void);

BOOL DwmCreateThumbnail(HWND hWnd);

BOOL DwmUpdateVisual(HWND hWnd);

BOOL DwmBindVisual(HWND hWnd);

BOOL DwmEnableWindowComposition(HWND hWnd, BOOL fEnable);