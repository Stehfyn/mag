#include "framework.h"
#include "graphics.h"

#pragma comment(lib, "Msimg32")

typedef struct MAGGDISTATE
{
  HDC      hColorDC;
  HBITMAP  hColorBitmap;
  HBITMAP  hColorBitmapOld;
  DWORD*   colorPixel;
} MAGGDISTATE;

static BOOL magGraphicsGdiIsAvailable(LPTSTR reason, UINT reasonCount)
{
    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    return TRUE;
}

static BOOL magGraphicsGdiCreate(HWND hWnd, SIZE clientSize, void** stateOut)
{
    MAGGDISTATE* state;
    BITMAPINFO bmi = { 0 };

    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(clientSize);

    if (!stateOut)
    {
      return FALSE;
    }
    *stateOut = NULL;

    state = (MAGGDISTATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }

    state->hColorDC = CreateCompatibleDC(NULL);
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = 1;
    bmi.bmiHeader.biHeight = -1;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    state->hColorBitmap = CreateDIBSection(
      state->hColorDC,
      &bmi,
      DIB_RGB_COLORS,
      (void**)&state->colorPixel,
      NULL,
      0);

    if (!state->hColorDC || !state->hColorBitmap || !state->colorPixel)
    {
      if (state->hColorBitmap)
      {
        DeleteBitmap(state->hColorBitmap);
      }
      if (state->hColorDC)
      {
        DeleteDC(state->hColorDC);
      }
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }

    state->hColorBitmapOld = SelectBitmap(state->hColorDC, state->hColorBitmap);
    if (!state->hColorBitmapOld)
    {
      DeleteBitmap(state->hColorBitmap);
      DeleteDC(state->hColorDC);
      HeapFree(GetProcessHeap(), 0, state);
      return FALSE;
    }

    *stateOut = state;
    return TRUE;
}

static void magGraphicsGdiDestroy(HWND hWnd, void* opaqueState)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);

    if (!state)
    {
      return;
    }

    if (state->hColorDC && state->hColorBitmapOld)
    {
      SelectBitmap(state->hColorDC, state->hColorBitmapOld);
    }
    if (state->hColorBitmap)
    {
      DeleteBitmap(state->hColorBitmap);
    }
    if (state->hColorDC)
    {
      DeleteDC(state->hColorDC);
    }
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsGdiResize(HWND hWnd, void* state, SIZE clientSize)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(clientSize);
    return NULL != state;
}

static BYTE magGraphicsGdiColorByte(FLOAT value)
{
    value = CLAMP(value, 0.0f, 1.0f);
    return (BYTE)(value * 255.0f + 0.5f);
}

static void magGraphicsGdiAlphaFill(MAGGDISTATE* state, HDC hDC, const RECT* rect, MAGCOLORF color)
{
    BLENDFUNCTION blend = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    const BYTE a = magGraphicsGdiColorByte(color.a);
    const BYTE r = (BYTE)((magGraphicsGdiColorByte(color.r) * a + 127) / 255);
    const BYTE g = (BYTE)((magGraphicsGdiColorByte(color.g) * a + 127) / 255);
    const BYTE b = (BYTE)((magGraphicsGdiColorByte(color.b) * a + 127) / 255);
    const int width = rect->right - rect->left;
    const int height = rect->bottom - rect->top;

    if (width <= 0 || height <= 0 || !a)
    {
      return;
    }

    *state->colorPixel = ((DWORD)a << 24) | ((DWORD)r << 16) | ((DWORD)g << 8) | b;
    AlphaBlend(
      hDC,
      rect->left,
      rect->top,
      width,
      height,
      state->hColorDC,
      0,
      0,
      1,
      1,
      blend);
}

static void magGraphicsGdiStroke(MAGGDISTATE* state, HDC hDC, const MAGUIDRAWCOMMAND* command)
{
    const LONG thickness = max(1, (LONG)(command->thickness + 0.5f));
    RECT edges[4] =
    {
      { command->rect.left, command->rect.top, command->rect.right, command->rect.top + thickness },
      { command->rect.left, command->rect.bottom - thickness, command->rect.right, command->rect.bottom },
      { command->rect.left, command->rect.top + thickness, command->rect.left + thickness, command->rect.bottom - thickness },
      { command->rect.right - thickness, command->rect.top + thickness, command->rect.right, command->rect.bottom - thickness },
    };
    UINT i;

    for (i = 0; i < ARRAYSIZE(edges); ++i)
    {
      magGraphicsGdiAlphaFill(state, hDC, &edges[i], command->color);
    }
}

static BOOL magGraphicsGdiRender(HWND hWnd, void* opaqueState, const MAGPIXELBUFFER* frame, const MAGUIDRAWLIST* ui)
{
    MAGGDISTATE* state = (MAGGDISTATE*)opaqueState;
    BITMAPINFO bmi = { 0 };
    HDC hDC;
    UINT i;
    int copied;

    if (!state || !frame || !frame->pixels || !frame->width || !frame->height)
    {
      return FALSE;
    }

    hDC = GetDC(hWnd);
    if (!hDC)
    {
      return FALSE;
    }

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = (LONG)frame->width;
    bmi.bmiHeader.biHeight = MAG_ROW_ORDER_TOP_DOWN == frame->rowOrder ? -(LONG)frame->height : (LONG)frame->height;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    copied = SetDIBitsToDevice(
      hDC,
      0,
      0,
      frame->width,
      frame->height,
      0,
      0,
      0,
      frame->height,
      frame->pixels,
      &bmi,
      DIB_RGB_COLORS);

    if (ui)
    {
      for (i = 0; i < ui->count; ++i)
      {
        if (MAG_UI_DRAW_FILL_RECT == ui->commands[i].type)
        {
          magGraphicsGdiAlphaFill(state, hDC, &ui->commands[i].rect, ui->commands[i].color);
        }
        else if (MAG_UI_DRAW_STROKE_RECT == ui->commands[i].type)
        {
          magGraphicsGdiStroke(state, hDC, &ui->commands[i]);
        }
      }
    }

    GdiFlush();
    ReleaseDC(hWnd, hDC);
    return 0 != copied;
}

static HANDLE magGraphicsGdiGetFrameWaitHandle(void* state)
{
    UNREFERENCED_PARAMETER(state);
    return NULL;
}

const MAGGRAPHICSBACKEND g_magGraphicsGdiBackend =
{
  GRAPHICS_API_GDI,
  TEXT("GDI"),
  TRUE,
  magGraphicsGdiIsAvailable,
  magGraphicsGdiCreate,
  magGraphicsGdiDestroy,
  magGraphicsGdiResize,
  magGraphicsSetPresentationEnabledNoop,
  magGraphicsGdiRender,
  magGraphicsGdiGetFrameWaitHandle,
};
