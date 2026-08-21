#include "framework.h"
#include "graphics.h"
#include "presentation.h"

#include <d3d9.h>

#pragma comment(lib, "d3d9")

typedef struct MAGD3D9STATE
{
  IDirect3D9*        d3d;
  IDirect3D9Ex*      d3dEx;
  IDirect3DDevice9*  device;
  IDirect3DDevice9Ex* deviceEx;
  IDirect3DTexture9* texture;
  D3DPRESENT_PARAMETERS present;
  MAGCPUCOMPOSITOR   compositor;
  UINT               width;
  UINT               height;
  UINT               capacityWidth;
  UINT               capacityHeight;
  UINT64             resourceGeneration;
  UINT               adapterOrdinal;
  MAGPRESENTATIONTARGET configuredTarget;
  BOOL               flipEx;
} MAGD3D9STATE;

typedef struct MAGD3D9VERTEX
{
  FLOAT x;
  FLOAT y;
  FLOAT z;
  FLOAT rhw;
  DWORD color;
  FLOAT u;
  FLOAT v;
} MAGD3D9VERTEX;

#define MAG_D3D9_FVF (D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)

static BOOL magGraphicsD3D9IsAvailable(LPTSTR reason, UINT reasonCount)
{
    HMODULE module = LoadLibrary(TEXT("d3d9.dll"));

    if (!magGraphicsIsInputDesktop())
    {
      if (module)
      {
        FreeLibrary(module);
      }
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("Direct3D 9 window presentation is unavailable on the private non-input test desktop."), reasonCount);
      }
      return FALSE;
    }

    if (module)
    {
      FreeLibrary(module);
      if (reason && reasonCount)
      {
        reason[0] = TEXT('\0');
      }
      return TRUE;
    }

    if (reason && reasonCount)
    {
      lstrcpyn(reason, TEXT("Direct3D 9 is not installed."), reasonCount);
    }
    return FALSE;
}

static void magGraphicsD3D9ReleaseDeviceResources(MAGD3D9STATE* state)
{
    if (state->texture)
    {
      IDirect3DTexture9_Release(state->texture);
      state->texture = NULL;
    }
}

static BOOL magGraphicsD3D9CreateTexture(MAGD3D9STATE* state, SIZE reservoirSize)
{
    HRESULT hr;

    magGraphicsD3D9ReleaseDeviceResources(state);
    hr = IDirect3DDevice9_CreateTexture(
      state->device,
      (UINT)reservoirSize.cx,
      (UINT)reservoirSize.cy,
      1,
      D3DUSAGE_DYNAMIC,
      D3DFMT_A8R8G8B8,
      D3DPOOL_DEFAULT,
      &state->texture,
      NULL);
    if (FAILED(hr))
    {
      return FALSE;
    }

    state->capacityWidth = (UINT)reservoirSize.cx;
    state->capacityHeight = (UINT)reservoirSize.cy;
    ++state->resourceGeneration;
    return TRUE;
}

static void magGraphicsD3D9SetPresentParameters(
  MAGD3D9STATE* state,
  HWND hWnd,
  SIZE clientSize,
  const MAGPRESENTATIONSETTINGS* presentation)
{
    static const UINT intervals[] =
    {
      D3DPRESENT_INTERVAL_IMMEDIATE,
      D3DPRESENT_INTERVAL_ONE,
      D3DPRESENT_INTERVAL_TWO,
      D3DPRESENT_INTERVAL_THREE,
      D3DPRESENT_INTERVAL_FOUR,
    };

    ZeroMemory(&state->present, sizeof(state->present));
    state->present.BackBufferWidth = (UINT)clientSize.cx;
    state->present.BackBufferHeight = (UINT)clientSize.cy;
    state->present.BackBufferFormat = D3DFMT_X8R8G8B8;
    state->present.BackBufferCount = presentation->bufferCount;
    state->present.MultiSampleType = D3DMULTISAMPLE_NONE;
    state->configuredTarget = presentation->target;
    state->flipEx = MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP == presentation->target ||
      MAG_PRESENT_COMPOSED_FLIP == presentation->target ||
      MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP == presentation->target;
    if (state->flipEx)
    {
      state->present.SwapEffect = D3DSWAPEFFECT_FLIPEX;
      state->present.BackBufferCount = max(2U, presentation->bufferCount);
    }
    else if (MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER == presentation->target)
    {
      state->present.SwapEffect = D3DSWAPEFFECT_COPY;
      state->present.BackBufferCount = 1;
    }
    else if (MAG_PRESENT_HARDWARE_LEGACY_FLIP == presentation->target)
    {
      state->present.SwapEffect = D3DSWAPEFFECT_FLIP;
    }
    else
    {
      state->present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    }
    state->present.hDeviceWindow = hWnd;
    state->present.Windowed = TRUE;
    state->present.PresentationInterval = intervals[presentation->syncInterval];
}

static UINT magGraphicsD3D9FindAdapter(
  IDirect3D9* d3d,
  const MAGPRESENTATIONSETTINGS* presentation)
{
    UINT adapterCount = IDirect3D9_GetAdapterCount(d3d);
    UINT adapter;

    if (!presentation->display.deviceName[0])
    {
      return D3DADAPTER_DEFAULT;
    }
    for (adapter = 0; adapter < adapterCount; ++adapter)
    {
      HMONITOR monitor = IDirect3D9_GetAdapterMonitor(d3d, adapter);
      MONITORINFOEX monitorInfo = { sizeof(monitorInfo) };

      if (monitor && GetMonitorInfo(monitor, (MONITORINFO*)&monitorInfo) &&
          0 == lstrcmpi(monitorInfo.szDevice, presentation->display.deviceName))
      {
        return adapter;
      }
    }
    return D3DADAPTER_DEFAULT;
}

static BOOL magGraphicsD3D9Create(
  HWND hWnd,
  SIZE clientSize,
  const struct MAGPRESENTATIONSETTINGS* presentation,
  void** stateOut)
{
    MAGD3D9STATE* state;
    SIZE reservoirSize;
    HRESULT hr;

    if (!stateOut || !presentation || clientSize.cx < 1 || clientSize.cy < 1 ||
        presentation->bufferCount < 1 || presentation->bufferCount > 3 ||
        presentation->syncInterval > 4)
    {
      return FALSE;
    }
    *stateOut = NULL;

    state = (MAGD3D9STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }

    hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &state->d3dEx);
    if (SUCCEEDED(hr) && state->d3dEx)
    {
      state->d3d = (IDirect3D9*)state->d3dEx;
    }
    else
    {
      state->d3d = Direct3DCreate9(D3D_SDK_VERSION);
    }
    if (!state->d3d)
    {
      if (state->d3dEx)
      {
        IDirect3D9Ex_Release(state->d3dEx);
      }
      else if (state->d3d)
      {
        IDirect3D9_Release(state->d3d);
      }
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }

    state->adapterOrdinal = magGraphicsD3D9FindAdapter(state->d3d, presentation);
    reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
    magGraphicsD3D9SetPresentParameters(state, hWnd, reservoirSize, presentation);
    if (state->flipEx && !state->d3dEx)
    {
      IDirect3D9_Release(state->d3d);
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }
    if (state->d3dEx)
    {
      const DWORD behaviorFlags =
        D3DCREATE_HARDWARE_VERTEXPROCESSING |
        D3DCREATE_FPU_PRESERVE |
        (state->flipEx ? D3DCREATE_ENABLE_PRESENTSTATS : 0);
      hr = IDirect3D9Ex_CreateDeviceEx(
        state->d3dEx,
        state->adapterOrdinal,
        D3DDEVTYPE_HAL,
        hWnd,
        behaviorFlags,
        &state->present,
        NULL,
        &state->deviceEx);
      state->device = (IDirect3DDevice9*)state->deviceEx;
    }
    else
    {
      hr = IDirect3D9_CreateDevice(
        state->d3d,
        state->adapterOrdinal,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &state->present,
        &state->device);
    }
    if (FAILED(hr))
    {
      if (state->d3dEx)
      {
        const DWORD behaviorFlags =
          D3DCREATE_SOFTWARE_VERTEXPROCESSING |
          D3DCREATE_FPU_PRESERVE |
          (state->flipEx ? D3DCREATE_ENABLE_PRESENTSTATS : 0);
        hr = IDirect3D9Ex_CreateDeviceEx(
          state->d3dEx,
          state->adapterOrdinal,
          D3DDEVTYPE_HAL,
          hWnd,
          behaviorFlags,
          &state->present,
          NULL,
          &state->deviceEx);
        state->device = (IDirect3DDevice9*)state->deviceEx;
      }
      else
      {
        hr = IDirect3D9_CreateDevice(
          state->d3d,
          state->adapterOrdinal,
          D3DDEVTYPE_HAL,
          hWnd,
          D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
          &state->present,
          &state->device);
      }
    }

    if (FAILED(hr) || !magGraphicsD3D9CreateTexture(state, reservoirSize) ||
        !magGraphicsReserveCpuCompositor(
          &state->compositor,
          (UINT)reservoirSize.cx,
          (UINT)reservoirSize.cy))
    {
      if (state->device)
      {
        if (state->deviceEx)
        {
          IDirect3DDevice9Ex_Release(state->deviceEx);
        }
        else
        {
          IDirect3DDevice9_Release(state->device);
        }
      }
      if (state->d3dEx)
      {
        IDirect3D9Ex_Release(state->d3dEx);
      }
      else
      {
        IDirect3D9_Release(state->d3d);
      }
      magGraphicsDestroyCpuCompositor(&state->compositor);
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    *stateOut = state;
    return TRUE;
}

static void magGraphicsD3D9Destroy(HWND hWnd, void* opaqueState)
{
    MAGD3D9STATE* state = (MAGD3D9STATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return;
    }

    magGraphicsD3D9ReleaseDeviceResources(state);
    if (state->device)
    {
      if (state->deviceEx)
      {
        IDirect3DDevice9Ex_Release(state->deviceEx);
      }
      else
      {
        IDirect3DDevice9_Release(state->device);
      }
    }
    if (state->d3d)
    {
      if (state->d3dEx)
      {
        IDirect3D9Ex_Release(state->d3dEx);
      }
      else
      {
        IDirect3D9_Release(state->d3d);
      }
    }
    magGraphicsDestroyCpuCompositor(&state->compositor);
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsD3D9Reset(HWND hWnd, MAGD3D9STATE* state, SIZE clientSize)
{
    HRESULT hr;
    SIZE reservoirSize;

    if (clientSize.cx < 1 || clientSize.cy < 1)
    {
      return TRUE;
    }

    reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
    reservoirSize.cx = max(reservoirSize.cx, (LONG)state->capacityWidth);
    reservoirSize.cy = max(reservoirSize.cy, (LONG)state->capacityHeight);
    magGraphicsD3D9ReleaseDeviceResources(state);
    state->present.BackBufferWidth = (UINT)reservoirSize.cx;
    state->present.BackBufferHeight = (UINT)reservoirSize.cy;
    hr = state->deviceEx
      ? IDirect3DDevice9Ex_ResetEx(state->deviceEx, &state->present, NULL)
      : IDirect3DDevice9_Reset(state->device, &state->present);
    if (FAILED(hr) || !magGraphicsD3D9CreateTexture(state, reservoirSize))
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    return TRUE;
}

static BOOL magGraphicsD3D9Resize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGD3D9STATE* state = (MAGD3D9STATE*)opaqueState;

    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    if (state->width <= state->capacityWidth && state->height <= state->capacityHeight)
    {
      return TRUE;
    }
    return magGraphicsD3D9Reset(hWnd, state, clientSize);
}

static BOOL magGraphicsD3D9EnsureReady(HWND hWnd, MAGD3D9STATE* state)
{
    const HRESULT hr = IDirect3DDevice9_TestCooperativeLevel(state->device);

    if (D3DERR_DEVICELOST == hr)
    {
      return FALSE;
    }
    if (D3DERR_DEVICENOTRESET == hr)
    {
      RECT rect;
      SIZE size;

      GetClientRect(hWnd, &rect);
      size.cx = max(1, RECTWIDTH(rect));
      size.cy = max(1, RECTHEIGHT(rect));
      return magGraphicsD3D9Reset(hWnd, state, size);
    }
    return SUCCEEDED(hr);
}

static BOOL magGraphicsD3D9Render(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  const MAGPRESENTINTENT* intent)
{
    MAGD3D9STATE* state = (MAGD3D9STATE*)opaqueState;
    MAGPIXELBUFFER composed;
    D3DLOCKED_RECT locked;
    MAGD3D9VERTEX vertices[4];
    RECT sourceRect;
    RECT destinationRect;
    FLOAT uMax;
    FLOAT vMax;
    FLOAT drawWidth;
    FLOAT drawHeight;
    UINT y;
    HRESULT hr;

    UNREFERENCED_PARAMETER(intent);

    if (!state || !frame || frame->width != state->width || frame->height != state->height ||
        !magGraphicsD3D9EnsureReady(hWnd, state) ||
        !magGraphicsComposeFrame(&state->compositor, frame, ui, &composed))
    {
      return FALSE;
    }

    hr = IDirect3DTexture9_LockRect(state->texture, 0, &locked, NULL, D3DLOCK_DISCARD);
    if (FAILED(hr))
    {
      return FALSE;
    }
    for (y = 0; y < composed.height; ++y)
    {
      CopyMemory(
        (BYTE*)locked.pBits + (SIZE_T)y * locked.Pitch,
        composed.pixels + (SIZE_T)y * composed.stride,
        (SIZE_T)composed.width * 4U);
    }
    IDirect3DTexture9_UnlockRect(state->texture, 0);

    IDirect3DDevice9_SetRenderState(state->device, D3DRS_CULLMODE, D3DCULL_NONE);
    IDirect3DDevice9_SetRenderState(state->device, D3DRS_ZENABLE, FALSE);
    IDirect3DDevice9_SetRenderState(state->device, D3DRS_ALPHABLENDENABLE, FALSE);
    IDirect3DDevice9_SetSamplerState(state->device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(state->device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
    IDirect3DDevice9_SetSamplerState(state->device, 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetSamplerState(state->device, 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP);
    IDirect3DDevice9_SetTextureStageState(state->device, 0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    IDirect3DDevice9_SetTextureStageState(state->device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    IDirect3DDevice9_SetTextureStageState(state->device, 0, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
    IDirect3DDevice9_SetFVF(state->device, MAG_D3D9_FVF);
    IDirect3DDevice9_SetTexture(state->device, 0, (IDirect3DBaseTexture9*)state->texture);

    uMax = (FLOAT)state->width / (FLOAT)state->capacityWidth;
    vMax = (FLOAT)state->height / (FLOAT)state->capacityHeight;
    drawWidth = D3DSWAPEFFECT_COPY == state->present.SwapEffect
      ? (FLOAT)state->width
      : (FLOAT)state->capacityWidth;
    drawHeight = D3DSWAPEFFECT_COPY == state->present.SwapEffect
      ? (FLOAT)state->height
      : (FLOAT)state->capacityHeight;
    vertices[0] = (MAGD3D9VERTEX){ -0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    vertices[1] = (MAGD3D9VERTEX){ drawWidth - 0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, uMax, 0.0f };
    vertices[2] = (MAGD3D9VERTEX){ -0.5f, drawHeight - 0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, vMax };
    vertices[3] = (MAGD3D9VERTEX){ drawWidth - 0.5f, drawHeight - 0.5f, 0.0f, 1.0f, 0xFFFFFFFF, uMax, vMax };

    hr = IDirect3DDevice9_BeginScene(state->device);
    if (SUCCEEDED(hr))
    {
      hr = IDirect3DDevice9_DrawPrimitiveUP(
        state->device,
        D3DPT_TRIANGLESTRIP,
        2,
        vertices,
        sizeof(vertices[0]));
      IDirect3DDevice9_EndScene(state->device);
    }
    if (FAILED(hr))
    {
      return FALSE;
    }

    if (state->flipEx)
    {
      DWORD presentFlags = 0;

      if (intent && intent->restartSequence)
      {
        presentFlags |= D3DPRESENT_FORCEIMMEDIATE;
      }
      if (intent && !intent->synchronize)
      {
        presentFlags |= D3DPRESENT_FORCEIMMEDIATE | D3DPRESENT_DONOTWAIT;
      }
      hr = IDirect3DDevice9Ex_PresentEx(
        state->deviceEx,
        NULL,
        NULL,
        NULL,
        NULL,
        presentFlags);
      return SUCCEEDED(hr) || D3DERR_WASSTILLDRAWING == hr;
    }

    if (D3DSWAPEFFECT_COPY == state->present.SwapEffect)
    {
      SetRect(&sourceRect, 0, 0, (LONG)state->width, (LONG)state->height);
      GetClientRect(hWnd, &destinationRect);
      hr = IDirect3DDevice9_Present(
        state->device,
        &sourceRect,
        &destinationRect,
        NULL,
        NULL);
    }
    else
    {
      hr = IDirect3DDevice9_Present(
        state->device,
        NULL,
        NULL,
        NULL,
        NULL);
    }
    return SUCCEEDED(hr);
}

static HANDLE magGraphicsD3D9GetFrameWaitHandle(void* state)
{
    UNREFERENCED_PARAMETER(state);
    return NULL;
}

static UINT64 magGraphicsD3D9GetResourceGeneration(void* opaqueState)
{
    MAGD3D9STATE* state = (MAGD3D9STATE*)opaqueState;
    return state ? state->resourceGeneration + state->compositor.generation : 0;
}

const MAGGRAPHICSBACKEND g_magGraphicsD3D9Backend =
{
  GRAPHICS_API_D3D9,
  TEXT("Direct3D 9"),
  TRUE,
  magGraphicsD3D9IsAvailable,
  magGraphicsD3D9Create,
  magGraphicsD3D9Destroy,
  magGraphicsD3D9Resize,
  magGraphicsSetPresentationEnabledNoop,
  magGraphicsD3D9Render,
  magGraphicsD3D9GetFrameWaitHandle,
  magGraphicsD3D9GetResourceGeneration,
  NULL,
  NULL,
};
