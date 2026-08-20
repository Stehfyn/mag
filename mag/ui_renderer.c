#include "ui_renderer.h"

#include <windowsx.h>

#include <d2d1.h>
#include <dwrite.h>

#pragma comment(lib, "d2d1")
#pragma comment(lib, "dwrite")

#define MAG_RECT_WIDTH(Rect) ((Rect).right - (Rect).left)
#define MAG_RECT_HEIGHT(Rect) ((Rect).bottom - (Rect).top)
#define MAG_CLAMP(Value, Low, High) (max(min((Value), (High)), (Low)))

#define MAG_ATLAS_FIRST_CHAR 32
#define MAG_ATLAS_GLYPH_COUNT 96
#define MAG_ATLAS_COLUMNS 16
#define MAG_ATLAS_CELL_WIDTH 24
#define MAG_ATLAS_CELL_HEIGHT 24
#define MAG_ATLAS_WIDTH (MAG_ATLAS_COLUMNS * MAG_ATLAS_CELL_WIDTH)
#define MAG_ATLAS_ROWS ((MAG_ATLAS_GLYPH_COUNT + MAG_ATLAS_COLUMNS - 1) / MAG_ATLAS_COLUMNS)
#define MAG_ATLAS_HEIGHT (MAG_ATLAS_ROWS * MAG_ATLAS_CELL_HEIGHT)

struct MAGUIRENDERER
{
  UIGRAPHICSAPI uiApi;
  TEXTRENDERER textRenderer;
  HDC           hDC;
  HBITMAP       hBitmap;
  HBITMAP       hBitmapOld;
  BYTE*         pixels;
  UINT          width;
  UINT          height;
  UINT          capacityWidth;
  UINT          capacityHeight;
  UINT64        surfaceGeneration;
  HFONT         hFont;
  MAGCPUCOMPOSITOR baseCompositor;

  ID2D1Factory*          d2dFactory;
  ID2D1DCRenderTarget*   d2dTarget;
  ID2D1SolidColorBrush*  d2dBrush;
  IDWriteFactory*        dwriteFactory;
  IDWriteTextFormat*     textFormat;

  BYTE*         atlasAlpha;
  MAGGLYPHINFO  glyphs[MAG_ATLAS_GLYPH_COUNT];
  MAGGLYPHATLAS atlas;
};

static void magUiReleaseUnknown(IUnknown** object)
{
    if (*object)
    {
      (*object)->Release();
      *object = NULL;
    }
}

static BYTE magUiColorByte(FLOAT value)
{
    value = MAG_CLAMP(value, 0.0f, 1.0f);
    return (BYTE)(value * 255.0f + 0.5f);
}

static BOOL magUiCreateSurface(MAGUIRENDERER* renderer, SIZE clientSize)
{
    BITMAPINFO bmi = { 0 };
    void* pixels = NULL;
    SIZE reservoirSize;
    HBITMAP newBitmap;
    HBITMAP replacedBitmap;

    if (clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }

    if (!renderer->hDC)
    {
      renderer->hDC = CreateCompatibleDC(NULL);
      if (!renderer->hDC)
      {
        return FALSE;
      }
    }

    renderer->width = (UINT)clientSize.cx;
    renderer->height = (UINT)clientSize.cy;
    if (renderer->hBitmap &&
        renderer->width <= renderer->capacityWidth &&
        renderer->height <= renderer->capacityHeight)
    {
      return TRUE;
    }

    reservoirSize = magGraphicsChooseReservoirSize(NULL, clientSize);
    reservoirSize.cx = max(reservoirSize.cx, (LONG)renderer->capacityWidth);
    reservoirSize.cy = max(reservoirSize.cy, (LONG)renderer->capacityHeight);

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = reservoirSize.cx;
    bmi.bmiHeader.biHeight = -reservoirSize.cy;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    newBitmap = CreateDIBSection(
      renderer->hDC,
      &bmi,
      DIB_RGB_COLORS,
      &pixels,
      NULL,
      0);
    if (!newBitmap || !pixels)
    {
      if (newBitmap)
      {
        DeleteBitmap(newBitmap);
      }
      return FALSE;
    }

    replacedBitmap = SelectBitmap(renderer->hDC, newBitmap);
    if (!replacedBitmap)
    {
      DeleteBitmap(newBitmap);
      return FALSE;
    }

    if (!renderer->hBitmapOld)
    {
      renderer->hBitmapOld = replacedBitmap;
    }
    else if (renderer->hBitmap)
    {
      DeleteBitmap(renderer->hBitmap);
    }

    renderer->hBitmap = newBitmap;
    renderer->pixels = (BYTE*)pixels;
    renderer->capacityWidth = (UINT)reservoirSize.cx;
    renderer->capacityHeight = (UINT)reservoirSize.cy;
    ++renderer->surfaceGeneration;
    return TRUE;
}

static BOOL magUiCreateD2DResources(MAGUIRENDERER* renderer)
{
    D2D1_RENDER_TARGET_PROPERTIES properties;
    D2D1_COLOR_F color = { 1.0f, 1.0f, 1.0f, 1.0f };
    HRESULT hr;

    ZeroMemory(&properties, sizeof(properties));
    properties.type = D2D1_RENDER_TARGET_TYPE_SOFTWARE;
    properties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
    properties.pixelFormat.alphaMode = D2D1_ALPHA_MODE_IGNORE;
    properties.usage = D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE;

    hr = D2D1CreateFactory(
      D2D1_FACTORY_TYPE_SINGLE_THREADED,
      &renderer->d2dFactory);
    if (SUCCEEDED(hr))
    {
      hr = renderer->d2dFactory->CreateDCRenderTarget(&properties, &renderer->d2dTarget);
    }
    if (SUCCEEDED(hr))
    {
      hr = renderer->d2dTarget->CreateSolidColorBrush(&color, NULL, &renderer->d2dBrush);
    }
    return SUCCEEDED(hr);
}

static BOOL magUiCreateDirectWriteResources(MAGUIRENDERER* renderer)
{
    HRESULT hr = DWriteCreateFactory(
      DWRITE_FACTORY_TYPE_SHARED,
      __uuidof(IDWriteFactory),
      (IUnknown**)&renderer->dwriteFactory);

    if (SUCCEEDED(hr))
    {
      hr = renderer->dwriteFactory->CreateTextFormat(
        L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_NORMAL,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        12.0f,
        L"en-us",
        &renderer->textFormat);
    }
    if (SUCCEEDED(hr))
    {
      renderer->textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
      renderer->textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
      renderer->textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    }
    return SUCCEEDED(hr);
}

static BOOL magUiCreateGlyphAtlas(MAGUIRENDERER* renderer)
{
    HDC atlasDC = NULL;
    HBITMAP atlasBitmap = NULL;
    HBITMAP atlasBitmapOld = NULL;
    BITMAPINFO bmi = { 0 };
    DWORD* atlasPixels = NULL;
    RECT atlasRect = { 0, 0, MAG_ATLAS_WIDTH, MAG_ATLAS_HEIGHT };
    D2D1_COLOR_F black = { 0.0f, 0.0f, 0.0f, 1.0f };
    D2D1_COLOR_F white = { 1.0f, 1.0f, 1.0f, 1.0f };
    UINT i;
    BOOL success = FALSE;

    renderer->atlasAlpha = (BYTE*)HeapAlloc(
      GetProcessHeap(),
      0,
      MAG_ATLAS_WIDTH * MAG_ATLAS_HEIGHT);
    atlasDC = CreateCompatibleDC(NULL);
    if (!renderer->atlasAlpha || !atlasDC)
    {
      goto cleanup;
    }

    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = MAG_ATLAS_WIDTH;
    bmi.bmiHeader.biHeight = -MAG_ATLAS_HEIGHT;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;
    atlasBitmap = CreateDIBSection(
      atlasDC,
      &bmi,
      DIB_RGB_COLORS,
      (void**)&atlasPixels,
      NULL,
      0);
    if (!atlasBitmap || !atlasPixels)
    {
      goto cleanup;
    }

    atlasBitmapOld = SelectBitmap(atlasDC, atlasBitmap);
    if (!atlasBitmapOld || !renderer->d2dTarget || !renderer->d2dBrush ||
        !renderer->dwriteFactory || !renderer->textFormat ||
        FAILED(renderer->d2dTarget->BindDC(atlasDC, &atlasRect)))
    {
      goto cleanup;
    }

    renderer->d2dTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    renderer->d2dBrush->SetColor(&white);
    renderer->d2dTarget->BeginDraw();
    renderer->d2dTarget->Clear(&black);

    for (i = 0; i < MAG_ATLAS_GLYPH_COUNT; ++i)
    {
      const WCHAR character = (WCHAR)(MAG_ATLAS_FIRST_CHAR + i);
      const LONG column = i % MAG_ATLAS_COLUMNS;
      const LONG row = i / MAG_ATLAS_COLUMNS;
      D2D1_RECT_F cell =
      {
        (FLOAT)(column * MAG_ATLAS_CELL_WIDTH),
        (FLOAT)(row * MAG_ATLAS_CELL_HEIGHT),
        (FLOAT)((column + 1) * MAG_ATLAS_CELL_WIDTH),
        (FLOAT)((row + 1) * MAG_ATLAS_CELL_HEIGHT),
      };
      IDWriteTextLayout* layout = NULL;
      DWRITE_TEXT_METRICS metrics = { 0 };

      if (SUCCEEDED(renderer->dwriteFactory->CreateTextLayout(
            &character,
            1,
            renderer->textFormat,
            (FLOAT)MAG_ATLAS_CELL_WIDTH,
            (FLOAT)MAG_ATLAS_CELL_HEIGHT,
            &layout)))
      {
        layout->GetMetrics(&metrics);
      }
      SetRect(
        &renderer->glyphs[i].rect,
        column * MAG_ATLAS_CELL_WIDTH,
        row * MAG_ATLAS_CELL_HEIGHT,
        (column + 1) * MAG_ATLAS_CELL_WIDTH,
        (row + 1) * MAG_ATLAS_CELL_HEIGHT);
      renderer->glyphs[i].advance = max(
        1,
        (LONG)(metrics.widthIncludingTrailingWhitespace + 0.999f));
      renderer->d2dTarget->DrawText(
        &character,
        1,
        renderer->textFormat,
        &cell,
        renderer->d2dBrush,
        D2D1_DRAW_TEXT_OPTIONS_CLIP,
        DWRITE_MEASURING_MODE_NATURAL);
      if (layout)
      {
        layout->Release();
      }
    }
    if (FAILED(renderer->d2dTarget->EndDraw(NULL, NULL)))
    {
      goto cleanup;
    }
    GdiFlush();

    for (i = 0; i < MAG_ATLAS_WIDTH * MAG_ATLAS_HEIGHT; ++i)
    {
      const DWORD pixel = atlasPixels[i];
      const BYTE blue = (BYTE)(pixel & 0xFF);
      const BYTE green = (BYTE)((pixel >> 8) & 0xFF);
      const BYTE red = (BYTE)((pixel >> 16) & 0xFF);
      renderer->atlasAlpha[i] = max(red, max(green, blue));
    }
    success = TRUE;

cleanup:
    if (atlasDC && atlasBitmapOld)
    {
      SelectBitmap(atlasDC, atlasBitmapOld);
    }
    if (atlasBitmap)
    {
      DeleteBitmap(atlasBitmap);
    }
    if (atlasDC)
    {
      DeleteDC(atlasDC);
    }
    return success;
}

static void magUiDrawAtlasText(MAGUIRENDERER* renderer, const MAGUIDRAWCOMMAND* command)
{
    const UINT colorAlpha = magUiColorByte(command->color.a);
    const UINT red = magUiColorByte(command->color.r);
    const UINT green = magUiColorByte(command->color.g);
    const UINT blue = magUiColorByte(command->color.b);
    LONG destinationX = command->rect.left;
    LONG destinationY = command->rect.top;
    UINT characterIndex;

    for (characterIndex = 0;
         command->text[characterIndex] && destinationX < command->rect.right;
         ++characterIndex)
    {
      WCHAR character = command->text[characterIndex];
      UINT glyphIndex;
      const MAGGLYPHINFO* glyph;
      LONG y;

      if (character < MAG_ATLAS_FIRST_CHAR ||
          character >= MAG_ATLAS_FIRST_CHAR + MAG_ATLAS_GLYPH_COUNT)
      {
        character = L'?';
      }
      glyphIndex = character - MAG_ATLAS_FIRST_CHAR;
      glyph = &renderer->glyphs[glyphIndex];

      for (y = 0; y < MAG_RECT_HEIGHT(glyph->rect); ++y)
      {
        const LONG targetY = destinationY + y;
        LONG x;

        if (targetY < command->rect.top || targetY >= command->rect.bottom ||
            targetY < 0 || targetY >= (LONG)renderer->height)
        {
          continue;
        }

        for (x = 0; x < MAG_RECT_WIDTH(glyph->rect); ++x)
        {
          const LONG targetX = destinationX + x;
          const BYTE glyphAlpha = renderer->atlasAlpha[
            (glyph->rect.top + y) * MAG_ATLAS_WIDTH + glyph->rect.left + x];
          const UINT alpha = (glyphAlpha * colorAlpha + 127U) / 255U;
          const UINT inverseAlpha = 255U - alpha;
          BYTE* pixel;

          if (!alpha || targetX < command->rect.left || targetX >= command->rect.right ||
              targetX < 0 || targetX >= (LONG)renderer->width)
          {
            continue;
          }

          pixel = renderer->pixels +
            (SIZE_T)targetY * renderer->capacityWidth * 4U +
            (SIZE_T)targetX * 4U;
          pixel[0] = (BYTE)((blue * alpha + pixel[0] * inverseAlpha + 127U) / 255U);
          pixel[1] = (BYTE)((green * alpha + pixel[1] * inverseAlpha + 127U) / 255U);
          pixel[2] = (BYTE)((red * alpha + pixel[2] * inverseAlpha + 127U) / 255U);
          pixel[3] = 255;
        }
      }
      destinationX += glyph->advance;
    }
}

static void magUiDrawGdiText(MAGUIRENDERER* renderer, const MAGUIDRAWCOMMAND* command)
{
    RECT rect = command->rect;

    SelectFont(renderer->hDC, renderer->hFont);
    SetBkMode(renderer->hDC, TRANSPARENT);
    SetTextColor(
      renderer->hDC,
      RGB(
        magUiColorByte(command->color.r),
        magUiColorByte(command->color.g),
        magUiColorByte(command->color.b)));
    DrawText(
      renderer->hDC,
      command->text,
      -1,
      &rect,
      DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);
}

static void magUiSetD2DBrushColor(
  MAGUIRENDERER* renderer,
  MAGCOLORF source)
{
    D2D1_COLOR_F color = { source.r, source.g, source.b, source.a };
    renderer->d2dBrush->SetColor(&color);
}

BOOL magUiRendererCreate(
  UIGRAPHICSAPI uiApi,
  TEXTRENDERER textRenderer,
  SIZE clientSize,
  MAGUIRENDERER** rendererOut,
  LPTSTR reason,
  UINT reasonCount)
{
    MAGUIRENDERER* renderer;

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    if (!rendererOut || uiApi >= UI_GRAPHICS_API_COUNT || textRenderer >= TEXT_RENDERER_COUNT)
    {
      return FALSE;
    }
    *rendererOut = NULL;

    renderer = (MAGUIRENDERER*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*renderer));
    if (!renderer)
    {
      return FALSE;
    }
    renderer->uiApi = uiApi;
    renderer->textRenderer = textRenderer;
    renderer->hFont = CreateFont(
      -16,
      0,
      0,
      0,
      FW_NORMAL,
      FALSE,
      FALSE,
      FALSE,
      DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS,
      CLIP_DEFAULT_PRECIS,
      ANTIALIASED_QUALITY,
      DEFAULT_PITCH | FF_DONTCARE,
      TEXT("Segoe UI"));

    if (!renderer->hFont || !magUiCreateSurface(renderer, clientSize) ||
        !magGraphicsReserveCpuCompositor(
          &renderer->baseCompositor,
          renderer->capacityWidth,
          renderer->capacityHeight) ||
        ((UI_GRAPHICS_API_DIRECT2D == uiApi ||
          TEXT_RENDERER_DIRECTWRITE == textRenderer ||
          TEXT_RENDERER_GPU_GLYPH_ATLAS == textRenderer) &&
          !magUiCreateD2DResources(renderer)) ||
        ((TEXT_RENDERER_DIRECTWRITE == textRenderer ||
          TEXT_RENDERER_GPU_GLYPH_ATLAS == textRenderer) &&
          !magUiCreateDirectWriteResources(renderer)) ||
        (TEXT_RENDERER_GPU_GLYPH_ATLAS == textRenderer && !magUiCreateGlyphAtlas(renderer)))
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("The selected UI or text renderer could not initialize."), reasonCount);
      }
      magUiRendererDestroy(renderer);
      return FALSE;
    }

    if (TEXT_RENDERER_GPU_GLYPH_ATLAS == textRenderer)
    {
      renderer->atlas.alpha = renderer->atlasAlpha;
      renderer->atlas.width = MAG_ATLAS_WIDTH;
      renderer->atlas.height = MAG_ATLAS_HEIGHT;
      renderer->atlas.stride = MAG_ATLAS_WIDTH;
      renderer->atlas.firstCharacter = MAG_ATLAS_FIRST_CHAR;
      renderer->atlas.glyphCount = MAG_ATLAS_GLYPH_COUNT;
      renderer->atlas.glyphs = renderer->glyphs;
    }

    *rendererOut = renderer;
    return TRUE;
}

void magUiRendererDestroy(MAGUIRENDERER* renderer)
{
    if (!renderer)
    {
      return;
    }

    magUiReleaseUnknown((IUnknown**)&renderer->textFormat);
    magUiReleaseUnknown((IUnknown**)&renderer->dwriteFactory);
    magUiReleaseUnknown((IUnknown**)&renderer->d2dBrush);
    magUiReleaseUnknown((IUnknown**)&renderer->d2dTarget);
    magUiReleaseUnknown((IUnknown**)&renderer->d2dFactory);
    if (renderer->hDC && renderer->hBitmapOld)
    {
      SelectBitmap(renderer->hDC, renderer->hBitmapOld);
    }
    if (renderer->hBitmap)
    {
      DeleteBitmap(renderer->hBitmap);
    }
    if (renderer->hDC)
    {
      DeleteDC(renderer->hDC);
    }
    if (renderer->hFont)
    {
      DeleteFont(renderer->hFont);
    }
    if (renderer->atlasAlpha)
    {
      HeapFree(GetProcessHeap(), 0, renderer->atlasAlpha);
    }
    magGraphicsDestroyCpuCompositor(&renderer->baseCompositor);
    HeapFree(GetProcessHeap(), 0, renderer);
}

BOOL magUiRendererResize(MAGUIRENDERER* renderer, SIZE clientSize)
{
    return renderer && magUiCreateSurface(renderer, clientSize);
}

static BOOL magUiRendererComposeInternal(
  MAGUIRENDERER* renderer,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output,
  BOOL includeText)
{
    MAGPIXELBUFFER base;
    const MAGUIDRAWLIST* cpuUi = UI_GRAPHICS_API_NATIVE == renderer->uiApi ? ui : NULL;
    RECT targetRect = { 0, 0, (LONG)renderer->width, (LONG)renderer->height };
    UINT i;

    if (!renderer || !frame || !output || frame->width != renderer->width || frame->height != renderer->height ||
        !magGraphicsComposeFrame(&renderer->baseCompositor, frame, cpuUi, &base))
    {
      return FALSE;
    }
    for (i = 0; i < base.height; ++i)
    {
      CopyMemory(
        renderer->pixels + (SIZE_T)i * renderer->capacityWidth * 4U,
        base.pixels + (SIZE_T)i * base.stride,
        (SIZE_T)base.width * 4U);
    }

    if (UI_GRAPHICS_API_DIRECT2D == renderer->uiApi || TEXT_RENDERER_DIRECTWRITE == renderer->textRenderer)
    {
      if (FAILED(renderer->d2dTarget->BindDC(renderer->hDC, &targetRect)))
      {
        return FALSE;
      }

      renderer->d2dTarget->BeginDraw();
      if (ui)
      {
        for (i = 0; i < ui->count; ++i)
        {
          const MAGUIDRAWCOMMAND* command = &ui->commands[i];
          D2D1_RECT_F rect =
          {
            (FLOAT)command->rect.left,
            (FLOAT)command->rect.top,
            (FLOAT)command->rect.right,
            (FLOAT)command->rect.bottom,
          };

          if (UI_GRAPHICS_API_DIRECT2D == renderer->uiApi &&
              MAG_UI_DRAW_FILL_RECT == command->type)
          {
            magUiSetD2DBrushColor(renderer, command->color);
            renderer->d2dTarget->FillRectangle(&rect, renderer->d2dBrush);
          }
          else if (UI_GRAPHICS_API_DIRECT2D == renderer->uiApi &&
                   MAG_UI_DRAW_STROKE_RECT == command->type)
          {
            magUiSetD2DBrushColor(renderer, command->color);
            renderer->d2dTarget->DrawRectangle(
              &rect,
              renderer->d2dBrush,
              command->thickness,
              NULL);
          }
          else if (includeText &&
                   TEXT_RENDERER_DIRECTWRITE == renderer->textRenderer &&
                   MAG_UI_DRAW_TEXT == command->type)
          {
            magUiSetD2DBrushColor(renderer, command->color);
            renderer->d2dTarget->DrawText(
              command->text,
              lstrlen(command->text),
              renderer->textFormat,
              &rect,
              renderer->d2dBrush,
              D2D1_DRAW_TEXT_OPTIONS_CLIP,
              DWRITE_MEASURING_MODE_NATURAL);
          }
        }
      }
      if (FAILED(renderer->d2dTarget->EndDraw(NULL, NULL)))
      {
        return FALSE;
      }
    }

    if (includeText && ui && TEXT_RENDERER_GDI == renderer->textRenderer)
    {
      for (i = 0; i < ui->count; ++i)
      {
        if (MAG_UI_DRAW_TEXT == ui->commands[i].type)
        {
          magUiDrawGdiText(renderer, &ui->commands[i]);
        }
      }
      GdiFlush();
    }
    else if (includeText && ui && TEXT_RENDERER_GPU_GLYPH_ATLAS == renderer->textRenderer)
    {
      for (i = 0; i < ui->count; ++i)
      {
        if (MAG_UI_DRAW_TEXT == ui->commands[i].type)
        {
          magUiDrawAtlasText(renderer, &ui->commands[i]);
        }
      }
    }

    output->pixels = renderer->pixels;
    output->width = renderer->width;
    output->height = renderer->height;
    output->stride = renderer->capacityWidth * 4U;
    output->rowOrder = MAG_ROW_ORDER_TOP_DOWN;
    output->alphaMode = MAG_ALPHA_MODE_IGNORE;
    return TRUE;
}

BOOL magUiRendererCompose(
  MAGUIRENDERER* renderer,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output)
{
    return magUiRendererComposeInternal(renderer, frame, ui, output, TRUE);
}

BOOL magUiRendererComposeWithoutText(
  MAGUIRENDERER* renderer,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output)
{
    return magUiRendererComposeInternal(renderer, frame, ui, output, FALSE);
}

const MAGGLYPHATLAS* magUiRendererGetGlyphAtlas(const MAGUIRENDERER* renderer)
{
    return renderer && renderer->atlas.alpha ? &renderer->atlas : NULL;
}

UINT64 magUiRendererGetSurfaceGeneration(const MAGUIRENDERER* renderer)
{
    return renderer
      ? renderer->surfaceGeneration + renderer->baseCompositor.generation
      : 0;
}
