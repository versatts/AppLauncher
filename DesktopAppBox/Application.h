#pragma once
#include "ImGuiObj.h"
#include <d3d11.h>

#define ENABLE_VIEWPORTS 1
class Application
{
public:
	ID3D11Device* g_pd3dDevice = nullptr;
	ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
	IDXGISwapChain* g_pSwapChain = nullptr;
	bool                     g_SwapChainOccluded = false;
	//UINT                     g_ResizeWidth = 0, g_ResizeHeight = 0;
	ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;
	ImFont* g_TitleTimeFont = nullptr;

	void InitImGuiContext(float);
	// Forward declarations of helper functions
	bool CreateDeviceD3D(HWND hWnd);
	void CleanupDeviceD3D();
	void CreateRenderTarget();
	void CleanupRenderTarget();
};

extern Application theApp;
