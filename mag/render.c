#include "render.h"

#define WGLCHECK(Func) do { if (!(Func)) { __debugbreak(); } } while (0)

void render_wglInit(HWND hWnd);
void render_wglInitPBuffer(HWND hWnd);
void render_wglCreateResources(HWND hWnd);
void render_gdiCreateResources(HWND hWnd);
void render_wglResizeSurface(HWND hWnd);
void render_gdiResizeSurface(HWND hWnd);
void render_wglRender(HWND hWnd);
void render_gdiCaptureScreen(HWND hWnd);
void render_updateSurfaceInfo(HWND hWnd);
BOOL render_gdiCreateCaptureBitmap(HWND hWnd);
void render_gdiDeleteCaptureBitmap(HWND hWnd);

void render_updateSurfaceInfo(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    RECT rc = { 0 };
    LONG width;
    LONG height;

    GetClientRect(hWnd, &rc);
    width = RECTWIDTH(rc);
    height = RECTHEIGHT(rc);

    if (width < 1) width = 1;
    if (height < 1) height = 1;

    lpsd->rc = rc;
    lpsd->bi.biSize = sizeof(lpsd->bi);
    lpsd->bi.biWidth = width;
    lpsd->bi.biHeight = height;
    lpsd->bi.biPlanes = 1;
    lpsd->bi.biBitCount = BITS_PER_PIXEL;
    lpsd->bi.biCompression = BI_RGB;
    lpsd->bi.biSizeImage = width * height * CHANNELS;
}

BOOL render_gdiCreateCaptureBitmap(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    BITMAPINFO bmi = { 0 };
    VOID* bits = NULL;

    if (!lpsd->hDesktopDC || !lpsd->hCaptureDC)
    {
      return FALSE;
    }

    bmi.bmiHeader = lpsd->bi;
    lpsd->hBitmapBg = CreateDIBSection(lpsd->hDesktopDC, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);

    if (!lpsd->hBitmapBg || !bits)
    {
      if (lpsd->hBitmapBg)
      {
        DeleteBitmap(lpsd->hBitmapBg);
        lpsd->hBitmapBg = NULL;
      }

      lpsd->glScreenData = NULL;
      return FALSE;
    }

    lpsd->hBitmapOld = SelectBitmap(lpsd->hCaptureDC, lpsd->hBitmapBg);
    if (!lpsd->hBitmapOld)
    {
      DeleteBitmap(lpsd->hBitmapBg);
      lpsd->hBitmapBg = NULL;
      lpsd->glScreenData = NULL;
      return FALSE;
    }

    lpsd->glScreenData = (GLubyte*)bits;
    return TRUE;
}

void render_gdiDeleteCaptureBitmap(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hCaptureDC && lpsd->hBitmapOld)
    {
      SelectBitmap(lpsd->hCaptureDC, lpsd->hBitmapOld);
      lpsd->hBitmapOld = NULL;
    }

    if (lpsd->hBitmapBg)
    {
      DeleteBitmap(lpsd->hBitmapBg);
      lpsd->hBitmapBg = NULL;
    }

    lpsd->glScreenData = NULL;
}

void render_wglInit(HWND hWnd)
{
    const PIXELFORMATDESCRIPTOR pfd =
    {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,
      PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      PFD_TYPE_RGBA,
      32,
      0, 0, 0, 0, 0, 0,
      8,
      0,
      0,
      0, 0, 0, 0,
      24,
      8,
      0,
      PFD_MAIN_PLANE,
      0,
      0, 0, 0
    };

    const int iAttribs[] =
    {
      WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
      WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
      WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
      WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
      WGL_COLOR_BITS_ARB, 32,
      WGL_DEPTH_BITS_ARB, 24,
      WGL_ALPHA_BITS_ARB, 8,
      WGL_SWAP_METHOD_ARB,WGL_SWAP_COPY_ARB,
      WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
      0
    };

    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    wglInit();
    lpsd->hDC = GetDC(hWnd);
    SetPixelFormat(lpsd->hDC, wglFindPixelFormat(lpsd->hDC, iAttribs, NULL), &pfd);
    lpsd->hRC = wglCreateContextAttribsARB(lpsd->hDC, NULL, NULL);
    wglMakeCurrent(lpsd->hDC, lpsd->hRC);

    render_updateSurfaceInfo(hWnd);
    render_gdiCreateResources(hWnd);
    render_wglCreateResources(hWnd);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    wglSwapIntervalEXT(0);
}

void render_wglInitPBuffer(HWND hWnd)
{
    // basic pixel format
    const PIXELFORMATDESCRIPTOR pfd =
    {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,
      PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
      PFD_TYPE_RGBA,
      32,
      0, 0, 0, 0, 0, 0,
      8,
      0,
      0,
      0, 0, 0, 0,
      24,
      8,
      0,
      PFD_MAIN_PLANE,
      0,
      0, 0, 0
    };

    const int iAttribs[] =
    {
      WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
      WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
      WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
      WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
      WGL_COLOR_BITS_ARB, 32,
      WGL_DEPTH_BITS_ARB, 24,
      WGL_ALPHA_BITS_ARB, 8,
      WGL_SWAP_METHOD_ARB,WGL_SWAP_COPY_ARB,
      WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
      0
    };

    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    wglInit();
    lpsd->hDC = GetDC(0);

    int pf = ChoosePixelFormat(lpsd->hDC, &pfd);
    WGLCHECK(SetPixelFormat(lpsd->hDC, pf, &pfd));

    // dummy GL context
    HGLRC rc = wglCreateContext(lpsd->hDC);
    WGLCHECK(wglMakeCurrent(lpsd->hDC, rc));

    // choose pbuffer format
    int iattribs[] = {
        WGL_DRAW_TO_PBUFFER_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB,  GL_TRUE,
        WGL_BIND_TO_TEXTURE_RGBA_ARB, GL_TRUE,
        WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
        0
    };

    int formats[1];
    UINT count;
    if (!wglChoosePixelFormatARB(lpsd->hDC, iattribs, NULL, 1, formats, &count))
      __debugbreak();

    // create pbuffer
    int pattribs[] = {
        WGL_TEXTURE_FORMAT_ARB, WGL_TEXTURE_RGBA_ARB,
        WGL_TEXTURE_TARGET_ARB, WGL_TEXTURE_2D_ARB,
        0
    };

    lpsd->pb = wglCreatePbufferARB(lpsd->hDC, formats[0], 256, 256, pattribs);
    HDC pbdc = wglGetPbufferDCARB(lpsd->pb);

    // render context for pbuffer
    HGLRC pbrc = wglCreateContext(pbdc);
    wglMakeCurrent(pbdc, pbrc);

    render_updateSurfaceInfo(hWnd);
    render_gdiCreateResources(hWnd);
    render_wglCreateResources(hWnd);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_BLEND);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_TEXTURE_2D);

    wglSwapIntervalEXT(0);
}

void render_wglCreateResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    glGenTextures(1, &lpsd->glScreenTexture);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, lpsd->bi.biWidth, lpsd->bi.biHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);

    lpsd->fScale = 1.0f;
    lpsd->fTexScaler = 1.0f;
}

void render_gdiCreateResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    
    gdiGetDisplayInfo(&lpsd->di);
    lpsd->hDesktopDC = GetDC(NULL); // whole virtual screen (handles negative/secondary monitor coords)
    lpsd->hCaptureDC = CreateCompatibleDC(lpsd->hDesktopDC);
    render_updateSurfaceInfo(hWnd);
    render_gdiCreateCaptureBitmap(hWnd);
}

void render_wglResizeSurface(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd->hRC || !lpsd->glScreenTexture)
    {
      return;
    }
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glDeleteTextures(1, &lpsd->glScreenTexture);

    glGenTextures(1, &lpsd->glScreenTexture);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, lpsd->bi.biWidth, lpsd->bi.biHeight, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
}

void render_gdiResizeSurface(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    render_updateSurfaceInfo(hWnd);

    if (!lpsd->hCaptureDC)
    {
      return;
    }

    render_gdiDeleteCaptureBitmap(hWnd);
    render_gdiCreateCaptureBitmap(hWnd);
}

void render_wglRender(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!lpsd->glScreenData || !lpsd->glScreenTexture)
    {
      return;
    }

    //glClearColor(
    //  lpsd->cfClearColor[0],
    //  lpsd->cfClearColor[1],
    //  lpsd->cfClearColor[2],
    //  lpsd->cfClearColor[3]);
    //glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, lpsd->bi.biWidth, lpsd->bi.biHeight);
    glDisable(GL_BLEND);

    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    //glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_PRIORITY, 1);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lpsd->bi.biWidth, lpsd->bi.biHeight, GL_BGRA, GL_UNSIGNED_BYTE, lpsd->glScreenData);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    //glFlush();
    SwapBuffers(lpsd->hDC);
    glFinish();
}

void render_gdiCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hBitmapBg && lpsd->glScreenData)
    {
      const LONG cw = lpsd->bi.biWidth;
      const LONG ch = lpsd->bi.biHeight;
      POINT tl = { 0, 0 };
      const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;

      if (!ClientToScreen(hWnd, &tl))
      {
        return;
      }

      PatBlt(lpsd->hCaptureDC, 0, 0, cw, ch, BLACKNESS);

      if (m <= 1.0001f)
      {
        lpsd->pt = tl;
        BitBlt(
          lpsd->hCaptureDC,
          0,
          0,
          cw,
          ch,
          lpsd->hDesktopDC,
          tl.x,
          tl.y,
          SRCCOPY | CAPTUREBLT);
      }
      else
      {
        const POINT center = { tl.x + cw / 2, tl.y + ch / 2 };
        LONG srcW = (LONG)(cw / m);
        LONG srcH = (LONG)(ch / m);
        LONG srcX;
        LONG srcY;

        if (srcW < 1) srcW = 1;
        if (srcH < 1) srcH = 1;

        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
        lpsd->pt = center;

        SetStretchBltMode(lpsd->hCaptureDC, COLORONCOLOR);
        StretchBlt(
          lpsd->hCaptureDC,
          0,
          0,
          cw,
          ch,
          lpsd->hDesktopDC,
          srcX,
          srcY,
          srcW,
          srcH,
          SRCCOPY | CAPTUREBLT);
      }

      GdiFlush();
    }
}

void renderInit(HWND hWnd)
{
    render_wglInit(hWnd);
    //render_wglInitPBuffer(hWnd);
}

void renderResizeCapture(HWND hWnd)
{
    render_gdiResizeSurface(hWnd);
    render_wglResizeSurface(hWnd);
}

void renderRender(HWND hWnd)
{
    render_gdiCaptureScreen(hWnd);
    render_wglRender(hWnd);
}
