
// AppLauncherDlg.h: 头文件
//

#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>

#include <thread>
#include <atomic>
// CAppLauncherDlg 对话框
class CAppLauncherDlg : public CDialogEx
{
// 构造
public:
	CAppLauncherDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_APPLAUNCHER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
#if 1
    afx_msg LRESULT OnImGuiRender(WPARAM wParam, LPARAM lParam);
	afx_msg void OnDestroy();
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnPopMsgBox(WPARAM, LPARAM);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
#endif
	DECLARE_MESSAGE_MAP()

private:
    // D3D11 设备
    ID3D11Device* m_pd3dDevice = nullptr;
    ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
    IDXGISwapChain* m_pSwapChain = nullptr;
    ID3D11RenderTargetView* m_pRenderTargetView = nullptr;

    bool m_bImGuiInited = false;
    void CreateD3D11Resources();
    void ReleaseD3D11Resources();
    void ImGuiRenderFrame();

	std::atomic<bool> m_threadRun{ false };
	std::thread m_renderThread;

private:
	ImRect      m_imguiTitleBarScreenRect; // 保存标题栏屏幕矩形
	ImVec2  m_lastImGuiWinSize = { 0,0 };
	bool        m_imguiWindowOpen = true; // 用于关闭叉按钮
};
