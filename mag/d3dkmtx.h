#pragma once

#include "framework.h"

typedef struct MAGD3DKMTDISPLAYPLANECAPS
{
  BOOL queried;
  BOOL directFlip;
  BOOL independentFlip;
  BOOL multiPlaneOverlay;
  BOOL multiPlaneOverlayDecode;
  BOOL panelFitter;
  BOOL mpo3Ddi;
  BOOL mpoKernelCaps;
  BOOL mpoHudKernel;
  BOOL mpoHud;
  BOOL mpoSecondary;
  BOOL independentFlipSecondary;
  BOOL mpoStretch;
  BOOL overlayCapsKnown;
  BOOL postCompositionCaps;
  BOOL overlayRotation;
  BOOL overlayRotationWithoutIndependentFlip;
  BOOL overlayVerticalFlip;
  BOOL overlayHorizontalFlip;
  BOOL overlayStretchRgb;
  BOOL overlayStretchYuv;
  BOOL overlayBilinearFilter;
  BOOL overlayHighFilter;
  BOOL overlayShared;
  BOOL overlayImmediate;
  BOOL overlayPlane0ForVirtualModeOnly;
  BOOL overlayVersion3Ddi;
  BOOL displayCapsKnown;
  BOOL displayPreferPhysicallyContiguous;
  BOOL displayCursorScaledWithMpoPlane0;
  BOOL displayCursorNoXorWithMpo;
  BOOL scanoutCapsKnown;
  BOOL crossAdapterSupportKnown;
  BOOL wddm3CapsKnown;
  BOOL hardwareFlipQueueSupported;
  BOOL hardwareFlipQueueEnabled;
  BOOL displayableSupported;
  UINT hardwareFlipQueueSupportState;
  UINT vidPnSourceId;
  UINT maxPlanes;
  UINT maxRgbPlanes;
  UINT maxYuvPlanes;
  UINT overlayCaps;
  UINT scanoutCaps;
  UINT crossAdapterSupportTier;
  FLOAT maxStretchFactor;
  FLOAT maxShrinkFactor;
  FLOAT postCompositionMaxStretchFactor;
  FLOAT postCompositionMaxShrinkFactor;
} MAGD3DKMTDISPLAYPLANECAPS;

BOOL D3DKMTWaitForVerticalBlank(HWND hWnd);
BOOL D3DKMTQueryDisplayPlaneCaps(
  LPCWSTR deviceName,
  MAGD3DKMTDISPLAYPLANECAPS* caps);
