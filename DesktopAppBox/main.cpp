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

FolderUIData gFUD;
FolderUI gFU;

static int g_pendingReloadIdx = -1;

TabHeadUI gTabUI;
ResLoader gRes;

// 全局幾何變數
static int g_OSWinH = 0;          // 折疊前的"完整" ImGui 高度，供恢復用
bool isCollapsed = false;
static HWND g_hwnd = nullptr;      // Win32 Host 窗口 HWND

static constexpr int DEFAULT_IMGUI_W = 600;
static constexpr int DEFAULT_IMGUI_H = 400;

void ShowCloseConfDlg(bool& show_close_confirmation)
{
    if (show_close_confirmation)
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImVec2 center = ImVec2(
            viewport->Pos.x + viewport->Size.x * 0.5f,
            viewport->Pos.y + viewport->Size.y * 0.5f
        );
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::OpenPopup("ExitConfirmation");
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
            ::PostMessage(g_hwnd, WM_CLOSE, 0, 0);
        }
        ImGui::SameLine();
        if (ImGui::Button("重启", ImVec2(button_width, 0)))
        {
            ImGui::CloseCurrentPopup();
            show_close_confirmation = false;
            ::PostMessage(g_hwnd, WM_CLOSE, 1, 0);
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

    // 🟢 🎯 核心簡化 1：Win32 建立時給予預設值即可。不需要讀寫 Ini 函數。
    // 因為隨後進入主循環後，ImGui 載入完 ini 會自己把視窗搬到正確的位置，每幀同步代碼會自動把 Win32 拉過去貼合。
    int w = (int)(DEFAULT_IMGUI_W * main_scale);
    int h = (int)(DEFAULT_IMGUI_H * main_scale);
    g_OSWinH = h;

    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        wc.lpszClassName,
        L"-AppBox-",
        WS_POPUP/* | WS_CLIPCHILDREN*/,
        0, 0, w, h,
        nullptr, nullptr, wc.hInstance, nullptr
    );
    g_hwnd = hwnd;

    // 🚀 【核心修正：開啟 Win32 視窗系統級透明】
    // 將邊設為 -1，代表將透明區域擴展到整個視窗的客戶區（Sheet of Glass 效果）
    MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    //显示桌面时不会被隐藏
    if (HWND hProgman = ::FindWindowW(L"Progman", L"Program Manager"))
    {
        ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
    }


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

    ::DragAcceptFiles(hwnd, gCfg.m_bEnableFileDragIO ? TRUE : FALSE);

    theApp.InitImGuiContext(main_scale);

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

            // 每幀將 Win32 宿主視窗同步貼合到 ImGui 主視窗的位置（優化副屏閃爍）
            {
                ImGuiWindow* imguiWin = ImGui::FindWindowByName("###-AppBox-");
                if (imguiWin)
                {
                    ImVec2 ipos = (imguiWin->Viewport != nullptr) ? imguiWin->Viewport->Pos : imguiWin->Pos;
                    ImVec2 isz = imguiWin->Size;

                    // 🟢 使用 round 代替 floor，避免負數座標或 DPI 縮放帶來的邊界計算誤差
                    int targetX = (int)std::round(ipos.x);
                    int targetY = (int)std::round(ipos.y);
                    int targetW = isz.x > 0 ? (int)std::round(isz.x) : w;
                    int targetH = isz.y > 0 ? (int)std::round(isz.y) : h;

                    static int lastX = INT_MIN, lastY = INT_MIN, lastW = INT_MIN, lastH = INT_MIN;

                    // 🟢 只有當位移大於 1 像素時才觸發 SetWindowPos，防止在副屏 DPI 邊界上來回抖動
                    if (std::abs(lastX - targetX) > 1 || std::abs(lastY - targetY) > 1 ||
                        std::abs(lastW - targetW) > 1 || std::abs(lastH - targetH) > 1)
                    {
                        lastX = targetX; lastY = targetY; lastW = targetW; lastH = targetH;

                        // 🟢 加入 SWP_NOREDRAW 標誌，避免 Win32 系統重複觸發無效的視窗擦除，減少閃爍
                        ::SetWindowPos(hwnd, NULL, targetX, targetY, targetW, targetH,
                            SWP_NOACTIVATE | SWP_NOOWNERZORDER | SWP_NOREDRAW);
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
                //std::strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d (%a) %H:%M:%S", &tm_now);
                std::strftime(timeBuffer, sizeof(timeBuffer), "(%a) %H:%M:%S", &tm_now);

                // 🎯 核心修正：如果確認關閉的彈窗正在顯示，將整個主視窗的元件狀態切換為「停用（Disabled）」
                // 這會讓 ImGui 自動在底層切斷該視窗內的所有滑鼠幾何判定、單擊、雙擊與拖拽
                if (show_close_confirmation)
                {
                    ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
                  //  ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.6f); // 可選：讓背景微微變暗，視覺效果更好
                }

                ImGui::Begin("###-AppBox-", &bOpen, win_flags);

                // ============================================================
                // 🚀 核心新增：在標題列上繪製自訂進度條
                // ============================================================
                {
                    // 1. 取得進度條需要的所有幾何資訊
                    ImDrawList* drawList = ImGui::GetForegroundDrawList(); // 使用 Foreground 確保進度條不會被主視窗內容遮擋
                    ImVec2 winPos = ImGui::GetWindowPos();
                    float winWidth = ImGui::GetWindowWidth();
                    float topPadding = ImGui::GetStyle().FramePadding.y;
                    float barHeight = ImGui::GetFontSize() + topPadding * 2;

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
                    int midy = barHeight / 2 + winPos.y;
                    ImVec2 barMin = ImVec2(midx - 50, midy - 6);
                    ImVec2 barMax = ImVec2(midx + 50, midy + 7);

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

                ImDrawList* drawList = ImGui::GetForegroundDrawList();
                ImVec2 winPos = ImGui::GetWindowPos();
                float winWidth = ImGui::GetWindowWidth();
                float titleBarHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
                ImGui::PushFont(theApp.g_TitleTimeFont);
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
                    if (sz.y > 0) g_OSWinH = (int)sz.y;
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

                if (show_close_confirmation)
                {
                   // ImGui::PopStyleVar();
                    ImGui::PopItemFlag();
                }
            }
            if (!bOpen)
            {
                bOpen = true;
                show_close_confirmation = true;
            }

            ShowCloseConfDlg(show_close_confirmation);
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
            fprintf(f, "\n[Window][###-AppBox-]\n");
            fprintf(f, "Pos=%d,%d\n", g_FinalWinX, g_FinalWinY);
            fprintf(f, "Size=%d,%d\n", g_FinalWinW, g_FinalWinH);
            fprintf(f, "Collapsed=%d\n\n", isCollapsed ? 1 : 0);
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
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;
    switch (msg)
    {
    case WM_NCHITTEST:
    {
        POINT pt = { (int)(short)LOWORD(lParam), (int)(short)HIWORD(lParam) };
        POINT screenPt = pt;
        ::ScreenToClient(hWnd, &pt);
        ImGuiWindow* pWin = ImGui::FindWindowByName("###-AppBox-");
        if (pWin && !isCollapsed)
        {
            ImRect titleRect = pWin->TitleBarRect();
            float distFromTop = 0.0f;
#if ENABLE_VIEWPORTS
            distFromTop = (float)screenPt.y - titleRect.Min.y;
#else
            distFromTop = (float)pt.y - titleRect.Min.y;
#endif
            if (distFromTop >= 0.0f && distFromTop <= 5.0f)
            {
                return HTCLIENT;
            }
            float h = titleRect.GetHeight();
            titleRect.Min.x += h;
            titleRect.Max.x -= h;
            titleRect.Min.y += 5.0f;
#if ENABLE_VIEWPORTS
            if (titleRect.Contains(ImVec2((float)screenPt.x, (float)screenPt.y)))
#else
            if (titleRect.Contains(ImVec2((float)pt.x, (float)pt.y)))
#endif
            {
                return HTCAPTION;
            }
        }
        else if (pWin && isCollapsed)
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
                return HTCAPTION;
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
            // 🟢 🎯 核心簡化 2：不需要任何檔案寫入函數！
            // 優先從 Viewport 取得絕對座標，然後直接校正修正 ImGui 記憶體裡的 Ini 快取。
            ImVec2 p = (imguiWin->Viewport != nullptr) ? imguiWin->Viewport->Pos : imguiWin->Pos;
            ImVec2 s = imguiWin->Size;
            // 僅「記錄」數值，先不寫入 ImGui 內部
            g_FinalWinX = (int)p.x;
            g_FinalWinY = (int)p.y;
            g_FinalWinW = (int)s.x;
            g_FinalWinH = isCollapsed ? g_OSWinH : (int)s.y;
            g_HasFinalGeometry = true;
        }

        g_bStart = (1 == (int)wParam);
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