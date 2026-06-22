#include "render.h"

#pragma comment(lib, "d3d11")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "dxguid")

#define WGLCHECK(Func) do { if (!(Func)) { __debugbreak(); } } while (0)
#define SAFERELEASE(Obj) do { if ((Obj)) { IUnknown_Release((IUnknown*)(Obj)); (Obj) = NULL; } } while (0)

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax);
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
void render_dxgiDeleteResources(HWND hWnd);
void render_dxgiDeleteOutputResources(LPDXGIOUTPUTCAPTURE lpOutput);
LPDXGIOUTPUTCAPTURE render_dxgiFindOutputCapture(LPSHAREDWGLDATA lpsd, HMONITOR hMonitor);
LPDXGIOUTPUTCAPTURE render_dxgiFindFreeOutputCapture(LPSHAREDWGLDATA lpsd);
BOOL render_dxgiCreateDuplicationForMonitor(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput);
BOOL render_dxgiEnsureDuplication(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput);
BOOL render_dxgiEnsureStagingTexture(LPDXGIOUTPUTCAPTURE lpOutput, UINT width, UINT height);
BOOL render_dxgiUpdateFrame(LPDXGIOUTPUTCAPTURE lpOutput);
void render_dxgiCopyMappedPixelsToRect(LPSHAREDWGLDATA lpsd, const BYTE* src, UINT srcWidth, UINT srcHeight, UINT srcPitch, const RECT* lprcDst);
BOOL render_dxgiCaptureIntersection(LPSHAREDWGLDATA lpsd, LPDXGIOUTPUTCAPTURE lpOutput, const RECT* lprcSource, const RECT* lprcIntersection);
void render_dxgiCaptureScreen(HWND hWnd);

LONG render_clipSourceOrigin(LONG origin, LONG sourceExtent, LONG clipMin, LONG clipMax)
{
    const LONG maxOrigin = clipMax - sourceExtent;

    if (maxOrigin < clipMin)
    {
      return clipMin;
    }

    return CLAMP(origin, clipMin, maxOrigin);
}

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

void render_dxgiDeleteOutputResources(LPDXGIOUTPUTCAPTURE lpOutput)
{
    SAFERELEASE(lpOutput->dxgiStagingTexture);
    SAFERELEASE(lpOutput->dxgiFrameTexture);
    SAFERELEASE(lpOutput->dxgiDuplication);
    SAFERELEASE(lpOutput->d3dContext);
    SAFERELEASE(lpOutput->d3dDevice);
    ZeroMemory(lpOutput, sizeof(*lpOutput));
}

void render_dxgiDeleteResources(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      render_dxgiDeleteOutputResources(&lpsd->dxgiOutputs[i]);
    }
}

LPDXGIOUTPUTCAPTURE render_dxgiFindOutputCapture(LPSHAREDWGLDATA lpsd, HMONITOR hMonitor)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      if (lpsd->dxgiOutputs[i].hMonitor == hMonitor)
      {
        return &lpsd->dxgiOutputs[i];
      }
    }

    return NULL;
}

LPDXGIOUTPUTCAPTURE render_dxgiFindFreeOutputCapture(LPSHAREDWGLDATA lpsd)
{
    UINT i;

    for (i = 0; i < ARRAYSIZE(lpsd->dxgiOutputs); ++i)
    {
      if (!lpsd->dxgiOutputs[i].hMonitor)
      {
        return &lpsd->dxgiOutputs[i];
      }
    }

    return NULL;
}

BOOL render_dxgiCreateDuplicationForMonitor(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDXGIOUTPUTCAPTURE lpOutput = render_dxgiFindFreeOutputCapture(lpsd);
    IDXGIFactory1* factory = NULL;
    IDXGIAdapter1* adapter = NULL;
    IDXGIOutput* output = NULL;
    IDXGIOutput1* output1 = NULL;
    HRESULT hr;
    UINT adapterIndex;
    BOOL fResult = FALSE;

    if (!lpOutput)
    {
      return FALSE;
    }

    hr = CreateDXGIFactory1(&IID_IDXGIFactory1, (void**)&factory);
    if (FAILED(hr))
    {
      return FALSE;
    }

    for (adapterIndex = 0; SUCCEEDED(IDXGIFactory1_EnumAdapters1(factory, adapterIndex, &adapter)); ++adapterIndex)
    {
      UINT outputIndex;

      for (outputIndex = 0; SUCCEEDED(IDXGIAdapter1_EnumOutputs(adapter, outputIndex, &output)); ++outputIndex)
      {
        DXGI_OUTPUT_DESC outputDesc;

        if (SUCCEEDED(IDXGIOutput_GetDesc(output, &outputDesc)) && outputDesc.Monitor == hMonitor)
        {
          D3D11_TEXTURE2D_DESC frameDesc = { 0 };
          D3D_FEATURE_LEVEL featureLevel;

          hr = D3D11CreateDevice(
            (IDXGIAdapter*)adapter,
            D3D_DRIVER_TYPE_UNKNOWN,
            NULL,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT,
            NULL,
            0,
            D3D11_SDK_VERSION,
            &lpOutput->d3dDevice,
            &featureLevel,
            &lpOutput->d3dContext);

          if (SUCCEEDED(hr))
          {
            hr = IDXGIOutput_QueryInterface(output, &IID_IDXGIOutput1, (void**)&output1);
          }

          if (SUCCEEDED(hr))
          {
            hr = IDXGIOutput1_DuplicateOutput(output1, (IUnknown*)lpOutput->d3dDevice, &lpOutput->dxgiDuplication);
          }

          if (SUCCEEDED(hr))
          {
            frameDesc.Width = RECTWIDTH(outputDesc.DesktopCoordinates);
            frameDesc.Height = RECTHEIGHT(outputDesc.DesktopCoordinates);
            frameDesc.MipLevels = 1;
            frameDesc.ArraySize = 1;
            frameDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
            frameDesc.SampleDesc.Count = 1;
            frameDesc.Usage = D3D11_USAGE_DEFAULT;

            hr = ID3D11Device_CreateTexture2D(lpOutput->d3dDevice, &frameDesc, NULL, &lpOutput->dxgiFrameTexture);
          }

          if (SUCCEEDED(hr))
          {
            lpOutput->hMonitor = hMonitor;
            lpOutput->rcOutput = outputDesc.DesktopCoordinates;
            *lplpOutput = lpOutput;
            fResult = TRUE;
          }

          SAFERELEASE(output1);
          SAFERELEASE(output);
          SAFERELEASE(adapter);
          SAFERELEASE(factory);

          if (!fResult)
          {
            render_dxgiDeleteOutputResources(lpOutput);
          }

          return fResult;
        }

        SAFERELEASE(output);
      }

      SAFERELEASE(adapter);
    }

    SAFERELEASE(factory);
    return FALSE;
}

BOOL render_dxgiEnsureDuplication(HWND hWnd, HMONITOR hMonitor, LPDXGIOUTPUTCAPTURE* lplpOutput)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);
    LPDXGIOUTPUTCAPTURE lpOutput = render_dxgiFindOutputCapture(lpsd, hMonitor);

    if (lpOutput && lpOutput->dxgiDuplication)
    {
      *lplpOutput = lpOutput;
      return TRUE;
    }

    return render_dxgiCreateDuplicationForMonitor(hWnd, hMonitor, lplpOutput);
}

BOOL render_dxgiEnsureStagingTexture(LPDXGIOUTPUTCAPTURE lpOutput, UINT width, UINT height)
{
    D3D11_TEXTURE2D_DESC desc = { 0 };

    if (lpOutput->dxgiStagingTexture &&
        lpOutput->dxgiStagingWidth == width &&
        lpOutput->dxgiStagingHeight == height)
    {
      return TRUE;
    }

    SAFERELEASE(lpOutput->dxgiStagingTexture);
    lpOutput->dxgiStagingWidth = 0;
    lpOutput->dxgiStagingHeight = 0;

    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    if (FAILED(ID3D11Device_CreateTexture2D(lpOutput->d3dDevice, &desc, NULL, &lpOutput->dxgiStagingTexture)))
    {
      return FALSE;
    }

    lpOutput->dxgiStagingWidth = width;
    lpOutput->dxgiStagingHeight = height;
    return TRUE;
}

BOOL render_dxgiUpdateFrame(LPDXGIOUTPUTCAPTURE lpOutput)
{
    IDXGIResource* frameResource = NULL;
    ID3D11Texture2D* frameTexture = NULL;
    DXGI_OUTDUPL_FRAME_INFO frameInfo = { 0 };
    HRESULT hr = IDXGIOutputDuplication_AcquireNextFrame(lpOutput->dxgiDuplication, 0, &frameInfo, &frameResource);

    if (DXGI_ERROR_WAIT_TIMEOUT == hr)
    {
      return lpOutput->fHasFrame;
    }

    if (DXGI_ERROR_ACCESS_LOST == hr)
    {
      render_dxgiDeleteOutputResources(lpOutput);
      return FALSE;
    }

    if (FAILED(hr))
    {
      return FALSE;
    }

    hr = IDXGIResource_QueryInterface(frameResource, &IID_ID3D11Texture2D, (void**)&frameTexture);
    if (SUCCEEDED(hr))
    {
      ID3D11DeviceContext_CopyResource(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiFrameTexture, (ID3D11Resource*)frameTexture);
      lpOutput->fHasFrame = TRUE;
    }

    SAFERELEASE(frameTexture);
    SAFERELEASE(frameResource);
    IDXGIOutputDuplication_ReleaseFrame(lpOutput->dxgiDuplication);

    return lpOutput->fHasFrame;
}

void render_dxgiCopyMappedPixelsToRect(LPSHAREDWGLDATA lpsd, const BYTE* src, UINT srcWidth, UINT srcHeight, UINT srcPitch, const RECT* lprcDst)
{
    const UINT dstWidth = (UINT)RECTWIDTH((*lprcDst));
    const UINT dstHeight = (UINT)RECTHEIGHT((*lprcDst));
    const UINT screenWidth = (UINT)lpsd->bi.biWidth;
    const UINT screenHeight = (UINT)lpsd->bi.biHeight;
    const UINT screenPitch = screenWidth * CHANNELS;
    UINT y;

    if (!dstWidth || !dstHeight || !srcWidth || !srcHeight)
    {
      return;
    }

    for (y = 0; y < dstHeight; ++y)
    {
      const UINT srcY = min((UINT)(((ULONGLONG)y * srcHeight) / dstHeight), srcHeight - 1);
      const UINT dstY = (UINT)lprcDst->top + y;
      BYTE* dstRow = ((BYTE*)lpsd->glScreenData) + ((screenHeight - 1 - dstY) * screenPitch) + ((UINT)lprcDst->left * CHANNELS);
      const BYTE* srcRow = src + (srcY * srcPitch);

      if (srcWidth == dstWidth)
      {
        CopyMemory(dstRow, srcRow, dstWidth * CHANNELS);
      }
      else
      {
        UINT x;

        for (x = 0; x < dstWidth; ++x)
        {
          const UINT srcX = min((UINT)(((ULONGLONG)x * srcWidth) / dstWidth), srcWidth - 1);
          CopyMemory(dstRow + (x * CHANNELS), srcRow + (srcX * CHANNELS), CHANNELS);
        }
      }
    }
}

BOOL render_dxgiCaptureIntersection(LPSHAREDWGLDATA lpsd, LPDXGIOUTPUTCAPTURE lpOutput, const RECT* lprcSource, const RECT* lprcIntersection)
{
    const UINT srcPartWidth = (UINT)RECTWIDTH((*lprcIntersection));
    const UINT srcPartHeight = (UINT)RECTHEIGHT((*lprcIntersection));
    RECT rcDst;
    D3D11_BOX box;
    D3D11_MAPPED_SUBRESOURCE mapped;
    HRESULT hr;

    if (!render_dxgiUpdateFrame(lpOutput) ||
        !render_dxgiEnsureStagingTexture(lpOutput, srcPartWidth, srcPartHeight))
    {
      return FALSE;
    }

    box.left = (UINT)(lprcIntersection->left - lpOutput->rcOutput.left);
    box.top = (UINT)(lprcIntersection->top - lpOutput->rcOutput.top);
    box.front = 0;
    box.right = box.left + srcPartWidth;
    box.bottom = box.top + srcPartHeight;
    box.back = 1;

    ID3D11DeviceContext_CopySubresourceRegion(
      lpOutput->d3dContext,
      (ID3D11Resource*)lpOutput->dxgiStagingTexture,
      0,
      0,
      0,
      0,
      (ID3D11Resource*)lpOutput->dxgiFrameTexture,
      0,
      &box);

    hr = ID3D11DeviceContext_Map(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiStagingTexture, 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr))
    {
      return FALSE;
    }

    rcDst.left = MulDiv(lprcIntersection->left - lprcSource->left, lpsd->bi.biWidth, RECTWIDTH((*lprcSource)));
    rcDst.top = MulDiv(lprcIntersection->top - lprcSource->top, lpsd->bi.biHeight, RECTHEIGHT((*lprcSource)));
    rcDst.right = MulDiv(lprcIntersection->right - lprcSource->left, lpsd->bi.biWidth, RECTWIDTH((*lprcSource)));
    rcDst.bottom = MulDiv(lprcIntersection->bottom - lprcSource->top, lpsd->bi.biHeight, RECTHEIGHT((*lprcSource)));

    render_dxgiCopyMappedPixelsToRect(lpsd, (const BYTE*)mapped.pData, srcPartWidth, srcPartHeight, mapped.RowPitch, &rcDst);
    ID3D11DeviceContext_Unmap(lpOutput->d3dContext, (ID3D11Resource*)lpOutput->dxgiStagingTexture, 0);

    return TRUE;
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
      POINT center = { 0, 0 };
      RECT rcSource = lpsd->di.rc;
      const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
      BOOL fCenterOnCursor;

      if (!ClientToScreen(hWnd, &tl))
      {
        return;
      }

      fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
      if (!fCenterOnCursor)
      {
        center.x = tl.x + cw / 2;
        center.y = tl.y + ch / 2;
      }

      if (IsRectEmpty(&rcSource))
      {
        rcSource.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        rcSource.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        rcSource.right = rcSource.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        rcSource.bottom = rcSource.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
      }

      PatBlt(lpsd->hCaptureDC, 0, 0, cw, ch, BLACKNESS);

      if (m <= 1.0001f)
      {
        LONG srcX = tl.x;
        LONG srcY = tl.y;

        if (fCenterOnCursor)
        {
          srcX = center.x - cw / 2;
          srcY = center.y - ch / 2;
        }

        if (fCenterOnCursor)
        {
          srcX = render_clipSourceOrigin(srcX, cw, rcSource.left, rcSource.right);
          srcY = render_clipSourceOrigin(srcY, ch, rcSource.top, rcSource.bottom);
        }

        lpsd->pt = fCenterOnCursor ? center : tl;
        BitBlt(
          lpsd->hCaptureDC,
          0,
          0,
          cw,
          ch,
          lpsd->hDesktopDC,
          srcX,
          srcY,
          SRCCOPY | CAPTUREBLT);
      }
      else
      {
        LONG srcW = (LONG)(cw / m);
        LONG srcH = (LONG)(ch / m);
        LONG srcX;
        LONG srcY;

        if (srcW < 1) srcW = 1;
        if (srcH < 1) srcH = 1;

        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
        if (fCenterOnCursor)
        {
          srcX = render_clipSourceOrigin(srcX, srcW, rcSource.left, rcSource.right);
          srcY = render_clipSourceOrigin(srcY, srcH, rcSource.top, rcSource.bottom);
        }
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

void render_dxgiCaptureScreen(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    if (lpsd->glScreenData)
    {
      const LONG cw = lpsd->bi.biWidth;
      const LONG ch = lpsd->bi.biHeight;
      POINT tl = { 0, 0 };
      POINT center = { 0, 0 };
      RECT rcVirtual = lpsd->di.rc;
      RECT rcSource;
      const FLOAT m = (lpsd->fTexScaler < 1.0f) ? 1.0f : lpsd->fTexScaler;
      BOOL fCenterOnCursor;
      LONG srcW;
      LONG srcH;
      LONG srcX;
      LONG srcY;
      UINT i;
      BOOL fCapturedAny = FALSE;

      if (!ClientToScreen(hWnd, &tl))
      {
        return;
      }

      fCenterOnCursor = lpsd->fTrackCursor && GetCursorPos(&center);
      if (!fCenterOnCursor)
      {
        center.x = tl.x + cw / 2;
        center.y = tl.y + ch / 2;
      }

      if (m <= 1.0001f)
      {
        srcW = cw;
        srcH = ch;
        srcX = fCenterOnCursor ? center.x - srcW / 2 : tl.x;
        srcY = fCenterOnCursor ? center.y - srcH / 2 : tl.y;
        lpsd->pt = fCenterOnCursor ? center : tl;
      }
      else
      {
        srcW = (LONG)(cw / m);
        srcH = (LONG)(ch / m);

        if (srcW < 1) srcW = 1;
        if (srcH < 1) srcH = 1;

        srcX = center.x - srcW / 2;
        srcY = center.y - srcH / 2;
        lpsd->pt = center;
      }

      if (IsRectEmpty(&rcVirtual))
      {
        rcVirtual.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
        rcVirtual.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
        rcVirtual.right = rcVirtual.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
        rcVirtual.bottom = rcVirtual.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
      }

      srcX = render_clipSourceOrigin(srcX, srcW, rcVirtual.left, rcVirtual.right);
      srcY = render_clipSourceOrigin(srcY, srcH, rcVirtual.top, rcVirtual.bottom);
      SetRect(&rcSource, srcX, srcY, srcX + srcW, srcY + srcH);

      ZeroMemory(lpsd->glScreenData, lpsd->bi.biSizeImage);

      for (i = 0; i < lpsd->di.numMonitors; ++i)
      {
        RECT rcMonitor = lpsd->di.monitors[i].monitorInfoEx.rcMonitor;
        RECT rcIntersection;
        POINT ptIntersection;
        HMONITOR hMonitor;
        LPDXGIOUTPUTCAPTURE lpOutput = NULL;

        if (!IntersectRect(&rcIntersection, &rcSource, &rcMonitor))
        {
          continue;
        }

        ptIntersection.x = rcIntersection.left + RECTWIDTH(rcIntersection) / 2;
        ptIntersection.y = rcIntersection.top + RECTHEIGHT(rcIntersection) / 2;
        hMonitor = MonitorFromPoint(ptIntersection, MONITOR_DEFAULTTONULL);

        if (!hMonitor ||
            !render_dxgiEnsureDuplication(hWnd, hMonitor, &lpOutput) ||
            !render_dxgiCaptureIntersection(lpsd, lpOutput, &rcSource, &rcIntersection))
        {
          render_gdiCaptureScreen(hWnd);
          return;
        }

        fCapturedAny = TRUE;
      }

      if (!fCapturedAny)
      {
        render_gdiCaptureScreen(hWnd);
      }
    }
}

void renderInit(HWND hWnd)
{
    render_wglInit(hWnd);
    //render_wglInitPBuffer(hWnd);
}

void renderCleanup(HWND hWnd)
{
    render_dxgiDeleteResources(hWnd);
}

void renderResizeCapture(HWND hWnd)
{
    render_gdiResizeSurface(hWnd);
    render_wglResizeSurface(hWnd);
}

void renderRender(HWND hWnd)
{
    LPSHAREDWGLDATA lpsd = (LPSHAREDWGLDATA)GetWindowLongPtr(hWnd, GWLP_USERDATA);

    switch (lpsd->captureApi)
    {
    case CAPTURE_API_DXGI_DESKTOP_DUPLICATION:
      render_dxgiCaptureScreen(hWnd);
      break;
    case CAPTURE_API_GDI_BITBLT:
    default:
      render_gdiCaptureScreen(hWnd);
      break;
    }

    render_wglRender(hWnd);
}
