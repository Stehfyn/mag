#pragma once

#include "graphics.h"
#include "presentation.h"

#include <d3d11.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MAGPRESENTATIONMANAGERPRESENTER MAGPRESENTATIONMANAGERPRESENTER;

BOOL magPresentationManagerPresenterCreate(
  HWND hWnd,
  ID3D11Device* device,
  SIZE reservoirSize,
  const MAGPRESENTATIONSETTINGS* settings,
  MAGPRESENTATIONMANAGERPRESENTER** presenterOut,
  LPTSTR reason,
  UINT reasonCount);
void magPresentationManagerPresenterDestroy(
  MAGPRESENTATIONMANAGERPRESENTER* presenter);
BOOL magPresentationManagerPresenterSetEnabled(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  BOOL enabled);
BOOL magPresentationManagerPresenterResize(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  SIZE clientSize);
BOOL magPresentationManagerPresenterAcquire(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  BOOL allowWait,
  ID3D11Texture2D** textureOut,
  UINT* bufferIndexOut);
BOOL magPresentationManagerPresenterPresent(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  UINT bufferIndex,
  SIZE contentSize,
  const MAGPRESENTINTENT* intent);
HANDLE magPresentationManagerPresenterGetFrameWaitHandle(
  MAGPRESENTATIONMANAGERPRESENTER* presenter);
BOOL magPresentationManagerPresenterGetNextEstimatedFrameTime(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  LONGLONG* frameTime);
BOOL magPresentationManagerPresenterGetObservedTarget(
  MAGPRESENTATIONMANAGERPRESENTER* presenter,
  UINT* target);
UINT64 magPresentationManagerPresenterGetResourceGeneration(
  MAGPRESENTATIONMANAGERPRESENTER* presenter);

#ifdef __cplusplus
}
#endif
