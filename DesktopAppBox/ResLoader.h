#pragma once

#define NOMINMAX
#include <windows.h>
#include <wincodec.h>
#include <vector>
#include <string>
#include <d3d11.h>


class ResLoader
{
public:
	~ResLoader();
	ID3D11ShaderResourceView* LoadHighestResIconSRV(ID3D11Device* pDevice, const wchar_t* exePath, UINT& outW, UINT& outH);
private:
	ID3D11ShaderResourceView* IconToD3D11SRV_Simple(ID3D11Device* pDevice, HICON hIcon, int& outW, int& outH);
	ID3D11ShaderResourceView* CreateSRVFromPngBlob(ID3D11Device* pDevice, const std::vector<BYTE>& pngBlob, UINT& outW, UINT& outH);
	bool LoadLargestIconResourceFromExe(const wchar_t* exePath, std::vector<BYTE>& pBlob, UINT& width, UINT& height);
private:
	std::vector<ID3D11ShaderResourceView*> m_vRes;
};


