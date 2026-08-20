#pragma once

#include "graphics.h"
#include "presentation.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MAGLAYEREDPRESENTER MAGLAYEREDPRESENTER;

BOOL magLayeredPresenterCreate(
  HWND hWnd,
  SIZE clientSize,
  MAGLAYEREDPRESENTER** presenterOut);
void magLayeredPresenterDestroy(MAGLAYEREDPRESENTER* presenter);
BOOL magLayeredPresenterPresent(
  MAGLAYEREDPRESENTER* presenter,
  HWND hWnd,
  const MAGPIXELBUFFER* frame,
  const MAGPRESENTATIONSETTINGS* presentation);
UINT64 magLayeredPresenterGetResourceGeneration(
  const MAGLAYEREDPRESENTER* presenter);

#ifdef __cplusplus
}
#endif
