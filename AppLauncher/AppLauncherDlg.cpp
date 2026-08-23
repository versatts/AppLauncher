
// AppLauncherDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "AppLauncher.h"
#include "AppLauncherDlg.h"
#include "afxdialogex.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ImGui Win32 外部声明
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define WM_IMGUI_RENDER (WM_USER + 1001)
#define WM_POP_MSG (WM_USER+1002)
// 用于应用程序“关于”菜单项的 CAboutDlg 对话框

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_ABOUTBOX };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV 支持

// 实现
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CAppLauncherDlg 对话框



CAppLauncherDlg::CAppLauncherDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_APPLAUNCHER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CAppLauncherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAppLauncherDlg, CDialogEx)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
	ON_WM_NCHITTEST()
	ON_MESSAGE(WM_IMGUI_RENDER, &CAppLauncherDlg::OnImGuiRender)
	ON_MESSAGE(WM_POP_MSG, &CAppLauncherDlg::OnPopMsgBox)
	ON_WM_SIZE()
END_MESSAGE_MAP()


// CAppLauncherDlg 消息处理程序

BOOL CAppLauncherDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 将“关于...”菜单项添加到系统菜单中。

	// IDM_ABOUTBOX 必须在系统命令范围内。
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != nullptr)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码

	// 创建D3D11
	CreateD3D11Resources();

	// 初始化ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImFontConfig cfg;
	cfg.OversampleH = 2;
	cfg.OversampleV = 2;
	cfg.PixelSnapH = true;

	// 加载微软雅黑，字号17，中文完整字符集
	ImFont* fontYaHei = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\msyh.ttc",
		17.0f,
		&cfg,
		io.Fonts->GetGlyphRangesChineseFull()
	);
	IM_ASSERT(fontYaHei != nullptr); // 失败直接断言，排查路径问题
	io.FontDefault = fontYaHei; // 设置全局默认字体，全部UI生效

	ImGui::StyleColorsLight();

	ImGuiStyle& style = ImGui::GetStyle();

	style.FramePadding.x = 4.0f; // 增大，标题栏变高；减小，标题栏变矮
	style.FramePadding.y = 4.0f; // 增大，标题栏变高；减小，标题栏变矮

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.30f, 1.0f); // 窗口未激活标题栏
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.80f, 0.70f, 0.90f, 1.0f); // 当前激活窗口标题栏
	colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.15f, 0.15f, 0.15f, 0.6f); // 窗口折叠后的标题栏

	// ImplWin32 使用对话框HWND
	ImGui_ImplWin32_Init(m_hWnd);
	ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);

	m_bImGuiInited = true;

	// 启动渲染工作线程
	m_threadRun = true;
	m_renderThread = std::thread([this]()
		{
			while (m_threadRun)
			{
				// ⚠子线程：只发消息，不做任何渲染！渲染在主线程消息函数里
				::PostMessage(m_hWnd, WM_IMGUI_RENDER, 0, 0);

				// 限速 ~60fps，16ms。这里Sleep没问题，是子线程sleep，不卡UI主线程
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
		});

	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

void CAppLauncherDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CAppLauncherDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
	//	CDialogEx::OnPaint();
		// 注意：因为D3D直接绘制窗口，这里不要调用CDialog::OnPaint()，避免MFC GDI覆盖D3D画面
		CPaintDC dc(this);
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CAppLauncherDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}


void CAppLauncherDlg::CreateD3D11Resources()
{

	DXGI_SWAP_CHAIN_DESC sd = {};
	sd.BufferCount = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_hWnd;
	sd.SampleDesc.Count = 1;
	sd.Windowed = TRUE;

	UINT createDeviceFlags = 0;
	D3D11CreateDeviceAndSwapChain(
		nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags,
		nullptr, 0, D3D11_SDK_VERSION,
		&sd, &m_pSwapChain, &m_pd3dDevice, nullptr, &m_pd3dDeviceContext);

	// 创建RenderTarget
	ID3D11Texture2D* pBackBuffer = nullptr;
	m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&pBackBuffer);
	m_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &m_pRenderTargetView);
	pBackBuffer->Release();
}

void CAppLauncherDlg::ReleaseD3D11Resources()
{
	if (m_bImGuiInited)
	{
		ImGui_ImplDX11_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
		m_bImGuiInited = false;
	}

	if (m_pRenderTargetView) { m_pRenderTargetView->Release(); m_pRenderTargetView = nullptr; }
	if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
	if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
	if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

LRESULT CAppLauncherDlg::OnImGuiRender(WPARAM wParam, LPARAM lParam)
{
	if (!m_bImGuiInited || !::IsWindow(m_hWnd))
		return 0;

	ImGuiRenderFrame();

	return 0;
}

void CAppLauncherDlg::ImGuiRenderFrame()
{
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();


	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;


	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);     // 永远贴客户区左上角
	// ImGui窗口禁止自身移动
	//ImGui::Begin("Drag Me TitleBar", nullptr, ImGuiWindowFlags_NoMove);
	ImGui::Begin("Drag Me TitleBar", &m_imguiWindowOpen, ImGuiWindowFlags_NoMove| ImGuiWindowFlags_NoCollapse);
	ImGui::ShowMetricsWindow();
#if 1
	ImVec2 curWinSize = ImGui::GetWindowSize();

	// 最小尺寸硬保护，杜绝缩没
	const float minW = 10.f;
	const float minH = 10.f;	
	if (curWinSize.x < minW) curWinSize.x = minW;
	if (curWinSize.y < minH) curWinSize.y = minH;

	bool sizeChanged = (fabsf(curWinSize.x - m_lastImGuiWinSize.x) > 2.f)
		|| (fabsf(curWinSize.y - m_lastImGuiWinSize.y) > 2.f);

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


	ImGui::Text(u8"拖动本窗口标题栏 → MFC对话框整体移动");
	static float f = 0.0f;
	ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
	if (ImGui::Button("MFC+ImGui Test Button"))
	{
		PostMessage(WM_POP_MSG, 0, 0);
	}
	ImGui::End();

	// 处理关闭叉：点击关闭叉 m_imguiWindowOpen=false
	if (!m_imguiWindowOpen)
	{
		// 这里你可以选择：关闭MFC对话框
		PostMessage(WM_CLOSE, 0, 0);
	}
	// ===========================================
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

// 对话框销毁时释放资源
void CAppLauncherDlg::OnDestroy()
{
	CDialogEx::OnDestroy();
	ReleaseD3D11Resources();
}

LRESULT CAppLauncherDlg::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
#if 0
	// ImGui优先处理窗口消息
	if (m_bImGuiInited)
	{
		LRESULT ret = ImGui_ImplWin32_WndProcHandler(m_hWnd, message, wParam, lParam);
		if (ret != 0)
		{
			return ret; // ImGui已经消化这条消息，不再传给MFC
		}
	}
	return CDialogEx::WindowProc(message, wParam, lParam);
#endif


	if (m_bImGuiInited)
	{
		// 只传给handler，但！绝不return它的返回值！！
		// ImGui_ImplWin32 内部会读取GetCursorPos得到鼠标，不需要靠窗口消息返回拦截
		ImGui_ImplWin32_WndProcHandler(m_hWnd, message, wParam, lParam);
	}
	return CDialogEx::WindowProc(message, wParam, lParam);
}

LRESULT CAppLauncherDlg::OnPopMsgBox(WPARAM, LPARAM)
{
	AfxMessageBox(_T("ImGui按钮点击！"));
	return 0;
}

LRESULT CAppLauncherDlg::OnNcHitTest(CPoint point)
{
#if 0
	//point是屏幕坐标

	if (m_bImGuiInited
		&& m_imguiTitleBarScreenRect.Contains(ImVec2((float)point.x, (float)point.y)))
	{
		// 告诉Windows：这个点是窗口标题栏，系统自动处理拖动
		return HTCAPTION;
	}
	return CDialogEx::OnNcHitTest(point);
#endif
#if 0
	if (!m_bImGuiInited)
		return CDialogEx::OnNcHitTest(point);

	ImVec2 pt((float)point.x, (float)point.y);
	if (!m_imguiTitleBarScreenRect.Contains(pt))
	{
		return CDialogEx::OnNcHitTest(point);
	}

	// ----关键：箭头+关闭按钮的宽度，ImGui默认标题栏左侧控件区域约24像素----
	const float titleButtonsAreaWidth = 24.0f;
	ImRect buttonsRect = m_imguiTitleBarScreenRect;
	buttonsRect.Max.x = buttonsRect.Min.x + titleButtonsAreaWidth;

	// 如果点在【箭头/关闭按钮】区域 → HTCLIENT，交给ImGui处理点击
	if (buttonsRect.Contains(pt))
	{
		return HTCLIENT;
	}

	// 点在标题栏空白部分 → HTCAPTION，系统拖动MFC窗口
	return HTCAPTION;
#endif

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

void CAppLauncherDlg::OnSize(UINT nType, int cx, int cy)
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
