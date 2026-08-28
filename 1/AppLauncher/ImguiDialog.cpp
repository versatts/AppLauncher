#include "pch.h"
#include "ImguiDialog.h"
#include "ImguiPack.h"
#define IDD_IMGUI_TEMP 10000

// ImGui Win32 外部声明
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);



BEGIN_MESSAGE_MAP(ImguiDialog, CDialogEx)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_NCHITTEST()

END_MESSAGE_MAP()

ImguiDialog::ImguiDialog(DWORD resID, CWnd* pParent)
	:CDialogEx(resID, pParent)
{

}

BOOL ImguiDialog::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	InitDialog();
	return TRUE;
}
void ImguiDialog::OnDestroy()
{
	CDialogEx::OnDestroy();

	m_threadRun = false;
	m_renderThread.join();

	ReleaseImgui();
}

LRESULT ImguiDialog::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_bImGuiInited)
	{
		// 只传给handler，但！绝不return它的返回值！！
		// ImGui_ImplWin32 内部会读取GetCursorPos得到鼠标，不需要靠窗口消息返回拦截
		ImGui_ImplWin32_WndProcHandler(m_hWnd, message, wParam, lParam);
	}
	return CDialogEx::WindowProc(message, wParam, lParam);
}

LRESULT ImguiDialog::OnNcHitTest(CPoint point)
{

	if (!m_bImGuiInited)
		return CDialogEx::OnNcHitTest(point);

	ImVec2 pt((float)point.x, (float)point.y);

	// 鼠标不在标题栏范围内，交给基类处理
	if (!m_imguiTitleBarScreenRect.Contains(pt))
	{
		return CDialogEx::OnNcHitTest(point);
	}

	// -------- 仅右上角关闭按钮区域 --------
	const float closeBtnWidth = 24.0f;
	ImRect closeButtonRect = m_imguiTitleBarScreenRect;
	// X按钮：标题栏最右边往左24像素
	closeButtonRect.Min.x = closeButtonRect.Max.x - closeBtnWidth;

	// 如果鼠标落在关闭按钮区域 → HTCLIENT，把点击交给ImGui，X才能点动
	if (closeButtonRect.Contains(pt))
	{
		return HTCLIENT;
	}

	// 标题栏其余空白部分：交给Windows实现拖动MFC窗口
	return HTCAPTION;
}

void ImguiDialog::OnSize(UINT nType, int cx, int cy)
{
	CDialogEx::OnSize(nType, cx, cy);
	if (!m_bImGuiInited || cx <= 0 || cy <= 0)
		return;

	if (m_pRenderTargetView)
	{
		m_pRenderTargetView->Release();
		m_pRenderTargetView = nullptr;
	}
	m_pSwapChain->ResizeBuffers(1, cx, cy, DXGI_FORMAT_R8G8B8A8_UNORM, 0);

	ID3D11Texture2D* pBackBuffer = nullptr;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
	pBackBuffer->Release();
}
void ImguiDialog::ReleaseImgui()
{
	if (m_bImGuiInited)
	{
		ImGui_ImplWin32_Shutdown();
		m_bImGuiInited = false;
	}

	if (m_pRenderTargetView) { m_pRenderTargetView->Release(); m_pRenderTargetView = nullptr; }
	if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
}

HRESULT ImguiDialog::CreateSwapChainAndRTV(HWND hWnd, ID3D11Device* pGlobalDevice)
{
	// 1. 获取 IDXGIFactory
	IDXGIFactory1* pFactory = nullptr;
	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&pFactory);
	if (FAILED(hr)) return hr;

	// 2. 填充交换链描述，绑定当前对话框hWnd
	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 2;
	sd.BufferDesc.Width = 0;   //0=跟随窗口
	sd.BufferDesc.Height = 0;
	sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 0;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = hWnd;     // ✅绑定对话框窗口句柄
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;
	sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags = 0;

	// 3. 创建SwapChain，使用【全局的g_pd3dDevice】
	hr = pFactory->CreateSwapChain(pGlobalDevice, &sd, &m_pSwapChain);
	pFactory->Release();
	if (FAILED(hr)) return hr;

	//4. 从SwapChain拿到BackBuffer，创建RTV
	ID3D11Texture2D* pBackBuffer = nullptr;
	hr = m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	if (FAILED(hr)) return hr;

	hr = pGlobalDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
	pBackBuffer->Release();

	return hr;
}

void ImguiDialog::InitDialog()
{
	extern ImguiPack g_GUI;
	m_pd3dDevice = g_GUI.m_pd3dDevice;
	m_pd3dDeviceContext = g_GUI.m_pd3dDeviceContext;
	// ImplWin32 使用对话框HWND
	CreateSwapChainAndRTV(m_hWnd, m_pd3dDevice);
	ImGui_ImplWin32_Init(m_hWnd);
	m_bImGuiInited = true;


	// 启动渲染工作线程
	m_threadRun = true;
	m_renderThread = std::thread([this]()
		{
			while (m_threadRun)
			{
				this->ImGuiRenderFrame_Start();
				this->ImGuiRenderFrame();
				this->ImGuiRenderFrame_End();
				this->ImGuiRenderFrame_PostProc();

				// 限速 ~60fps，16ms。这里Sleep没问题，是子线程sleep，不卡UI主线程
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
		});
}


void ImguiDialog::ImGuiRenderFrame_Start()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;

	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);     // 永远贴客户区左上角

	ImGuiWindowFlags flags = 0;
	if (!m_bHasTitleBar)
	{
		flags |= ImGuiWindowFlags_NoTitleBar;   // 移除标题栏
	}
	flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;// ImGui窗口 禁移动,禁折叠
	ImGui::Begin("Drag Me TitleBar", &m_imguiWindowOpen, flags);

	ImVec2 curWinSize = ImGui::GetWindowSize();

	// 最小尺寸硬保护，杜绝缩没
	const float minW = 100.f;
	const float minH = 100.f;
	if (curWinSize.x < minW) curWinSize.x = minW;
	if (curWinSize.y < minH) curWinSize.y = minH;

	bool sizeChanged = (fabsf(curWinSize.x - m_lastImGuiWinSize.x) > 2.f)
		|| (fabsf(curWinSize.y - m_lastImGuiWinSize.y) > 2.f);
#if 0
	if (sizeChanged)
	{
		m_lastImGuiWinSize = curWinSize;

		CRect wndOuter;
		::GetWindowRect(m_hWnd, &wndOuter);
		CRect wndClient;
		::GetClientRect(m_hWnd, &wndClient);

		int borderLeftRight = wndOuter.Width() - wndClient.Width();
		int borderTopBottom = wndOuter.Height() - wndClient.Height();

		int targetOuterW = (int)curWinSize.x + borderLeftRight;
		int targetOuterH = (int)curWinSize.y + borderTopBottom;

		::SetWindowPos(m_hWnd, nullptr,
			wndOuter.left,
			wndOuter.top,
			targetOuterW,
			targetOuterH,
			SWP_NOZORDER);
	}

#endif
	// ========= 计算标题栏【屏幕坐标矩形】保存给MFC OnNcHitTest使用 =========
	ImVec2 winPosClient = ImGui::GetWindowPos();   // 相对于对话框客户区
	ImVec2 winSize = ImGui::GetWindowSize();
	float titleBarH = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2;

	CPoint clientOrigin = { 0,0 };
	::ClientToScreen(m_hWnd, &clientOrigin);

	m_imguiTitleBarScreenRect.Min.x = clientOrigin.x + winPosClient.x;
	m_imguiTitleBarScreenRect.Min.y = clientOrigin.y + winPosClient.y;
	m_imguiTitleBarScreenRect.Max.x = m_imguiTitleBarScreenRect.Min.x + winSize.x;
	m_imguiTitleBarScreenRect.Max.y = m_imguiTitleBarScreenRect.Min.y + titleBarH;
	// =====================================================================
}


void ImguiDialog::ImGuiRenderFrame_End()
{
	// D3D Clear
	const float clear_color[] = { 0.2f,0.2f,0.2f,1.0f };
	m_pd3dDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);
	m_pd3dDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clear_color);

	// ImGui绘制
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// 交换链呈现
	m_pSwapChain->Present(1, 0);
}

void ImguiDialog::ImGuiRenderFrame_PostProc()
{

}
