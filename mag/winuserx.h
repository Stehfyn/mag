#pragma once

#include "framework.h"

BOOL SetWindowAlwaysOnTop(HWND hWnd, BOOL fEnable);

BOOL SetWindowOwner(HWND hWnd, HWND hWndOwner);

BOOL SetCurrentProcessEfficiencyQoS(void);

BOOL InMenu(void);

HMENU LoadPopupMenu(HINSTANCE hInstance, LPCTSTR lpMenuName);

void ForceTimerMessagesToBeCreatedIfNecessary(LPMSG lpMsg);

HANDLE
UnloadFile(
    LPTSTR  lpszName,
    LPCVOID lpBuffer,
    DWORD   nNumberOfBytesToWrite);

HANDLE
UnloadResource(
    HMODULE hModule, 
    UINT    nResourceId, 
    LPTSTR  lpszName);