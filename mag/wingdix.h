#pragma once

#include "framework.h"

#define MAX_ENUM_MONITORS                (16U)

#define GL_TRUE 1
#define GL_FALSE 0

#define WGL_DRAW_TO_WINDOW_ARB            0x2001
#define WGL_ACCELERATION_ARB              0x2003
#define WGL_NO_ACCELERATION_ARB           0x2025
#define WGL_GENERIC_ACCELERATION_ARB      0x2026
#define WGL_FULL_ACCELERATION_ARB         0x2027
#define WGL_SWAP_METHOD_ARB               0x2007
#define WGL_SUPPORT_OPENGL_ARB            0x2010
#define WGL_DOUBLE_BUFFER_ARB             0x2011
#define WGL_PIXEL_TYPE_ARB                0x2013
#define WGL_COLOR_BITS_ARB                0x2014
#define WGL_ALPHA_BITS_ARB                0x201B
#define WGL_DEPTH_BITS_ARB                0x2022
#define WGL_SWAP_EXCHANGE_ARB             0x2028
#define WGL_TYPE_RGBA_ARB                 0x202B
#define WGL_SAMPLE_BUFFERS_ARB            0x2041
#define WGL_SAMPLES_ARB                   0x2042
#define GL_BGRA                           0x80E1

typedef BOOL(WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC) (HDC hdc, const int* piAttribIList, const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;

typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC) (HDC hDC, HGLRC hShareContext, const int* attribList);
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;

typedef BOOL(WINAPI* PFNWGLSWAPINTERVALEXTPROC) (int interval);
PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

typedef int (WINAPI* PFNWGLGETSWAPINTERVALEXTPROC) (void);
PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT;

typedef struct MONITORINFO2
{
  CHAR          deviceName[CCHDEVICENAME + 1];
  BYTE          unused;
  WCHAR         deviceNameW[CCHDEVICENAME + 1];
  DEVMODE       deviceMode;
  MONITORINFOEX monitorInfoEx;
} MONITORINFO2, * LPMONITORINFO2;

typedef struct DISPLAYINFO
{
  RECT         rc;
  UINT         numMonitors;
  MONITORINFO2 monitors[MAX_ENUM_MONITORS];
} DISPLAYINFO, * LPDISPLAYINFO;

void wglInit(void);

int wglFindPixelFormat(HDC hDC, const int* piAttribIList, const FLOAT* pfAttribFList);

BOOL gdiGetDisplayInfo(LPDISPLAYINFO lpdi);

#define OBS_COMPLETELYCOVERED       0
#define OBS_PARTIALLYVISIBLE        1
#define OBS_COMPLETELYVISIBLE       2

int gdiCheckOcclusionStatus(HWND hwnd);
