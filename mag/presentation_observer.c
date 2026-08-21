#include "framework.h"
#include "presentation_observer.h"

#include <evntrace.h>
#include <evntcons.h>
#include <tdh.h>

#pragma comment(lib, "advapi32")
#pragma comment(lib, "tdh")

#define MAG_OBSERVER_PRESENT_CAPACITY 64
#define MAG_OBSERVER_MAPPING_CAPACITY 128
#define MAG_OBSERVER_SESSION_NAME_COUNT 96

#define MAG_DXGK_EVENT_BLIT 0x00a6
#define MAG_DXGK_EVENT_FLIP 0x00a8
#define MAG_DXGK_EVENT_PRESENT_HISTORY_START 0x00ab
#define MAG_DXGK_EVENT_PRESENT_HISTORY_INFO 0x00ac
#define MAG_DXGK_EVENT_QUEUE_PACKET_START 0x00b2
#define MAG_DXGK_EVENT_QUEUE_PACKET_STOP 0x00b4
#define MAG_DXGK_EVENT_PRESENT_INFO 0x00b8
#define MAG_DXGK_EVENT_PRESENT_HISTORY_DETAILED_START 0x00d7
#define MAG_DXGK_EVENT_QUEUE_PACKET_START_2 0x00f4
#define MAG_DXGK_EVENT_FLIP_MPO 0x00fc
#define MAG_DXGK_EVENT_MMIO_FLIP_MPO 0x0103
#define MAG_DXGK_EVENT_INDEPENDENT_FLIP 0x010a
#define MAG_DXGK_EVENT_VSYNC_DPC_MPO 0x0111
#define MAG_DXGK_EVENT_HSYNC_DPC_MPO 0x017e
#define MAG_DXGK_EVENT_MMIO_FLIP_MPO3 0x0182

#define MAG_WIN32K_EVENT_TOKEN_COMPOSITION 0x00c9
#define MAG_WIN32K_EVENT_TOKEN_STATE 0x012d

#define MAG_D3DKMT_PM_UNINITIALIZED 0U
#define MAG_D3DKMT_PM_REDIRECTED_GDI 1U
#define MAG_D3DKMT_PM_REDIRECTED_FLIP 2U
#define MAG_D3DKMT_PM_REDIRECTED_BLT 3U
#define MAG_D3DKMT_PM_REDIRECTED_VISTABLT 4U
#define MAG_D3DKMT_PM_REDIRECTED_GDI_SYSMEM 6U
#define MAG_D3DKMT_PM_REDIRECTED_COMPOSITION 7U

#define MAG_WIN32K_TOKEN_STATE_IN_FRAME 3U

static const GUID MAG_PROVIDER_DXGKRNL =
  { 0x802ec45a, 0x1e99, 0x4b83, { 0x99, 0x20, 0x87, 0xc9, 0x82, 0x77, 0xba, 0x9d } };
static const GUID MAG_PROVIDER_WIN32K =
  { 0x8c416c79, 0xd49b, 0x4f01, { 0xa4, 0x67, 0xe5, 0x6d, 0x3a, 0xa8, 0x23, 0x4c } };

typedef struct MAGOBSERVEDPRESENT
{
  BOOL used;
  DWORD threadId;
  HWND hWnd;
  UINT64 generation;
  UINT64 qpc;
  UINT64 historyToken;
  UINT64 compositionSurfaceLuid;
  UINT64 compositionPresentCount;
  UINT64 compositionBindId;
  UINT submitSequence;
  MAGPRESENTATIONTARGET target;
} MAGOBSERVEDPRESENT;

typedef struct MAGOBSERVERSUBMITMAPPING
{
  BOOL used;
  UINT submitSequence;
  UINT presentIndex;
  UINT64 presentGeneration;
  UINT64 qpc;
} MAGOBSERVERSUBMITMAPPING;

typedef struct MAGOBSERVERTOKENMAPPING
{
  BOOL used;
  UINT64 first;
  UINT64 second;
  UINT64 third;
  UINT presentIndex;
  UINT64 presentGeneration;
  UINT64 qpc;
} MAGOBSERVERTOKENMAPPING;

typedef struct MAGPRESENTATIONOBSERVERCORE
{
  HWND expectedWindow;
  DWORD processId;
  UINT64 resetQpc;
  UINT64 nextPresentGeneration;
  UINT64 nextObservationSequence;
  MAGPRESENTATIONTARGET latestTarget;
  UINT64 latestSequence;
  UINT64 latestQpc;
  BOOL latestValid;
  MAGOBSERVEDPRESENT presents[MAG_OBSERVER_PRESENT_CAPACITY];
  MAGOBSERVERSUBMITMAPPING submits[MAG_OBSERVER_MAPPING_CAPACITY];
  MAGOBSERVERTOKENMAPPING historyTokens[MAG_OBSERVER_MAPPING_CAPACITY];
  MAGOBSERVERTOKENMAPPING compositionTokens[MAG_OBSERVER_MAPPING_CAPACITY];
} MAGPRESENTATIONOBSERVERCORE;

typedef struct MAGPRESENTATIONOBSERVER
{
  SRWLOCK lock;
  MAGPRESENTATIONOBSERVERCORE core;
  TRACEHANDLE session;
  TRACEHANDLE trace;
  HANDLE thread;
  EVENT_TRACE_PROPERTIES* properties;
  WCHAR sessionName[MAG_OBSERVER_SESSION_NAME_COUNT];
  DWORD error;
  DWORD startTraceError;
  DWORD dxgKrnlError;
  DWORD win32kError;
  DWORD openTraceError;
  BOOL active;
  BOOL dxgKrnlEnabled;
  BOOL win32kEnabled;
} MAGPRESENTATIONOBSERVER;

static MAGPRESENTATIONOBSERVER g_observer =
{
  SRWLOCK_INIT
};

static BOOL magObserverGuidEqual(const GUID* left, const GUID* right)
{
    return left && right && 0 == memcmp(left, right, sizeof(*left));
}

static UINT64 magObserverQpc(void)
{
    LARGE_INTEGER value;

    return QueryPerformanceCounter(&value) ? (UINT64)value.QuadPart : 0;
}

static void magObserverCoreReset(
  MAGPRESENTATIONOBSERVERCORE* core,
  HWND hWnd,
  UINT64 qpc)
{
    if (!core)
    {
      return;
    }
    core->expectedWindow = hWnd;
    core->processId = GetCurrentProcessId();
    core->resetQpc = qpc;
    core->latestTarget = MAG_PRESENT_AUTO;
    core->latestValid = FALSE;
    core->latestQpc = 0;
    ZeroMemory(core->presents, sizeof(core->presents));
    ZeroMemory(core->submits, sizeof(core->submits));
    ZeroMemory(core->historyTokens, sizeof(core->historyTokens));
    ZeroMemory(core->compositionTokens, sizeof(core->compositionTokens));
}

static UINT magObserverPresentIndex(
  const MAGPRESENTATIONOBSERVERCORE* core,
  const MAGOBSERVEDPRESENT* present)
{
    return (UINT)(present - core->presents);
}

static MAGOBSERVEDPRESENT* magObserverBeginPresent(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD threadId,
  UINT64 qpc)
{
    MAGOBSERVEDPRESENT* selected = NULL;
    UINT index;

    if (!core || qpc < core->resetQpc)
    {
      return NULL;
    }
    for (index = 0; index < ARRAYSIZE(core->presents); ++index)
    {
      MAGOBSERVEDPRESENT* candidate = &core->presents[index];

      if (!candidate->used)
      {
        selected = candidate;
        break;
      }
      if (!selected || candidate->qpc < selected->qpc)
      {
        selected = candidate;
      }
    }
    if (!selected)
    {
      return NULL;
    }
    ZeroMemory(selected, sizeof(*selected));
    selected->used = TRUE;
    selected->threadId = threadId;
    selected->qpc = qpc;
    selected->generation = ++core->nextPresentGeneration;
    selected->target = MAG_PRESENT_AUTO;
    return selected;
}

static MAGOBSERVEDPRESENT* magObserverFindThreadPresent(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD threadId)
{
    MAGOBSERVEDPRESENT* selected = NULL;
    UINT index;

    if (!core)
    {
      return NULL;
    }
    for (index = 0; index < ARRAYSIZE(core->presents); ++index)
    {
      MAGOBSERVEDPRESENT* candidate = &core->presents[index];

      if (candidate->used && candidate->threadId == threadId &&
          (!selected || candidate->qpc > selected->qpc))
      {
        selected = candidate;
      }
    }
    return selected;
}

static MAGOBSERVEDPRESENT* magObserverFindOrBeginPresent(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc)
{
    MAGOBSERVEDPRESENT* present;

    if (!core || processId != core->processId || qpc < core->resetQpc)
    {
      return NULL;
    }
    present = magObserverFindThreadPresent(core, threadId);
    return present ? present : magObserverBeginPresent(core, threadId, qpc);
}

static void magObserverPublish(
  MAGPRESENTATIONOBSERVERCORE* core,
  MAGOBSERVEDPRESENT* present,
  MAGPRESENTATIONTARGET target,
  UINT64 qpc)
{
    if (!core || !present || target <= MAG_PRESENT_AUTO ||
        target >= MAG_PRESENT_COUNT || qpc < core->resetQpc ||
        (present->hWnd && core->expectedWindow &&
         present->hWnd != core->expectedWindow))
    {
      return;
    }
    present->target = target;
    present->qpc = max(present->qpc, qpc);
    core->latestTarget = target;
    core->latestQpc = qpc;
    core->latestSequence = ++core->nextObservationSequence;
    core->latestValid = TRUE;
}

static MAGOBSERVERSUBMITMAPPING* magObserverChooseSubmitMapping(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT submitSequence)
{
    MAGOBSERVERSUBMITMAPPING* selected = NULL;
    UINT index;

    for (index = 0; index < ARRAYSIZE(core->submits); ++index)
    {
      MAGOBSERVERSUBMITMAPPING* mapping = &core->submits[index];

      if (mapping->used && mapping->submitSequence == submitSequence)
      {
        return mapping;
      }
      if (!mapping->used)
      {
        return mapping;
      }
      if (!selected || mapping->qpc < selected->qpc)
      {
        selected = mapping;
      }
    }
    return selected;
}

static void magObserverMapSubmit(
  MAGPRESENTATIONOBSERVERCORE* core,
  MAGOBSERVEDPRESENT* present,
  UINT submitSequence,
  UINT64 qpc)
{
    MAGOBSERVERSUBMITMAPPING* mapping;

    if (!core || !present || !submitSequence)
    {
      return;
    }
    mapping = magObserverChooseSubmitMapping(core, submitSequence);
    if (!mapping)
    {
      return;
    }
    mapping->used = TRUE;
    mapping->submitSequence = submitSequence;
    mapping->presentIndex = magObserverPresentIndex(core, present);
    mapping->presentGeneration = present->generation;
    mapping->qpc = qpc;
    present->submitSequence = submitSequence;
}

static MAGOBSERVEDPRESENT* magObserverFindSubmit(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT submitSequence)
{
    UINT index;

    if (!core || !submitSequence)
    {
      return NULL;
    }
    for (index = 0; index < ARRAYSIZE(core->submits); ++index)
    {
      const MAGOBSERVERSUBMITMAPPING* mapping = &core->submits[index];

      if (mapping->used && mapping->submitSequence == submitSequence &&
          mapping->presentIndex < ARRAYSIZE(core->presents))
      {
        MAGOBSERVEDPRESENT* present = &core->presents[mapping->presentIndex];

        if (present->used &&
            present->generation == mapping->presentGeneration)
        {
          return present;
        }
      }
    }
    return NULL;
}

static MAGOBSERVERTOKENMAPPING* magObserverChooseTokenMapping(
  MAGOBSERVERTOKENMAPPING* mappings,
  UINT count,
  UINT64 first,
  UINT64 second,
  UINT64 third)
{
    MAGOBSERVERTOKENMAPPING* selected = NULL;
    UINT index;

    for (index = 0; index < count; ++index)
    {
      MAGOBSERVERTOKENMAPPING* mapping = &mappings[index];

      if (mapping->used && mapping->first == first &&
          mapping->second == second && mapping->third == third)
      {
        return mapping;
      }
      if (!mapping->used)
      {
        return mapping;
      }
      if (!selected || mapping->qpc < selected->qpc)
      {
        selected = mapping;
      }
    }
    return selected;
}

static void magObserverMapToken(
  MAGPRESENTATIONOBSERVERCORE* core,
  MAGOBSERVERTOKENMAPPING* mappings,
  UINT mappingCount,
  MAGOBSERVEDPRESENT* present,
  UINT64 first,
  UINT64 second,
  UINT64 third,
  UINT64 qpc)
{
    MAGOBSERVERTOKENMAPPING* mapping;

    if (!core || !mappings || !present || !first)
    {
      return;
    }
    mapping = magObserverChooseTokenMapping(
      mappings,
      mappingCount,
      first,
      second,
      third);
    if (!mapping)
    {
      return;
    }
    mapping->used = TRUE;
    mapping->first = first;
    mapping->second = second;
    mapping->third = third;
    mapping->presentIndex = magObserverPresentIndex(core, present);
    mapping->presentGeneration = present->generation;
    mapping->qpc = qpc;
}

static MAGOBSERVEDPRESENT* magObserverFindToken(
  MAGPRESENTATIONOBSERVERCORE* core,
  const MAGOBSERVERTOKENMAPPING* mappings,
  UINT mappingCount,
  UINT64 first,
  UINT64 second,
  UINT64 third)
{
    UINT index;

    if (!core || !mappings || !first)
    {
      return NULL;
    }
    for (index = 0; index < mappingCount; ++index)
    {
      const MAGOBSERVERTOKENMAPPING* mapping = &mappings[index];

      if (mapping->used && mapping->first == first &&
          mapping->second == second && mapping->third == third &&
          mapping->presentIndex < ARRAYSIZE(core->presents))
      {
        MAGOBSERVEDPRESENT* present = &core->presents[mapping->presentIndex];

        if (present->used &&
            present->generation == mapping->presentGeneration)
        {
          return present;
        }
      }
    }
    return NULL;
}

static void magObserverOnFlip(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc)
{
    MAGOBSERVEDPRESENT* present;

    if (!core || processId != core->processId)
    {
      return;
    }
    present = magObserverBeginPresent(core, threadId, qpc);
    magObserverPublish(core, present, MAG_PRESENT_HARDWARE_LEGACY_FLIP, qpc);
}

static void magObserverOnBlit(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc,
  HWND hWnd,
  BOOL redirected)
{
    MAGOBSERVEDPRESENT* present;

    if (!core || processId != core->processId ||
        (hWnd && core->expectedWindow && hWnd != core->expectedWindow))
    {
      return;
    }
    present = magObserverBeginPresent(core, threadId, qpc);
    if (present)
    {
      present->hWnd = hWnd;
      present->target = redirected
        ? MAG_PRESENT_COMPOSED_COPY_CPU_GDI
        : MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER;
    }
}

static void magObserverOnPresentHistory(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc,
  UINT64 token,
  UINT model)
{
    MAGOBSERVEDPRESENT* present = magObserverFindOrBeginPresent(
      core,
      processId,
      threadId,
      qpc);

    if (!present)
    {
      return;
    }
    present->qpc = qpc;
    present->historyToken = token;
    magObserverMapToken(
      core,
      core->historyTokens,
      ARRAYSIZE(core->historyTokens),
      present,
      token,
      0,
      0,
      qpc);

    if (MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER == present->target &&
        (MAG_D3DKMT_PM_UNINITIALIZED == model ||
         MAG_D3DKMT_PM_REDIRECTED_BLT == model))
    {
      present->target = MAG_PRESENT_COMPOSED_COPY_GPU_GDI;
    }
    else if (MAG_PRESENT_AUTO == present->target)
    {
      switch (model)
      {
      case MAG_D3DKMT_PM_REDIRECTED_GDI:
      case MAG_D3DKMT_PM_REDIRECTED_VISTABLT:
      case MAG_D3DKMT_PM_REDIRECTED_GDI_SYSMEM:
        present->target = MAG_PRESENT_COMPOSED_COPY_CPU_GDI;
        break;
      case MAG_D3DKMT_PM_REDIRECTED_BLT:
        present->target = MAG_PRESENT_COMPOSED_COPY_GPU_GDI;
        break;
      case MAG_D3DKMT_PM_REDIRECTED_FLIP:
      case MAG_D3DKMT_PM_REDIRECTED_COMPOSITION:
      default:
        present->target = MAG_PRESENT_COMPOSED_FLIP;
        break;
      }
    }
}

static void magObserverOnPresentHistoryInfo(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT64 qpc,
  UINT64 token,
  BOOL publishComposedFlip)
{
    MAGOBSERVEDPRESENT* present = magObserverFindToken(
      core,
      core->historyTokens,
      ARRAYSIZE(core->historyTokens),
      token,
      0,
      0);

    if (present && (MAG_PRESENT_COMPOSED_COPY_GPU_GDI == present->target ||
                    MAG_PRESENT_COMPOSED_COPY_CPU_GDI == present->target))
    {
      magObserverPublish(core, present, present->target, qpc);
    }
    else if (present && publishComposedFlip &&
             MAG_PRESENT_COMPOSED_FLIP == present->target)
    {
      /* A DxgKrnl-only session cannot receive the Win32k token-state event
         that distinguishes composed from independent flip.  Publish the
         composed result here; a later DxgKrnl IndependentFlip/MPO event can
         still promote the same present to the hardware path. */
      magObserverPublish(core, present, present->target, qpc);
    }
}

static void magObserverOnPresentInfo(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc,
  HWND hWnd)
{
    MAGOBSERVEDPRESENT* present;

    if (!core || processId != core->processId)
    {
      return;
    }
    present = magObserverFindThreadPresent(core, threadId);
    if (!present)
    {
      return;
    }
    present->hWnd = hWnd;
    if (MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER == present->target ||
        MAG_PRESENT_COMPOSED_COPY_GPU_GDI == present->target ||
        MAG_PRESENT_COMPOSED_COPY_CPU_GDI == present->target)
    {
      magObserverPublish(core, present, present->target, qpc);
    }
}

static void magObserverOnQueueSubmit(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc,
  UINT submitSequence)
{
    MAGOBSERVEDPRESENT* present = magObserverFindOrBeginPresent(
      core,
      processId,
      threadId,
      qpc);

    magObserverMapSubmit(core, present, submitSequence, qpc);
}

static void magObserverOnQueueComplete(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT64 qpc,
  UINT submitSequence)
{
    MAGOBSERVEDPRESENT* present = magObserverFindSubmit(core, submitSequence);

    if (present && (MAG_PRESENT_HARDWARE_LEGACY_FLIP == present->target ||
                    MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER == present->target))
    {
      magObserverPublish(core, present, present->target, qpc);
    }
}

static void magObserverOnIndependentFlip(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT64 qpc,
  UINT submitSequence)
{
    MAGOBSERVEDPRESENT* present = magObserverFindSubmit(core, submitSequence);

    if (present)
    {
      magObserverPublish(
        core,
        present,
        MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP,
        qpc);
    }
}

static void magObserverOnMultiPlaneSync(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT64 qpc,
  UINT submitSequence,
  UINT activePlaneCount)
{
    MAGOBSERVEDPRESENT* present = magObserverFindSubmit(core, submitSequence);

    if (!present)
    {
      return;
    }
    if (activePlaneCount > 1 &&
        (MAG_PRESENT_COMPOSED_FLIP == present->target ||
         MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP == present->target))
    {
      magObserverPublish(
        core,
        present,
        MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP,
        qpc);
    }
    else if (MAG_PRESENT_AUTO != present->target)
    {
      magObserverPublish(core, present, present->target, qpc);
    }
}

static void magObserverOnCompositionToken(
  MAGPRESENTATIONOBSERVERCORE* core,
  DWORD processId,
  DWORD threadId,
  UINT64 qpc,
  UINT64 surfaceLuid,
  UINT64 presentCount,
  UINT64 bindId)
{
    MAGOBSERVEDPRESENT* present;

    if (!core || processId != core->processId)
    {
      return;
    }
    present = magObserverBeginPresent(core, threadId, qpc);
    if (!present)
    {
      return;
    }
    present->compositionSurfaceLuid = surfaceLuid;
    present->compositionPresentCount = presentCount;
    present->compositionBindId = bindId;
    present->target = MAG_PRESENT_COMPOSED_FLIP;
    magObserverMapToken(
      core,
      core->compositionTokens,
      ARRAYSIZE(core->compositionTokens),
      present,
      surfaceLuid,
      presentCount,
      bindId,
      qpc);
}

static void magObserverOnTokenState(
  MAGPRESENTATIONOBSERVERCORE* core,
  UINT64 qpc,
  UINT64 surfaceLuid,
  UINT64 presentCount,
  UINT64 bindId,
  UINT newState,
  BOOL independentFlip)
{
    MAGOBSERVEDPRESENT* present;

    if (MAG_WIN32K_TOKEN_STATE_IN_FRAME != newState)
    {
      return;
    }
    present = magObserverFindToken(
      core,
      core->compositionTokens,
      ARRAYSIZE(core->compositionTokens),
      surfaceLuid,
      presentCount,
      bindId);
    if (present)
    {
      magObserverPublish(
        core,
        present,
        independentFlip
          ? MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP
          : MAG_PRESENT_COMPOSED_FLIP,
        qpc);
    }
}

static BOOL magObserverReadProperty(
  const EVENT_RECORD* record,
  LPCWSTR name,
  ULONG arrayIndex,
  void* value,
  ULONG valueCapacity,
  ULONG* valueSize)
{
    PROPERTY_DATA_DESCRIPTOR descriptor = { 0 };
    ULONG size = 0;
    ULONG status;

    if (!record || !name || !value || !valueCapacity)
    {
      return FALSE;
    }
    descriptor.PropertyName = (ULONGLONG)(ULONG_PTR)name;
    descriptor.ArrayIndex = arrayIndex;
    status = TdhGetPropertySize(
      (PEVENT_RECORD)record,
      0,
      NULL,
      1,
      &descriptor,
      &size);
    if (ERROR_SUCCESS != status || !size || size > valueCapacity)
    {
      return FALSE;
    }
    ZeroMemory(value, valueCapacity);
    status = TdhGetProperty(
      (PEVENT_RECORD)record,
      0,
      NULL,
      1,
      &descriptor,
      size,
      (PBYTE)value);
    if (ERROR_SUCCESS == status && valueSize)
    {
      *valueSize = size;
    }
    return ERROR_SUCCESS == status;
}

static BOOL magObserverReadU64Index(
  const EVENT_RECORD* record,
  LPCWSTR name,
  ULONG arrayIndex,
  UINT64* value)
{
    BYTE storage[8];
    ULONG size = 0;

    if (!value || !magObserverReadProperty(
          record,
          name,
          arrayIndex,
          storage,
          sizeof(storage),
          &size))
    {
      return FALSE;
    }
    *value = 0;
    CopyMemory(value, storage, min(size, (ULONG)sizeof(*value)));
    return TRUE;
}

static BOOL magObserverReadU64(
  const EVENT_RECORD* record,
  LPCWSTR name,
  UINT64* value)
{
    return magObserverReadU64Index(record, name, ULONG_MAX, value);
}

static BOOL magObserverReadU32Index(
  const EVENT_RECORD* record,
  LPCWSTR name,
  ULONG arrayIndex,
  UINT* value)
{
    UINT64 wide = 0;

    if (!value || !magObserverReadU64Index(record, name, arrayIndex, &wide))
    {
      return FALSE;
    }
    *value = (UINT)wide;
    return TRUE;
}

static BOOL magObserverReadU32(
  const EVENT_RECORD* record,
  LPCWSTR name,
  UINT* value)
{
    return magObserverReadU32Index(record, name, ULONG_MAX, value);
}

static void magObserverHandleDxgKrnl(
  MAGPRESENTATIONOBSERVERCORE* core,
  const EVENT_RECORD* record)
{
    const EVENT_HEADER* header = &record->EventHeader;
    const UINT eventId = header->EventDescriptor.Id;
    const UINT64 qpc = (UINT64)header->TimeStamp.QuadPart;

    switch (eventId)
    {
    case MAG_DXGK_EVENT_FLIP:
    case MAG_DXGK_EVENT_FLIP_MPO:
      magObserverOnFlip(
        core,
        header->ProcessId,
        header->ThreadId,
        qpc);
      break;

    case MAG_DXGK_EVENT_BLIT:
      {
        UINT64 window = 0;
        UINT redirected = 0;

        magObserverReadU64(record, L"hwnd", &window);
        magObserverReadU32(record, L"bRedirectedPresent", &redirected);
        magObserverOnBlit(
          core,
          header->ProcessId,
          header->ThreadId,
          qpc,
          (HWND)(ULONG_PTR)window,
          0 != redirected);
      }
      break;

    case MAG_DXGK_EVENT_PRESENT_HISTORY_START:
    case MAG_DXGK_EVENT_PRESENT_HISTORY_DETAILED_START:
      {
        UINT64 token = 0;
        UINT model = MAG_D3DKMT_PM_UNINITIALIZED;

        if (magObserverReadU64(record, L"Token", &token) &&
            magObserverReadU32(record, L"Model", &model))
        {
          magObserverOnPresentHistory(
            core,
            header->ProcessId,
            header->ThreadId,
            qpc,
            token,
            model);
        }
      }
      break;

    case MAG_DXGK_EVENT_PRESENT_HISTORY_INFO:
      {
        UINT64 token = 0;

        if (magObserverReadU64(record, L"Token", &token))
        {
          magObserverOnPresentHistoryInfo(
            core,
            qpc,
            token,
            !g_observer.win32kEnabled);
        }
      }
      break;

    case MAG_DXGK_EVENT_PRESENT_INFO:
      {
        UINT64 window = 0;

        magObserverReadU64(record, L"hWindow", &window);
        magObserverOnPresentInfo(
          core,
          header->ProcessId,
          header->ThreadId,
          qpc,
          (HWND)(ULONG_PTR)window);
      }
      break;

    case MAG_DXGK_EVENT_QUEUE_PACKET_START:
    case MAG_DXGK_EVENT_QUEUE_PACKET_START_2:
      {
        UINT submitSequence = 0;

        if (magObserverReadU32(record, L"SubmitSequence", &submitSequence))
        {
          magObserverOnQueueSubmit(
            core,
            header->ProcessId,
            header->ThreadId,
            qpc,
            submitSequence);
        }
      }
      break;

    case MAG_DXGK_EVENT_QUEUE_PACKET_STOP:
      {
        UINT submitSequence = 0;

        if (magObserverReadU32(record, L"SubmitSequence", &submitSequence))
        {
          magObserverOnQueueComplete(core, qpc, submitSequence);
        }
      }
      break;

    case MAG_DXGK_EVENT_INDEPENDENT_FLIP:
      {
        UINT submitSequence = 0;

        if (magObserverReadU32(record, L"SubmitSequence", &submitSequence))
        {
          magObserverOnIndependentFlip(core, qpc, submitSequence);
        }
      }
      break;

    case MAG_DXGK_EVENT_VSYNC_DPC_MPO:
    case MAG_DXGK_EVENT_HSYNC_DPC_MPO:
      {
        UINT planeCount = 0;
        UINT flipEntryCount = 0;
        UINT activePlaneCount = 0;
        UINT index;

        if (!magObserverReadU32(record, L"PlaneCount", &planeCount) ||
            !magObserverReadU32(record, L"FlipEntryCount", &flipEntryCount))
        {
          break;
        }
        planeCount = min(planeCount, 32U);
        flipEntryCount = min(flipEntryCount, 32U);
        for (index = 0; index < planeCount; ++index)
        {
          UINT64 address = 0;
          LPCWSTR addressName =
            MAG_DXGK_EVENT_VSYNC_DPC_MPO == eventId &&
            header->EventDescriptor.Version >= 1
              ? L"PresentIdOrPhysicalAddress"
              : L"ScannedPhysicalAddress";

          if (magObserverReadU64Index(record, addressName, index, &address) &&
              address)
          {
            ++activePlaneCount;
          }
        }
        for (index = 0; index < flipEntryCount; ++index)
        {
          UINT64 packedSequence = 0;

          if (magObserverReadU64Index(
                record,
                L"FlipSubmitSequence",
                index,
                &packedSequence) &&
              packedSequence)
          {
            magObserverOnMultiPlaneSync(
              core,
              qpc,
              (UINT)(packedSequence >> 32),
              activePlaneCount);
          }
        }
      }
      break;

    default:
      break;
    }
}

static void magObserverHandleWin32k(
  MAGPRESENTATIONOBSERVERCORE* core,
  const EVENT_RECORD* record)
{
    const EVENT_HEADER* header = &record->EventHeader;
    const UINT64 qpc = (UINT64)header->TimeStamp.QuadPart;

    if (MAG_WIN32K_EVENT_TOKEN_COMPOSITION == header->EventDescriptor.Id)
    {
      UINT64 surfaceLuid = 0;
      UINT64 presentCount = 0;
      UINT64 bindId = 0;

      if (magObserverReadU64(
            record,
            L"CompositionSurfaceLuid",
            &surfaceLuid) &&
          magObserverReadU64(record, L"PresentCount", &presentCount) &&
          magObserverReadU64(record, L"BindId", &bindId))
      {
        magObserverOnCompositionToken(
          core,
          header->ProcessId,
          header->ThreadId,
          qpc,
          surfaceLuid,
          presentCount,
          bindId);
      }
    }
    else if (MAG_WIN32K_EVENT_TOKEN_STATE == header->EventDescriptor.Id)
    {
      UINT64 surfaceLuid = 0;
      UINT64 presentCount = 0;
      UINT64 bindId = 0;
      UINT newState = 0;
      UINT independentFlip = 0;

      if (magObserverReadU64(
            record,
            L"CompositionSurfaceLuid",
            &surfaceLuid) &&
          magObserverReadU64(record, L"PresentCount", &presentCount) &&
          magObserverReadU64(record, L"BindId", &bindId) &&
          magObserverReadU32(record, L"NewState", &newState))
      {
        magObserverReadU32(record, L"IndependentFlip", &independentFlip);
        magObserverOnTokenState(
          core,
          qpc,
          surfaceLuid,
          presentCount,
          bindId,
          newState,
          0 != independentFlip);
      }
    }
}

static VOID WINAPI magObserverEventRecord(EVENT_RECORD* record)
{
    MAGPRESENTATIONOBSERVER* observer = record
      ? (MAGPRESENTATIONOBSERVER*)record->UserContext
      : NULL;

    if (!observer)
    {
      return;
    }
    AcquireSRWLockExclusive(&observer->lock);
    if (observer->active)
    {
      if (magObserverGuidEqual(
            &record->EventHeader.ProviderId,
            &MAG_PROVIDER_DXGKRNL))
      {
        magObserverHandleDxgKrnl(&observer->core, record);
      }
      else if (magObserverGuidEqual(
                 &record->EventHeader.ProviderId,
                 &MAG_PROVIDER_WIN32K))
      {
        magObserverHandleWin32k(&observer->core, record);
      }
    }
    ReleaseSRWLockExclusive(&observer->lock);
}

static DWORD WINAPI magObserverThread(void* context)
{
    MAGPRESENTATIONOBSERVER* observer =
      (MAGPRESENTATIONOBSERVER*)context;
    TRACEHANDLE trace;

    if (!observer)
    {
      return ERROR_INVALID_PARAMETER;
    }
    AcquireSRWLockShared(&observer->lock);
    trace = observer->trace;
    ReleaseSRWLockShared(&observer->lock);
    return (DWORD)ProcessTrace(&trace, 1, NULL, NULL);
}

static ULONG magObserverEnableProvider(
  TRACEHANDLE session,
  const GUID* provider,
  const USHORT* eventIds,
  UINT eventIdCount)
{
    BYTE filterStorage[
      sizeof(EVENT_FILTER_EVENT_ID) +
      (MAX_EVENT_FILTER_EVENT_ID_COUNT - 1) * sizeof(USHORT)];
    EVENT_FILTER_EVENT_ID* filter =
      (EVENT_FILTER_EVENT_ID*)filterStorage;
    EVENT_FILTER_DESCRIPTOR descriptor = { 0 };
    ENABLE_TRACE_PARAMETERS parameters = { 0 };
    UINT index;

    if (!provider || !eventIds || !eventIdCount ||
        eventIdCount > MAX_EVENT_FILTER_EVENT_ID_COUNT)
    {
      return ERROR_INVALID_PARAMETER;
    }
    ZeroMemory(filterStorage, sizeof(filterStorage));
    filter->FilterIn = TRUE;
    filter->Count = (USHORT)eventIdCount;
    for (index = 0; index < eventIdCount; ++index)
    {
      filter->Events[index] = eventIds[index];
    }
    descriptor.Ptr = (ULONGLONG)(ULONG_PTR)filter;
    descriptor.Size = sizeof(EVENT_FILTER_EVENT_ID) +
      (eventIdCount - 1U) * sizeof(USHORT);
    descriptor.Type = EVENT_FILTER_TYPE_EVENT_ID;
    parameters.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    parameters.EnableProperty = EVENT_ENABLE_PROPERTY_IGNORE_KEYWORD_0;
    parameters.EnableFilterDesc = &descriptor;
    parameters.FilterDescCount = 1;
    return EnableTraceEx2(
      session,
      provider,
      EVENT_CONTROL_CODE_ENABLE_PROVIDER,
      TRACE_LEVEL_VERBOSE,
      MAXUINT64,
      0,
      0,
      &parameters);
}

static void magObserverStopSession(
  TRACEHANDLE session,
  TRACEHANDLE trace,
  EVENT_TRACE_PROPERTIES* properties,
  HANDLE thread)
{
    if (session && properties)
    {
      ControlTrace(session, NULL, properties, EVENT_TRACE_CONTROL_STOP);
    }
    if (trace && INVALID_PROCESSTRACE_HANDLE != trace)
    {
      CloseTrace(trace);
    }
    if (thread)
    {
      WaitForSingleObject(thread, 5000);
      CloseHandle(thread);
    }
}

BOOL magPresentationObserverStart(HWND hWnd)
{
    static const USHORT dxgKrnlEvents[] =
    {
      MAG_DXGK_EVENT_BLIT,
      MAG_DXGK_EVENT_FLIP,
      MAG_DXGK_EVENT_PRESENT_HISTORY_START,
      MAG_DXGK_EVENT_PRESENT_HISTORY_INFO,
      MAG_DXGK_EVENT_QUEUE_PACKET_START,
      MAG_DXGK_EVENT_QUEUE_PACKET_STOP,
      MAG_DXGK_EVENT_PRESENT_INFO,
      MAG_DXGK_EVENT_PRESENT_HISTORY_DETAILED_START,
      MAG_DXGK_EVENT_QUEUE_PACKET_START_2,
      MAG_DXGK_EVENT_FLIP_MPO,
      MAG_DXGK_EVENT_MMIO_FLIP_MPO,
      MAG_DXGK_EVENT_INDEPENDENT_FLIP,
      MAG_DXGK_EVENT_VSYNC_DPC_MPO,
      MAG_DXGK_EVENT_HSYNC_DPC_MPO,
      MAG_DXGK_EVENT_MMIO_FLIP_MPO3,
    };
    static const USHORT win32kEvents[] =
    {
      MAG_WIN32K_EVENT_TOKEN_COMPOSITION,
      MAG_WIN32K_EVENT_TOKEN_STATE,
    };
    EVENT_TRACE_LOGFILE log = { 0 };
    EVENT_TRACE_PROPERTIES* properties;
    TRACEHANDLE session = 0;
    TRACEHANDLE trace = INVALID_PROCESSTRACE_HANDLE;
    HANDLE thread = NULL;
    ULONG result;
    ULONG startTraceResult;
    ULONG openTraceResult = ERROR_NOT_READY;
    ULONG dxgKrnlResult = ERROR_NOT_SUPPORTED;
    ULONG win32kResult = ERROR_NOT_SUPPORTED;
    const ULONG propertySize = sizeof(*properties) +
      MAG_OBSERVER_SESSION_NAME_COUNT * sizeof(WCHAR);

    if (!hWnd)
    {
      SetLastError(ERROR_INVALID_WINDOW_HANDLE);
      return FALSE;
    }
    AcquireSRWLockExclusive(&g_observer.lock);
    if (g_observer.active)
    {
      magObserverCoreReset(&g_observer.core, hWnd, magObserverQpc());
      ReleaseSRWLockExclusive(&g_observer.lock);
      return TRUE;
    }
    ReleaseSRWLockExclusive(&g_observer.lock);

    properties = (EVENT_TRACE_PROPERTIES*)HeapAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      propertySize);
    if (!properties)
    {
      SetLastError(ERROR_NOT_ENOUGH_MEMORY);
      return FALSE;
    }
    _snwprintf_s(
      g_observer.sessionName,
      ARRAYSIZE(g_observer.sessionName),
      _TRUNCATE,
      L"Mag-Presentation-%lu-%llu",
      GetCurrentProcessId(),
      GetTickCount64());
    properties->Wnode.BufferSize = propertySize;
    properties->Wnode.ClientContext = 1;
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    properties->BufferSize = 64;
    properties->MinimumBuffers = 2;
    properties->MaximumBuffers = 8;
    properties->FlushTimer = 1;
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
    properties->LoggerNameOffset = sizeof(*properties);
    lstrcpynW(
      (LPWSTR)((BYTE*)properties + properties->LoggerNameOffset),
      g_observer.sessionName,
      MAG_OBSERVER_SESSION_NAME_COUNT);

    startTraceResult = StartTrace(&session, g_observer.sessionName, properties);
    result = startTraceResult;
    if (ERROR_SUCCESS == result)
    {
      dxgKrnlResult = magObserverEnableProvider(
        session,
        &MAG_PROVIDER_DXGKRNL,
        dxgKrnlEvents,
        ARRAYSIZE(dxgKrnlEvents));
      win32kResult = magObserverEnableProvider(
        session,
        &MAG_PROVIDER_WIN32K,
        win32kEvents,
        ARRAYSIZE(win32kEvents));
      result = ERROR_SUCCESS == dxgKrnlResult || ERROR_SUCCESS == win32kResult
        ? ERROR_SUCCESS
        : dxgKrnlResult;
    }
    if (ERROR_SUCCESS == result)
    {
      log.LoggerName = g_observer.sessionName;
      log.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME |
        PROCESS_TRACE_MODE_EVENT_RECORD |
        PROCESS_TRACE_MODE_RAW_TIMESTAMP;
      log.EventRecordCallback = magObserverEventRecord;
      log.Context = &g_observer;
      trace = OpenTrace(&log);
      if (INVALID_PROCESSTRACE_HANDLE == trace)
      {
        openTraceResult = GetLastError();
        result = openTraceResult;
      }
      else
      {
        openTraceResult = ERROR_SUCCESS;
      }
    }
    if (ERROR_SUCCESS == result)
    {
      AcquireSRWLockExclusive(&g_observer.lock);
      g_observer.session = session;
      g_observer.trace = trace;
      g_observer.properties = properties;
      g_observer.dxgKrnlEnabled = ERROR_SUCCESS == dxgKrnlResult;
      g_observer.win32kEnabled = ERROR_SUCCESS == win32kResult;
      g_observer.active = TRUE;
      g_observer.error = ERROR_SUCCESS != dxgKrnlResult
        ? dxgKrnlResult
        : win32kResult;
      g_observer.startTraceError = startTraceResult;
      g_observer.dxgKrnlError = dxgKrnlResult;
      g_observer.win32kError = win32kResult;
      g_observer.openTraceError = openTraceResult;
      magObserverCoreReset(&g_observer.core, hWnd, magObserverQpc());
      ReleaseSRWLockExclusive(&g_observer.lock);
      thread = CreateThread(NULL, 0, magObserverThread, &g_observer, 0, NULL);
      if (!thread)
      {
        result = GetLastError();
      }
    }
    if (ERROR_SUCCESS == result)
    {
      AcquireSRWLockExclusive(&g_observer.lock);
      g_observer.thread = thread;
      ReleaseSRWLockExclusive(&g_observer.lock);
      return TRUE;
    }

    AcquireSRWLockExclusive(&g_observer.lock);
    g_observer.active = FALSE;
    g_observer.dxgKrnlEnabled = FALSE;
    g_observer.win32kEnabled = FALSE;
    g_observer.error = result;
    g_observer.startTraceError = startTraceResult;
    g_observer.dxgKrnlError = dxgKrnlResult;
    g_observer.win32kError = win32kResult;
    g_observer.openTraceError = openTraceResult;
    g_observer.session = 0;
    g_observer.trace = INVALID_PROCESSTRACE_HANDLE;
    g_observer.thread = NULL;
    g_observer.properties = NULL;
    ReleaseSRWLockExclusive(&g_observer.lock);
    magObserverStopSession(session, trace, properties, thread);
    HeapFree(GetProcessHeap(), 0, properties);
    SetLastError(result);
    return FALSE;
}

void magPresentationObserverStop(void)
{
    TRACEHANDLE session;
    TRACEHANDLE trace;
    HANDLE thread;
    EVENT_TRACE_PROPERTIES* properties;

    AcquireSRWLockExclusive(&g_observer.lock);
    session = g_observer.session;
    trace = g_observer.trace;
    thread = g_observer.thread;
    properties = g_observer.properties;
    g_observer.active = FALSE;
    g_observer.session = 0;
    g_observer.trace = INVALID_PROCESSTRACE_HANDLE;
    g_observer.thread = NULL;
    g_observer.properties = NULL;
    g_observer.dxgKrnlEnabled = FALSE;
    g_observer.win32kEnabled = FALSE;
    ReleaseSRWLockExclusive(&g_observer.lock);

    magObserverStopSession(session, trace, properties, thread);
    if (properties)
    {
      HeapFree(GetProcessHeap(), 0, properties);
    }
}

void magPresentationObserverReset(HWND hWnd)
{
    AcquireSRWLockExclusive(&g_observer.lock);
    magObserverCoreReset(&g_observer.core, hWnd, magObserverQpc());
    ReleaseSRWLockExclusive(&g_observer.lock);
}

BOOL magPresentationObserverGetLatest(
  HWND hWnd,
  MAGPRESENTATIONTARGET* target,
  UINT64* sequence)
{
    BOOL result = FALSE;

    if (!target)
    {
      return FALSE;
    }
    AcquireSRWLockShared(&g_observer.lock);
    if (g_observer.active && g_observer.core.latestValid &&
        (!hWnd || !g_observer.core.expectedWindow ||
         hWnd == g_observer.core.expectedWindow))
    {
      *target = g_observer.core.latestTarget;
      if (sequence)
      {
        *sequence = g_observer.core.latestSequence;
      }
      result = TRUE;
    }
    ReleaseSRWLockShared(&g_observer.lock);
    return result;
}

void magPresentationObserverGetStatus(
  MAGPRESENTATIONOBSERVERSTATUS* status)
{
    if (!status)
    {
      return;
    }
    AcquireSRWLockShared(&g_observer.lock);
    status->active = g_observer.active;
    status->dxgKrnlEnabled = g_observer.dxgKrnlEnabled;
    status->win32kEnabled = g_observer.win32kEnabled;
    status->error = g_observer.error;
    status->startTraceError = g_observer.startTraceError;
    status->dxgKrnlError = g_observer.dxgKrnlError;
    status->win32kError = g_observer.win32kError;
    status->openTraceError = g_observer.openTraceError;
    ReleaseSRWLockShared(&g_observer.lock);
}

static BOOL magObserverExpectTarget(
  const MAGPRESENTATIONOBSERVERCORE* core,
  MAGPRESENTATIONTARGET target,
  LPCTSTR testName,
  LPTSTR reason,
  UINT reasonCount)
{
    if (core && core->latestValid && core->latestTarget == target)
    {
      return TRUE;
    }
    if (reason && reasonCount)
    {
      _sntprintf_s(
        reason,
        reasonCount,
        _TRUNCATE,
        TEXT("Presentation observer classifier failed %s: expected %u, observed %u, valid=%u."),
        testName,
        (UINT)target,
        core ? (UINT)core->latestTarget : 0,
        core ? core->latestValid : FALSE);
    }
    return FALSE;
}

BOOL magPresentationObserverRunClassifierTests(
  LPTSTR reason,
  UINT reasonCount)
{
    MAGPRESENTATIONOBSERVERCORE core = { 0 };
    const DWORD processId = GetCurrentProcessId();
    const HWND hWnd = (HWND)(ULONG_PTR)0x1234;
    MAGOBSERVEDPRESENT* present;

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    magObserverCoreReset(&core, hWnd, 1);
    magObserverOnFlip(&core, processId, 10, 2);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_HARDWARE_LEGACY_FLIP,
          TEXT("legacy flip"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    magObserverCoreReset(&core, hWnd, 10);
    magObserverOnBlit(&core, processId, 11, 11, hWnd, FALSE);
    magObserverOnPresentInfo(&core, processId, 11, 12, hWnd);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER,
          TEXT("legacy copy"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    magObserverCoreReset(&core, hWnd, 20);
    magObserverOnBlit(&core, processId, 12, 21, hWnd, FALSE);
    magObserverOnPresentHistory(
      &core,
      processId,
      12,
      22,
      0x100,
      MAG_D3DKMT_PM_REDIRECTED_BLT);
    magObserverOnPresentHistoryInfo(&core, 23, 0x100, FALSE);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_COMPOSED_COPY_GPU_GDI,
          TEXT("GPU GDI copy"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    magObserverCoreReset(&core, hWnd, 30);
    magObserverOnBlit(&core, processId, 13, 31, hWnd, TRUE);
    magObserverOnPresentHistory(
      &core,
      processId,
      13,
      32,
      0x101,
      MAG_D3DKMT_PM_REDIRECTED_BLT);
    magObserverOnPresentHistoryInfo(&core, 33, 0x101, FALSE);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_COMPOSED_COPY_CPU_GDI,
          TEXT("CPU GDI copy"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    magObserverCoreReset(&core, hWnd, 35);
    magObserverOnPresentHistory(
      &core,
      processId,
      16,
      36,
      0x102,
      MAG_D3DKMT_PM_REDIRECTED_FLIP);
    magObserverOnPresentHistoryInfo(&core, 37, 0x102, TRUE);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_COMPOSED_FLIP,
          TEXT("DxgKrnl-only composed flip"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    magObserverCoreReset(&core, hWnd, 40);
    magObserverOnCompositionToken(&core, processId, 14, 41, 0x200, 5, 7);
    magObserverOnTokenState(&core, 42, 0x200, 5, 7, 3, FALSE);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_COMPOSED_FLIP,
          TEXT("composed flip"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    magObserverCoreReset(&core, hWnd, 50);
    magObserverOnCompositionToken(&core, processId, 15, 51, 0x201, 6, 8);
    magObserverOnTokenState(&core, 52, 0x201, 6, 8, 3, TRUE);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_HARDWARE_INDEPENDENT_FLIP,
          TEXT("independent flip"),
          reason,
          reasonCount))
    {
      return FALSE;
    }

    present = magObserverFindThreadPresent(&core, 15);
    magObserverMapSubmit(&core, present, 99, 53);
    magObserverOnMultiPlaneSync(&core, 54, 99, 2);
    if (!magObserverExpectTarget(
          &core,
          MAG_PRESENT_HARDWARE_COMPOSED_INDEPENDENT_FLIP,
          TEXT("hardware-composed independent flip"),
          reason,
          reasonCount))
    {
      return FALSE;
    }
    return TRUE;
}
