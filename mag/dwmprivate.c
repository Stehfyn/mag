#define COBJMACROS

#include "dwmprivate.h"
#include "dcompabi.h"
#include "graphics.h"

#include <d3d11.h>
#include <dwmapi.h>
#include <dxgi1_3.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dwmapi")
#pragma comment(lib, "dxgi")

#define DWM_ORD_CREATE_SHARED_THUMBNAIL_VISUAL 147
#define DWM_ORD_QUERY_WINDOW_THUMBNAIL_SOURCE_SIZE 162
#define DWM_ORD_CREATE_SHARED_DESKTOP_VISUAL 163
#define DWM_ORD_UPDATE_SHARED_DESKTOP_VISUAL 164
#define DWM_PRIVATE_MULTIWINDOW_BUILD 20000
#define DWM_PRIVATE_THUMBNAIL_TYPE_ICONIC 2
#define DWM_PRIVATE_MAX_TOP_WINDOWS 64
#define DWM_PRIVATE_MAX_SHELL_VISUALS 32
#define DWM_PRIVATE_MAX_EXCLUSIONS (DWM_PRIVATE_MAX_SHELL_VISUALS + 1)

#ifndef DWM_TNP_ENABLE3D
#define DWM_TNP_ENABLE3D 0x04000000
#endif

typedef HRESULT (WINAPI* PFN_DCOMPOSITIONCREATEDEVICE3)(
  IUnknown*, REFIID, void**);
typedef HRESULT (WINAPI* PFN_DWMPCREATESHAREDTHUMBNAILVISUAL)(
  HWND, HWND, DWORD, DWM_THUMBNAIL_PROPERTIES*, void*, void**, PHTHUMBNAIL);
typedef HRESULT (WINAPI* PFN_DWMPQUERYWINDOWTHUMBNAILSOURCESIZE)(
  HWND, BOOL, SIZE*);
typedef HRESULT (WINAPI* PFN_DWMPCREATESHAREDDESKTOPVISUAL)(
  HWND, void*, void**, PHTHUMBNAIL);
typedef HRESULT (WINAPI* PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL)(
  HTHUMBNAIL, HWND*, DWORD, HWND*, DWORD, RECT*, SIZE*);
typedef HRESULT (WINAPI* PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL)(
  HTHUMBNAIL, HWND*, DWORD, HWND*, DWORD, RECT*, SIZE*, DWORD);

typedef enum MAG_WINDOWCOMPOSITIONATTRIB
{
  MAG_WCA_EXCLUDED_FROM_LIVEPREVIEW = 0x0D
} MAG_WINDOWCOMPOSITIONATTRIB;

typedef struct MAG_WINDOWCOMPOSITIONATTRIBDATA
{
  MAG_WINDOWCOMPOSITIONATTRIB attrib;
  void*                       data;
  DWORD                       dataSize;
} MAG_WINDOWCOMPOSITIONATTRIBDATA;

typedef BOOL (WINAPI* PFN_SETWINDOWCOMPOSITIONATTRIBUTE)(
  HWND, MAG_WINDOWCOMPOSITIONATTRIBDATA*);

typedef struct MAG_RTL_OSVERSIONINFOW
{
  ULONG dwOSVersionInfoSize;
  ULONG dwMajorVersion;
  ULONG dwMinorVersion;
  ULONG dwBuildNumber;
  ULONG dwPlatformId;
  WCHAR szCSDVersion[128];
} MAG_RTL_OSVERSIONINFOW;

typedef LONG (WINAPI* PFN_RTLGETVERSION)(MAG_RTL_OSVERSIONINFOW*);

typedef enum DWMPRIVATESHELLLAYER
{
  DWM_PRIVATE_SHELL_DESKTOP = 0,
  DWM_PRIVATE_SHELL_TASKBAR
} DWMPRIVATESHELLLAYER;

typedef enum DWMPRIVATETOPWINDOWKIND
{
  DWM_PRIVATE_TOP_OTHER = 0,
  DWM_PRIVATE_TOP_PROGMAN,
  DWM_PRIVATE_TOP_WORKER,
  DWM_PRIVATE_TOP_TASKBAR
} DWMPRIVATETOPWINDOWKIND;

typedef struct DWMPRIVATETOPWINDOW
{
  HWND                    hwnd;
  RECT                    rect;
  DWMPRIVATETOPWINDOWKIND kind;
  BOOL                    visible;
  BOOL                    hasDesktopView;
} DWMPRIVATETOPWINDOW;

typedef struct DWMPRIVATESHELLCANDIDATE
{
  HWND                    hwnd;
  RECT                    rect;
  DWMPRIVATESHELLLAYER    layer;
} DWMPRIVATESHELLCANDIDATE;

typedef struct DWMPRIVATESHELLVISUAL
{
  HWND                          hwnd;
  RECT                          rect;
  SIZE                          sourceSize;
  DWMPRIVATESHELLLAYER          layer;
  DWM_THUMBNAIL_PROPERTIES      properties;
  MAG_IDCompositionVisual*      visual;
  HTHUMBNAIL                    thumbnail;
} DWMPRIVATESHELLVISUAL;

typedef struct MAG_D2D_MATRIX_3X2_F
{
  FLOAT m11;
  FLOAT m12;
  FLOAT m21;
  FLOAT m22;
  FLOAT dx;
  FLOAT dy;
} MAG_D2D_MATRIX_3X2_F;

typedef struct MAG_D2D_RECT_F
{
  FLOAT left;
  FLOAT top;
  FLOAT right;
  FLOAT bottom;
} MAG_D2D_RECT_F;

struct DWMPRIVATECAPTURESTATE
{
  HWND                                      hwndDestination;
  HMODULE                                   hDcomp;
  HMODULE                                   hDwmApi;
  HMODULE                                   hUser32;
  PFN_DCOMPOSITIONCREATEDEVICE3             createDcompDevice;
  PFN_DWMPCREATESHAREDTHUMBNAILVISUAL       createSharedThumbnailVisual;
  PFN_DWMPQUERYWINDOWTHUMBNAILSOURCESIZE    queryWindowThumbnailSourceSize;
  PFN_DWMPCREATESHAREDDESKTOPVISUAL         createSharedVisual;
  FARPROC                                   updateSharedVisual;
  PFN_SETWINDOWCOMPOSITIONATTRIBUTE         setWindowCompositionAttribute;
  ID3D11Device*                             d3dDevice;
  ID3D11DeviceContext*                      d3dContext;
  IDXGIDevice*                              dxgiDevice;
  MAG_IDCompositionDesktopDevice*           dcompDevice;
  MAG_IDCompositionTarget*                  dcompTarget;
  MAG_IDCompositionVisual*                  rootVisual;
  MAG_IDCompositionVisual*                  captureVisual;
  MAG_IDCompositionVisual*                  viewVisual;
  MAG_IDCompositionVisual*                  sharedVisual;
  MAG_IDCompositionVisual*                  overlayVisual;
  MAG_IDCompositionSurface*                overlaySurface;
  HTHUMBNAIL                                thumbnail;
  DWORD*                                    overlayPixels;
  UINT                                      overlayWidth;
  UINT                                      overlayHeight;
  UINT                                      overlayCapacityWidth;
  UINT                                      overlayCapacityHeight;
  UINT64                                    overlayResourceGeneration;
  BOOL                                      useMultiWindow;
  BOOL                                      livePreviewExcluded;
  BOOL                                      desktopConfigured;
  BOOL                                      viewConfigured;
  BOOL                                      viewVisible;
  BOOL                                      treeDirty;
  BOOL                                      overlayInitialized;
  RECT                                      desktopSource;
  RECT                                      viewDesktop;
  RECT                                      viewSource;
  RECT                                      viewDestination;
  DWMPRIVATETOPWINDOW                       topWindows[DWM_PRIVATE_MAX_TOP_WINDOWS];
  UINT                                      topWindowCount;
  DWMPRIVATESHELLCANDIDATE                  shellCandidates[DWM_PRIVATE_MAX_SHELL_VISUALS];
  UINT                                      shellCandidateCount;
  DWMPRIVATESHELLVISUAL                     shellVisuals[DWM_PRIVATE_MAX_SHELL_VISUALS];
  UINT                                      shellVisualCount;
  DWMPRIVATESHELLVISUAL                     retiredShellVisuals[DWM_PRIVATE_MAX_SHELL_VISUALS];
  UINT                                      retiredShellVisualCount;
  HWND                                      desktopExclusions[DWM_PRIVATE_MAX_EXCLUSIONS];
  UINT                                      desktopExclusionCount;
};

static void DwmPrivateReleaseUnknown(void** object)
{
  if (object && *object)
  {
    IUnknown_Release((IUnknown*)*object);
    *object = NULL;
  }
}

static LONG DwmPrivateClampLong(LONG value, LONG minimum, LONG maximum)
{
  if (value < minimum)
  {
    return minimum;
  }
  if (value > maximum)
  {
    return maximum;
  }
  return value;
}

static BYTE DwmPrivateFloatToByte(FLOAT value)
{
  if (value <= 0.0f)
  {
    return 0;
  }
  if (value >= 1.0f)
  {
    return 255;
  }
  return (BYTE)(value * 255.0f + 0.5f);
}

static DWORD DwmPrivatePremultiplyColor(const FLOAT color[4])
{
  const UINT alpha = DwmPrivateFloatToByte(color[3]);
  const UINT red = (DwmPrivateFloatToByte(color[0]) * alpha + 127U) / 255U;
  const UINT green = (DwmPrivateFloatToByte(color[1]) * alpha + 127U) / 255U;
  const UINT blue = (DwmPrivateFloatToByte(color[2]) * alpha + 127U) / 255U;
  return blue | (green << 8) | (red << 16) | (alpha << 24);
}

static DWORD DwmPrivateBlendPixel(DWORD destination, DWORD source)
{
  const UINT sourceAlpha = (source >> 24) & 0xffU;
  const UINT inverseAlpha = 255U - sourceAlpha;
  const UINT destinationBlue = destination & 0xffU;
  const UINT destinationGreen = (destination >> 8) & 0xffU;
  const UINT destinationRed = (destination >> 16) & 0xffU;
  const UINT destinationAlpha = (destination >> 24) & 0xffU;
  const UINT blue = (source & 0xffU) +
    ((destinationBlue * inverseAlpha + 127U) / 255U);
  const UINT green = ((source >> 8) & 0xffU) +
    ((destinationGreen * inverseAlpha + 127U) / 255U);
  const UINT red = ((source >> 16) & 0xffU) +
    ((destinationRed * inverseAlpha + 127U) / 255U);
  const UINT alpha = sourceAlpha +
    ((destinationAlpha * inverseAlpha + 127U) / 255U);
  return blue | (green << 8) | (red << 16) | (alpha << 24);
}

static RECT DwmPrivateClipRect(
  const DWMPRIVATECAPTURESTATE* state,
  const RECT* rect)
{
  RECT clipped = *rect;
  clipped.left = DwmPrivateClampLong(
    clipped.left, 0, (LONG)state->overlayWidth);
  clipped.top = DwmPrivateClampLong(
    clipped.top, 0, (LONG)state->overlayHeight);
  clipped.right = DwmPrivateClampLong(
    clipped.right, 0, (LONG)state->overlayWidth);
  clipped.bottom = DwmPrivateClampLong(
    clipped.bottom, 0, (LONG)state->overlayHeight);
  return clipped;
}

static void DwmPrivateFillRect(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* rect,
  DWORD color)
{
  const RECT clipped = DwmPrivateClipRect(state, rect);
  LONG y;

  if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
  {
    return;
  }
  for (y = clipped.top; y < clipped.bottom; ++y)
  {
    DWORD* row = state->overlayPixels +
      (SIZE_T)y * state->overlayCapacityWidth;
    LONG x;
    for (x = clipped.left; x < clipped.right; ++x)
    {
      row[x] = DwmPrivateBlendPixel(row[x], color);
    }
  }
}

static void DwmPrivateClearRect(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* rect)
{
  const RECT clipped = DwmPrivateClipRect(state, rect);
  LONG y;

  if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
  {
    return;
  }
  for (y = clipped.top; y < clipped.bottom; ++y)
  {
    DWORD* row = state->overlayPixels +
      (SIZE_T)y * state->overlayCapacityWidth;
    ZeroMemory(
      row + clipped.left,
      (SIZE_T)(clipped.right - clipped.left) * sizeof(*row));
  }
}

static void DwmPrivateStrokeRect(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* rect,
  UINT thickness,
  DWORD color)
{
  const RECT clipped = DwmPrivateClipRect(state, rect);
  const LONG stroke = (LONG)(thickness ? thickness : 1U);
  LONG y;

  if (clipped.right <= clipped.left || clipped.bottom <= clipped.top)
  {
    return;
  }
  for (y = clipped.top; y < clipped.bottom; ++y)
  {
    DWORD* row = state->overlayPixels +
      (SIZE_T)y * state->overlayCapacityWidth;
    LONG x;
    for (x = clipped.left; x < clipped.right; ++x)
    {
      if (x < rect->left + stroke || x >= rect->right - stroke ||
          y < rect->top + stroke || y >= rect->bottom - stroke)
      {
        row[x] = DwmPrivateBlendPixel(row[x], color);
      }
    }
  }
}

static BOOL DwmPrivateIsMultiWindowBuild(void)
{
  HMODULE hNtdll = GetModuleHandle(TEXT("ntdll.dll"));
  PFN_RTLGETVERSION rtlGetVersion = hNtdll
    ? (PFN_RTLGETVERSION)GetProcAddress(hNtdll, "RtlGetVersion")
    : NULL;
  MAG_RTL_OSVERSIONINFOW version;

  ZeroMemory(&version, sizeof(version));
  version.dwOSVersionInfoSize = sizeof(version);
  if (rtlGetVersion && rtlGetVersion(&version) >= 0)
  {
    return version.dwBuildNumber >= DWM_PRIVATE_MULTIWINDOW_BUILD;
  }
  return sizeof(void*) == 8 ? TRUE : FALSE;
}

static BOOL DwmPrivateSetExcludedFromLivePreview(
  DWMPRIVATECAPTURESTATE* state,
  BOOL excluded)
{
  MAG_WINDOWCOMPOSITIONATTRIBDATA data;
  BOOL enabled = excluded;

  if (!state->setWindowCompositionAttribute)
  {
    return FALSE;
  }
  ZeroMemory(&data, sizeof(data));
  data.attrib = MAG_WCA_EXCLUDED_FROM_LIVEPREVIEW;
  data.data = &enabled;
  data.dataSize = sizeof(enabled);
  return state->setWindowCompositionAttribute(state->hwndDestination, &data);
}

static DWMPRIVATETOPWINDOWKIND DwmPrivateClassifyTopWindow(HWND hwnd)
{
  WCHAR className[64];

  ZeroMemory(className, sizeof(className));
  if (!GetClassNameW(hwnd, className, ARRAYSIZE(className)))
  {
    return DWM_PRIVATE_TOP_OTHER;
  }
  if (!lstrcmpiW(className, L"Progman"))
  {
    return DWM_PRIVATE_TOP_PROGMAN;
  }
  if (!lstrcmpiW(className, L"WorkerW"))
  {
    return DWM_PRIVATE_TOP_WORKER;
  }
  if (!lstrcmpiW(className, L"Shell_TrayWnd") ||
      !lstrcmpiW(className, L"Shell_SecondaryTrayWnd"))
  {
    return DWM_PRIVATE_TOP_TASKBAR;
  }
  return DWM_PRIVATE_TOP_OTHER;
}

typedef struct DWMPRIVATEENUMCONTEXT
{
  DWMPRIVATECAPTURESTATE* state;
  BOOL                    overflow;
} DWMPRIVATEENUMCONTEXT;

static BOOL CALLBACK DwmPrivateEnumTopWindow(HWND hwnd, LPARAM parameter)
{
  DWMPRIVATEENUMCONTEXT* context = (DWMPRIVATEENUMCONTEXT*)parameter;
  DWMPRIVATETOPWINDOWKIND kind = DwmPrivateClassifyTopWindow(hwnd);
  DWMPRIVATETOPWINDOW* window;

  if (DWM_PRIVATE_TOP_OTHER == kind)
  {
    return TRUE;
  }
  if (context->state->topWindowCount >= DWM_PRIVATE_MAX_TOP_WINDOWS)
  {
    context->overflow = TRUE;
    return FALSE;
  }
  window = &context->state->topWindows[context->state->topWindowCount];
  ZeroMemory(window, sizeof(*window));
  if (!GetWindowRect(hwnd, &window->rect))
  {
    return TRUE;
  }
  window->hwnd = hwnd;
  window->kind = kind;
  window->visible = IsWindowVisible(hwnd);
  window->hasDesktopView =
    NULL != FindWindowExW(hwnd, NULL, L"SHELLDLL_DefView", NULL);
  ++context->state->topWindowCount;
  return TRUE;
}

static BOOL DwmPrivateIsWallpaperWorker(
  const DWMPRIVATECAPTURESTATE* state,
  UINT candidateIndex)
{
  UINT hostIndex;
  for (hostIndex = 0; hostIndex < candidateIndex; ++hostIndex)
  {
    UINT siblingIndex;
    if (!state->topWindows[hostIndex].hasDesktopView)
    {
      continue;
    }
    for (siblingIndex = hostIndex + 1;
         siblingIndex < state->topWindowCount;
         ++siblingIndex)
    {
      if (DWM_PRIVATE_TOP_WORKER == state->topWindows[siblingIndex].kind)
      {
        if (siblingIndex == candidateIndex)
        {
          return TRUE;
        }
        break;
      }
    }
  }
  return FALSE;
}

static HRESULT DwmPrivateAppendShellCandidate(
  DWMPRIVATECAPTURESTATE* state,
  const DWMPRIVATETOPWINDOW* window,
  DWMPRIVATESHELLLAYER layer,
  const RECT* desktopSource)
{
  RECT intersection;
  UINT index;

  if (!IntersectRect(&intersection, &window->rect, desktopSource))
  {
    return S_FALSE;
  }
  for (index = 0; index < state->shellCandidateCount; ++index)
  {
    if (state->shellCandidates[index].hwnd == window->hwnd &&
        state->shellCandidates[index].layer == layer)
    {
      return S_FALSE;
    }
  }
  if (state->shellCandidateCount >= DWM_PRIVATE_MAX_SHELL_VISUALS)
  {
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }
  state->shellCandidates[state->shellCandidateCount].hwnd = window->hwnd;
  state->shellCandidates[state->shellCandidateCount].rect = window->rect;
  state->shellCandidates[state->shellCandidateCount].layer = layer;
  ++state->shellCandidateCount;
  return S_OK;
}

static HRESULT DwmPrivateBuildShellCandidates(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* desktopSource)
{
  DWMPRIVATEENUMCONTEXT context;
  UINT index;

  ZeroMemory(&context, sizeof(context));
  context.state = state;
  state->topWindowCount = 0;
  state->shellCandidateCount = 0;
  if (!EnumWindows(DwmPrivateEnumTopWindow, (LPARAM)&context) &&
      context.overflow)
  {
    return HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER);
  }

  index = state->topWindowCount;
  while (index-- > 0)
  {
    const DWMPRIVATETOPWINDOW* window = &state->topWindows[index];
    const BOOL isDesktop =
      DWM_PRIVATE_TOP_PROGMAN == window->kind ||
      (DWM_PRIVATE_TOP_WORKER == window->kind &&
       (window->hasDesktopView || DwmPrivateIsWallpaperWorker(state, index)));
    if (isDesktop)
    {
      HRESULT hr = DwmPrivateAppendShellCandidate(
        state, window, DWM_PRIVATE_SHELL_DESKTOP, desktopSource);
      if (FAILED(hr))
      {
        return hr;
      }
    }
  }

  index = state->topWindowCount;
  while (index-- > 0)
  {
    const DWMPRIVATETOPWINDOW* window = &state->topWindows[index];
    if (DWM_PRIVATE_TOP_TASKBAR == window->kind && window->visible)
    {
      HRESULT hr = DwmPrivateAppendShellCandidate(
        state, window, DWM_PRIVATE_SHELL_TASKBAR, desktopSource);
      if (FAILED(hr))
      {
        return hr;
      }
    }
  }
  return S_OK;
}

static SIZE DwmPrivateQueryShellSourceSize(
  DWMPRIVATECAPTURESTATE* state,
  HWND hwnd,
  const RECT* windowRect)
{
  SIZE size;
  SIZE queriedSize;

  size.cx = windowRect->right - windowRect->left;
  size.cy = windowRect->bottom - windowRect->top;
  ZeroMemory(&queriedSize, sizeof(queriedSize));
  if (state->queryWindowThumbnailSourceSize &&
      SUCCEEDED(state->queryWindowThumbnailSourceSize(
        hwnd, FALSE, &queriedSize)) &&
      queriedSize.cx > 0 && queriedSize.cy > 0)
  {
    size = queriedSize;
  }
  return size;
}

static HRESULT DwmPrivateBuildShellThumbnailProperties(
  const RECT* desktopSource,
  const RECT* windowRect,
  SIZE sourceSize,
  DWM_THUMBNAIL_PROPERTIES* properties)
{
  const LONG windowWidth = windowRect->right - windowRect->left;
  const LONG windowHeight = windowRect->bottom - windowRect->top;
  RECT intersection;

  if (!properties || windowWidth < 1 || windowHeight < 1 ||
      sourceSize.cx < 1 || sourceSize.cy < 1 ||
      !IntersectRect(&intersection, windowRect, desktopSource))
  {
    return E_INVALIDARG;
  }
  ZeroMemory(properties, sizeof(*properties));
  properties->dwFlags = DWM_TNP_RECTDESTINATION |
    DWM_TNP_RECTSOURCE | DWM_TNP_OPACITY | DWM_TNP_VISIBLE |
    DWM_TNP_SOURCECLIENTAREAONLY | DWM_TNP_ENABLE3D;
  properties->rcDestination.left = intersection.left - desktopSource->left;
  properties->rcDestination.top = intersection.top - desktopSource->top;
  properties->rcDestination.right = intersection.right - desktopSource->left;
  properties->rcDestination.bottom = intersection.bottom - desktopSource->top;
  properties->rcSource.left = MulDiv(
    intersection.left - windowRect->left, sourceSize.cx, windowWidth);
  properties->rcSource.top = MulDiv(
    intersection.top - windowRect->top, sourceSize.cy, windowHeight);
  properties->rcSource.right = MulDiv(
    intersection.right - windowRect->left, sourceSize.cx, windowWidth);
  properties->rcSource.bottom = MulDiv(
    intersection.bottom - windowRect->top, sourceSize.cy, windowHeight);
  properties->opacity = 255;
  properties->fVisible = TRUE;
  properties->fSourceClientAreaOnly = FALSE;
  if (properties->rcSource.right <= properties->rcSource.left ||
      properties->rcSource.bottom <= properties->rcSource.top)
  {
    return E_INVALIDARG;
  }
  return S_OK;
}

static BOOL DwmPrivateEqualThumbnailProperties(
  const DWM_THUMBNAIL_PROPERTIES* left,
  const DWM_THUMBNAIL_PROPERTIES* right)
{
  return left->dwFlags == right->dwFlags &&
    EqualRect(&left->rcDestination, &right->rcDestination) &&
    EqualRect(&left->rcSource, &right->rcSource) &&
    left->opacity == right->opacity &&
    left->fVisible == right->fVisible &&
    left->fSourceClientAreaOnly == right->fSourceClientAreaOnly;
}

static void DwmPrivateDestroyShellVisual(DWMPRIVATESHELLVISUAL* shellVisual)
{
  if (shellVisual->thumbnail)
  {
    DwmUnregisterThumbnail(shellVisual->thumbnail);
  }
  DwmPrivateReleaseUnknown((void**)&shellVisual->visual);
  ZeroMemory(shellVisual, sizeof(*shellVisual));
}

static HRESULT DwmPrivateCreateShellVisual(
  DWMPRIVATECAPTURESTATE* state,
  const DWMPRIVATESHELLCANDIDATE* candidate,
  const RECT* desktopSource,
  DWMPRIVATESHELLVISUAL* shellVisual)
{
  HRESULT hr;

  if (!state->createSharedThumbnailVisual)
  {
    return E_NOINTERFACE;
  }
  ZeroMemory(shellVisual, sizeof(*shellVisual));
  shellVisual->hwnd = candidate->hwnd;
  shellVisual->rect = candidate->rect;
  shellVisual->layer = candidate->layer;
  shellVisual->sourceSize = DwmPrivateQueryShellSourceSize(
    state, candidate->hwnd, &candidate->rect);
  hr = DwmPrivateBuildShellThumbnailProperties(
    desktopSource,
    &candidate->rect,
    shellVisual->sourceSize,
    &shellVisual->properties);
  if (SUCCEEDED(hr))
  {
    hr = state->createSharedThumbnailVisual(
      state->hwndDestination,
      candidate->hwnd,
      DWM_PRIVATE_THUMBNAIL_TYPE_ICONIC,
      &shellVisual->properties,
      state->dcompDevice,
      (void**)&shellVisual->visual,
      &shellVisual->thumbnail);
  }
  if (FAILED(hr))
  {
    DwmPrivateDestroyShellVisual(shellVisual);
  }
  return hr;
}

static HRESULT DwmPrivateUpdateShellVisual(
  DWMPRIVATECAPTURESTATE* state,
  const DWMPRIVATESHELLCANDIDATE* candidate,
  const RECT* desktopSource,
  DWMPRIVATESHELLVISUAL* shellVisual)
{
  DWM_THUMBNAIL_PROPERTIES properties;
  SIZE sourceSize = shellVisual->sourceSize;
  HRESULT hr;

  if (!EqualRect(&shellVisual->rect, &candidate->rect))
  {
    sourceSize = DwmPrivateQueryShellSourceSize(
      state, candidate->hwnd, &candidate->rect);
  }
  hr = DwmPrivateBuildShellThumbnailProperties(
    desktopSource, &candidate->rect, sourceSize, &properties);
  if (SUCCEEDED(hr) &&
      !DwmPrivateEqualThumbnailProperties(&shellVisual->properties, &properties))
  {
    hr = DwmUpdateThumbnailProperties(shellVisual->thumbnail, &properties);
  }
  if (SUCCEEDED(hr))
  {
    shellVisual->rect = candidate->rect;
    shellVisual->sourceSize = sourceSize;
    shellVisual->properties = properties;
  }
  return hr;
}

static HRESULT DwmPrivatePopulateViewChildren(
  DWMPRIVATECAPTURESTATE* state,
  const DWMPRIVATESHELLVISUAL* shellVisuals,
  UINT shellVisualCount)
{
  HRESULT hr = state->viewVisual->lpVtbl->RemoveAllVisuals(state->viewVisual);
  UINT index;

  for (index = 0; SUCCEEDED(hr) && index < shellVisualCount; ++index)
  {
    if (DWM_PRIVATE_SHELL_DESKTOP == shellVisuals[index].layer)
    {
      /* With no reference visual, insertAbove=TRUE inserts at the bottom of
         the child list.  Wallpaper/desktop visuals must remain behind the
         shared multi-window visual. */
      hr = state->viewVisual->lpVtbl->AddVisual(
        state->viewVisual, shellVisuals[index].visual, TRUE, NULL);
    }
  }
  if (SUCCEEDED(hr))
  {
    /* With no reference visual, insertAbove=FALSE inserts at the top. */
    hr = state->viewVisual->lpVtbl->AddVisual(
      state->viewVisual, state->sharedVisual, FALSE, NULL);
  }
  for (index = 0; SUCCEEDED(hr) && index < shellVisualCount; ++index)
  {
    if (DWM_PRIVATE_SHELL_TASKBAR == shellVisuals[index].layer)
    {
      /* Taskbars are shell-owned top-level surfaces and belong above both the
         desktop and ordinary application windows. */
      hr = state->viewVisual->lpVtbl->AddVisual(
        state->viewVisual, shellVisuals[index].visual, FALSE, NULL);
    }
  }
  return hr;
}

static HRESULT DwmPrivateConfigureShellVisuals(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* desktopSource)
{
  DWMPRIVATESHELLVISUAL pending[DWM_PRIVATE_MAX_SHELL_VISUALS];
  BOOL pendingNew[DWM_PRIVATE_MAX_SHELL_VISUALS];
  BOOL activeUsed[DWM_PRIVATE_MAX_SHELL_VISUALS];
  BOOL orderChanged;
  HRESULT hr;
  UINT candidateIndex;

  ZeroMemory(pending, sizeof(pending));
  ZeroMemory(pendingNew, sizeof(pendingNew));
  ZeroMemory(activeUsed, sizeof(activeUsed));
  hr = DwmPrivateBuildShellCandidates(state, desktopSource);
  if (FAILED(hr))
  {
    return hr;
  }

  for (candidateIndex = 0;
       candidateIndex < state->shellCandidateCount;
       ++candidateIndex)
  {
    const DWMPRIVATESHELLCANDIDATE* candidate =
      &state->shellCandidates[candidateIndex];
    UINT activeIndex;
    BOOL found = FALSE;

    for (activeIndex = 0;
         activeIndex < state->shellVisualCount;
         ++activeIndex)
    {
      if (!activeUsed[activeIndex] &&
          state->shellVisuals[activeIndex].hwnd == candidate->hwnd &&
          state->shellVisuals[activeIndex].layer == candidate->layer)
      {
        pending[candidateIndex] = state->shellVisuals[activeIndex];
        hr = DwmPrivateUpdateShellVisual(
          state, candidate, desktopSource, &pending[candidateIndex]);
        if (SUCCEEDED(hr))
        {
          state->shellVisuals[activeIndex].rect = pending[candidateIndex].rect;
          state->shellVisuals[activeIndex].sourceSize =
            pending[candidateIndex].sourceSize;
          state->shellVisuals[activeIndex].properties =
            pending[candidateIndex].properties;
        }
        activeUsed[activeIndex] = TRUE;
        found = TRUE;
        break;
      }
    }
    if (!found)
    {
      hr = DwmPrivateCreateShellVisual(
        state, candidate, desktopSource, &pending[candidateIndex]);
      pendingNew[candidateIndex] = SUCCEEDED(hr);
    }
    if (FAILED(hr))
    {
      UINT cleanupIndex;
      for (cleanupIndex = 0;
           cleanupIndex <= candidateIndex;
           ++cleanupIndex)
      {
        if (pendingNew[cleanupIndex])
        {
          DwmPrivateDestroyShellVisual(&pending[cleanupIndex]);
        }
      }
      return hr;
    }
  }

  orderChanged = state->shellVisualCount != state->shellCandidateCount;
  if (!orderChanged)
  {
    for (candidateIndex = 0;
         candidateIndex < state->shellCandidateCount;
         ++candidateIndex)
    {
      if (state->shellVisuals[candidateIndex].hwnd != pending[candidateIndex].hwnd ||
          state->shellVisuals[candidateIndex].layer != pending[candidateIndex].layer)
      {
        orderChanged = TRUE;
        break;
      }
    }
  }
  if (!orderChanged)
  {
    for (candidateIndex = 0;
         candidateIndex < state->shellCandidateCount;
         ++candidateIndex)
    {
      state->shellVisuals[candidateIndex].rect = pending[candidateIndex].rect;
      state->shellVisuals[candidateIndex].sourceSize =
        pending[candidateIndex].sourceSize;
      state->shellVisuals[candidateIndex].properties =
        pending[candidateIndex].properties;
    }
    return S_OK;
  }

  hr = DwmPrivatePopulateViewChildren(
    state, pending, state->shellCandidateCount);
  if (FAILED(hr))
  {
    DwmPrivatePopulateViewChildren(
      state, state->shellVisuals, state->shellVisualCount);
    for (candidateIndex = 0;
         candidateIndex < state->shellCandidateCount;
         ++candidateIndex)
    {
      if (pendingNew[candidateIndex])
      {
        DwmPrivateDestroyShellVisual(&pending[candidateIndex]);
      }
    }
    return hr;
  }

  state->retiredShellVisualCount = 0;
  for (candidateIndex = 0;
       candidateIndex < state->shellVisualCount;
       ++candidateIndex)
  {
    if (!activeUsed[candidateIndex])
    {
      state->retiredShellVisuals[state->retiredShellVisualCount++] =
        state->shellVisuals[candidateIndex];
    }
  }
  ZeroMemory(state->shellVisuals, sizeof(state->shellVisuals));
  CopyMemory(
    state->shellVisuals,
    pending,
    state->shellCandidateCount * sizeof(pending[0]));
  state->shellVisualCount = state->shellCandidateCount;
  state->treeDirty = TRUE;
  return S_OK;
}

static BOOL DwmPrivateWindowArrayEqual(
  const HWND* left,
  UINT leftCount,
  const HWND* right,
  UINT rightCount)
{
  return leftCount == rightCount &&
    !memcmp(left, right, leftCount * sizeof(left[0]));
}

static HRESULT DwmPrivateConfigureDesktop(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* desktopSource)
{
  HWND exclusions[DWM_PRIVATE_MAX_EXCLUSIONS];
  UINT exclusionCount = 1;
  RECT sourceCopy = *desktopSource;
  SIZE destinationSize;
  HRESULT hr;
  UINT shellIndex;

  hr = DwmPrivateConfigureShellVisuals(state, desktopSource);
  if (FAILED(hr))
  {
    return hr;
  }
  destinationSize.cx = sourceCopy.right - sourceCopy.left;
  destinationSize.cy = sourceCopy.bottom - sourceCopy.top;
  if (destinationSize.cx < 1 || destinationSize.cy < 1)
  {
    return E_INVALIDARG;
  }

  exclusions[0] = state->hwndDestination;
  for (shellIndex = 0;
       shellIndex < state->shellVisualCount;
       ++shellIndex)
  {
    UINT existingIndex;
    BOOL exists = FALSE;
    for (existingIndex = 0;
         existingIndex < exclusionCount;
         ++existingIndex)
    {
      if (exclusions[existingIndex] == state->shellVisuals[shellIndex].hwnd)
      {
        exists = TRUE;
        break;
      }
    }
    if (!exists)
    {
      exclusions[exclusionCount++] = state->shellVisuals[shellIndex].hwnd;
    }
  }
  if (state->desktopConfigured &&
      EqualRect(&state->desktopSource, desktopSource) &&
      DwmPrivateWindowArrayEqual(
        state->desktopExclusions,
        state->desktopExclusionCount,
        exclusions,
        exclusionCount))
  {
    return S_OK;
  }

  if (state->useMultiWindow)
  {
    PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL update =
      (PFN_DWMPUPDATESHAREDMULTIWINDOWVISUAL)state->updateSharedVisual;
    hr = update(
      state->thumbnail,
      NULL,
      0,
      exclusions,
      exclusionCount,
      &sourceCopy,
      &destinationSize,
      1);
  }
  else
  {
    PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL update =
      (PFN_DWMPUPDATESHAREDVIRTUALDESKTOPVISUAL)state->updateSharedVisual;
    hr = update(
      state->thumbnail,
      NULL,
      0,
      exclusions,
      exclusionCount,
      &sourceCopy,
      &destinationSize);
  }
  if (SUCCEEDED(hr))
  {
    state->desktopSource = *desktopSource;
    CopyMemory(
      state->desktopExclusions,
      exclusions,
      exclusionCount * sizeof(exclusions[0]));
    state->desktopExclusionCount = exclusionCount;
    state->desktopConfigured = TRUE;
  }
  return hr;
}

static HRESULT DwmPrivateEnsureOverlay(
  DWMPRIVATECAPTURESTATE* state,
  UINT width,
  UINT height)
{
  MAG_IDCompositionSurface* surface = NULL;
  DWORD* pixels = NULL;
  SIZE minimumSize;
  SIZE reservoirSize;
  UINT capacityWidth;
  UINT capacityHeight;
  SIZE_T pixelCount;
  HRESULT hr = S_OK;

  if (state->overlaySurface &&
      width <= state->overlayCapacityWidth &&
      height <= state->overlayCapacityHeight)
  {
    state->overlayWidth = width;
    state->overlayHeight = height;
    return S_OK;
  }
  if (!width || !height)
  {
    return E_INVALIDARG;
  }
  minimumSize.cx = (LONG)width;
  minimumSize.cy = (LONG)height;
  reservoirSize = magGraphicsChooseReservoirSize(
    state->hwndDestination, minimumSize);
  capacityWidth = (UINT)reservoirSize.cx;
  capacityHeight = (UINT)reservoirSize.cy;
  pixelCount = (SIZE_T)capacityWidth * capacityHeight;
  if (!capacityWidth || !capacityHeight ||
      capacityWidth > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      capacityHeight > D3D11_REQ_TEXTURE2D_U_OR_V_DIMENSION ||
      pixelCount > ((SIZE_T)-1) / sizeof(*pixels))
  {
    return E_INVALIDARG;
  }
  pixels = (DWORD*)HeapAlloc(
    GetProcessHeap(), HEAP_ZERO_MEMORY, pixelCount * sizeof(*pixels));
  if (!pixels)
  {
    return E_OUTOFMEMORY;
  }

  hr = state->dcompDevice->lpVtbl->CreateSurface(
    state->dcompDevice,
    capacityWidth,
    capacityHeight,
    DXGI_FORMAT_B8G8R8A8_UNORM,
    DXGI_ALPHA_MODE_PREMULTIPLIED,
    &surface);
  if (SUCCEEDED(hr))
  {
    hr = state->overlayVisual->lpVtbl->SetContent(
      state->overlayVisual, (IUnknown*)surface);
  }
  if (FAILED(hr))
  {
    DwmPrivateReleaseUnknown((void**)&surface);
    HeapFree(GetProcessHeap(), 0, pixels);
    return hr;
  }

  DwmPrivateReleaseUnknown((void**)&state->overlaySurface);
  if (state->overlayPixels)
  {
    HeapFree(GetProcessHeap(), 0, state->overlayPixels);
  }
  state->overlaySurface = surface;
  state->overlayPixels = pixels;
  state->overlayWidth = width;
  state->overlayHeight = height;
  state->overlayCapacityWidth = capacityWidth;
  state->overlayCapacityHeight = capacityHeight;
  state->overlayInitialized = FALSE;
  state->treeDirty = TRUE;
  ++state->overlayResourceGeneration;
  return S_OK;
}

static HRESULT DwmPrivateUpdateOverlay(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* captureDestination,
  const DWMPRIVATEDRAWCOMMAND* drawCommands,
  UINT drawCommandCount)
{
  IDXGISurface* updateSurface = NULL;
  ID3D11Texture2D* updateTexture = NULL;
  const DWORD opaqueBlack = 0xff000000U;
  D3D11_TEXTURE2D_DESC textureDesc;
  D3D11_BOX updateBox;
  RECT updateRect;
  POINT updateOffset;
  HRESULT endHr;
  HRESULT hr;
  BOOL drawBegun = FALSE;
  const UINT updateWidth = state->overlayInitialized
    ? state->overlayWidth
    : state->overlayCapacityWidth;
  const UINT updateHeight = state->overlayInitialized
    ? state->overlayHeight
    : state->overlayCapacityHeight;
  UINT y;
  UINT i;

  /* The first update initializes the complete reservoir.  DWM may expose an
     area beyond the old client extent before WM_WINDOWPOSCHANGED publishes
     the new geometry; transparent reservoir pixels would reveal the HWND's
     redirection surface for one composition and visibly flash. */
  for (y = 0; y < updateHeight; ++y)
  {
    DWORD* row = state->overlayPixels +
      (SIZE_T)y * state->overlayCapacityWidth;
    UINT x;
    for (x = 0; x < updateWidth; ++x)
    {
      row[x] = opaqueBlack;
    }
  }
  if (captureDestination)
  {
    DwmPrivateClearRect(state, captureDestination);
  }
  for (i = 0; i < drawCommandCount; ++i)
  {
    const DWMPRIVATEDRAWCOMMAND* command = &drawCommands[i];
    const DWORD color = DwmPrivatePremultiplyColor(command->color);
    if (DWM_PRIVATE_DRAW_FILL == command->type)
    {
      DwmPrivateFillRect(state, &command->rc, color);
    }
    else if (DWM_PRIVATE_DRAW_STROKE == command->type)
    {
      DwmPrivateStrokeRect(
        state, &command->rc, command->thickness, color);
    }
    else
    {
      return E_INVALIDARG;
    }
  }

  SetRect(
    &updateRect,
    0,
    0,
    (LONG)updateWidth,
    (LONG)updateHeight);
  ZeroMemory(&updateOffset, sizeof(updateOffset));
  hr = state->overlaySurface->lpVtbl->BeginDraw(
    state->overlaySurface,
    &updateRect,
    &IID_IDXGISurface,
    (void**)&updateSurface,
    &updateOffset);
  if (FAILED(hr))
  {
    return hr;
  }
  if (SUCCEEDED(hr))
  {
    drawBegun = TRUE;
    hr = IDXGISurface_QueryInterface(
      updateSurface, &IID_ID3D11Texture2D, (void**)&updateTexture);
  }
  if (SUCCEEDED(hr))
  {
    ID3D11Texture2D_GetDesc(updateTexture, &textureDesc);
    if (updateWidth > textureDesc.Width)
    {
      hr = E_INVALIDARG;
    }
    else if (updateHeight > textureDesc.Height)
    {
      hr = E_INVALIDARG;
    }
    else if (updateOffset.x < 0 || updateOffset.y < 0)
    {
      hr = E_INVALIDARG;
    }
    else if ((UINT)updateOffset.x >
             textureDesc.Width - updateWidth)
    {
      hr = E_INVALIDARG;
    }
    else if ((UINT)updateOffset.y >
             textureDesc.Height - updateHeight)
    {
      hr = E_INVALIDARG;
    }
    else
    {
      ZeroMemory(&updateBox, sizeof(updateBox));
      updateBox.left = (UINT)updateOffset.x;
      updateBox.top = (UINT)updateOffset.y;
      updateBox.right = updateBox.left + updateWidth;
      updateBox.bottom = updateBox.top + updateHeight;
      updateBox.back = 1;
      ID3D11DeviceContext_UpdateSubresource(
        state->d3dContext,
        (ID3D11Resource*)updateTexture,
        0,
        &updateBox,
        state->overlayPixels,
        state->overlayCapacityWidth * sizeof(DWORD),
        0);
    }
  }
  DwmPrivateReleaseUnknown((void**)&updateTexture);
  DwmPrivateReleaseUnknown((void**)&updateSurface);
  if (drawBegun)
  {
    endHr = state->overlaySurface->lpVtbl->EndDraw(state->overlaySurface);
    if (SUCCEEDED(hr))
    {
      hr = endHr;
      if (SUCCEEDED(hr))
      {
        state->overlayInitialized = TRUE;
      }
    }
  }
  return hr;
}

void DwmPrivateCaptureDestroy(DWMPRIVATECAPTURESTATE* state)
{
  UINT index;

  if (!state)
  {
    return;
  }
  if (state->dcompTarget)
  {
    state->dcompTarget->lpVtbl->SetRoot(state->dcompTarget, NULL);
  }
  if (state->dcompDevice)
  {
    state->dcompDevice->lpVtbl->Commit(state->dcompDevice);
    DwmFlush();
  }
  if (state->livePreviewExcluded)
  {
    DwmPrivateSetExcludedFromLivePreview(state, FALSE);
  }
  for (index = 0; index < state->shellVisualCount; ++index)
  {
    DwmPrivateDestroyShellVisual(&state->shellVisuals[index]);
  }
  for (index = 0; index < state->retiredShellVisualCount; ++index)
  {
    DwmPrivateDestroyShellVisual(&state->retiredShellVisuals[index]);
  }
  if (state->thumbnail)
  {
    DwmUnregisterThumbnail(state->thumbnail);
  }
  DwmPrivateReleaseUnknown((void**)&state->overlaySurface);
  DwmPrivateReleaseUnknown((void**)&state->overlayVisual);
  DwmPrivateReleaseUnknown((void**)&state->sharedVisual);
  DwmPrivateReleaseUnknown((void**)&state->viewVisual);
  DwmPrivateReleaseUnknown((void**)&state->captureVisual);
  DwmPrivateReleaseUnknown((void**)&state->rootVisual);
  DwmPrivateReleaseUnknown((void**)&state->dcompTarget);
  DwmPrivateReleaseUnknown((void**)&state->dcompDevice);
  DwmPrivateReleaseUnknown((void**)&state->dxgiDevice);
  DwmPrivateReleaseUnknown((void**)&state->d3dContext);
  DwmPrivateReleaseUnknown((void**)&state->d3dDevice);
  if (state->overlayPixels)
  {
    HeapFree(GetProcessHeap(), 0, state->overlayPixels);
  }
  if (state->hUser32)
  {
    FreeLibrary(state->hUser32);
  }
  if (state->hDwmApi)
  {
    FreeLibrary(state->hDwmApi);
  }
  if (state->hDcomp)
  {
    FreeLibrary(state->hDcomp);
  }
  HeapFree(GetProcessHeap(), 0, state);
}

BOOL DwmPrivateCaptureCreate(
  HWND hWnd,
  DWMPRIVATECAPTURESTATE** stateOut)
{
  DWMPRIVATECAPTURESTATE* state;
  D3D_FEATURE_LEVEL featureLevel;
  HRESULT hr;

  if (!hWnd || !stateOut)
  {
    return FALSE;
  }
  *stateOut = NULL;
  state = (DWMPRIVATECAPTURESTATE*)HeapAlloc(
    GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
  if (!state)
  {
    return FALSE;
  }
  state->hwndDestination = hWnd;
  state->hDcomp = LoadLibrary(TEXT("dcomp.dll"));
  state->hDwmApi = LoadLibrary(TEXT("dwmapi.dll"));
  state->hUser32 = LoadLibrary(TEXT("user32.dll"));
  if (state->hDcomp)
  {
    state->createDcompDevice = (PFN_DCOMPOSITIONCREATEDEVICE3)GetProcAddress(
      state->hDcomp, "DCompositionCreateDevice3");
  }
  if (state->hDwmApi)
  {
    state->createSharedThumbnailVisual =
      (PFN_DWMPCREATESHAREDTHUMBNAILVISUAL)GetProcAddress(
        state->hDwmApi,
        MAKEINTRESOURCEA(DWM_ORD_CREATE_SHARED_THUMBNAIL_VISUAL));
    state->queryWindowThumbnailSourceSize =
      (PFN_DWMPQUERYWINDOWTHUMBNAILSOURCESIZE)GetProcAddress(
        state->hDwmApi,
        MAKEINTRESOURCEA(DWM_ORD_QUERY_WINDOW_THUMBNAIL_SOURCE_SIZE));
    state->createSharedVisual =
      (PFN_DWMPCREATESHAREDDESKTOPVISUAL)GetProcAddress(
        state->hDwmApi,
        MAKEINTRESOURCEA(DWM_ORD_CREATE_SHARED_DESKTOP_VISUAL));
    state->updateSharedVisual = GetProcAddress(
      state->hDwmApi,
      MAKEINTRESOURCEA(DWM_ORD_UPDATE_SHARED_DESKTOP_VISUAL));
  }
  if (state->hUser32)
  {
    state->setWindowCompositionAttribute =
      (PFN_SETWINDOWCOMPOSITIONATTRIBUTE)GetProcAddress(
        state->hUser32, "SetWindowCompositionAttribute");
  }
  if (!state->createDcompDevice || !state->createSharedThumbnailVisual ||
      !state->createSharedVisual || !state->updateSharedVisual)
  {
    DwmPrivateCaptureDestroy(state);
    return FALSE;
  }

  hr = D3D11CreateDevice(
    NULL,
    D3D_DRIVER_TYPE_HARDWARE,
    NULL,
    D3D11_CREATE_DEVICE_BGRA_SUPPORT,
    NULL,
    0,
    D3D11_SDK_VERSION,
    &state->d3dDevice,
    &featureLevel,
    &state->d3dContext);
  if (FAILED(hr))
  {
    hr = D3D11CreateDevice(
      NULL,
      D3D_DRIVER_TYPE_WARP,
      NULL,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT,
      NULL,
      0,
      D3D11_SDK_VERSION,
      &state->d3dDevice,
      &featureLevel,
      &state->d3dContext);
  }
  if (SUCCEEDED(hr))
  {
    hr = ID3D11Device_QueryInterface(
      state->d3dDevice, &IID_IDXGIDevice, (void**)&state->dxgiDevice);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->createDcompDevice(
      (IUnknown*)state->dxgiDevice,
      &IID_MAG_IDCompositionDesktopDevice,
      (void**)&state->dcompDevice);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->createSharedVisual(
      hWnd,
      state->dcompDevice,
      (void**)&state->sharedVisual,
      &state->thumbnail);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->lpVtbl->CreateVisual(
      state->dcompDevice, &state->rootVisual);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->lpVtbl->CreateVisual(
      state->dcompDevice, &state->captureVisual);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->lpVtbl->CreateVisual(
      state->dcompDevice, &state->viewVisual);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->lpVtbl->CreateVisual(
      state->dcompDevice, &state->overlayVisual);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->dcompDevice->lpVtbl->CreateTargetForHwnd(
      state->dcompDevice, hWnd, TRUE, &state->dcompTarget);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->viewVisual->lpVtbl->AddVisual(
      state->viewVisual, state->sharedVisual, TRUE, NULL);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->captureVisual->lpVtbl->AddVisual(
      state->captureVisual, state->viewVisual, TRUE, NULL);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->rootVisual->lpVtbl->AddVisual(
      state->rootVisual, state->captureVisual, TRUE, NULL);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->rootVisual->lpVtbl->AddVisual(
      state->rootVisual,
      state->overlayVisual,
      TRUE,
      state->captureVisual);
  }
  if (SUCCEEDED(hr))
  {
    hr = state->dcompTarget->lpVtbl->SetRoot(
      state->dcompTarget, state->rootVisual);
  }
  if (SUCCEEDED(hr))
  {
    state->livePreviewExcluded =
      DwmPrivateSetExcludedFromLivePreview(state, TRUE);
    state->useMultiWindow = DwmPrivateIsMultiWindowBuild();
    /* SetRoot and the empty child tree stay pending.  The first Update adds
       the complete desktop/window/taskbar stack, initialized overlay, view
       transform, and clip before publishing one atomic transaction. */
  }
  if (FAILED(hr))
  {
    DwmPrivateCaptureDestroy(state);
    return FALSE;
  }
  *stateOut = state;
  return TRUE;
}

BOOL DwmPrivateCalculateViewTransform(
  const RECT* desktop,
  const RECT* viewSource,
  const RECT* viewDestination,
  DWMPRIVATEVIEWTRANSFORM* transform)
{
  const LONG desktopWidth = desktop ? desktop->right - desktop->left : 0;
  const LONG desktopHeight = desktop ? desktop->bottom - desktop->top : 0;
  const LONG sourceWidth = viewSource
    ? viewSource->right - viewSource->left : 0;
  const LONG sourceHeight = viewSource
    ? viewSource->bottom - viewSource->top : 0;
  const LONG destinationWidth = viewDestination
    ? viewDestination->right - viewDestination->left : 0;
  const LONG destinationHeight = viewDestination
    ? viewDestination->bottom - viewDestination->top : 0;

  if (!transform || desktopWidth < 1 || desktopHeight < 1 ||
      sourceWidth < 1 || sourceHeight < 1 ||
      destinationWidth < 1 || destinationHeight < 1 ||
      viewSource->left < desktop->left || viewSource->top < desktop->top ||
      viewSource->right > desktop->right ||
      viewSource->bottom > desktop->bottom)
  {
    return FALSE;
  }
  transform->m11 = (FLOAT)destinationWidth / sourceWidth;
  transform->m12 = 0.0f;
  transform->m21 = 0.0f;
  transform->m22 = (FLOAT)destinationHeight / sourceHeight;
  transform->dx = (FLOAT)viewDestination->left -
    (FLOAT)(viewSource->left - desktop->left) * transform->m11;
  transform->dy = (FLOAT)viewDestination->top -
    (FLOAT)(viewSource->top - desktop->top) * transform->m22;
  transform->clip = *viewDestination;
  return TRUE;
}

static HRESULT DwmPrivateConfigureView(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* desktop,
  const RECT* viewSource,
  const RECT* viewDestination)
{
  const BOOL visible = viewSource && viewDestination;
  HRESULT hr = S_OK;

  if (state->viewConfigured && state->viewVisible == visible &&
      (!visible ||
       (EqualRect(&state->viewDesktop, desktop) &&
        EqualRect(&state->viewSource, viewSource) &&
        EqualRect(&state->viewDestination, viewDestination))))
  {
    return S_OK;
  }
  if (visible)
  {
    DWMPRIVATEVIEWTRANSFORM properties;
    MAG_D2D_MATRIX_3X2_F matrix;
    MAG_D2D_RECT_F clip;

    if (!DwmPrivateCalculateViewTransform(
          desktop, viewSource, viewDestination, &properties))
    {
      return E_INVALIDARG;
    }
    matrix.m11 = properties.m11;
    matrix.m12 = properties.m12;
    matrix.m21 = properties.m21;
    matrix.m22 = properties.m22;
    matrix.dx = properties.dx;
    matrix.dy = properties.dy;
    clip.left = (FLOAT)properties.clip.left;
    clip.top = (FLOAT)properties.clip.top;
    clip.right = (FLOAT)properties.clip.right;
    clip.bottom = (FLOAT)properties.clip.bottom;
    hr = state->viewVisual->lpVtbl->SetTransformMatrix(
      state->viewVisual, &matrix);
    if (FAILED(hr))
    {
      return hr;
    }
    if (SUCCEEDED(hr))
    {
      hr = state->captureVisual->lpVtbl->SetClipRect(
        state->captureVisual, &clip);
      if (FAILED(hr))
      {
        return hr;
      }
    }
  }
  else
  {
    MAG_D2D_RECT_F emptyClip;
    ZeroMemory(&emptyClip, sizeof(emptyClip));
    hr = state->captureVisual->lpVtbl->SetClipRect(
      state->captureVisual, &emptyClip);
  }
  if (SUCCEEDED(hr))
  {
    state->viewConfigured = TRUE;
    state->viewVisible = visible;
    state->viewDesktop = *desktop;
    if (visible)
    {
      state->viewSource = *viewSource;
      state->viewDestination = *viewDestination;
    }
    else
    {
      SetRectEmpty(&state->viewSource);
      SetRectEmpty(&state->viewDestination);
    }
    state->treeDirty = TRUE;
  }
  return hr;
}

BOOL DwmPrivateCaptureUpdate(
  DWMPRIVATECAPTURESTATE* state,
  const RECT* desktop,
  const RECT* viewSource,
  const RECT* viewDestination,
  SIZE targetSize,
  const DWMPRIVATEDRAWCOMMAND* drawCommands,
  UINT drawCommandCount,
  BOOL restartSequence,
  BOOL synchronize)
{
  HRESULT hr;

  if (!state || !desktop || IsRectEmpty(desktop) ||
      targetSize.cx < 1 || targetSize.cy < 1 ||
      (drawCommandCount && !drawCommands) ||
      drawCommandCount > DWM_PRIVATE_MAX_DRAW_COMMANDS ||
      (!!viewSource != !!viewDestination))
  {
    return FALSE;
  }
  hr = DwmPrivateEnsureOverlay(
    state, (UINT)targetSize.cx, (UINT)targetSize.cy);
  if (SUCCEEDED(hr))
  {
    hr = DwmPrivateConfigureDesktop(state, desktop);
  }
  if (SUCCEEDED(hr))
  {
    hr = DwmPrivateConfigureView(
      state, desktop, viewSource, viewDestination);
  }
  if (SUCCEEDED(hr))
  {
    hr = DwmPrivateUpdateOverlay(
      state,
      viewDestination,
      drawCommands,
      drawCommandCount);
  }
  if (SUCCEEDED(hr))
  {
    UINT index;

    hr = state->dcompDevice->lpVtbl->Commit(state->dcompDevice);
    if (SUCCEEDED(hr))
    {
      state->treeDirty = FALSE;
      for (index = 0;
           index < state->retiredShellVisualCount;
           ++index)
      {
        DwmPrivateDestroyShellVisual(&state->retiredShellVisuals[index]);
      }
      state->retiredShellVisualCount = 0;
    }
  }
  if (SUCCEEDED(hr) && synchronize)
  {
    hr = DwmFlush();
  }
  if (FAILED(hr))
  {
    SetLastError((DWORD)hr);
    return FALSE;
  }
  UNREFERENCED_PARAMETER(restartSequence);
  return TRUE;
}

UINT64 DwmPrivateCaptureGetResourceGeneration(
  const DWMPRIVATECAPTURESTATE* state)
{
  return state ? state->overlayResourceGeneration : 0;
}

UINT DwmPrivateCaptureGetWindowCoverage(
  const DWMPRIVATECAPTURESTATE* state,
  HWND hwnd)
{
  UINT coverage = DWM_PRIVATE_WINDOW_COVERAGE_NONE;
  UINT index;

  if (!state || !hwnd || !IsWindow(hwnd))
  {
    return coverage;
  }
  for (index = 0; index < state->desktopExclusionCount; ++index)
  {
    if (state->desktopExclusions[index] == hwnd)
    {
      coverage |= DWM_PRIVATE_WINDOW_COVERAGE_EXCLUDED;
      break;
    }
  }
  for (index = 0; index < state->shellVisualCount; ++index)
  {
    if (state->shellVisuals[index].hwnd == hwnd)
    {
      coverage |= DWM_PRIVATE_SHELL_DESKTOP == state->shellVisuals[index].layer
        ? DWM_PRIVATE_WINDOW_COVERAGE_DESKTOP
        : DWM_PRIVATE_WINDOW_COVERAGE_TASKBAR;
      return coverage;
    }
  }
  if (state->desktopConfigured &&
      !(coverage & DWM_PRIVATE_WINDOW_COVERAGE_EXCLUDED))
  {
    /* The private multi-window visual is configured with a NULL include list,
       which means every top-level window not present in the exclusion list. */
    coverage |= DWM_PRIVATE_WINDOW_COVERAGE_SHARED;
  }
  return coverage;
}
