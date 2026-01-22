#pragma once

#include "framework.h"

#include <gl/GL.h>

#define CHANNELS (4)
#define BITS_PER_PIXEL (CHANNELS << 3) // 8 bits = 2^3 
#define SURFACE_BYTES(rc) (CHANNELS * RECTWIDTH((rc)) * RECTHEIGHT((rc)))

typedef struct SHAREDWGLDATA
{
  BOOL             fTrackCursor;
  POINT            pt;
  RECT             rc;
  DISPLAYINFO      di;
  BITMAPINFOHEADER bi;
  HDC              hDC;
  HGLRC            hRC;
  FLOAT            fScale;
  HDC              hCaptureDC;
  HDC              hDesktopDC;
  HBITMAP          hBitmapBg;
  HBITMAP          hBitmapOld;
  GLclampf         cfClearColor[CHANNELS];
  FLOAT            fTexScaler;
  GLuint           glScreenTexture;
  //GLubyte*         glScreenData;
  GLubyte          glScreenData[3840*2160*4];

} SHAREDWGLDATA, *LPSHAREDWGLDATA;

void renderInit(HWND hWnd);

void renderCreateResources(HWND hWnd);

void renderResizeCapture(HWND hWnd);

void renderRender(HWND hWnd);



