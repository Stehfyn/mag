#include "d3dkmtx.h"

typedef LONG NTSTATUS;
#include <d3dkmthk.h>

BOOL D3DKMTWaitForVerticalBlank(HWND hWnd)
{
    NTSTATUS status;
    static D3DKMT_WAITFORVERTICALBLANKEVENT vbe = { 0 };

    if (!vbe.hAdapter)
    {
      D3DKMT_OPENADAPTERFROMHDC oa = { 0 };
      oa.hDc = GetDC(hWnd);

      if (!oa.hDc)
      {
        return FALSE;
      }

      status = D3DKMTOpenAdapterFromHdc(&oa);

      ReleaseDC(hWnd, oa.hDc);
    
      if (0 != status)
      {
        return FALSE;
      }

      vbe.hAdapter      = oa.hAdapter;
      vbe.VidPnSourceId = oa.VidPnSourceId;
      vbe.hDevice       = 0;
    }

    status = D3DKMTWaitForVerticalBlankEvent(&vbe);
    if (0 != status)
    {
      return FALSE;
    }

    {
      D3DKMT_GETSCANLINE gsl = { 0 };

      gsl.hAdapter = vbe.hAdapter;
      gsl.VidPnSourceId = vbe.VidPnSourceId;

      do
      {
        status = D3DKMTGetScanLine(&gsl);
      }
      while ((0 != status) || gsl.InVerticalBlank);
    }

    return TRUE;
}
