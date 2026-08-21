#include "framework.h"
#include "graphics.h"
#include "graphics_dcomp.h"
#include "graphics_presentation_manager.h"
#include "presentation.h"

#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#pragma comment(lib, "d3d12")
#pragma comment(lib, "dxgi")

#define MAG_D3D12_MAX_BUFFER_COUNT 16

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
  MAGPRESENTATIONMANAGERPRESENTER* presentationManager;
  ID3D12Device*         device;
  ID3D12CommandQueue*   queue;
  ID3D11Device*         on12Device11;
  ID3D11DeviceContext*  on12Context11;
  ID3D11On12Device2*    on12Device;
  IDXGISwapChain3*      swapChain;
  ID3D12GraphicsCommandList* commandList;
  ID3D12Fence*          fence;
  HANDLE                fenceEvent;
  HANDLE                frameWaitHandle;
  MAGD3D12FRAME         frames[MAG_D3D12_MAX_BUFFER_COUNT];
  D3D12_PLACED_SUBRESOURCE_FOOTPRINT uploadFootprint;
  UINT64                nextFenceValue;
  MAGCPUCOMPOSITOR      compositor;
  UINT                  width;
  UINT                  height;
  UINT                  capacityWidth;
  UINT                  capacityHeight;
  UINT64                resourceGeneration;
  UINT                  bufferCount;
  UINT                  syncInterval;
  UINT                  presentFlags;
  UINT                  swapChainFlags;
  BOOL                  warp;
  BOOL                  compositionHost;
  BOOL                  presentationManagerHost;
  BOOL                  waitableSwapChain;
  BOOL                  presentationEnabled;
} MAGD3D12STATE;

static BOOL magGraphicsD3D12IsAvailable(LPTSTR reason, UINT reasonCount)
{
    HMODULE module = LoadLibrary(TEXT("d3d12.dll"));

    if (!magGraphicsIsInputDesktop())
    {
      if (module)
      {
        FreeLibrary(module);
      }
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("Direct3D 12 composition presentation is unavailable on the private non-input test desktop."), reasonCount);
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

    for (i = 0; i < state->bufferCount; ++i)
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

    for (i = 0; i < state->bufferCount; ++i)
    {
      if ((!state->presentationManagerHost &&
           FAILED(IDXGISwapChain3_GetBuffer(
             state->swapChain,
             i,
             &IID_ID3D12Resource,
             (void**)&state->frames[i].backBuffer))) ||
          !magGraphicsD3D12CreateUploadBuffer(state, &state->frames[i], uploadSize))
      {
        magGraphicsD3D12ReleaseFrameResources(state);
        return FALSE;
      }
    }

    state->capacityWidth = (UINT)clientSize.cx;
    state->capacityHeight = (UINT)clientSize.cy;
    ++state->resourceGeneration;
    return TRUE;
}

static BOOL magGraphicsD3D12SelectDevice(
  MAGD3D12STATE* state,
  const MAGPRESENTATIONSETTINGS* presentation)
{
    IDXGIAdapter1* adapter = NULL;
    BOOL selected;

    if (!presentation)
    {
      return FALSE;
    }
    state->warp = MAG_HARDWARE_ADAPTER_WARP == presentation->hardware.mode;
    selected = state->warp
      ? SUCCEEDED(IDXGIFactory4_EnumWarpAdapter(
          state->factory,
          &IID_IDXGIAdapter1,
          (void**)&adapter))
      : magAdapterOpenDxgi(presentation->hardware.adapterLuid, &adapter);

    if (selected && adapter && SUCCEEDED(D3D12CreateDevice(
          (IUnknown*)adapter,
          D3D_FEATURE_LEVEL_11_0,
          &IID_ID3D12Device,
          (void**)&state->device)))
    {
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

static BOOL magGraphicsD3D12Create(
  HWND hWnd,
  SIZE clientSize,
  const struct MAGPRESENTATIONSETTINGS* presentation,
  void** stateOut)
{
    MAGD3D12STATE* state;
    D3D12_COMMAND_QUEUE_DESC queueDesc = { 0 };
    DXGI_SWAP_CHAIN_DESC1 swapDesc = { 0 };
    IDXGISwapChain1* swapChain1 = NULL;
    IDXGIFactory5* factory5 = NULL;
    SIZE reservoirSize;
    UINT i;
    BOOL allowTearing = FALSE;
    BOOL compositionHost;
    BOOL presentationManagerHost;
    BOOL waitableSwapChain;
    HRESULT hr;

    if (!stateOut || !presentation || clientSize.cx < 1 || clientSize.cy < 1 ||
        presentation->bufferCount < 2 ||
        presentation->bufferCount > MAG_D3D12_MAX_BUFFER_COUNT)
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
    state->bufferCount = presentation->bufferCount;
    compositionHost = MAG_HOST_DIRECTCOMPOSITION == presentation->host;
    presentationManagerHost = MAG_HOST_PRESENTATION_MANAGER == presentation->host;
    waitableSwapChain = !presentationManagerHost &&
      MAG_WAITABLE_SWAP_CHAIN_ENABLED == presentation->waitableSwapChainMode;
    state->compositionHost = compositionHost;
    state->presentationManagerHost = presentationManagerHost;
    state->waitableSwapChain = waitableSwapChain;
    reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);

    hr = CreateDXGIFactory1(&IID_IDXGIFactory4, (void**)&state->factory);
    if (FAILED(hr) || !magGraphicsD3D12SelectDevice(state, presentation))
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

    if (presentationManagerHost)
    {
      IUnknown* queues[] = { (IUnknown*)state->queue };
      TCHAR managerReason[256];

      hr = D3D11On12CreateDevice(
        (IUnknown*)state->device,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT |
          D3D11_CREATE_DEVICE_SINGLETHREADED |
          D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS,
        NULL,
        0,
        queues,
        ARRAYSIZE(queues),
        0,
        &state->on12Device11,
        &state->on12Context11,
        NULL);
      if (SUCCEEDED(hr))
      {
        hr = ID3D11Device_QueryInterface(
          state->on12Device11,
          &IID_ID3D11On12Device2,
          (void**)&state->on12Device);
      }
      if (SUCCEEDED(hr) &&
          !magPresentationManagerPresenterCreate(
            hWnd,
            state->on12Device11,
            reservoirSize,
            presentation,
            &state->presentationManager,
            managerReason,
            ARRAYSIZE(managerReason)))
      {
        hr = E_FAIL;
      }
      if (FAILED(hr))
      {
        SetLastError((DWORD)hr);
        magGraphicsD3D12Destroy(hWnd, state);
        return FALSE;
      }
      state->capacityWidth = (UINT)reservoirSize.cx;
      state->capacityHeight = (UINT)reservoirSize.cy;
      state->syncInterval = presentation->syncInterval;
      state->presentFlags = 0;
      state->presentationEnabled = TRUE;
    }
    else
    {
    swapDesc.Width = (UINT)reservoirSize.cx;
    swapDesc.Height = (UINT)reservoirSize.cy;
    swapDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapDesc.SampleDesc.Count = 1;
    swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapDesc.BufferCount = state->bufferCount;
    swapDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapDesc.Scaling = DXGI_SCALING_STRETCH;
    swapDesc.AlphaMode = compositionHost &&
      MAG_LAYER_ALPHA_OPAQUE != presentation->alphaMode
      ? DXGI_ALPHA_MODE_PREMULTIPLIED
      : DXGI_ALPHA_MODE_IGNORE;
    swapDesc.Flags = waitableSwapChain
      ? DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
      : 0;

    if (presentation->allowTearing && 0 == presentation->syncInterval &&
        SUCCEEDED(IDXGIFactory4_QueryInterface(
          state->factory,
          &IID_IDXGIFactory5,
          (void**)&factory5)))
    {
      if (FAILED(IDXGIFactory5_CheckFeatureSupport(
            factory5,
            DXGI_FEATURE_PRESENT_ALLOW_TEARING,
            &allowTearing,
            sizeof(allowTearing))))
      {
        allowTearing = FALSE;
      }
      IDXGIFactory5_Release(factory5);
      if (allowTearing)
      {
        swapDesc.Flags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
      }
    }

    if (compositionHost)
    {
      hr = IDXGIFactory4_CreateSwapChainForComposition(
        state->factory,
        (IUnknown*)state->queue,
        &swapDesc,
        NULL,
        &swapChain1);
    }
    else
    {
      hr = IDXGIFactory4_CreateSwapChainForHwnd(
        state->factory,
        (IUnknown*)state->queue,
        hWnd,
        &swapDesc,
        NULL,
        NULL,
        &swapChain1);
    }
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

    if (FAILED(IDXGISwapChain2_SetSourceSize(
          (IDXGISwapChain2*)state->swapChain,
          (UINT)clientSize.cx,
          (UINT)clientSize.cy)) ||
        (compositionHost &&
         !magDCompPresenterCreate(
           hWnd,
           (IUnknown*)state->swapChain,
           &state->composition)))
    {
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    if (waitableSwapChain)
    {
      if (FAILED(IDXGISwapChain2_SetMaximumFrameLatency(
            (IDXGISwapChain2*)state->swapChain,
            presentation->maximumFrameLatency)))
      {
        magGraphicsD3D12Destroy(hWnd, state);
        return FALSE;
      }
      state->frameWaitHandle = IDXGISwapChain2_GetFrameLatencyWaitableObject(
        (IDXGISwapChain2*)state->swapChain);
      if (!state->frameWaitHandle)
      {
        magGraphicsD3D12Destroy(hWnd, state);
        return FALSE;
      }
    }
    state->syncInterval = presentation->syncInterval;
    state->presentFlags = allowTearing ? DXGI_PRESENT_ALLOW_TEARING : 0;
    state->swapChainFlags = swapDesc.Flags;
    state->presentationEnabled = TRUE;
    }

    for (i = 0; i < state->bufferCount; ++i)
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
    if (!state->fenceEvent || !magGraphicsD3D12CreateFrameResources(state, reservoirSize) ||
        !magGraphicsReserveCpuCompositor(
          &state->compositor,
          (UINT)reservoirSize.cx,
          (UINT)reservoirSize.cy))
    {
      if (ERROR_SUCCESS == GetLastError())
      {
        SetLastError(ERROR_INVALID_DATA);
      }
      magGraphicsD3D12Destroy(hWnd, state);
      return FALSE;
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
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
    magPresentationManagerPresenterDestroy(state->presentationManager);
    state->presentationManager = NULL;
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
    for (i = 0; i < state->bufferCount; ++i)
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
    if (state->on12Context11)
    {
      ID3D11DeviceContext_ClearState(state->on12Context11);
      ID3D11DeviceContext_Flush(state->on12Context11);
      ID3D11DeviceContext_Release(state->on12Context11);
    }
    if (state->on12Device)
    {
      ID3D11On12Device2_Release(state->on12Device);
    }
    if (state->on12Device11)
    {
      ID3D11Device_Release(state->on12Device11);
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

    if (!state || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    if (state->presentationManagerHost)
    {
      return magPresentationManagerPresenterResize(
        state->presentationManager,
        clientSize);
    }
    if (state->width <= state->capacityWidth && state->height <= state->capacityHeight)
    {
      return SUCCEEDED(IDXGISwapChain2_SetSourceSize(
        (IDXGISwapChain2*)state->swapChain,
        state->width,
        state->height));
    }

    {
    SIZE reservoirSize = magGraphicsChooseReservoirSize(hWnd, clientSize);
    reservoirSize.cx = max(reservoirSize.cx, (LONG)state->capacityWidth);
    reservoirSize.cy = max(reservoirSize.cy, (LONG)state->capacityHeight);
    if (!magGraphicsD3D12WaitIdle(state))
    {
      return FALSE;
    }
    magGraphicsD3D12ReleaseFrameResources(state);
    hr = IDXGISwapChain3_ResizeBuffers(
      state->swapChain,
      state->bufferCount,
      (UINT)reservoirSize.cx,
      (UINT)reservoirSize.cy,
      DXGI_FORMAT_B8G8R8A8_UNORM,
      state->swapChainFlags);
    return SUCCEEDED(hr) &&
      magGraphicsD3D12CreateFrameResources(state, reservoirSize) &&
      SUCCEEDED(IDXGISwapChain2_SetSourceSize(
        (IDXGISwapChain2*)state->swapChain,
        state->width,
        state->height));
    }
}

static BOOL magGraphicsD3D12SetPresentationEnabled(HWND hWnd, void* opaqueState, BOOL enabled)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state)
    {
      return FALSE;
    }
    if (state->presentationManagerHost &&
        !magPresentationManagerPresenterSetEnabled(
          state->presentationManager,
          enabled))
    {
      return FALSE;
    }
    if (state->compositionHost &&
        !magDCompPresenterSetEnabled(state->composition, enabled))
    {
      return FALSE;
    }
    state->presentationEnabled = enabled;
    return TRUE;
}

static BOOL magGraphicsD3D12Render(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  const MAGPRESENTINTENT* intent)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    MAGPIXELBUFFER composed;
    MAGD3D12FRAME* current;
    D3D12_RESOURCE_BARRIER barriers[2] = { 0 };
    D3D12_TEXTURE_COPY_LOCATION destination = { 0 };
    D3D12_TEXTURE_COPY_LOCATION source = { 0 };
    D3D12_BOX sourceBox;
    ID3D12CommandList* commandLists[1];
    UINT frameIndex;
    UINT y;
    UINT64 fenceValue;
    HRESULT hr;
    ID3D11Texture2D* presentationTexture = NULL;
    ID3D12Resource* presentationResource = NULL;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !frame || frame->width != state->width || frame->height != state->height ||
        !magGraphicsComposeFrame(&state->compositor, frame, ui, &composed))
    {
      return FALSE;
    }

    if (state->presentationManagerHost)
    {
      if (!magPresentationManagerPresenterAcquire(
            state->presentationManager,
            !intent || intent->synchronize,
            &presentationTexture,
            &frameIndex) ||
          FAILED(ID3D11On12Device2_UnwrapUnderlyingResource(
            state->on12Device,
            (ID3D11Resource*)presentationTexture,
            state->queue,
            &IID_ID3D12Resource,
            (void**)&presentationResource)))
      {
        if (presentationTexture)
        {
          ID3D11Texture2D_Release(presentationTexture);
        }
        return FALSE;
      }
    }
    else
    {
      frameIndex = IDXGISwapChain3_GetCurrentBackBufferIndex(state->swapChain);
    }
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
        (SIZE_T)composed.width * 4U);
    }

    if (FAILED(ID3D12CommandAllocator_Reset(current->allocator)) ||
        FAILED(ID3D12GraphicsCommandList_Reset(state->commandList, current->allocator, NULL)))
    {
      return FALSE;
    }

    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barriers[0].Transition.pResource = state->presentationManagerHost
      ? presentationResource
      : current->backBuffer;
    barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barriers[0].Transition.StateBefore = state->presentationManagerHost
      ? D3D12_RESOURCE_STATE_COMMON
      : D3D12_RESOURCE_STATE_PRESENT;
    barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1] = barriers[0];
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barriers[1].Transition.StateAfter = state->presentationManagerHost
      ? D3D12_RESOURCE_STATE_COMMON
      : D3D12_RESOURCE_STATE_PRESENT;

    destination.pResource = state->presentationManagerHost
      ? presentationResource
      : current->backBuffer;
    destination.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    destination.SubresourceIndex = 0;
    source.pResource = current->upload;
    source.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    source.PlacedFootprint = state->uploadFootprint;
    sourceBox.left = 0;
    sourceBox.top = 0;
    sourceBox.front = 0;
    sourceBox.right = state->width;
    sourceBox.bottom = state->height;
    sourceBox.back = 1;

    ID3D12GraphicsCommandList_ResourceBarrier(state->commandList, 1, &barriers[0]);
    ID3D12GraphicsCommandList_CopyTextureRegion(
      state->commandList,
      &destination,
      0,
      0,
      0,
      &source,
      &sourceBox);
    ID3D12GraphicsCommandList_ResourceBarrier(state->commandList, 1, &barriers[1]);
    if (FAILED(ID3D12GraphicsCommandList_Close(state->commandList)))
    {
      return FALSE;
    }

    commandLists[0] = (ID3D12CommandList*)state->commandList;
    ID3D12CommandQueue_ExecuteCommandLists(state->queue, 1, commandLists);
    if (state->presentationManagerHost)
    {
      fenceValue = state->nextFenceValue++;
      if (FAILED(ID3D12CommandQueue_Signal(state->queue, state->fence, fenceValue)) ||
          FAILED(ID3D11On12Device2_ReturnUnderlyingResource(
            state->on12Device,
            (ID3D11Resource*)presentationTexture,
            1,
            &fenceValue,
            &state->fence)))
      {
        ID3D12Resource_Release(presentationResource);
        ID3D11Texture2D_Release(presentationTexture);
        return FALSE;
      }
      current->fenceValue = fenceValue;
      ID3D12Resource_Release(presentationResource);
      ID3D11Texture2D_Release(presentationTexture);
      return magPresentationManagerPresenterPresent(
        state->presentationManager,
        frameIndex,
        (SIZE){ (LONG)state->width, (LONG)state->height },
        intent);
    }
    if (intent && intent->restartSequence)
    {
      HRESULT first = IDXGISwapChain3_Present(
        state->swapChain,
        0,
        state->presentFlags | DXGI_PRESENT_RESTART |
          (state->waitableSwapChain ? DXGI_PRESENT_DO_NOT_WAIT : 0));

      if (!intent->synchronize && !state->waitableSwapChain)
      {
        return SUCCEEDED(first);
      }
      hr = IDXGISwapChain3_Present(
        state->swapChain,
        intent->synchronize ? 1 : 0,
        intent->synchronize
          ? DXGI_PRESENT_DO_NOT_SEQUENCE
          : state->presentFlags | DXGI_PRESENT_DO_NOT_WAIT);
      if (FAILED(first) && DXGI_ERROR_WAS_STILL_DRAWING != first)
      {
        return FALSE;
      }
    }
    else if (intent && !intent->synchronize)
    {
      hr = IDXGISwapChain3_Present(
        state->swapChain,
        0,
        state->presentFlags |
          (state->waitableSwapChain ? DXGI_PRESENT_DO_NOT_WAIT : 0));
    }
    else
    {
      hr = IDXGISwapChain3_Present(
        state->swapChain,
        state->syncInterval,
        state->presentFlags);
    }
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
    if (!state)
    {
      return NULL;
    }
    return state->presentationManagerHost
      ? magPresentationManagerPresenterGetFrameWaitHandle(
          state->presentationManager)
      : state->frameWaitHandle;
}

static UINT64 magGraphicsD3D12GetResourceGeneration(void* opaqueState)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    return state
      ? state->resourceGeneration + state->compositor.generation +
        magPresentationManagerPresenterGetResourceGeneration(
          state->presentationManager)
      : 0;
}

static BOOL magGraphicsD3D12GetNextEstimatedFrameTime(
  void* opaqueState,
  LONGLONG* frameTime)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;
    if (!state)
    {
      return FALSE;
    }
    return state->presentationManagerHost
      ? magPresentationManagerPresenterGetNextEstimatedFrameTime(
          state->presentationManager,
          frameTime)
      : magDCompPresenterGetNextEstimatedFrameTime(
          state->composition,
          frameTime);
}

static BOOL magGraphicsD3D12GetObservedPresentationTarget(
  void* opaqueState,
  UINT* target)
{
    MAGD3D12STATE* state = (MAGD3D12STATE*)opaqueState;

    if (state && state->presentationManagerHost)
    {
      return magPresentationManagerPresenterGetObservedTarget(
        state->presentationManager,
        target);
    }
    return FALSE;
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
  magGraphicsD3D12GetResourceGeneration,
  magGraphicsD3D12GetNextEstimatedFrameTime,
  magGraphicsD3D12GetObservedPresentationTarget,
};
