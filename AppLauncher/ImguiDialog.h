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

class ImguiDialog : public CDialogEx
{
public:
	ImguiDialog(DWORD resID, CWnd* pParent = nullptr);	// 标准构造函数

protected:
	void InitDialog();
	virtual BOOL OnInitDialog();
	afx_msg void OnDestroy();
	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
	afx_msg LRESULT OnNcHitTest(CPoint point);
	afx_msg void OnSize(UINT nType, int cx, int cy);
	virtual afx_msg LRESULT OnImguiMsg(WPARAM, LPARAM) = 0;

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
	virtual void ImGuiRenderFrame() = 0;
	virtual void ImGuiRenderFrame_End();
	virtual void ImGuiRenderFrame_PostProc();
	//virtual void ImguiNotify(WPARAM, LPARAM) = 0;

	std::atomic<bool> m_threadRun{ false };
	std::thread m_renderThread;

protected:
	ImRect      m_imguiTitleBarScreenRect; // 保存标题栏屏幕矩形
	ImVec2  m_lastImGuiWinSize = { 0,0 };
	bool        m_imguiWindowOpen = true; // 用于关闭叉按钮

	//外观设定
protected:
	bool m_bHasTitleBar = true;
};

