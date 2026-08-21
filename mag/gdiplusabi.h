#pragma once

#include <windows.h>

#ifdef __cplusplus
#error gdiplusabi.h is the C-only GDI+ flat ABI used by MAG
#endif

/*
 * The Windows SDK's gdiplus.h family wraps these exports in C++ classes and
 * namespaces.  MAG uses the same Gdiplus.dll flat ABI directly so the entire
 * renderer remains C.  Keep this surface limited to functions MAG consumes.
 */

typedef INT MAGGDIPSTATUS;
typedef DWORD MAGGDIPARGB;
typedef INT MAGGDIPPIXELFORMAT;

typedef struct GpBitmap GpBitmap;
typedef struct GpBrush GpBrush;
typedef struct GpGraphics GpGraphics;
typedef struct GpImage GpImage;
typedef struct GpImageAttributes GpImageAttributes;
typedef struct GpSolidFill GpSolidFill;

typedef BOOL (CALLBACK *MAGGDIPDRAWIMAGEABORT)(VOID* data);

typedef struct MAGGDIPLUSSTARTUPINPUT
{
  UINT32 GdiplusVersion;
  VOID (WINAPI *DebugEventCallback)(INT level, CHAR* message);
  BOOL SuppressBackgroundThread;
  BOOL SuppressExternalCodecs;
} MAGGDIPLUSSTARTUPINPUT;

enum
{
  MAG_GDIP_STATUS_OK = 0,
  MAG_GDIP_UNIT_PIXEL = 2,
  MAG_GDIP_COMPOSITING_SOURCE_OVER = 0,
  MAG_GDIP_COMPOSITING_SOURCE_COPY = 1,
  MAG_GDIP_COMPOSITING_QUALITY_HIGH_SPEED = 1,
  MAG_GDIP_INTERPOLATION_NEAREST_NEIGHBOR = 5,
  MAG_GDIP_PIXEL_OFFSET_HIGH_SPEED = 1,
  MAG_GDIP_FLUSH_SYNC = 1,
  MAG_GDIP_PIXEL_FORMAT_GDI = 0x00020000,
  MAG_GDIP_PIXEL_FORMAT_32BPP_RGB =
    9 | (32 << 8) | MAG_GDIP_PIXEL_FORMAT_GDI,
};

MAGGDIPSTATUS WINAPI GdiplusStartup(
  ULONG_PTR* token,
  const MAGGDIPLUSSTARTUPINPUT* input,
  VOID* output);
VOID WINAPI GdiplusShutdown(ULONG_PTR token);

MAGGDIPSTATUS WINAPI GdipCreateBitmapFromScan0(
  INT width,
  INT height,
  INT stride,
  MAGGDIPPIXELFORMAT format,
  BYTE* scan0,
  GpBitmap** bitmap);
MAGGDIPSTATUS WINAPI GdipDisposeImage(GpImage* image);
MAGGDIPSTATUS WINAPI GdipGetImageGraphicsContext(
  GpImage* image,
  GpGraphics** graphics);
MAGGDIPSTATUS WINAPI GdipDeleteGraphics(GpGraphics* graphics);
MAGGDIPSTATUS WINAPI GdipSetCompositingMode(
  GpGraphics* graphics,
  INT compositingMode);
MAGGDIPSTATUS WINAPI GdipSetCompositingQuality(
  GpGraphics* graphics,
  INT compositingQuality);
MAGGDIPSTATUS WINAPI GdipSetInterpolationMode(
  GpGraphics* graphics,
  INT interpolationMode);
MAGGDIPSTATUS WINAPI GdipSetPixelOffsetMode(
  GpGraphics* graphics,
  INT pixelOffsetMode);
MAGGDIPSTATUS WINAPI GdipSetPageUnit(GpGraphics* graphics, INT unit);
MAGGDIPSTATUS WINAPI GdipDrawImageRectRectI(
  GpGraphics* graphics,
  GpImage* image,
  INT destinationX,
  INT destinationY,
  INT destinationWidth,
  INT destinationHeight,
  INT sourceX,
  INT sourceY,
  INT sourceWidth,
  INT sourceHeight,
  INT sourceUnit,
  const GpImageAttributes* imageAttributes,
  MAGGDIPDRAWIMAGEABORT callback,
  VOID* callbackData);
MAGGDIPSTATUS WINAPI GdipCreateSolidFill(
  MAGGDIPARGB color,
  GpSolidFill** brush);
MAGGDIPSTATUS WINAPI GdipSetSolidFillColor(
  GpSolidFill* brush,
  MAGGDIPARGB color);
MAGGDIPSTATUS WINAPI GdipDeleteBrush(GpBrush* brush);
MAGGDIPSTATUS WINAPI GdipFillRectangleI(
  GpGraphics* graphics,
  GpBrush* brush,
  INT x,
  INT y,
  INT width,
  INT height);
MAGGDIPSTATUS WINAPI GdipFlush(GpGraphics* graphics, INT intention);
