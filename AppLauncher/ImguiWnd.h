#pragma once
#include <afxwin.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>
#include <dxgi.h>

#include <thread>
#include <atomic>

#include "ImguiPack.h"


class ImguiWnd : public CWnd
{
public:
	ImguiWnd(CWnd* pParent = nullptr);	// 标准构造函数

	CWnd*            m_pParentWnd; // 保存父窗口指针
	static ATOM      m_wndClassAtom;
	CRect            m_rcWnd;

	// 模仿CDialog接口
	int DoModal(bool bModal = true, CRect* rc = nullptr);
	void EndModal(int nRetCode);

	void Render();

	void Stop(bool bDelete);
	void Start(bool bAdd);
protected:
	BOOL CreateWnd(const CRect& rc, CWnd* pParentWnd);
	afx_msg void OnDestroy();
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual afx_msg LRESULT OnImguiMsg(WPARAM, LPARAM) = 0;
	afx_msg LRESULT OnRender(WPARAM, LPARAM);
DECLARE_MESSAGE_MAP()
protected:
	// D3D11 设备
	ID3D11Device* m_pd3dDevice = nullptr;
	ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;
	IDXGISwapChain* m_pSwapChain = nullptr;
	ID3D11RenderTargetView* m_pRenderTargetView = nullptr;

	bool m_bImGuiInited = false;

	HRESULT CreateSwapChainAndRTV(HWND hWnd, ID3D11Device* pGlobalDevice);
	void ReleaseImgui();

	virtual void ImGuiRenderFrame_Start();
	virtual void ImGuiRenderFrame();
	virtual void ImGuiRenderFrame_End();

	std::atomic<bool> m_threadRun{ false };
	std::thread m_renderThread;

protected:
	ImRect      m_imguiTitleBarScreenRect; // 保存标题栏屏幕矩形
	ImVec2  m_lastImGuiWinSize = { 0,0 };
	bool        m_imguiWindowOpen = true; // 用于关闭叉按钮

	//外观设定
protected:
	bool m_bHasTitleBar = true;
	CRect m_rcMargin;//绘制区距四边的像索数
	bool m_bResize = false;

	//
protected:
	int              m_nRetCode = IDCANCEL;

};

