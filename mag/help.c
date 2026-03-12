#include "help.h"

#pragma comment(lib, "htmlhelp")
#pragma comment(lib, "Shlwapi")

static TCHAR s_szChmPath[MAX_PATH + 1];

void help_Show(HWND hWnd)
{
    if (!s_szChmPath[0])
    {
        TCHAR szImageDir[MAX_PATH + 1];

        if (!GetModuleFileName(NULL, szImageDir, MAX_PATH + 1) ||
            !PathRemoveFileSpec(szImageDir)                     ||
            !PathCombine(s_szChmPath, szImageDir, TEXT("mag.chm")))
        {
            s_szChmPath[0] = TEXT('\0');
            return;
        }
    }

    {
        HANDLE hFile = UnloadResource(GetModuleHandle(NULL), IDR_CHM, s_szChmPath);

        if (hFile && INVALID_HANDLE_VALUE != hFile)
            CloseHandle(hFile);
    }

    SetWindowOwner(HtmlHelp(hWnd, s_szChmPath, HH_HELP_FINDER, NULL), GetDesktopWindow());
}

void help_Cleanup(void)
{
    if (s_szChmPath[0])
    {
        HtmlHelp(NULL, NULL, HH_CLOSE_ALL, 0);
        DeleteFile(s_szChmPath);
        s_szChmPath[0] = TEXT('\0');
    }
}
