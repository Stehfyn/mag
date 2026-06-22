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
  CAPTURE_API_MAGNIFICATION,
  CAPTURE_API_COUNT
} CAPTUREAPI;

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

typedef struct SHAREDWGLDATA
{
  BOOL             fTrackCursor;
  GRAPHICSAPI      graphicsApi;
  CAPTUREAPI       captureApi;
  POINT            pt;
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
  GLclampf         cfClearColor[CHANNELS];
  FLOAT            fTexScaler;
  GLuint           glScreenTexture;
  GLubyte*         glScreenData;

} SHAREDWGLDATA, *LPSHAREDWGLDATA;

void renderInit(HWND hWnd);

void renderCleanup(HWND hWnd);

void renderResizeCapture(HWND hWnd);

void renderRender(HWND hWnd);



