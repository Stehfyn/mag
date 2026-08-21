#pragma once

#include "framework.h"
#include "dwmprivate.h"
#include "graphics.h"
#include "graphics_layered.h"
#include "presentation.h"
#include "ui_renderer.h"

#include <d3d11.h>
#include <dxgi1_2.h>

#define CHANNELS (4)
#define BITS_PER_PIXEL (CHANNELS << 3) // 8 bits = 2^3 
#define SURFACE_BYTES(rc) (CHANNELS * RECTWIDTH((rc)) * RECTHEIGHT((rc)))

typedef enum MAGVIEWMODE
{
  MAG_VIEW_WINDOW = 0,
  MAG_VIEW_FOLLOW_MOUSE,
  MAG_VIEW_LENS,
  MAG_VIEW_COUNT
} MAGVIEWMODE;

typedef struct DXGIOUTPUTCAPTURE
{
  ID3D11Device*             d3dDevice;
  ID3D11DeviceContext*      d3dContext;
  IDXGIOutputDuplication*   dxgiDuplication;
  ID3D11Texture2D*          dxgiFrameTexture;
  ID3D11Texture2D*          dxgiStagingTexture;
  HMONITOR                  hMonitor;
  RECT                      rcOutput;
  UINT                      dxgiStagingWidth;
  UINT                      dxgiStagingHeight;
  BOOL                      fHasFrame;
} DXGIOUTPUTCAPTURE, *LPDXGIOUTPUTCAPTURE;

typedef struct WGCMONITORCAPTURE
{
  ID3D11Device*             d3dDevice;
  ID3D11DeviceContext*      d3dContext;
  IUnknown*                 wgcDevice;
  IUnknown*                 wgcItem;
  IUnknown*                 wgcFramePool;
  IUnknown*                 wgcSession;
  ID3D11Texture2D*          wgcFrameTexture;
  ID3D11Texture2D*          wgcStagingTexture;
  HMONITOR                  hMonitor;
  RECT                      rcOutput;
  UINT                      wgcFrameWidth;
  UINT                      wgcFrameHeight;
  UINT                      wgcStagingWidth;
  UINT                      wgcStagingHeight;
  BOOL                      fHasFrame;
} WGCMONITORCAPTURE, *LPWGCMONITORCAPTURE;

typedef struct DWMTHUMBNAILCAPTURE
{
  HTHUMBNAIL hThumbnail;
  HWND       hwndSource;
} DWMTHUMBNAILCAPTURE, *LPDWMTHUMBNAILCAPTURE;

typedef struct DWMPRIVATEVISUALCAPTURE
{
  DWMPRIVATECAPTURESTATE* state;
} DWMPRIVATEVISUALCAPTURE, *LPDWMPRIVATEVISUALCAPTURE;

typedef struct MAGVIEWGEOMETRY
{
  RECT rcClient;
  RECT rcWindow;
  RECT rcCapture;
  RECT rcSource;
  RECT rcClippedSource;
  RECT rcDestination;
  SIZE targetSize;
  BOOL sourceVisible;
} MAGVIEWGEOMETRY, *LPMAGVIEWGEOMETRY;

typedef struct MAGSTATE
{
  BOOL             fTrackCursor;
  BOOL             fWinRtInitialized;
  BOOL             fUseSourceOrigin;
  BOOL             fMouseRelativeZoom;
  BOOL             fMiniMapDragging;
  BOOL             fMiniMapHoldVisible;
  BOOL             fMiniMapHaveLastCursor;
  BOOL             fSourceOriginPinned;
  BOOL             fInSizeMove;
  BOOL             fRenderMessageDriven;
  MAGVIEWMODE      viewMode;
  GRAPHICSAPI      graphicsApi;
  UIGRAPHICSAPI    uiGraphicsApi;
  TEXTRENDERER     textRenderer;
  CAPTUREAPI       captureApi;
  CAPTUREAPI       activeCaptureApi;
  MAGPRESENTATIONSETTINGS presentationSettings;
  MAGPRESENTATIONSETTINGS resolvedPresentation;
  MAGPRESENTATIONSTATUS presentationStatus;
  MAGLAYEREDPRESENTER* layeredPresenter;
  SRWLOCK          graphicsLock;
  const MAGGRAPHICSBACKEND* graphicsBackend;
  void*            graphicsState;
  void*            parkedOpenGlState;
  void*            parkedVulkanState;
  POINT            pt;
  POINT            ptSourceOrigin;
  POINT            ptMiniMapDragOffset;
  POINT            ptMiniMapLastCursor;
  DWORD            dwMiniMapLastActivity;
  RECT             rc;
  DISPLAYINFO      di;
  BITMAPINFOHEADER bi;
  MAGVIEWGEOMETRY  viewGeometry;
  BOOL             fViewGeometryActive;
  MAGPIXELBUFFER   frame;
  UINT             frameCapacityWidth;
  UINT             frameCapacityHeight;
  UINT64           captureSurfaceGeneration;
  MAGUIDRAWLIST    uiDrawList;
  MAGUIRENDERER*   uiRenderer;
  MAGPIXELBUFFER   presentationFrame;
  SIZE             presentedContentSize;
  BOOL             fPresentedContentValid;
  BOOL             fInResizePresent;
  BOOL             fGraphicsPresentationEnabled;
  BOOL             fResizeContractViolation;
  BOOL             fForceGraphicsRecreate;
  BOOL             fGeometryTransition;
  BOOL             fDeferredResize;
  SIZE             deferredClientSize;
  UINT64           geometryEpoch;
  UINT64           committedGeometryEpoch;
  UINT64           presentedGeometryEpoch;
  UINT64           stateTransitionEpoch;
  UINT64           presentedStateTransitionEpoch;
  LONGLONG         presentedTargetFrame;
  MAGPRESENTINTENT presentIntent;
  BOOL             fPresentIntentActive;
  UINT             resizePrecommitCount;
  UINT             resizeCommitCount;
  FLOAT            fScale;
  HDC              hCaptureDC;
  HDC              hDesktopDC;
  HBITMAP          hBitmapBg;
  HBITMAP          hBitmapOld;
  DXGIOUTPUTCAPTURE dxgiOutputs[MAX_ENUM_MONITORS];
  WGCMONITORCAPTURE wgcMonitors[MAX_ENUM_MONITORS];
  DWMTHUMBNAILCAPTURE dwmThumbnail;
  DWMPRIVATEVISUALCAPTURE dwmPrivate;
  FLOAT            outlineColor[CHANNELS];
  FLOAT            fTexScaler;

} MAGSTATE, *LPMAGSTATE;

void renderInit(HWND hWnd);

void renderCleanup(HWND hWnd);

BOOL renderResizeCapture(HWND hWnd);
BOOL renderPrepareWindowResize(HWND hWnd, SIZE proposedClientSize);
BOOL renderPresentCommittedGeometry(HWND hWnd);
BOOL renderSubmitStateTransitionFrame(HWND hWnd, BOOL synchronize);

BOOL renderSetGraphicsApi(HWND hWnd, GRAPHICSAPI api, LPTSTR reason, UINT reasonCount);
BOOL renderApplyPresentationSettings(
  HWND hWnd,
  GRAPHICSAPI api,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  LPTSTR reason,
  UINT reasonCount);
BOOL renderApplySettings(
  HWND hWnd,
  GRAPHICSAPI api,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  LPTSTR reason,
  UINT reasonCount);
BOOL renderApplyFullSettings(
  HWND hWnd,
  GRAPHICSAPI api,
  CAPTUREAPI captureApi,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  const MAGPRESENTATIONSETTINGS* presentation,
  LPTSTR reason,
  UINT reasonCount);
BOOL renderSetUiRendering(
  HWND hWnd,
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  LPTSTR reason,
  UINT reasonCount);

void renderSetMessageDriven(HWND hWnd, BOOL fMessageDriven);
HANDLE renderDuplicateFrameWaitHandle(HWND hWnd);

void renderRender(HWND hWnd);
BOOL renderSubmit(HWND hWnd);
BOOL renderSubmitLiveFrame(HWND hWnd);
int renderRunGraphicsSmoke(
  HWND hWnd,
  HWND hDesktopFixture,
  HWND hTaskbarFixture,
  HWND hPeerFixture);
int renderRunDwmPrivateSmoke(
  HWND hWnd,
  HWND hDesktopFixture,
  HWND hTaskbarFixture,
  HWND hPeerFixture);

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax);
BOOL render_calculateZoomedSourceOrigin(
  const RECT* oldSource,
  SIZE clientSize,
  FLOAT scaler,
  DOUBLE anchorU,
  DOUBLE anchorV,
  const RECT* capture,
  POINT* sourceOrigin);

void render_computeSourceRect(HWND hWnd, RECT* lprcSource);

BOOL render_minimapHitTest(HWND hWnd, POINT ptClient);
void render_minimapNotifyActivity(HWND hWnd);
BOOL render_minimapBeginDrag(HWND hWnd, POINT ptClient);
BOOL render_minimapDrag(HWND hWnd, POINT ptClient);
void render_minimapEndDrag(HWND hWnd);
