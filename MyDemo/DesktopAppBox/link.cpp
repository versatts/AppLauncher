#include "link.h"
#include <intshcut.h>
// 获取当前exe所在目录下所有 *.lnk 文件的完整路径
// subDirName: 要進入的子目錄名（若為空 L""，則在 lnk 根目錄進行搜尋）
// outSubDirList: 輸出參數，返回當前搜尋目錄內所包含的資料夾名稱列表
// 返回值：不論是否傳空，皆返回當前搜尋目錄內的所有 .lnk 和 .url 檔案全路徑
std::vector<std::wstring> EnumLnkFilesInAppDir( const std::wstring& subDirName, std::vector<std::wstring>& outSubDirList)
{
    std::vector<std::wstring> fileResult;
    outSubDirList.clear(); // 每次執行先清空

    // 1. 取得當前 exe 所在目錄
    WCHAR exeFullPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exeFullPath, MAX_PATH);

    WCHAR appDir[MAX_PATH] = { 0 };
    WCHAR* pSlash = wcsrchr(exeFullPath, L'\\');
    if (!pSlash) return fileResult;
    size_t dirLen = pSlash - exeFullPath;
    wcsncpy_s(appDir, exeFullPath, dirLen);
    appDir[dirLen] = L'\0';

    // 2. 決定目標搜尋目錄
    // 如果 subDirName 為空，路徑為 C:\xxx\lnk
    // 如果 subDirName 不為空，路徑為 C:\xxx\lnk\子目錄名
    std::wstring targetDir = std::wstring(appDir) + L"\\lnk";
    if (!subDirName.empty())
    {
        targetDir += L"\\" + subDirName;
    }

    // 3. 💥 步驟一：列舉當前 targetDir 目錄內的所有「子資料夾」
    {
        std::wstring folderPattern = targetDir + L"\\*";
        WIN32_FIND_DATAW findData = {};
        HANDLE hFind = FindFirstFileW(folderPattern.c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do {
                // 必須是資料夾，且排除系統虛擬目錄 "." 和 ".."
                if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
                    wcscmp(findData.cFileName, L".") != 0 &&
                    wcscmp(findData.cFileName, L"..") != 0)
                {
                    outSubDirList.push_back(findData.cFileName);
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }

    // 4. 💥 步驟二：列舉當前 targetDir 目錄內的所有 .lnk 與 .url 檔案
    std::vector<std::wstring> extensions = { L"\\*.lnk", L"\\*.url" };
    for (const auto& ext : extensions)
    {
        std::wstring filePattern = targetDir + ext;
        WIN32_FIND_DATAW findData = {};
        HANDLE hFind = FindFirstFileW(filePattern.c_str(), &findData);

        if (hFind != INVALID_HANDLE_VALUE)
        {
            do {
                // 排除資料夾，確保只拿檔案
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::wstring fullPath = targetDir + L"\\" + findData.cFileName;
                    fileResult.push_back(fullPath);
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }

    return fileResult;
}

bool ResolveLnkTarget(LPCWSTR lnkFullPath, WCHAR* outExePath, int outPathBufSize)
{
    *outExePath = 0;
    IShellLinkW* pShellLink = nullptr;
    IPersistFile* pPersistFile = nullptr;

    HRESULT hr = CoCreateInstance(CLSID_ShellLink, nullptr, CLSCTX_INPROC_SERVER, IID_IShellLinkW, (void**)&pShellLink);
    if (FAILED(hr)) return false;

    hr = pShellLink->QueryInterface(IID_IPersistFile, (void**)&pPersistFile);
    if (FAILED(hr)) { pShellLink->Release(); return false; }

    // 加载lnk文件
    hr = pPersistFile->Load(lnkFullPath, STGM_READ);
    if (SUCCEEDED(hr))
    {
        // 解析链接（SLR_NOTRACK：不启用文件跟踪，避免弹窗）
        hr = pShellLink->Resolve(nullptr, SLR_NO_UI | SLR_NOTRACK);
        if (SUCCEEDED(hr))
        {
            // 获取目标路径
            hr = pShellLink->GetPath(outExePath, outPathBufSize, nullptr, SLGP_UNCPRIORITY);
        }
    }

    if (pPersistFile) pPersistFile->Release();
    if (pShellLink) pShellLink->Release();
    return SUCCEEDED(hr);
}

#include <intshcut.h> // 💡 必須引入此標頭檔

bool ResolveUrlTarget(LPCWSTR urlFullPath, WCHAR* outExePath, int outPathBufSize, WCHAR* outIconPath, int outIconBufSize)
{
    *outExePath = 0;
    if (outIconPath) *outIconPath = 0;

    // 1. 使用 INI 配置讀取 API 來解析更高效、更安全（不怕 Steam 協議特殊格式）
    // 讀取啟動協議路徑 (steam://rungameid/xxxx)
    GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"", outExePath, outPathBufSize, urlFullPath);

    // 讀取 Steam 快取的本地圖標路徑
    if (outIconPath)
    {
        GetPrivateProfileStringW(L"InternetShortcut", L"IconFile", L"", outIconPath, outIconBufSize, urlFullPath);
    }

    // 如果成功拿到 URL，就代表解析成功
    return (wcslen(outExePath) > 0);
}
// 提取exe第0号图标（主图标）
HICON ExtractExeMainIcon(LPCWSTR exePath)
{
    HICON hIconLarge = nullptr;
    HICON hIconSmall = nullptr;
    UINT nIcons = ExtractIconEx(exePath, 0, &hIconLarge, &hIconSmall, 1);
    if (nIcons == 0)
        return nullptr;
    DestroyIcon(hIconSmall); // 小图标不用，直接释放
    return hIconLarge;
}