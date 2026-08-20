#define NOMINMAX

#include "mag/dwmprivate.h"

#include <dwmapi.h>
#include <stdio.h>

static LRESULT CALLBACK SmokeWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  if (WM_NCCREATE == message)
  {
    const CREATESTRUCT* create = reinterpret_cast<const CREATESTRUCT*>(lParam);
    SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
  }

  if (WM_ERASEBKGND == message)
  {
    return 1;
  }

  if (WM_PAINT == message)
  {
    PAINTSTRUCT paint = {};
    RECT client = {};
    HDC dc = BeginPaint(hWnd, &paint);
    HBRUSH brush = CreateSolidBrush(static_cast<COLORREF>(GetWindowLongPtr(hWnd, GWLP_USERDATA)));
    GetClientRect(hWnd, &client);
    FillRect(dc, &client, brush);
    DeleteObject(brush);
    EndPaint(hWnd, &paint);
    return 0;
  }

  return DefWindowProc(hWnd, message, wParam, lParam);
}

static void PumpFor(DWORD milliseconds)
{
  const DWORD end = GetTickCount() + milliseconds;
  MSG message = {};

  do
  {
    while (PeekMessage(&message, NULL, 0, 0, PM_REMOVE))
    {
      TranslateMessage(&message);
      DispatchMessage(&message);
    }
    Sleep(1);
  } while (static_cast<LONG>(end - GetTickCount()) > 0);
}

static RECT ClientScreenRect(HWND hWnd)
{
  RECT rect = {};
  POINT origin = {};
  GetClientRect(hWnd, &rect);
  ClientToScreen(hWnd, &origin);
  OffsetRect(&rect, origin.x, origin.y);
  return rect;
}

int main()
{
  HINSTANCE instance = GetModuleHandle(NULL);
  WNDCLASS windowClass = {};
  RECT workArea = {};
  RECT desktop = {};
  RECT sourceRed = {};
  RECT sourceGreen = {};
  RECT destination = { 0, 0, 220, 160 };
  SIZE targetSize = { 220, 160 };
  DWMPRIVATECAPTURESTATE* state = NULL;
  COLORREF redSample = CLR_INVALID;
  COLORREF greenSample = CLR_INVALID;
  HWND redWindow;
  HWND greenWindow;
  HWND hostWindow;

  windowClass.lpfnWndProc = SmokeWndProc;
  windowClass.hInstance = instance;
  windowClass.lpszClassName = TEXT("DwmPrivateTransformSmoke");
  RegisterClass(&windowClass);
  SystemParametersInfo(SPI_GETWORKAREA, 0, &workArea, 0);

  redWindow = CreateWindow(
    windowClass.lpszClassName,
    TEXT("red"),
    WS_POPUP | WS_VISIBLE,
    workArea.left + 20,
    workArea.top + 20,
    220,
    160,
    NULL,
    NULL,
    instance,
    reinterpret_cast<void*>(RGB(240, 10, 10)));
  greenWindow = CreateWindow(
    windowClass.lpszClassName,
    TEXT("green"),
    WS_POPUP | WS_VISIBLE,
    workArea.left + 260,
    workArea.top + 20,
    220,
    160,
    NULL,
    NULL,
    instance,
    reinterpret_cast<void*>(RGB(10, 240, 10)));
  hostWindow = CreateWindow(
    windowClass.lpszClassName,
    TEXT("host"),
    WS_POPUP | WS_VISIBLE,
    workArea.left + 500,
    workArea.top + 20,
    220,
    160,
    NULL,
    NULL,
    instance,
    reinterpret_cast<void*>(RGB(0, 0, 0)));

  if (!redWindow || !greenWindow || !hostWindow)
  {
    return 1;
  }

  desktop.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  desktop.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  desktop.right = desktop.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
  desktop.bottom = desktop.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);
  sourceRed = ClientScreenRect(redWindow);
  sourceGreen = ClientScreenRect(greenWindow);
  PumpFor(100);
  DwmFlush();

  if (!DwmPrivateCaptureCreate(hostWindow, &state) ||
      !DwmPrivateCaptureUpdate(state, &desktop, &sourceRed, &destination, targetSize, NULL, 0))
  {
    DwmPrivateCaptureDestroy(state);
    return 2;
  }

  PumpFor(150);
  {
    HDC desktopDc = GetDC(NULL);
    RECT host = ClientScreenRect(hostWindow);
    redSample = GetPixel(desktopDc, (host.left + host.right) / 2, (host.top + host.bottom) / 2);
    ReleaseDC(NULL, desktopDc);
  }

  if (!DwmPrivateCaptureUpdate(state, &desktop, &sourceGreen, &destination, targetSize, NULL, 0))
  {
    DwmPrivateCaptureDestroy(state);
    return 3;
  }

  PumpFor(150);
  {
    HDC desktopDc = GetDC(NULL);
    RECT host = ClientScreenRect(hostWindow);
    greenSample = GetPixel(desktopDc, (host.left + host.right) / 2, (host.top + host.bottom) / 2);
    ReleaseDC(NULL, desktopDc);
  }

  DwmPrivateCaptureDestroy(state);
  DestroyWindow(hostWindow);
  DestroyWindow(greenWindow);
  DestroyWindow(redWindow);

  printf(
    "red=%u,%u,%u green=%u,%u,%u\n",
    GetRValue(redSample),
    GetGValue(redSample),
    GetBValue(redSample),
    GetRValue(greenSample),
    GetGValue(greenSample),
    GetBValue(greenSample));

  return GetRValue(redSample) > GetGValue(redSample) &&
         GetGValue(greenSample) > GetRValue(greenSample)
    ? 0
    : 4;
}
