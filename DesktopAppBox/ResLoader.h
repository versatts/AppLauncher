#pragma once

#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <d3d11.h>
#include <vector>
struct LoadedIconItem
{
	int index = 0;                          // 在 DLL 中的順序索引 (0, 1, 2...)
	WORD resourceId = 0;                    // 若為數字 ID，記錄其資源 ID
	std::wstring resourceName;              // 若為字串名稱，記錄其名稱
	ID3D11ShaderResourceView* pSRV = nullptr;
	UINT width = 0;
	UINT height = 0;
};
class ResLoader
{
public:
	~ResLoader();
	ID3D11Device* m_pDevice;
	ID3D11ShaderResourceView* LoadHighestResIconSRV(ID3D11Device* pDevice, const wchar_t* exePath, UINT& outW, UINT& outH);
	ID3D11ShaderResourceView* LoadPng(const std::wstring& sPath, UINT& outW, UINT& outH);

	// 載入指定 DLL 內的所有圖示 (例如 L"C:\\Windows\\System32\\shell32.dll")
	void LoadAllIconsFromDll(ID3D11Device* pDevice, const wchar_t* dllPath, int targetSize = 64);

	std::vector<LoadedIconItem> m_vSysIcon;
private:

	ID3D11ShaderResourceView* CreateSRVFromPixels(ID3D11Device* pDevice, std::vector<DWORD>& pixels, int& outW, int& outH, int targetSize);
	bool ConvertHIconToBlob(HICON hIcon, std::vector<BYTE>& pBlob, UINT& width, UINT& height);
	ID3D11ShaderResourceView* IconToD3D11SRV_Simple(ID3D11Device* pDevice, HICON hIcon, int& outW, int& outH, int targetSize = 0);
	ID3D11ShaderResourceView* CreateSRVFromPngBlob(ID3D11Device* pDevice, const std::vector<BYTE>& pngBlob, UINT& outW, UINT& outH);
	bool LoadLargestIconResourceFromExe(const wchar_t* exePath, std::vector<BYTE>& pBlob, UINT& width, UINT& height);
private:
	std::vector<ID3D11ShaderResourceView*> m_vRes;
};


