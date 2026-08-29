#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <d3d11.h>

#include <thread>
#include <atomic>
#include <stack>
#include <mutex>

#include <wincodec.h>
#include <cstring>
#pragma comment(lib,"windowscodecs.lib")

#define MY_IMGUI_MSG (WM_USER+5000)
#define MY_IMGUI_RENDER (WM_USER+5001)
#define MY_IMGUI_AFTER_CREATE (WM_USER+5002)

enum IMGUI_ACTION
{
	SLIDER_CHANGED,
	BUTTON_CLICKED,
	EDIT_CHANGE,
	COUNT
};
class ImGuiWndBase;
class ImguiPack
{
public:
	ImguiPack();
	~ImguiPack();
public:
	// D3D11 …Ë±∏
	ID3D11Device* m_pd3dDevice = nullptr;
	ID3D11DeviceContext* m_pd3dDeviceContext = nullptr;

	void CreateD3D11Resources();
	void ReleaseD3D11Resources();


	std::atomic<bool> m_threadRun{ false };
	std::atomic<bool> m_threadPause{ true };
	std::thread m_renderThread;

	std::mutex m_mtx;
	std::stack<ImGuiWndBase*> m_AllWnd;
	void AddWnd(ImGuiWndBase*);
	void DelWnd(ImGuiWndBase*);
	void ContinueRender(bool bStart);


	HRESULT LoadPngToSRV(HINSTANCE hInst, LPCWSTR filePath, ID3D11ShaderResourceView** outSRV);
	ID3D11ShaderResourceView* m_srvInfo = nullptr;
};

