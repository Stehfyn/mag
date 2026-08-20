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
void magDCompPresenterDestroy(MAGDCOMPPRESENTER* presenter);
BOOL magDCompPresenterSetEnabled(MAGDCOMPPRESENTER* presenter, BOOL enabled);

#ifdef __cplusplus
}
#endif
