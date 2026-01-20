#pragma once

#include "framework.h"

#include <gl/GL.h>

#define DIM (256)
#define CHANNELS (4)

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
  GLclampf         cfClearColor[CHANNELS];
  FLOAT            fTexScaler;
  GLsizei          glScreenWidth;
  GLsizei          glScreenHeight;
  GLuint           glScreenTexture;
  GLubyte          glScreenData[DIM*DIM*CHANNELS];

} SHAREDWGLDATA, *LPSHAREDWGLDATA;

void renderInit(HWND hWnd);

void renderCreateResources(HWND hWnd);

void renderRender(HWND hWnd);



