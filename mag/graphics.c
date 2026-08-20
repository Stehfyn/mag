#include "graphics.h"

static const MAGGRAPHICSBACKEND* const g_graphicsBackends[] =
{
  &g_magGraphicsOpenGLBackend,
  &g_magGraphicsGdiBackend,
  &g_magGraphicsD3D9Backend,
  &g_magGraphicsD3D11Backend,
  &g_magGraphicsD3D12Backend,
  &g_magGraphicsVulkanBackend,
};

C_ASSERT(ARRAYSIZE(g_graphicsBackends) == GRAPHICS_API_COUNT);

BOOL magGraphicsSetPresentationEnabledNoop(HWND hWnd, void* state, BOOL enabled)
{
    UNREFERENCED_PARAMETER(hWnd);
    UNREFERENCED_PARAMETER(enabled);
    return NULL != state;
}

static BYTE magGraphicsColorByte(FLOAT value)
{
    value = max(0.0f, min(1.0f, value));
    return (BYTE)(value * 255.0f + 0.5f);
}

static void magGraphicsCpuFillRect(
  LPMAGPIXELBUFFER output,
  const RECT* sourceRect,
  MAGCOLORF color)
{
    RECT bounds = { 0, 0, (LONG)output->width, (LONG)output->height };
    RECT rect;
    const UINT alpha = magGraphicsColorByte(color.a);
    const UINT inverseAlpha = 255U - alpha;
    const UINT blue = magGraphicsColorByte(color.b);
    const UINT green = magGraphicsColorByte(color.g);
    const UINT red = magGraphicsColorByte(color.r);
    LONG y;

    if (!alpha || !IntersectRect(&rect, sourceRect, &bounds))
    {
      return;
    }

    for (y = rect.top; y < rect.bottom; ++y)
    {
      BYTE* pixel = output->pixels + (SIZE_T)y * output->stride + (SIZE_T)rect.left * 4U;
      LONG x;

      for (x = rect.left; x < rect.right; ++x, pixel += 4)
      {
        pixel[0] = (BYTE)((blue * alpha + pixel[0] * inverseAlpha + 127U) / 255U);
        pixel[1] = (BYTE)((green * alpha + pixel[1] * inverseAlpha + 127U) / 255U);
        pixel[2] = (BYTE)((red * alpha + pixel[2] * inverseAlpha + 127U) / 255U);
        pixel[3] = 255;
      }
    }
}

static void magGraphicsCpuStrokeRect(
  LPMAGPIXELBUFFER output,
  const MAGUIDRAWCOMMAND* command)
{
    const LONG thickness = max(1, (LONG)(command->thickness + 0.5f));
    const RECT edges[] =
    {
      { command->rect.left, command->rect.top, command->rect.right, command->rect.top + thickness },
      { command->rect.left, command->rect.bottom - thickness, command->rect.right, command->rect.bottom },
      { command->rect.left, command->rect.top + thickness, command->rect.left + thickness, command->rect.bottom - thickness },
      { command->rect.right - thickness, command->rect.top + thickness, command->rect.right, command->rect.bottom - thickness },
    };
    UINT i;

    for (i = 0; i < ARRAYSIZE(edges); ++i)
    {
      magGraphicsCpuFillRect(output, &edges[i], command->color);
    }
}

BOOL magGraphicsComposeFrame(
  LPMAGCPUCOMPOSITOR compositor,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  LPMAGPIXELBUFFER output)
{
    SIZE_T required;
    BYTE* pixels;
    UINT y;
    UINT i;

    if (!compositor || !frame || !frame->pixels || !frame->width || !frame->height || !output ||
        frame->width > MAXUINT / 4U)
    {
      return FALSE;
    }

    required = (SIZE_T)frame->width * 4U * frame->height;
    if (required / frame->height != (SIZE_T)frame->width * 4U)
    {
      return FALSE;
    }

    if (compositor->capacity < required)
    {
      pixels = compositor->pixels
        ? (BYTE*)HeapReAlloc(GetProcessHeap(), 0, compositor->pixels, required)
        : (BYTE*)HeapAlloc(GetProcessHeap(), 0, required);
      if (!pixels)
      {
        return FALSE;
      }
      compositor->pixels = pixels;
      compositor->capacity = required;
    }

    output->pixels = compositor->pixels;
    output->width = frame->width;
    output->height = frame->height;
    output->stride = frame->width * 4U;
    output->rowOrder = MAG_ROW_ORDER_TOP_DOWN;
    output->alphaMode = MAG_ALPHA_MODE_IGNORE;

    for (y = 0; y < frame->height; ++y)
    {
      const UINT sourceY = MAG_ROW_ORDER_TOP_DOWN == frame->rowOrder ? y : frame->height - 1U - y;
      CopyMemory(
        output->pixels + (SIZE_T)y * output->stride,
        frame->pixels + (SIZE_T)sourceY * frame->stride,
        output->stride);
    }

    if (ui)
    {
      for (i = 0; i < ui->count; ++i)
      {
        if (MAG_UI_DRAW_FILL_RECT == ui->commands[i].type)
        {
          magGraphicsCpuFillRect(output, &ui->commands[i].rect, ui->commands[i].color);
        }
        else if (MAG_UI_DRAW_STROKE_RECT == ui->commands[i].type)
        {
          magGraphicsCpuStrokeRect(output, &ui->commands[i]);
        }
      }
    }

    return TRUE;
}

void magGraphicsDestroyCpuCompositor(LPMAGCPUCOMPOSITOR compositor)
{
    if (compositor)
    {
      if (compositor->pixels)
      {
        HeapFree(GetProcessHeap(), 0, compositor->pixels);
      }
      ZeroMemory(compositor, sizeof(*compositor));
    }
}

void magUiDrawListReset(LPMAGUIDRAWLIST list)
{
    if (list)
    {
      list->count = 0;
      list->glyphAtlas = NULL;
    }
}

BOOL magUiDrawListAppendFill(LPMAGUIDRAWLIST list, const RECT* rect, MAGCOLORF color)
{
    MAGUIDRAWCOMMAND* command;

    if (!list || !rect || list->count >= ARRAYSIZE(list->commands))
    {
      return FALSE;
    }

    command = &list->commands[list->count++];
    command->type = MAG_UI_DRAW_FILL_RECT;
    command->rect = *rect;
    command->color = color;
    command->thickness = 0.0f;
    return TRUE;
}

BOOL magUiDrawListAppendStroke(LPMAGUIDRAWLIST list, const RECT* rect, MAGCOLORF color, FLOAT thickness)
{
    MAGUIDRAWCOMMAND* command;

    if (!list || !rect || list->count >= ARRAYSIZE(list->commands) || thickness <= 0.0f)
    {
      return FALSE;
    }

    command = &list->commands[list->count++];
    command->type = MAG_UI_DRAW_STROKE_RECT;
    command->rect = *rect;
    command->color = color;
    command->thickness = thickness;
    return TRUE;
}

BOOL magUiDrawListAppendText(
  LPMAGUIDRAWLIST list,
  const RECT* rect,
  MAGCOLORF color,
  FLOAT fontSize,
  LPCTSTR value)
{
    MAGUIDRAWCOMMAND* command;

    if (!list || !rect || !value || list->count >= ARRAYSIZE(list->commands) || fontSize <= 0.0f)
    {
      return FALSE;
    }

    command = &list->commands[list->count++];
    ZeroMemory(command, sizeof(*command));
    command->type = MAG_UI_DRAW_TEXT;
    command->rect = *rect;
    command->color = color;
    command->fontSize = fontSize;
    lstrcpyn(command->text, value, ARRAYSIZE(command->text));
    return TRUE;
}

UINT magGraphicsGetBackendCount(void)
{
    return ARRAYSIZE(g_graphicsBackends);
}

const MAGGRAPHICSBACKEND* magGraphicsGetBackendAt(UINT index)
{
    return index < ARRAYSIZE(g_graphicsBackends) ? g_graphicsBackends[index] : NULL;
}

const MAGGRAPHICSBACKEND* magGraphicsGetBackend(GRAPHICSAPI api)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(g_graphicsBackends); ++i)
    {
      if (g_graphicsBackends[i]->api == api)
      {
        return g_graphicsBackends[i];
      }
    }

    return NULL;
}
