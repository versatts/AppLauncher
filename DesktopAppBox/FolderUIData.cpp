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

bool FolderUIData::ReloadFolder(ID3D11Device* pd3dDevice, ResLoader* pRL, int idx)
{
    if (idx < 0 || idx >= (int)gvFolder.size())
    {
        return false;
    }

    // 分頁 0 = lnk 根目錄；其他分頁 = lnk\<分頁名>
    std::wstring subDir;
    if (idx > 0)
    {
        subDir = gvFolderName[idx];
    }

    std::vector<std::wstring> vIgnore; // 子目錄列表此處不需要
    auto lnkList = EnumLnkFilesInAppDir(subDir, vIgnore);

    ST_FOLDER& folder = gvFolder[idx];
    folder.vApp.clear();
    MakeOneFolder(pd3dDevice, pRL, lnkList, folder);

    return true;
}

// ============================================================
// 🚀 TAB 拖動排序：把第 from 個分頁（含其 vApp 資料與目錄名）移到第 to 個位置。
//    記憶體順序即 TAB 顯示順序；磁碟上的資料夾名在退出時由 PersistOrder 統一改名。
//    分頁 0（lnk 根目錄 "Useful"）不參與：from/to 均須 > 0。
// ============================================================
void FolderUIData::MoveFolder(int from, int to)
{
    if (from == to || from <= 0 || to <= 0 ||
        from >= (int)gvFolder.size() || to >= (int)gvFolder.size())
    {
        return;
    }

    // 同步搬移資料與目錄名（保持 gvFolder[i] ↔ gvFolderName[i] 對應）
    ST_FOLDER movedFolder = std::move(gvFolder[from]);
    std::wstring movedName = std::move(gvFolderName[from]);
    gvFolder.erase(gvFolder.begin() + from);
    gvFolderName.erase(gvFolderName.begin() + from);
    gvFolder.insert(gvFolder.begin() + to, std::move(movedFolder));
    gvFolderName.insert(gvFolderName.begin() + to, std::move(movedName));

    // 修正當前選中分頁索引（m_pIdx 指向 TabHeadUI::m_Idx）：
    // 拖了哪個分頁就顯示哪個。ImGui 按鈕「按住直接拖走」不會產生點擊切換
    // （點擊是鬆開時觸發），m_Idx 停留在拖動前的分頁——這裡強制切到被拖
    // 分頁的新位置，符合「移動完顯示被移動的那個」的直覺
    if (m_pIdx)
    {
        *m_pIdx = to;
    }
}

// ============================================================
// 🚀 退出時持久化顯示順序：按各分頁當前顯示順序，把 lnk/url 檔案
// 重命名為 "0000-原名"、"0001-原名"...（兩階段重命名避免互相碰撞）
// ============================================================
void FolderUIData::PersistOrder()
{
    for (size_t idx = 0; idx < gvFolder.size(); ++idx)
    {
        // 分頁 0 = lnk 根目錄；其他分頁 = lnk\<分頁名>
        std::wstring subDir = (idx == 0) ? std::wstring() : gvFolderName[idx];
        std::wstring dir = GetLnkDirForFolder(subDir);
        if (dir.empty()) continue;

        const auto& vApp = gvFolder[idx].vApp;

        std::vector<std::pair<std::wstring, std::wstring>> tmpRenames; // (原路徑, 臨時路徑)
        std::vector<std::wstring> finalPaths;

        int order = 0;
        for (const auto& app : vApp)
        {
            if (app.sLnkPath.empty()) continue;

            size_t slash = app.sLnkPath.find_last_of(L"\\/");
            if (slash == std::wstring::npos) continue;
            std::wstring fileName = app.sLnkPath.substr(slash + 1);

            // 拆分副檔名（.lnk / .url）
            size_t dot = fileName.find_last_of(L'.');
            std::wstring ext  = (dot != std::wstring::npos) ? fileName.substr(dot) : L"";
            std::wstring stem = (dot != std::wstring::npos) ? fileName.substr(0, dot) : fileName;

            // 去掉舊前綴，套上本次顯示順序的新前綴
            WCHAR prefix[8];
            swprintf_s(prefix, L"%04d", order++);
            std::wstring finalName = std::wstring(prefix) + L"-" + StripOrderPrefix(stem) + ext;

            // 兩階段：先全部改成唯一臨時名（避免 A 的最終名與 B 的原名碰撞）
            std::wstring tmpPath = dir + L"\\__ordtmp__" + fileName;
            tmpRenames.push_back({ app.sLnkPath, tmpPath });
            finalPaths.push_back(dir + L"\\" + finalName);

            // 🚀 同名覆蓋圖標（xxx.png / xxx.ico）也要跟著一起改名，否則改名後圖標會失聯
            const wchar_t* iconExts[] = { L".png", L".ico" };
            for (const wchar_t* iconExt : iconExts)
            {
                std::wstring oldIcon = dir + L"\\" + stem + iconExt;
                DWORD attr = ::GetFileAttributesW(oldIcon.c_str());
                if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    continue; // 無同名圖標
                }

                std::wstring iconTmpName = dir + L"\\__ordtmp__" + stem + iconExt;
                std::wstring iconNewName = dir + L"\\" + prefix + L"-" + StripOrderPrefix(stem) + iconExt;

                tmpRenames.push_back({ oldIcon, iconTmpName });
                finalPaths.push_back(iconNewName);
            }
        }

        for (size_t i = 0; i < tmpRenames.size(); ++i)
        {
            ::MoveFileExW(tmpRenames[i].first.c_str(), tmpRenames[i].second.c_str(), 0);
        }
        for (size_t i = 0; i < tmpRenames.size(); ++i)
        {
            ::MoveFileExW(tmpRenames[i].second.c_str(), finalPaths[i].c_str(), 0);
        }
    }

    // ============================================================
    // 🚀 分頁資料夾本身也按當前 TAB 順序持久化：重命名為 "NNNN-原名"
    //    （與 lnk 檔案同一套前綴規則；分頁 0 = lnk 根目錄 "Useful"，
    //     不參與改名且恆為第一位）
    //    必須放在各分頁 lnk 改名之後：lnk 的 sLnkPath 是含舊目錄名的絕對路徑
    // ============================================================
    {
        std::wstring rootDir = GetLnkDirForFolder(L"");
        if (!rootDir.empty())
        {
            struct DirRenameJob
            {
                std::wstring oldName; // 原目錄名（含舊前綴或無前綴）
                std::wstring tmpName; // 唯一臨時名（第一階段）
                std::wstring newName; // 最終名 "NNNN-原名"
            };
            std::vector<DirRenameJob> jobs;
            std::vector<std::pair<size_t, std::wstring>> memoryUpdates; // (gvFolderName 下標, 最終名)

            int dirOrder = 0;
            for (size_t idx = 1; idx < gvFolderName.size(); ++idx)
            {
                std::wstring oldName = gvFolderName[idx];
                std::wstring stem = StripOrderPrefix(oldName);

                wchar_t szPrefix[16];
                swprintf_s(szPrefix, L"%04d", dirOrder++);

                std::wstring newName = std::wstring(szPrefix) + L"-" + stem;

                memoryUpdates.push_back({ idx, newName });
                if (_wcsicmp(oldName.c_str(), newName.c_str()) != 0)
                {
                    DirRenameJob job;
                    job.oldName = oldName;
                    job.tmpName = std::wstring(L"__ordtmpdir__") + szPrefix;
                    job.newName = newName;
                    jobs.push_back(std::move(job));
                }
            }

            // 兩階段重命名（與 lnk 同理：避免 A 的最終名與 B 的原名碰撞）
            for (const auto& j : jobs)
            {
                ::MoveFileExW((rootDir + L"\\" + j.oldName).c_str(),
                              (rootDir + L"\\" + j.tmpName).c_str(), 0);
            }
            for (const auto& j : jobs)
            {
                ::MoveFileExW((rootDir + L"\\" + j.tmpName).c_str(),
                              (rootDir + L"\\" + j.newName).c_str(), 0);
            }

            // 同步記憶體（目錄名含前綴，路徑拼接全部繼續有效）
            for (const auto& mu : memoryUpdates)
            {
                gvFolderName[mu.first] = mu.second;
            }
        }
    }
}

void FolderUIData::MakeOneFolder(ID3D11Device* pd3dDevice, ResLoader* pRL, std::vector<std::wstring>& lnkList, ST_FOLDER& folder)
{
    for (const auto& path : lnkList)
    {
        ST_APP item;
        bool resolved = false;
        WCHAR iconPath[INTERNET_MAX_URL_LENGTH] = { 0 };

        // 記錄該捷徑檔案的原始完整路徑（拖出刪檔時使用）
        item.sLnkPath = path;

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

            // 顯示名：去掉檔名中的 "NNNN-" 排序前綴
            item.sDisplay = GetUtf8FileNameFromWstring(StripOrderPrefixFromPath(path));

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

