#pragma once

#include "framework.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <gl/GL.h>

#define CHANNELS (4)
#define BITS_PER_PIXEL (CHANNELS << 3) // 8 bits = 2^3 
#define SURFACE_BYTES(rc) (CHANNELS * RECTWIDTH((rc)) * RECTHEIGHT((rc)))

typedef enum GRAPHICSAPI
{
  GRAPHICS_API_OPENGL = 0,
  GRAPHICS_API_COUNT
} GRAPHICSAPI;

typedef enum CAPTUREAPI
{
  CAPTURE_API_GDI_BITBLT = 0,
  CAPTURE_API_DXGI_DESKTOP_DUPLICATION,
  CAPTURE_API_WINDOWS_GRAPHICS_CAPTURE,
  CAPTURE_API_DWM_THUMBNAIL,
  CAPTURE_API_DWM_PRIVATE_VISUAL,
  CAPTURE_API_COUNT
} CAPTUREAPI;

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
  HWND         hwndHost;
  HMODULE      hDComp;
  HMODULE      hDwmApi;
  HMODULE      hUser32;
  FARPROC      pDCompositionCreateDevice;
  FARPROC      pDCompositionCreateDevice3;
  FARPROC      pCreateSharedVisual;
  FARPROC      pUpdateSharedVisual;
  FARPROC      pSetWindowCompositionAttribute;
  ID3D11Device* d3dDevice;
  IDXGIDevice* dxgiDevice;
  IUnknown*    dcompDevice;
  IUnknown*    dcompTarget;
  IUnknown*    dcompVisual;
  HTHUMBNAIL   hThumbnail;
  BOOL         fDesktopDevice;
  BOOL         fUseMultiWindow;
  BOOL         fInitialized;
} DWMPRIVATEVISUALCAPTURE, *LPDWMPRIVATEVISUALCAPTURE;

typedef struct SHAREDWGLDATA
{
  BOOL             fTrackCursor;
  BOOL             fWinRtInitialized;
  BOOL             fUseSourceOrigin;
  BOOL             fMouseRelativeZoom;
  BOOL             fMiniMapDragging;
  BOOL             fMiniMapHoldVisible;
  BOOL             fMiniMapHaveLastCursor;
  BOOL             fSourceOriginPinned;
  MAGVIEWMODE      viewMode;
  GRAPHICSAPI      graphicsApi;
  CAPTUREAPI       captureApi;
  POINT            pt;
  POINT            ptSourceOrigin;
  POINT            ptMiniMapDragOffset;
  POINT            ptMiniMapLastCursor;
  HWND             hwndLensMouseTarget;
  DWORD            dwMiniMapLastActivity;
  RECT             rc;
  DISPLAYINFO      di;
  BITMAPINFOHEADER bi;
  HDC              hDC;
  HGLRC            hRC;
  HPBUFFERARB      pb;
  FLOAT            fScale;
  HDC              hCaptureDC;
  HDC              hDesktopDC;
  HBITMAP          hBitmapBg;
  HBITMAP          hBitmapOld;
  DXGIOUTPUTCAPTURE dxgiOutputs[MAX_ENUM_MONITORS];
  WGCMONITORCAPTURE wgcMonitors[MAX_ENUM_MONITORS];
  DWMTHUMBNAILCAPTURE dwmThumbnail;
  DWMPRIVATEVISUALCAPTURE dwmPrivate;
  GLclampf         cfClearColor[CHANNELS];
  FLOAT            fTexScaler;
  GLuint           glScreenTexture;
  GLubyte*         glScreenData;

} SHAREDWGLDATA, *LPSHAREDWGLDATA;

void renderInit(HWND hWnd);

void renderCleanup(HWND hWnd);

void renderResizeCapture(HWND hWnd);

void renderRender(HWND hWnd);

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax);

void render_computeSourceRect(HWND hWnd, RECT* lprcSource);

BOOL render_minimapHitTest(HWND hWnd, POINT ptClient);
void render_minimapNotifyActivity(HWND hWnd);
BOOL render_minimapBeginDrag(HWND hWnd, POINT ptClient);
BOOL render_minimapDrag(HWND hWnd, POINT ptClient);
void render_minimapEndDrag(HWND hWnd);
