# MAG graphics, presentation, composition, and zero-copy parity plan

Status: implementation contract approved by user; implementation follows the planning checkpoint commit

Revised: 2026-08-20

This plan supersedes the earlier graphics-only parity plan. The earlier work established selectable OpenGL, GDI, Direct3D 9, Direct3D 11, Direct3D 12, Vulkan, Direct2D UI, and three text paths, but it did not establish presentation-model parity, surface-ownership parity, honest zero-copy routing, or visible zero-flicker resize. Those are part of the definition of done here.

## Non-negotiable outcome

MAG will expose rendering API, capture API, UI API, text API, presentation target, surface ownership, and composition host as separate choices. It will select the fastest valid route for `Auto`, make a strict zero-copy route directly selectable when the machine can support it, and report why a requested route is unavailable instead of silently substituting a slower one.

The implementation is not complete merely because it builds, returns successful HRESULTs, increments present counters, or passes a headless smoke test. Completion requires observing the real MAG window while it is moved, resized from every edge and corner, switched between modes, and exercised on the relevant display paths.

The following rules apply throughout:

- The captured image remains a native GPU resource or compositor visual for as long as the selected APIs permit. CPU readback and CPU re-upload are forbidden in strict zero-copy mode.
- A GPU-local copy is identified as a copy; it is never called zero-copy. A compositor-owned visual pass-through is identified separately because MAG never obtains the pixels.
- `Auto` chooses the highest-performance supported route. It never chooses a traditional per-pixel layered-window round trip, a legacy BitBlt presentation path, WARP, or a CPU staging path while a hardware native route is usable.
- Explicit legacy and diagnostic presentation targets remain available because full presentation-model parity requires them, but the UI labels their copy class and performance cost.
- Requested presentation model and observed presentation model are different fields. Independent Flip, Hardware Composed Independent Flip, and multi-plane overlay promotion are system outcomes, not modes an application can force with one API call.
- Presentation is orthogonal to rendering: `renderer -> native frame -> presenter -> host`. Every target profile is implemented for every renderer through a native shared-resource bridge where the APIs permit one, or through an explicitly classified GPU/CPU transfer where that target inherently requires it. A target must not remain a renderer-specific placeholder or be rejected merely because the selected renderer is not Direct3D 11.
- Applying a setting is transactional. The old visible host and resources remain usable until the candidate has rendered and submitted a first frame.
- A failed candidate leaves the old configuration active and leaves persisted settings unchanged.
- Zero flicker is the minimum viable behavior for every selectable combination. A route that cannot preserve continuous, geometry-matched content during resize is unavailable and explains why; it is never exposed as a working setting with a known flickering fallback.
- The zero-flicker resize gate is a visual gate. Existing resize code is treated as defective until that gate passes.

## Sources read and the contracts taken from them

### Presentation taxonomy and modern DXGI behavior

- [Special K Presentation Model](https://wiki.special-k.info/Presentation_Model) supplies the user-facing taxonomy and the relationship among swap effect, window mode, Fullscreen Optimizations, DirectFlip, independent flip, and multi-plane overlays.
- [PresentMon console documentation](https://github.com/GameTechDev/PresentMon/blob/main/README-ConsoleApplication.md) supplies the exact seven observed present-mode labels used in the UI and telemetry.
- [DXGI_SWAP_EFFECT](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/ne-dxgi-dxgi_swap_effect) defines the BitBlt and flip swap effects and the restriction that Direct3D 12 supports flip sequential and flip discard only.
- [For best performance, use DXGI flip model](https://learn.microsoft.com/en-us/windows/win32/direct3ddxgi/for-best-performance--use-dxgi-flip-model) defines DirectFlip, panel-fitter scaling, multi-plane-overlay eligibility, Independent Flip, tearing, frame-latency wait handles, and the conditions under which the compositor can be bypassed.
- [Windows 11 Composition Swapchain API](https://learn.microsoft.com/en-us/windows/win32/comp_swapchain/comp-swapchain) defines `IPresentationFactory`/`IPresentationManager`, presentation surfaces backed by displayable D3D11 textures, target-time scheduling, buffer retirement fences, atomic presentation, and presentation statistics. This API was missing from the earlier plan and is the preferred presenter when it is available and compatible.
- [D3D11 displayable surfaces](https://learn.microsoft.com/en-us/windows/win32/direct3d11/displayable-surfaces) defines `D3D11_RESOURCE_MISC_SHARED_DISPLAYABLE`, which is required by the Composition Swapchain path.

The exact observed values are:

1. `Hardware: Legacy Flip`
2. `Hardware: Legacy Copy to front buffer`
3. `Hardware: Independent Flip`
4. `Composed: Flip`
5. `Hardware Composed: Independent Flip`
6. `Composed: Copy with GPU GDI`
7. `Composed: Copy with CPU GDI`

MAG will expose all seven as strict target profiles plus `Auto`. A target profile configures all controllable prerequisites and then verifies the observed result. If Windows selects a different result, strict Apply fails with an explanation; an optional non-strict experiment can remain active while showing requested and observed values side by side.

### The three requested layering and Direct2D articles

#### 2009: traditional layered windows and their unavoidable copy boundary

[Layered Windows with Direct2D (December 2009)](https://learn.microsoft.com/en-us/archive/msdn-magazine/2009/december/windows-with-c-layered-windows-with-direct2d) was read as an implementation source, not merely linked.

Its requirements for MAG are:

- Support both whole-window alpha/color-key layering through `SetLayeredWindowAttributes` and per-pixel premultiplied alpha through `UpdateLayeredWindowIndirect`.
- Represent the per-pixel source as top-down 32-bpp premultiplied BGRA, use a positive stride, and expose `SourceConstantAlpha` independently from each pixel's alpha.
- Treat `UpdateLayeredWindowIndirect` as the presentation operation for that host rather than pretending that `WM_PAINT` presents its contents.
- Preserve alpha-based User32 hit testing for the traditional per-pixel layered host.
- State the architectural cost honestly: User32 needs a system-memory image for per-pixel hit testing. Hardware rendering therefore incurs GPU-to-CPU synchronization/readback followed by the DWM upload. This route can never satisfy strict zero-copy.
- Do not choose a Direct2D DC render target for a new software route because it adds an internal WIC-to-DC copy. The best traditional software route is a WIC bitmap rendered directly by Direct2D, then exposed through GDI interop for the layered update.
- For the explicit hardware traditional-layered experiment, render to a GDI-compatible BGRA DXGI surface and acquire/release its DC correctly. It remains a CPU round-trip presentation class even though rendering itself is accelerated.

#### 2013: Direct2D 1.1 must target the same DXGI image, not a detached CPU overlay

[Introducing Direct2D 1.1 (May 2013)](https://learn.microsoft.com/en-us/archive/msdn-magazine/2013/may/windows-with-c-introducing-direct2d-1-1) was read as the Direct2D/DXGI interop contract.

Its requirements for MAG are:

- Create Direct3D 11 devices with `D3D11_CREATE_DEVICE_BGRA_SUPPORT`, obtain `IDXGIDevice`, and create `ID2D1Device` plus `ID2D1DeviceContext` from that device.
- Use an `ID2D1DeviceContext`, not `ID2D1HwndRenderTarget`, for modern UI rendering.
- Obtain the presentable image as an `IDXGISurface`, wrap it with `ID2D1Bitmap1`, use `D2D1_BITMAP_OPTIONS_TARGET` (and `CANNOT_DRAW` when appropriate), and call `SetTarget` so Direct2D draws into the same GPU image.
- Keep drawing completion (`EndDraw`) separate from presentation (`Present` or Presentation Manager submission).
- Release every Direct2D reference to a swap-chain buffer before `ResizeBuffers`; device loss and occlusion have explicit state transitions.
- Use BGRA8 and the correct alpha mode. Opaque HWND targets use ignored alpha; DirectComposition and layered targets use premultiplied alpha.

This replaces the earlier CPU-bitmap-first Direct2D overlay design on every native GPU route. A CPU/WIC overlay remains only for an explicitly CPU-owned GDI or traditional layered route.

#### 2014: high-performance GPU layering through no-redirection and DirectComposition

[High-Performance Window Layering Using the Windows Composition Engine (June 2014)](https://learn.microsoft.com/en-us/archive/msdn-magazine/2014/june/windows-with-c-high-performance-window-layering-using-the-windows-composition-engine) was read as the high-performance layered-host contract.

Its requirements for MAG are:

- Create the visible host with `WS_EX_NOREDIRECTIONBITMAP` so Windows does not allocate an opaque redirection surface that MAG does not use.
- Create a composition swap chain with premultiplied BGRA buffers and flip model, attach it as DirectComposition visual content, set the visual as the target root, and commit the visual tree.
- Keep the texture entirely on the GPU. This avoids the GPU-to-CPU-to-GPU trip of traditional per-pixel layered windows.
- Treat DirectComposition tree `Commit` and swap-chain `Present` as distinct operations. A static tree is not recommitted every frame.
- Expose the tradeoff: unlike traditional per-pixel layered windows, this route gives the HWND uniform hit testing. Pixel-alpha hit testing requires an explicit MAG hit-test mask/region policy and is not claimed to be User32's native per-pixel behavior.
- Use the visual tree for independent content, UI, outline, clip, transform, opacity, effect, and animation ownership rather than flattening all layers in CPU memory.

### Surface ownership, layering, and DirectComposition reference documentation

- [Extended Window Styles](https://learn.microsoft.com/en-us/windows/win32/winmsg/extended-window-styles) defines `WS_EX_NOREDIRECTIONBITMAP`, `WS_EX_LAYERED`, and the incompatibility between `WS_EX_LAYERED` and classes registered with `CS_OWNDC` or `CS_CLASSDC`.
- [Layered Windows](https://learn.microsoft.com/en-us/windows/win32/winmsg/window-features#layered-windows), [UpdateLayeredWindow](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-updatelayeredwindow), and [SetLayeredWindowAttributes](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setlayeredwindowattributes) define global alpha, color key, premultiplied per-pixel alpha, hit testing, and the rule that `UpdateLayeredWindow` fails after `SetLayeredWindowAttributes` until the layered style is reset.
- [DirectComposition effects](https://learn.microsoft.com/en-us/windows/win32/directcomp/effects), [animations](https://learn.microsoft.com/en-us/windows/win32/directcomp/animation), [clipping](https://learn.microsoft.com/en-us/windows/win32/directcomp/clipping), and [bitmap surfaces](https://learn.microsoft.com/en-us/windows/win32/directcomp/bitmap-surfaces) define the feature set that the DirectComposition host must expose.
- [Surface sharing between Windows graphics APIs](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/surface-sharing-between-windows-graphics-apis) and [IDXGISurface1::GetDC](https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgisurface1-getdc) define shared-handle and GDI-compatible-surface interoperability.

## Current implementation audit

The repository already contains backend files for GDI, OpenGL, Direct3D 9, Direct3D 11, Direct3D 12, and Vulkan plus DirectComposition and UI helpers. That is a useful baseline, not presentation parity.

The material gaps found in the current code are:

- GDI, DXGI Desktop Duplication, and Windows Graphics Capture are flattened into a CPU BGRA frame for the common path. D3D11 capture maps a staging texture; WGC maps a staging texture. Every hardware presenter then uploads that CPU image again.
- OpenGL uses `glTexSubImage2D`; D3D9 updates a texture; D3D11 calls `UpdateSubresource` and copies; D3D12 uses an upload resource; Vulkan uses a staging buffer. None is zero-copy from native DXGI/WGC capture.
- The current Direct3D 11 HWND swap chain uses legacy `DXGI_SWAP_EFFECT_DISCARD`, so it cannot provide modern flip-model parity.
- The Direct3D 12 composition swap chain is flip discard, but its frame is still assembled in CPU memory and uploaded.
- The DirectComposition helper is a presenter attachment, not a complete visual-tree, layering, effects, synchronization, or host-ownership implementation.
- The settings dialog exposes graphics, capture, UI, and text selections only. It has no presentation target, surface ownership, composition host, alpha/hit-test, zero-copy, observed-mode, or capability controls.
- The visible window class includes `CS_OWNDC`. That is incompatible with `WS_EX_LAYERED` and makes a single permanent HWND/class unsuitable for all required hosts.
- `mag_UpdateViewWindowStyle` unconditionally removes `WS_EX_LAYERED`, preventing layered parity.
- `mag_OnNCCalcSize` returns `WVR_VALIDRECTS` without filling the required source and destination valid rectangles. Windows is therefore allowed to preserve the wrong pixels during resize.
- `WM_SIZE` immediately reallocates the CPU frame, calls backend resize/`ResizeBuffers`, and destroys the previous-size presentation state while the interactive size transaction is still underway.
- Vblank-driven `WM_MAG_RENDER` work can interleave with `WM_NCCALCSIZE`, `WM_SIZE`, and `WM_WINDOWPOSCHANGED`; there is no geometry epoch excluding future-size content from an old-size host.
- Present bookkeeping records a sampled client rectangle, not the geometry and buffer that DWM actually displayed. `DwmFlush` is a compositor barrier, not proof that window geometry, DirectComposition commits, and heterogeneous present queues changed atomically.

The resize implementation is therefore reopened. The earlier automated old/new-size counters are not accepted as evidence of zero flicker.

## Independent settings and persisted model

The current `GRAPHICSAPI`, capture API, `UIGRAPHICSAPI`, and `TEXTRENDERER` selections remain independent. Add these authoritative enums and generate both registry serialization and settings-combo contents from their registries:

```c
typedef enum MAGPRESENTATIONTARGET {
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

typedef enum MAGSURFACEOWNERSHIP {
  MAG_SURFACE_AUTO = 0,
  MAG_SURFACE_REDIRECTION,
  MAG_SURFACE_NO_REDIRECTION,
  MAG_SURFACE_COUNT
} MAGSURFACEOWNERSHIP;

typedef enum MAGCOMPOSITIONHOST {
  MAG_HOST_AUTO = 0,
  MAG_HOST_REDIRECTED_HWND,
  MAG_HOST_TRADITIONAL_LAYERED,
  MAG_HOST_DIRECTCOMPOSITION,
  MAG_HOST_PRESENTATION_MANAGER,
  MAG_HOST_COUNT
} MAGCOMPOSITIONHOST;

typedef enum MAGCOPYREQUIREMENT {
  MAG_COPY_AUTO_FASTEST = 0,
  MAG_COPY_STRICT_ZERO_COPY,
  MAG_COPY_ALLOW_GPU_LOCAL,
  MAG_COPY_ALLOW_CPU_ROUND_TRIP,
  MAG_COPY_COUNT
} MAGCOPYREQUIREMENT;

typedef enum MAGLAYEREDALPHAMODE {
  MAG_ALPHA_OPAQUE = 0,
  MAG_ALPHA_CONSTANT,
  MAG_ALPHA_COLOR_KEY,
  MAG_ALPHA_PER_PIXEL_PREMULTIPLIED,
  MAG_ALPHA_COUNT
} MAGLAYEREDALPHAMODE;
```

Persist requested values separately from runtime facts. Add a versioned settings record and migrate old installations to `Auto` without changing their selected graphics/capture/UI/text APIs.

The settings property sheet will have these pages:

- **Rendering**: capture API, graphics API, Display Adapter, Hardware Adapter, UI API, text API.
- **Presentation**: presentation target, host, redirection/no-redirection, swap effect profile, buffer count, maximum frame latency, sync interval, tearing, fullscreen/borderless eligibility, and strict-result toggle.
- **Layering**: alpha mode, constant opacity, color key, per-pixel alpha source, hit-test policy, DirectComposition visual/effect/animation options, clip, transform, and overlay ownership.
- **Performance**: Auto fastest, strict zero-copy, allow one GPU-local copy, or explicit CPU diagnostic routes. Show resource format, adapter LUID, synchronization primitive, and measured copy class.
- **Status**: requested and observed presentation modes, DirectFlip/MPO capabilities, active host/style/class, queue depth, dropped/late frames, device/adapter, last failure, and why every unavailable choice is unavailable.

Every page is keyboard accessible, has labels associated with controls, exposes capability reasons without relying on color, and supports restoring a known-good configuration.

### Display Adapter and Hardware Adapter settings

MAG exposes two independent adapter selectors because the adapter that owns the visible output is not necessarily the adapter that should allocate/render the image:

- **Display Adapter** selects the DXGI adapter/output that owns presentation. Its values are `Auto (window/output)`, `Follow captured display`, and every active display target grouped under its physical adapter. Each entry shows friendly adapter name, output name, desktop coordinates, refresh rate, color/HDR state, and whether it currently owns the MAG window.
- **Hardware Adapter** selects the physical GPU used to create capture, render, UI, sharing, and presentable resources. Its values are `Auto (highest performance compatible)`, `Same as Display Adapter`, `Same as Capture Adapter`, and every enumerated hardware adapter. WARP/software rasterizers are visibly separate fallback choices and are never presented as hardware adapters.

Adapters and outputs are persisted by stable identity, not transient enumeration index: DXGI adapter LUID plus output/monitor identity and display-config target path. On startup, hot-plug, GPU reset, docking, or topology change, the resolver re-enumerates and either rebinds the same identity or reports the saved selection as disconnected; it does not silently bind ordinal `0` to a different GPU.

`IDXGIFactory6::EnumAdapterByGpuPreference` supplies the Auto hardware preference, followed by actual API/feature/format/sharing/presentation capability checks. D3D11 and D3D12 use the selected DXGI adapter directly. D3D9/9Ex resolves the matching adapter/monitor. Vulkan matches the DXGI LUID through Vulkan device-ID properties and requires the selected physical device's presentation and external-memory capabilities. OpenGL uses a matching GPU-affinity/render-GPU extension only where the installed ICD exposes one; otherwise an explicit non-default hardware choice is unavailable with a reason. GDI binds the selected display/output DC but remains CPU-classified.

The capability solver tracks at least four identities for every candidate: capture adapter, hardware/render adapter, display/presentation adapter, and compositor owner. `Same as Capture Adapter` is preferred for direct sampling; `Same as Display Adapter` is preferred for direct scanout/presentation. Auto scores the whole route and favors true zero-copy first, then one same-GPU copy, and rejects implicit cross-adapter CPU staging. If the user's explicit Display Adapter and Hardware Adapter choices require a cross-adapter transfer, Status shows both LUIDs and the required transfer class. Strict zero-copy disables Apply unless a genuine cross-adapter shared-resource route exists and passes synchronization/capability validation.

## Presentation target profiles

The target is a solver input, not a false promise. Each profile owns the controllable configuration below and has an observation predicate.

| Target | Candidate configuration | Successful observation |
| --- | --- | --- |
| Auto | Presentation Manager when supported; otherwise DirectComposition or flip-discard HWND; hardware adapter; waitable pacing; zero-copy or lowest GPU-copy route | Fastest stable observed mode available for the current geometry |
| Hardware: Legacy Flip | D3D9/D3D9Ex or legacy exclusive-fullscreen-capable path with the required flip semantics | Exact PresentMon observation |
| Hardware: Legacy Copy to front buffer | Explicit legacy diagnostic presenter only; never selected by Auto | Exact PresentMon observation |
| Hardware: Independent Flip | HWND flip-discard/sequential profile, eligible client/display coverage, no incompatible alpha/composition features | Exact observation after a stability window, not one sample |
| Composed: Flip | Flip-model HWND or composition swap chain whose content is composed | Exact observation after a stability window |
| Hardware Composed: Independent Flip | Flip model plus DirectFlip/MPO-eligible format, size, scaling, opacity, and overlay configuration | Exact observation including the hardware-composed classification |
| Composed: Copy with GPU GDI | Explicit redirected BitBlt/GDI-compatible GPU path | Exact observation |
| Composed: Copy with CPU GDI | Explicit CPU DIB/GDI diagnostic path | Exact observation |

The table applies across GDI, OpenGL, D3D9/9Ex, D3D11, D3D12, and Vulkan renderers. The presenter, not the renderer enum, owns the target model. The resolver may reject a product only for a measured machine/API capability or an unavoidable copy-policy conflict, and the reason must identify that failed prerequisite rather than say the target is unimplemented for that renderer.

`IDXGIOutput6::CheckHardwareCompositionSupport` is used as a capability hint, not as proof of the active mode. Classic paths get a minimal process-scoped PresentMon-compatible ETW observer. Presentation Manager paths also consume composition-swapchain presentation statistics and buffer-retirement information. Observation shuts down cleanly and never installs a machine-wide service.

## Surface and host parity

### Redirection surface

The redirected HWND host lets DWM own the standard redirection surface. It supports normal GDI painting, legacy BitBlt paths, and regular HWND swap chains. It is the compatibility host, not the default when a no-redirection composition path is faster.

### No-redirection surface

The no-redirection host is created with `WS_EX_NOREDIRECTIONBITMAP`; MAG supplies the visible content through DirectComposition or Presentation Manager. The style is treated as an ownership boundary, not a late optimization bit. If content attachment fails, the hidden candidate is destroyed before it can become visible.

### Host classes and HWND recreation

One permanent visible class cannot satisfy the matrix because `CS_OWNDC` conflicts with layered windows. Split process-lifetime application state from a recreatable `MAGWINDOWHOST`:

- a redirected class with only the styles its presenter needs;
- a no-redirection/composition class without `CS_OWNDC` or `CS_CLASSDC`;
- a traditional layered class without those DC class styles;
- a message-only/helper WGL owner with `CS_OWNDC` when OpenGL needs a stable context.

Changing surface ownership, traditional layering, or an incompatible presenter creates a candidate HWND of the correct class. Geometry, owner, z-order, visibility, input policy, accessibility identity, and focus are transferred transactionally. Application state and hotkeys do not restart.

## Layered-window feature parity

Traditional layered composition implements all relevant User32 modes:

- constant alpha and color key with `SetLayeredWindowAttributes`;
- per-pixel premultiplied BGRA plus global `SourceConstantAlpha` with `UpdateLayeredWindowIndirect`;
- `ULW_ALPHA`, `ULW_COLORKEY`, and `ULW_OPAQUE` where semantically valid;
- `ULW_EX_NORESIZE` validation where supported;
- top-down DIB/WIC storage and correct blend function;
- alpha-based native hit testing, optional whole-window pass-through, and explicit custom hit-test policy;
- the required style reset when switching from `SetLayeredWindowAttributes` back to `UpdateLayeredWindow` semantics;
- atomic content/position/size updates during movement and resize.

This host is tagged `CPU_ROUND_TRIP` for per-pixel content. Strict zero-copy disables it with the reason from the 2009 architecture contract.

The high-performance layered equivalent uses no-redirection plus DirectComposition/Presentation Manager:

- premultiplied BGRA displayable images;
- independent content, magnifier image, UI, outline, and debug/status visuals;
- visual offset, 2D/3D transform, clip, opacity, interpolation, and bitmap content;
- effect-group support for the effects MAG exposes;
- time-based DirectComposition animation for properties that benefit from compositor scheduling;
- surfaces and virtual surfaces with dirty-rectangle updates where appropriate;
- `BeginDraw`/`EndDraw` update-offset handling for composition surfaces;
- explicit tree-commit generation; no redundant per-frame `Commit` when only content presents;
- uniform HWND hit testing plus an optional MAG-owned alpha-mask/region policy. The UI clearly distinguishes this from native layered-window alpha hit testing.

## Native image and copy contracts

Replace the CPU-frame-as-authority model with a plane-based native frame:

```c
typedef enum MAGIMAGEKIND {
  MAG_IMAGE_CPU_BGRA,
  MAG_IMAGE_D3D11_TEXTURE,
  MAG_IMAGE_D3D12_RESOURCE,
  MAG_IMAGE_DXGI_SHARED_HANDLE,
  MAG_IMAGE_VULKAN_IMAGE,
  MAG_IMAGE_OPENGL_TEXTURE,
  MAG_IMAGE_DCOMP_VISUAL
} MAGIMAGEKIND;

typedef struct MAGNATIVEIMAGE {
  MAGIMAGEKIND kind;
  LUID adapterLuid;
  UINT width;
  UINT height;
  DXGI_FORMAT format;
  MAGALPHAMODE alphaMode;
  MAGCOPYCLASS copyClass;
  HANDLE sharedHandle;
  UINT64 acquireValue;
  UINT64 releaseValue;
  void* object;
} MAGNATIVEIMAGE;
```

A `MAGFRAME` contains one plane per captured output plus source/destination rectangles and synchronization. Multi-monitor capture is not flattened on the CPU. The renderer samples the planes in screen coordinates and the presentation host owns the final presentable image pool.

The measured copy classes are:

- `TRUE_ZERO_COPY`: the producer image is sampled/attached directly and never copied or mapped by MAG;
- `COMPOSITOR_VISUAL`: DWM owns the source visual and MAG transforms/clips that visual without pixel access;
- `GPU_LOCAL_COPY`: one or more GPU copy/resolve operations, no CPU readback;
- `CPU_SOURCE`: source is natively CPU-owned, as with GDI capture;
- `CPU_ROUND_TRIP`: a GPU result is mapped/read back and uploaded/passed back to the compositor.

Strict zero-copy installs instrumentation at every map/readback/upload boundary. `Map` on staging capture resources, `GetDC` for GPU output, DIB presentation, `glTex(Sub)Image`, D3D upload heaps for captured pixels, Vulkan host staging, and equivalent paths fail the strict route rather than silently continuing.

## Capture-to-render routes

- **DWM Thumbnail / DWM Private Visual**: compositor-visual pass-through. Apply pan, zoom, clip, and overlay through MAG-owned wrapper visuals. Do not flatten the captured visual or mutate the shared DWM source visual.
- **Windows Graphics Capture -> D3D11/D2D/DirectComposition**: retain the `ID3D11Texture2D`, use the same adapter/device when possible, and sample or attach it directly with explicit frame-pool lifetime and synchronization.
- **Desktop Duplication -> D3D11/D2D/DirectComposition**: retain acquired D3D11 textures until the consuming frame retires; sample them directly on the owning adapter.
- **D3D12**: import a compatible shared D3D11/DXGI resource when the producer exposes one. Otherwise use a GPU-local bridge and classify it accordingly. D3D11On12 is used for D2D/UI rendering into D3D12-owned resources, not as a pretext to call an arbitrary D3D11 capture zero-copy.
- **Vulkan**: use Win32 external-memory and external-semaphore interop only when the D3D producer resource is shareable and adapter-compatible. Otherwise use a GPU-local shared bridge; strict zero-copy is unavailable for that pair.
- **OpenGL**: use `WGL_NV_DX_interop2` for direct D3D texture access only when the driver and resource support it. Otherwise use the fastest explicit GPU bridge; CPU texture upload is an opt-in diagnostic route.
- **D3D9Ex**: use compatible shared surfaces where supported. Otherwise use a GPU-local conversion/copy; traditional D3D9 upload is a diagnostic fallback.
- **GDI capture or presentation**: classify it as CPU-owned or CPU round-trip. It can satisfy pixel and legacy-present parity but not strict zero-copy.

Cross-adapter capture is never called zero-copy. The solver prefers a renderer/presenter on the capture adapter; if display ownership forces another adapter, it reports the cross-adapter transfer class and offers only routes that synchronize correctly.

## Graphics-backend parity

Every graphics backend consumes the same frame planes, transform, point-sampling rule, alpha contract, UI command list, color space, and presentation-host interface.

### GDI

Retain the deterministic CPU oracle, redirected BitBlt/paint presentation, and both GPU-GDI and CPU-GDI diagnostic target profiles. GDI is never an Auto hardware fallback unless no hardware route can initialize and the user permits CPU fallback.

### OpenGL/WGL

Keep the WGL context on its own `CS_OWNDC` helper when the visible host cannot use that class style. Add direct DXGI texture interop when available, explicit swap-interval control, and a host capability table. Do not claim flip/independent-flip support unless ETW observes it.

### Direct3D 9/9Ex

Use Direct3D 9Ex where present, including `D3DSWAPEFFECT_FLIPEX` eligibility, and preserve the classic device-lost/reset route for Direct3D 9. Implement hardware, mixed, and software vertex-processing fallback reporting without silently changing the selected API. Cover legacy flip/copy profiles and shared-surface interop honestly.

### Direct3D 11

Replace the legacy discard-only design with separately creatable BitBlt and flip-model presenters. Use BGRA support, hardware adapter selection, waitable-object frame latency, tearing where valid, occlusion handling, adapter migration, and complete device-removal diagnostics. D3D11 is the primary native interop hub for WGC, Desktop Duplication, Direct2D 1.1, DirectComposition, and displayable surfaces.

### Direct3D 12

Use flip-model composition/HWND presenters only, per-buffer allocators and fences, a bounded frame pool, waitable pacing, tearing where valid, and complete device-removed diagnostics. Captured native resources remain GPU-resident; upload heaps are reserved for CPU-owned sources and UI data. Use D3D11On12 for Direct2D/DirectWrite rendering into D3D12-owned presentable resources.

### Vulkan

Retain dynamic loader discovery, choose a surface-compatible adapter/queue, add a real sampled-image render path, external-memory/semaphore interop, color/alpha correctness, FIFO/mailbox/immediate mapping where supported, out-of-date/suboptimal rebuild, and device-loss diagnostics. A host staging upload is not the normal native capture route.

### WARP and software adapters

WARP remains an explicit adapter/fallback choice inside D3D11/D3D12, visibly reported. Auto never selects it while a functional hardware device exists. It does not create another `GRAPHICSAPI` value.

## Direct2D UI and text parity

Direct2D is a UI renderer, DirectWrite is a text layout/rasterization API, and neither is mislabeled as a display presentation model.

For D3D11, DirectComposition, and Presentation Manager, Direct2D 1.1 targets the same DXGI image or an independent GPU overlay visual. For D3D12 it targets D3D12-owned resources through D3D11On12. For Vulkan/OpenGL, the backend renders the shared neutral draw list natively or consumes a GPU overlay through supported interop; it does not force a CPU overlay in strict zero-copy mode. GDI and traditional layered paths use the documented WIC/GDI route.

Text settings retain full visible parity:

- **DirectWrite**: shared shaping/layout metrics rendered by Direct2D to the native target;
- **GPU glyph atlas**: DirectWrite-rasterized cached glyphs rendered as native quads by OpenGL, D3D9, D3D11, D3D12, and Vulkan;
- **GDI text**: GDI compatibility baseline on CPU-owned targets.

All paths share the Unicode string, locale, font family, weight/style/stretch, em size, DPI, measuring mode, alignment rectangle, baseline, clipping, and color. Pixel comparisons allow documented rasterizer tolerances while geometry and baseline comparisons are exact.

## Transactional configuration and recovery

Applying settings performs these phases:

1. Resolve the complete candidate against capture API, adapters, format/alpha, renderer, UI/text, host, surface ownership, presentation target, and copy requirement.
2. Produce a capability report before mutating live state.
3. Create a hidden candidate HWND of the required class plus candidate devices, image pool, visual tree/swap chain, observer, and frame pacing.
4. Acquire/render/submit a first candidate frame and verify its synchronization and requested/observed predicate where possible.
5. Transfer geometry, owner, z-order, accessibility state, input policy, focus, and visibility in one UI-thread transaction.
6. Publish the candidate generation atomically and persist the requested settings.
7. Retire old capture images, swap-chain buffers, visuals, HWND, and devices only after their fences/statistics prove they are no longer in use.

On device loss, rebuild the same route once. A fallback is allowed only by the user's selected copy/performance policy and is always surfaced in Status. A strict route never rewrites the stored request to a fallback.

## Zero-flicker interactive resize

The BorderlessWindow32 reference was studied, especially:

`C:\Users\Steph2\Desktop\startup\shamboni2\opus\BorderlessWindow32\immersivewindowdemo-zero-flicker.diff`

Its useful principle is to bind content generations to window-geometry generations and not expose future-size content before the corresponding geometry commit. MAG cannot copy it mechanically because MAG has multiple capture producers and heterogeneous presentation queues.

The replacement resize protocol is:

1. Introduce monotonic geometry epochs containing proposed window rect, committed client rect, content size, buffer identity, present/commit token, and retirement token.
2. On `WM_ENTERSIZEMOVE`, enter live-resize mode and reserve enough presentable images for the active host. Keep the last committed frame visible.
3. On `WM_NCCALCSIZE`, compute the borderless client geometry. Return `0` unless MAG fills both `rgrc[1]` and `rgrc[2]` with a genuinely valid preservation mapping; remove the current invalid `WVR_VALIDRECTS` use.
4. Do not call `ResizeBuffers`, destroy the current render target, or reallocate the sole CPU frame directly from `WM_SIZE` during a drag. Record the proposed epoch and render into a candidate image sized for it.
5. Exclude ordinary vblank `WM_MAG_RENDER` submissions while a geometry transaction is between NCCALCSIZE and WINDOWPOSCHANGED. Render work carries an epoch and stale/future epochs are rejected at publication.
6. On `WM_WINDOWPOSCHANGED`, commit the matching geometry and publish only the matching image. The previous image stays alive until DWM/presenter retirement proves it is no longer scanned/composed.
7. Traditional layered hosts use one `UpdateLayeredWindowIndirect` call to update content, size, and position atomically.
8. DirectComposition/Presentation Manager hosts stage visual offset/clip/transform and content for the same generation, commit the tree only when it changes, and use presentation statistics/retirement fences rather than `DwmFlush` as the ownership proof.
9. HWND flip presenters use a live-resize reservoir and defer destructive `ResizeBuffers` until a safe boundary. If a temporary composition bridge is needed to retain pixels, it is app-owned and removed after the new chain has presented.
10. On `WM_EXITSIZEMOVE`, coalesce to the final size, retire surplus images after their tokens complete, and return to normal pacing.

`WM_EXITSIZEMOVE` is never the first point at which new visuals become visible. Every committed `WM_WINDOWPOSCHANGED` publishes a fresh frame for that exact client geometry during the modal sizing loop. Ordinary live-resize frames use a nonblocking latest-frame policy; late work is dropped instead of being queued and displayed after newer geometry. Automated tracing fails a route if any geometry epoch is omitted, displayed out of order, or exceeds the bounded live-resize submission latency.

Every present stamp records epoch, HWND generation, actual client/window rectangles, image identity/size, target and observed model, visual-tree commit id, present id, and retirement result. This makes stale strips and future-size frames diagnosable.

## Implementation stages

### Stage 0: planning checkpoint

- Save this complete plan.
- Run `git diff --check` and review the checkpoint diff.
- Commit the entire dirty tree as the requested pre-implementation checkpoint. At the time of this revision the tree was clean before this plan changed, so the checkpoint contains the plan revision rather than hiding unrelated changes.

### Stage 1: contracts, registries, settings, and telemetry

- Add presentation/surface/host/copy/alpha registries and versioned persistence.
- Add capability solver, exact failure reasons, requested/observed runtime state, and settings pages.
- Add `MAGNATIVEIMAGE`, plane-based frames, copy instrumentation, adapter identity, and synchronization contracts without removing the current correctness path yet.

### Stage 2: host ownership and transactional switching

- Split application lifetime from visible-host lifetime.
- Register compatible redirected, no-redirection, layered, and WGL-helper classes.
- Implement hidden candidate creation, first-frame validation, publish, and fenced retirement.

### Stage 3: native D3D11 capture/render core

- Remove staging maps from WGC and Desktop Duplication native routes.
- Render capture planes plus UI directly into GPU images.
- Add separate BitBlt and flip-model presenters, pacing, tearing, and device-loss recovery.
- Establish strict-zero-copy tests on the D3D11 same-adapter route.

### Stage 4: DirectComposition and Presentation Manager

- Implement the complete DirectComposition visual tree and no-redirection host.
- Implement displayable-surface Presentation Manager when runtime/driver support is present.
- Implement opacity, transforms, clips, effects, animations, surfaces, visual overlays, target-time scheduling, statistics, and retirement.

### Stage 5: backend and presentation parity

- Complete D3D12/D3D11On12 native sharing and presenters.
- Complete Vulkan external-memory/semaphore and sampled presentation.
- Complete OpenGL DX interop and helper-host presentation.
- Complete D3D9Ex shared/FLIPEX paths and reset semantics.
- Complete GDI and legacy target profiles.
- Verify every target profile's candidate and observation behavior for every meaningful backend/host pair; unsupported pairs have exact reasons.

### Stage 6: traditional layered parity

- Implement constant alpha, color key, per-pixel premultiplied alpha, global opacity, hit-test policies, style-mode transitions, and atomic layered updates.
- Implement WIC Direct2D software and GDI-compatible hardware experiments with their copy class exposed.
- Prove strict zero-copy rejects this host.

### Stage 7: Direct2D 1.1 and text parity

- Replace detached CPU overlays on GPU routes with device-context/DXGI targets or native backend draw-list rendering.
- Complete DirectWrite, native glyph-atlas, and GDI text matrix with DPI/layout tests.

### Stage 8: geometry-epoch resize replacement

- Remove invalid `WVR_VALIDRECTS` behavior and destructive mid-drag resize.
- Implement the per-host epoch, image-reservoir, commit, and retirement protocols above.
- Add deterministic resize tracing and fault injection before visible testing.

### Stage 9: full verification and documentation

- Run all build/static, API matrix, strict-copy, device-loss, switching, DPI/topology, presentation-observation, and visible resize gates.
- Update user help with every setting, capability reason, copy class, and host tradeoff.
- Update this plan with evidence, not assertions.
- Commit the complete implementation as the requested final commit only after the gates pass.

## Acceptance matrix

### Build and static gates

- Debug and Release, Win32 and x64, using the repository's supported Windows SDK/toolset.
- Every automated HWND/render test runs on a private non-input Win32 desktop created specifically for that test process. Failure to create or bind the private desktop fails closed before any test HWND is created. Automated tests never call `SwitchDesktop` and never create, move, capture, or present a test window on the user's input desktop.
- No Windows SDK header is generated, edited, deleted, or vendored. Any missing two-macro message cracker belongs in the project's own extension header.
- Every enum appears exactly once in an authoritative registry; UI and persistence are registry-driven.
- WGL, D3D9, D3D11, D3D12, Vulkan, Direct2D, DirectWrite, DirectComposition, and Presentation API debug/validation output has no live-object or synchronization errors at shutdown.
- `git diff --check` passes and generated help/source artifacts are updated only by their repository generator.
- Normal startup creates the host hidden, creates only resources required by the resolved route, submits a complete first frame, and reveals it in one transaction. Timing traces fail startup that exposes the class/background brush, a white application frame, a busy cursor, or performs redundant renderer/presenter construction before first visibility.

### Functional matrix

- Every graphics API with every meaningful capture API, UI API, text API, host, surface ownership, and presentation target.
- Every connected Display Adapter with `Auto`, `Same as Display`, `Same as Capture`, and every compatible explicit Hardware Adapter; disconnected persisted identities and unsupported cross-adapter pairs produce exact status without rebinding to the wrong device.
- Every ordered live transition between supported configurations, including failure rollback.
- During every modal resize, each committed geometry epoch produces a fresh matching visual before the next epoch; no route may defer its first update until `WM_EXITSIZEMOVE`. Per-epoch submission stays within the test's bounded latency and stale/late frames are rejected rather than displayed.
- Window, Follow Mouse, and Lens modes; pan, wheel zoom, pointer-relative zoom, minimap fade/drag, outline, overlays, activation policy, and pointer input.
- Negative-coordinate monitors, mixed DPI, mixed refresh, HDR/SDR where supported, adapter migration, external-monitor-only, display disconnect/reconnect, minimize/restore, lock/UAC desktop transition, and occlusion.
- Device-lost/out-of-date injection for D3D9, D3D11, D3D12, and Vulkan.

### Zero-copy proof

- A same-adapter WGC-to-D3D11-to-DirectComposition/Presentation Manager route reports `TRUE_ZERO_COPY` and trips no map/readback/upload instrumentation.
- Display Adapter and Hardware Adapter selections are present in every trace by stable output identity and adapter LUID; changing either performs a transactional resource/host migration.
- DWM compositor-visual routes report `COMPOSITOR_VISUAL` and preserve app-owned wrapper transform/clip behavior.
- D3D12, Vulkan, OpenGL, and D3D9 routes report `TRUE_ZERO_COPY` only when actual shared-resource interop is active; otherwise they report `GPU_LOCAL_COPY` or unavailable under strict mode.
- Cross-adapter, GDI, traditional layered, and CPU fallback routes cannot masquerade as zero-copy.
- Resource identity, adapter LUID, acquire/release synchronization, and retirement are present in diagnostic traces.

### Presentation proof

- Each of the seven target settings can be selected when its prerequisite route exists and produces an exact observed result or a precise failure reason.
- Requested and observed modes are always visible separately.
- Auto selects the highest-performing stable route demonstrated on that machine and never a legacy/CPU route when a valid hardware-native route exists.
- Frame latency, tearing, target time, queue depth, dropped frames, and present statistics match the active API's contract.

### Layering and composition proof

- Constant alpha, color key, per-pixel premultiplied alpha, global opacity, and documented hit-testing behavior are visibly correct.
- DirectComposition content/UI/outline visuals independently exercise offset, transform, clip, opacity, effects, animation, surface update, and tree replacement.
- Switching between `SetLayeredWindowAttributes`, `UpdateLayeredWindowIndirect`, redirected HWND, no-redirection DirectComposition, and Presentation Manager leaves no stale style or resource ownership.

### Visible zero-flicker resize proof

Run the real `mag.exe` and record/capture slow and rapid drags from all four edges and all four corners for every presentation-host family. Include grow, shrink, reversal mid-drag, off-screen edges, mixed-DPI monitor crossing, active video, and external-monitor-only.

The test matrix covers the Cartesian product of every combination the capability solver leaves selectable: graphics, capture, UI, text, target, surface ownership, composition host, copy requirement, alpha mode, Display Adapter, and Hardware Adapter. Unsupported products must be rejected before Apply. A single flickering supported product fails the release; there is no exempt legacy, diagnostic, layered, or software route.

This is a deliberate human-visible acceptance session, not an automated test. All automated resize and presentation tests remain on their private non-input desktop; no automation is allowed to take control of or place test windows on the desktop the user is using.

The failure count must be zero for:

- background/non-client flashes;
- stale strips or duplicated edges;
- future-size content shown in old geometry;
- black/transparent frames;
- one-frame jumps between capture and UI/outline;
- input/activation regressions;
- leaked old hosts or buffers.

The visible result is reviewed frame-by-frame. `DwmFlush`, present counts, or smoke assertions alone do not pass this gate.

## Commit sequence

There are exactly two requested task commits:

1. **Planning checkpoint**: commit the complete dirty working tree after this plan is saved and reviewed.
2. **Final implementation**: implement the entire plan without intermediate task commits, run the full acceptance gates, update the evidence in this document, and commit the complete dirty working tree.

If a gate cannot be run on the current machine, the plan is not marked complete and the final commit is not represented as fully verified. The exact unrun gate and reason remain documented for the user.
