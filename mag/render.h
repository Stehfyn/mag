#pragma once

#include "framework.h"

#include <gl/GL.h>

typedef struct SHAREDWGLDATA
{
  BOOL             fTrackCursor;
  BOOL             fTopMostForce;
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
  GLclampf         cfClearColor[4];
  FLOAT            fTexScaler;
  GLsizei          glScreenWidth;
  GLsizei          glScreenHeight;
  GLuint           glScreenTexture;
  GLubyte          glScreenData[3*256*256*4];
  //GLubyte          glScreenData[3840*2160*4];

} SHAREDWGLDATA, *LPSHAREDWGLDATA;

void renderInit(HWND hWnd);

void renderRender(HWND hWnd);



