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
    std::string sPath;
    std::wstring sLnkPath;   // 拖出用：lnk 資料夾中該捷徑檔案的完整原始路徑
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

    // 熱重載指定分頁的捷徑資料（用於 Explorer 拖入新 .lnk/.url 後的即時刷新）
    bool ReloadFolder(ID3D11Device* pd3dDevice, ResLoader* pRL, int idx);

    // ? TAB 拖動排序：把第 from 個分頁移到第 to 個位置（gvFolder 與 gvFolderName 同步重排）
    //    分頁 0（lnk 根目錄 "Useful"）鎖定第一位，from/to 均須 > 0
    void MoveFolder(int from, int to);

    // 退出時持久化各分頁的顯示順序：把 lnk/url 檔案重命名為 "0000-原名"、"0001-原名"...
    // 下次啟動時按前綴數字排序，即恢復本次退出時的順序
    void PersistOrder();
    std::vector<ST_FOLDER> gvFolder;
    std::vector<ST_APP>* gpvApp = nullptr;
    std::vector<std::wstring> gvFolderName;

    int* m_pIdx = nullptr;
    bool m_bShowName = true;
private:
    void MakeOneFolder(ID3D11Device* pd3dDevice, ResLoader* pRL, std::vector<std::wstring>& lnkList, ST_FOLDER& folder);
};

