#include "pch.h"
#include "MsgBox.h"

IMPLEMENT_DYNAMIC(MsgBox, CWnd)


MsgBox::MsgBox(CWnd* pParent /*=nullptr*/)
	: ImguiWnd(pParent)
{
	m_rcMargin = CRect(3, 3, 3, 3);
}

MsgBox::~MsgBox()
{
}

BEGIN_MESSAGE_MAP(MsgBox, ImguiWnd)
//	ON_WM_PAINT()
//	ON_WM_KEYDOWN()
//	ON_WM_NCHITTEST()
//	ON_WM_DESTROY()
END_MESSAGE_MAP()


void MsgBox::ImGuiRenderFrame()
{
	float vSpace = 20.0f;
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vSpace);

	const char* szText = u8"这是一条信息。\n这是另一条信息。\n这是另另一条信息。";
	ImVec2 textSize = ImGui::CalcTextSize(szText);
	float availW = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;
	ImGui::SetCursorPosX(ImGui::GetWindowContentRegionMin().x + (availW - textSize.x) * 0.5f);
	ImGui::Text("%s", szText);

	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + vSpace);

	const char* btn1Text = u8"确定";
	const char* btn2Text = u8"取消";
	const float btnFixedW = 90.0f;
	const float gap = 10.0f;

	float cMinX = ImGui::GetWindowContentRegionMin().x;
	float cMaxX = ImGui::GetWindowContentRegionMax().x;

	// 总占用宽度：按钮1 + 间隙 + 按钮2
	float totalW = btnFixedW + gap + btnFixedW;

	// 设置起始X，整体靠右
	ImGui::SetCursorPosX(cMaxX - totalW);

	if (ImGui::Button(btn1Text, ImVec2(btnFixedW, 0.0f)))
	{
		m_nRetCode = IDOK;
		::PostMessage(GetSafeHwnd(), WM_CLOSE, 0, 0);
	}
	ImGui::SameLine(cMaxX - btnFixedW); // 第二个按钮右对齐到内容区右边界
	if (ImGui::Button(btn2Text, ImVec2(btnFixedW, 0.0f)))
	{
		m_nRetCode = IDCANCEL;
		::PostMessage(GetSafeHwnd(), WM_CLOSE, 0, 0);
	}

	///////
	float localCursorY = ImGui::GetCursorPosY(); // ✔ 窗口上下文内读取坐标

	// ========== ✔ End之后再做全部MFC尺寸计算、AdjustWindowRectEx、SetWindowPos ==========
	int desiredClientHeight = (int)(m_rcMargin.top + localCursorY + cMinX);

	if (desiredClientHeight != m_lastDesiredClientHeight)
	{
		m_lastDesiredClientHeight = desiredClientHeight;
		CRect rcClient;
		GetClientRect(&rcClient);
		CRect rcWnd;
		rcWnd.SetRect(0, 0, rcClient.Width(), desiredClientHeight);
		DWORD dwStyle = GetStyle();
		DWORD dwExStyle = GetExStyle();
		::AdjustWindowRectEx(&rcWnd, dwStyle, FALSE, dwExStyle);
		SetWindowPos(nullptr, 0, 0, rcWnd.Width(), rcWnd.Height(), SWP_NOMOVE | SWP_NOZORDER);
	}

	// 处理关闭叉：点击关闭叉 m_imguiWindowOpen=false
	if (!m_imguiWindowOpen)
	{
		// 这里你可以选择：关闭MFC对话框
		::PostMessage(GetSafeHwnd(), WM_CLOSE, 0, 0);
	}
	// ===========================================
}


LRESULT MsgBox::OnImguiMsg(WPARAM wparam, LPARAM lparam)
{
	IMGUI_ACTION a = (IMGUI_ACTION)wparam;
	switch (a)
	{
	case BUTTON_CLICKED:
	{
		AfxMessageBox(_T("ImGui按钮点击！"));
#if 0
		MsgBox* pMsgBox = new MsgBox(this);
		pMsgBox->m_rcWnd = CRect(300, 300, 800, 600);
		int nRet = pMsgBox->DoModal();
		delete pMsgBox;
#endif
		break;
	}
	case SLIDER_CHANGED:
	{
		float f;
		memcpy(&f, &lparam, 4);
		CString s; s.Format(_T("%.4f"), f);
		s = _T("slider: ") + s;
		AfxMessageBox(s);

		break;
	}
	}

	return 0;
}


LRESULT MsgBox::OnAfterCreate(WPARAM wparam, LPARAM lparam)
{
	::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST,
		0, 0, 0, 0,
		SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	return 0;
}
