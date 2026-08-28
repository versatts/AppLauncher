#include "pch.h"
#include "ImguiPack.h"
#include "ImguiWnd.h"
ImguiPack::ImguiPack()
{
	// 创建D3D11
	CreateD3D11Resources();

	// 初始化ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;

	ImFontConfig cfg;
	cfg.OversampleH = 2;
	cfg.OversampleV = 2;
	cfg.PixelSnapH = true;

	// 加载微软雅黑，字号17，中文完整字符集
	ImFont* fontYaHei = io.Fonts->AddFontFromFileTTF(
		"C:\\Windows\\Fonts\\msyh.ttc",
		17.0f,
		&cfg,
		io.Fonts->GetGlyphRangesChineseFull()
	);
	IM_ASSERT(fontYaHei != nullptr); // 失败直接断言，排查路径问题
	io.FontDefault = fontYaHei; // 设置全局默认字体，全部UI生效

	ImGui::StyleColorsLight();

	ImGuiStyle& style = ImGui::GetStyle();

	style.FramePadding.x = 4.0f; // 增大，标题栏变高；减小，标题栏变矮
	style.FramePadding.y = 4.0f; // 增大，标题栏变高；减小，标题栏变矮

	ImVec4* colors = style.Colors;
	colors[ImGuiCol_TitleBg] = ImColor(0x4B, 0x4B, 0x52); // 窗口未激活标题栏
	colors[ImGuiCol_TitleBgActive] = ImColor(0x4B, 0x4B, 0x52);// 当前激活窗口标题栏
	colors[ImGuiCol_TitleBgCollapsed] = ImColor(0x4B, 0x4B, 0x52); // 窗口折叠后的标题栏

	//#37373D
	// 1. 窗口主体背景
	style.Colors[ImGuiCol_WindowBg] = ImColor(0x37 ,0x37, 0x3D);

	style.Colors[ImGuiCol_Text] = ImColor(0xFF, 0xFF, 0xFF);      //普通文字
	style.Colors[ImGuiCol_TextDisabled] = ImColor(0x80, 0x80, 0x80); //禁用控件文字;

	style.Colors[ImGuiCol_FrameBg] = ImColor(40, 40, 40, 255);
	style.Colors[ImGuiCol_FrameBgHovered] = ImColor(60, 60, 60, 255);
	style.Colors[ImGuiCol_FrameBgActive] = ImColor(80, 80, 80, 255);

	style.Colors[ImGuiCol_SliderGrab] = ImColor(60, 180, 255, 255);
	style.Colors[ImGuiCol_SliderGrabActive] = ImColor(90, 200, 255, 255);
	
	ImGui_ImplDX11_Init(m_pd3dDevice, m_pd3dDeviceContext);

#if 1
	// 启动渲染工作线程
	m_threadRun = true;
	m_renderThread = std::thread([this]()
		{
			while (m_threadRun)
			{
				{
					std::lock_guard<std::mutex> mtx(m_mtx);

					if (!m_threadPause)
					{
						if (m_AllWnd.size())
						{
							auto a = m_AllWnd.top();
							//主线程绘制，否则崩溃
							::PostMessage(a->GetSafeHwnd(), MY_IMGUI_RENDER, 0, 0);
						}
					}
				}
				// 限速 ~60fps，16ms
				std::this_thread::sleep_for(std::chrono::milliseconds(16));
			}
		});
#endif
}
ImguiPack::~ImguiPack()
{
	ReleaseD3D11Resources();
}
void ImguiPack::CreateD3D11Resources()
{

	D3D_FEATURE_LEVEL featureLevels[] = { D3D_FEATURE_LEVEL_11_0 };
	UINT createFlags = 0;
#ifdef _DEBUG
	createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
	HRESULT hr = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createFlags,
		featureLevels,
		_countof(featureLevels),
		D3D11_SDK_VERSION,
		&m_pd3dDevice,
		nullptr,
		&m_pd3dDeviceContext
	);
}

void ImguiPack::ReleaseD3D11Resources()
{
	ImGui_ImplDX11_Shutdown();
	ImGui::DestroyContext();

	if (m_pd3dDeviceContext) { m_pd3dDeviceContext->Release(); m_pd3dDeviceContext = nullptr; }
	if (m_pd3dDevice) { m_pd3dDevice->Release(); m_pd3dDevice = nullptr; }
}

void ImguiPack::AddWnd(ImguiWnd* w)
{
	std::lock_guard<std::mutex> mtx(m_mtx);
	if (m_AllWnd.size())
	{
		auto a = m_AllWnd.top();
		a->Stop(false);
	}
	m_AllWnd.push(w);
	m_threadPause = true;
}
void ImguiPack::DelWnd(ImguiWnd* w)
{
	std::lock_guard<std::mutex> mtx(m_mtx);
	if (m_AllWnd.size())
	{
		m_AllWnd.pop();
	}
	m_threadPause = true;
}

void ImguiPack::ContinueRender(bool bStart)
{
	std::lock_guard<std::mutex> mtx(m_mtx);
	if (!bStart)
	{
		if (m_AllWnd.size())
		{
			auto a = m_AllWnd.top();
			a->Start(false);
		}
	}
	m_threadPause = false;
}



HRESULT ImguiPack::LoadPngToSRV(HINSTANCE hInst, LPCWSTR filePath, ID3D11ShaderResourceView** outSRV)
{
	*outSRV = nullptr;
	ID3D11Device* pd3dDevice = m_pd3dDevice;
	ID3D11DeviceContext* pd3dCtx = m_pd3dDeviceContext;

	IWICImagingFactory* pWIC = nullptr;
	HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
		IID_IWICImagingFactory, (void**)&pWIC);
	if (FAILED(hr)) 
		return hr;

	IWICBitmapDecoder* pDecoder = nullptr;
	hr = pWIC->CreateDecoderFromFilename(filePath, nullptr, GENERIC_READ,
		WICDecodeMetadataCacheOnLoad, &pDecoder);
	if (FAILED(hr)) { pWIC->Release(); return hr; }

	IWICBitmapFrameDecode* pFrame = nullptr;
	hr = pDecoder->GetFrame(0, &pFrame);
	pDecoder->Release();
	if (FAILED(hr)) { pWIC->Release(); return hr; }

	UINT w, h;
	pFrame->GetSize(&w, &h);

	IWICFormatConverter* pConv = nullptr;
	hr = pWIC->CreateFormatConverter(&pConv);
	if (SUCCEEDED(hr))
	{
		hr = pConv->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA,
			WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeMedianCut);
	}
	pFrame->Release();

	UINT stride = w * 4;
	UINT bufSize = stride * h;
	BYTE* pBits = new BYTE[bufSize];
	hr = pConv->CopyPixels(nullptr, stride, bufSize, pBits);
	pConv->Release();
	pWIC->Release();
	if (FAILED(hr)) { delete[] pBits; return hr; }

	D3D11_TEXTURE2D_DESC texDesc{};
	texDesc.Width = w;
	texDesc.Height = h;
	texDesc.MipLevels = 1;
	texDesc.ArraySize = 1;
	texDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.Usage = D3D11_USAGE_IMMUTABLE;
	texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA sub{};
	sub.pSysMem = pBits;
	sub.SysMemPitch = stride;

	ID3D11Texture2D* pTex = nullptr;
	hr = pd3dDevice->CreateTexture2D(&texDesc, &sub, &pTex);
	delete[] pBits;

	if (SUCCEEDED(hr))
	{
		hr = pd3dDevice->CreateShaderResourceView(pTex, nullptr, outSRV);
		pTex->Release();
	}
	return hr;
}
