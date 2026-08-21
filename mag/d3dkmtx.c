#include "d3dkmtx.h"

typedef LONG NTSTATUS;
#include <d3dkmthk.h>

/* These query payloads are stable public D3DKMT ABIs introduced after the
 * oldest Windows 10 SDK that MAG still builds against.  Keep the narrow C
 * declarations local so runtime probing does not raise the SDK floor. */
typedef struct MAG_D3DKMT_QUERY_SCANOUT_CAPS
{
  UINT vidPnSourceId;
  UINT caps;
} MAG_D3DKMT_QUERY_SCANOUT_CAPS;

typedef struct MAG_D3DKMT_DISPLAY_CAPS
{
  UINT64 value;
} MAG_D3DKMT_DISPLAY_CAPS;

typedef struct MAG_D3DKMT_CROSS_ADAPTER_SUPPORT
{
  UINT supportTier;
} MAG_D3DKMT_CROSS_ADAPTER_SUPPORT;

typedef struct MAG_D3DKMT_WDDM3_CAPS
{
  UINT value;
} MAG_D3DKMT_WDDM3_CAPS;

enum
{
  MAG_KMTQAITYPE_SCANOUT_CAPS = 67,
  MAG_KMTQAITYPE_DISPLAY_CAPS = 74,
  MAG_KMTQAITYPE_CROSS_ADAPTER_RESOURCE_SUPPORT = 76,
  MAG_KMTQAITYPE_WDDM_3_0_CAPS = 77,
};

static BOOL D3DKMTQueryAdapterBoolean(
  D3DKMT_HANDLE adapter,
  KMTQUERYADAPTERINFOTYPE type,
  void* value,
  UINT valueSize)
{
    D3DKMT_QUERYADAPTERINFO query = { 0 };

    query.hAdapter = adapter;
    query.Type = type;
    query.pPrivateDriverData = value;
    query.PrivateDriverDataSize = valueSize;
    return 0 == D3DKMTQueryAdapterInfo(&query);
}

BOOL D3DKMTQueryDisplayPlaneCaps(
  LPCWSTR deviceName,
  MAGD3DKMTDISPLAYPLANECAPS* caps)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME openAdapter = { 0 };
    D3DKMT_CLOSEADAPTER closeAdapter = { 0 };
    D3DKMT_DIRECTFLIP_SUPPORT directFlip = { 0 };
    D3DKMT_INDEPENDENTFLIP_SUPPORT independentFlip = { 0 };
    D3DKMT_MULTIPLANEOVERLAY_SUPPORT multiPlaneOverlay = { 0 };
    D3DKMT_MULTIPLANEOVERLAY_DECODE_SUPPORT multiPlaneOverlayDecode = { 0 };
    D3DKMT_PANELFITTER_SUPPORT panelFitter = { 0 };
    D3DKMT_MPO3DDI_SUPPORT mpo3Ddi = { 0 };
    D3DKMT_MPOKERNELCAPS_SUPPORT mpoKernelCaps = { 0 };
    D3DKMT_MULTIPLANEOVERLAY_HUD_SUPPORT mpoHud = { 0 };
    D3DKMT_MULTIPLANEOVERLAY_SECONDARY_SUPPORT mpoSecondary = { 0 };
    D3DKMT_INDEPENDENTFLIP_SECONDARY_SUPPORT independentFlipSecondary = { 0 };
    D3DKMT_MULTIPLANEOVERLAY_STRETCH_SUPPORT mpoStretch = { 0 };
    D3DKMT_GET_MULTIPLANE_OVERLAY_CAPS overlay = { 0 };
    D3DKMT_GET_POST_COMPOSITION_CAPS postComposition = { 0 };
    MAG_D3DKMT_QUERY_SCANOUT_CAPS scanout = { 0 };
    MAG_D3DKMT_DISPLAY_CAPS displayCaps = { 0 };
    MAG_D3DKMT_CROSS_ADAPTER_SUPPORT crossAdapter = { 0 };
    MAG_D3DKMT_WDDM3_CAPS wddm3 = { 0 };
    PFND3DKMT_GETMULTIPLANEOVERLAYCAPS getOverlayCaps = NULL;
    PFND3DKMT_GETPOSTCOMPOSITIONCAPS getPostCompositionCaps = NULL;
    HMODULE gdi32;

    if (!deviceName || !deviceName[0] || !caps)
    {
      return FALSE;
    }
    ZeroMemory(caps, sizeof(*caps));
    lstrcpynW(
      openAdapter.DeviceName,
      deviceName,
      ARRAYSIZE(openAdapter.DeviceName));
    if (0 != D3DKMTOpenAdapterFromGdiDisplayName(&openAdapter))
    {
      return FALSE;
    }

    caps->vidPnSourceId = openAdapter.VidPnSourceId;
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_DIRECTFLIP_SUPPORT,
          &directFlip,
          sizeof(directFlip)))
    {
      caps->queried = TRUE;
      caps->directFlip = directFlip.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_INDEPENDENTFLIP_SUPPORT,
          &independentFlip,
          sizeof(independentFlip)))
    {
      caps->queried = TRUE;
      caps->independentFlip = independentFlip.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_MULTIPLANEOVERLAY_SUPPORT,
          &multiPlaneOverlay,
          sizeof(multiPlaneOverlay)))
    {
      caps->queried = TRUE;
      caps->multiPlaneOverlay = multiPlaneOverlay.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_QUERY_MULTIPLANEOVERLAY_DECODE_SUPPORT,
          &multiPlaneOverlayDecode,
          sizeof(multiPlaneOverlayDecode)))
    {
      caps->queried = TRUE;
      caps->multiPlaneOverlayDecode = multiPlaneOverlayDecode.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_PANELFITTER_SUPPORT,
          &panelFitter,
          sizeof(panelFitter)))
    {
      caps->queried = TRUE;
      caps->panelFitter = panelFitter.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_MPO3DDI_SUPPORT,
          &mpo3Ddi,
          sizeof(mpo3Ddi)))
    {
      caps->queried = TRUE;
      caps->mpo3Ddi = mpo3Ddi.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_MPOKERNELCAPS_SUPPORT,
          &mpoKernelCaps,
          sizeof(mpoKernelCaps)))
    {
      caps->queried = TRUE;
      caps->mpoKernelCaps = mpoKernelCaps.Supported;
    }
    mpoHud.VidPnSourceId = openAdapter.VidPnSourceId;
    mpoHud.Update = TRUE;
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_MULTIPLANEOVERLAY_HUD_SUPPORT,
          &mpoHud,
          sizeof(mpoHud)))
    {
      caps->queried = TRUE;
      caps->mpoHudKernel = mpoHud.KernelSupported;
      caps->mpoHud = mpoHud.HudSupported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_MULTIPLANEOVERLAY_SECONDARY_SUPPORT,
          &mpoSecondary,
          sizeof(mpoSecondary)))
    {
      caps->queried = TRUE;
      caps->mpoSecondary = mpoSecondary.Supported;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_INDEPENDENTFLIP_SECONDARY_SUPPORT,
          &independentFlipSecondary,
          sizeof(independentFlipSecondary)))
    {
      caps->queried = TRUE;
      caps->independentFlipSecondary = independentFlipSecondary.Supported;
    }
    mpoStretch.VidPnSourceId = openAdapter.VidPnSourceId;
    mpoStretch.Update = TRUE;
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          KMTQAITYPE_MULTIPLANEOVERLAY_STRETCH_SUPPORT,
          &mpoStretch,
          sizeof(mpoStretch)))
    {
      caps->queried = TRUE;
      caps->mpoStretch = mpoStretch.Supported;
    }
    scanout.vidPnSourceId = openAdapter.VidPnSourceId;
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          (KMTQUERYADAPTERINFOTYPE)MAG_KMTQAITYPE_SCANOUT_CAPS,
          &scanout,
          sizeof(scanout)))
    {
      caps->queried = TRUE;
      caps->scanoutCapsKnown = TRUE;
      caps->scanoutCaps = scanout.caps;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          (KMTQUERYADAPTERINFOTYPE)MAG_KMTQAITYPE_DISPLAY_CAPS,
          &displayCaps,
          sizeof(displayCaps)))
    {
      caps->queried = TRUE;
      caps->displayCapsKnown = TRUE;
      caps->displayPreferPhysicallyContiguous =
        0 != (displayCaps.value & 0x1ULL);
      caps->displayCursorScaledWithMpoPlane0 =
        0 != (displayCaps.value & 0x2ULL);
      caps->displayCursorNoXorWithMpo =
        0 != (displayCaps.value & 0x4ULL);
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          (KMTQUERYADAPTERINFOTYPE)
            MAG_KMTQAITYPE_CROSS_ADAPTER_RESOURCE_SUPPORT,
          &crossAdapter,
          sizeof(crossAdapter)))
    {
      caps->queried = TRUE;
      caps->crossAdapterSupportKnown = TRUE;
      caps->crossAdapterSupportTier = crossAdapter.supportTier;
    }
    if (D3DKMTQueryAdapterBoolean(
          openAdapter.hAdapter,
          (KMTQUERYADAPTERINFOTYPE)MAG_KMTQAITYPE_WDDM_3_0_CAPS,
          &wddm3,
          sizeof(wddm3)))
    {
      caps->queried = TRUE;
      caps->wddm3CapsKnown = TRUE;
      caps->hardwareFlipQueueSupportState = wddm3.value & 0x3U;
      caps->hardwareFlipQueueSupported = 0 != (wddm3.value & 0x3U);
      caps->hardwareFlipQueueEnabled = 0 != (wddm3.value & 0x4U);
      caps->displayableSupported = 0 != (wddm3.value & 0x8U);
    }

    gdi32 = GetModuleHandle(TEXT("gdi32.dll"));
    if (gdi32)
    {
      getOverlayCaps = (PFND3DKMT_GETMULTIPLANEOVERLAYCAPS)GetProcAddress(
        gdi32,
        "D3DKMTGetMultiPlaneOverlayCaps");
      getPostCompositionCaps =
        (PFND3DKMT_GETPOSTCOMPOSITIONCAPS)GetProcAddress(
          gdi32,
          "D3DKMTGetPostCompositionCaps");
    }
    if (getOverlayCaps)
    {
      overlay.hAdapter = openAdapter.hAdapter;
      overlay.VidPnSourceId = openAdapter.VidPnSourceId;
      if (0 == getOverlayCaps(&overlay))
      {
        caps->queried = TRUE;
        caps->overlayCapsKnown = TRUE;
        caps->maxPlanes = overlay.MaxPlanes;
        caps->maxRgbPlanes = overlay.MaxRGBPlanes;
        caps->maxYuvPlanes = overlay.MaxYUVPlanes;
        caps->overlayCaps = overlay.OverlayCaps.Value;
        caps->overlayRotation = overlay.OverlayCaps.Rotation;
        caps->overlayRotationWithoutIndependentFlip =
          overlay.OverlayCaps.RotationWithoutIndependentFlip;
        caps->overlayVerticalFlip = overlay.OverlayCaps.VerticalFlip;
        caps->overlayHorizontalFlip = overlay.OverlayCaps.HorizontalFlip;
        caps->overlayStretchRgb = overlay.OverlayCaps.StretchRGB;
        caps->overlayStretchYuv = overlay.OverlayCaps.StretchYUV;
        caps->overlayBilinearFilter = overlay.OverlayCaps.BilinearFilter;
        caps->overlayHighFilter = overlay.OverlayCaps.HighFilter;
        caps->overlayShared = overlay.OverlayCaps.Shared;
        caps->overlayImmediate = overlay.OverlayCaps.Immediate;
        caps->overlayPlane0ForVirtualModeOnly =
          overlay.OverlayCaps.Plane0ForVirtualModeOnly;
        caps->overlayVersion3Ddi = overlay.OverlayCaps.Version3DDISupport;
        caps->maxStretchFactor = overlay.MaxStretchFactor;
        caps->maxShrinkFactor = overlay.MaxShrinkFactor;
        caps->multiPlaneOverlay = overlay.MaxPlanes > 1;
      }
    }
    if (getPostCompositionCaps)
    {
      postComposition.hAdapter = openAdapter.hAdapter;
      postComposition.VidPnSourceId = openAdapter.VidPnSourceId;
      if (0 == getPostCompositionCaps(&postComposition))
      {
        caps->queried = TRUE;
        caps->postCompositionCaps = TRUE;
        caps->postCompositionMaxStretchFactor =
          postComposition.MaxStretchFactor;
        caps->postCompositionMaxShrinkFactor =
          postComposition.MaxShrinkFactor;
      }
    }

    closeAdapter.hAdapter = openAdapter.hAdapter;
    D3DKMTCloseAdapter(&closeAdapter);
    return caps->queried;
}

BOOL D3DKMTWaitForVerticalBlank(HWND hWnd)
{
    NTSTATUS status;
    static D3DKMT_WAITFORVERTICALBLANKEVENT vbe = { 0 };

    if (!vbe.hAdapter)
    {
      D3DKMT_OPENADAPTERFROMHDC oa = { 0 };
      oa.hDc = GetDC(hWnd);

      if (!oa.hDc)
      {
        return FALSE;
      }

      status = D3DKMTOpenAdapterFromHdc(&oa);

      ReleaseDC(hWnd, oa.hDc);
    
      if (0 != status)
      {
        return FALSE;
      }

      vbe.hAdapter      = oa.hAdapter;
      vbe.VidPnSourceId = oa.VidPnSourceId;
      vbe.hDevice       = 0;
    }

    status = D3DKMTWaitForVerticalBlankEvent(&vbe);
    if (0 != status)
    {
      return FALSE;
    }

    {
      D3DKMT_GETSCANLINE gsl = { 0 };

      gsl.hAdapter = vbe.hAdapter;
      gsl.VidPnSourceId = vbe.VidPnSourceId;

      do
      {
        status = D3DKMTGetScanLine(&gsl);
      }
      while ((0 != status) || gsl.InVerticalBlank);
    }

    return TRUE;
}
