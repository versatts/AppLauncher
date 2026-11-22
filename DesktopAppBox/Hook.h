#pragma once
#include <windows.h>
class Hook
{
};

extern bool IsStartMenuWindow(HWND hwnd);
extern void UpdateWindowZOrderForStartMenu(HWND foregroundHwnd);
extern void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime);

extern bool g_isAppBoxActiveMode; // true = 已被點擊喚醒，保持在一般視窗最上層(HWND_TOP)，不沉底
extern bool g_isTopMost;          // 記錄目前是否處於 TOPMOST 狀態

extern bool g_keepTopAfterEsc;
extern ULONGLONG g_escTriggerTime;
