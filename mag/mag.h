#pragma once

#include "framework.h"

#define WM_MAG_RENDER (WM_APP + 2)
#define WM_MAG_PRESENTATION_STATUS (WM_APP + 3)

HWND magInitInstance(HINSTANCE, int);
void mag_OnTimer(HWND hWnd, UINT_PTR idEvent);
void mag_UpdateViewWindowStyle(HWND hWnd);
BOOL mag_RunSettingsDialogSmoke(
  HWND hWnd,
  LPTSTR reason,
  UINT reasonCount);
