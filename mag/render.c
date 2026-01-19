#include "render.h"

void render_wglInit(HWND hWnd);
void render_wglRender(HWND hWnd);
void render_gdiCaptureScreen(HWND hWnd);

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
      WGL_SWAP_METHOD_ARB,WGL_SWAP_EXCHANGE_ARB,
      WGL_ACCELERATION_ARB, WGL_FULL_ACCELERATION_ARB,
      WGL_SAMPLE_BUFFERS_ARB, GL_TRUE,
      WGL_SAMPLES_ARB, 4,
      0
    };

    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    int pf;

    wglInit();

    lpsd->hDC = GetDC(hWnd);
    pf = wglFindPixelFormat(lpsd->hDC, iAttribs, NULL);

    SetPixelFormat(lpsd->hDC, pf, &pfd);

    lpsd->hRC = wglCreateContextAttribsARB(lpsd->hDC, NULL, NULL);
    wglMakeCurrent(lpsd->hDC, lpsd->hRC);

    glGenTextures(1, &lpsd->glScreenTexture);
    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);

    // Upload texture data
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    // glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_BGRA, 256, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, lpsd->glScreenData);

    gdiGetDisplayInfo(&lpsd->di);
    lpsd->hDesktopDC = GetDC(GetDesktopWindow());
    lpsd->hCaptureDC = CreateCompatibleDC(lpsd->hDesktopDC);
    lpsd->hBitmapBg = CreateCompatibleBitmap(lpsd->hDesktopDC, RECTWIDTH(lpsd->di.rc), RECTHEIGHT(lpsd->di.rc));
    SelectObject(lpsd->hCaptureDC, lpsd->hBitmapBg);

    lpsd->bi.biSize = sizeof(lpsd->bi);
    lpsd->bi.biWidth = 256;
    lpsd->bi.biHeight = 256;
    lpsd->bi.biPlanes = 1;
    lpsd->bi.biBitCount = 32; // RGBA
    lpsd->bi.biCompression = BI_RGB;

    wglSwapIntervalEXT(0);
    lpsd->fScale = 1.0f;
    lpsd->fTexScaler = 1.0f;
}

void render_wglRender(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (!GetClientRect(hWnd, &lpsd->rc))
    {
      return;
    }

    glClearColor(
      lpsd->cfClearColor[0],
      lpsd->cfClearColor[1],
      lpsd->cfClearColor[2],
      lpsd->cfClearColor[3]);
    glClear(GL_COLOR_BUFFER_BIT);
    glViewport(0, 0, RECTWIDTH(lpsd->rc), RECTHEIGHT(lpsd->rc));

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1.0, 1.0, -1.0, 1.0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_BLEND);
    glEnable(GL_TEXTURE_2D);

    glBindTexture(GL_TEXTURE_2D, lpsd->glScreenTexture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, lpsd->glScreenWidth, lpsd->glScreenHeight, GL_BGRA, GL_UNSIGNED_BYTE, lpsd->glScreenData);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-lpsd->fTexScaler, -lpsd->fTexScaler);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(lpsd->fTexScaler, -lpsd->fTexScaler);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(lpsd->fTexScaler, lpsd->fTexScaler);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-lpsd->fTexScaler, lpsd->fTexScaler);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    D3DKMTWaitForVerticalBlank(hWnd);

    SwapBuffers(lpsd->hDC);
}

void render_gdiCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->hBitmapBg && GetCursorPos(&lpsd->pt))
    {
      BitBlt(
        lpsd->hCaptureDC,
        0,
        0,
        256,
        256,
        lpsd->hDesktopDC,
        CLAMP(lpsd->pt.x - (LONG)(0.5f * 256), lpsd->di.rc.left, lpsd->di.rc.right - 256),
        CLAMP(lpsd->pt.y - (LONG)(0.5f * 256), lpsd->di.rc.top, lpsd->di.rc.bottom - 256),
        SRCCOPY | CAPTUREBLT);

      GetDIBits(lpsd->hCaptureDC, lpsd->hBitmapBg, 0, 256, lpsd->glScreenData, (BITMAPINFO*)&lpsd->bi, DIB_RGB_COLORS);
    }

    // BitBlt(hDC, 0, 0, nScreenWidth, nScreenHeight, hCaptureDC, 0, 0, SRCCOPY);
    // here to save the captured image to disk

    //for (int i = 0; i < 256; ++i)
    //  for (int j = 0; j < 256; ++j)
    //    lpsd->glScreenData[((j + (i * 256)) * 4) + 3] = 225;   // Alpha is at offset 3

    lpsd->glScreenWidth = 256;
    lpsd->glScreenHeight = 256;
}

void renderInit(HWND hWnd)
{
    //render_wglInit(hWnd);
    //ShowWindow(hWnd, SW_SHOW);
    DwmInitPrivate();
    DwmCreateDevice();
    DwmCreateThumbnail(hWnd);
    DwmBindVisual(hWnd);
}

void renderRender(HWND hWnd)
{
    render_gdiCaptureScreen(hWnd);
    render_wglRender(hWnd);

    DwmUpdateVisual(hWnd);
}