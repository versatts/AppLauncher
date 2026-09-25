#pragma once
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <vector>
#include <string>
#include <d3d11.h>
#include <tchar.h>
// 定義符合網路標準的網頁 URL 最大長度 (2048)
#ifndef INTERNET_MAX_URL_LENGTH
#define INTERNET_MAX_URL_LENGTH 2048
#endif

struct ST_APP
{
    ID3D11ShaderResourceView* iconSrv;
    WCHAR exePathBuf[INTERNET_MAX_URL_LENGTH] = { 0 };
    std::string sDisplay;
};

struct ST_FOLDER
{
    std::vector<ST_APP> vApp;
    std::wstring sName;
};

class ResLoader;
class FolderUIData
{
public:
    void Init(ID3D11Device* pd3dDevice, ResLoader* pRL);
    std::vector<ST_FOLDER> gvFolder;
    std::vector<ST_APP>* gpvApp = nullptr;
    std::vector<std::wstring> gvFolderName;

    int gCurTab = 0;
    bool m_bShowName = true;
private:
    void MakeOneFolder(ID3D11Device* pd3dDevice, ResLoader* pRL, std::vector<std::wstring>& lnkList, ST_FOLDER& folder);
};

