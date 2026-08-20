#include "framework.h"
#include "graphics.h"
#include "graphics_dcomp.h"

#include <d3d12.h>
#include <dxgi1_4.h>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")

#define MAG_D3D12_BUFFER_COUNT 2

typedef struct MAGD3D12FRAME
{
  ID3D12Resource*       backBuffer;
  ID3D12Resource*       upload;
  ID3D12CommandAllocator* allocator;
  BYTE*                 mappedUpload;
  UINT64                fenceValue;
} MAGD3D12FRAME;

typedef struct MAGD3D12STATE
{
  IDXGIFactory4*        factory;
  MAGDCOMPPRESENTER*    composition;
  ID3D12Device*         device;
  ID3D12CommandQueue*   queue;
  IDXGISwapChain3*      swapChain;
  ID3D12GraphicsCommandList* commandList;
  ID3D12Fence*          fence;
  HANDLE                fenceEvent;
  HANDLE                frameWaitHandle;
  MAGD3D12FRAME         frames[MAG_D3D12_BUFFER_COUNT];
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint;
  UINT64                nextFenceValue;
  MAGCPUCOMPOSITOR      compositor;
  UINT                  width;
  UINT                  height;
  BOOL                  warp;
} MAGD3D12STATE;

static BOOL magGraphicsD3D12IsAvailable(LPTSTR reason, UINT reasonCount)
{
    HMODULE module = LoadLibrary(TEXT("d3d12.dll"));

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
      lstrcpyn(reason, TEXT("Direct3D 12 is not installed."), reasonCount);
    }
    return FALSE;
}

static void magGraphicsD3D12ReleaseResource(ID3D12Resource** resource)
{
    if (*resource)
    {
      ID3D12Resource_Release(*resource);
      *resource = NULL;
    }
}

static BOOL magGraphicsD3D12WaitForFence(MAGD3D12STATE* state, UINT64 value)
{
    if (!value || ID3D12Fence_GetCompletedValue(state->fence) >= value)
    {
      return TRUE;
    }

    if (FAILED(ID3D12Fence_SetEventOnCompletion(state->fence, value, state->fenceEvent)))
    {
      return FALSE;
    }
    return WAIT_OBJECT_0 == WaitForSingleObject(state->fenceEvent, INFINITE);
}

static BOOL magGraphicsD3D12WaitIdle(MAGD3D12STATE* state)
{
    const UINT64 value = state->nextFenceValue++;

    if (!state->queue || !state->fence ||
        FAILED(ID3D12CommandQueue_Signal(state->queue, state->fence, value)))
    {
      return FALSE;
    }
    return magGraphicsD3D12WaitForFence(state, value);
}

static void magGraphicsD3D12ReleaseFrameResources(MAGD3D12STATE* state)
{
    UINT i;

    for (i = 0; i < MAG_D3D12_BUFFER_COUNT; ++i)
    {
      if (state->frames[i].upload && state->frames[i].mappedUpload)
      {
        ID3D12Resource_Unmap(state->frames[i].upload, 0, NULL);
        state->frames[i].mappedUpload = NULL;
      }
      magGraphicsD3D12ReleaseResource(&state->frames[i].upload);
      magGraphicsD3D12ReleaseResource(&state->frames[i].backBuffer);
      state->frames[i].fenceValue = 0;
    }
}

static BOOL magGraphicsD3D12CreateUploadBuffer(
  MAGD3D12STATE* state,
  MAGD3D12FRAME* frame,
  UINT64 size)
{
    D3D12_HEAP_PROPERTIES heap = { 0 };
    D3D12_RESOURCE_DESC desc = { 0 };
    D3D12_RANGE readRange = { 0, 0 };
    HRESULT hr;

    heap.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap.CreationNodeMask = 1;
    heap.VisibleNodeMask = 1;
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    hr = ID3D12Device_CreateCommittedResource(
      state->device,
      &heap,
      D3D12_HEAP_FLAG_NONE,
      &desc,
      D3D12_RESOURCE_STATE_GENERIC_READ,
      NULL,
      &IID_ID3D12Resource,
      (void**)&frame->upload);
    if (FAILED(hr))
    {
      return FALSE;
    }

    hr = ID3D12Resource_Map(frame->upload, 0, &readRange, (void**)&frame->mappedUpload);
    return SUCCEEDED(hr);
}

static BOOL magGraphicsD3D12CreateFrameResources(MAGD3D12STATE* state, SIZE clientSize)
{
    D3D12_RESOURCE_DESC textureDesc = { 0 };
    UINT rows;
    UINT64 rowSize;
    UINT64 uploadSize;
    UINT i;

    textureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    textureDesc.Width = (UINT64)clientSize.cx;
    textureDesc.Height = (UINT)clientSize.cy;
    textureDesc.DepthOrArraySize = 1;
    textureDesc.MipLevels = 1;
    textureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;

    ID3D12Device_GetCopyableFootprints(
      state->device,
      &textureDesc,
      0,
      1,
      0,
      &state->uploadFootprint,
      &rows,
      &rowSize,
      &uploadSize);

    for (i = 0; i < MAG_D3D12_BUFFER_COUNT; ++i)
    {
      if (FAILED(IDXGISwapChain3_GetBuffer(
            state->swapChain,
            i,
            &IID_ID3D12Resource,
            (void**)&state->frames[i].backBuffer)) ||
          !magGraphicsD3D12CreateUploadBuffer(state, &state->frames[i], uploadSize))
      {
        magGraphicsD3D12ReleaseFrameResources(state);
        return FALSE;
      }
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    return TRUE;
}

static BOOL magGraphicsD3D12SelectDevice(MAGD3D12STATE* state)
{
    IDXGIAdapter1* adapter = NULL;
    UINT index;

    for (index = 0; ; ++index)
    {
      DXGI_ADAPTER_DESC1 desc;
      HRESULT hr;

      if (FAILED(IDXGIFactory4_EnumAdapters1(state->factory, index, &adapter)))
      {
        break;
      }

      IDXGIAdapter1_GetDesc1(adapter, &desc);
      hr = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        ? E_FAIL
        : D3D12CreateDevice(
            (IUnknown*)adapter,
            D3D_FEATURE_LEVEL_11_0,
            &IID_ID3D12Device,
            (void**)&state->device);
      IDXGIAdapter1_Release(adapter);
      adapter = NULL;
      if (SUCCEEDED(hr))
      {
        return TRUE;
      }
    }

    if (SUCCEEDED(IDXGIFactory4_EnumWarpAdapter(
          state->factory,
          &IID_IDXGIAdapter1,
          (void**)&adapter)) &&
        SUCCEEDED(D3D12CreateDevice(
          (IUnknown*)adapter,
          D3D_FEATURE_LEVEL_11_0,
          &IID_ID3D12Device,
          (void**)&state->device)))
    {
      state->warp = TRUE;
      IDXGIAdapter1_Release(adapter);
      return TRUE;
    }

    if (adapter)
    {
      IDXGIAdapter1_Release(adapter);
    }
    return FALSE;
}

static void magGraphicsD3D12Destroy(HWND hWnd, void* opaqueState);

static BOOL magGraphicsD3D12Create(HWND hWnd, SIZE clientSize, void** stateOut)
{
    MAGD3D12STATE* state;
    D3D12_COMMAND_QUEUE_DESC queueDesc = { 0 };
    DXGI_SWAP_CHAIN_DESC1 swapDesc = { 0 };
    IDXGISwapChain1* swapChain1 = NULL;
    UINT i;
    HRESULT hr;

    if (!stateOut || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *stateOut = NULL;
    SetLastError(ERROR_SUCCESS);

    state = (MAGD3D12STATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }
    state->nextFenceValue = 1;

    hr = CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&state->factory);
    if (FAILED(hr) || !magGraphicsD3D12SelectDevice(state))
    {
      SetLastError(FAILED(hr) ? (DWORD)hr : ERROR_NOT_SUPPORTED);
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (FAILED(ID3D12Device_CreateCommandQueue(
          state->device,
          &queueDesc,
          &IID_ID3D12CommandQueue,
          (void**)&state->queue)))
    {
      SetLastError(ERROR_INVALID_FUNCTION);
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    swapDesc.Width = (UINT)clientSize.cx;
    swapDesc.Height = (UINT)clientSize.cy;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = MAG_D3D12_BUFFER_COUNT;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

    hr = IDXGIFactory4_CreateSwapChainForComposition(
      state->factory,
      (IUnknown*)state->queue,
      &swapDesc,
      NULL,
      &swapChain1);
    if (SUCCEEDED(hr))
    {
      hr = IDXGISwapChain1_QueryInterface(
        swapChain1,
        &IID_IDXGISwapChain3,
        (void**)&state->swapChain);
      IDXGISwapChain1_Release(swapChain1);
    }
    if (FAILED(hr))
    {
      SetLastError((DWORD)hr);
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    if (!magDCompPresenterCreate(
          hWnd,
          (IUnknown*)state->swapChain,
          &state->composition))
    {
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    IDXGISwapChain2_SetMaximumFrameLatency((IDXGISwapChain2*)state->swapChain, 1);
    state->frameWaitHandle = IDXGISwapChain2_GetFrameLatencyWaitableObject((IDXGISwapChain2*)state->swapChain);

    for (i = 0; i < MAG_D3D12_BUFFER_COUNT; ++i)
    {
      if (FAILED(ID3D12Device_CreateCommandAllocator(
            state->device,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            &IID_ID3D12CommandAllocator,
            (void**)&state->frames[i].allocator)))
      {
        SetLastError(ERROR_INVALID_FUNCTION);
        magGraphicsD3D12Destroy(hWnd, state);
        return FALSE;
      }
    }

    hr = ID3D12Device_CreateCommandList(
      state->device,
      0,
      D3D12_COMMAND_LIST_TYPE_DIRECT,
      state->frames[0].allocator,
      NULL,
      &IID_ID3D12GraphicsCommandList,
      (void**)&state->commandList);
    if (FAILED(hr) || FAILED(ID3D12GraphicsCommandList_Close(state->commandList)) ||
        FAILED(ID3D12Device_CreateFence(
          state->device,
          0,
          D3D12_FENCE_FLAG_NONE,
          &IID_ID3D12Fence,
          (void**)&state->fence)))
    {
      SetLastError(FAILED(hr) ? (DWORD)hr : ERROR_INVALID_FUNCTION);
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    state->fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!state->fenceEvent || !magGraphicsD3D12CreateFrameResources(state, clientSize))
    {
      if (ERROR_SUCCESS == GetLastError())
      {
        SetLastError(ERROR_INVALID_DATA);
      }
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    *stateOut = state;
    return TRUE;
}

static void magGraphicsD3D12Destroy(HWND hWnd, void* opaqueState)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    UINT i;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return;
    }

    if (state->queue && state->fence && state->fenceEvent)
    {
      magGraphicsD3D12WaitIdle(state);
    }
    magDCompPresenterDestroy(state->composition);
    state->composition = NULL;
    if (state->swapChain)
    {
      DwmFlush();
    }
    magGraphicsD3D12ReleaseFrameResources(state);
    if (state->commandList)
    {
      ID3D12GraphicsCommandList_Release(state->commandList);
    }
    for (i = 0; i < MAG_D3D12_BUFFER_COUNT; ++i)
    {
      if (state->frames[i].allocator)
      {
        ID3D12CommandAllocator_Release(state->frames[i].allocator);
      }
    }
    if (state->fence)
    {
      ID3D12Fence_Release(state->fence);
    }
    if (state->fenceEvent)
    {
      CloseHandle(state->fenceEvent);
    }
    if (state->frameWaitHandle)
    {
      CloseHandle(state->frameWaitHandle);
    }
    if (state->swapChain)
    {
      IDXGISwapChain3_Release(state->swapChain);
    }
    if (state->queue)
    {
      ID3D12CommandQueue_Release(state->queue);
    }
    if (state->device)
    {
      ID3D12Device_Release(state->device);
    }
    if (state->factory)
    {
      IDXGIFactory4_Release(state->factory);
    }
    magGraphicsDestroyCpuCompositor(&state->compositor);
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsD3D12Resize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    HRESULT hr;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    if ((UINT)clientSize.cx == state->width && (UINT)clientSize.cy == state->height)
    {
      return TRUE;
    }

    if (!magGraphicsD3D12WaitIdle(state))
    {
      return FALSE;
    }
    magGraphicsD3D12ReleaseFrameResources(state);
    hr = IDXGISwapChain3_ResizeBuffers(
      state->swapChain,
      MAG_D3D12_BUFFER_COUNT,
      (UINT)clientSize.cx,
      (UINT)clientSize.cy,
      DXGI_FORMAT_B8G8R8A8_UNORM,
      DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT);
    return SUCCEEDED(hr) && magGraphicsD3D12CreateFrameResources(state, clientSize);
}

static BOOL magGraphicsD3D12SetPresentationEnabled(HWND hWnd, void* opaqueState, BOOL enabled)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    return state && magDCompPresenterSetEnabled(state->composition, enabled);
}

static BOOL magGraphicsD3D12Render(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    MAGPIXELBUFFER composed;
    MAGD3D12FRAME* current;
    D3D12_RESOURCE_BARRIER barriers[2] = { 0 };
    D3D12_TEXTURE_COPY_LOCATION destination = { 0 };
    D3D12_TEXTURE_COPY_LOCATION source = { 0 };
    ID3D12CommandList* commandLists[1];
    UINT frameIndex;
    UINT y;
    UINT64 fenceValue;
    HRESULT hr;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !frame || frame->width != state->width || frame->height != state->height ||
        !magGraphicsComposeFrame(&state->compositor, frame, ui, &composed))
    {
      return FALSE;
    }

    frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(state->swapChain);
    current = &state->frames[frameIndex];
    if (!magGraphicsD3D12WaitForFence(state, current->fenceValue))
    {
      return FALSE;
    }

    for (y = 0; y < composed.height; ++y)
    {
      CopyMemory(
        current->mappedUpload + state->uploadFootprint.Offset +
          (SIZE_T)y * state->uploadFootprint.Footprint.RowPitch,
        composed.pixels + (SIZE_T)y * composed.stride,
        composed.stride);
    }

    if (FAILED(ID3D12CommandAllocator_Reset(current->allocator)) ||
        FAILED(ID3D12GraphicsCommandList_Reset(state->commandList, current->allocator, NULL)))
    {
      return FALSE;
    }

    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = current->backBuffer;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1] = barriers[0];
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;

    destination.pResource = current->backBuffer;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;
    source.pResource = current->upload;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = state->uploadFootprint;

    ID3D12GraphicsCommandList_ResourceBarrier(state->commandList, 1, &barriers[0]);
    ID3D12GraphicsCommandList_CopyTextureRegion(
      state->commandList,
      &destination,
      0,
      0,
      0,
      &source,
      NULL);
    ID3D12GraphicsCommandList_ResourceBarrier(state->commandList, 1, &barriers[1]);
    if (FAILED(ID3D12GraphicsCommandList_Close(state->commandList)))
    {
      return FALSE;
    }

    commandLists[0] = (ID3D12CommandList*)state->commandList;
    ID3D12CommandQueue_ExecuteCommandLists(state->queue, 1, commandLists);
    hr = IDXGISwapChain3_Present(state->swapChain, 1, 0);
    if (FAILED(hr))
    {
      return FALSE;
    }

    fenceValue = state->nextFenceValue++;
    if (FAILED(ID3D12CommandQueue_Signal(state->queue, state->fence, fenceValue)))
    {
      return FALSE;
    }
    current->fenceValue = fenceValue;
    return TRUE;
}

static HANDLE magGraphicsD3D12GetFrameWaitHandle(void* opaqueState)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    return state ? state->frameWaitHandle : NULL;
}

const MAGGRAPHICSBACKEND g_magGraphicsD3D12Backend =
{
  GRAPHICS_API_D3D12,
  TEXT("Direct3D 12"),
  TRUE,
  magGraphicsD3D12IsAvailable,
  magGraphicsD3D12Create,
  magGraphicsD3D12Destroy,
  magGraphicsD3D12Resize,
  magGraphicsD3D12SetPresentationEnabled,
  magGraphicsD3D12Render,
  magGraphicsD3D12GetFrameWaitHandle,
};
