#include "framework.h"
#include "presentation.h"

#pragma comment(lib, "dxgi")

static BOOL magPresentationIsGdiFamily(GRAPHICSAPI graphicsApi)
{
    return GRAPHICS_API_GDI == graphicsApi ||
      GRAPHICS_API_GDIPLUS == graphicsApi;
}

#define MAG_PRESENTATION_SETTINGS_VERSION 1U

static const MAGNAMEDOPTION g_presentationTargets[] =
{
  { MAG_PRESENT_AUTO, TEXT("Auto (highest performance)") },
  { MAG_PRESENT_HARDWARE_LEGACY_FLIP, TEXT("Hardware: Legacy Flip") },
  { MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER, TEXT("Hardware: Legacy Copy to front buffer") },
  { MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP, TEXT("Hardware: Independent Flip") },
  { MAG_PRESENT_COMPOSED_FLIP, TEXT("Composed: Flip") },
  { MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP, TEXT("Hardware Composed: Independent Flip") },
  { MAG_PRESENT_COMPOSED_COPY_GPU_GDI, TEXT("Composed: Copy with GPU GDI") },
  { MAG_PRESENT_COMPOSED_COPY_CPU_GDI, TEXT("Composed: Copy with CPU GDI") },
};

static const MAGNAMEDOPTION g_surfaceOwnershipOptions[] =
{
  { MAG_SURFACE_AUTO, TEXT("Auto (host-owned)") },
  { MAG_SURFACE_REDIRECTION, TEXT("DWM redirection surface") },
  { MAG_SURFACE_NO_REDIRECTION, TEXT("No redirection surface") },
};

static const MAGNAMEDOPTION g_compositionHostOptions[] =
{
  { MAG_HOST_AUTO, TEXT("Auto (highest performance)") },
  { MAG_HOST_REDIRECTED_HWND, TEXT("Redirected HWND") },
  { MAG_HOST_TRADITIONAL_LAYERED, TEXT("Traditional layered window") },
  { MAG_HOST_DIRECTCOMPOSITION, TEXT("DirectComposition") },
  { MAG_HOST_PRESENTATION_MANAGER, TEXT("Windows 11 Presentation Manager") },
};

static const MAGNAMEDOPTION g_copyRequirementOptions[] =
{
  { MAG_COPY_AUTO_FASTEST, TEXT("Auto (fastest valid route)") },
  { MAG_COPY_STRICT_ZERO_COPY, TEXT("Strict zero-copy") },
  { MAG_COPY_ALLOW_GPU_LOCAL, TEXT("Allow GPU-local copy") },
  { MAG_COPY_ALLOW_CPU_ROUND_TRIP, TEXT("Allow CPU round trip (diagnostic)") },
};

static const MAGNAMEDOPTION g_layeredAlphaOptions[] =
{
  { MAG_LAYER_ALPHA_OPAQUE, TEXT("Opaque") },
  { MAG_LAYER_ALPHA_CONSTANT, TEXT("Constant alpha") },
  { MAG_LAYER_ALPHA_COLOR_KEY, TEXT("Color key") },
  { MAG_LAYER_ALPHA_PER_PIXEL_PREMULTIPLIED, TEXT("Per-pixel premultiplied alpha") },
};

static const MAGNAMEDOPTION g_waitableSwapChainOptions[] =
{
  { MAG_WAITABLE_SWAP_CHAIN_AUTO, TEXT("Auto (only while IF/MPO eligible)") },
  { MAG_WAITABLE_SWAP_CHAIN_ENABLED, TEXT("Enabled (DXGI latency object)") },
  { MAG_WAITABLE_SWAP_CHAIN_DISABLED, TEXT("Disabled (driver queue)") },
};

C_ASSERT(ARRAYSIZE(g_presentationTargets) == MAG_PRESENT_COUNT);
C_ASSERT(ARRAYSIZE(g_surfaceOwnershipOptions) == MAG_SURFACE_COUNT);
C_ASSERT(ARRAYSIZE(g_compositionHostOptions) == MAG_HOST_COUNT);
C_ASSERT(ARRAYSIZE(g_copyRequirementOptions) == MAG_COPY_COUNT);
C_ASSERT(ARRAYSIZE(g_layeredAlphaOptions) == MAG_LAYER_ALPHA_COUNT);
C_ASSERT(ARRAYSIZE(g_waitableSwapChainOptions) == MAG_WAITABLE_SWAP_CHAIN_MODE_COUNT);

static void magPresentationSetReason(LPTSTR destination, UINT destinationCount, LPCTSTR value)
{
    if (destination && destinationCount)
    {
      lstrcpyn(destination, value ? value : TEXT(""), destinationCount);
    }
}

static BOOL magPresentationFunctionAvailable(LPCWSTR moduleName, LPCSTR functionName)
{
    HMODULE module = LoadLibraryW(moduleName);
    BOOL available = FALSE;

    if (module)
    {
      available = NULL != GetProcAddress(module, functionName);
      FreeLibrary(module);
    }
    return available;
}

void magPresentationSettingsSetDefaults(MAGPRESENTATIONSETTINGS* settings)
{
    if (!settings)
    {
      return;
    }

    ZeroMemory(settings, sizeof(*settings));
    settings->version = MAG_PRESENTATION_SETTINGS_VERSION;
    settings->target = MAG_PRESENT_AUTO;
    settings->surfaceOwnership = MAG_SURFACE_AUTO;
    settings->host = MAG_HOST_AUTO;
    settings->copyRequirement = MAG_COPY_AUTO_FASTEST;
    settings->alphaMode = MAG_LAYER_ALPHA_OPAQUE;
    settings->constantAlpha = 255;
    settings->strictTarget = TRUE;
    settings->allowTearing = TRUE;
    settings->waitableSwapChainMode = MAG_WAITABLE_SWAP_CHAIN_AUTO;
    settings->bufferCount = 3;
    settings->maximumFrameLatency = 1;
    settings->syncInterval = 1;
    settings->display.mode = MAG_DISPLAY_ADAPTER_AUTO;
    settings->hardware.mode = MAG_HARDWARE_ADAPTER_AUTO;
}

BOOL magPresentationSettingsEqual(
  const MAGPRESENTATIONSETTINGS* left,
  const MAGPRESENTATIONSETTINGS* right)
{
    return left && right &&
      left->version == right->version &&
      left->target == right->target &&
      left->surfaceOwnership == right->surfaceOwnership &&
      left->host == right->host &&
      left->copyRequirement == right->copyRequirement &&
      left->alphaMode == right->alphaMode &&
      left->constantAlpha == right->constantAlpha &&
      left->colorKey == right->colorKey &&
      left->strictTarget == right->strictTarget &&
      left->allowTearing == right->allowTearing &&
      left->waitableSwapChainMode == right->waitableSwapChainMode &&
      left->bufferCount == right->bufferCount &&
      left->maximumFrameLatency == right->maximumFrameLatency &&
      left->syncInterval == right->syncInterval &&
      left->display.mode == right->display.mode &&
      magAdapterLuidEqual(left->display.adapterLuid, right->display.adapterLuid) &&
      0 == lstrcmpi(left->display.deviceName, right->display.deviceName) &&
      left->hardware.mode == right->hardware.mode &&
      magAdapterLuidEqual(left->hardware.adapterLuid, right->hardware.adapterLuid);
}

#define MAG_DEFINE_OPTION_ACCESSORS(prefix, array) \
  UINT prefix##Count(void) { return ARRAYSIZE(array); } \
  const MAGNAMEDOPTION* prefix##At(UINT index) { return index < ARRAYSIZE(array) ? &array[index] : NULL; }

MAG_DEFINE_OPTION_ACCESSORS(magPresentationTarget, g_presentationTargets)
MAG_DEFINE_OPTION_ACCESSORS(magSurfaceOwnership, g_surfaceOwnershipOptions)
MAG_DEFINE_OPTION_ACCESSORS(magCompositionHost, g_compositionHostOptions)
MAG_DEFINE_OPTION_ACCESSORS(magCopyRequirement, g_copyRequirementOptions)
MAG_DEFINE_OPTION_ACCESSORS(magLayeredAlphaMode, g_layeredAlphaOptions)
MAG_DEFINE_OPTION_ACCESSORS(magWaitableSwapChainMode, g_waitableSwapChainOptions)

LPCTSTR magPresentationTargetName(MAGPRESENTATIONTARGET target)
{
    UINT index;

    for (index = 0; index < ARRAYSIZE(g_presentationTargets); ++index)
    {
      if (g_presentationTargets[index].id == (UINT)target)
      {
        return g_presentationTargets[index].name;
      }
    }
    return TEXT("Unknown presentation target");
}

LPCTSTR magPresentationObservationSourceName(
  MAGPRESENTATIONOBSERVATIONSOURCE source)
{
    switch (source)
    {
    case MAG_PRESENT_OBSERVATION_PRESENTATION_MANAGER:
      return TEXT("Presentation Manager statistics");
    case MAG_PRESENT_OBSERVATION_ETW:
      return TEXT("process-scoped ETW");
    case MAG_PRESENT_OBSERVATION_NONE:
    default:
      return TEXT("no runtime observer");
    }
}

LPCTSTR magCrossAdapterSupportTierName(UINT tier)
{
    switch (tier)
    {
    case 1:
      return TEXT("copy");
    case 2:
      return TEXT("texture");
    case 3:
      return TEXT("scanout");
    case 0:
    default:
      return TEXT("none");
    }
}

BOOL magAdapterLuidEqual(LUID left, LUID right)
{
    return left.HighPart == right.HighPart && left.LowPart == right.LowPart;
}

static BOOL magAdapterCatalogContains(const MAGADAPTERCATALOG* catalog, LUID luid)
{
    UINT i;

    for (i = 0; i < catalog->adapterCount; ++i)
    {
      if (magAdapterLuidEqual(catalog->adapters[i].luid, luid))
      {
        return TRUE;
      }
    }
    return FALSE;
}

static BOOL magAdapterCatalogAppend(
  MAGADAPTERCATALOG* catalog,
  IDXGIAdapter1* adapter)
{
    DXGI_ADAPTER_DESC1 desc;
    MAGADAPTERINFO* adapterInfo;
    UINT adapterIndex;
    UINT outputIndex;

    if (!catalog || !adapter || catalog->adapterCount >= ARRAYSIZE(catalog->adapters) ||
        FAILED(IDXGIAdapter1_GetDesc1(adapter, &desc)) ||
        magAdapterCatalogContains(catalog, desc.AdapterLuid))
    {
      return FALSE;
    }

    adapterIndex = catalog->adapterCount++;
    adapterInfo = &catalog->adapters[adapterIndex];
    ZeroMemory(adapterInfo, sizeof(*adapterInfo));
    adapterInfo->luid = desc.AdapterLuid;
    adapterInfo->dedicatedVideoMemory = desc.DedicatedVideoMemory;
    adapterInfo->software = 0 != (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE);
    adapterInfo->remote = 0 != (desc.Flags & DXGI_ADAPTER_FLAG_REMOTE);
    lstrcpyn(adapterInfo->description, desc.Description, ARRAYSIZE(adapterInfo->description));

    for (outputIndex = 0; catalog->outputCount < ARRAYSIZE(catalog->outputs); ++outputIndex)
    {
      IDXGIOutput* output = NULL;
      DXGI_OUTPUT_DESC outputDesc;
      MAGOUTPUTINFO* outputInfo;
      HRESULT hr = IDXGIAdapter1_EnumOutputs(adapter, outputIndex, &output);

      if (DXGI_ERROR_NOT_FOUND == hr)
      {
        break;
      }
      if (FAILED(hr) || !output)
      {
        continue;
      }
      if (FAILED(IDXGIOutput_GetDesc(output, &outputDesc)))
      {
        IDXGIOutput_Release(output);
        continue;
      }

      outputInfo = &catalog->outputs[catalog->outputCount++];
      ZeroMemory(outputInfo, sizeof(*outputInfo));
      outputInfo->adapterIndex = adapterIndex;
      outputInfo->monitor = outputDesc.Monitor;
      outputInfo->desktopCoordinates = outputDesc.DesktopCoordinates;
      outputInfo->attachedToDesktop = outputDesc.AttachedToDesktop;
      lstrcpyn(outputInfo->deviceName, outputDesc.DeviceName, ARRAYSIZE(outputInfo->deviceName));

      {
        DEVMODE mode = { 0 };
        mode.dmSize = sizeof(mode);
        if (EnumDisplaySettings(outputDesc.DeviceName, ENUM_CURRENT_SETTINGS, &mode))
        {
          outputInfo->refreshNumerator = mode.dmDisplayFrequency;
          outputInfo->refreshDenominator = 1;
        }
      }
      {
        IDXGIOutput6* output6 = NULL;
        if (SUCCEEDED(IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput6, (void**)&output6)))
        {
          DXGI_OUTPUT_DESC1 desc1;
          UINT hardwareCompositionSupport = 0;
          if (SUCCEEDED(IDXGIOutput6_GetDesc1(output6, &desc1)))
          {
            outputInfo->hdr = DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020 == desc1.ColorSpace;
          }
          if (SUCCEEDED(IDXGIOutput6_CheckHardwareCompositionSupport(
                output6,
                &hardwareCompositionSupport)))
          {
            outputInfo->hardwareCompositionSupportKnown = TRUE;
            outputInfo->hardwareCompositionSupport = hardwareCompositionSupport;
          }
          IDXGIOutput6_Release(output6);
        }
      }
      {
        MAGD3DKMTDISPLAYPLANECAPS planeCaps;

        if (D3DKMTQueryDisplayPlaneCaps(
              outputDesc.DeviceName,
              &planeCaps))
        {
          outputInfo->displayPlaneCapsKnown = planeCaps.queried;
          outputInfo->directFlipCapable = planeCaps.directFlip;
          outputInfo->independentFlipCapable = planeCaps.independentFlip;
          outputInfo->multiPlaneOverlayCapable = planeCaps.multiPlaneOverlay;
          outputInfo->multiPlaneOverlayDecodeCapable =
            planeCaps.multiPlaneOverlayDecode;
          outputInfo->panelFitterCapable = planeCaps.panelFitter;
          outputInfo->mpo3DdiCapable = planeCaps.mpo3Ddi;
          outputInfo->mpoKernelCapsCapable = planeCaps.mpoKernelCaps;
          outputInfo->mpoHudKernelCapable = planeCaps.mpoHudKernel;
          outputInfo->mpoHudCapable = planeCaps.mpoHud;
          outputInfo->mpoSecondaryCapable = planeCaps.mpoSecondary;
          outputInfo->independentFlipSecondaryCapable =
            planeCaps.independentFlipSecondary;
          outputInfo->mpoStretchCapable = planeCaps.mpoStretch;
          outputInfo->overlayCapsKnown = planeCaps.overlayCapsKnown;
          outputInfo->postCompositionCapsKnown =
            planeCaps.postCompositionCaps;
          outputInfo->overlayRotationCapable = planeCaps.overlayRotation;
          outputInfo->overlayRotationWithoutIndependentFlip =
            planeCaps.overlayRotationWithoutIndependentFlip;
          outputInfo->overlayVerticalFlipCapable =
            planeCaps.overlayVerticalFlip;
          outputInfo->overlayHorizontalFlipCapable =
            planeCaps.overlayHorizontalFlip;
          outputInfo->overlayStretchRgbCapable = planeCaps.overlayStretchRgb;
          outputInfo->overlayStretchYuvCapable = planeCaps.overlayStretchYuv;
          outputInfo->overlayBilinearFilterCapable =
            planeCaps.overlayBilinearFilter;
          outputInfo->overlayHighFilterCapable = planeCaps.overlayHighFilter;
          outputInfo->overlaySharedAcrossOutputs = planeCaps.overlayShared;
          outputInfo->overlayImmediateCapable = planeCaps.overlayImmediate;
          outputInfo->overlayPlane0ForVirtualModeOnly =
            planeCaps.overlayPlane0ForVirtualModeOnly;
          outputInfo->overlayVersion3DdiCapable =
            planeCaps.overlayVersion3Ddi;
          outputInfo->displayCapsKnown = planeCaps.displayCapsKnown;
          outputInfo->displayPreferPhysicallyContiguous =
            planeCaps.displayPreferPhysicallyContiguous;
          outputInfo->displayCursorScaledWithMpoPlane0 =
            planeCaps.displayCursorScaledWithMpoPlane0;
          outputInfo->displayCursorNoXorWithMpo =
            planeCaps.displayCursorNoXorWithMpo;
          outputInfo->scanoutCapsKnown = planeCaps.scanoutCapsKnown;
          outputInfo->crossAdapterSupportKnown =
            planeCaps.crossAdapterSupportKnown;
          outputInfo->wddm3CapsKnown = planeCaps.wddm3CapsKnown;
          outputInfo->hardwareFlipQueueSupported =
            planeCaps.hardwareFlipQueueSupported;
          outputInfo->hardwareFlipQueueEnabled =
            planeCaps.hardwareFlipQueueEnabled;
          outputInfo->displayableSupported = planeCaps.displayableSupported;
          outputInfo->hardwareFlipQueueSupportState =
            planeCaps.hardwareFlipQueueSupportState;
          outputInfo->vidPnSourceId = planeCaps.vidPnSourceId;
          outputInfo->maxPlanes = planeCaps.maxPlanes;
          outputInfo->maxRgbPlanes = planeCaps.maxRgbPlanes;
          outputInfo->maxYuvPlanes = planeCaps.maxYuvPlanes;
          outputInfo->overlayCaps = planeCaps.overlayCaps;
          outputInfo->scanoutCaps = planeCaps.scanoutCaps;
          outputInfo->crossAdapterSupportTier =
            planeCaps.crossAdapterSupportTier;
          outputInfo->maxStretchFactor = planeCaps.maxStretchFactor;
          outputInfo->maxShrinkFactor = planeCaps.maxShrinkFactor;
          outputInfo->postCompositionMaxStretchFactor =
            planeCaps.postCompositionMaxStretchFactor;
          outputInfo->postCompositionMaxShrinkFactor =
            planeCaps.postCompositionMaxShrinkFactor;
        }
      }
      IDXGIOutput_Release(output);
    }
    return TRUE;
}

BOOL magAdapterCatalogEnumerate(MAGADAPTERCATALOG* catalog, LPTSTR reason, UINT reasonCount)
{
    IDXGIFactory1* factory1 = NULL;
    IDXGIFactory6* factory6 = NULL;
    HRESULT hr;
    UINT index;

    if (!catalog)
    {
      magPresentationSetReason(reason, reasonCount, TEXT("The adapter catalog destination is invalid."));
      return FALSE;
    }
    ZeroMemory(catalog, sizeof(*catalog));
    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory1);
    if (FAILED(hr))
    {
      magPresentationSetReason(reason, reasonCount, TEXT("DXGI could not enumerate display adapters."));
      return FALSE;
    }

    if (SUCCEEDED(IDXGIFactory1_QueryInterface(factory1, &IID_IDXGIFactory6, (void**)&factory6)))
    {
      for (index = 0; index < MAG_MAX_ADAPTERS; ++index)
      {
        IDXGIAdapter1* adapter = NULL;
        hr = IDXGIFactory6_EnumAdapterByGpuPreference(
          factory6,
          index,
          DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
          &IID_IDXGIAdapter1,
          (void**)&adapter);
        if (DXGI_ERROR_NOT_FOUND == hr)
        {
          break;
        }
        if (SUCCEEDED(hr) && adapter)
        {
          magAdapterCatalogAppend(catalog, adapter);
          IDXGIAdapter1_Release(adapter);
        }
      }
      IDXGIFactory6_Release(factory6);
    }
    else
    {
      for (index = 0; index < MAG_MAX_ADAPTERS; ++index)
      {
        IDXGIAdapter1* adapter = NULL;
        hr = IDXGIFactory1_EnumAdapters1(factory1, index, &adapter);
        if (DXGI_ERROR_NOT_FOUND == hr)
        {
          break;
        }
        if (SUCCEEDED(hr) && adapter)
        {
          magAdapterCatalogAppend(catalog, adapter);
          IDXGIAdapter1_Release(adapter);
        }
      }
    }
    IDXGIFactory1_Release(factory1);

    if (!catalog->adapterCount)
    {
      magPresentationSetReason(reason, reasonCount, TEXT("DXGI reported no usable adapters."));
      return FALSE;
    }
    magPresentationSetReason(reason, reasonCount, TEXT(""));
    return TRUE;
}

const MAGADAPTERINFO* magAdapterCatalogFindAdapter(const MAGADAPTERCATALOG* catalog, LUID luid)
{
    UINT i;

    if (!catalog)
    {
      return NULL;
    }
    for (i = 0; i < catalog->adapterCount; ++i)
    {
      if (magAdapterLuidEqual(catalog->adapters[i].luid, luid))
      {
        return &catalog->adapters[i];
      }
    }
    return NULL;
}

BOOL magAdapterOpenDxgi(LUID luid, IDXGIAdapter1** adapterOut)
{
    IDXGIFactory1* factory1 = NULL;
    IDXGIFactory4* factory4 = NULL;
    UINT index;
    BOOL found = FALSE;

    if (!adapterOut)
    {
      return FALSE;
    }
    *adapterOut = NULL;
    if (FAILED(CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory1)))
    {
      return FALSE;
    }

    if (SUCCEEDED(IDXGIFactory1_QueryInterface(factory1, &IID_IDXGIFactory4, (void**)&factory4)))
    {
      found = SUCCEEDED(IDXGIFactory4_EnumAdapterByLuid(
        factory4,
        luid,
        &IID_IDXGIAdapter1,
        (void**)adapterOut));
      IDXGIFactory4_Release(factory4);
    }
    if (!found)
    {
      for (index = 0; ; ++index)
      {
        IDXGIAdapter1* adapter = NULL;
        DXGI_ADAPTER_DESC1 desc;
        HRESULT hr = IDXGIFactory1_EnumAdapters1(factory1, index, &adapter);

        if (DXGI_ERROR_NOT_FOUND == hr)
        {
          break;
        }
        if (SUCCEEDED(hr) && adapter &&
            SUCCEEDED(IDXGIAdapter1_GetDesc1(adapter, &desc)) &&
            magAdapterLuidEqual(desc.AdapterLuid, luid))
        {
          *adapterOut = adapter;
          found = TRUE;
          break;
        }
        if (adapter)
        {
          IDXGIAdapter1_Release(adapter);
        }
      }
    }
    IDXGIFactory1_Release(factory1);
    return found;
}

const MAGOUTPUTINFO* magAdapterCatalogFindOutput(
  const MAGADAPTERCATALOG* catalog,
  LUID adapterLuid,
  LPCTSTR deviceName)
{
    UINT i;

    if (!catalog)
    {
      return NULL;
    }
    for (i = 0; i < catalog->outputCount; ++i)
    {
      const MAGOUTPUTINFO* output = &catalog->outputs[i];
      if (output->adapterIndex < catalog->adapterCount &&
          magAdapterLuidEqual(catalog->adapters[output->adapterIndex].luid, adapterLuid) &&
          (!deviceName || !deviceName[0] || 0 == lstrcmpi(output->deviceName, deviceName)))
      {
        return output;
      }
    }
    return NULL;
}

static const MAGOUTPUTINFO* magPresentationFindWindowOutput(
  HWND hWnd,
  const MAGADAPTERCATALOG* catalog)
{
    HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
    UINT i;

    for (i = 0; catalog && i < catalog->outputCount; ++i)
    {
      if (catalog->outputs[i].monitor == monitor)
      {
        return &catalog->outputs[i];
      }
    }
    return NULL;
}

static BOOL magPresentationResolveDisplay(
  HWND hWnd,
  const MAGPRESENTATIONSETTINGS* requested,
  const MAGADAPTERCATALOG* catalog,
  MAGPRESENTATIONSETTINGS* resolved,
  MAGPRESENTATIONSTATUS* status)
{
    const MAGOUTPUTINFO* output = NULL;

    if (MAG_DISPLAY_ADAPTER_EXPLICIT == requested->display.mode)
    {
      output = magAdapterCatalogFindOutput(
        catalog,
        requested->display.adapterLuid,
        requested->display.deviceName);
      if (!output)
      {
        lstrcpyn(status->reason, TEXT("The saved Display Adapter is disconnected or unavailable."), ARRAYSIZE(status->reason));
        return FALSE;
      }
    }
    else
    {
      output = magPresentationFindWindowOutput(hWnd, catalog);
      if (!output && catalog->outputCount)
      {
        output = &catalog->outputs[0];
      }
    }

    if (!output || output->adapterIndex >= catalog->adapterCount)
    {
      lstrcpyn(status->reason, TEXT("No active output can satisfy the Display Adapter selection."), ARRAYSIZE(status->reason));
      return FALSE;
    }

    resolved->display.adapterLuid = catalog->adapters[output->adapterIndex].luid;
    lstrcpyn(resolved->display.deviceName, output->deviceName, ARRAYSIZE(resolved->display.deviceName));
    status->displayAdapterLuid = resolved->display.adapterLuid;
    status->displayAdapterResolved = TRUE;
    status->hardwareCompositionSupportKnown =
      output->hardwareCompositionSupportKnown;
    status->hardwareCompositionSupport =
      output->hardwareCompositionSupport;
    status->displayPlaneCapsKnown = output->displayPlaneCapsKnown;
    status->directFlipCapable = output->directFlipCapable;
    status->independentFlipCapable = output->independentFlipCapable;
    status->multiPlaneOverlayCapable = output->multiPlaneOverlayCapable;
    status->multiPlaneOverlayDecodeCapable =
      output->multiPlaneOverlayDecodeCapable;
    status->panelFitterCapable = output->panelFitterCapable;
    status->mpo3DdiCapable = output->mpo3DdiCapable;
    status->mpoKernelCapsCapable = output->mpoKernelCapsCapable;
    status->mpoHudKernelCapable = output->mpoHudKernelCapable;
    status->mpoHudCapable = output->mpoHudCapable;
    status->mpoSecondaryCapable = output->mpoSecondaryCapable;
    status->independentFlipSecondaryCapable =
      output->independentFlipSecondaryCapable;
    status->mpoStretchCapable = output->mpoStretchCapable;
    status->overlayCapsKnown = output->overlayCapsKnown;
    status->postCompositionCapsKnown = output->postCompositionCapsKnown;
    status->overlayRotationCapable = output->overlayRotationCapable;
    status->overlayRotationWithoutIndependentFlip =
      output->overlayRotationWithoutIndependentFlip;
    status->overlayVerticalFlipCapable = output->overlayVerticalFlipCapable;
    status->overlayHorizontalFlipCapable = output->overlayHorizontalFlipCapable;
    status->overlayStretchRgbCapable = output->overlayStretchRgbCapable;
    status->overlayStretchYuvCapable = output->overlayStretchYuvCapable;
    status->overlayBilinearFilterCapable =
      output->overlayBilinearFilterCapable;
    status->overlayHighFilterCapable = output->overlayHighFilterCapable;
    status->overlaySharedAcrossOutputs = output->overlaySharedAcrossOutputs;
    status->overlayImmediateCapable = output->overlayImmediateCapable;
    status->overlayPlane0ForVirtualModeOnly =
      output->overlayPlane0ForVirtualModeOnly;
    status->overlayVersion3DdiCapable = output->overlayVersion3DdiCapable;
    status->displayCapsKnown = output->displayCapsKnown;
    status->displayPreferPhysicallyContiguous =
      output->displayPreferPhysicallyContiguous;
    status->displayCursorScaledWithMpoPlane0 =
      output->displayCursorScaledWithMpoPlane0;
    status->displayCursorNoXorWithMpo = output->displayCursorNoXorWithMpo;
    status->scanoutCapsKnown = output->scanoutCapsKnown;
    status->crossAdapterSupportKnown = output->crossAdapterSupportKnown;
    status->wddm3CapsKnown = output->wddm3CapsKnown;
    status->hardwareFlipQueueSupported = output->hardwareFlipQueueSupported;
    status->hardwareFlipQueueEnabled = output->hardwareFlipQueueEnabled;
    status->displayableSupported = output->displayableSupported;
    status->hardwareFlipQueueSupportState =
      output->hardwareFlipQueueSupportState;
    status->vidPnSourceId = output->vidPnSourceId;
    status->maxPlanes = output->maxPlanes;
    status->maxRgbPlanes = output->maxRgbPlanes;
    status->maxYuvPlanes = output->maxYuvPlanes;
    status->overlayCaps = output->overlayCaps;
    status->scanoutCaps = output->scanoutCaps;
    status->crossAdapterSupportTier = output->crossAdapterSupportTier;
    status->maxStretchFactor = output->maxStretchFactor;
    status->maxShrinkFactor = output->maxShrinkFactor;
    status->postCompositionMaxStretchFactor =
      output->postCompositionMaxStretchFactor;
    status->postCompositionMaxShrinkFactor =
      output->postCompositionMaxShrinkFactor;
    return TRUE;
}

static BOOL magPresentationResolveHardware(
  const MAGPRESENTATIONSETTINGS* requested,
  const MAGADAPTERCATALOG* catalog,
  MAGPRESENTATIONSETTINGS* resolved,
  MAGPRESENTATIONSTATUS* status)
{
    const MAGADAPTERINFO* adapter = NULL;

    switch (requested->hardware.mode)
    {
    case MAG_HARDWARE_ADAPTER_WARP:
      status->hardwareAdapterResolved = TRUE;
      status->hardwareAdapterLuid.LowPart = 0;
      status->hardwareAdapterLuid.HighPart = 0;
      return TRUE;
    case MAG_HARDWARE_ADAPTER_EXPLICIT:
      adapter = magAdapterCatalogFindAdapter(catalog, requested->hardware.adapterLuid);
      break;
    case MAG_HARDWARE_ADAPTER_SAME_AS_DISPLAY:
    case MAG_HARDWARE_ADAPTER_SAME_AS_CAPTURE:
      adapter = magAdapterCatalogFindAdapter(catalog, resolved->display.adapterLuid);
      break;
    case MAG_HARDWARE_ADAPTER_AUTO:
    default:
      {
        UINT i;
        for (i = 0; i < catalog->adapterCount; ++i)
        {
          if (!catalog->adapters[i].software && !catalog->adapters[i].remote)
          {
            adapter = &catalog->adapters[i];
            break;
          }
        }
      }
      break;
    }

    if (!adapter || adapter->software)
    {
      lstrcpyn(status->reason, TEXT("The selected Hardware Adapter is disconnected, software-only, or unavailable."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    resolved->hardware.adapterLuid = adapter->luid;
    status->hardwareAdapterLuid = adapter->luid;
    status->hardwareAdapterResolved = TRUE;
    return TRUE;
}

static BOOL magPresentationIsModernFlipTarget(
  MAGPRESENTATIONTARGET target)
{
    return MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP == target ||
      MAG_PRESENT_COMPOSED_FLIP == target ||
      MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP == target;
}

static BOOL magPresentationClientCoversOutput(
  HWND hWnd,
  const MAGADAPTERCATALOG* catalog,
  const MAGPRESENTATIONSETTINGS* resolved)
{
    const MAGOUTPUTINFO* output;
    RECT client;
    POINT points[2];

    if (!hWnd || !catalog || !resolved)
    {
      return FALSE;
    }
    output = magAdapterCatalogFindOutput(
      catalog,
      resolved->display.adapterLuid,
      resolved->display.deviceName);
    if (!output || !GetClientRect(hWnd, &client))
    {
      return FALSE;
    }
    points[0].x = client.left;
    points[0].y = client.top;
    points[1].x = client.right;
    points[1].y = client.bottom;
    if (!ClientToScreen(hWnd, &points[0]) ||
        !ClientToScreen(hWnd, &points[1]))
    {
      return FALSE;
    }
    return points[0].x <= output->desktopCoordinates.left &&
      points[0].y <= output->desktopCoordinates.top &&
      points[1].x >= output->desktopCoordinates.right &&
      points[1].y >= output->desktopCoordinates.bottom;
}

static MAGCOMPOSITIONHOST magPresentationChooseAutoHost(
  GRAPHICSAPI graphicsApi,
  CAPTUREAPI captureApi,
  MAGPRESENTATIONTARGET target,
  const MAGPRESENTATIONSTATUS* status)
{
    if (CAPTURE_API_DWM_THUMBNAIL == captureApi ||
        CAPTURE_API_DWM_PRIVATE_VISUAL == captureApi)
    {
      return MAG_HOST_DIRECTCOMPOSITION;
    }
    if (MAG_PRESENT_COMPOSED_COPY_CPU_GDI == target)
    {
      return magPresentationIsGdiFamily(graphicsApi)
        ? MAG_HOST_REDIRECTED_HWND
        : MAG_HOST_TRADITIONAL_LAYERED;
    }
    if (MAG_PRESENT_HARDWARE_LEGACY_FLIP == target ||
        MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER == target ||
        MAG_PRESENT_COMPOSED_COPY_GPU_GDI == target)
    {
      return MAG_HOST_REDIRECTED_HWND;
    }
    if (MAG_PRESENT_COMPOSED_FLIP == target &&
        (GRAPHICS_API_D3D11 == graphicsApi ||
         GRAPHICS_API_D3D12 == graphicsApi) &&
        status->directCompositionAvailable)
    {
      return MAG_HOST_DIRECTCOMPOSITION;
    }
    if (magPresentationIsModernFlipTarget(target) &&
        (GRAPHICS_API_D3D11 == graphicsApi ||
         GRAPHICS_API_D3D12 == graphicsApi ||
         magPresentationIsGdiFamily(graphicsApi)) &&
        status->presentationManagerAvailable &&
        magGraphicsIsInputDesktop())
    {
      return MAG_HOST_PRESENTATION_MANAGER;
    }
    if (MAG_PRESENT_AUTO == target &&
        (GRAPHICS_API_D3D11 == graphicsApi ||
         GRAPHICS_API_D3D12 == graphicsApi ||
         magPresentationIsGdiFamily(graphicsApi)) &&
        status->presentationManagerAvailable &&
        magGraphicsIsInputDesktop())
    {
      return MAG_HOST_PRESENTATION_MANAGER;
    }
    if ((MAG_PRESENT_AUTO == target || MAG_PRESENT_COMPOSED_FLIP == target) &&
        (GRAPHICS_API_D3D11 == graphicsApi ||
         GRAPHICS_API_D3D12 == graphicsApi) &&
        status->directCompositionAvailable)
    {
      return MAG_HOST_DIRECTCOMPOSITION;
    }
    return MAG_HOST_REDIRECTED_HWND;
}

static MAGPRESENTATIONTARGET magPresentationChooseAutoTarget(
  GRAPHICSAPI graphicsApi,
  MAGCOMPOSITIONHOST host)
{
    if (MAG_HOST_TRADITIONAL_LAYERED == host)
    {
      return MAG_PRESENT_COMPOSED_COPY_CPU_GDI;
    }
    if (MAG_HOST_PRESENTATION_MANAGER == host)
    {
      return MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP;
    }
    if (MAG_HOST_DIRECTCOMPOSITION == host)
    {
      return MAG_PRESENT_COMPOSED_FLIP;
    }
    if (GRAPHICS_API_D3D11 == graphicsApi ||
        GRAPHICS_API_D3D12 == graphicsApi ||
        GRAPHICS_API_D3D9 == graphicsApi)
    {
      return MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP;
    }
    if (GRAPHICS_API_OPENGL == graphicsApi)
    {
      return MAG_PRESENT_HARDWARE_LEGACY_FLIP;
    }
    if (GRAPHICS_API_VULKAN == graphicsApi)
    {
      return MAG_PRESENT_COMPOSED_FLIP;
    }
    return MAG_PRESENT_COMPOSED_COPY_CPU_GDI;
}

static BOOL magPresentationValidateTargetProfile(
  GRAPHICSAPI graphicsApi,
  CAPTUREAPI captureApi,
  const MAGPRESENTATIONSETTINGS* resolved,
  MAGPRESENTATIONSTATUS* status)
{
    const BOOL compositorCapture =
      CAPTURE_API_DWM_THUMBNAIL == captureApi ||
      CAPTURE_API_DWM_PRIVATE_VISUAL == captureApi;
    const BOOL d3dModern =
      GRAPHICS_API_D3D11 == graphicsApi ||
      GRAPHICS_API_D3D12 == graphicsApi;
    const BOOL presentationManagerRenderer =
      d3dModern || magPresentationIsGdiFamily(graphicsApi);

    if (compositorCapture)
    {
      if (MAG_HOST_DIRECTCOMPOSITION == resolved->host &&
          MAG_PRESENT_COMPOSED_FLIP == resolved->target)
      {
        return TRUE;
      }
      lstrcpyn(
        status->reason,
        TEXT("A compositor-owned DWM capture is a DirectComposition visual-tree route; it can be observed as Composed Flip but cannot be relabeled as an app-owned scanout plane."),
        ARRAYSIZE(status->reason));
      return FALSE;
    }

    switch (resolved->target)
    {
    case MAG_PRESENT_HARDWARE_LEGACY_FLIP:
      if ((GRAPHICS_API_D3D9 == graphicsApi ||
           GRAPHICS_API_OPENGL == graphicsApi) &&
          MAG_HOST_REDIRECTED_HWND == resolved->host)
      {
        return TRUE;
      }
      lstrcpyn(
        status->reason,
        TEXT("Hardware Legacy Flip requires the redirected Direct3D 9 D3DSWAPEFFECT_FLIP profile or an exchange-model WGL pixel format."),
        ARRAYSIZE(status->reason));
      return FALSE;

    case MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER:
      if ((GRAPHICS_API_D3D9 == graphicsApi ||
           GRAPHICS_API_OPENGL == graphicsApi) &&
          MAG_HOST_REDIRECTED_HWND == resolved->host)
      {
        return TRUE;
      }
      lstrcpyn(
        status->reason,
        TEXT("Hardware Legacy Copy requires the redirected Direct3D 9 D3DSWAPEFFECT_COPY profile or a copy-model WGL pixel format."),
        ARRAYSIZE(status->reason));
      return FALSE;

    case MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP:
      if (MAG_HOST_PRESENTATION_MANAGER == resolved->host &&
          presentationManagerRenderer)
      {
        break;
      }
      if (MAG_HOST_REDIRECTED_HWND == resolved->host &&
          (d3dModern || GRAPHICS_API_D3D9 == graphicsApi ||
           GRAPHICS_API_OPENGL == graphicsApi ||
           GRAPHICS_API_VULKAN == graphicsApi))
      {
        break;
      }
      lstrcpyn(
        status->reason,
        TEXT("Independent Flip requires an HWND flip-model chain or displayable Presentation Manager buffers on the selected adapter."),
        ARRAYSIZE(status->reason));
      return FALSE;

    case MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP:
      if ((MAG_HOST_PRESENTATION_MANAGER == resolved->host &&
           presentationManagerRenderer) ||
          (MAG_HOST_DIRECTCOMPOSITION == resolved->host && d3dModern) ||
          (MAG_HOST_REDIRECTED_HWND == resolved->host &&
           (d3dModern || GRAPHICS_API_D3D9 == graphicsApi ||
            GRAPHICS_API_OPENGL == graphicsApi ||
            GRAPHICS_API_VULKAN == graphicsApi)))
      {
        break;
      }
      lstrcpyn(
        status->reason,
        TEXT("Hardware-composed Independent Flip requires a displayable Presentation Manager surface, an MPO-eligible DirectComposition chain, or a windowed flip/exchange presenter."),
        ARRAYSIZE(status->reason));
      return FALSE;

    case MAG_PRESENT_COMPOSED_FLIP:
      if ((MAG_HOST_PRESENTATION_MANAGER == resolved->host &&
           presentationManagerRenderer) ||
          (MAG_HOST_DIRECTCOMPOSITION == resolved->host && d3dModern) ||
          (MAG_HOST_REDIRECTED_HWND == resolved->host &&
           (d3dModern || GRAPHICS_API_D3D9 == graphicsApi ||
            GRAPHICS_API_OPENGL == graphicsApi ||
            GRAPHICS_API_VULKAN == graphicsApi)))
      {
        return TRUE;
      }
      lstrcpyn(
        status->reason,
        TEXT("Composed Flip requires a flip-model DXGI, D3D9Ex, Vulkan, DirectComposition, or Presentation Manager presenter."),
        ARRAYSIZE(status->reason));
      return FALSE;

    case MAG_PRESENT_COMPOSED_COPY_GPU_GDI:
      if (MAG_HOST_REDIRECTED_HWND == resolved->host &&
          (GRAPHICS_API_D3D11 == graphicsApi ||
           GRAPHICS_API_D3D9 == graphicsApi ||
           GRAPHICS_API_OPENGL == graphicsApi))
      {
        return TRUE;
      }
      lstrcpyn(
        status->reason,
        TEXT("Composed Copy with GPU GDI requires the D3D11 DXGI BitBlt presenter, the D3D9 redirected BitBlt presenter, or a copy-model WGL pixel format."),
        ARRAYSIZE(status->reason));
      return FALSE;

    case MAG_PRESENT_COMPOSED_COPY_CPU_GDI:
      if (MAG_HOST_TRADITIONAL_LAYERED == resolved->host ||
          (MAG_HOST_REDIRECTED_HWND == resolved->host &&
           magPresentationIsGdiFamily(graphicsApi)))
      {
        return TRUE;
      }
      lstrcpyn(
        status->reason,
        TEXT("Composed Copy with CPU GDI requires the redirected GDI/GDI+ presenter or the CPU-backed traditional layered presenter."),
        ARRAYSIZE(status->reason));
      return FALSE;

    default:
      lstrcpyn(
        status->reason,
        TEXT("The presentation target is invalid."),
        ARRAYSIZE(status->reason));
      return FALSE;
    }

    return TRUE;
}

BOOL magPresentationResolve(
  HWND hWnd,
  GRAPHICSAPI graphicsApi,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  const MAGPRESENTATIONSETTINGS* requested,
  const MAGADAPTERCATALOG* catalog,
  MAGPRESENTATIONSETTINGS* resolved,
  MAGPRESENTATIONSTATUS* status)
{
    BOOL noRedirection;

    UNREFERENCED_PARAMETER(uiApi);
    UNREFERENCED_PARAMETER(textRenderer);

    if (!requested || !catalog || !resolved || !status ||
        requested->target >= MAG_PRESENT_COUNT ||
        requested->surfaceOwnership >= MAG_SURFACE_COUNT ||
        requested->host >= MAG_HOST_COUNT ||
        requested->copyRequirement >= MAG_COPY_COUNT ||
        requested->alphaMode >= MAG_LAYER_ALPHA_COUNT ||
        requested->waitableSwapChainMode >= MAG_WAITABLE_SWAP_CHAIN_MODE_COUNT ||
        requested->display.mode >= MAG_DISPLAY_ADAPTER_MODE_COUNT ||
        requested->hardware.mode >= MAG_HARDWARE_ADAPTER_MODE_COUNT)
    {
      return FALSE;
    }

    ZeroMemory(status, sizeof(*status));
    *resolved = *requested;
    status->presentationManagerAvailable = magPresentationFunctionAvailable(L"dcomp.dll", "CreatePresentationFactory");
    status->directCompositionAvailable = magPresentationFunctionAvailable(L"dcomp.dll", "DCompositionCreateDevice");

    if (!magPresentationResolveDisplay(hWnd, requested, catalog, resolved, status) ||
        !magPresentationResolveHardware(requested, catalog, resolved, status))
    {
      return FALSE;
    }
    if (requested->bufferCount < 2 || requested->bufferCount > 16 ||
        requested->maximumFrameLatency < 1 || requested->maximumFrameLatency > 16 ||
        requested->syncInterval > 4)
    {
      lstrcpyn(status->reason, TEXT("Presentation pacing values are outside their supported ranges."), ARRAYSIZE(status->reason));
      return FALSE;
    }

    if (MAG_HOST_AUTO == resolved->host)
    {
      resolved->host = magPresentationChooseAutoHost(
        graphicsApi,
        captureApi,
        resolved->target,
        status);
    }
    if (MAG_SURFACE_AUTO == resolved->surfaceOwnership)
    {
      resolved->surfaceOwnership =
        MAG_HOST_DIRECTCOMPOSITION == resolved->host ||
        MAG_HOST_PRESENTATION_MANAGER == resolved->host
          ? MAG_SURFACE_NO_REDIRECTION
          : MAG_SURFACE_REDIRECTION;
    }
    noRedirection = MAG_SURFACE_NO_REDIRECTION == resolved->surfaceOwnership;

    if (MAG_HARDWARE_ADAPTER_WARP == resolved->hardware.mode &&
        GRAPHICS_API_D3D11 != graphicsApi && GRAPHICS_API_D3D12 != graphicsApi &&
        !(magPresentationIsGdiFamily(graphicsApi) &&
          MAG_HOST_PRESENTATION_MANAGER == resolved->host))
    {
      lstrcpyn(status->reason, TEXT("WARP is available to Direct3D renderers and to the GDI-to-Presentation-Manager bridge."), ARRAYSIZE(status->reason));
      return FALSE;
    }

    if (MAG_HOST_PRESENTATION_MANAGER == resolved->host && !status->presentationManagerAvailable)
    {
      lstrcpyn(status->reason, TEXT("Presentation Manager is unavailable on this Windows/driver configuration."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_HOST_PRESENTATION_MANAGER == resolved->host &&
        GRAPHICS_API_D3D11 != graphicsApi &&
        GRAPHICS_API_D3D12 != graphicsApi &&
        !magPresentationIsGdiFamily(graphicsApi))
    {
      lstrcpyn(status->reason, TEXT("The selected renderer does not expose an adapter-compatible native frame to the Presentation Manager bridge."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_HOST_DIRECTCOMPOSITION == resolved->host && !status->directCompositionAvailable)
    {
      lstrcpyn(status->reason, TEXT("DirectComposition is unavailable on this Windows configuration."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if ((GRAPHICS_API_D3D11 == graphicsApi || GRAPHICS_API_D3D12 == graphicsApi) &&
        MAG_HOST_DIRECTCOMPOSITION != resolved->host &&
        MAG_HOST_PRESENTATION_MANAGER != resolved->host &&
        MAG_HOST_REDIRECTED_HWND != resolved->host &&
        MAG_HOST_TRADITIONAL_LAYERED != resolved->host)
    {
      lstrcpyn(status->reason, TEXT("This Direct3D backend currently requires its DirectComposition presentation host."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (GRAPHICS_API_D3D11 != graphicsApi && GRAPHICS_API_D3D12 != graphicsApi &&
        !(magPresentationIsGdiFamily(graphicsApi) &&
          MAG_HOST_PRESENTATION_MANAGER == resolved->host) &&
        CAPTURE_API_DWM_THUMBNAIL != captureApi &&
        CAPTURE_API_DWM_PRIVATE_VISUAL != captureApi &&
        MAG_HOST_REDIRECTED_HWND != resolved->host &&
        MAG_HOST_TRADITIONAL_LAYERED != resolved->host)
    {
      lstrcpyn(status->reason, TEXT("The selected graphics backend currently supports only its redirected HWND host."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (noRedirection && MAG_HOST_DIRECTCOMPOSITION != resolved->host && MAG_HOST_PRESENTATION_MANAGER != resolved->host)
    {
      lstrcpyn(status->reason, TEXT("No-redirection requires DirectComposition or Presentation Manager content."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_LAYER_ALPHA_OPAQUE != resolved->alphaMode &&
        MAG_HOST_TRADITIONAL_LAYERED != resolved->host &&
        MAG_HOST_DIRECTCOMPOSITION != resolved->host &&
        MAG_HOST_PRESENTATION_MANAGER != resolved->host)
    {
      lstrcpyn(status->reason, TEXT("The selected alpha mode is unavailable until the host-specific premultiplied/constant/color-key path passes the flicker-free matrix."), ARRAYSIZE(status->reason));
      return FALSE;
    }

    if (MAG_PRESENT_AUTO == resolved->target)
    {
      resolved->target = magPresentationChooseAutoTarget(
        graphicsApi,
        resolved->host);
    }
    if (!magPresentationValidateTargetProfile(
          graphicsApi,
          captureApi,
          resolved,
          status))
    {
      return FALSE;
    }
    status->fullscreenHardwareCompositionCapable =
      status->hardwareCompositionSupportKnown &&
      0 != (status->hardwareCompositionSupport &
        DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_FULLSCREEN);
    status->windowedHardwareCompositionCapable =
      status->hardwareCompositionSupportKnown &&
      0 != (status->hardwareCompositionSupport &
        DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_WINDOWED);
    status->hardwareCompositionCursorStretchRisk =
      status->hardwareCompositionSupportKnown &&
      0 != (status->hardwareCompositionSupport &
        DXGI_HARDWARE_COMPOSITION_SUPPORT_FLAG_CURSOR_STRETCHED);
    status->directFlipGeometryEligible =
      magPresentationClientCoversOutput(hWnd, catalog, resolved);
    status->independentFlipEligible =
      magPresentationIsModernFlipTarget(resolved->target) &&
      MAG_LAYER_ALPHA_OPAQUE == resolved->alphaMode &&
      magAdapterLuidEqual(
        status->hardwareAdapterLuid,
        status->displayAdapterLuid) &&
      ((status->directFlipCapable &&
        status->independentFlipCapable &&
        status->directFlipGeometryEligible) ||
       (status->independentFlipCapable &&
        status->multiPlaneOverlayCapable &&
        status->maxRgbPlanes > 0 &&
        status->windowedHardwareCompositionCapable) ||
       MAG_HOST_PRESENTATION_MANAGER == resolved->host);
    {
      const BOOL waitableSwapChainSupported =
        (GRAPHICS_API_D3D11 == graphicsApi ||
         GRAPHICS_API_D3D12 == graphicsApi) &&
        MAG_HOST_PRESENTATION_MANAGER != resolved->host &&
        magPresentationIsModernFlipTarget(resolved->target);

      if (MAG_WAITABLE_SWAP_CHAIN_AUTO == resolved->waitableSwapChainMode)
      {
        resolved->waitableSwapChainMode =
          waitableSwapChainSupported && status->independentFlipEligible
            ? MAG_WAITABLE_SWAP_CHAIN_ENABLED
            : MAG_WAITABLE_SWAP_CHAIN_DISABLED;
      }
      else if (MAG_WAITABLE_SWAP_CHAIN_ENABLED ==
                 resolved->waitableSwapChainMode &&
               !waitableSwapChainSupported)
      {
        lstrcpyn(
          status->reason,
          TEXT("A waitable swap chain requires a D3D11/D3D12 flip-model DXGI presenter; Presentation Manager uses its own buffer-available event."),
          ARRAYSIZE(status->reason));
        return FALSE;
      }
    }
    status->observedTarget = MAG_PRESENT_AUTO;
    status->observedTargetValid = FALSE;
    status->strictTargetSatisfied =
      !requested->strictTarget || MAG_PRESENT_AUTO == requested->target;
    if (MAG_HOST_TRADITIONAL_LAYERED == resolved->host && noRedirection)
    {
      lstrcpyn(status->reason, TEXT("Traditional layered windows require the User32 layered-window surface."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_LAYER_ALPHA_COLOR_KEY == resolved->alphaMode &&
        (MAG_HOST_DIRECTCOMPOSITION == resolved->host ||
         MAG_HOST_PRESENTATION_MANAGER == resolved->host))
    {
      lstrcpyn(status->reason, TEXT("Composition color-key alpha requires the renderer key-to-alpha pass, which is unavailable for this frame route."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_LAYER_ALPHA_PER_PIXEL_PREMULTIPLIED == resolved->alphaMode &&
        MAG_HOST_TRADITIONAL_LAYERED != resolved->host &&
        MAG_HOST_DIRECTCOMPOSITION != resolved->host &&
        MAG_HOST_PRESENTATION_MANAGER != resolved->host)
    {
      lstrcpyn(status->reason, TEXT("Per-pixel alpha requires a layered or composition host."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_HOST_TRADITIONAL_LAYERED == resolved->host &&
        (CAPTURE_API_DWM_THUMBNAIL == captureApi || CAPTURE_API_DWM_PRIVATE_VISUAL == captureApi))
    {
      lstrcpyn(status->reason, TEXT("A compositor-owned DWM visual cannot be redirected through the User32 layered-window bitmap host."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_HOST_TRADITIONAL_LAYERED == resolved->host)
    {
      status->copyClass = MAG_COPY_CLASS_CPU_ROUND_TRIP;
    }
    else if (CAPTURE_API_DWM_THUMBNAIL == captureApi || CAPTURE_API_DWM_PRIVATE_VISUAL == captureApi)
    {
      status->copyClass = MAG_COPY_CLASS_COMPOSITOR_VISUAL;
    }
    else if (CAPTURE_API_GDI_BITBLT == captureApi)
    {
      status->copyClass = magPresentationIsGdiFamily(graphicsApi)
        ? MAG_COPY_CLASS_CPU_SOURCE
        : MAG_COPY_CLASS_CPU_ROUND_TRIP;
    }
    else
    {
      status->copyClass = MAG_COPY_CLASS_CPU_ROUND_TRIP;
    }

    if (MAG_COPY_STRICT_ZERO_COPY == resolved->copyRequirement &&
        MAG_COPY_CLASS_TRUE_ZERO_COPY != status->copyClass &&
        MAG_COPY_CLASS_COMPOSITOR_VISUAL != status->copyClass)
    {
      lstrcpyn(status->reason, TEXT("The active capture-to-present route crosses a CPU map/upload boundary and cannot satisfy strict zero-copy."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (MAG_COPY_ALLOW_GPU_LOCAL == resolved->copyRequirement &&
        (MAG_COPY_CLASS_CPU_SOURCE == status->copyClass ||
         MAG_COPY_CLASS_CPU_ROUND_TRIP == status->copyClass))
    {
      lstrcpyn(status->reason, TEXT("This route requires CPU-owned pixels; the selected copy policy allows GPU-local copies only."), ARRAYSIZE(status->reason));
      return FALSE;
    }
    if (!magAdapterLuidEqual(status->hardwareAdapterLuid, status->displayAdapterLuid) &&
        MAG_COPY_STRICT_ZERO_COPY == resolved->copyRequirement)
    {
      lstrcpyn(status->reason, TEXT("The selected Hardware Adapter differs from the Display Adapter; no validated cross-adapter zero-copy route is active."), ARRAYSIZE(status->reason));
      return FALSE;
    }

    status->configurationSupported = TRUE;
    status->flickerFree = TRUE;
    if (MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP == resolved->target &&
        status->displayPlaneCapsKnown &&
        (!status->multiPlaneOverlayCapable ||
         !status->maxRgbPlanes))
    {
      lstrcpyn(
        status->reason,
        TEXT("The MPO-eligible presenter is configured, but the selected VidPN source reports no usable RGB overlay plane; Windows will compose until a hardware plane becomes available."),
        ARRAYSIZE(status->reason));
    }
    else if (MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP == resolved->target &&
             !status->independentFlipEligible)
    {
      if (!magAdapterLuidEqual(
            status->hardwareAdapterLuid,
            status->displayAdapterLuid))
      {
        lstrcpyn(
          status->reason,
          TEXT("The flip-model presenter is configured, but the render and display adapters differ; Independent Flip is not eligible without a validated direct cross-adapter scanout path."),
          ARRAYSIZE(status->reason));
      }
      else if (MAG_LAYER_ALPHA_OPAQUE != resolved->alphaMode)
      {
        lstrcpyn(
          status->reason,
          TEXT("The flip-model presenter is configured, but this alpha mode requires composition; select Opaque to make DirectFlip or Independent Flip eligible."),
          ARRAYSIZE(status->reason));
      }
      else if (status->displayPlaneCapsKnown &&
               !status->independentFlipCapable)
      {
        lstrcpyn(
          status->reason,
          TEXT("The flip-model presenter is configured, but the selected VidPN source reports no Independent Flip support."),
          ARRAYSIZE(status->reason));
      }
      else if (!status->directFlipGeometryEligible &&
               (!status->multiPlaneOverlayCapable ||
                !status->maxRgbPlanes))
      {
        lstrcpyn(
          status->reason,
          TEXT("The flip-model presenter is configured, but the client does not cover the output and no usable RGB MPO plane is reported; Windows will remain in Composed Flip."),
          ARRAYSIZE(status->reason));
      }
      else
      {
        lstrcpyn(
          status->reason,
          TEXT("The flip-model presenter is configured and plane-eligible; Windows decides the active mode dynamically from occlusion, scaling, and display state."),
          ARRAYSIZE(status->reason));
      }
    }
    else
    {
      lstrcpyn(
        status->reason,
        TEXT("Configuration passed the presenter, display-plane, copy, pacing, and resize-capacity gates."),
        ARRAYSIZE(status->reason));
    }
    return TRUE;
}

LPCTSTR magCopyClassName(MAGCOPYCLASS copyClass)
{
    switch (copyClass)
    {
    case MAG_COPY_CLASS_TRUE_ZERO_COPY:
      return TEXT("True zero-copy");
    case MAG_COPY_CLASS_COMPOSITOR_VISUAL:
      return TEXT("Compositor visual");
    case MAG_COPY_CLASS_GPU_LOCAL:
      return TEXT("GPU-local copy");
    case MAG_COPY_CLASS_CPU_SOURCE:
      return TEXT("CPU source");
    case MAG_COPY_CLASS_CPU_ROUND_TRIP:
      return TEXT("CPU round trip");
    default:
      return TEXT("Unknown");
    }
}
