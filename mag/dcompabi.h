#pragma once

#include <windows.h>
#include <unknwn.h>
#include <dxgi1_2.h>

/* The Windows SDK dcomp.h interface declarations use C++ overloads and
   references.  DirectComposition is still a COM ABI, so the C build uses the
   stable vtable layout directly and names only the slots MAG consumes. */

typedef struct MAG_DCOMPOSITION_FRAME_STATISTICS
{
  LARGE_INTEGER lastFrameTime;
  struct { UINT Numerator; UINT Denominator; } currentCompositionRate;
  LARGE_INTEGER currentTime;
  LARGE_INTEGER timeFrequency;
  LARGE_INTEGER nextEstimatedFrameTime;
} MAG_DCOMPOSITION_FRAME_STATISTICS;

typedef struct MAG_IDCompositionDevice MAG_IDCompositionDevice;
typedef struct MAG_IDCompositionDesktopDevice MAG_IDCompositionDesktopDevice;
typedef struct MAG_IDCompositionTarget MAG_IDCompositionTarget;
typedef struct MAG_IDCompositionVisual MAG_IDCompositionVisual;
typedef struct MAG_IDCompositionEffectGroup MAG_IDCompositionEffectGroup;
typedef struct MAG_IDCompositionSurface MAG_IDCompositionSurface;

typedef struct MAG_IDCompositionDeviceVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionDevice*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionDevice*);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionDevice*);
  HRESULT (STDMETHODCALLTYPE* Commit)(MAG_IDCompositionDevice*);
  HRESULT (STDMETHODCALLTYPE* WaitForCommitCompletion)(MAG_IDCompositionDevice*);
  HRESULT (STDMETHODCALLTYPE* GetFrameStatistics)(MAG_IDCompositionDevice*, MAG_DCOMPOSITION_FRAME_STATISTICS*);
  HRESULT (STDMETHODCALLTYPE* CreateTargetForHwnd)(MAG_IDCompositionDevice*, HWND, BOOL, MAG_IDCompositionTarget**);
  HRESULT (STDMETHODCALLTYPE* CreateVisual)(MAG_IDCompositionDevice*, MAG_IDCompositionVisual**);
  HRESULT (STDMETHODCALLTYPE* CreateSurface)(MAG_IDCompositionDevice*, UINT, UINT, DXGI_FORMAT, DXGI_ALPHA_MODE, MAG_IDCompositionSurface**);
  void* CreateVirtualSurface;
  HRESULT (STDMETHODCALLTYPE* CreateSurfaceFromHandle)(MAG_IDCompositionDevice*, HANDLE, IUnknown**);
  void* CreateSurfaceFromHwnd;
  void* CreateTranslateTransform;
  void* CreateScaleTransform;
  void* CreateRotateTransform;
  void* CreateSkewTransform;
  void* CreateMatrixTransform;
  void* CreateTransformGroup;
  void* CreateTranslateTransform3D;
  void* CreateScaleTransform3D;
  void* CreateRotateTransform3D;
  void* CreateMatrixTransform3D;
  void* CreateTransform3DGroup;
  HRESULT (STDMETHODCALLTYPE* CreateEffectGroup)(MAG_IDCompositionDevice*, MAG_IDCompositionEffectGroup**);
  void* CreateRectangleClip;
  void* CreateAnimation;
  void* CheckDeviceState;
} MAG_IDCompositionDeviceVtbl;

struct MAG_IDCompositionDevice
{
  const MAG_IDCompositionDeviceVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionDesktopDeviceVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionDesktopDevice*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionDesktopDevice*);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionDesktopDevice*);
  HRESULT (STDMETHODCALLTYPE* Commit)(MAG_IDCompositionDesktopDevice*);
  HRESULT (STDMETHODCALLTYPE* WaitForCommitCompletion)(MAG_IDCompositionDesktopDevice*);
  HRESULT (STDMETHODCALLTYPE* GetFrameStatistics)(MAG_IDCompositionDesktopDevice*, MAG_DCOMPOSITION_FRAME_STATISTICS*);
  HRESULT (STDMETHODCALLTYPE* CreateVisual)(MAG_IDCompositionDesktopDevice*, MAG_IDCompositionVisual**);
  void* CreateSurfaceFactory;
  HRESULT (STDMETHODCALLTYPE* CreateSurface)(MAG_IDCompositionDesktopDevice*, UINT, UINT, DXGI_FORMAT, DXGI_ALPHA_MODE, MAG_IDCompositionSurface**);
  void* CreateVirtualSurface;
  void* CreateTranslateTransform;
  void* CreateScaleTransform;
  void* CreateRotateTransform;
  void* CreateSkewTransform;
  void* CreateMatrixTransform;
  void* CreateTransformGroup;
  void* CreateTranslateTransform3D;
  void* CreateScaleTransform3D;
  void* CreateRotateTransform3D;
  void* CreateMatrixTransform3D;
  void* CreateTransform3DGroup;
  void* CreateEffectGroup;
  void* CreateRectangleClip;
  void* CreateAnimation;
  HRESULT (STDMETHODCALLTYPE* CreateTargetForHwnd)(MAG_IDCompositionDesktopDevice*, HWND, BOOL, MAG_IDCompositionTarget**);
  HRESULT (STDMETHODCALLTYPE* CreateSurfaceFromHandle)(MAG_IDCompositionDesktopDevice*, HANDLE, IUnknown**);
  void* CreateSurfaceFromHwnd;
} MAG_IDCompositionDesktopDeviceVtbl;

struct MAG_IDCompositionDesktopDevice
{
  const MAG_IDCompositionDesktopDeviceVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionSurfaceVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionSurface*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionSurface*);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionSurface*);
  HRESULT (STDMETHODCALLTYPE* BeginDraw)(MAG_IDCompositionSurface*, const RECT*, REFIID, void**, POINT*);
  HRESULT (STDMETHODCALLTYPE* EndDraw)(MAG_IDCompositionSurface*);
  HRESULT (STDMETHODCALLTYPE* SuspendDraw)(MAG_IDCompositionSurface*);
  HRESULT (STDMETHODCALLTYPE* ResumeDraw)(MAG_IDCompositionSurface*);
  HRESULT (STDMETHODCALLTYPE* Scroll)(MAG_IDCompositionSurface*, const RECT*, const RECT*, int, int);
} MAG_IDCompositionSurfaceVtbl;

struct MAG_IDCompositionSurface
{
  const MAG_IDCompositionSurfaceVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionTargetVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionTarget*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionTarget*);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionTarget*);
  HRESULT (STDMETHODCALLTYPE* SetRoot)(MAG_IDCompositionTarget*, MAG_IDCompositionVisual*);
} MAG_IDCompositionTargetVtbl;

struct MAG_IDCompositionTarget
{
  const MAG_IDCompositionTargetVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionVisualVtbl
{
  /* MSVC lays the overloaded object-taking slots before their scalar/struct
     counterparts.  That ABI order differs from their textual order in
     dcomp.h and is required when consuming the Windows implementation. */
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionVisual*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionVisual*);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionVisual*);
  void* SetOffsetXAnimation;
  void* SetOffsetXValue;
  void* SetOffsetYAnimation;
  void* SetOffsetYValue;
  void* SetTransformObject;
  HRESULT (STDMETHODCALLTYPE* SetTransformMatrix)(MAG_IDCompositionVisual*, const void*);
  void* SetTransformParent;
  HRESULT (STDMETHODCALLTYPE* SetEffect)(MAG_IDCompositionVisual*, void*);
  void* SetBitmapInterpolationMode;
  void* SetBorderMode;
  void* SetClipObject;
  HRESULT (STDMETHODCALLTYPE* SetClipRect)(MAG_IDCompositionVisual*, const void*);
  HRESULT (STDMETHODCALLTYPE* SetContent)(MAG_IDCompositionVisual*, IUnknown*);
  HRESULT (STDMETHODCALLTYPE* AddVisual)(MAG_IDCompositionVisual*, MAG_IDCompositionVisual*, BOOL, MAG_IDCompositionVisual*);
  HRESULT (STDMETHODCALLTYPE* RemoveVisual)(MAG_IDCompositionVisual*, MAG_IDCompositionVisual*);
  HRESULT (STDMETHODCALLTYPE* RemoveAllVisuals)(MAG_IDCompositionVisual*);
  void* SetCompositeMode;
} MAG_IDCompositionVisualVtbl;

struct MAG_IDCompositionVisual
{
  const MAG_IDCompositionVisualVtbl* lpVtbl;
};

typedef struct MAG_IDCompositionEffectGroupVtbl
{
  HRESULT (STDMETHODCALLTYPE* QueryInterface)(MAG_IDCompositionEffectGroup*, REFIID, void**);
  ULONG (STDMETHODCALLTYPE* AddRef)(MAG_IDCompositionEffectGroup*);
  ULONG (STDMETHODCALLTYPE* Release)(MAG_IDCompositionEffectGroup*);
  void* SetOpacityAnimation;
  HRESULT (STDMETHODCALLTYPE* SetOpacityValue)(MAG_IDCompositionEffectGroup*, FLOAT);
  void* SetTransform3D;
} MAG_IDCompositionEffectGroupVtbl;

struct MAG_IDCompositionEffectGroup
{
  const MAG_IDCompositionEffectGroupVtbl* lpVtbl;
};

static const IID IID_MAG_IDCompositionDevice =
  { 0xc37ea93a, 0xe7aa, 0x450d, { 0xb1, 0x6f, 0x97, 0x46, 0xcb, 0x04, 0x07, 0xf3 } };
static const IID IID_MAG_IDCompositionDesktopDevice =
  { 0x5f4633fe, 0x1e08, 0x4cb8, { 0x8c, 0x75, 0xce, 0x24, 0x33, 0x3f, 0x56, 0x02 } };

#define MAG_COMPOSITIONOBJECT_ALL_ACCESS 0x0003L

HRESULT WINAPI DCompositionCreateDevice(
  IUnknown* renderingDevice,
  REFIID iid,
  void** dcompositionDevice);
HRESULT WINAPI DCompositionCreateSurfaceHandle(
  DWORD desiredAccess,
  SECURITY_ATTRIBUTES* securityAttributes,
  HANDLE* surfaceHandle);
