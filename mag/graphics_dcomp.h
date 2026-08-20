#pragma once

#include <windows.h>
#include <unknwn.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MAGDCOMPPRESENTER MAGDCOMPPRESENTER;

BOOL magDCompPresenterCreate(
  HWND hWnd,
  IUnknown* content,
  MAGDCOMPPRESENTER** presenterOut);
BOOL magDCompPresenterCreateFromSurfaceHandle(
  HWND hWnd,
  HANDLE compositionSurfaceHandle,
  MAGDCOMPPRESENTER** presenterOut);
void magDCompPresenterDestroy(MAGDCOMPPRESENTER* presenter);
BOOL magDCompPresenterSetEnabled(MAGDCOMPPRESENTER* presenter, BOOL enabled);
BOOL magDCompPresenterSetOpacity(MAGDCOMPPRESENTER* presenter, FLOAT opacity);
BOOL magDCompPresenterGetNextEstimatedFrameTime(
  MAGDCOMPPRESENTER* presenter,
  LONGLONG* frameTime);

#ifdef __cplusplus
}
#endif
