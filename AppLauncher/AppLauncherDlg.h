
// AppLauncherDlg.h: 头文件
//

#pragma once


#include "ImGuiWndBase.h"
#define WM_POPUP_ENSURE_Z (WM_USER + 101)
class CAboutDlg;
// CAppLauncherDlg 对话框
class CAppLauncherDlg : public ImGuiWndBase
{
// 构造
public:
	CAppLauncherDlg(CWnd* pParent = nullptr);	// 标准构造函数

	CAboutDlg* m_pDlg;

// 实现
protected:
//	HICON m_hIcon;

	// 生成的消息映射函数
//	virtual BOOL OnInitDialog();
//	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
//	afx_msg void OnPaint();
//	afx_msg HCURSOR OnQueryDragIcon();
	virtual afx_msg LRESULT OnImguiMsg(WPARAM, LPARAM);
	afx_msg LRESULT OnAfterCreate(WPARAM, LPARAM);
//	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnDestroy();
//	afx_msg void OnActivate(UINT nState, CWnd* pWndOther, BOOL bMinimized);
//	afx_msg LRESULT OnPopupEnsureZ(WPARAM, LPARAM);
	DECLARE_MESSAGE_MAP()

private:
    virtual void ImGuiRender_Main();
	bool bTest = false;

	void ShowWinTop();
protected:
	HWND        m_hPopupWnd = nullptr;
	int         m_popupW;
	int         m_popupH;
	bool        m_bNeedRaisePopup = false; // 需要抬升标记
	bool CreatePopupOwned(int w, int h);

	void SyncPopupCorner();
	CRect       m_lastMainWndRect; // 保存上一次主窗口屏幕矩形，用于变化检测

};
