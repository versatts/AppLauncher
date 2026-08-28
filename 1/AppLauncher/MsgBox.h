#pragma once
#include <afxwin.h>
#include "ImguiWnd.h"
class MsgBox : public ImguiWnd 
{
	DECLARE_DYNAMIC(MsgBox)
public:
	MsgBox(CWnd* pParent = nullptr);
	virtual ~MsgBox();
#if 0
	// Ä£·ÂCDialog½Ó¿Ú
	int DoModal(bool bModal = true);
	void EndModal(int nRetCode);
#endif
protected:
//	BOOL CreateMsgBox(const CRect& rc, CWnd* pParentWnd);

//	afx_msg void OnPaint();
//	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
//	afx_msg LRESULT OnNcHitTest(CPoint point);
//	afx_msg void OnDestroy();
	virtual afx_msg LRESULT OnImguiMsg(WPARAM, LPARAM);
	DECLARE_MESSAGE_MAP()

	virtual void ImGuiRenderFrame();
private:
//	int              m_nRetCode;
	int m_lastDesiredClientHeight = 0;
};
