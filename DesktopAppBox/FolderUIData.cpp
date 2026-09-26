#include "FolderUIData.h"
#include "link.h"
#include "ResLoader.h"
void FolderUIData::Init(ID3D11Device* pd3dDevice, ResLoader* pRL)
{
    std::wstring sDir;
    std::vector<std::wstring> vFolder;
    auto lnkList = EnumLnkFilesInAppDir(sDir, vFolder);

    ST_FOLDER folder0;
    folder0.sName = _T("Useful");
    MakeOneFolder(pd3dDevice, pRL, lnkList, folder0);
    gvFolder.push_back(folder0);
    gvFolderName.push_back(folder0.sName);
    gpvApp = &(gvFolder[0].vApp);


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
        WCHAR iconPath[MAX_PATH] = { 0 };

        // 判斷是否為 Steam 的 .url 快捷方式
        if (path.size() > 4 && _wcsicmp(path.c_str() + path.size() - 4, L".url") == 0)
        {
            std::wstring sIcon;
            if (CheckSpecificIcon(path, sIcon))
            {
                wcsncpy_s(iconPath, sIcon.c_str(), INTERNET_MAX_URL_LENGTH);
                resolved = true;
            }
            // 解析 URL 機制

            WCHAR iconPath2[MAX_PATH] = { 0 };
            bool bRet = ResolveUrlTarget(path.c_str(), item.exePathBuf, INTERNET_MAX_URL_LENGTH, iconPath2, MAX_PATH);
            if (!resolved && bRet)
            {
                wcsncpy_s(iconPath, iconPath2, INTERNET_MAX_URL_LENGTH);
                resolved = true;
            }
        }
        else
        {
            std::wstring sIcon;
            if (CheckSpecificIcon(path, sIcon))
            {
                wcsncpy_s(iconPath, sIcon.c_str(), INTERNET_MAX_URL_LENGTH);
                resolved = true;
            }
            // 原有的 .lnk 解析機制
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
            folder.vApp.push_back(item);
        }
    }
}

