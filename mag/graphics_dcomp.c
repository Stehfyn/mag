#include "graphics_dcomp.h"
#include "dcompabi.h"

#include <dwmapi.h>

#pragma comment(lib, "dcomp")
#pragma comment(lib, "dwmapi")

struct MAGDCOMPPRESENTER
{
  MAG_IDCompositionDevice*      device;
  MAG_IDCompositionTarget*      target;
  MAG_IDCompositionVisual*      visual;
  MAG_IDCompositionEffectGroup* effects;
};

static void magDCompPresenterRelease(MAGDCOMPPRESENTER* presenter)
{
    if (!presenter)
    {
      return;
    }
    if (presenter->effects)
    {
      presenter->effects->lpVtbl->Release(presenter->effects);
    }
    if (presenter->visual)
    {
      presenter->visual->lpVtbl->Release(presenter->visual);
    }
    if (presenter->target)
    {
      presenter->target->lpVtbl->Release(presenter->target);
    }
    if (presenter->device)
    {
      presenter->device->lpVtbl->Release(presenter->device);
    }
    HeapFree(GetProcessHeap(), 0, presenter);
}

static BOOL magDCompPresenterCreateWithContent(
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
    *presenterOut = NULL;

    presenter = (MAGDCOMPPRESENTER*)HeapAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      sizeof(*presenter));
    if (!presenter)
    {
      return FALSE;
    }

    hr = DCompositionCreateDevice(
      NULL,
      &IID_MAG_IDCompositionDevice,
      (void**)&presenter->device);
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->CreateTargetForHwnd(
        presenter->device,
        hWnd,
        TRUE,
        &presenter->target);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->CreateVisual(
        presenter->device,
        &presenter->visual);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->visual->lpVtbl->SetContent(presenter->visual, content);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->target->lpVtbl->SetRoot(
        presenter->target,
        presenter->visual);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->Commit(presenter->device);
    }
    if (FAILED(hr))
    {
      magDCompPresenterRelease(presenter);
      SetLastError((DWORD)hr);
      return FALSE;
    }

    *presenterOut = presenter;
    return TRUE;
}

BOOL magDCompPresenterCreate(
  HWND hWnd,
  IUnknown* content,
  MAGDCOMPPRESENTER** presenterOut)
{
    return magDCompPresenterCreateWithContent(hWnd, content, presenterOut);
}

BOOL magDCompPresenterCreateFromSurfaceHandle(
  HWND hWnd,
  HANDLE compositionSurfaceHandle,
  MAGDCOMPPRESENTER** presenterOut)
{
    MAGDCOMPPRESENTER* presenter;
    IUnknown* content = NULL;
    HRESULT hr;

    if (!hWnd || !compositionSurfaceHandle || !presenterOut)
    {
      return FALSE;
    }

    *presenterOut = NULL;
    presenter = (MAGDCOMPPRESENTER*)HeapAlloc(
      GetProcessHeap(),
      HEAP_ZERO_MEMORY,
      sizeof(*presenter));
    if (!presenter)
    {
      return FALSE;
    }

    hr = DCompositionCreateDevice(
      NULL,
      &IID_MAG_IDCompositionDevice,
      (void**)&presenter->device);
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->CreateSurfaceFromHandle(
        presenter->device,
        compositionSurfaceHandle,
        &content);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->CreateTargetForHwnd(
        presenter->device,
        hWnd,
        TRUE,
        &presenter->target);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->CreateVisual(
        presenter->device,
        &presenter->visual);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->visual->lpVtbl->SetContent(presenter->visual, content);
    }
    if (content)
    {
      content->lpVtbl->Release(content);
      content = NULL;
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->target->lpVtbl->SetRoot(
        presenter->target,
        presenter->visual);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->Commit(presenter->device);
    }
    if (FAILED(hr))
    {
      magDCompPresenterRelease(presenter);
      SetLastError((DWORD)hr);
      return FALSE;
    }
    *presenterOut = presenter;
    return TRUE;
}

void magDCompPresenterDestroy(MAGDCOMPPRESENTER* presenter)
{
    if (!presenter)
    {
      return;
    }

    if (presenter->visual)
    {
      presenter->visual->lpVtbl->SetContent(presenter->visual, NULL);
    }
    if (presenter->target)
    {
      presenter->target->lpVtbl->SetRoot(presenter->target, NULL);
    }
    if (presenter->device)
    {
      presenter->device->lpVtbl->Commit(presenter->device);
      DwmFlush();
    }
    magDCompPresenterRelease(presenter);
}

BOOL magDCompPresenterSetEnabled(
  MAGDCOMPPRESENTER* presenter,
  BOOL enabled)
{
    HRESULT hr;

    if (!presenter || !presenter->target || !presenter->device)
    {
      return FALSE;
    }

    hr = presenter->target->lpVtbl->SetRoot(
      presenter->target,
      enabled ? presenter->visual : NULL);
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->Commit(presenter->device);
    }
    return SUCCEEDED(hr);
}

BOOL magDCompPresenterSetOpacity(
  MAGDCOMPPRESENTER* presenter,
  FLOAT opacity)
{
    HRESULT hr = S_OK;

    if (!presenter || !presenter->visual || !presenter->device ||
        opacity < 0.0f || opacity > 1.0f)
    {
      return FALSE;
    }

    if (!presenter->effects)
    {
      hr = presenter->device->lpVtbl->CreateEffectGroup(
        presenter->device,
        &presenter->effects);
      if (SUCCEEDED(hr))
      {
        hr = presenter->visual->lpVtbl->SetEffect(
          presenter->visual,
          presenter->effects);
      }
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->effects->lpVtbl->SetOpacityValue(presenter->effects, opacity);
    }
    if (SUCCEEDED(hr))
    {
      hr = presenter->device->lpVtbl->Commit(presenter->device);
    }
    return SUCCEEDED(hr);
}

BOOL magDCompPresenterGetNextEstimatedFrameTime(
  MAGDCOMPPRESENTER* presenter,
  LONGLONG* frameTime)
{
    MAG_DCOMPOSITION_FRAME_STATISTICS statistics;
    HRESULT hr;

    ZeroMemory(&statistics, sizeof(statistics));
    if (!presenter || !presenter->device || !frameTime)
    {
      return FALSE;
    }
    hr = presenter->device->lpVtbl->GetFrameStatistics(
      presenter->device,
      &statistics);
    if (SUCCEEDED(hr))
    {
      *frameTime = statistics.nextEstimatedFrameTime.QuadPart;
    }
    return SUCCEEDED(hr);
}
