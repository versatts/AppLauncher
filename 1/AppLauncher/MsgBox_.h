#pragma once
#include <afxwin.h>

class MsgBox : public CWnd
{
	DECLARE_DYNAMIC(MsgBox)
public:
	MsgBox(CWnd* pParent = nullptr);
	virtual ~MsgBox();

	// 模仿CDialog接口
	int DoModal();
	void EndModal(int nRetCode);

protected:
	BOOL CreateMsgBox(const CRect& rc, CWnd* pParentWnd);

	afx_msg void OnPaint();
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnDestroy();

	DECLARE_MESSAGE_MAP()

private:
	static ATOM      m_wndClassAtom;
	int              m_nRetCode;
	CWnd*            m_pParentWnd; // 保存父窗口指针
public:
	CRect            m_rcWnd;
};
