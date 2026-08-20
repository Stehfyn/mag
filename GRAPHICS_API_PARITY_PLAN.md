# Graphics, UI, and Text API Parity Plan

Status: parity baseline implemented; automated gates pass; interactive/cross-machine acceptance remains

Checkpoint commit: `97a97a8` (`Checkpoint message crackers and graphics parity plan`)

## Implementation record (2026-08-20)

Implemented in the working tree:

- One registry and lifecycle contract for OpenGL, GDI, Direct3D 9, Direct3D 11, Direct3D 12, and Vulkan.
- A top-down/bottom-up-aware CPU BGRA frame contract, a shared fill/stroke/text draw list, and a deterministic CPU compositor/fallback.
- Offscreen Direct2D UI, DirectWrite text, GDI text, and a cached DirectWrite-rasterized glyph atlas. OpenGL renders atlas glyphs as GPU quads; other graphics APIs use the same atlas through the CPU fallback.
- Atomic application of graphics, capture, UI, and text settings. Registry values are written only after the candidate configuration succeeds.
- Runtime presenter recreation followed by GDI fallback, frame-wait-handle duplication under an SRW lock, WGL context parking, and parked Vulkan WSI state for safe 32-bit API switching.
- Direct3D 11 and Direct3D 12 WARP fallback. Direct3D 12 uses a composition swap chain and an app-owned DirectComposition target so its target can detach while DWM owns presentation.
- Dynamic Vulkan loading with an application-local implicit-layer safe mode only when the caller has not explicitly configured `VK_LOADER_LAYERS_DISABLE`.
- DWM capture transitions release only capture-owned state. DWM Thumbnail/Private Visual retain compositor ownership; the selected graphics/UI/text configuration is their explicit pixel fallback.
- The BorderlessWindow32 resize contract: current-size content is latched and flushed from `WM_NCCALCSIZE`, resources are resized by `WM_SIZE`, and new-size content is presented only after `WM_WINDOWPOSCHANGED` commits geometry.

Automated evidence now covered by `--graphics-smoke`:

- Every available graphics backend x both UI modes x all three text modes.
- All 36 ordered graphics-backend transitions on this machine.
- Every graphics backend x all five capture selections, including compositor target detach/reattach and pixel fallback.
- Two real HWND resizes per backend, with assertions that both the pre-commit old-size frame and post-commit new-size frame were successfully presented and DWM-flushed.
- CPU checks for row orientation, BGRA channels, alpha blending, and all four one-pixel outline edges.

The remaining gate is deliberately not collapsed into “build passed”: interactive edge/corner dragging and the full visible matrix still need observation on the real desktop, external-monitor-only, and Windows 11 paths. The Windows-app control connection was unavailable in this run, so those checks remain open below.

## Goal

Add behaviorally equivalent GDI, OpenGL, Direct3D 9, Direct3D 11, Direct3D 12, and Vulkan presentation backends without coupling capture selection to presentation selection. Add a separate UI renderer selection with Direct2D and a separate text renderer selection that can compare DirectWrite, backend-native glyph-atlas rendering, and GDI.

"Parity" means the same visible pixels and interaction contract, not merely successful initialization or equivalent API calls.

## API taxonomy

The user-facing selections will be separate because these APIs operate at different layers:

```c
typedef enum GRAPHICSAPI
{
  GRAPHICS_API_OPENGL = 0, /* preserve the existing value */
  GRAPHICS_API_GDI,
  GRAPHICS_API_D3D9,
  GRAPHICS_API_D3D11,
  GRAPHICS_API_D3D12,
  GRAPHICS_API_VULKAN,
  GRAPHICS_API_COUNT
} GRAPHICSAPI;

typedef enum UIGRAPHICSAPI
{
  UI_GRAPHICS_API_NATIVE = 0,
  UI_GRAPHICS_API_DIRECT2D,
  UI_GRAPHICS_API_COUNT
} UIGRAPHICSAPI;

typedef enum TEXTRENDERER
{
  TEXT_RENDERER_DIRECTWRITE = 0,
  TEXT_RENDERER_GPU_GLYPH_ATLAS,
  TEXT_RENDERER_GDI,
  TEXT_RENDERER_COUNT
} TEXTRENDERER;
```

Direct2D is intentionally a UI renderer rather than another `GRAPHICSAPI` in the first implementation. DirectWrite is text layout/rasterization, not a presentation API. DXGI is adapter/resource/swap-chain infrastructure. DirectComposition and Windows Composition are composition/presentation layers. WARP is a D3D adapter fallback. D3D11On12 is internal interop. None belongs in the user-facing `GRAPHICSAPI` enum.

Other APIs considered but deliberately excluded from the first parity set:

- Direct3D 10: a legacy API with no useful coverage beyond D3D11. Direct3D 9 is included explicitly as the legacy-device/reset-model baseline.
- GDI+: a legacy convenience library; Direct2D and GDI cover the needed 2D baselines.
- OpenGL ES/ANGLE: useful only if an ANGLE compatibility target becomes a requirement.
- WebGPU/Dawn, Skia, Cairo, and FreeType: third-party dependency stacks rather than Windows presentation APIs. Reconsider only for a portability objective.
- Software GPU implementations: expose D3D11/D3D12 WARP as an adapter option/fallback, not as another graphics API.

## Current constraints to remove first

- `GRAPHICSAPI` currently contains only OpenGL and is stored but never dispatched.
- `SHAREDWGLDATA` mixes application state, capture state, GDI staging, and WGL/OpenGL objects.
- GDI, DXGI duplication, and Windows Graphics Capture all eventually write a bottom-up CPU BGRA DIB named `glScreenData`; presentation always uploads it to OpenGL.
- DWM Thumbnail and DWM Private Visual are compositor-owned capture/presentation paths and return before OpenGL presentation.
- The minimap and outline geometry are implemented directly as OpenGL calls. DWM Private Visual has a second, separate fill/stroke command representation.
- Switching the settings enum does not transactionally create a replacement graphics backend or destroy the old one.
- Capture cleanup is performed repeatedly from `renderSubmit` instead of at a capture-backend transition.
- There is no custom text renderer today. The settings dialog uses native Win32 controls.

## Architecture

### 1. Neutral application and renderer state

Rename `SHAREDWGLDATA` to `MAGSTATE`. Keep window/view/input state there, but move API objects into opaque backend-owned state:

```c
typedef struct MAGSTATE
{
  /* view, input, geometry, capture selection */
  GRAPHICSAPI graphicsApi;
  UIGRAPHICSAPI uiGraphicsApi;
  TEXTRENDERER textRenderer;
  const MAGGRAPHICSBACKEND* graphicsBackend;
  void* graphicsState;
  const MAGCAPTUREBACKEND* captureBackend;
  void* captureState;
  MAGPIXELBUFFER cpuFrame;
  MAGUIDRAWLIST uiDrawList;
} MAGSTATE;
```

Do not expose OpenGL, Vulkan, D3D, or Direct2D types from the neutral header.

### 2. Backend registry and lifecycle

One authoritative registry owns names, availability, capabilities, and operations. The settings dialog enumerates this registry rather than maintaining a second hand-written table.

```c
typedef struct MAGGRAPHICSBACKEND
{
  GRAPHICSAPI api;
  LPCTSTR name;
  BOOL implemented;
  BOOL (*IsAvailable)(void);
  BOOL (*Create)(HWND hWnd, void** stateOut);
  void (*Destroy)(HWND hWnd, void* state);
  BOOL (*Resize)(HWND hWnd, void* state, SIZE clientSize);
  BOOL (*SetPresentationEnabled)(HWND hWnd, void* state, BOOL enabled);
  BOOL (*Render)(HWND hWnd, void* state,
                 const MAGFRAME* frame,
                 const MAGUIDRAWLIST* ui);
  HANDLE (*GetFrameWaitHandle)(void* state);
} MAGGRAPHICSBACKEND;
```

`renderSetGraphicsApi` creates and resizes a candidate backend before publishing it. On failure it keeps the old backend alive and returns a concrete status message. Only a successful transition updates the selected enum. Destroying the former backend happens after the replacement is usable.

Capture selection participates in the same settings transaction. Capture resources remain lazy because several capture APIs can fail transiently and fall back to the canonical pixel path; cleanup occurs only at a capture transition.

### 3. Canonical captured-frame contract

The correctness path for every pixel-producing capture backend is an explicitly described CPU BGRA8 frame:

```c
typedef struct MAGPIXELBUFFER
{
  BYTE* pixels;
  UINT width;
  UINT height;
  UINT stride;
  MAGROWORDER rowOrder;
  MAGALPHAMODE alphaMode;
} MAGPIXELBUFFER;
```

`MAGFRAME` also allows optional native handles (`ID3D11Texture2D`, shared NT handle, synchronization value, adapter LUID) for later zero-copy fast paths. Native resources are optional optimizations; the CPU path remains the parity oracle and fallback.

GDI BitBlt, DXGI duplication, and WGC must first pass through this common contract. D3D11 can then add direct-texture consumption, followed by D3D12 and Vulkan shared-resource import only after the CPU path passes the same tests.

DWM Thumbnail and DWM Private Visual do not expose captured pixels. They therefore cannot form a literal all-by-all resource matrix with GDI/Vulkan/D3D renderers. They are marked `CAPTURE_CAP_COMPOSITOR_OUTPUT`; parity for those paths means the same source geometry, overlays, input, lifecycle, and visible result through their compositor visual. The settings UI must explain or disable combinations that cannot have meaningful renderer ownership instead of pretending the selected renderer consumes those pixels.

### 4. Backend-neutral UI draw list

Extract minimap, outline, and future HUD geometry into one draw list:

```c
typedef enum MAGUIDRAWCOMMANDTYPE
{
  MAG_UI_DRAW_FILL_RECT,
  MAG_UI_DRAW_STROKE_RECT,
  MAG_UI_DRAW_IMAGE,
  MAG_UI_DRAW_TEXT
} MAGUIDRAWCOMMANDTYPE;
```

Geometry, colors, opacity, clipping, and text layout inputs are computed once. OpenGL, GDI, D3D11, D3D12, Vulkan, and the DWM Private compositor path consume that same list. This replaces both the OpenGL-only minimap implementation and the private DWM-only fill/stroke format.

The one-pixel outline remains in physical client pixels. The bottom edge contract remains `height - 2` through `height - 1` so it is visible on the current surface.

### 5. Direct2D UI

Direct2D renders the UI draw list into a premultiplied BGRA overlay bitmap. Rendering offscreen makes the D2D UI choice usable with every pixel-presenting graphics backend without requiring fragile D2D/OpenGL, D2D/Vulkan, or D2D/D3D12 swap-chain interop.

- D3D11 may later use a native D2D/DXGI target as a fast path.
- D3D12 may later use D3D11On12 as a fast path.
- OpenGL and Vulkan consume the same overlay bitmap as an alpha texture.
- GDI composites it with `AlphaBlend`.
- The compositor capture paths translate the shared draw list to their owned overlay visual.

The initial UI text is a compact status/zoom label attached to the existing minimap lifetime so the text choices have a visible, testable output without permanently covering magnified content.

### 6. Text rendering

Text shaping/layout and glyph compositing are treated separately even though the settings expose three useful presets:

- `DirectWrite`: DirectWrite layout plus Direct2D rendering into the UI overlay.
- `GPU glyph atlas`: cached DirectWrite-rasterized A8 glyphs. OpenGL renders native atlas quads; Vulkan/D3D/GDI consume the same cached atlas through the CPU compositor so the output contract remains available without cross-API interop.
- `GDI`: `DrawTextW`/`ExtTextOutW` into the BGRA overlay DIB as the compatibility baseline.

All paths use the same Unicode string, font family, em size, DPI, alignment rectangle, and color. Exact antialiasing pixels may differ, so parity assertions compare layout bounds/baselines and use renderer-specific image tolerances.

## Backend implementation order

### Stage A: extraction without visible change

1. Introduce neutral state, pixel-frame, draw-list, backend registry, and transactional switch APIs.
2. Move existing WGL code into `render_opengl.c` behind the registry.
3. Move existing CPU capture DIB ownership out of WGL-named fields.
4. Generate minimap and outline commands once and have OpenGL consume them.
5. Keep OpenGL as the default and prove pixel identity before adding another backend.

### Stage B: GDI reference backend

1. Present the canonical BGRA frame with `StretchDIBits`/`SetDIBitsToDevice` from `WM_PAINT` or the render tick.
2. Render fill/stroke/image UI commands with GDI and composite premultiplied overlays with `AlphaBlend`.
3. Use GDI as the deterministic software/reference backend and last-resort fallback.

### Stage C: Direct3D 9

1. Create a windowed D3D9 device and a dynamic `A8R8G8B8` texture, using the fixed-function pre-transformed textured-quad path with point filtering.
2. Match the shared BGRA row-order, nearest-neighbor, UI draw-list, and outline contracts.
3. Implement the cooperative-level lost-device/reset lifecycle across minimize, resize, display topology, and adapter changes.
4. Fall back from hardware vertex processing to software vertex processing without creating another user-facing API value.

### Stage D: Direct3D 11

1. Create a BGRA-capable device and blt-model DXGI swap chain. The blt model is deliberate here: it avoids the one-flip-chain-per-HWND restriction while candidates are created before the old backend is released.
2. Match OpenGL nearest-neighbor magnification and client-pixel geometry.
3. Handle resize, occlusion, adapter migration, and device removal.
4. Add WARP fallback while retaining the same `GRAPHICS_API_D3D11` selection and reporting the active adapter.
5. Add optional direct consumption of DXGI/WGC D3D11 textures only after CPU-path parity passes.

### Stage E: Direct3D 12

1. Add device/queue, a flip-model composition swap chain, an app-owned DirectComposition target, frame contexts, upload resources, fences, and device-lost rebuild.
2. Use the same shader inputs and blend/nearest-sampling rules as D3D11.
3. Add WARP fallback and frame-latency waitable-object pacing.
4. Keep readback/cross-adapter/native-sharing optimizations behind capability checks.

### Stage F: Vulkan

1. Add Win32 surface, physical-device/queue selection, swap chain, per-frame synchronization, and staging upload. The canonical frame is copied directly to transfer-capable swap-chain images, so no shader or descriptor build artifact is required for this parity path.
2. Select an adapter compatible with the presentation surface and report a clear unavailable state if the loader/device/extensions are absent.
3. Keep the direct-transfer path shader-free; add generated SPIR-V only if a later native overlay/scale fast path actually needs shaders.
4. Handle resize/out-of-date/suboptimal/device-lost paths without changing the selected API until a replacement is ready.
5. Add D3D11 shared-texture import only as an optional fast path; never make it the correctness path.

### Stage G: Direct2D and text presets

1. Add the offscreen Direct2D overlay renderer and DirectWrite format/layout cache.
2. Add GDI text into the same overlay surface.
3. Add the DirectWrite-derived glyph atlas and backend-native text quads.
4. Add Graphics API, UI API, and Text Renderer controls to Settings, with capability filtering and a live status string.
5. Persist selections only after successful application.

## Presentation and pacing

The current D3DKMT vblank thread is retained as a fallback, not imposed on every backend. Each backend may expose a frame-latency waitable handle. The main scheduler waits on the active backend handle when available, otherwise D3DKMT vertical blank, otherwise the existing timer. No render backend may call an unconditional full-device wait per frame (`glFinish`, queue idle, or fence wait after every present) once its own frame resources are correctly synchronized.

## Failure and fallback policy

- Initialization failure: retain the previous backend; show the exact failed API and reason.
- Runtime device loss: attempt one rebuild on the same API/adapter, then fall back to GDI and report the fallback.
- Vulkan unavailable: keep it visible but disabled with a reason when the loader or required Win32 presentation support is missing.
- D3D hardware creation failure: retry WARP before declaring the API unavailable.
- Unsupported capture/graphics combination: disable Apply and name the compositor-owned incompatibility.
- Never silently change the stored selection before the replacement backend is rendering.

## Acceptance gates

### Build and static gates

- [x] Debug and Release, x86 and x64 build.
- [x] No OpenGL, Vulkan, D3D, or Direct2D implementation types leak into `graphics.h`.
- [x] The graphics registry contains every enum exactly once and generates the Graphics API settings list.
- [x] Repeated create/switch/resize/destroy smoke completes on x86 and x64 without a process fault.

### Automated pixel gates

For OpenGL, GDI, D3D9, D3D11, D3D12, and Vulkan using the CPU frame path:

- [ ] Visible source/clipping comparison on a negative-coordinate multi-monitor desktop.
- [ ] Visible nearest-neighbor comparison at 1x, fractional zoom, and integer zoom.
- [x] CPU oracle checks BGRA channel order, row orientation, alpha, and black/overlay composition.
- [x] CPU oracle checks all four one-pixel outline edges.
- [ ] Visible minimap geometry, opacity, drag mapping, and renderer-specific text tolerance comparison.

### Switching and lifecycle gates

- [x] Exercise every ordered graphics transition, including switching back to OpenGL.
- [x] Exercise two real HWND grow/shrink cycles on every backend and assert old/new geometry presentation order.
- [ ] Minimize/restore, DPI change, monitor move, external-monitor-only, and display-topology change.
- [ ] Explicit device-lost/out-of-date injection where the API permits it.
- [x] Exercise every capture selection with every graphics backend without deleting unrelated graphics state.
- [x] Candidate failure paths retain the old configuration; runtime presenter failure retries once and then selects GDI.

### User-visible acceptance gate

Run the real `mag.exe` and verify every supported graphics/capture/UI/text combination relevant to its capability matrix across:

- Window, Follow Mouse, and Lens modes.
- Pan, wheel zoom, mouse-relative zoom, resize, and move.
- Minimap fade/drag and the accent outline.
- Chrome or another hardware-video surface, with no black visual or disappearing video.
- Focus/input behavior and normal movement speed.
- Windows 10 and Windows 11, native display and external-monitor-only paths.

A successful build, successful API return values, a responsive process, or a focused smoke harness is progress but not completion of this gate.

### Final resize-flicker stage

Study `C:\Users\Steph2\Desktop\startup\shamboni2\opus\BorderlessWindow32`, especially `immersivewindowdemo-zero-flicker.diff`, and adapt its relevant borderless resize/paint ownership contract to `mag`. Preserve the renderer and capture boundaries above; prove that interactive edge/corner resize no longer exposes background, stale strips, or non-client flashes with every pixel-presenting backend.

Implementation status: the ordering contract and automated old/new geometry assertions are complete. Interactive edge/corner observation is still required because the Windows-app control connection was unavailable during this run.

## Commit sequence

1. Checkpoint the pre-existing dirty tree plus this plan.
2. Neutral contracts and OpenGL extraction with pixel identity.
3. GDI backend and reference tests.
4. D3D9 backend and tests.
5. D3D11 backend and tests.
6. D3D12 backend and tests.
7. Vulkan backend, shader generation, and tests.
8. Direct2D UI plus DirectWrite/GDI/GPU-atlas text.
9. Settings/capability/persistence integration.
10. Full matrix verification and plan status update.
11. Borderless-window zero-flicker resize adaptation and interactive verification.
