#include "framework.h"
#include "graphics.h"

#include <d3d9.h>

#pragma comment(lib, "d3d9")

typedef struct MAGD3D9STATE
{
  IDirect3D9*        d3d;
  IDirect3DDevice9*  device;
  IDirect3DTexture9* texture;
  D3DPRESENT_PARAMETERS present;
  MAGCPUCOMPOSITOR   compositor;
  UINT               width;
  UINT               height;
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

static BOOL magGraphicsD3D9CreateTexture(MAGD3D9STATE* state, SIZE clientSize)
{
    HRESULT hr;

    magGraphicsD3D9ReleaseDeviceResources(state);
    hr = IDirect3DDevice9_CreateTexture(
      state->device,
      (UINT)clientSize.cx,
      (UINT)clientSize.cy,
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

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    return TRUE;
}

static void magGraphicsD3D9SetPresentParameters(
  MAGD3D9STATE* state,
  HWND hWnd,
  SIZE clientSize)
{
    ZeroMemory(&state->present, sizeof(state->present));
    state->present.BackBufferWidth = (UINT)clientSize.cx;
    state->present.BackBufferHeight = (UINT)clientSize.cy;
    state->present.BackBufferFormat = D3DFMT_X8R8G8B8;
    state->present.BackBufferCount = 1;
    state->present.MultiSampleType = D3DMULTISAMPLE_NONE;
    state->present.SwapEffect = D3DSWAPEFFECT_DISCARD;
    state->present.hDeviceWindow = hWnd;
    state->present.Windowed = TRUE;
    state->present.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
}

static BOOL magGraphicsD3D9Create(HWND hWnd, SIZE clientSize, void** stateOut)
{
    MAGD3D9STATE* state;
    HRESULT hr;

    if (!stateOut || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *stateOut = NULL;

    state = (MAGD3D9STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }

    state->d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!state->d3d)
    {
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }

    magGraphicsD3D9SetPresentParameters(state, hWnd, clientSize);
    hr = IDirect3D9_CreateDevice(
      state->d3d,
      D3DADAPTER_DEFAULT,
      D3DDEVTYPE_HAL,
      hWnd,
      D3DCREATE_HARDWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
      &state->present,
      &state->device);
    if (FAILED(hr))
    {
      hr = IDirect3D9_CreateDevice(
        state->d3d,
        D3DADAPTER_DEFAULT,
        D3DDEVTYPE_HAL,
        hWnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE,
        &state->present,
        &state->device);
    }

    if (FAILED(hr) || !magGraphicsD3D9CreateTexture(state, clientSize))
    {
      if (state->device)
      {
        IDirect3DDevice9_Release(state->device);
      }
      IDirect3D9_Release(state->d3d);
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }

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
      IDirect3DDevice9_Release(state->device);
    }
    if (state->d3d)
    {
      IDirect3D9_Release(state->d3d);
    }
    magGraphicsDestroyCpuCompositor(&state->compositor);
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsD3D9Reset(HWND hWnd, MAGD3D9STATE* state, SIZE clientSize)
{
    HRESULT hr;

    if (clientSize.cx < 1 || clientSize.cy < 1)
    {
      return TRUE;
    }

    magGraphicsD3D9ReleaseDeviceResources(state);
    magGraphicsD3D9SetPresentParameters(state, hWnd, clientSize);
    hr = IDirect3DDevice9_Reset(state->device, &state->present);
    return SUCCEEDED(hr) && magGraphicsD3D9CreateTexture(state, clientSize);
}

static BOOL magGraphicsD3D9Resize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGD3D9STATE* state = (MAGD3D9STATE*)opaqueState;

    return state && magGraphicsD3D9Reset(hWnd, state, clientSize);
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
  const MAGUIDRAWLIST* ui)
{
    MAGD3D9STATE* state = (MAGD3D9STATE*)opaqueState;
    MAGPIXELBUFFER composed;
    D3DLOCKED_RECT locked;
    MAGD3D9VERTEX vertices[4];
    UINT y;
    HRESULT hr;

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
        composed.stride);
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

    vertices[0] = (MAGD3D9VERTEX){ -0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 0.0f };
    vertices[1] = (MAGD3D9VERTEX){ (FLOAT)state->width - 0.5f, -0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 1.0f, 0.0f };
    vertices[2] = (MAGD3D9VERTEX){ -0.5f, (FLOAT)state->height - 0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 0.0f, 1.0f };
    vertices[3] = (MAGD3D9VERTEX){ (FLOAT)state->width - 0.5f, (FLOAT)state->height - 0.5f, 0.0f, 1.0f, 0xFFFFFFFF, 1.0f, 1.0f };

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

    hr = IDirect3DDevice9_Present(state->device, NULL, NULL, hWnd, NULL);
    return SUCCEEDED(hr);
}

static HANDLE magGraphicsD3D9GetFrameWaitHandle(void* state)
{
    UNREFERENCED_PARAMETER(state);
    return NULL;
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
};
