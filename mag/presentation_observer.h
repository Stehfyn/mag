#pragma once

#include "presentation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MAGPRESENTATIONOBSERVERSTATUS
{
  BOOL active;
  BOOL dxgKrnlEnabled;
  BOOL win32kEnabled;
  DWORD error;
  DWORD startTraceError;
  DWORD dxgKrnlError;
  DWORD win32kError;
  DWORD openTraceError;
} MAGPRESENTATIONOBSERVERSTATUS;

BOOL magPresentationObserverStart(HWND hWnd);
void magPresentationObserverStop(void);
void magPresentationObserverReset(HWND hWnd);
BOOL magPresentationObserverGetLatest(
  HWND hWnd,
  MAGPRESENTATIONTARGET* target,
  UINT64* sequence);
void magPresentationObserverGetStatus(
  MAGPRESENTATIONOBSERVERSTATUS* status);

BOOL magPresentationObserverRunClassifierTests(
  LPTSTR reason,
  UINT reasonCount);

#ifdef __cplusplus
}
#endif
