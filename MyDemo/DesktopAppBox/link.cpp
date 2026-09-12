#include "link.h"
#include <intshcut.h>
// 获取当前exe所在目录下所有 *.lnk 文件的完整路径
// 返回值：所有lnk全路径列表
std::vector<std::wstring> EnumLnkFilesInAppDir()
{
    std::vector<std::wstring> result;
    WCHAR exeFullPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exeFullPath, MAX_PATH);

    WCHAR appDir[MAX_PATH] = { 0 };
    WCHAR* pSlash = wcsrchr(exeFullPath, L'\\');
    if (!pSlash) return result;
    size_t dirLen = pSlash - exeFullPath;
    wcsncpy_s(appDir, exeFullPath, dirLen);
    appDir[dirLen] = L'\0';

    // 💡 同時處理 .lnk 與 .url
    std::vector<std::wstring> extensions = { L"\\lnk\\*.lnk", L"\\lnk\\*.url" };

    for (const auto& ext : extensions)
    {
        WCHAR searchPath[MAX_PATH] = { 0 };
        swprintf_s(searchPath, L"%s%s", appDir, ext.c_str());

        WIN32_FIND_DATAW findData = {};
        HANDLE hFind = FindFirstFileW(searchPath, &findData);
        if (hFind != INVALID_HANDLE_VALUE)
        {
            do {
                if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
                {
                    std::wstring fullPath = std::wstring(appDir) + L"\\lnk\\" + findData.cFileName;
                    result.push_back(fullPath);
                }
            } while (FindNextFileW(hFind, &findData));
            FindClose(hFind);
        }
    }
    return result;
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