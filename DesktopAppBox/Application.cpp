#include "Application.h"
#include <algorithm>
#include "link.h"
#include "AppConfig.h"
#include "imguiobj.h"

// Helper functions
bool Application::CreateDeviceD3D(HWND hWnd)
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

void Application::CleanupDeviceD3D()
{
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

void Application::CreateRenderTarget()
{
    ID3D11Texture2D* pBackBuffer;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
    pBackBuffer->Release();
}

void Application::CleanupRenderTarget()
{
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}

void Application::InitImGuiContext(float fScale)
{
    //    IMGUI_CHECKVERSION();
     //   ImGui::CreateContext();

      //  RegisterWin32IniHandler();



    ImGuiIO& io = ImGui::GetIO();
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
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
    //ImGui::StyleColorsClassic();

    // Setup scaling
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(fScale);        // Bake a fixed style scale. (until we have a solution for dynamic style scaling, changing this requires resetting Style + calling this again)
    style.FontScaleDpi = fScale;        // Set initial font scale. (in docking branch: using io.ConfigDpiScaleFonts=true automatically overrides this for every window depending on the current monitor)
    io.ConfigDpiScaleFonts = true;          // [Experimental] Automatically overwrite style.FontScaleDpi in Begin() when Monitor DPI changes. This will scale fonts but _NOT_ scale sizes/padding for now.
    io.ConfigDpiScaleViewports = true;      // [Experimental] Scale Dear ImGui and Platform Windows when Monitor DPI changes.

    style.FramePadding.x = 4.0f; // 增大，标题栏变高；减小，标题栏变矮
    style.FramePadding.y = 4.0f; // 增大，标题栏变高；减小，标题栏变矮
    style.FrameBorderSize = 0.5f;
    style.FrameRounding = 0;

    style.ScrollbarSize = 7.0f; // 🚀 細滾動條（配合 FolderUI 的自動隱藏滾動條；須在 ScaleAllSizes 之後設定）

    style.TabRounding = style.FramePadding.x * 2; // Rounded tops
    style.TabBorderSize = 0.0f; // 💡 CHANGED: Force 1-pixel frame around the tab headers

    ImVec4* colors = style.Colors;
#if 1
    auto ChangeColorBrightness = [](const ImVec4& color, float factor)->ImVec4 {
        return ImVec4(
            std::clamp(color.x * factor, 0.0f, 1.0f),
            std::clamp(color.y * factor, 0.0f, 1.0f),
            std::clamp(color.z * factor, 0.0f, 1.0f),
            color.w
        );
        };
    // 1. 獲取系統 Windows 強調色字串 (例如 "#4B4B52")
    std::string winColorStr = GetWindowsAccentColor();
    ImVec4 accClr = IMVEC4(winColorStr);

    // 2. 核心背景色設置
    // 視窗背景 (WindowBg) 與 標題列 (TitleBg) 都套用系統主色
    colors[ImGuiCol_WindowBg] = ChangeColorBrightness(accClr, 0.5);
    colors[ImGuiCol_FrameBg] = ChangeColorBrightness(accClr, 0.8);
    //colors[ImGuiCol_Border] = ChangeColorBrightness(accClr, 1.11);

    colors[ImGuiCol_FrameBgHovered] = ChangeColorBrightness(accClr, 0.9 );
    colors[ImGuiCol_FrameBgActive] = ChangeColorBrightness(accClr, 0.8);
        
    colors[ImGuiCol_TitleBg] = ChangeColorBrightness(accClr, 0.3);
    colors[ImGuiCol_TitleBgActive] = accClr;
    colors[ImGuiCol_TitleBgCollapsed] = IMVEC4(winColorStr, 0.4f);

    colors[ImGuiCol_TextDisabled] = accClr;

    colors[ImGuiCol_Tab] = ChangeColorBrightness(accClr, 0.8);
    colors[ImGuiCol_TabActive] = ChangeColorBrightness(accClr, 0.8);
    colors[ImGuiCol_TabHovered] = ChangeColorBrightness(accClr, 1.1);


    // 🚀 隱藏右下角 resize grip 的三條斜線（小三角圖示）：保留可拖拽 resize 的行為，
    //    僅把 grip 顏色設為完全透明，使用者看不到三角形但仍能拖右下角改變大小。
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ResizeGripHovered]     = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_ResizeGripActive]      = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    colors[ImGuiCol_Button] = ChangeColorBrightness(accClr, 0.8);
    colors[ImGuiCol_ButtonActive] = ChangeColorBrightness(accClr, 0.8);
    colors[ImGuiCol_ButtonHovered] = ChangeColorBrightness(accClr, 0.9);

    // 🚀 滾動條顏色：5px 細滾動條需要高對比，正常狀態的滑塊(grab)也要清晰可見
    colors[ImGuiCol_ScrollbarBg] = ChangeColorBrightness(accClr, 0.7);
    colors[ImGuiCol_ScrollbarGrab] = ChangeColorBrightness(accClr, 1.1);
    colors[ImGuiCol_ScrollbarGrabHovered] = ChangeColorBrightness(accClr, 1.1);
    colors[ImGuiCol_ScrollbarGrabActive] = ChangeColorBrightness(accClr, 1.1);

    // 3. 自動計算「文字反差色」
    // 解析出 RGB 的 0.0f - 1.0f 數值
    ImVec4 bg = colors[ImGuiCol_WindowBg];

    // W3C 相對亮度公式 (Relative Luminance)
    // 綠色對人眼最敏感，藍色最不敏感，公式比例：R*0.299 + G*0.587 + B*0.114
    float luminance = (bg.x * 0.299f) + (bg.y * 0.587f) + (bg.z * 0.114f);

    // 如果背景亮度大於 0.5 (偏亮/淺色)，文字就用黑色；反之用白色
    ImVec4 textColor = (luminance > 0.5f) ? ImVec4(0.0f, 0.0f, 0.0f, 1.0f) : ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
    ImVec4 disabledTextColor = ImVec4(textColor.x, textColor.y, textColor.z, 0.5f); // 半透明做為停用文字

    // 4. 套用文字反差色
    colors[ImGuiCol_Text] = textColor;
    colors[ImGuiCol_TextDisabled] = disabledTextColor;
    colors[ImGuiCol_TableHeaderBg] = IMVEC4(winColorStr, 0.8f);
    colors[ImGuiCol_CheckMark] = textColor;
    
#endif


    // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
    {
        style.WindowRounding = 10.0f;
        style.Colors[ImGuiCol_WindowBg].w = 1.f;
    }

    m_vFont;
    // 2. Load font
    float fontSize = (gCfg.m_nFontSize > 0) ? static_cast<float>(gCfg.m_nFontSize) : 24.0f;

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 2;
    cfg.PixelSnapH = true;

    // ====== 新增：套用粗體與斜體設定 ======
    // 粗體處理：調高光柵化乘數來加粗線條（預設為 1.0f，1.3f~1.5f 左右有明顯的加粗效果）
    if (gCfg.m_bFontBold)
    {
        cfg.RasterizerMultiply = 1.35f;
    }

    std::string sFontPath = "C:\\Windows\\Fonts\\";
    std::string sFont = sFontPath + gCfg.m_sEnFont;

    std::string sFallback = sFontPath + "msyhbd.ttc"; // 微軟雅黑
    ImFont* font0 = io.Fonts->AddFontFromFileTTF(sFallback.c_str(), fontSize, &cfg, io.Fonts->GetGlyphRangesChineseFull());
    m_vFont.push_back(font0);

    // 檢查檔案是否存在，避免因設定錯誤字體導致崩潰
    ImFont* fontDefault = nullptr;
    float fs = fontSize;
    for (int i = 0; i < 5; i++)
    {
        ImFont* font = nullptr;
        if (fontSize > i * 2)
        {
            fs = fontSize - i * 2;
        }
        cfg.MergeMode = false;
        cfg.GlyphOffset.y = 0;

        DWORD attrib = GetFileAttributesA(sFont.c_str());
        if (attrib != INVALID_FILE_ATTRIBUTES && !(attrib & FILE_ATTRIBUTE_DIRECTORY))
        {
            font = io.Fonts->AddFontFromFileTTF(sFont.c_str(), fs, &cfg);
        }
        else
        {
            std::string sFallback = sFontPath + "msyhbd.ttc"; // 微軟雅黑
            font = io.Fonts->AddFontFromFileTTF(sFallback.c_str(), fs, &cfg, io.Fonts->GetGlyphRangesChineseFull());
        }
        if (i == 0)
        {
            fontDefault = font;
        }
        else
        {
            m_vFont.push_back(font);
        }

        cfg.MergeMode = true; 
        cfg.GlyphOffset.y = -1.f;
        io.Fonts->AddFontFromFileTTF( "C:\\Windows\\Fonts\\msyhbd.ttc", fs, &cfg, io.Fonts->GetGlyphRangesChineseFull());
    }
    
    if (fontDefault != nullptr) {
        io.FontDefault = fontDefault; // 設置全局預設字體
    }
    else {
        // 如果加載失敗的防禦方案：使用系統預設
        fontDefault = io.Fonts->AddFontDefault();
    }

    m_vFont.push_back(fontDefault);

    //  io.Fonts->Build();
}
