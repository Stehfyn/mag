#include "wingdix.h"

#pragma comment (lib, "opengl32")

ATOM wgl_RegisterClass(void);
BOOL wgl_SelectPixelFormat(HDC hDC);
void wgl_Load(void);
void wgl_GetProc(HMODULE hModule, const char* pszProc, PROC* pProc);

BOOL __stdcall gdi_GetDisplayInfo(HMONITOR hMonitor, HDC hDC, LPRECT lprc, LPARAM lParam);

ATOM wgl_RegisterClass(void)
{
    WNDCLASS wc = { 0 };

    wc.hInstance     = GetModuleHandle(NULL);
    wc.lpfnWndProc   = DefWindowProc;
    wc.lpszClassName = _T("wglDummyWindow");

    return RegisterClass(&wc);
}

BOOL wgl_SelectPixelFormat(HDC hDC)
{
    PIXELFORMATDESCRIPTOR pfd = { sizeof(pfd) };

    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_SUPPORT_COMPOSITION | PFD_GENERIC_ACCELERATED;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cColorBits = 32;
    pfd.iPixelType = PFD_TYPE_RGBA;

    return SetPixelFormat(hDC, ChoosePixelFormat(hDC, &pfd), &pfd);
}

void wgl_Load(void)
{
    HMODULE __opengl32;

    if (!(__opengl32 = LoadLibrary(TEXT("opengl32.dll"))))
    {
        return;
    }

#define pproc(proc) (# proc), ((PROC*)&proc)
    wgl_GetProc(__opengl32, pproc(wglChoosePixelFormatARB));
    wgl_GetProc(__opengl32, pproc(wglCreateContextAttribsARB));
    wgl_GetProc(__opengl32, pproc(wglSwapIntervalEXT));
    wgl_GetProc(__opengl32, pproc(wglGetSwapIntervalEXT));
#undef pproc
}

void wgl_GetProc(HMODULE hModule, const char* pszProc, PROC* pProc)
{
    typedef PROC(__stdcall* PFN_wglGetProcAddress)(LPCSTR);
    static PFN_wglGetProcAddress wglGetProcAddress2;

    if (!wglGetProcAddress2)
    {
      wglGetProcAddress2 = (PFN_wglGetProcAddress)GetProcAddress(hModule, "wglGetProcAddress");
    }

    if (!wglGetProcAddress2)
    {
      (*pProc) = (PROC)GetProcAddress(hModule, pszProc);
    }
    else
    {
      (*pProc) = wglGetProcAddress2(pszProc);

      if (!(*pProc))
      {
        (*pProc) = (PROC)GetProcAddress(hModule, pszProc);
      }
    }
}

BOOL gdi_GetDisplayInfo(HMONITOR hMonitor, HDC hDC, LPRECT lprc, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(hDC);

    LPDISPLAYINFO   lpdi   = (LPDISPLAYINFO)lParam;
    LPMONITORINFO2  lpmi2  = (LPMONITORINFO2)&lpdi->monitors[lpdi->numMonitors];
    LPMONITORINFOEX lpmiex = (LPMONITORINFOEX)&lpmi2->monitorInfoEx;
    LPDEVMODE       lpdm   = (LPDEVMODE)&lpmi2->deviceMode;

    lpmiex->cbSize = (DWORD) sizeof(MONITORINFOEX);
    lpdm->dmSize = (WORD) sizeof(DEVMODE);

    GetMonitorInfo(hMonitor, (LPMONITORINFO)lpmiex);
    UnionRect(&lpdi->rc, &lpdi->rc, lprc);

  #ifdef UNICODE
    memcpy(lpmi2->deviceNameW, lpmiex->szDevice, sizeof(lpmiex->szDevice));
    lpmi2->deviceNameW[CCHDEVICENAME] = TEXT('\0');
    WideCharToMultiByte(CP_UTF8, 0, (LPCWCH)lpmiex->szDevice, CCHDEVICENAME, (LPSTR)lpmi2->deviceName, CCHDEVICENAME + 1, 0, NULL);
    EnumDisplaySettings(lpmi2->deviceNameW, ENUM_CURRENT_SETTINGS, &lpmi2->deviceMode);
  #else
    memcpy(lpmi2->deviceName, lpmiex->szDevice, sizeof(lpmiex->szDevice));
    lpmi2->deviceName[CCHDEVICENAME] = TEXT('\0');
    MultiByteToWideChar(CP_UTF8, 0, (LPCCH)lpmiex->szDevice, CCHDEVICENAME, (LPWSTR)lpmi2->deviceNameW, CCHDEVICENAME + 1);
    EnumDisplaySettings(lpmi2->deviceName, ENUM_CURRENT_SETTINGS, &lpmi2->deviceMode);
  #endif

    return (MAX_ENUM_MONITORS > lpdi->numMonitors++);
}

void wglInit(void)
{
    HWND hWnd = CreateWindow(
      (LPTSTR)wgl_RegisterClass(),
      TEXT("wglDummyWindow"),
      WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      CW_USEDEFAULT,
      NULL,
      NULL,
      GetModuleHandle(NULL),
      NULL);
    HDC hDC = GetDC(hWnd);

    if (!hWnd)
    {
      return;
    }

    if (!wgl_SelectPixelFormat(hDC))
    {
      return;
    }

    if (!wglMakeCurrent(hDC, wglCreateContext(hDC)))
    {
      return;
    }

    wgl_Load();
}

int wglFindPixelFormat(HDC hDC, const int* piAttribIList, const FLOAT* pfAttribFList)
{
    int format = 0;
    int pf = GetPixelFormat(hDC);

    while (0 <= pf--)
    {
      UINT num_formats;

      if (wglChoosePixelFormatARB(hDC, piAttribIList, pfAttribFList, 1, &format, &num_formats))
      {
        if (num_formats > 0)
        {
          break;
        }
      }
    }

    return format;
}

BOOL gdiGetDisplayInfo(LPDISPLAYINFO lpdi)
{
    return EnumDisplayMonitors(NULL, NULL, gdi_GetDisplayInfo, (LPARAM)lpdi);
}

int gdiCheckOcclusionStatus(HWND hwnd)
{
    HDC hdc;
    RECT rc, rcClient;
    int iType;

    hdc = GetDC(hwnd);
    iType = GetClipBox(hdc, &rc);

    ReleaseDC(hwnd, hdc);

    if (iType == NULLREGION)
        return OBS_COMPLETELYCOVERED;
    if (iType == COMPLEXREGION)
        return OBS_PARTIALLYVISIBLE;

    GetClientRect(hwnd, &rcClient);
    if (EqualRect(&rc, &rcClient))
        return OBS_COMPLETELYVISIBLE;

    return OBS_PARTIALLYVISIBLE;
}