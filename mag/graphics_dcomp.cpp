#include "graphics_dcomp.h"

#include <dcomp.h>
#include <dwmapi.h>
#include <new>
#include <wrl/client.h>

#pragma comment(lib, "dcomp")
#pragma comment(lib, "dwmapi")

using Microsoft::WRL::ComPtr;

struct MAGDCOMPPRESENTER
{
  ComPtr<IDCompositionDevice> device;
  ComPtr<IDCompositionTarget> target;
  ComPtr<IDCompositionVisual> visual;
};

extern "C" BOOL magDCompPresenterCreate(
  HWND hWnd,
  IUnknown* content,
  MAGDCOMPPRESENTER** presenterOut)
{
    MAGDCOMPPRESENTER* presenter;
    HRESULT hr;

    if (!hWnd || !content || !presenterOut)
    {
      return FALSE;
    }
    *presenterOut = nullptr;

    presenter = new (std::nothrow) MAGDCOMPPRESENTER;
    if (!presenter)
    {
      return FALSE;
    }

    hr = DCompositionCreateDevice(
      nullptr,
      IID_PPV_ARGS(&presenter->device));
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->CreateTargetForHwnd(
        hWnd,
        TRUE,
        &presenter->target);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->CreateVisual(&presenter->visual);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->visual->SetContent(content);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->target->SetRoot(presenter->visual.Get());
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->Commit();
    }
    if (FAILED(hr))
    {
      delete presenter;
      SetLastError((DWORD)hr);
      return FALSE;
    }

    *presenterOut = presenter;
    return TRUE;
}

extern "C" void magDCompPresenterDestroy(MAGDCOMPPRESENTER* presenter)
{
    if (!presenter)
    {
      return;
    }

    if (presenter->visual)
    {
      presenter->visual->SetContent(nullptr);
    }
    if (presenter->target)
    {
      presenter->target->SetRoot(nullptr);
    }
    if (presenter->device)
    {
      presenter->device->Commit();
      DwmFlush();
    }
    delete presenter;
}

extern "C" BOOL magDCompPresenterSetEnabled(
  MAGDCOMPPRESENTER* presenter,
  BOOL enabled)
{
    if (!presenter || !presenter->target || !presenter->device)
    {
      return FALSE;
    }

    HRESULT hr = presenter->target->SetRoot(
      enabled ? presenter->visual.Get() : nullptr);
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->Commit();
    }
    return SUCCEEDED(hr);
}
