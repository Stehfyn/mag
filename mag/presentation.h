#pragma once

#include "graphics.h"

#include <dxgi1_6.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAG_MAX_ADAPTERS 16
#define MAG_MAX_OUTPUTS 32
#define MAG_PRESENTATION_REASON_LENGTH 512

typedef enum MAGPRESENTATIONTARGET
{
  MAG_PRESENT_AUTO = 0,
  MAG_PRESENT_HARDWARE_LEGACY_FLIP,
  MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER,
  MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP,
  MAG_PRESENT_COMPOSED_FLIP,
  MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP,
  MAG_PRESENT_COMPOSED_COPY_GPU_GDI,
  MAG_PRESENT_COMPOSED_COPY_CPU_GDI,
  MAG_PRESENT_COUNT
} MAGPRESENTATIONTARGET;

typedef enum MAGSURFACEOWNERSHIP
{
  MAG_SURFACE_AUTO = 0,
  MAG_SURFACE_REDIRECTION,
  MAG_SURFACE_NO_REDIRECTION,
  MAG_SURFACE_COUNT
} MAGSURFACEOWNERSHIP;

typedef enum MAGCOMPOSITIONHOST
{
  MAG_HOST_AUTO = 0,
  MAG_HOST_REDIRECTED_HWND,
  MAG_HOST_TRADITIONAL_LAYERED,
  MAG_HOST_DIRECTCOMPOSITION,
  MAG_HOST_PRESENTATION_MANAGER,
  MAG_HOST_COUNT
} MAGCOMPOSITIONHOST;

typedef enum MAGCOPYREQUIREMENT
{
  MAG_COPY_AUTO_FASTEST = 0,
  MAG_COPY_STRICT_ZERO_COPY,
  MAG_COPY_ALLOW_GPU_LOCAL,
  MAG_COPY_ALLOW_CPU_ROUND_TRIP,
  MAG_COPY_COUNT
} MAGCOPYREQUIREMENT;

typedef enum MAGLAYEREDALPHAMODE
{
  MAG_LAYER_ALPHA_OPAQUE = 0,
  MAG_LAYER_ALPHA_CONSTANT,
  MAG_LAYER_ALPHA_COLOR_KEY,
  MAG_LAYER_ALPHA_PER_PIXEL_PREMULTIPLIED,
  MAG_LAYER_ALPHA_COUNT
} MAGLAYEREDALPHAMODE;

typedef enum MAGWAITABLESWAPCHAINMODE
{
  MAG_WAITABLE_SWAP_CHAIN_AUTO = 0,
  MAG_WAITABLE_SWAP_CHAIN_ENABLED,
  MAG_WAITABLE_SWAP_CHAIN_DISABLED,
  MAG_WAITABLE_SWAP_CHAIN_MODE_COUNT
} MAGWAITABLESWAPCHAINMODE;

typedef enum MAGDISPLAYADAPTERMODE
{
  MAG_DISPLAY_ADAPTER_AUTO = 0,
  MAG_DISPLAY_ADAPTER_FOLLOW_CAPTURE,
  MAG_DISPLAY_ADAPTER_EXPLICIT,
  MAG_DISPLAY_ADAPTER_MODE_COUNT
} MAGDISPLAYADAPTERMODE;

typedef enum MAGHARDWAREADAPTERMODE
{
  MAG_HARDWARE_ADAPTER_AUTO = 0,
  MAG_HARDWARE_ADAPTER_SAME_AS_DISPLAY,
  MAG_HARDWARE_ADAPTER_SAME_AS_CAPTURE,
  MAG_HARDWARE_ADAPTER_EXPLICIT,
  MAG_HARDWARE_ADAPTER_WARP,
  MAG_HARDWARE_ADAPTER_MODE_COUNT
} MAGHARDWAREADAPTERMODE;

typedef enum MAGCOPYCLASS
{
  MAG_COPY_CLASS_UNKNOWN = 0,
  MAG_COPY_CLASS_TRUE_ZERO_COPY,
  MAG_COPY_CLASS_COMPOSITOR_VISUAL,
  MAG_COPY_CLASS_GPU_LOCAL,
  MAG_COPY_CLASS_CPU_SOURCE,
  MAG_COPY_CLASS_CPU_ROUND_TRIP
} MAGCOPYCLASS;

typedef struct MAGNAMEDOPTION
{
  UINT id;
  LPCTSTR name;
} MAGNAMEDOPTION;

typedef struct MAGDISPLAYSELECTION
{
  MAGDISPLAYADAPTERMODE mode;
  LUID adapterLuid;
  TCHAR deviceName[CCHDEVICENAME];
} MAGDISPLAYSELECTION;

typedef struct MAGHARDWARESELECTION
{
  MAGHARDWAREADAPTERMODE mode;
  LUID adapterLuid;
} MAGHARDWARESELECTION;

typedef struct MAGPRESENTATIONSETTINGS
{
  UINT version;
  MAGPRESENTATIONTARGET target;
  MAGSURFACEOWNERSHIP surfaceOwnership;
  MAGCOMPOSITIONHOST host;
  MAGCOPYREQUIREMENT copyRequirement;
  MAGLAYEREDALPHAMODE alphaMode;
  BYTE constantAlpha;
  COLORREF colorKey;
  BOOL strictTarget;
  BOOL allowTearing;
  MAGWAITABLESWAPCHAINMODE waitableSwapChainMode;
  UINT bufferCount;
  UINT maximumFrameLatency;
  UINT syncInterval;
  MAGDISPLAYSELECTION display;
  MAGHARDWARESELECTION hardware;
} MAGPRESENTATIONSETTINGS;

typedef struct MAGADAPTERINFO
{
  LUID luid;
  TCHAR description[128];
  SIZE_T dedicatedVideoMemory;
  BOOL software;
  BOOL remote;
} MAGADAPTERINFO;

typedef struct MAGOUTPUTINFO
{
  UINT adapterIndex;
  HMONITOR monitor;
  TCHAR deviceName[CCHDEVICENAME];
  RECT desktopCoordinates;
  UINT refreshNumerator;
  UINT refreshDenominator;
  BOOL attachedToDesktop;
  BOOL hdr;
  BOOL hardwareCompositionSupportKnown;
  UINT hardwareCompositionSupport;
  BOOL displayPlaneCapsKnown;
  BOOL directFlipCapable;
  BOOL independentFlipCapable;
  BOOL multiPlaneOverlayCapable;
  BOOL multiPlaneOverlayDecodeCapable;
  BOOL panelFitterCapable;
  BOOL mpo3DdiCapable;
  BOOL mpoKernelCapsCapable;
  BOOL mpoHudKernelCapable;
  BOOL mpoHudCapable;
  BOOL mpoSecondaryCapable;
  BOOL independentFlipSecondaryCapable;
  BOOL mpoStretchCapable;
  BOOL overlayCapsKnown;
  BOOL postCompositionCapsKnown;
  BOOL overlayRotationCapable;
  BOOL overlayRotationWithoutIndependentFlip;
  BOOL overlayVerticalFlipCapable;
  BOOL overlayHorizontalFlipCapable;
  BOOL overlayStretchRgbCapable;
  BOOL overlayStretchYuvCapable;
  BOOL overlayBilinearFilterCapable;
  BOOL overlayHighFilterCapable;
  BOOL overlaySharedAcrossOutputs;
  BOOL overlayImmediateCapable;
  BOOL overlayPlane0ForVirtualModeOnly;
  BOOL overlayVersion3DdiCapable;
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
} MAGOUTPUTINFO;

typedef struct MAGADAPTERCATALOG
{
  MAGADAPTERINFO adapters[MAG_MAX_ADAPTERS];
  UINT adapterCount;
  MAGOUTPUTINFO outputs[MAG_MAX_OUTPUTS];
  UINT outputCount;
} MAGADAPTERCATALOG;

typedef enum MAGPRESENTATIONOBSERVATIONSOURCE
{
  MAG_PRESENT_OBSERVATION_NONE = 0,
  MAG_PRESENT_OBSERVATION_PRESENTATION_MANAGER,
  MAG_PRESENT_OBSERVATION_ETW,
  MAG_PRESENT_OBSERVATION_COUNT,
} MAGPRESENTATIONOBSERVATIONSOURCE;

typedef struct MAGPRESENTATIONSTATUS
{
  BOOL configurationSupported;
  BOOL flickerFree;
  BOOL presentationManagerAvailable;
  BOOL directCompositionAvailable;
  BOOL hardwareAdapterResolved;
  BOOL displayAdapterResolved;
  BOOL hardwareCompositionSupportKnown;
  BOOL displayPlaneCapsKnown;
  BOOL fullscreenHardwareCompositionCapable;
  BOOL windowedHardwareCompositionCapable;
  BOOL hardwareCompositionCursorStretchRisk;
  BOOL directFlipCapable;
  BOOL directFlipGeometryEligible;
  BOOL independentFlipCapable;
  BOOL multiPlaneOverlayCapable;
  BOOL multiPlaneOverlayDecodeCapable;
  BOOL panelFitterCapable;
  BOOL mpo3DdiCapable;
  BOOL mpoKernelCapsCapable;
  BOOL mpoHudKernelCapable;
  BOOL mpoHudCapable;
  BOOL mpoSecondaryCapable;
  BOOL independentFlipSecondaryCapable;
  BOOL mpoStretchCapable;
  BOOL overlayCapsKnown;
  BOOL postCompositionCapsKnown;
  BOOL overlayRotationCapable;
  BOOL overlayRotationWithoutIndependentFlip;
  BOOL overlayVerticalFlipCapable;
  BOOL overlayHorizontalFlipCapable;
  BOOL overlayStretchRgbCapable;
  BOOL overlayStretchYuvCapable;
  BOOL overlayBilinearFilterCapable;
  BOOL overlayHighFilterCapable;
  BOOL overlaySharedAcrossOutputs;
  BOOL overlayImmediateCapable;
  BOOL overlayPlane0ForVirtualModeOnly;
  BOOL overlayVersion3DdiCapable;
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
  BOOL independentFlipEligible;
  BOOL observationAvailable;
  BOOL observationFullFidelity;
  BOOL observationDxgKrnl;
  BOOL observationWin32k;
  BOOL observedTargetValid;
  BOOL strictTargetSatisfied;
  LUID hardwareAdapterLuid;
  LUID displayAdapterLuid;
  UINT hardwareCompositionSupport;
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
  DWORD observationError;
  MAGCOPYCLASS copyClass;
  MAGPRESENTATIONOBSERVATIONSOURCE observationSource;
  MAGPRESENTATIONTARGET observedTarget;
  UINT64 configurationGeneration;
  UINT64 geometryEpoch;
  TCHAR reason[MAG_PRESENTATION_REASON_LENGTH];
} MAGPRESENTATIONSTATUS;

void magPresentationSettingsSetDefaults(MAGPRESENTATIONSETTINGS* settings);
BOOL magPresentationSettingsEqual(
  const MAGPRESENTATIONSETTINGS* left,
  const MAGPRESENTATIONSETTINGS* right);

UINT magPresentationTargetCount(void);
const MAGNAMEDOPTION* magPresentationTargetAt(UINT index);
LPCTSTR magPresentationTargetName(MAGPRESENTATIONTARGET target);
LPCTSTR magPresentationObservationSourceName(
  MAGPRESENTATIONOBSERVATIONSOURCE source);
LPCTSTR magCrossAdapterSupportTierName(UINT tier);
UINT magSurfaceOwnershipCount(void);
const MAGNAMEDOPTION* magSurfaceOwnershipAt(UINT index);
UINT magCompositionHostCount(void);
const MAGNAMEDOPTION* magCompositionHostAt(UINT index);
UINT magCopyRequirementCount(void);
const MAGNAMEDOPTION* magCopyRequirementAt(UINT index);
UINT magLayeredAlphaModeCount(void);
const MAGNAMEDOPTION* magLayeredAlphaModeAt(UINT index);
UINT magWaitableSwapChainModeCount(void);
const MAGNAMEDOPTION* magWaitableSwapChainModeAt(UINT index);

BOOL magAdapterLuidEqual(LUID left, LUID right);
BOOL magAdapterCatalogEnumerate(MAGADAPTERCATALOG* catalog, LPTSTR reason, UINT reasonCount);
const MAGADAPTERINFO* magAdapterCatalogFindAdapter(const MAGADAPTERCATALOG* catalog, LUID luid);
BOOL magAdapterOpenDxgi(LUID luid, IDXGIAdapter1** adapterOut);
const MAGOUTPUTINFO* magAdapterCatalogFindOutput(
  const MAGADAPTERCATALOG* catalog,
  LUID adapterLuid,
  LPCTSTR deviceName);
BOOL magPresentationResolve(
  HWND hWnd,
  GRAPHICSAPI graphicsApi,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  const MAGPRESENTATIONSETTINGS* requested,
  const MAGADAPTERCATALOG* catalog,
  MAGPRESENTATIONSETTINGS* resolved,
  MAGPRESENTATIONSTATUS* status);

LPCTSTR magCopyClassName(MAGCOPYCLASS copyClass);

#ifdef __cplusplus
}
#endif
