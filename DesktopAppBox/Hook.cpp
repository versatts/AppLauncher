#include "Hook.h"

#include <psapi.h>
#pragma comment(lib, "psapi.lib")

extern HWND g_hwnd;
// 判斷指定的視窗是否屬於 Windows 開始功能表
bool IsStartMenuWindow(HWND hwnd)
{
    if (!hwnd) return false;

    DWORD pid = 0;
    ::GetWindowThreadProcessId(hwnd, &pid);
    if (pid == 0) return false;

    HANDLE hProcess = ::OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!hProcess) return false;

    wchar_t exePath[MAX_PATH] = { 0 };
    DWORD size = MAX_PATH;
    bool isStartMenu = false;

    if (::QueryFullProcessImageNameW(hProcess, 0, exePath, &size))
    {
        // 提取檔名
        const wchar_t* fileName = wcsrchr(exePath, L'\\');
        fileName = fileName ? (fileName + 1) : exePath;

        // Win10/11 開始功能表進程名稱
        if (_wcsicmp(fileName, L"StartMenuExperienceHost.exe") == 0 ||
            _wcsicmp(fileName, L"SearchHost.exe") == 0) // 若希望按 Win 鍵後直接打字搜尋時也保持置頂，可加上此項
        {
            isStartMenu = true;
        }
    }
    ::CloseHandle(hProcess);
    return isStartMenu;
}

// 根據開始功能表狀態切換 Z-Order
void UpdateWindowZOrderForStartMenu(HWND foregroundHwnd)
{
    if (!g_hwnd || !::IsWindow(g_hwnd)) return;

    static bool s_isTopMost = false;
    bool shouldBeTop = IsStartMenuWindow(foregroundHwnd);

    // 如果目前正在點擊操作您自己的視窗（例如拖曳或點擊圖示），先不要把它沉下去
    if (!shouldBeTop && foregroundHwnd == g_hwnd)
    {
        return;
    }

    if (shouldBeTop && !s_isTopMost)
    {
        s_isTopMost = true;
        ::SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
    else if (!shouldBeTop && s_isTopMost)
    {
        s_isTopMost = false;
        // 必須先取消 TOPMOST，才能真正沉到 HWND_BOTTOM
        ::SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        ::SetWindowPos(g_hwnd, HWND_BOTTOM, 0, 0, 0, 0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

// WinEvent 回呼函數：當系統前景視窗改變時自動觸發
void CALLBACK WinEventProc(HWINEVENTHOOK hWinEventHook, DWORD event, HWND hwnd,
    LONG idObject, LONG idChild, DWORD dwEventThread, DWORD dwmsEventTime)
{
    if (event == EVENT_SYSTEM_FOREGROUND)
    {
        UpdateWindowZOrderForStartMenu(hwnd);
    }
}
