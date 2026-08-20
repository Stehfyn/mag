#pragma once

#include "graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MAGUIRENDERER MAGUIRENDERER;

BOOL magUiRendererCreate(
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  SIZE clientSize,
  MAGUIRENDERER** rendererOut,
  LPTSTR reason,
  UINT reasonCount);
void magUiRendererDestroy(MAGUIRENDERER* renderer);
BOOL magUiRendererResize(MAGUIRENDERER* renderer, SIZE clientSize);
BOOL magUiRendererCompose(
  MAGUIRENDERER* renderer,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output);
BOOL magUiRendererComposeWithoutText(
  MAGUIRENDERER* renderer,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output);
const MAGGLYPHATLAS* magUiRendererGetGlyphAtlas(const MAGUIRENDERER* renderer);
UINT64 magUiRendererGetSurfaceGeneration(const MAGUIRENDERER* renderer);

#ifdef __cplusplus
}
#endif
