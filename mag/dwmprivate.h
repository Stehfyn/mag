#pragma once

#include <windows.h>

#define DWM_PRIVATE_MAX_DRAW_COMMANDS 64

typedef enum DWMPRIVATEDRAWCOMMANDTYPE
{
  DWM_PRIVATE_DRAW_FILL = 0,
  DWM_PRIVATE_DRAW_STROKE
} DWMPRIVATEDRAWCOMMANDTYPE;

typedef struct DWMPRIVATEDRAWCOMMAND
{
  DWMPRIVATEDRAWCOMMANDTYPE type;
  RECT                      rc;
  FLOAT                     color[4];
  UINT                      thickness;
} DWMPRIVATEDRAWCOMMAND, *LPDWMPRIVATEDRAWCOMMAND;

typedef struct DWMPRIVATECAPTURESTATE DWMPRIVATECAPTURESTATE;

#ifdef __cplusplus
extern "C" {
#endif

BOOL DwmPrivateCaptureCreate(HWND hWnd, DWMPRIVATECAPTURESTATE** lplpState);

void DwmPrivateCaptureDestroy(DWMPRIVATECAPTURESTATE* lpState);

BOOL DwmPrivateCaptureUpdate(
  DWMPRIVATECAPTURESTATE* lpState,
  const RECT*             lprcDesktop,
  const RECT*             lprcViewSource,
  const RECT*             lprcViewDestination,
  SIZE                    targetSize,
  const DWMPRIVATEDRAWCOMMAND* lpDrawCommands,
  UINT                    drawCommandCount);

UINT64 DwmPrivateCaptureGetResourceGeneration(
  const DWMPRIVATECAPTURESTATE* lpState);

#ifdef __cplusplus
}
#endif
