
// AppLauncherDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "AppLauncher.h"
#include "AppLauncherDlg.h"
#include "afxdialogex.h"
#include "MsgBox.h"
#include "ImguiMsgBox.h"
#ifdef _DEBUG
#define new DEBUG_NEW
#endif

// ImGui Win32 外部声明
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define WM_IMGUI_RENDER (WM_USER + 1001)
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
public:
	afx_msg void OnBnClickedBtnTest();
};

CAboutDlg::CAboutDlg() : CDialogEx(IDD_ABOUTBOX)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
	ON_BN_CLICKED(IDC_BTN_TEST, &CAboutDlg::OnBnClickedBtnTest)
END_MESSAGE_MAP()


// CAppLauncherDlg 对话框



CAppLauncherDlg::CAppLauncherDlg(CWnd* pParent /*=nullptr*/)
	: ImguiWnd(pParent)
{
	m_rcMargin = CRect(5, 60, 5 + 480 + 5, 80);
	m_bResize = true;
}
BEGIN_MESSAGE_MAP(CAppLauncherDlg, ImguiWnd)
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_WM_DESTROY()
//	ON_WM_ACTIVATE()
//	ON_WM_SIZE()
//	ON_WM_NCHITTEST()
	ON_WM_WINDOWPOSCHANGED() // 主窗口位置/大小变化
	ON_MESSAGE(MY_IMGUI_MSG, &ImguiWnd::OnImguiMsg)
	ON_MESSAGE(MY_IMGUI_AFTER_CREATE, &CAppLauncherDlg::OnAfterCreate)
	//ON_MESSAGE(WM_POPUP_ENSURE_Z, &CAppLauncherDlg::OnPopupEnsureZ)
END_MESSAGE_MAP()

void CAppLauncherDlg::ImGuiRenderFrame()
{
	// 当前光标位置：标题栏+原有WindowPadding.y的位置
	float baseY = ImGui::GetCursorPosY();
	// 目标：距离标题栏底部100。原有padding会叠加，所以向上偏移差值
	float desiredTop = 50.0f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + desiredTop);

	static float f = 0.0f;
	ImGui::Text(u8"拖动本窗口标题栏 → MFC对话框整体移动 %.2f", f);
	ImGui::SliderFloat(u8"滑动条", &f, 0.0f, 1.0f);
	CString ss; ss.Format(_T("%.3f"), f);
	if (ImGui::IsItemDeactivatedAfterEdit())//紧跟SliderFloat
	{
		//编辑完成，在这里给MFC发消息
		m_pDlg->GetDlgItem(IDC_EDIT1)->SetWindowText(ss);
	}
	ImGui::SameLine(0, 14);
	ImGui::Text(ss.GetBuffer());

	if (ImGui::Button("MFC+ImGui Test Button"))
	{
		bTest = true;
		static int switchaa = 1;
		switchaa++;
		if(switchaa % 2 == 0)
			::PostMessage(GetSafeHwnd(), MY_IMGUI_MSG, BUTTON_CLICKED, 0);
		else
			ImGuiMsgBox_Show(ImGuiMsgBoxType::Info, u8"提示", u8"操作已完成。");
	}
	else
	{
		bTest = false;
	}

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10);
	static char m_bufEdit[256] = { 0 };
	if (ImGui::InputText(u8"编辑框", m_bufEdit, IM_ARRAYSIZE(m_bufEdit)))
	{
		//文字发生变化
		::PostMessage(GetSafeHwnd(), MY_IMGUI_MSG, EDIT_CHANGE, (LPARAM)m_bufEdit);
	}
	ImGui::Text(m_bufEdit);

	ImGui::ShowMetricsWindow();


	if (m_hPopupWnd != nullptr && !m_bNeedRaisePopup)
	{
		m_bNeedRaisePopup = true;
		// 投递消息到主线程WndProc，做Z序提升
		PostMessage(WM_POPUP_ENSURE_Z);
	}

	// 处理关闭叉：点击关闭叉 m_imguiWindowOpen=false
	if (!m_imguiWindowOpen)
	{
		// 这里你可以选择：关闭MFC对话框
		::PostMessage(GetSafeHwnd(), WM_CLOSE, 0, 0);
	}
	// ===========================================

	// ========== 必须在这里调用！Begin 之后，End 之前 ==========
	bool ok = ImGuiMsgBox_Render();
	if (ok)
	{
		// 用户点了确定，MFC场景建议PostMessage，不要在这里做阻塞操作
	}
	// ========================================================
	ImGui::End();


	//========= 1. 顶部窗口：占用顶部margin空白区域 =========
	CRect clientRc;
	GetClientRect(&clientRc);
	float clientW = (float)clientRc.Width();
	float clientH = (float)clientRc.Height(); 
	
	ImGui::SetNextWindowPos(ImVec2(5.0f, 5.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2((float)clientW - 10, (float)m_rcMargin.top - 2 - 5), ImGuiCond_Always);

	// 去掉标题栏、边框、拖拽、缩放，纯容器
	ImGuiWindowFlags topWndFlags =
		ImGuiWindowFlags_NoDecoration
		| ImGuiWindowFlags_NoMove
		| ImGuiWindowFlags_NoResize
		| ImGuiWindowFlags_NoScrollbar
		| ImGuiWindowFlags_NoScrollWithMouse;

	ImGui::Begin("TopMarginWindow", nullptr, topWndFlags);
	{
		// 这里写顶部区域UI，按钮、文字都可以
		ImGui::Text("这是顶部Margin区域窗口");
		if (ImGui::Button("顶部按钮"))
		{
			// do something
		}
	}
	ImGui::End();
}

LRESULT CAppLauncherDlg::OnAfterCreate(WPARAM wparam, LPARAM lparam)
{
	CreatePopupOwned(480, 300);

	m_pDlg = new CAboutDlg;
	m_pDlg->Create(IDD_ABOUTBOX, CWnd::FromHandle(m_hPopupWnd));
	m_pDlg->ShowWindow(SW_SHOW);
	HWND hDlg = m_pDlg->m_hWnd;

	// 1. 强制移除WS_POPUP，开启WS_CHILD
	LONG_PTR style = GetWindowLongPtr(hDlg, GWL_STYLE);
	style &= ~WS_POPUP;
	style |= WS_CHILD;
	SetWindowLongPtr(hDlg, GWL_STYLE, style);

	// 2. !!!关键：SetParent，把对话框真正挂到 m_hPopupWnd
	::SetParent(hDlg, m_hPopupWnd);

	// 3. 样式变更必须调用SetWindowPos(带SWP_FRAMECHANGED)才生效
	::SetWindowPos(hDlg, NULL, 0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

	// 获取宿主(m_hPopupWnd)客户区，把对话框贴进去
	CRect rcClient;
	::GetClientRect(m_hPopupWnd, &rcClient);
	m_pDlg->MoveWindow(&rcClient);

	m_pDlg->ShowWindow(SW_SHOW);
	return 0;
}
LRESULT CAppLauncherDlg::OnImguiMsg(WPARAM wparam, LPARAM lparam)
{
	IMGUI_ACTION a = (IMGUI_ACTION)wparam;
	switch (a)
	{
	case BUTTON_CLICKED:
	{
#if 0
		AfxMessageBox(_T("ImGui按钮点击！"));
#else
		MsgBox* pMsgBox = new MsgBox(this);
		pMsgBox->m_rcWnd = CRect(300, 300, 800, 600);
		int nRet = pMsgBox->DoModal();
		delete pMsgBox;
		CString s; s.Format(_T("Button [%d] clicked"), nRet);
		AfxMessageBox(s);
#endif
		break;
	}
	case SLIDER_CHANGED:
	{
		float f;
		memcpy(&f, &lparam, 4);
		CString s; s.Format(_T("Slider [%.4f]"), f);
		AfxMessageBox(s);
		break;
	}
	case EDIT_CHANGE:
	{

		break;
	}
	}

	return 0;
}

bool CAppLauncherDlg::CreatePopupOwned(int w, int h)
{
	if (!::IsWindow(m_hWnd))
	{
		TRACE0(_T("主窗口HWND无效\n"));
		return false;
	}
	if (m_hPopupWnd != nullptr)
		return true;

	m_popupW = w;
	m_popupH = h;

	DWORD dwStyle = /*WS_POPUP |*/ WS_VISIBLE | WS_BORDER | WS_CHILD | SS_NOTIFY;
	DWORD dwExStyle = WS_EX_NOACTIVATE;

	HWND hPopup = ::CreateWindowEx(
		dwExStyle,
		_T("STATIC"),
		nullptr,
		dwStyle,
		0, 0, w, h,
		GetSafeHwnd(),
		NULL,
		AfxGetInstanceHandle(),
		nullptr);

	if (!hPopup)
	{
		DWORD e = ::GetLastError();
		TRACE1(_T("CreateWindowEx err=%u\n"), e);
		return false;
	}

	m_hPopupWnd = hPopup;

	::SetWindowPos(m_hPopupWnd, GetSafeHwnd(),//HWND_TOPMOST,
		0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

	GetWindowRect(&m_lastMainWndRect);
	SyncPopupCorner();
	return true;
}
void CAppLauncherDlg::OnDestroy()
{
	ImguiWnd::OnDestroy();
	if (m_hPopupWnd != nullptr)
	{
		::DestroyWindow(m_hPopupWnd);
		m_hPopupWnd = nullptr;
	}
}

void CAppLauncherDlg::SyncPopupCorner()
{
	if (!m_hPopupWnd)
		return;

	CRect rcMainScreen;
	GetWindowRect(&rcMainScreen);

	int x = 475;// rcMainScreen.right - m_popupW - 10;
	int y = 60 + 2;// rcMainScreen.top + 80;

	::MoveWindow(m_hPopupWnd, x, y, m_popupW, m_popupH, TRUE);
#if 0
	::SetWindowPos(m_hPopupWnd, m_hWnd,//HWND_TOPMOST,
		x, y,
		m_popupW, m_popupH,
		SWP_NOACTIVATE);
#endif
}


void CAboutDlg::OnBnClickedBtnTest()
{
	// TODO: 在此添加控件通知处理程序代码

	//AfxMessageBox(_T("TEST clicked."));
}
