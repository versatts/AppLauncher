// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com
// - Getting Started      https://dearimgui.com
// - Documentation        https://dearimgui.com (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "ImGuiObj.h"
#include <d3d11.h>
#include <tchar.h>
#include <vector>
#include <algorithm>
#include <chrono>
#include <ctime>
#include <cmath>    // std::floor
#include <string>
#include <cstdio>   // std::snprintf, fopen, fgets, fread, fwrite
#include <climits>  // INT_MIN

#include <shellapi.h>   // Explorer 檔案拖放 (WM_DROPFILES / DragQueryFileW)
#pragma comment(lib, "shell32.lib")

#include "Application.h"
#include "link.h"
#include "ResLoader.h"
#include "TabHeadUI.h"
#include "FolderUIData.h"
#include "FolderUI.h"

Application theApp;
// Data
static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
ImFont* g_TitleTimeFont = nullptr;

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

int g_WinW = 600;
int g_WinH = 400;

FolderUIData gFUD;
FolderUI gFU;

bool g_bEnableFileDragIO = true;
static int g_pendingReloadIdx = -1;

TabHeadUI gTabUI;
ResLoader gRes;

// 🟢 g_OSWinX/Y/W/H 用來保存 ImGui 主窗口（"-AppBox-"）的位置和大小
static int g_OSWinX = 100, g_OSWinY = 100, g_OSWinW = 0, g_OSWinH = 0;
static int g_OSWinH_0 = 0;          // 折疊前的"完整" ImGui 高度，供保存恢復用
bool isCollapsed = false;
static HWND g_hwnd = nullptr;      // Win32 Host 窗口 HWND

// ImGui 默認窗口大小（首次啟動 / ini 中沒有 Pos/Size 時使用）
static constexpr int DEFAULT_IMGUI_W = 600;
static constexpr int DEFAULT_IMGUI_H = 400;

// 手動從 imgui.ini 中解析 [Window][-AppBox-] 節點的 Pos= / Size=
static void LoadAppBoxRectFromIni(const char* iniPath, bool& found, int& x, int& y, int& w, int& h)
{
    found = false;
    x = y = w = h = 0;
    if (!iniPath) return;
    FILE* f = ::fopen(iniPath, "rt");
    if (!f) return;

    char line[512];
    bool inAppBox = false;
    while (::fgets(line, sizeof(line), f))
    {
        if (line[0] == '[')
        {
            inAppBox = (::strstr(line, "[-AppBox-]") != nullptr);
            continue;
        }
        if (!inAppBox) continue;

        int tx = 0, ty = 0, tw = 0, th = 0;
        if (::strncmp(line, "Pos=", 4) == 0 && ::sscanf(line + 4, "%d,%d", &tx, &ty) == 2)
        {
            x = tx;
            y = ty;
            found = true;
        }
        else if (::strncmp(line, "Size=", 5) == 0 && ::sscanf(line + 5, "%d,%d", &tw, &th) == 2)
        {
            w = tw;
            h = th;
            found = true;
        }
    }
    ::fclose(f);
}

// 寫入時也寫到 ImGui 默認節點 [Window][-AppBox-] 中（覆蓋 Pos= 和 Size=）
static void WriteAppBoxRectToIni(const char* iniPath, int x, int y, int w, int h)
{
    if (!iniPath) return;
    FILE* f = ::fopen(iniPath, "rt");
    std::string body;
    if (f)
    {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (sz > 0 && sz < 4 * 1024 * 1024)
        {
            body.assign((size_t)sz, '\0');
            fread(body.data(), 1, (size_t)sz, f);
        }
        fclose(f);
    }

    auto replace_or_insert = [&](const std::string& key, int v1, int v2) {
        char needle[64];
        std::snprintf(needle, sizeof(needle), "%s=", key.c_str());
        std::string repl = key + "=" + std::to_string(v1) + "," + std::to_string(v2);

        size_t pos = body.find("[Window][-AppBox-]");
        if (pos == std::string::npos) {
            if (!body.empty() && body.back() != '\n') body.push_back('\n');
            body += "[Window][-AppBox-]\n";
            body += repl + "\n\n";
            return;
        }

        size_t next = body.find("\n[", pos);
        size_t end = (next == std::string::npos) ? body.size() : next;

        bool replaced = false;
        size_t scan = pos;
        while (scan < end)
        {
            size_t eol = body.find('\n', scan);
            if (eol == std::string::npos || eol > end) eol = end;
            std::string l = body.substr(scan, eol - scan);
            if (l.rfind(needle, 0) == 0)
            {
                body.replace(scan, eol - scan, repl);
                replaced = true;
                break;
            }
            scan = eol + 1;
        }

        if (!replaced)
        {
            size_t lineEnd = body.find('\n', pos);
            if (lineEnd == std::string::npos) lineEnd = body.size();
            body.insert(lineEnd + 1, repl + "\n");
        }
        };

    replace_or_insert("Pos", x, y);
    replace_or_insert("Size", w, h);

    f = ::fopen(iniPath, "wt");
    if (f)
    {
        fwrite(body.data(), 1, body.size(), f);
        fclose(f);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::LoadIniSettingsFromDisk(io.IniFilename);

    {
        bool iniFound = false;
        LoadAppBoxRectFromIni(io.IniFilename, iniFound, g_OSWinX, g_OSWinY, g_OSWinW, g_OSWinH);
        if (iniFound)
        {
            if (g_OSWinW < 100) g_OSWinW = 100;
            if (g_OSWinH < 50)  g_OSWinH = 50;
        }
    }

    int cx = ::GetSystemMetrics(SM_CXSCREEN);
    int cy = ::GetSystemMetrics(SM_CYSCREEN);

    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui-001", nullptr };
    ::RegisterClassExW(&wc);

    if (g_OSWinW == 0)
    {
        g_OSWinW = (int)(DEFAULT_IMGUI_W * main_scale);
        g_OSWinH = (int)(DEFAULT_IMGUI_H * main_scale);
        g_OSWinX = (cx - g_OSWinW) / 2;
        g_OSWinY = (cy - g_OSWinH) / 2;
    }
    if (g_OSWinW < 100) g_OSWinW = 100;
    if (g_OSWinH < 50)  g_OSWinH = 50;
    g_OSWinH_0 = g_OSWinH;

    // 🟢 🎯 修正點 1：建立 Win32 視窗時，初始大小直接與 ImGui 窗口的大小完全同步
    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"-AppBox-",
        WS_POPUP | WS_CLIPCHILDREN,
        g_OSWinX, g_OSWinY, g_OSWinW, g_OSWinH, // 使用 ImGui 的坐標與寬高
        nullptr, nullptr, wc.hInstance, nullptr
    );
    g_hwnd = hwnd;

    if (HWND hProgman = ::FindWindowW(L"Progman", L"Program Manager"))
    {
        ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
    }

    ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0,
        SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);

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

    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    ::DragAcceptFiles(hwnd, g_bEnableFileDragIO ? TRUE : FALSE);

    theApp.InitImGuiContext(main_scale);
    g_TitleTimeFont = theApp.g_TitleTimeFont;

    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    gFUD.Init(g_pd3dDevice, &gRes);

    gTabUI.m_Idx = 0;
    if (gFUD.gvFolder.size())
    {
        gTabUI.m_arrTab = gFUD.gvFolderName;
        gFUD.m_pIdx = &gTabUI.m_Idx;
    }

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4 clear_color = ImVec4(1.0f, 1.0f, 1.0f, 0.0f);

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

        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            theApp.CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            theApp.CreateRenderTarget();
            g_mainRenderTargetView = theApp.g_mainRenderTargetView;
        }

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        {
            static bool show_close_confirmation = false;

            // 🟢 🎯 修正點 2：每幀精準同步 Win32 宿主視窗的大小的與位置到與 ImGui 主窗口一致
            {
                ImGuiWindow* imguiWin = ImGui::FindWindowByName("###-AppBox-");
                if (imguiWin)
                {
                    ImVec2 ipos = imguiWin->Pos;
                    ImVec2 isz = imguiWin->Size;

                    // Fallback 機制防止首幀異常
                    int targetX = (int)std::floor(ipos.x);
                    int targetY = (int)std::floor(ipos.y);
                    int targetW = isz.x > 0 ? (int)std::floor(isz.x) : g_OSWinW;
                    int targetH = isz.y > 0 ? (int)std::floor(isz.y) : g_OSWinH;

                    // 只有當位置或大小發生改變時才調用 SetWindowPos，避免多餘的 Win32 重繪抖動
                    static int lastX = INT_MIN, lastY = INT_MIN, lastW = INT_MIN, lastH = INT_MIN;
                    if (lastX != targetX || lastY != targetY || lastW != targetW || lastH != targetH)
                    {
                        lastX = targetX; lastY = targetY; lastW = targetW; lastH = targetH;
                        // 讓 Win32 宿主視窗的位置、寬、高與 ImGui 保持 1:1 完全貼合
                        ::SetWindowPos(hwnd, HWND_BOTTOM, targetX, targetY, targetW, targetH,
                            SWP_NOACTIVATE | SWP_NOOWNERZORDER);
                    }
                }
            }
            ImGuiWindowFlags win_flags = 0;
            win_flags |= ImGuiWindowFlags_NoDocking;
            static float f = 0.0f;
            static int counter = 0;
            static bool bOpen = true;
            if (bOpen)
            {
                auto now = std::chrono::system_clock::now();
                std::time_t time_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now;
                localtime_s(&tm_now, &time_now);
                char timeBuffer[64];
                std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d (%a) %H:%M:%S", &tm_now);
                ImGui::Begin("###-AppBox-", &bOpen, win_flags);
                ImDrawList* drawList = ImGui::GetForegroundDrawList();
                ImVec2 winPos = ImGui::GetWindowPos();
                float winWidth = ImGui::GetWindowWidth();
                float titleBarHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
                ImGui::PushFont(g_TitleTimeFont);
                ImVec2 textSize = ImGui::CalcTextSize(timeBuffer);
                float rightMargin = bOpen ? 45.0f : 15.0f;
                ImVec2 textPos;
                textPos.x = winPos.x + winWidth - textSize.x - rightMargin;
                textPos.y = winPos.y + (titleBarHeight - textSize.y) * 0.5f + ImGui::GetStyle().FramePadding.y / 2;
                drawList->AddText(textPos, IM_COL32(233, 233, 233, 255), timeBuffer);
                ImVec2 leftTextPos(winPos.x + titleBarHeight, textPos.y);
                drawList->AddText(leftTextPos, IM_COL32(233, 233, 233, 255), "-AppBox-");
                ImGui::PopFont();
                isCollapsed = ImGui::IsWindowCollapsed();
                if (!isCollapsed)
                {
                    ImVec2 sz = ImGui::GetWindowSize();
                    if (sz.y > 0) g_OSWinH_0 = (int)sz.y;
                }
                if (!isCollapsed)
                {
                    gTabUI.Render();
                    if (gTabUI.m_pendingTabMoveFrom > 0 && gTabUI.m_pendingTabMoveTo > 0)
                    {
                        gFUD.MoveFolder(gTabUI.m_pendingTabMoveFrom, gTabUI.m_pendingTabMoveTo);
                        gTabUI.m_arrTab = gFUD.gvFolderName;
                        gTabUI.m_tabBarVersion++;
                        gTabUI.m_restoreCountdown = 10;
                        gTabUI.m_pendingTabMoveFrom = -1;
                        gTabUI.m_pendingTabMoveTo = -1;
                    }
                    gFU.Render(hwnd, gFUD);
                }
                ImGui::End();
            }
            if (!bOpen)
            {
                bOpen = true;
                show_close_confirmation = true;
            }
            if (show_close_confirmation)
            {
                ImGui::OpenPopup("ExitConfirmation");
                ImGuiViewport* viewport = ImGui::GetMainViewport();
                ImVec2 center = ImVec2(
                    viewport->Pos.x + viewport->Size.x * 0.5f,
                    viewport->Pos.y + viewport->Size.y * 0.5f
                );
                ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
            }
            if (ImGui::BeginPopupModal("ExitConfirmation", NULL, ImGuiWindowFlags_AlwaysAutoResize))
            {
                ImGui::Text("確定要關閉程式嗎？");
                ImGui::Separator();
                float button_width = ImGui::GetFontSize() * 5.0f;
                if (ImGui::Button("確定", ImVec2(button_width, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    show_close_confirmation = false;
                    ::PostMessage(hwnd, WM_CLOSE, 0, 0);
                }
                ImGui::SameLine();
                if (ImGui::Button("取消", ImVec2(button_width, 0)))
                {
                    ImGui::CloseCurrentPopup();
                    show_close_confirmation = false;
                }
                ImGui::EndPopup();
            }
        }
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
#if ENABLE_VIEWPORTS
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
            for (int i = 0; i < platform_io.Viewports.Size; i++)
            {
                ImGuiViewport* vp = platform_io.Viewports[i];
                if (vp == ImGui::GetMainViewport())
                    continue;
                HWND hImGuiWnd = reinterpret_cast<HWND>(vp->PlatformHandleRaw);
                if (!hImGuiWnd || !::IsWindow(hImGuiWnd))
                    continue;
                ::SetWindowPos(
                    hImGuiWnd,
                    HWND_TOP,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
                );
            }
        }
#endif
        HRESULT hr = g_pSwapChain->Present(1, 0);
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
        if (g_pendingReloadIdx != -1)
        {
            gFUD.ReloadFolder(g_pd3dDevice, &gRes, g_pendingReloadIdx);
            g_pendingReloadIdx = -1;
        }
    }
    gFUD.PersistOrder();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    theApp.CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    CoUninitialize();
    return 0;
}
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
void AppRemoveDragOutItem(const std::wstring& lnkPath)
{
    if (lnkPath.empty()) return;
    ::DeleteFileW(lnkPath.c_str());
    size_t slash = lnkPath.find_last_of(L"\/");
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
    if (!g_bEnableFileDragIO)
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
    if (curIdx > 0 && curIdx < (int)gFUD.gvFolderName.size())
    {
        subDir = gFUD.gvFolderName[curIdx];
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

        // 🎯 核心修正：正確初始化 wstring 緩衝區並傳入 &srcPath[0] 指針
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
                // 🎯 核心修正：將 Win32 Client 座標轉換為全域 Screen 座標，與 ImGui::GetItemRectMin() 的口徑完全一致
                ::ClientToScreen(g_hwnd, &pt);
                if (gFU.ApplyIconToItem(pt, srcPath))
                {
                    copiedAny = true;
                }
            }
        }
    }
    ::DragFinish(hDrop);

    if (copiedAny)
    {
        gFUD.ReloadFolder(g_pd3dDevice, &gRes, curIdx);
    }
}

LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
    case WM_NCHITTEST:
    {
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        POINT screenPt = pt;
        ::ScreenToClient(hWnd, &pt);
        RECT rc;
        ::GetClientRect(hWnd, &rc);
        const int border = 8;
        bool left = (pt.x < border);
        bool right = (pt.x > rc.right - border);
        bool top = (pt.y < border);
        bool bottom = (pt.y > rc.bottom - border);
        if (!isCollapsed)
        {
            if (top && left)     return HTTOPLEFT;
            if (top && right)    return HTTOPRIGHT;
            if (bottom && left)  return HTBOTTOMLEFT;
            if (bottom && right) return HTBOTTOMRIGHT;
            if (top)             return HTTOP;
            if (bottom)          return HTBOTTOM;
        }
        if (left)            return HTLEFT;
        if (right)           return HTRIGHT;
        ImGuiWindow* pWin = ImGui::FindWindowByName("-AppBox-");
        if (pWin)
        {
            ImRect titleRect = pWin->TitleBarRect();
            float h = titleRect.GetHeight();
            titleRect.Min.x += h;
            titleRect.Max.x -= h;
#if ENABLE_VIEWPORTS
            if (titleRect.Contains(ImVec2((float)screenPt.x, (float)screenPt.y)))
#else
            if (titleRect.Contains(ImVec2((float)pt.x, (float)pt.y)))
#endif
            {
                return HTCAPTION;
            }
        }
        return HTCLIENT;
    }
    case WM_CREATE:
        break;
    case WM_CLOSE:
    {
        ImGuiWindow* imguiWin = ImGui::FindWindowByName("###-AppBox-");
        if (imguiWin)
        {
            ImVec2 p = imguiWin->Pos;
            ImVec2 s = imguiWin->Size;
            g_OSWinX = (int)p.x;
            g_OSWinY = (int)p.y;
            g_OSWinW = (int)s.x;
            if (isCollapsed)
                g_OSWinH = g_OSWinH_0;
            else
                g_OSWinH = (int)s.y;
        }
        if (g_hwnd)
        {
            ImGuiIO& ioLocal = ImGui::GetIO();
            if (ioLocal.IniFilename && ioLocal.IniFilename[0])
                WriteAppBoxRectToIni(ioLocal.IniFilename, g_OSWinX, g_OSWinY, g_OSWinW, g_OSWinH);
        }
    }
    break;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam);
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
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
