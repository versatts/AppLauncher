#include "FolderUIData.h"
#include "link.h"
#include "ResLoader.h"
void FolderUIData::Init(ID3D11Device* pd3dDevice, ResLoader* pRL)
{
    std::wstring sDir;
    std::vector<std::wstring> vFolder;
    auto lnkList = EnumLnkFilesInAppDir(sDir, vFolder);

    if (lnkList.size())
    {
        ST_FOLDER folder0;
        folder0.sName = _T("Useful");
        MakeOneFolder(pd3dDevice, pRL, lnkList, folder0);
        gvFolder.push_back(folder0);
        gvFolderName.push_back(folder0.sName);
        gpvApp = &(gvFolder[0].vApp);
    }

    for (auto fx : vFolder)
    {
        ST_FOLDER folderx;
        folderx.sName = fx;
        std::vector<std::wstring> vEmpty;
        auto lnkList = EnumLnkFilesInAppDir(fx, vEmpty);
        MakeOneFolder(pd3dDevice, pRL, lnkList, folderx);
        gvFolder.push_back(folderx);
        gvFolderName.push_back(folderx.sName);
    }

 
}

void FolderUIData::MakeOneFolder(ID3D11Device* pd3dDevice, ResLoader* pRL, std::vector<std::wstring>& lnkList, ST_FOLDER& folder)
{
    for (const auto& path : lnkList)
    {
        ST_APP item;
        bool resolved = false;
        WCHAR iconPath[INTERNET_MAX_URL_LENGTH] = { 0 };

        // 不論 .url 還是 .lnk，最先嘗試同名 .ico 覆蓋圖標
        std::wstring sIcon;
        if (CheckSpecificIcon(path, sIcon))
        {
            wcsncpy_s(iconPath, sIcon.c_str(), INTERNET_MAX_URL_LENGTH);
            resolved = true;
        }

        // 再依副檔名解析啟動目標（exePathBuf 不受圖標影響，始終解析）
        if (path.size() > 4 && _wcsicmp(path.c_str() + path.size() - 4, L".url") == 0)
        {
            // .url：解析 URL 機制
            WCHAR iconPath2[INTERNET_MAX_URL_LENGTH] = { 0 };
            bool bRet = ResolveUrlTarget(path.c_str(), item.exePathBuf, INTERNET_MAX_URL_LENGTH, iconPath2, INTERNET_MAX_URL_LENGTH);
            if (!resolved && bRet)
            {
                wcsncpy_s(iconPath, iconPath2, INTERNET_MAX_URL_LENGTH);
                resolved = true;
            }
        }
        else
        {
            // .lnk：解析實體路徑
            bool bRet = ResolveLnkTarget(path.c_str(), item.exePathBuf, INTERNET_MAX_URL_LENGTH);
            if (!resolved && bRet)
            {
                // 標準 exe 的圖標路徑就是它自己
                wcsncpy_s(iconPath, item.exePathBuf, INTERNET_MAX_URL_LENGTH);
                resolved = true;
            }
        }

        if (resolved)
        {
            UINT w, h;
            // 💡 傳入 iconPath（如果是 Steam 會是快取的 .ico，如果是普通 EXE 會是 exe 自己的路徑）
            item.iconSrv = pRL->LoadHighestResIconSRV(pd3dDevice, iconPath, w, h);

            item.sDisplay = GetUtf8FileNameFromWstring(path);

            std::string sTemp;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, item.exePathBuf, -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                sTemp.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, item.exePathBuf, -1, &sTemp[0], size_needed, NULL, NULL);
            }

            int pos = sTemp.length() - 4;
            if ((sTemp.find(".exe") == pos) || (sTemp.find(".ico") == pos))
            {
            }
            else
            {
                item.sPath = sTemp;
            }
            folder.vApp.push_back(item);
        }
    }
}

