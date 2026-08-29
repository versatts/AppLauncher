#include "pch.h"
#include "ImGuiWndBase.h"

// ImGui Win32 外部声明
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern ImguiPack g_GUI;
ATOM ImGuiWndBase::m_wndClassAtom = 0;

BEGIN_MESSAGE_MAP(ImGuiWndBase, CWnd)
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_NCHITTEST()
	ON_MESSAGE(MY_IMGUI_RENDER, &ImGuiWndBase::OnRender)
END_MESSAGE_MAP()

ImGuiWndBase::ImGuiWndBase(CWnd* pParent)
	:CWnd()
	, m_pParentWnd(pParent)

{
	m_rcMargin = CRect(0, 0, 0, 0);
}

BOOL ImGuiWndBase::CreateWnd(const CRect& rc, CWnd* pParentWnd)
{
	// 注册窗口类，仅注册一次
	if (m_wndClassAtom == 0)
	{
		WNDCLASSEX wcex = { 0 };
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.lpfnWndProc = AfxWndProc;
		wcex.hInstance = AfxGetInstanceHandle();
		wcex.lpszClassName = _T("MsgBox_CustomWnd");
		wcex.hCursor = AfxGetApp()->LoadStandardCursor(IDC_ARROW);
		wcex.style = CS_HREDRAW | CS_VREDRAW;

		m_wndClassAtom = ::RegisterClassEx(&wcex);
		ASSERT(m_wndClassAtom);
	}

	DWORD dwStyle = WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN;
	dwStyle &= ~(WS_CAPTION/* | WS_BORDER | WS_THICKFRAME*/);

	BOOL bOk = CreateEx(
		0,
		_T("MsgBox_CustomWnd"),
		nullptr,
		dwStyle,
		rc.left, rc.top, rc.Width(), rc.Height(),
		pParentWnd ? pParentWnd->GetSafeHwnd() : nullptr,
		nullptr,
		AfxGetInstanceHandle()
	);

	return bOk;
}
void ImGuiWndBase::OnDestroy()
{
	CWnd::OnDestroy();
	Stop(true);
}

void ImGuiWndBase::Stop(bool bDelete)
{
	if (bDelete)
	{
		g_GUI.DelWnd(this);
	}

	ReleaseImgui();
	
	if (bDelete)
	{
		g_GUI.ContinueRender(false);
	}
}

LRESULT ImGuiWndBase::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	if (m_bImGuiInited)
	{
		// 只传给handler，但！绝不return它的返回值！！
		// ImGui_ImplWin32 内部会读取GetCursorPos得到鼠标，不需要靠窗口消息返回拦截
		ImGui_ImplWin32_WndProcHandler(m_hWnd, message, wParam, lParam);
	}
	return CWnd::WindowProc(message, wParam, lParam);
}

LRESULT ImGuiWndBase::OnNcHitTest(CPoint point)
{
	// point: 屏幕坐标
	CRect rcWnd;
	GetWindowRect(&rcWnd);

	if (m_bResize)
	{
		// -------- 1.优先检测缩放边缘 --------
		const int borderSize = 8;
		bool left = (point.x <= rcWnd.left + borderSize);
		bool right = (point.x >= rcWnd.right - borderSize);
		bool top = (point.y <= rcWnd.top + borderSize);
		bool bottom = (point.y >= rcWnd.bottom - borderSize);

		if (top && left)		return HTTOPLEFT;
		if (top && right)		return HTTOPRIGHT;
		if (bottom && left)		return HTBOTTOMLEFT;
		if (bottom && right)	return HTBOTTOMRIGHT;
		if (top)				return HTTOP;
		if (bottom)				return HTBOTTOM;
		if (left)				return HTLEFT;
		if (right)				return HTRIGHT;
	}

	// -------- 2.边缘之外，执行原有ImGui标题栏拖拽逻辑 --------
	if (!m_bImGuiInited)
		return CWnd::OnNcHitTest(point);

	ImVec2 pt((float)point.x, (float)point.y);
	if (!m_imguiTitleBarScreenRect.Contains(pt))
	{
		return HTCLIENT;
	}

	const float closeBtnWidth = 24.0f;
	ImRect closeButtonRect = m_imguiTitleBarScreenRect;
	closeButtonRect.Min.x = closeButtonRect.Max.x - closeBtnWidth;
	if (closeButtonRect.Contains(pt))
	{
		return HTCLIENT;
	}
	// 标题栏空白，允许拖动窗口整体
	return HTCAPTION;
}

void ImGuiWndBase::OnSize(UINT nType, int cx, int cy)
{
	CWnd::OnSize(nType, cx, cy);
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
void ImGuiWndBase::ReleaseImgui()
{
	if (m_bImGuiInited)
	{
		ImGui_ImplWin32_Shutdown();
		m_bImGuiInited = false;
	}

	if (m_pRenderTargetView) { m_pRenderTargetView->Release(); m_pRenderTargetView = nullptr; }
	if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
}

HRESULT ImGuiWndBase::CreateSwapChainAndRTV(HWND hWnd, ID3D11Device* pGlobalDevice)
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

void ImGuiWndBase::Start(bool bAdd)
{
	if (bAdd)
	{
		g_GUI.AddWnd(this);
	}

	m_pd3dDevice = g_GUI.m_pd3dDevice;
	m_pd3dDeviceContext = g_GUI.m_pd3dDeviceContext;
	// ImplWin32 使用对话框HWND
	CreateSwapChainAndRTV(m_hWnd, m_pd3dDevice);
	ImGui_ImplWin32_Init(m_hWnd);
	m_bImGuiInited = true;

	if (bAdd)
	{
		g_GUI.ContinueRender(true);
	}
}

void ImGuiWndBase::Render()
{
	this->ImGuiRender_Begin();
	this->ImGuiRender_Main_Pre();
	this->ImGuiRender_Main();
	this->ImGuiRender_Main_Post();
	this->ImGuiRender_End();
}

void ImGuiWndBase::ImGuiRender_Begin()
{
	//float clearColor[4] = { 204.0f / 255.f, 206.0f / 255.f, 219.0f / 255.f, 0.5f };
	//m_pd3dDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clearColor);

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;


	// ===== 关键：读取CWnd真实客户区，强制覆盖DisplaySize =====
	CRect clientRc;
	GetClientRect(&clientRc);
	m_clientW = (float)clientRc.Width();
	m_clientH = (float)clientRc.Height();
	io.DisplaySize = ImVec2(m_clientW, m_clientH);

/*********************************/

	ImDrawList* bg_drawlist = ImGui::GetBackgroundDrawList();

	// 整个CDialog客户区范围
	ImVec2 clientMin(0, 0);
	ImVec2 clientMax(io.DisplaySize.x, io.DisplaySize.y);

	// margin内框（你的ImGui窗口所在矩形）
	ImVec2 winMin(m_rcMargin.left, m_rcMargin.top);
	ImVec2 winMax(
		m_rcMargin.left + (m_clientW - (m_rcMargin.left + m_rcMargin.right)),
		m_rcMargin.top + (m_clientH - (m_rcMargin.top + m_rcMargin.bottom))
	);

	// 画4块外边：上、下、左、右，就是margin外围颜色
	ImU32 marginOuterColor = ImColor(0xB5, 0xE6, 0x1D); // margin外围颜色#B5E61D
	ImU32 outerBorderColor = ImColor(0, 0, 0);          // 最外层黑线
	float borderThickness = 2.0f;                     // 黑线像素宽度

	// 1. 绘制四周margin填充色块
	bg_drawlist->AddRectFilled(ImVec2(clientMin.x, clientMin.y), ImVec2(clientMax.x, winMin.y), marginOuterColor);
	bg_drawlist->AddRectFilled(ImVec2(clientMin.x, winMax.y), ImVec2(clientMax.x, clientMax.y), marginOuterColor);
	bg_drawlist->AddRectFilled(ImVec2(clientMin.x, winMin.y), ImVec2(winMin.x, winMax.y), marginOuterColor);
	bg_drawlist->AddRectFilled(ImVec2(winMax.x, winMin.y), ImVec2(clientMax.x, winMax.y), marginOuterColor);

	// 2. 在【整个客户区最外围】画一圈黑色边框
	bg_drawlist->AddRect(clientMin, clientMax, outerBorderColor, 0.0f, 0, borderThickness);
/***********************************/


}


void ImGuiWndBase::ImGuiRender_Main()
{
//	ImGui::Text(u8"Empty window");

	// 处理关闭叉：点击关闭叉 m_imguiWindowOpen=false
	if (m_bHasTitleBar && !m_imguiWindowOpen)
	{
		// 这里你可以选择：关闭MFC对话框
		::PostMessage(GetSafeHwnd(), WM_CLOSE, 0, 0);
	}
	// ===========================================

}

void ImGuiWndBase::ImGuiWinBegin(const char* szTitle)
{
	// ImGui窗口
	ImGui::SetNextWindowPos(ImVec2(m_rcMargin.left, m_rcMargin.top), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(m_clientW - (m_rcMargin.left + m_rcMargin.right), m_clientH - (m_rcMargin.top + m_rcMargin.bottom)), ImGuiCond_Always);


	ImGuiWindowFlags flags = 0;
	if (!m_bHasTitleBar)
	{
		flags |= ImGuiWindowFlags_NoTitleBar;   // 移除标题栏
	}
	flags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;// ImGui窗口 
	flags |= ImGuiWindowFlags_NoMove;//禁移动
	flags |= ImGuiWindowFlags_NoResize;
	flags |= ImGuiWindowFlags_NoCollapse;//禁折叠
	flags |= ImGuiWindowFlags_NoScrollbar;
	ImGui::Begin(szTitle, &m_imguiWindowOpen, flags);

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

void ImGuiWndBase::ImGuiWinEnd()
{
	ImGui::End();
}

void ImGuiWndBase::ImGuiRender_End()
{
	ImGui::Render();

	// D3D Clear
	const float clear_color[] = { 0.2f,0.2f,0.2f,1.0f };
	m_pd3dDeviceContext->OMSetRenderTargets(1, &m_pRenderTargetView, nullptr);
	m_pd3dDeviceContext->ClearRenderTargetView(m_pRenderTargetView, clear_color);

	// ImGui绘制
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	// 交换链呈现
	m_pSwapChain->Present(1, 0);
}

int ImGuiWndBase::DoModal(bool bModal, CRect* rc)
{
	ASSERT(m_hWnd == nullptr);

	// 默认窗口大小位置，你也可以外部修改m_rcWnd
	if (rc)
		m_rcWnd = *rc;

	if (!CreateWnd(m_rcWnd, m_pParentWnd))
	{
		return -1;
	}

	Start(true);

	CenterWindow();
	//after create completed
	::PostMessage(m_hWnd, MY_IMGUI_AFTER_CREATE, 0, 0);


	if (bModal)
	{
		// 模态消息循环
		MSG msg{ 0 };
		while (::IsWindow(m_hWnd) && ::GetMessage(&msg, nullptr, 0, 0))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
		return m_nRetCode;
	}
	return 0;
}

void ImGuiWndBase::EndModal(int nRetCode)
{
	m_nRetCode = nRetCode;
	if (IsWindow(m_hWnd))
	{
		DestroyWindow();
	}
}

LRESULT ImGuiWndBase::OnRender(WPARAM, LPARAM)
{
	Render();
	return 0;
}
