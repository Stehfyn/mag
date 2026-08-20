#include "framework.h"
#include "graphics_layered.h"

struct MAGLAYEREDPRESENTER
{
  HDC memoryDC;
  HBITMAP bitmap;
  HBITMAP oldBitmap;
  BYTE* pixels;
  UINT capacityWidth;
  UINT capacityHeight;
  UINT64 resourceGeneration;
};

BOOL magLayeredPresenterCreate(
  HWND hWnd,
  SIZE clientSize,
  MAGLAYEREDPRESENTER** presenterOut)
{
    MAGLAYEREDPRESENTER* presenter;
    BITMAPINFO bitmapInfo = { 0 };
    SIZE capacity;
    void* pixels = NULL;

    if (!hWnd || !presenterOut || clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }
    *presenterOut = NULL;
    presenter = (MAGLAYEREDPRESENTER*)HeapAlloc(
      GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*presenter));
    if (!presenter)
    {
      return FALSE;
    }

    capacity = magGraphicsChooseReservoirSize(hWnd, clientSize);
    presenter->memoryDC = CreateCompatibleDC(NULL);
    bitmapInfo.bmiHeader.biSize = sizeof(bitmapInfo.bmiHeader);
    bitmapInfo.bmiHeader.biWidth = capacity.cx;
    bitmapInfo.bmiHeader.biHeight = -capacity.cy;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;
    presenter->bitmap = CreateDIBSection(
      presenter->memoryDC,
      &bitmapInfo,
      DIB_RGB_COLORS,
      &pixels,
      NULL,
      0);
    if (!presenter->memoryDC || !presenter->bitmap || !pixels)
    {
      magLayeredPresenterDestroy(presenter);
      return FALSE;
    }
    presenter->oldBitmap = SelectBitmap(presenter->memoryDC, presenter->bitmap);
    if (!presenter->oldBitmap)
    {
      magLayeredPresenterDestroy(presenter);
      return FALSE;
    }
    presenter->pixels = (BYTE*)pixels;
    presenter->capacityWidth = (UINT)capacity.cx;
    presenter->capacityHeight = (UINT)capacity.cy;
    presenter->resourceGeneration = 1;
    *presenterOut = presenter;
    return TRUE;
}

void magLayeredPresenterDestroy(MAGLAYEREDPRESENTER* presenter)
{
    if (!presenter)
    {
      return;
    }
    if (presenter->memoryDC && presenter->oldBitmap)
    {
      SelectBitmap(presenter->memoryDC, presenter->oldBitmap);
    }
    if (presenter->bitmap)
    {
      DeleteBitmap(presenter->bitmap);
    }
    if (presenter->memoryDC)
    {
      DeleteDC(presenter->memoryDC);
    }
    HeapFree(GetProcessHeap(), 0, presenter);
}

static void magLayeredCopyPixels(
  MAGLAYEREDPRESENTER* presenter,
  const MAGPIXELBUFFER* frame,
  BOOL perPixelAlpha)
{
    UINT y;

    for (y = 0; y < frame->height; ++y)
    {
      const UINT sourceY = MAG_ROW_ORDER_TOP_DOWN == frame->rowOrder
        ? y
        : frame->height - 1U - y;
      const BYTE* source = frame->pixels + (SIZE_T)sourceY * frame->stride;
      BYTE* destination = presenter->pixels +
        (SIZE_T)y * presenter->capacityWidth * 4U;
      UINT x;

      if (!perPixelAlpha && MAG_ALPHA_MODE_IGNORE == frame->alphaMode)
      {
        CopyMemory(destination, source, (SIZE_T)frame->width * 4U);
        for (x = 0; x < frame->width; ++x)
        {
          destination[(SIZE_T)x * 4U + 3U] = 255;
        }
        continue;
      }

      for (x = 0; x < frame->width; ++x)
      {
        const BYTE* sourcePixel = source + (SIZE_T)x * 4U;
        BYTE* destinationPixel = destination + (SIZE_T)x * 4U;
        UINT alpha = MAG_ALPHA_MODE_IGNORE == frame->alphaMode
          ? 255U
          : sourcePixel[3];

        destinationPixel[3] = (BYTE)alpha;
        if (perPixelAlpha && MAG_ALPHA_MODE_STRAIGHT == frame->alphaMode)
        {
          destinationPixel[0] = (BYTE)((sourcePixel[0] * alpha + 127U) / 255U);
          destinationPixel[1] = (BYTE)((sourcePixel[1] * alpha + 127U) / 255U);
          destinationPixel[2] = (BYTE)((sourcePixel[2] * alpha + 127U) / 255U);
        }
        else
        {
          destinationPixel[0] = sourcePixel[0];
          destinationPixel[1] = sourcePixel[1];
          destinationPixel[2] = sourcePixel[2];
        }
      }
    }
}

BOOL magLayeredPresenterPresent(
  MAGLAYEREDPRESENTER* presenter,
  HWND hWnd,
  const MAGPIXELBUFFER* frame,
  const MAGPRESENTATIONSETTINGS* presentation)
{
    RECT windowRect;
    POINT destination;
    POINT source = { 0, 0 };
    SIZE size;
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, 0 };
    UPDATELAYEREDWINDOWINFO info = { sizeof(info) };
    DWORD flags;
    BOOL perPixelAlpha;

    if (!presenter || !hWnd || !frame || !frame->pixels || !presentation ||
        !frame->width || !frame->height ||
        frame->width > presenter->capacityWidth ||
        frame->height > presenter->capacityHeight ||
        !GetWindowRect(hWnd, &windowRect))
    {
      return FALSE;
    }

    perPixelAlpha = MAG_LAYER_ALPHA_PER_PIXEL_PREMULTIPLIED == presentation->alphaMode;
    magLayeredCopyPixels(presenter, frame, perPixelAlpha);
    destination.x = windowRect.left;
    destination.y = windowRect.top;
    size.cx = (LONG)frame->width;
    size.cy = (LONG)frame->height;

    switch (presentation->alphaMode)
    {
    case MAG_LAYER_ALPHA_CONSTANT:
      blend.SourceConstantAlpha = presentation->constantAlpha;
      flags = ULW_ALPHA;
      break;
    case MAG_LAYER_ALPHA_COLOR_KEY:
      flags = ULW_COLORKEY;
      break;
    case MAG_LAYER_ALPHA_PER_PIXEL_PREMULTIPLIED:
      blend.AlphaFormat = AC_SRC_ALPHA;
      flags = ULW_ALPHA;
      break;
    case MAG_LAYER_ALPHA_OPAQUE:
    default:
      flags = ULW_ALPHA;
      break;
    }

    info.pptDst = &destination;
    info.psize = &size;
    info.hdcSrc = presenter->memoryDC;
    info.pptSrc = &source;
    info.crKey = presentation->colorKey;
    info.pblend = &blend;
    info.dwFlags = flags;
    return UpdateLayeredWindowIndirect(hWnd, &info);
}

UINT64 magLayeredPresenterGetResourceGeneration(
  const MAGLAYEREDPRESENTER* presenter)
{
    return presenter ? presenter->resourceGeneration : 0;
}
