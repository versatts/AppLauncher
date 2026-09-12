// Dear ImGui: standalone example application for Windows API + DirectX 11

// Learn about Dear ImGui:
// - FAQ                  https://dearimgui.com/faq
// - Getting Started      https://dearimgui.com/getting-started
// - Documentation        https://dearimgui.com/docs (same as your local docs/ folder).
// - Introduction, links and more at the top of imgui.cpp

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <d3d11.h>
#include <tchar.h>

#include "link.h"
#include "ResLoader.h"
#include "TabHead.h"
#include <vector>
#include <algorithm>

// Data
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static bool                     g_SwapChainOccluded = false;
static UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;

// Forward declarations of helper functions
bool CreateDeviceD3D(HWND hWnd);
void CleanupDeviceD3D();
void CreateRenderTarget();
void CleanupRenderTarget();
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);


HWND g_hSecondWnd = nullptr;

LRESULT CALLBACK SecondWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, msg, wParam, lParam);
    }
    return 0;
}

HWND gHwnd;
int g_WinW = 600;
int g_WinH = 400;

struct ST_APP
{
    ID3D11ShaderResourceView* iconSrv;
    WCHAR exePathBuf[MAX_PATH] = { 0 };
};

struct ST_FOLDER
{
    std::vector<ST_APP> vApp;
    std::wstring sName;
};

std::vector<ST_FOLDER> gvFolder;
std::vector<ST_APP>* gpvApp = nullptr;
int gCurTab = 0;
std::vector<std::wstring> gvFolderName;
TabHead gTab0;

bool IsPath(const std::string& inputStr)
{
    // Retrieve the file attributes from Windows
    DWORD attributes = ::GetFileAttributesA(inputStr.c_str());

    // INVALID_FILE_ATTRIBUTES means the path does not exist or is inaccessible
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    // Check if the FILE_ATTRIBUTE_DIRECTORY flag is present
    if (attributes & FILE_ATTRIBUTE_DIRECTORY)
    {
        return true; // It is a path/directory
    }

    return false; // It is a regular file
}

void MakeOneFolder(std::vector<std::wstring>& lnkList, ST_FOLDER& folder)
{
    for (const auto& path : lnkList)
    {
        ST_APP item;
        bool resolved = false;
        WCHAR iconPath[MAX_PATH] = { 0 };

        // 判斷是否為 Steam 的 .url 快捷方式
        if (path.size() > 4 && _wcsicmp(path.c_str() + path.size() - 4, L".url") == 0)
        {
            // 解析 URL 機制
            if (ResolveUrlTarget(path.c_str(), item.exePathBuf, MAX_PATH, iconPath, MAX_PATH))
            {
                resolved = true;
            }
        }
        else
        {
            // 原有的 .lnk 解析機制
            if (ResolveLnkTarget(path.c_str(), item.exePathBuf, MAX_PATH))
            {
                // 標準 exe 的圖標路徑就是它自己
                wcsncpy_s(iconPath, item.exePathBuf, MAX_PATH);
                resolved = true;
            }
        }

        if (resolved)
        {
            UINT w, h;
            // 💡 傳入 iconPath（如果是 Steam 會是快取的 .ico，如果是普通 EXE 會是 exe 自己的路徑）
            item.iconSrv = LoadHighestResIconSRV(g_pd3dDevice, iconPath, w, h);

            // 如果從指定路徑載入高清資源失敗（例如某些特殊 URL 快捷方式沒快取圖標）
            if (!item.iconSrv)
            {
                // 降級備用方案：使用傳統的系統圖標提取
                HICON hIco = ExtractExeMainIcon(iconPath);
                if (hIco) {
                    int sw = 0, sh = 0;
                    item.iconSrv = IconToD3D11SRV_Simple(g_pd3dDevice, hIco, sw, sh);
                    DestroyIcon(hIco);
                }
            }
            folder.vApp.push_back(item);
        }
    }
}

// 專門用來尋找桌面壁紙夾層的回呼函式
BOOL CALLBACK EnumWallpaperWindowsProc(HWND hwnd, LPARAM lParam)
{
    // 檢查這個視窗內部是否包含桌面圖標層 "SHELLDLL_DefView"
    HWND hShellView = ::FindWindowExW(hwnd, NULL, L"SHELLDLL_DefView", NULL);
    if (hShellView != NULL)
    {
        // 💡 找到了！在 Windows 架構中，真正拿來當桌布背景、且絕對不擋圖標的，
        // 就是緊跟在這個包含圖標層的視窗「後方」的下一個同級 "WorkerW" 視窗
        HWND* pResultHwnd = (HWND*)lParam;
        *pResultHwnd = ::FindWindowExW(NULL, hwnd, L"WorkerW", NULL);
        return FALSE; // 找到了就停止列舉，立刻退出
    }
    return TRUE; // 沒找到就繼續找下一個視窗
}
// Main code
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

    // 全局/静态缓存，一次性加载，不要每帧重复解析+创建纹理
    ImVec2 iconSize;


    int cx = ::GetSystemMetrics(SM_CXSCREEN);
    int cy = ::GetSystemMetrics(SM_CYSCREEN);

    // Make process DPI aware and obtain main monitor scale
    ImGui_ImplWin32_EnableDpiAwareness();
    float main_scale = ImGui_ImplWin32_GetDpiScaleForMonitor(::MonitorFromPoint(POINT{ 0, 0 }, MONITOR_DEFAULTTOPRIMARY));

    // Create application window
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr), nullptr, nullptr, nullptr, nullptr, L"ImGui-001", nullptr };
    ::RegisterClassExW(&wc);
#define NEW_PLAN 1
#if NEW_PLAN
    // ==================== 🚀 終極壁紙相容方案：可交互、不擋圖標版 ====================
     // 💡 核心修復一：移除 WS_EX_TRANSPARENT（滑鼠穿透），這樣按鈕和 Tab 就能正常點擊交互了！
     // 💡 核心修復二：保留 WS_EX_LAYERED 確保視窗跨越系統純色裁剪優化，100% 正常繪製
    HWND hwnd = ::CreateWindowExW(
        WS_EX_LAYERED,                      // 擴展樣式：僅保留層級，移除穿透
        wc.lpszClassName,
        L"-AppBox-",                         // 標題字串
        WS_POPUP | WS_CLIPCHILDREN,          // 標準彈出式（保持頂層身份，D3D才能穩定Present）
        (cx - g_WinW) / 2,
        (cy - g_WinH) / 2,
        (int)(g_WinW * main_scale),
        (int)(g_WinH * main_scale),
        nullptr, nullptr, wc.hInstance, nullptr
    );

    // 設定層級視窗的混色模式（不修改原本的透明度，但激發 Layered 獨立渲染鏈）
    ::SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);

    // 尋找 Progman 桌面管理器
    HWND hProgman = ::FindWindowW(L"Progman", L"Program Manager");
    if (hProgman)
    {
        // 💡 核心魔法：我們不呼叫 SetParent (會被裁剪隱形)，而是透過 SetWindowLongPtrW 
        // 將系統的 Progman 指定為我們視窗的 Owner（所有者）視窗！
        // 這樣可以讓視窗在邏輯層級上永久隸屬於桌面，同時保持獨立的渲染表面，不被純色優化給裁剪。
        ::SetWindowLongPtrW(hwnd, GWLP_HWNDPARENT, (LONG_PTR)hProgman);
    }

    // 初始將視窗推至最底部（與壁紙同高，絕對不遮擋桌面圖標）
    ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
    // ============================================================================
#else
 HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"", WS_POPUP | /*WS_OVERLAPPEDWINDOW | */WS_CLIPCHILDREN
        , (cx - g_WinW) / 2, (cy - g_WinH) / 2, (int)(g_WinW * main_scale), (int)(g_WinH * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

#endif
#define ENABLE_VIEWPORTS 1

#if 0
    // ==================== 🚀 終極壁紙嵌入：EnumWindows 萬能相容版 ====================
       // 第一步：發送 0x052C 神秘訊息，強迫系統生成桌布層
    HWND hProgman = ::FindWindowW(L"Progman", L"Program Manager");
    if (hProgman)
    {
        ::SendMessageTimeoutW(hProgman, 0x052C, 0, 0, SMTO_NORMAL, 1000, NULL);
    }

    // 第二步：呼叫萬能的 EnumWindows 進行全域視窗列舉，精確捕捉目標
    HWND hWallpaperTargetW = NULL;
    ::EnumWindows(EnumWallpaperWindowsProc, (LPARAM)&hWallpaperTargetW);

    // 第三步：【純色桌面特有降級防禦】
    // 如果 Enum 完發現 hWallpaperTargetW 依舊為空，說明系統在純色模式下把所有東西都壓在 Progman 裡
    // 此時我們就直接將父視窗指定為 hProgman 即可
    if (!hWallpaperTargetW)
    {
        hWallpaperTargetW = hProgman;
    }

    // 第四步：執行安全的子視窗化轉換與座標對齊
    if (hWallpaperTargetW)
    {
        // 賦予合法的子視窗 Control ID（徹底防止 Windows 阻斷繪製訊息）
        ::SetWindowLongPtrW(hwnd, GWLP_ID, (LONG_PTR)1001);

        // 轉換樣式為合法的子視窗
        DWORD style = ::GetWindowLongW(hwnd, GWL_STYLE);
        style &= ~WS_POPUP;
        style |= WS_CHILD;
        ::SetWindowLongW(hwnd, GWL_STYLE, style);

        // 💥 將您的視窗強行塞入我們定位出來的萬能壁紙容器中
        ::SetParent(hwnd, hWallpaperTargetW);

        // 重新對齊物理寬高與相對坐標
        int targetW = (int)(g_WinW * main_scale);
        int targetH = (int)(g_WinH * main_scale);
        int targetX = (cx - targetW) / 2;
        int targetY = (cy - targetH) / 2;

        ::MoveWindow(hwnd, targetX, targetY, targetW, targetH, TRUE);

        // 💡 如果最終去到了 Progman，需要強制下一筆 HWND_BOTTOM 確保不擋圖標
        if (hWallpaperTargetW == hProgman)
        {
            ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
    }
    // ============================================================================

#endif

    // Initialize Direct3D
    if (!CreateDeviceD3D(hwnd))
    {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }

    // Show the window
    ::ShowWindow(hwnd, SW_SHOW);
    ::UpdateWindow(hwnd);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;         // Enable Docking
#if ENABLE_VIEWPORTS
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
#endif 
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    //io.ConfigDockingAlwaysTabBar = true;
    //io.ConfigDockingTransparentPayload = true;

    // Setup Dear ImGui style
    //ImGui::StyleColorsDark();
    ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    style.FramePadding.x = 4.0f; // 增大，标题栏变高；减小，标题栏变矮
    style.FramePadding.y = 4.0f; // 增大，标题栏变高；减小，标题栏变矮

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_TitleBg] = ImColor(0x4B, 0x4B, 0x52); // 窗口未激活标题栏
    colors[ImGuiCol_TitleBgActive] = ImColor(0x4B, 0x4B, 0x52);// 当前激活窗口标题栏
    colors[ImGuiCol_TitleBgCollapsed] = ImColor(0x4B, 0x4B, 0x52); // 窗口折叠后的标题栏

    //#37373D
    // 1. 窗口主体背景
    style.Colors[ImGuiCol_WindowBg] = ImColor(0x37, 0x37, 0x3D);

    style.Colors[ImGuiCol_Text] = ImColor(0xFF, 0xFF, 0xFF);      //普通文字
    style.Colors[ImGuiCol_TextDisabled] = ImColor(0x80, 0x80, 0x80); //禁用控件文字;

    style.Colors[ImGuiCol_FrameBg] = ImColor(40, 40, 40, 255);
    style.Colors[ImGuiCol_FrameBgHovered] = ImColor(60, 60, 60, 255);
    style.Colors[ImGuiCol_FrameBgActive] = ImColor(80, 80, 80, 255);

    style.Colors[ImGuiCol_SliderGrab] = ImColor(60, 180, 255, 255);
    style.Colors[ImGuiCol_SliderGrabActive] = ImColor(90, 200, 255, 255);

    // Base color exactly matching RGB(85, 85, 85)
    style.Colors[ImGuiCol_Button] = ImVec4(0.333f, 0.333f, 0.333f, 1.0f); // RGB(85, 85, 85) - Base Button
    style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.400f, 0.400f, 0.400f, 1.0f); // RGB(102, 102, 102) - Elevated hover
    style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.266f, 0.266f, 0.266f, 1.0f); // RGB(68, 68, 68) - Sunken pressed state

    // Optional: If you want all buttons to have sharp corners like MFC globally
    style.FrameRounding = 10.0f;
    // =========================================================================

       // ==================== 🚀 FIXED TAB HEADER MATCHING STYLE ====================
    // 1. Core Tab Colors
    style.Colors[ImGuiCol_Tab] = ImVec4(0.266f, 0.266f, 0.266f, 1.0f); // RGB(68, 68, 68) - Inactive tab
    style.Colors[ImGuiCol_TabActive] = ImVec4(0.380f, 0.380f, 0.380f, 1.0f); // RGB(97, 97, 97) - Hovered highlight
    style.Colors[ImGuiCol_TabHovered] = ImVec4(0.480f, 0.480f, 0.480f, 1.0f); // RGB(55, 55, 55) - Active tab (blends with WindowBg)
    // 2. 💡 Frame / Border Customization
    // Overriding the border color to make sure it stands out explicitly around your selected items
    style.Colors[ImGuiCol_Border] = ImVec4(0.450f, 0.460f, 0.470f, 1.0f); // Sharp contrast gray for wireframe lines

    // 3. Geometry Tweak (Enabling Tab Border)
    style.TabRounding = 4.0f; // Rounded tops
    style.TabBorderSize = 1.0f; // 💡 CHANGED: Force 1-pixel frame around the tab headers
    // ============================================================================

        // ==================== 🚀 DARK TOOLTIP BACKGROUND ====================
    // Deep Charcoal background for all tooltips and popups
    style.Colors[ImGuiCol_PopupBg] = ImVec4(0.120f, 0.120f, 0.130f, 0.95f); // Near black with 95% opacity

    // Optional: Add a crisp subtle border to frame the tooltip nicely against the backdrop
    style.Colors[ImGuiCol_Border] = ImVec4(0.350f, 0.350f, 0.360f, 1.0f); // Sleek charcoal border
    style.PopupBorderSize = 1.0f; // Force a 1-pixel outline on popups/tooltips
    // ====================================================================


    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    std::wstring sDir;
    std::vector<std::wstring> vFolder;
    auto lnkList = EnumLnkFilesInAppDir(sDir, vFolder);

    ST_FOLDER folder0;
    folder0.sName = _T("Useful");
    MakeOneFolder(lnkList, folder0);
    gvFolder.push_back(folder0);
    gvFolderName.push_back(folder0.sName);
    gpvApp = &(gvFolder[0].vApp);
    gTab0.selectedTabIdx = &gCurTab;
    gTab0.folders = &gvFolderName;

    for (auto fx : vFolder)
    {
        ST_FOLDER folderx;
        folderx.sName = fx;
        std::vector<std::wstring> vEmpty;
        auto lnkList = EnumLnkFilesInAppDir(fx, vEmpty);
        MakeOneFolder(lnkList, folderx);
        gvFolder.push_back(folderx);
        gvFolderName.push_back(folderx.sName);
    }

    // 2. 設置您的微軟雅黑（如果您想讓它成為預設，直接加載並賦值給 FontDefault）
    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    // 清除或不要呼叫 AddFontDefaultVector / Bitmap
    ImFont* fontYaHei = io.Fonts->AddFontFromFileTTF(
        "C:\\Windows\\Fonts\\msyh.ttc",
        17.0f,
        &cfg,
        io.Fonts->GetGlyphRangesChineseFull() // 載入完整中文
    );

    if (fontYaHei != nullptr) {
        io.FontDefault = fontYaHei; // 設置全局預設字體
    }
    else {
        // 如果加載失敗的防禦方案：使用系統預設
        io.Fonts->AddFontDefault();
    }

    // ==================================================================

    // Our state
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle messages (inputs, window resize, etc.)
        // See the WndProc() function below for our to dispatch events to the Win32 backend.
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

        // Handle window being minimized or screen locked
        if (g_SwapChainOccluded && g_pSwapChain->Present(0, DXGI_PRESENT_TEST) == DXGI_STATUS_OCCLUDED)
        {
            ::Sleep(10);
            continue;
        }
        g_SwapChainOccluded = false;

        // Handle window resize (we don't resize directly in the WM_SIZE handler)
        if (g_ResizeWidth != 0 && g_ResizeHeight != 0)
        {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, g_ResizeWidth, g_ResizeHeight, DXGI_FORMAT_UNKNOWN, 0);
            g_ResizeWidth = g_ResizeHeight = 0;
            CreateRenderTarget();
        }

        // Start the Dear ImGui frame
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        // 0. Show a simple window that we create ourselves. We use a Begin/End pair to create a named window.
        {
            ImGuiViewport* main_vp = ImGui::GetMainViewport();

            ImGui::SetNextWindowPos(main_vp->WorkPos, ImGuiCond_Always);
            ImGui::SetNextWindowSize(main_vp->WorkSize, ImGuiCond_Always);
            // 窗口标志
            ImGuiWindowFlags win_flags = 0;
            //win_flags |= ImGuiWindowFlags_NoMove;         // ❌禁止拖动移动
            win_flags |= ImGuiWindowFlags_NoResize;       // ❌禁止缩放大小
           // win_flags |= ImGuiWindowFlags_NoCollapse;     // ❌禁止折叠（去掉右上角最小化按钮）
            // win_flags |= ImGuiWindowFlags_NoTitleBar;    // 可选：要不要标题栏；如果要保留标题栏就不要这个flag
            win_flags |= ImGuiWindowFlags_NoDocking;

            static float f = 0.0f;
            static int counter = 0;
            bool bOpen = true;
            ImGui::Begin("-AppBox-", &bOpen, win_flags);     

            // ==================== 🚀 修正版：Win32 主視窗跟隨折疊縮放（強制置底） ====================
            static bool lastCollapsedState = false;
            bool isCollapsed = ImGui::IsWindowCollapsed();

            if (isCollapsed != lastCollapsedState)
            {
                lastCollapsedState = isCollapsed;

                int currentW = (int)(g_WinW * main_scale);
                int targetH = 0;

                if (isCollapsed)
                {
                    // A. 被折疊了：縮小到只剩標題列高度
                    float titleBarHeight = ImGui::GetFontSize() + style.FramePadding.y * 2.0f;
                    targetH = (int)(titleBarHeight);// + style.WindowPadding.y);

                    g_ResizeWidth = 0;
                    g_ResizeHeight = 0;
                }
                else
                {
                    // B. 被展開了：還原回原本完整的物理高度
                    targetH = (int)(g_WinH * main_scale);
                }

                // 💡 核心修正：
                // 1. 第二個參數不能傳 NULL，必須強制鎖定為 HWND_BOTTOM
                // 2. 標記加上 SWP_NOOWNERZORDER，徹底禁止 Windows 調整其與桌面管理器的 Owner 層級關係
                ::SetWindowPos(hwnd, HWND_BOTTOM, 0, 0, currentW, targetH,
                    SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
            }
            // =====================================================================================


            // 如果被折疊了，後半段的按鈕網格和文字渲染就不用跑了（ImGui 內部會自動跳過，但我們這裡加個防禦）
            if (!isCollapsed)
            {
                //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
               // ImGui::Text(u8"帧率(%.0f FPS)", io.Framerate);

                gTab0.Render();
                gpvApp = &(gvFolder[gCurTab].vApp);

                if (gpvApp)
                {
                    int appIdx = 0;
                    float fSize = 64;
                    iconSize = ImVec2(fSize, fSize);

                    // 1. 計算一個完整按鈕所需的固定總寬度（包含按鈕內襯）
                    float buttonWidth = iconSize.x + style.FramePadding.x * 2.0f;

                    // 2. 取得當前視窗內容的可用總寬度
                    float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

                    // 3. 計算最多能塞下幾個按鈕
                    int maxItemsPerRow = (int)((windowWidth + style.ItemSpacing.x) / (buttonWidth + style.ItemSpacing.x));
                    if (maxItemsPerRow < 1) maxItemsPerRow = 1;

                    // 4. 動態計算「橫向等距間距」
                    float dynamicSpacingX = style.ItemSpacing.x;
                    if (maxItemsPerRow > 1 && gpvApp->size() >= (size_t)maxItemsPerRow)
                    {
                        float totalButtonsWidth = maxItemsPerRow * buttonWidth;
                        dynamicSpacingX = (windowWidth - totalButtonsWidth) / (maxItemsPerRow - 1);
                    }

                    // 5. 💡 關鍵：用 Style 統一注入橫向與縱向間距，讓 ImGui 自動處理行間距！
                    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(dynamicSpacingX, dynamicSpacingX));

                    for (size_t i = 0; i < gpvApp->size(); ++i)
                    {
                        auto& a = (*gpvApp)[i];

                        char szName[64] = { 0 };
                        sprintf_s(szName, "btn%d", appIdx);
                        appIdx++;

                        // 6. 💡 核心排版：由 ImGui 決定換行，完全不使用 SetCursorPos！
                        int col = (int)(i % maxItemsPerRow);
                        if (i > 0 && col > 0)
                        {
                            // 如果不是一行的第一個按鈕，就強行並排，並帶入我們計算好的動態間距
                            ImGui::SameLine(0.0f, dynamicSpacingX);
                        }

                        // 7. 繪製 ImageButton
                        {
                            if (ImGui::ImageButton(szName, (ImTextureID)a.iconSrv, iconSize))
                            {
                                ShellExecuteW(hwnd, L"open", a.exePathBuf, nullptr, nullptr, SW_SHOW);
                            }
                        }

                        // 8. 懸停 Tooltip
                        if (ImGui::IsItemHovered())
                        {
                            std::string sTip;
                            int size_needed = WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, NULL, 0, NULL, NULL);
                            if (size_needed > 0) {
                                sTip.resize(size_needed - 1);
                                WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, &sTip[0], size_needed, NULL, NULL);
                            }
                            if (IsPath(sTip))
                            {
                                ImGui::SetTooltip(sTip.c_str());
                            }
                        }
                    }

                    // 9. 💡 記得彈出剛才 Push 的樣式變數
                    ImGui::PopStyleVar();
                }
            }

            ImGui::End();

            if (!bOpen)
            {
                // 这里你可以选择：关闭MFC对话框
                ::PostMessage(hwnd, WM_CLOSE, 0, 0);
            }
        }


        // Rendering
        ImGui::Render();
        const float clear_color_with_alpha[4] = { clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w };
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clear_color_with_alpha);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

#if ENABLE_VIEWPORTS
        // Update and Render additional Platform Windows
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();

            // g_hWnd：你的主窗口HWND（example里CreateWindowW返回的hwnd变量）
            ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
            for (int i = 0; i < platform_io.Viewports.Size; i++)
            {
                ImGuiViewport* vp = platform_io.Viewports[i];
                if (vp == ImGui::GetMainViewport())
                    continue; // 跳过主视口

                // ✅直接读取公开的PlatformHandleRaw获取HWND，不碰私有结构体
                HWND hImGuiWnd = reinterpret_cast<HWND>(vp->PlatformHandleRaw);
                if (!hImGuiWnd || !::IsWindow(hImGuiWnd))
                    continue;

                // 把ImGui分离窗口放到主窗口之上，非全局置顶，不改动位置大小，不抢焦点
                ::SetWindowPos(
                    hImGuiWnd,
                    HWND_TOP,//hwnd,
                    0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE
                );
            }
        }
#endif

        // Present
        HRESULT hr = g_pSwapChain->Present(1, 0);   // Present with vsync
        //HRESULT hr = g_pSwapChain->Present(0, 0); // Present without vsync
        g_SwapChainOccluded = (hr == DXGI_STATUS_OCCLUDED);
    }

    // Cleanup
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);

    for (auto b : gvFolder)
    {
        for (auto a : b.vApp)
        {
            if (a.iconSrv)
            {
                a.iconSrv->Release();
                a.iconSrv = nullptr;
            }
        }
    }
    CoUninitialize();
    return 0;
}

// Helper functions
bool CreateDeviceD3D(HWND hWnd)
{
    // Setup swap chain
    // This is a basic setup. Optimally could use e.g. DXGI_SWAP_EFFECT_FLIP_DISCARD and handle fullscreen mode differently. See #8979 for suggestions.
    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory(&sd, sizeof(sd));
    sd.BufferCount = 2;
    sd.BufferDesc.Width = 0;
    sd.BufferDesc.Height = 0;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    UINT createDeviceFlags = 0;
    //createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
    D3D_FEATURE_LEVEL featureLevel;
    const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) // Try high-performance WARP software driver if hardware is not available.
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, createDeviceFlags, featureLevelArray, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res != S_OK)
        return false;

    // Disable DXGI's default Alt+Enter fullscreen behavior.
    // - You are free to leave this enabled, but it will not work properly with multiple viewports.
    // - This must be done for all windows associated to the device. Our DX11 backend does this automatically for secondary viewports that it creates.
    IDXGIFactory* pSwapChainFactory;
    if (SUCCEEDED(g_pSwapChain->GetParent(IID_PPV_ARGS(&pSwapChainFactory))))
    {
        pSwapChainFactory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER);
        pSwapChainFactory->Release();
    }

    CreateRenderTarget();
    return true;
}

void CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Win32 message handler
// You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
// - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
// - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
// Generally you may always pass all inputs to dear imgui, and hide them from your application based on those two flags.
#define ID_BTN_OPENMFC  1001
#define ID_BTN_CLOSEMFC 1002
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam))
        return true;

    switch (msg)
    {
#if NEW_PLAN
        // ==================== 🚀 終極釘死最底層防禦機制（不擋圖標） ====================
    case WM_WINDOWPOSCHANGING:
    {
        WINDOWPOS* wp = (WINDOWPOS*)lParam;

        // 💡 核心魔法：無論是誰（包含 ImGui 點擊、視窗聚焦）試圖把這個視窗提到最前，
        // 我們都強制將它的插入順序（InsertAfter）覆寫為 HWND_BOTTOM。
        // 這能確保它在外觀上與行為上，在點擊發生的同時，永遠死死地躺在桌面圖標的下方！
        wp->hwndInsertAfter = HWND_BOTTOM;
        wp->flags &= ~SWP_NOZORDER; // 確保 Z-Order 改變被強制執行
        break;
    }
    // ===============================================================================
#endif
    case WM_NCHITTEST:
    {
        // 获取屏幕坐标
        POINT pt = { LOWORD(lParam), HIWORD(lParam) };
        // 交给ImGui判断当前点是否在ImGui窗口标题栏
        ImGuiIO& io = ImGui::GetIO();
        ImVec2 imgPt = ImVec2((float)pt.x, (float)pt.y);
        ImGuiWindow* pWin = ImGui::FindWindowByName("-AppBox-");
        
        bool bCaption = false;
        if (pWin) 
        {
            ImRect a = pWin->TitleBarRect();
            float h = a.GetHeight();
            a.Min.x += h;
            a.Max.x -= h;
            if (a.Contains(imgPt))
                bCaption = true;
        }
 
        if (bCaption)
        {
            return HTCAPTION;
        }
        return HTCLIENT;
    }
    case WM_CREATE:
    {
    }
    break;
    case WM_SIZE:
        if (wParam == SIZE_MINIMIZED)
            return 0;
        g_ResizeWidth = (UINT)LOWORD(lParam); // Queue resize
        g_ResizeHeight = (UINT)HIWORD(lParam);
        return 0;
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        if (LOWORD(wParam) == ID_BTN_OPENMFC)
        {
            
            break;
        }
        break;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
