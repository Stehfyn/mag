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

typedef struct DWMPRIVATEVIEWTRANSFORM
{
  FLOAT m11;
  FLOAT m12;
  FLOAT m21;
  FLOAT m22;
  FLOAT dx;
  FLOAT dy;
  RECT  clip;
} DWMPRIVATEVIEWTRANSFORM, *LPDWMPRIVATEVIEWTRANSFORM;

typedef struct DWMPRIVATECAPTURESTATE DWMPRIVATECAPTURESTATE;

typedef enum DWMPRIVATEWINDOWCOVERAGE
{
  DWM_PRIVATE_WINDOW_COVERAGE_NONE = 0,
  DWM_PRIVATE_WINDOW_COVERAGE_SHARED = 0x1,
  DWM_PRIVATE_WINDOW_COVERAGE_DESKTOP = 0x2,
  DWM_PRIVATE_WINDOW_COVERAGE_TASKBAR = 0x4,
  DWM_PRIVATE_WINDOW_COVERAGE_EXCLUDED = 0x8,
  DWM_PRIVATE_WINDOW_COVERAGE_APPLICATION = 0x10
} DWMPRIVATEWINDOWCOVERAGE;

#ifdef __cplusplus
extern "C" {
#endif

BOOL DwmPrivateCaptureCreate(HWND hWnd, DWMPRIVATECAPTURESTATE** lplpState);

void DwmPrivateCaptureDestroy(DWMPRIVATECAPTURESTATE* lpState);

BOOL DwmPrivateCalculateViewTransform(
  const RECT* lprcDesktop,
  const RECT* lprcViewSource,
  const RECT* lprcViewDestination,
  DWMPRIVATEVIEWTRANSFORM* lpTransform);

BOOL DwmPrivateCaptureUpdate(
  DWMPRIVATECAPTURESTATE* lpState,
  const RECT*             lprcDesktop,
  const RECT*             lprcViewSource,
  const RECT*             lprcViewDestination,
  SIZE                    targetSize,
  const DWMPRIVATEDRAWCOMMAND* lpDrawCommands,
  UINT                    drawCommandCount,
  BOOL                    restartSequence,
  BOOL                    synchronize);

UINT64 DwmPrivateCaptureGetResourceGeneration(
  const DWMPRIVATECAPTURESTATE* lpState);

UINT DwmPrivateCaptureGetWindowCoverage(
  const DWMPRIVATECAPTURESTATE* lpState,
  HWND                          hWnd);

BOOL DwmPrivateCaptureGetWindowVisualPlacement(
  const DWMPRIVATECAPTURESTATE* lpState,
  HWND                          hWnd,
  RECT*                         lprcWindow,
  POINT*                        lpVisualOffset,
  UINT*                         lpZOrder);

#ifdef __cplusplus
}
#endif
