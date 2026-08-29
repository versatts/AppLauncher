#include "pch.h"
#include "MsgBox.h"

#include "pch.h"
#include "MsgBox.h"

IMPLEMENT_DYNAMIC(MsgBox, CWnd)

ATOM MsgBox::m_wndClassAtom = 0;

MsgBox::MsgBox(CWnd* pParent /*=nullptr*/)
	: CWnd()
	, m_nRetCode(IDCANCEL)
	, m_pParentWnd(pParent)

{
}

MsgBox::~MsgBox()
{
}

BEGIN_MESSAGE_MAP(MsgBox, CWnd)
	ON_WM_PAINT()
	ON_WM_KEYDOWN()
	ON_WM_NCHITTEST()
	ON_WM_DESTROY()
END_MESSAGE_MAP()

int MsgBox::DoModal()
{
	ASSERT(m_hWnd == nullptr);

	// 默认窗口大小位置，你也可以外部修改m_rcWnd
	m_rcWnd = CRect(200, 200, 700, 500);

	if (!CreateMsgBox(m_rcWnd, m_pParentWnd))
	{
		return -1;
	}

	// 模态消息循环
	MSG msg{ 0 };
	while (::IsWindow(m_hWnd) && ::GetMessage(&msg, nullptr, 0, 0))
	{
		::TranslateMessage(&msg);
		::DispatchMessage(&msg);
	}

	return m_nRetCode;
}

void MsgBox::EndModal(int nRetCode)
{
	m_nRetCode = nRetCode;
	if (IsWindow(m_hWnd))
	{
		DestroyWindow();
	}
}

BOOL MsgBox::CreateMsgBox(const CRect& rc, CWnd* pParentWnd)
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

	DWORD dwStyle = WS_POPUP | WS_VISIBLE;
	dwStyle &= ~(WS_CAPTION | WS_BORDER | WS_THICKFRAME);

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

void MsgBox::OnPaint()
{
	CPaintDC dc(this);
	CRect rcClient;
	GetClientRect(&rcClient);

	// 纯光板背景，修改颜色在这里
	dc.FillSolidRect(&rcClient, RGB(35, 35, 35));

	// 示例绘制文本
	dc.SetTextColor(RGB(255, 255, 255));
	dc.TextOut(20, 20, _T("MsgBox CWnd模态光板窗口"));
}

void MsgBox::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	if (nChar == VK_ESCAPE)
	{
		EndModal(IDCANCEL);
		return;
	}
	CWnd::OnKeyDown(nChar, nRepCnt, nFlags);
}

LRESULT MsgBox::OnNcHitTest(CPoint point)
{
	LRESULT hit = CWnd::OnNcHitTest(point);
	return (hit == HTCLIENT) ? HTCAPTION : hit;
}

void MsgBox::OnDestroy()
{
	CWnd::OnDestroy();
	// 注意：DoModal返回后对象由调用方释放，这里不要写delete this;
}
