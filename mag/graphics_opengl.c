#include "framework.h"
#include "graphics.h"
#include "presentation.h"

#include <gl/GL.h>

typedef struct MAGOPENGLSTATE
{
  HDC    hDC;
  HGLRC  hRC;
  GLuint texture;
  GLuint glyphTexture;
  const MAGGLYPHATLAS* glyphAtlasIdentity;
  UINT   width;
  UINT   height;
  UINT   capacityWidth;
  UINT   capacityHeight;
  UINT64 resourceGeneration;
  UINT   syncInterval;
} MAGOPENGLSTATE;

static BOOL magGraphicsOpenGLIsAvailable(LPTSTR reason, UINT reasonCount)
{
    if (!magGraphicsIsInputDesktop())
    {
      if (reason && reasonCount)
      {
        lstrcpyn(reason, TEXT("Window-system OpenGL is unavailable on the private non-input test desktop."), reasonCount);
      }
      return FALSE;
    }

    if (reason && reasonCount)
    {
      reason[0] = TEXT('\0');
    }
    return TRUE;
}

static BOOL magGraphicsOpenGLCreateTexture(MAGOPENGLSTATE* state, SIZE clientSize)
{
    SIZE reservoirSize;
    GLint maximumTextureSize = 0;

    if (clientSize.cx < 1 || clientSize.cy < 1)
    {
      return FALSE;
    }

    state->width = (UINT)clientSize.cx;
    state->height = (UINT)clientSize.cy;
    if (state->texture &&
        state->width <= state->capacityWidth &&
        state->height <= state->capacityHeight)
    {
      return TRUE;
    }

    reservoirSize = magGraphicsChooseReservoirSize(NULL, clientSize);
    reservoirSize.cx = max(reservoirSize.cx, (LONG)state->capacityWidth);
    reservoirSize.cy = max(reservoirSize.cy, (LONG)state->capacityHeight);
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maximumTextureSize);
    if (maximumTextureSize < clientSize.cx || maximumTextureSize < clientSize.cy)
    {
      return FALSE;
    }
    if (0 < maximumTextureSize)
    {
      reservoirSize.cx = min(reservoirSize.cx, maximumTextureSize);
      reservoirSize.cy = min(reservoirSize.cy, maximumTextureSize);
    }

    if (state->texture)
    {
      glBindTexture(GL_TEXTURE_2D, 0);
      glDeleteTextures(1, &state->texture);
      state->texture = 0;
    }

    glGenTextures(1, &state->texture);
    if (!state->texture)
    {
      return FALSE;
    }

    glBindTexture(GL_TEXTURE_2D, state->texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, reservoirSize.cx, reservoirSize.cy, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    state->capacityWidth = (UINT)reservoirSize.cx;
    state->capacityHeight = (UINT)reservoirSize.cy;
    ++state->resourceGeneration;
    return GL_NO_ERROR == glGetError();
}

static BOOL magGraphicsOpenGLCreate(
  HWND hWnd,
  SIZE clientSize,
  const struct MAGPRESENTATIONSETTINGS* presentation,
  void** stateOut)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };
    MAGOPENGLSTATE* state;
    int pixelFormat;

    if (!stateOut || !presentation)
    {
      return FALSE;
    }
    *stateOut = NULL;
    SetLastError(ERROR_SUCCESS);

    state = (MAGOPENGLSTATE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*state));
    if (!state)
    {
      return FALSE;
    }

    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_SUPPORT_COMPOSITION;
    if (MAG_PRESENT_HARDWARE_LEGACY_COPY_TO_FRONT_BUFFER == presentation->target ||
        MAG_PRESENT_COMPOSED_COPY_GPU_GDI == presentation->target)
    {
      pfd.dwFlags |= PFD_SWAP_COPY;
    }
    else
    {
      pfd.dwFlags |= PFD_SWAP_EXCHANGE;
    }
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    state->hDC = GetDC(hWnd);
    if (!state->hDC)
    {
      HeapFree(GetProcessHeap(), 0, state);
      SetLastError(0xE001);
      return FALSE;
    }

    pixelFormat = GetPixelFormat(state->hDC);
    if (!pixelFormat)
    {
      pixelFormat = ChoosePixelFormat(state->hDC, &pfd);
      if (!pixelFormat || !SetPixelFormat(state->hDC, pixelFormat, &pfd))
      {
        ReleaseDC(hWnd, state->hDC);
        HeapFree(GetProcessHeap(), 0, state);
        SetLastError(0xE002);
        return FALSE;
      }
    }

    state->hRC = wglCreateContext(state->hDC);
    if (!state->hRC || !wglMakeCurrent(state->hDC, state->hRC))
    {
      if (state->hRC)
      {
        wglDeleteContext(state->hRC);
      }
      ReleaseDC(hWnd, state->hDC);
      HeapFree(GetProcessHeap(), 0, state);
      SetLastError(0xE003);
      return FALSE;
    }

    wglLoadExtensions();
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    if (wglSwapIntervalEXT)
    {
      wglSwapIntervalEXT((int)presentation->syncInterval);
    }
    state->syncInterval = presentation->syncInterval;

    if (!magGraphicsOpenGLCreateTexture(state, clientSize))
    {
      const GLenum error = glGetError();
      const DWORD failure = error ? (0xE100 | (DWORD)error) : 0xE004;
      wglMakeCurrent(NULL, NULL);
      wglDeleteContext(state->hRC);
      ReleaseDC(hWnd, state->hDC);
      HeapFree(GetProcessHeap(), 0, state);
      SetLastError(failure);
      return FALSE;
    }

    *stateOut = state;
    return TRUE;
}

static void magGraphicsOpenGLDestroy(HWND hWnd, void* opaqueState)
{
    MAGOPENGLSTATE* state = (MAGOPENGLSTATE*)opaqueState;

    if (!state)
    {
      return;
    }

    if (state->hDC && state->hRC && wglMakeCurrent(state->hDC, state->hRC))
    {
      if (state->texture)
      {
        glDeleteTextures(1, &state->texture);
      }
      if (state->glyphTexture)
      {
        glDeleteTextures(1, &state->glyphTexture);
      }
    }

    if (wglGetCurrentContext() == state->hRC)
    {
      wglMakeCurrent(NULL, NULL);
    }
    if (state->hRC)
    {
      wglDeleteContext(state->hRC);
    }
    if (state->hDC)
    {
      ReleaseDC(hWnd, state->hDC);
    }
    HeapFree(GetProcessHeap(), 0, state);
}

static BOOL magGraphicsOpenGLResize(HWND hWnd, void* opaqueState, SIZE clientSize)
{
    MAGOPENGLSTATE* state = (MAGOPENGLSTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);

    return state &&
      wglMakeCurrent(state->hDC, state->hRC) &&
      magGraphicsOpenGLCreateTexture(state, clientSize);
}

static BOOL magGraphicsOpenGLSetPresentationEnabled(
  HWND hWnd,
  void* opaqueState,
  BOOL enabled)
{
    MAGOPENGLSTATE* state = (MAGOPENGLSTATE*)opaqueState;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !state->hDC || !state->hRC)
    {
      return FALSE;
    }

    if (enabled)
    {
      return wglGetCurrentContext() == state->hRC ||
        wglMakeCurrent(state->hDC, state->hRC);
    }

    return wglGetCurrentContext() != state->hRC ||
      wglMakeCurrent(NULL, NULL);
}

static void magGraphicsOpenGLVertex(const MAGOPENGLSTATE* state, FLOAT x, FLOAT y)
{
    glVertex2f(
      -1.0f + (2.0f * x / (FLOAT)state->width),
      1.0f - (2.0f * y / (FLOAT)state->height));
}

static void magGraphicsOpenGLFillRect(const MAGOPENGLSTATE* state, const MAGUIDRAWCOMMAND* command)
{
    glColor4f(command->color.r, command->color.g, command->color.b, command->color.a);
    glBegin(GL_QUADS);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.left, (FLOAT)command->rect.top);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.right, (FLOAT)command->rect.top);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.right, (FLOAT)command->rect.bottom);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.left, (FLOAT)command->rect.bottom);
    glEnd();
}

static void magGraphicsOpenGLStrokeRect(const MAGOPENGLSTATE* state, const MAGUIDRAWCOMMAND* command)
{
    glColor4f(command->color.r, command->color.g, command->color.b, command->color.a);
    glLineWidth(command->thickness);
    glBegin(GL_LINE_LOOP);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.left, (FLOAT)command->rect.top);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.right, (FLOAT)command->rect.top);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.right, (FLOAT)command->rect.bottom);
    magGraphicsOpenGLVertex(state, (FLOAT)command->rect.left, (FLOAT)command->rect.bottom);
    glEnd();
    glLineWidth(1.0f);
}

static BOOL magGraphicsOpenGLEnsureGlyphAtlas(
  MAGOPENGLSTATE* state,
  const MAGGLYPHATLAS* atlas)
{
    if (!atlas || !atlas->alpha || !atlas->width || !atlas->height ||
        atlas->stride != atlas->width)
    {
      return FALSE;
    }
    if (state->glyphTexture && state->glyphAtlasIdentity == atlas)
    {
      return TRUE;
    }

    if (state->glyphTexture)
    {
      glDeleteTextures(1, &state->glyphTexture);
      state->glyphTexture = 0;
    }

    glGenTextures(1, &state->glyphTexture);
    if (!state->glyphTexture)
    {
      return FALSE;
    }
    glBindTexture(GL_TEXTURE_2D, state->glyphTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(
      GL_TEXTURE_2D,
      0,
      GL_ALPHA,
      atlas->width,
      atlas->height,
      0,
      GL_ALPHA,
      GL_UNSIGNED_BYTE,
      atlas->alpha);
    state->glyphAtlasIdentity = atlas;
    return GL_NO_ERROR == glGetError();
}

static void magGraphicsOpenGLTexturedVertex(
  const MAGOPENGLSTATE* state,
  FLOAT x,
  FLOAT y,
  FLOAT s,
  FLOAT t)
{
    glTexCoord2f(s, t);
    magGraphicsOpenGLVertex(state, x, y);
}

static void magGraphicsOpenGLDrawText(
  MAGOPENGLSTATE* state,
  const MAGUIDRAWCOMMAND* command,
  const MAGGLYPHATLAS* atlas)
{
    RECT bounds = { 0, 0, (LONG)state->width, (LONG)state->height };
    LONG destinationX = command->rect.left;
    const LONG destinationY = command->rect.top;
    UINT characterIndex;

    if (!magGraphicsOpenGLEnsureGlyphAtlas(state, atlas))
    {
      return;
    }

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, state->glyphTexture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    glColor4f(command->color.r, command->color.g, command->color.b, command->color.a);
    glBegin(GL_QUADS);
    for (characterIndex = 0;
         command->text[characterIndex] && destinationX < command->rect.right;
         ++characterIndex)
    {
      WCHAR character = command->text[characterIndex];
      UINT glyphIndex;
      const MAGGLYPHINFO* glyph;
      RECT destination;
      RECT clip;
      RECT boundedClip;
      FLOAT s0;
      FLOAT t0;
      FLOAT s1;
      FLOAT t1;

      if (character < atlas->firstCharacter ||
          character >= atlas->firstCharacter + atlas->glyphCount)
      {
        character = L'?';
      }
      glyphIndex = character - atlas->firstCharacter;
      glyph = &atlas->glyphs[glyphIndex];
      SetRect(
        &destination,
        destinationX,
        destinationY,
        destinationX + RECTWIDTH(glyph->rect),
        destinationY + RECTHEIGHT(glyph->rect));
      if (IntersectRect(&clip, &destination, &command->rect) &&
          IntersectRect(&boundedClip, &clip, &bounds))
      {
        clip = boundedClip;
        s0 = (FLOAT)(glyph->rect.left + clip.left - destination.left) / (FLOAT)atlas->width;
        t0 = (FLOAT)(glyph->rect.top + clip.top - destination.top) / (FLOAT)atlas->height;
        s1 = (FLOAT)(glyph->rect.left + clip.right - destination.left) / (FLOAT)atlas->width;
        t1 = (FLOAT)(glyph->rect.top + clip.bottom - destination.top) / (FLOAT)atlas->height;
        magGraphicsOpenGLTexturedVertex(state, (FLOAT)clip.left, (FLOAT)clip.top, s0, t0);
        magGraphicsOpenGLTexturedVertex(state, (FLOAT)clip.right, (FLOAT)clip.top, s1, t0);
        magGraphicsOpenGLTexturedVertex(state, (FLOAT)clip.right, (FLOAT)clip.bottom, s1, t1);
        magGraphicsOpenGLTexturedVertex(state, (FLOAT)clip.left, (FLOAT)clip.bottom, s0, t1);
      }
      destinationX += glyph->advance;
    }
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

static BOOL magGraphicsOpenGLRender(
  HWND hWnd,
  void* opaqueState,
  const MAGPIXELBUFFER* frame,
  const MAGUIDRAWLIST* ui,
  const MAGPRESENTINTENT* intent)
{
    MAGOPENGLSTATE* state = (MAGOPENGLSTATE*)opaqueState;
    UINT i;
    const FLOAT uMax = state && state->capacityWidth
      ? (FLOAT)state->width / (FLOAT)state->capacityWidth
      : 1.0f;
    const FLOAT vMax = state && state->capacityHeight
      ? (FLOAT)state->height / (FLOAT)state->capacityHeight
      : 1.0f;

    UNREFERENCED_PARAMETER(hWnd);
    if (!state || !frame || !frame->pixels ||
        frame->width != state->width || frame->height != state->height ||
        !wglMakeCurrent(state->hDC, state->hRC))
    {
      return FALSE;
    }

    glViewport(0, 0, state->width, state->height);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, state->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, (GLint)(frame->stride / 4U));
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, state->width, state->height, GL_BGRA, GL_UNSIGNED_BYTE, frame->pixels);
    glPixelStorei(0x0CF2 /* GL_UNPACK_ROW_LENGTH */, 0);

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, frame->rowOrder == MAG_ROW_ORDER_TOP_DOWN ? vMax : 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(uMax, frame->rowOrder == MAG_ROW_ORDER_TOP_DOWN ? vMax : 0.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(uMax, frame->rowOrder == MAG_ROW_ORDER_TOP_DOWN ? 0.0f : vMax);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, frame->rowOrder == MAG_ROW_ORDER_TOP_DOWN ? 0.0f : vMax);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    glDisable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    if (ui)
    {
      for (i = 0; i < ui->count; ++i)
      {
        if (MAG_UI_DRAW_FILL_RECT == ui->commands[i].type)
        {
          magGraphicsOpenGLFillRect(state, &ui->commands[i]);
        }
        else if (MAG_UI_DRAW_STROKE_RECT == ui->commands[i].type)
        {
          magGraphicsOpenGLStrokeRect(state, &ui->commands[i]);
        }
        else if (MAG_UI_DRAW_TEXT == ui->commands[i].type && ui->glyphAtlas)
        {
          magGraphicsOpenGLDrawText(state, &ui->commands[i], ui->glyphAtlas);
        }
      }
    }

    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glFlush();
    if (intent && !intent->synchronize && wglSwapIntervalEXT)
    {
      wglSwapIntervalEXT(0);
    }
    {
      BOOL presented = SwapBuffers(state->hDC);
      if (intent && !intent->synchronize && wglSwapIntervalEXT)
      {
        wglSwapIntervalEXT((int)state->syncInterval);
      }
      return presented;
    }
}

static HANDLE magGraphicsOpenGLGetFrameWaitHandle(void* state)
{
    UNREFERENCED_PARAMETER(state);
    return NULL;
}

static UINT64 magGraphicsOpenGLGetResourceGeneration(void* opaqueState)
{
    MAGOPENGLSTATE* state = (MAGOPENGLSTATE*)opaqueState;
    return state ? state->resourceGeneration : 0;
}

const MAGGRAPHICSBACKEND g_magGraphicsOpenGLBackend =
{
  GRAPHICS_API_OPENGL,
  TEXT("OpenGL"),
  TRUE,
  magGraphicsOpenGLIsAvailable,
  magGraphicsOpenGLCreate,
  magGraphicsOpenGLDestroy,
  magGraphicsOpenGLResize,
  magGraphicsOpenGLSetPresentationEnabled,
  magGraphicsOpenGLRender,
  magGraphicsOpenGLGetFrameWaitHandle,
  magGraphicsOpenGLGetResourceGeneration,
  NULL,
  NULL,
};
