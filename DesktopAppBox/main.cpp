// Dear ImGui: standalone example application for Windows API + DirectX 11

#include "ImGuiObj.h"
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cmath>    // std::floor
#include <string>
#include <cstdio>   // std::snprintf
#include <climits>  // INT_MIN

#include <shellapi.h>   // Explorer 檔案拖放 (WM_DROPFILES / DragQueryFileW)
#pragma comment(lib, "shell32.lib")
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#include "AppConfig.h"
#include "Application.h"
#include "link.h"
#include "ResLoader.h"
#include "TabHeadUI.h"
#include "FolderUIData.h"
#include "FolderUI.h"
#include "ImGuiObj.h"
// 內部結構宣告，用於折疊時同步修改 ImGui 快取
#include <imgui_internal.h>

#include "Hook.h"
// 在全域宣告一組變數用來保存最後一刻的視窗幾何資訊
static int g_FinalWinX = 0, g_FinalWinY = 0, g_FinalWinW = 0, g_FinalWinH = 0;
static bool g_HasFinalGeometry = false;
static bool g_bStart = false;

AppConfig gCfg;
Application theApp;
// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void RenderOneFrame();

FolderUIData gFUD;
FolderUI gFU;

static int g_pendingReloadIdx = -1;

TabHeadUI gTabUI;
ResLoader gRes;

// 全局幾何變數
static int g_OSWinH = 0;          // 折疊前的"完整" ImGui 高度，供恢復用
bool isCollapsed = false;
HWND g_hwnd = nullptr;      // Win32 Host 窗口 HWND

static constexpr int DEFAULT_IMGUI_W = 600;
static constexpr int DEFAULT_IMGUI_H = 400;

void ShowCloseConfDlg(bool& show_close_confirmation)
{
    // 1. 確保 OpenPopup 與 BeginPopupModal 使用完全相同的識別碼 "ExitConfirmation"
    static std::string sTitle = u8"提示";
    if (show_close_confirmation)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 center = ImVec2(
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f
        );
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::OpenPopup(sTitle.c_str());
        show_close_confirmation = false; // 觸發後立即重設
    }

    // 2. 這裡的識別碼必須與 OpenPopup 一致
    if (ImGui::BeginPopupModal(sTitle.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("请确认操作");
        ImGui::Separator();

        float button_width = ImGui::GetFontSize() * 5.0f;

        if (ImGui::Button("退出", ImVec2(button_width, 0)))
        {
            ImGui::CloseCurrentPopup();
            ::PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        }

        //ImGui::SameLine();
        //if (ImGui::Button("重启", ImVec2(button_width, 0))) // 統一為繁體
        //{
        //    ImGui::CloseCurrentPopup();
        //    ::PostMessage(g_hwnd, WM_CLOSE, 1, 0); // WndProc 中需處理 wParam == 1 的邏輯
        //}

        ImGui::SameLine();
        if (ImGui::Button("取消", ImVec2(button_width, 0)))
        {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;

    // 🟢 讓 ImGui 啟用原生自動存檔載入機制
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui-001", nullptr };
    ::RegisterClassExW(&wc);

    // 在 CreateWindowExW 之前，讀取上次儲存的視窗位置與大小
    int winX = 100, winY = 100;
    int w = (int)(DEFAULT_IMGUI_W * main_scale);
    int h = (int)(DEFAULT_IMGUI_H * main_scale);

    if (FILE* f = nullptr; fopen_s(&f, "imgui.ini", "r") == 0 && f)
    {
        char line[256];
        bool inAppBoxSection = false;
        while (fgets(line, sizeof(line), f))
        {
            if (strncmp(line, "[Window][###-AppBox-]", 21) == 0 || strncmp(line, "[Window][-AppBox-]", 18) == 0)
                inAppBoxSection = true;
            else if (line[0] == '[')
                inAppBoxSection = false;
            else if (inAppBoxSection)
            {
                sscanf_s(line, "Pos=%d,%d", &winX, &winY);
                sscanf_s(line, "Size=%d,%d", &w, &h);
            }
        }
        fclose(f);
    }
    g_OSWinH = h;

    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOOLWINDOW/* | WS_EX_NOACTIVATE*/,//这个NOACTIVATE有什么用？
        wc.lpszClassName,
        L"-AppBox-",
        WS_POPUP/* | WS_CLIPCHILDREN*/,
        winX, winY, w, h,
        nullptr, nullptr, wc.hInstance, nullptr
    );
    g_hwnd = hwnd;

    // 🚀 【核心修正：開啟 Win32 視窗系統級透明】
    // 將邊設為 -1，代表將透明區域擴展到整個視窗的客戶區（Sheet of Glass 效果）45
    MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);
#if 0
    //窗口阴影
    DWMNCRENDERINGPOLICY policy = DWMNCRP_ENABLED;
    ::DwmSetWindowAttribute(hwnd, DWMWA_NCRENDERING_POLICY, &policy, sizeof(policy));
#endif
#if 1
    //显示桌面时不会被隐藏
    if (HWND hProgman = ::FindWindowW(L"Progman", L"Program Manager"))
    {
        ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
    }
#endif

    if (!theApp.CreateDeviceD3D(hwnd))
    {
        theApp.CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    g_pd3dDevice = theApp.g_pd3dDevice;
    g_pd3dDeviceContext = theApp.g_pd3dDeviceContext;
    g_pSwapChain = theApp.g_pSwapChain;
    g_SwapChainOccluded = theApp.g_SwapChainOccluded;

    gRes.m_pDevice = g_pd3dDevice;
#if 1
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\shell32.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\imageres.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\ddores.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\wmploc.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\mmcndmgr.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\netshell.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\setupapi.dll", 64);
    gRes.LoadAllIconsFromDll(gRes.m_pDevice, L"C:\\Windows\\System32\\compstui.dll", 64);
#endif
    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);


    ::DragAcceptFiles(hwnd, gCfg.m_bEnableFileDragIO ? TRUE : FALSE);

    theApp.InitImGuiContext(main_scale);

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    gFUD.Init(g_pd3dDevice, &gRes);

    gTabUI.m_Idx = 0;
    if (gFUD.gvFolder.size())
    {
        gFUD.m_pIdx = &gTabUI.m_Idx;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 clear_color = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);


    // 註冊全域系統前景視窗變化監聽 (不需要 DLL 注入，純進程內訊息迴圈接收)
    HWINEVENTHOOK hEventHook = ::SetWinEventHook(
        EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
        nullptr, WinEventProc, 0, 0,
        WINEVENT_OUTOFCONTEXT | WINEVENT_SKIPOWNPROCESS
    );

    // 程式啟動時先沉到最底層
    ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);


    bool done = false;
    while (!done)
    {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT)
                done = true;
        }
        if (done)
            break;

        RenderOneFrame();
    }

    // 迴圈結束後釋放鉤子
    if (hEventHook)
    {
        ::UnhookWinEvent(hEventHook);
    }

    gFUD.PersistOrder();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    theApp.CleanupDeviceD3D();
    ::DestroyWindow(hwnd);

    // 2. 🎯 【終極覆蓋方案】在 ImGui 寫完 ini 後，我們手動在檔案末尾追加正確的絕對座標！
    if (g_HasFinalGeometry)
    {
        // 將 io.IniFilename 轉換為可讀的字串路徑
        std::string iniPath = "imgui.ini";

        // 以「追加模式 (Append)」打開檔案
        FILE* f = nullptr;
        fopen_s(&f, iniPath.c_str(), "ab"); // "ab" 代表以二進位追加，防止換行符號問題
        if (f)
        {
            // 直接寫入標準的 ImGui 視窗設定格式
            // 注意：底部的設定會直接覆蓋掉 ImGui 在上面自動生成的 0,0 錯誤數據！
            fprintf(f, "\n[Window][-AppBox-]\n");//为何这里要加###，但自动生成的这一项是不带###的。而且在启动后还会把这里存的位置给应用上？
            fprintf(f, "Pos=%d,%d\n", g_FinalWinX, g_FinalWinY);
            fprintf(f, "Size=%d,%d\n", g_FinalWinW, g_FinalWinH);
            fprintf(f, "Collapsed=%d\n", isCollapsed ? 1 : 0);

            gCfg.Save(f);

            fclose(f);
        }
    }

    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    if (g_bStart)
    {
        // 1. 自動獲取當前 EXE 的完整絕對路徑
        wchar_t szExePath[MAX_PATH];
        ::GetModuleFileNameW(nullptr, szExePath, MAX_PATH);

        // 2. 啟動新的 EXE 行程
        // 使用 ShellExecuteW 是最簡單且安全的方式
        ::ShellExecuteW(nullptr, L"open", szExePath, nullptr, nullptr, SW_SHOW);
    }
    return 0;
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void AppRemoveDragOutItem(const std::wstring& lnkPath)
{
    if (lnkPath.empty()) return;
    ::DeleteFileW(lnkPath.c_str());
    size_t slash = lnkPath.find_last_of(L"/");
    size_t dot = lnkPath.find_last_of(L'.');
    if (slash != std::wstring::npos && dot != std::wstring::npos && dot > slash)
    {
        std::wstring stem = lnkPath.substr(slash + 1, dot - slash - 1);
        std::wstring dir = lnkPath.substr(0, slash);
        const wchar_t* iconExts[] = { L".png", L".ico" };
        for (const wchar_t* e : iconExts)
        {
            std::wstring icon = dir + L"\\" + stem + e;
                DWORD attr = ::GetFileAttributesW(icon.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                ::DeleteFileW(icon.c_str());
            }
        }
    }
    g_pendingReloadIdx = gTabUI.m_Idx;
}

static void HandleDropFiles(HDROP hDrop)
{
    // 總開關關閉時直接忽略
    if (!gCfg.m_bEnableFileDragIO)
    {
        ::DragFinish(hDrop);
        return;
    }

    UINT fileCount = ::DragQueryFileW(hDrop, 0xFFFFFFFF, nullptr, 0);
    if (fileCount == 0)
    {
        ::DragFinish(hDrop);
        return;
    }

    // 當前分頁對應的目標資料夾（分頁 0 = lnk 根目錄，其他 = lnk\<分頁名>）
    int curIdx = gTabUI.m_Idx;
    std::wstring subDir;
    if (curIdx > 0 && curIdx < (int)gFUD.m_vTabName.size())
    {
        subDir = gFUD.m_vTabName[curIdx];
    }
    std::wstring targetDir = GetLnkDirForFolder(subDir);
    if (targetDir.empty())
    {
        ::DragFinish(hDrop);
        return;
    }
    ::CreateDirectoryW(targetDir.c_str(), nullptr); // 保證目錄存在

    // 🚀 拖入的新檔案排在最後
    int nextPrefix = CountLnkUrlFilesInDir(targetDir);

    bool copiedAny = false;
    for (UINT i = 0; i < fileCount; ++i)
    {
        UINT len = ::DragQueryFileW(hDrop, i, nullptr, 0);
        if (len == 0) continue;

        // 🟢 🎯 核心修正：將 &srcPath 改為 &srcPath[0]，將字元緩衝區的第一個指標傳給 Windows API
        std::wstring srcPath(len + 1, L'\0');
        if (::DragQueryFileW(hDrop, i, &srcPath[0], len + 1) == 0) continue;
        srcPath.resize(len);

        if (srcPath.size() < 4) continue;
        const wchar_t* ext = srcPath.c_str() + srcPath.size() - 4;

        if (_wcsicmp(ext, L".lnk") == 0 || _wcsicmp(ext, L".url") == 0)
        {
            size_t lastSlash = srcPath.find_last_of(L"\\/");
            std::wstring fileName = (lastSlash == std::wstring::npos) ? srcPath : srcPath.substr(lastSlash + 1);

            WCHAR prefix[8];
            swprintf_s(prefix, L"%04d-", nextPrefix);
            std::wstring dstPath = targetDir + L"\\" + prefix + fileName;

            if (::CopyFileW(srcPath.c_str(), dstPath.c_str(), FALSE))
            {
                copiedAny = true;
                ++nextPrefix;
            }
        }
        else if (_wcsicmp(ext, L".ico") == 0 || _wcsicmp(ext, L".png") == 0)
        {
            POINT pt;
            if (::DragQueryPoint(hDrop, &pt))
            {
                // 將 Win32 Client 座標轉換為全域 Screen 座標，與 ImGui::GetItemRectMin() 的口徑完全一致
                ::ClientToScreen(g_hwnd, &pt);
                if (gFU.ApplyIconToItem(pt, srcPath))
                {
                    copiedAny = true;
                }
            }
        }
    }
    ::DragFinish(hDrop);

    // 如果有複製任何檔案，直接觸發當前分頁重載
    if (copiedAny)
    {
        gFUD.ReloadFolder(g_pd3dDevice, &gRes, curIdx);
    }
}
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // 🟢 直接在所有滑鼠按下訊息（客戶區左/右鍵、標題列非客戶區）最前端攔截
    if (msg == WM_MOUSEACTIVATE ||
        msg == WM_LBUTTONDOWN ||
        msg == WM_RBUTTONDOWN ||
        msg == WM_NCLBUTTONDOWN)
    {
        if (g_isTopMost)
        {
            // 🟢 標記：接下來觸發的開始功能表關閉事件是由點擊本視窗引起的，不要沉底！
            g_keepTopAfterEsc = true;
            g_escTriggerTime = ::GetTickCount64();

            g_isTopMost = false;
            g_isAppBoxActiveMode = true;

            // 先移除 TOPMOST 並暫時放在一般視窗頂端
            ::SetWindowPos(g_hwnd, HWND_TOP, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

            // 發送 Esc 禮貌關閉開始功能表
            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wVk = VK_ESCAPE;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wVk = VK_ESCAPE;
            inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
            ::SendInput(2, inputs, sizeof(INPUT));
        }

        if (msg == WM_MOUSEACTIVATE)
            return MA_NOACTIVATE;
    }

    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
    case WM_NCHITTEST:
    {
        POINT screenPt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        RECT rcWin;
        ::GetWindowRect(hWnd, &rcWin);

        // 1. 先判斷視窗四邊與四角的縮放感應區（6 像素寬）
        const int border = 6;
        bool onLeft = (screenPt.x >= rcWin.left && screenPt.x < rcWin.left + border);
        bool onRight = (screenPt.x <= rcWin.right && screenPt.x > rcWin.right - border);
        bool onTop = (screenPt.y >= rcWin.top && screenPt.y < rcWin.top + border);
        bool onBottom = (screenPt.y <= rcWin.bottom && screenPt.y > rcWin.bottom - border);

        if (onTop && onLeft)     return HTTOPLEFT;
        if (onTop && onRight)    return HTTOPRIGHT;
        if (onBottom && onLeft)  return HTBOTTOMLEFT;
        if (onBottom && onRight) return HTBOTTOMRIGHT;
        if (onTop)               return HTTOP;
        if (onBottom)            return HTBOTTOM;
        if (onLeft)              return HTLEFT;
        if (onRight)             return HTRIGHT;

        // 2. 再判斷自訂標題列拖曳移動區域
        POINT clientPt = screenPt;
        ::ScreenToClient(hWnd, &clientPt);

        float titleHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
        int winWidth = rcWin.right - rcWin.left;

        // 標題列區域（扣除頂部縮放邊緣與右側自訂按鈕寬度）
        if (clientPt.y >= border && clientPt.y < (int)titleHeight &&
            clientPt.x >= border && clientPt.x < (winWidth - (int)titleHeight))
        {
            return HTCAPTION;
        }

        return HTCLIENT;
    }
    case WM_CREATE:
        break;
    case WM_CLOSE:
    {
        RECT rcWin;
        if (::GetWindowRect(hWnd, &rcWin))
        {
            g_FinalWinX = rcWin.left;
            g_FinalWinY = rcWin.top;
            g_FinalWinW = rcWin.right - rcWin.left;
            g_FinalWinH = rcWin.bottom - rcWin.top;
            g_HasFinalGeometry = true;
        }
        g_bStart = (1 == (int)wParam);
    }
    break;
    case WM_QUERYENDSESSION:
        // 1. 檢查是否有未儲存的工作，可以在此跳出提示（但不建議阻擋太久）
        // 2. 回傳 TRUE 代表同意關機
        ::SendMessage(g_hwnd, WM_CLOSE, 0, 0);
        return TRUE;

    case WM_ENDSESSION:
        // wParam 為 TRUE 代表系統確定要關機/登出
        if (wParam == TRUE)
        {
            // 執行您的正常退出清理邏輯
            // 儲存資料
            // 釋放資源

            // 告知系統我們處理完畢並結束程式
            PostQuitMessage(0);
        }
        return 0;

    case WM_SIZE:
    {
        if (wParam == SIZE_MINIMIZED)
            return 0;

        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);

        // 🟢 拖曳縮放期間立即重繪一幀，解決放開滑鼠才刷新的卡頓問題
        static bool s_isRenderingInSize = false;
        if (!s_isRenderingInSize && g_pd3dDevice != nullptr)
        {
            s_isRenderingInSize = true;
            RenderOneFrame();
            s_isRenderingInSize = false;
        }
        return 0;
    }
    case WM_DROPFILES:
        HandleDropFiles((HDROP)wParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU)
            return 0;
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}

void RenderOneFrame()
{
    if (!g_pd3dDevice || !g_pSwapChain)
        return;

    if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        return;
    g_SwapChainOccluded = false;

    // 處理視窗大小改變時的 SwapChain 重建
    if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
    {
        theApp.CleanupRenderTarget();
        g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
        g_ResizeWidth = g_ResizeHeight = 0; // 記得重置為 0，避免每幀重複重建！
        theApp.CreateRenderTarget();
        g_mainRenderTargetView = theApp.g_mainRenderTargetView;
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // ... [這裡放您原本整個 AppBox UI 繪製與 ShowCloseConfDlg 的程式碼] ...
    {
        static bool show_close_confirmation = false;

        ImGuiViewport* mvp = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(mvp->Size);

        ImGuiWindowFlags win_flags = ImGuiWindowFlags_NoDocking
            | ImGuiWindowFlags_NoTitleBar
            | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoResize
            | ImGuiWindowFlags_NoBringToFrontOnFocus;

        static float f = 0.0f;
        static int counter = 0;
        {
            auto now = std::chrono::system_clock::now();
            std::time_t time_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_now;
            localtime_s(&tm_now, &time_now);
            char timeBuffer[64];
            //std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d (%a) %H:%M:%S", &tm_now);
            static std::string arrWeek[7] = { "⑦", "①", "②", "③", "④", "⑤", "⑥" };
            //static std::string arrWeek[7] = {u8"周日", u8"周一", u8"周二", u8"周三", u8"周四", u8"周五", u8"周六"};
            //std::strftime(timeBuffer, sizeof(timeBuffer), "%H:%M:%S", &tm_now);
            //std::string timeStr = arrWeek[tm_now.tm_wday] + " - " + timeBuffer;
            std::string timeStr0 = arrWeek[tm_now.tm_wday] + " ";
            std::string timeStr1 = "00:00";
            std::string timeStrFull = timeStr0 + timeStr1;
            const char* sTimeSymbol[] = { "0", ":" };

            // 🎯 核心修正：如果確認關閉的彈窗正在顯示，將整個主視窗的元件狀態切換為「停用（Disabled）」
            // 這會讓 ImGui 自動在底層切斷該視窗內的所有滑鼠幾何判定、單擊、雙擊與拖拽
            if (show_close_confirmation)
            {
                ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
            }

            ImGui::Begin("###-AppBox-", nullptr, win_flags);

            // ==================== 【精簡版自訂標題列】 ====================
            ImVec2 winPos = ImGui::GetWindowPos();
            float winWidth = ImGui::GetWindowWidth();
            float padX = ImGui::GetStyle().WindowPadding.x;
            float titleHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
            ImDrawList* drawList = ImGui::GetWindowDrawList();

            // 1. 繪製自訂標題列背景
            drawList->AddRectFilled(
                winPos,
                ImVec2(winPos.x + winWidth, winPos.y + titleHeight),
                ImGui::GetColorU32(ImGuiCol_TitleBgActive),
                ImGui::GetStyle().WindowRounding,
                ImDrawFlags_RoundCornersTop
            );

            // 2. 左側標題文字 "AppBox"（直接對齊左側內邊距，不再留折疊按鈕空位）
            float textOffsetY = (titleHeight - ImGui::GetFontSize()) * 0.5f;
            drawList->AddText(ImVec2(winPos.x + padX, winPos.y + textOffsetY), IMU32("#FaFaFa"), "AppBox");


            //// 1. 取得進度條需要的所有幾何資訊
            //ImDrawList* drawList = ImGui::GetForegroundDrawList(); // 使用 Foreground 確保進度條不會被主視窗內容遮擋
            //ImVec2 winPos = ImGui::GetWindowPos();
            //float winWidth = ImGui::GetWindowWidth();
            //float topPadding = ImGui::GetStyle().FramePadding.y;
            //float titleHeight = ImGui::GetFontSize() + topPadding * 2;

            // ============================================================
            // 🚀 核心新增：在標題列上繪製自訂進度條
            // ============================================================
            {
                // 🟢 宣告靜態快取變數，用來鎖定 10 分鐘內的數值
                static float progressValue = 1.0f;
                static ImColor cached_color = IMCLR("#3282F6");
                static int last_10min_block = -1; // 用來記錄上一次是在哪一個「10分鐘區塊」

                // 將時間統一轉換為當天的「分鐘數」以便計算
                int current_minutes = tm_now.tm_hour * 60 + tm_now.tm_min;

                // 🟢 計算當前屬於第幾個 10 分鐘區塊 (例如 8:32 是第 51 塊，8:39 也是第 51 塊)
                int current_10min_block = current_minutes / 10;

                // 🟢 只有當跨入新的 10 分鐘區塊時，才重新計算 progressValue 與顏色
                if (current_10min_block != last_10min_block)
                {
                    last_10min_block = current_10min_block;

                    int start_minutes = 8 * 60 + 30;   // 08:30 -> 510 分鐘
                    int end_minutes = 19 * 60 + 30;   // 19:30 -> 1170 分鐘

                    if (current_minutes < start_minutes)
                    {
                        progressValue = 1.0f;
                    }
                    else if (current_minutes >= end_minutes)
                    {
                        progressValue = 0.0f;
                    }
                    else
                    {
                        // 在區間內，計算當前時間（已鎖定在 10 分鐘階梯）距離晚上 19:30 的剩餘時間比例
                        float total_duration = (float)(end_minutes - start_minutes);
                        float remaining_time = (float)(end_minutes - current_minutes);
                        progressValue = remaining_time / total_duration;
                    }
                }
                // 3. 計算進度條在標題列底部的邊界座標
                // 我們把它設計成緊貼在標題列最底端的一條細線（高約 2~3 像素），這樣最精緻且不擋文字
                int midx = winPos.x + winWidth / 2;
                int midy = titleHeight / 2 + winPos.y;
                const int barHalfLen = 50;
                ImVec2 barMin = ImVec2(midx - barHalfLen, midy - 6);
                ImVec2 barMax = ImVec2(midx + barHalfLen, midy + 7);

                //  barMin.x += (winWidth / 2 - barHalfLen - titleHeight);
                //  barMax.x += (winWidth / 2 - barHalfLen - titleHeight);

                    // 4. 繪製進度條背景（淡淡的灰色或深色細條）
                drawList->AddRectFilled(barMin, barMax, IM_COL32(160, 160, 160, 100));

                // 5. 🎯 核心修改：計算當前進度，並改成從右向左填充
                float currentWidth = (barMax.x - barMin.x) * progressValue;
                // 🟢 這裡改為：高亮條的左端點（起點）是從右邊界 barMax.x 減去當前寬度
                ImVec2 progressMin = ImVec2(barMax.x - currentWidth, barMin.y);

                if (progressValue > 0.0f)
                {
                    static std::vector<ImColor> arrClr = { IMCLR("#3282F6"),
                        IMCLR("#00A2E8"),  IMCLR("#22B14C"),  IMCLR("#F5E800"),  IMCLR("#F77B26"),  IMCLR("#D41920"),
                        IMCLR("#3282F6") };
                    ImColor c = arrClr[tm_now.tm_wday];
                    // 🟢 傳入參數改為 progressMin（新起點）到 barMax（右終點）
                    drawList->AddRectFilled(progressMin, barMax, c);
                }
                barMin.x -= 1; barMin.y -= 1;
                drawList->AddLine(barMin, ImVec2(barMax.x + 1, barMin.y), IM_COL32(30, 30, 30, 200));
                drawList->AddLine(barMin, ImVec2(barMin.x, barMax.y + 1), IM_COL32(30, 30, 30, 200));
                drawList->AddLine(ImVec2(barMin.x + 1, barMax.y), barMax, IM_COL32(220, 220, 220, 200));
                drawList->AddLine(ImVec2(barMax.x, barMin.y + 1), ImVec2(barMax.x, barMax.y + 1), IM_COL32(220, 220, 220, 200));

            }

            {
                ImU32 color = IMU32("#FaFaFa");
                //                 ImGui::PushFont(theApp.g_TitleTimeFont);
                ImVec2 textSizeFull = ImGui::CalcTextSize(timeStrFull.c_str());
                ImVec2 textSizeWeek = ImGui::CalcTextSize(timeStr0.c_str());
                float rightMargin = titleHeight;
                ImVec2 textPos;
                textPos.x = winPos.x + winWidth - textSizeFull.x - rightMargin;
                textPos.y = winPos.y + (titleHeight - textSizeFull.y) * 0.5f + ImGui::GetStyle().FramePadding.y / 2;
                textPos.y -= 3;//L_TODO make it configable

                // 1. 只需把這兩個狀態/寬度宣告放在 Draw Week 之前
                static float secExpandProgress = 0.0f;
                static ImVec2 symSize0 = ImGui::CalcTextSize(sTimeSymbol[0]);
                static ImVec2 symSize1 = ImGui::CalcTextSize(sTimeSymbol[1]);
                const float secWidth = symSize1.x + symSize0.x * 2.0f; // ":SS" 的總寬度

                //Draw Week
                ImVec2 weekPos = textPos;
                weekPos.x -= secWidth * secExpandProgress; // 直接跟著時間的展開進度向左移動
                weekPos.y -= 2;
                drawList->AddText(weekPos, color, timeStr0.c_str());

                //Draw Time
                {
                    // 1. 定義支援的動畫類型
                    enum class TimeAnimType {
                        SlideDown, // 下滑並淡出，新數字從上滑入
                        Flip3D     // 3D 翻轉（繞水平軸垂直壓縮）
                    };

                    // 2. 控制變數：修改這個變數即可一鍵切換動畫效果！
                    static TimeAnimType animType = TimeAnimType::SlideDown;

                    // 3. 動態狀態結構 (維持 8 個位置)
                    struct DigitAnimation {
                        std::string currentStr = "";
                        std::string oldStr = "";
                        float progress = 1.0f;
                    };
                    static DigitAnimation timeAnims[8];
                    const float ANIM_DURATION = 0.15f; // 數字翻轉動畫持續時間 (秒)

                    // 秒數區塊展開/收合動畫進度與延遲停留計時器
                    //static float secExpandProgress = 0.0f;
                    static float secHoldTimer = 0.0f;      // 失去 Hover 後的剩餘停留秒數
                    const float SEC_HOLD_DELAY = 3.0f;     // 停留時間：3 秒
                    const float SEC_FADE_DURATION = 0.18f; // 展開/收合漸變時間

                    // 1. 每幀動態更新目標字串
                    std::string arrTime[] = { "", "", ":", "", "", ":", "", "" };
                    arrTime[0] = std::to_string(tm_now.tm_hour / 10);
                    arrTime[1] = std::to_string(tm_now.tm_hour % 10);
                    arrTime[3] = std::to_string(tm_now.tm_min / 10);
                    arrTime[4] = std::to_string(tm_now.tm_min % 10);
                    arrTime[6] = std::to_string(tm_now.tm_sec / 10);
                    arrTime[7] = std::to_string(tm_now.tm_sec % 10);

                    // 2. 寬度與高度快取
                    //static ImVec2 symSize0 = ImGui::CalcTextSize(sTimeSymbol[0]);
                    //static ImVec2 symSize1 = ImGui::CalcTextSize(sTimeSymbol[1]);
                    static float arrTimeLen[] = { symSize0.x, symSize0.x, symSize1.x, symSize0.x, symSize0.x, symSize1.x, symSize0.x, symSize0.x };
                    const float fontHeight = symSize0.y;

                    // 計算「時:分 (HH:MM)」寬度與「:秒秒 (:SS)」寬度
                    const float hmWidth = arrTimeLen[0] + arrTimeLen[1] + arrTimeLen[2] + arrTimeLen[3] + arrTimeLen[4];
                    //const float secWidth = arrTimeLen[5] + arrTimeLen[6] + arrTimeLen[7];

                    // 3. 準備顏色與時間增量
                    float deltaTime = ImGui::GetIO().DeltaTime;
                    ImVec4 colVec = ImGui::ColorConvertU32ToFloat4(color);

                    // 4. 鎖定「右邊界基準點」，計算隨展開進度往左延伸的動態範圍
                    textPos.x += textSizeWeek.x;
                    // 未展開時，HH:MM 佔據 [textPos.x, textPos.x + hmWidth]；此右邊界永遠固定不動
                    const float rightAnchorX = textPos.x + hmWidth;

                    // 當前實際佔用寬度（從 hmWidth 平滑過渡到 hmWidth + secWidth）
                    float currentTotalWidth = hmWidth + secWidth * secExpandProgress;
                    float currentLeftX = rightAnchorX - currentTotalWidth;

                    // Hover 偵測範圍：未展開時僅為 HH:MM 寬度；展開時包含左側推開後的完整 HH:MM:SS 寬度
                    ImVec2 timeRectMin(currentLeftX, textPos.y);
                    ImVec2 timeRectMax(rightAnchorX, textPos.y + fontHeight);
                    bool isTimeHovered = ImGui::IsMouseHoveringRect(timeRectMin, timeRectMax, false);

                    // 更新停留計時器與展開/收合進度
                    if (isTimeHovered)
                    {
                        secHoldTimer = SEC_HOLD_DELAY; // 只要處於 Hover，持續將停留時間刷新為 3 秒
                    }
                    else if (secHoldTimer > 0.0f)
                    {
                        secHoldTimer -= deltaTime;     // 離開 Hover 後開始倒數 3 秒
                        if (secHoldTimer < 0.0f) secHoldTimer = 0.0f;
                    }

                    // 只要仍在 Hover 或 3 秒倒數尚未歸零，就保持展開狀態
                    bool shouldShowSeconds = (isTimeHovered || secHoldTimer > 0.0f);
                    if (shouldShowSeconds)
                    {
                        secExpandProgress += deltaTime / SEC_FADE_DURATION;
                        if (secExpandProgress > 1.0f) secExpandProgress = 1.0f;
                    }
                    else
                    {
                        secExpandProgress -= deltaTime / SEC_FADE_DURATION;
                        if (secExpandProgress < 0.0f) secExpandProgress = 0.0f;
                    }

                    // 5. 設定繪製起點為動態左起點（隨著 secExpandProgress 增加，整體平滑往左移動）
                    textPos.x = currentLeftX;

                    for (int i = 0; i < 8; i++)
                    {
                        DigitAnimation& anim = timeAnims[i];
                        bool isSecondPart = (i >= 5); // 索引 5, 6, 7 分別為 ':', 'S', 'S'

                        // 偵測該位置字串是否改變
                        if (arrTime[i] != anim.currentStr)
                        {
                            anim.oldStr = anim.currentStr;
                            anim.currentStr = arrTime[i];
                            // 若秒數處於完全隱藏狀態，直接跳過舊數字翻轉動畫，確保展開時顯示最新秒數
                            if (anim.oldStr.empty() || (isSecondPart && secExpandProgress <= 0.0f))
                                anim.progress = 1.0f;
                            else
                                anim.progress = 0.0f;
                        }

                        // 若是秒數部分且完全收合，則不需繪製
                        if (isSecondPart && secExpandProgress <= 0.0f)
                        {
                            continue;
                        }

                        // 秒數部分隨展開進度淡入（HH:MM 保持 100% 不透明，隨整體平滑左移）
                        float partAlphaScale = isSecondPart ? secExpandProgress : 1.0f;
                        ImU32 baseCol = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, colVec.w * partAlphaScale));

                        // 更新數字翻轉動畫進度
                        if (anim.progress < 1.0f)
                        {
                            anim.progress += deltaTime / ANIM_DURATION;
                            if (anim.progress > 1.0f) anim.progress = 1.0f;
                        }

                        // 進行複合動畫繪製
                        if (anim.progress < 1.0f)
                        {
                            if (animType == TimeAnimType::SlideDown)
                            {
                                // ==================== 【模式一：下滑動畫】 ====================
                                const float MAX_OFFSET = fontHeight * 0.4f;

                                // 舊數字：淡出，向下移動
                                ImU32 colOut = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, colVec.w * partAlphaScale * (1.0f - anim.progress)));
                                drawList->AddText(ImVec2(textPos.x, textPos.y + anim.progress * MAX_OFFSET), colOut, anim.oldStr.c_str());

                                // 新數字：淡入，從上方滑下
                                ImU32 colIn = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, colVec.w * partAlphaScale * anim.progress));
                                drawList->AddText(ImVec2(textPos.x, textPos.y + (anim.progress - 1.0f) * MAX_OFFSET), colIn, anim.currentStr.c_str());
                            }
                            else if (animType == TimeAnimType::Flip3D)
                            {
                                // ==================== 【模式二：3D 翻轉動畫】 ====================
                                bool isIncoming = (anim.progress >= 0.5f);
                                float halfP = isIncoming ? ((anim.progress - 0.5f) * 2.0f) : (anim.progress * 2.0f);
                                std::string drawStr = isIncoming ? anim.currentStr : anim.oldStr;

                                float angleRad = (isIncoming ? (1.0f - halfP) : halfP) * (3.14159265f / 2.0f);
                                float scaleY = cosf(angleRad);

                                if (scaleY > 0.0f && !drawStr.empty())
                                {
                                    int vtx_idx_begin = drawList->VtxBuffer.Size;

                                    // 計算淡入淡出透明度（乘上秒數展開透明度）
                                    float alpha = (isIncoming ? halfP : (1.0f - halfP)) * partAlphaScale;
                                    ImU32 fadingColor = ImGui::ColorConvertFloat4ToU32(ImVec4(colVec.x, colVec.y, colVec.z, colVec.w * alpha));

                                    // 先以正常比例畫出文字
                                    drawList->AddText(textPos, fadingColor, drawStr.c_str());
                                    int vtx_idx_end = drawList->VtxBuffer.Size;

                                    // 透過頂點操作進行垂直壓縮 (寬度 100% 保持原樣)
                                    if (vtx_idx_end > vtx_idx_begin)
                                    {
                                        float centerY = textPos.y + fontHeight * 0.5f;
                                        for (int v = vtx_idx_begin; v < vtx_idx_end; v++)
                                        {
                                            drawList->VtxBuffer[v].pos.y = centerY + (drawList->VtxBuffer[v].pos.y - centerY) * scaleY;
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            // ==================== 【靜態或動畫結束】 ====================
                            drawList->AddText(textPos, baseCol, anim.currentStr.c_str());
                        }

                        // 移動 X 軸游標
                        textPos.x += arrTimeLen[i];
                    }
                }

                ////Draw Title
                //ImVec2 leftTextPos(winPos.x + titleHeight, textPos.y);
                //drawList->AddText(leftTextPos, color, "AppBox");

                // 5. 最右側：自訂圖示按鈕
                ImVec2 rightBtnPos(winPos.x + winWidth - titleHeight, winPos.y);
                ImGui::SetCursorScreenPos(rightBtnPos);
                if (ImGui::InvisibleButton("##TitleRightActionBtn", ImVec2(titleHeight, titleHeight)))
                {
                    // 🎯 在這裡執行您想要的功能（例如：沉到底層 / 關閉確認）
                    ::SetWindowPos(g_hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
                bool rightHovered = ImGui::IsItemHovered();
                if (rightHovered)
                {
                    drawList->AddRectFilled(
                        ImVec2(rightBtnPos.x + 3, rightBtnPos.y + 3),
                        ImVec2(rightBtnPos.x + titleHeight - 3, rightBtnPos.y + titleHeight - 3),
                        IM_COL32(255, 255, 255, 45), 4.0f
                    );
                }
                // 繪製自訂圖示（例如：向下收納箭頭，或換成 drawList->AddImage）
                {
                    ImVec2 c(rightBtnPos.x + titleHeight * 0.5f, rightBtnPos.y + titleHeight * 0.5f);
                    float r = titleHeight * 0.22f;
                    ImU32 iconCol = rightHovered ? IM_COL32(255, 255, 255, 255) : IM_COL32(210, 210, 210, 230);
                    drawList->AddLine(ImVec2(c.x - r, c.y - r * 0.4f), ImVec2(c.x, c.y + r * 0.5f), iconCol, 1.8f);
                    drawList->AddLine(ImVec2(c.x, c.y + r * 0.5f), ImVec2(c.x + r, c.y - r * 0.4f), iconCol, 1.8f);
                }

                // 6. 將游標移到標題列正下方，直接渲染分頁與資料夾內容
                ImGui::SetCursorScreenPos(ImVec2(winPos.x + padX, winPos.y + titleHeight + ImGui::GetStyle().ItemSpacing.y));

            }

            {
                ImVec2 sz = ImGui::GetWindowSize();
                if (sz.y > 0) g_OSWinH = (int)sz.y;
            }
            {
                gTabUI.Render();
                if (gTabUI.m_pendingTabMoveFrom > 0 && gTabUI.m_pendingTabMoveTo > 0)
                {
                    gFUD.MoveFolder(gTabUI.m_pendingTabMoveFrom, gTabUI.m_pendingTabMoveTo);
                    gTabUI.m_tabBarVersion++;
                    gTabUI.m_restoreCountdown = 10;
                    gTabUI.m_pendingTabMoveFrom = -1;
                    gTabUI.m_pendingTabMoveTo = -1;
                }
                gFU.Render(g_hwnd, gFUD);
            }


            ImGui::End();

            if (show_close_confirmation)
            {
                ImGui::PopItemFlag();
            }
        }

        ShowCloseConfDlg(show_close_confirmation);
    }

    ImGui::Render();
    const float clear_color_with_alpha[] = {0.0f, 0.0f, 0.0f, 0.0f};
    g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
    g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

    HRESULT hr = g_pSwapChain->Present(1, 0);
    g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);

    if (g_pendingReloadIdx != -1)
    {
        gFUD.ReloadFolder(g_pd3dDevice, &gRes, g_pendingReloadIdx);
        g_pendingReloadIdx = -1;
    }

}