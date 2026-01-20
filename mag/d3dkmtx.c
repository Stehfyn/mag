#include "d3dkmtx.h"

typedef LONG NTSTATUS;
#include <d3dkmthk.h>

BOOL D3DKMTWaitForVerticalBlank(HWND hWnd)
{
    NTSTATUS status;
  
    static D3DKMT_WAITFORVERTICALBLANKEVENT vbe = INIT_ONCE_STATIC_INIT;

    if (!vbe.hAdapter)
    {
      D3DKMT_OPENADAPTERFROMHDC oa;
      oa.hDc = GetDC(hWnd);

      status = D3DKMTOpenAdapterFromHdc(&oa);

      ReleaseDC(hWnd, oa.hDc);
    
      if (0 != status)
      {
        return FALSE;
      }

      vbe.hAdapter      = oa.hAdapter;
      vbe.VidPnSourceId = oa.VidPnSourceId;
      vbe.hDevice       = 0;

      return TRUE;
    }
    else
    {
      D3DKMT_GETSCANLINE gsl;
    
      status = D3DKMTWaitForVerticalBlankEvent(&vbe);
    
      if (0 != status)
      {
        return FALSE;
      }

      gsl.hAdapter        = vbe.hAdapter;
      gsl.VidPnSourceId   = vbe.VidPnSourceId;
      gsl.ScanLine        = 0;
      gsl.InVerticalBlank = 0;

      do
      {
        status = D3DKMTGetScanLine(&gsl);

      } while ((0 != status) || (gsl.InVerticalBlank));

      return TRUE;
    }
}
