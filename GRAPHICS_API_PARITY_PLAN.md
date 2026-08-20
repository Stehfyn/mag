# Graphics, UI, and Text API Parity Plan

Status: implementation plan

## Goal

Add behaviorally equivalent GDI, OpenGL, Direct3D 11, Direct3D 12, and Vulkan presentation backends without coupling capture selection to presentation selection. Add a separate UI renderer selection with Direct2D and a separate text renderer selection that can compare DirectWrite, backend-native glyph-atlas rendering, and GDI.

"Parity" means the same visible pixels and interaction contract, not merely successful initialization or equivalent API calls.

## API taxonomy

The user-facing selections will be separate because these APIs operate at different layers:

```c
typedef enum GRAPHICSAPI
{
  GRAPHICS_API_OPENGL = 0, /* preserve the existing value */
  GRAPHICS_API_GDI,
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

- Direct3D 9 and 10: legacy APIs with no useful coverage beyond D3D11.
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
  UINT capabilities;
  BOOL (*IsAvailable)(void);
  BOOL (*Create)(HWND hWnd, void** stateOut);
  void (*Destroy)(HWND hWnd, void* state);
  BOOL (*Resize)(HWND hWnd, void* state, SIZE clientSize);
  BOOL (*Render)(HWND hWnd, void* state,
                 const MAGFRAME* frame,
                 const MAGUIDRAWLIST* ui);
  HANDLE (*GetFrameWaitHandle)(void* state);
} MAGGRAPHICSBACKEND;
```

`renderSetGraphicsApi` creates and resizes a candidate backend before publishing it. On failure it keeps the old backend alive and returns a concrete status message. Only a successful transition updates the selected enum. Destroying the former backend happens after the replacement is usable.

Capture selection receives the same transactional lifecycle. Cleanup moves out of the per-frame switch.

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
- `GPU glyph atlas`: DirectWrite shaping/glyph analysis, cached A8 glyph atlas, and quads rendered by the selected graphics backend. With `GRAPHICS_API_OPENGL`, this is the requested OpenGL text path; the same contract also gives Vulkan/D3D parity.
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

### Stage C: Direct3D 11

1. Create a BGRA-capable device, flip-model DXGI swap chain, render target, sampler, dynamic/upload texture, and alpha-blended UI pipeline.
2. Match OpenGL nearest-neighbor magnification and client-pixel geometry.
3. Handle resize, occlusion, adapter migration, and device removal.
4. Add WARP fallback while retaining the same `GRAPHICS_API_D3D11` selection and reporting the active adapter.
5. Add optional direct consumption of DXGI/WGC D3D11 textures only after CPU-path parity passes.

### Stage D: Direct3D 12

1. Add device/queue/flip-model swap chain, RTV/SRV heaps, frame contexts, upload ring, fences, and device-lost rebuild.
2. Use the same shader inputs and blend/nearest-sampling rules as D3D11.
3. Add WARP fallback and frame-latency waitable-object pacing.
4. Keep readback/cross-adapter/native-sharing optimizations behind capability checks.

### Stage E: Vulkan

1. Add Win32 surface, physical-device/queue selection, swap chain, render pass or dynamic rendering, per-frame synchronization, staging upload, descriptor sets, and alpha blending.
2. Select an adapter compatible with the presentation surface and report a clear unavailable state if the loader/device/extensions are absent.
3. Build shaders from checked-in source through MSBuild; generated SPIR-V is build output or an explicitly tracked generated asset with a byte-identity check.
4. Handle resize/out-of-date/suboptimal/device-lost paths without changing the selected API until a replacement is ready.
5. Add D3D11 shared-texture import only as an optional fast path; never make it the correctness path.

### Stage F: Direct2D and text presets

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

- Debug and Release, Win32 and x64.
- No backend headers leak into neutral state headers.
- Registry contains every enum exactly once; settings options are generated from it.
- Every successfully initialized backend fully releases its HWND/DC/device/swap-chain/fence/descriptor/bitmap/font resources.

### Automated pixel gates

For OpenGL, GDI, D3D11, D3D12, and Vulkan using the CPU frame path:

- Identical source rectangle and clipping on negative-coordinate/multi-monitor desktops.
- Nearest-neighbor scale at 1x, fractional zoom, and integer zoom.
- Correct BGRA channel order, row orientation, alpha, and black fill outside clipped source.
- All four one-pixel outline edges visible.
- Minimap panel, monitor rectangles, window rectangle, source rectangle, opacity, and drag mapping.
- Text baseline/bounds parity and per-renderer image tolerance.

### Switching and lifecycle gates

- Exercise every ordered graphics transition, including switching back to OpenGL.
- Resize/minimize/restore, DPI change, monitor move, external-monitor-only, and display topology change.
- Device-lost/out-of-date simulation where the API permits it.
- Capture switching does not leak resources or delete unrelated active graphics resources.
- Failure injection proves transactional rollback and GDI fallback.

### User-visible acceptance gate

Run the real `mag.exe` and verify every supported graphics/capture/UI/text combination relevant to its capability matrix across:

- Window, Follow Mouse, and Lens modes.
- Pan, wheel zoom, mouse-relative zoom, resize, and move.
- Minimap fade/drag and the accent outline.
- Chrome or another hardware-video surface, with no black visual or disappearing video.
- Focus/input behavior and normal movement speed.
- Windows 10 and Windows 11, native display and external-monitor-only paths.

A successful build, successful API return values, a responsive process, or a focused smoke harness is progress but not completion of this gate.

## Commit sequence

1. Checkpoint the pre-existing dirty tree plus this plan.
2. Neutral contracts and OpenGL extraction with pixel identity.
3. GDI backend and reference tests.
4. D3D11 backend and tests.
5. D3D12 backend and tests.
6. Vulkan backend, shader generation, and tests.
7. Direct2D UI plus DirectWrite/GDI/GPU-atlas text.
8. Settings/capability/persistence integration.
9. Full matrix verification and plan status update.
