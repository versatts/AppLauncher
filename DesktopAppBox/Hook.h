#pragma once
#include <windows.h>
class Hook
{
};

extern bool IsStartMenuWindow(HWND hwnd);
extern void UpdateWindowZOrderForStartMenu(HWND foregroundHwnd);
extern void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

