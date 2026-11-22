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

bool g_isAppBoxActiveMode = false;
bool g_isTopMost = false;
bool g_keepTopAfterEsc = false;
ULONGLONG g_escTriggerTime = 0;

// 根據開始功能表狀態切換 Z-Order
void UpdateWindowZOrderForStartMenu(HWND foregroundHwnd)
{
    if (!g_hwnd || !::IsWindow(g_hwnd)) return;

    bool isStartMenu = IsStartMenuWindow(foregroundHwnd);

    if (isStartMenu)
    {
        g_keepTopAfterEsc = false; // 重置旗標
        g_isAppBoxActiveMode = false;
        if (!g_isTopMost)
        {
            g_isTopMost = true;
            ::SetWindowPos(g_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    else
    {
        // 🟢 檢查是否為剛剛點擊視窗發送 Esc 後（500ms 內）引發的焦點轉移
        bool isFromOurEsc = g_keepTopAfterEsc && ((::GetTickCount64() - g_escTriggerTime) < 500);
        g_keepTopAfterEsc = false; // 消耗掉這次保護機會（之後使用者再去點別的軟體就會正常沉底）

        bool clickedInsideAppBox = false;
        if (g_isTopMost || isFromOurEsc)
        {
            POINT ptCursor;
            RECT rcWin;
            if (::GetCursorPos(&ptCursor) && ::GetWindowRect(g_hwnd, &rcWin))
            {
                if (::PtInRect(&rcWin, ptCursor))
                {
                    clickedInsideAppBox = true;
                }
            }
        }

        // 確保移除 TOPMOST
        if (g_isTopMost)
        {
            g_isTopMost = false;
            ::SetWindowPos(g_hwnd, HWND_NOTOPMOST, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }

        if (clickedInsideAppBox || isFromOurEsc || foregroundHwnd == g_hwnd)
        {
            // 🟢 因為開始功能表關閉時系統把焦點還給了背後的視窗（如瀏覽器），
            // 這裡必須再次呼叫 SetWindowPos(HWND_TOP) 把我們的視窗壓在該視窗上面！
            g_isAppBoxActiveMode = true;
            ::SetWindowPos(g_hwnd, HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
        else
        {
            // 之後使用者再去點擊桌面或其他軟體時，因為 isFromOurEsc 已消耗完畢，就會順利沉底
            g_isAppBoxActiveMode = false;
            ::SetWindowPos(g_hwnd, HWND_BOTTOM, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
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
