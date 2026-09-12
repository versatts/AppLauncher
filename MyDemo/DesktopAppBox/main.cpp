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
std::vector<ST_APP> gvApp;
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
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"", WS_POPUP | /*WS_OVERLAPPEDWINDOW | */WS_CLIPCHILDREN
        , (cx - g_WinW) / 2, (cy - g_WinH) / 2, (int)(g_WinW * main_scale), (int)(g_WinH * main_scale), nullptr, nullptr, wc.hInstance, nullptr);

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
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;       // Enable Multi-Viewport / Platform Windows
    
    //io.ConfigViewportsNoAutoMerge = true;
    //io.ConfigViewportsNoTaskBarIcon = true;
    //io.ConfigDockingAlwaysTabBar = true;
    //io.ConfigDockingTransparentPayload = true;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = main_scale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 0.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.f;
    }

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    auto lnkList = EnumLnkFilesInAppDir();
    for (const auto& lnkPath : lnkList)
    {
        // lnkPath 是完整路径，直接传给 ResolveLnkTarget
        ST_APP item;
        if (ResolveLnkTarget(lnkPath.c_str(), item.exePathBuf, MAX_PATH))
        {
            HICON hIco = ExtractExeMainIcon(item.exePathBuf);
            UINT w, h;
            item.iconSrv = LoadHighestResIconSRV(g_pd3dDevice, item.exePathBuf, w, h);
            
            iconSize = ImVec2((float)w, (float)h);
            DestroyIcon(hIco); // HICON用完释放

            gvApp.push_back(item);
        }
    }

    // Load Fonts
    // - If fonts are not explicitly loaded, Dear ImGui will select an embedded font: either AddFontDefaultVector() or AddFontDefaultBitmap().
    //   This selection is based on (style.FontSizeBase * style.FontScaleMain * style.FontScaleDpi) reaching a small threshold.
    // - You can load multiple fonts and use ImGui::PushFont()/PopFont() to select them.
    // - If a file cannot be loaded, AddFont functions will return a nullptr. Please handle those errors in your code (e.g. use an assertion, display an error and quit).
    // - Read 'docs/FONTS.md' for more instructions and details.
    // - Use '#define IMGUI_ENABLE_FREETYPE' in your imconfig file to use FreeType for higher quality font rendering.
    // - Remember that in C/C++ if you want to include a backslash \ in a string literal you need to write a double backslash \\ !
    // ==================== 修正後的 Load Fonts 區段 ====================

    // 1. 如果需要加載 ImGui 預設的英文型態字體，呼叫 AddFontDefault() 即可（非必須）
    // io.Fonts->AddFontDefault(); 

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
            //win_flags |= ImGuiWindowFlags_NoCollapse;     // ❌禁止折叠（去掉右上角最小化按钮）
            // win_flags |= ImGuiWindowFlags_NoTitleBar;    // 可选：要不要标题栏；如果要保留标题栏就不要这个flag
            win_flags |= ImGuiWindowFlags_NoDocking;

            static float f = 0.0f;
            static int counter = 0;
            bool bOpen = true;
            ImGui::Begin("-AppBox-", &bOpen, win_flags);     
            //ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::Text(u8"帧率(%.0f FPS)", io.Framerate);


            int appIdx = 0;
            float fSize = 64;
            iconSize = ImVec2(fSize, fSize);

            ImGuiStyle& style = ImGui::GetStyle();

            // 1. 計算一個完整按鈕所需的固定總寬度（包含按鈕內襯）
            float buttonWidth = iconSize.x + style.FramePadding.x * 2.0f;

            // 2. 取得當前視窗內容的可用總寬度
            float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

            // 3. 計算最多能塞下幾個按鈕
            int maxItemsPerRow = (int)((windowWidth + style.ItemSpacing.x) / (buttonWidth + style.ItemSpacing.x));
            if (maxItemsPerRow < 1) maxItemsPerRow = 1;

            // 4. 動態計算「橫向等距間距」
            float dynamicSpacingX = style.ItemSpacing.x;
            if (maxItemsPerRow > 1 && gvApp.size() >= (size_t)maxItemsPerRow)
            {
                float totalButtonsWidth = maxItemsPerRow * buttonWidth;
                dynamicSpacingX = (windowWidth - totalButtonsWidth) / (maxItemsPerRow - 1);
            }

            // 5. 💡 關鍵：用 Style 統一注入橫向與縱向間距，讓 ImGui 自動處理行間距！
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(dynamicSpacingX, dynamicSpacingX));

            for (size_t i = 0; i < gvApp.size(); ++i)
            {
                auto& a = gvApp[i];

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
                if (ImGui::ImageButton(szName, (ImTextureID)a.iconSrv, iconSize))
                {
                    ShellExecuteW(hwnd, L"open", a.exePathBuf, nullptr, nullptr, SW_SHOW);
                }

                // 8. 懸停 Tooltip
                if (ImGui::IsItemHovered())
                {
                    ImGui::SetTooltip("\xE8\xB7\xAF\xE5\xBE\x91\xEF\xBC\x9A %ls", a.exePathBuf);
                }
            }

            // 9. 💡 記得彈出剛才 Push 的樣式變數
            ImGui::PopStyleVar();
           
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

    for (auto a : gvApp)
    {
        if (a.iconSrv)
        {
            a.iconSrv->Release();
            a.iconSrv = nullptr;
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
            pWin->TitleBarRect().Contains(imgPt);
            ImRect a = pWin->TitleBarRect();
            int h = a.GetHeight();
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
