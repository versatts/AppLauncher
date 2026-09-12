#include "link.h"

// 获取当前exe所在目录下所有 *.lnk 文件的完整路径
// 返回值：所有lnk全路径列表
std::vector<std::wstring> EnumLnkFilesInAppDir()
{
    std::vector<std::wstring> result;

    WCHAR exeFullPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exeFullPath, MAX_PATH);

    // 提取目录（去掉exe文件名）
    WCHAR appDir[MAX_PATH] = { 0 };
    WCHAR* pSlash = wcsrchr(exeFullPath, L'\\');
    if (!pSlash)
        return result;
    size_t dirLen = pSlash - exeFullPath;
    wcsncpy_s(appDir, exeFullPath, dirLen);
    appDir[dirLen] = L'\0';

    // 拼接搜索通配符：C:\xxx\*.lnk
    WCHAR searchPath[MAX_PATH] = { 0 };
    swprintf_s(searchPath, L"%s\\lnk\\*.lnk", appDir);

    WIN32_FIND_DATAW findData = {};
    HANDLE hFind = FindFirstFileW(searchPath, &findData);
    if (hFind == INVALID_HANDLE_VALUE)
        return result;

    do
    {
        // 跳过目录，只取文件
        if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
            std::wstring fullPath = std::wstring(appDir) + L"\\lnk\\" + findData.cFileName;
            result.push_back(fullPath);
        }
    } while (FindNextFileW(hFind, &findData));

    FindClose(hFind);
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