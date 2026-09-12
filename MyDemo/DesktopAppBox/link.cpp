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

// 輔助函式：從 Windows 註冊表自動撈出當前系統預設瀏覽器的可執行檔 (.exe) 路徑
std::wstring GetDefaultBrowserExePath()
{
    WCHAR exePath[MAX_PATH] = { 0 };
    DWORD bufSize = sizeof(exePath);
    HKEY hKey;

    // 1. 尋找當前用戶預設處理 http 協議的關聯名稱 (例如 "ChromeHTML", "MSEdgeHTM")
    LPCWSTR regPath = L"Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\http\\UserChoice";
    if (RegOpenKeyExW(HKEY_CURRENT_USER, regPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        WCHAR progId[256] = { 0 };
        DWORD progIdSize = sizeof(progId);
        if (RegQueryValueExW(hKey, L"ProgId", NULL, NULL, (LPBYTE)progId, &progIdSize) == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);

            // 2. 拿著 ProgId 去 Classes 核心尋找它真實對應的啟動指令路徑
            std::wstring commandPath = std::wstring(L"") + progId + L"\\shell\\open\\command";
            if (RegOpenKeyExW(HKEY_CLASSES_ROOT, commandPath.c_str(), 0, KEY_READ, &hKey) == ERROR_SUCCESS)
            {
                WCHAR rawCommand[MAX_PATH] = { 0 };
                DWORD rawSize = sizeof(rawCommand);
                if (RegQueryValueExW(hKey, L"", NULL, NULL, (LPBYTE)rawCommand, &rawSize) == ERROR_SUCCESS)
                {
                    // 3. 得到的字串通常帶有引號和參數，例如: "C:\Program Files\...\chrome.exe" -- "%1"
                    // 我們需要將真實的 .exe 路徑提取出來
                    std::wstring cmdStr = rawCommand;
                    size_t firstQuote = cmdStr.find(L"\"");
                    if (firstQuote != std::wstring::npos)
                    {
                        size_t secondQuote = cmdStr.find(L"\"", firstQuote + 1);
                        if (secondQuote != std::wstring::npos)
                        {
                            // 擷取雙引號中間的純路徑
                            std::wstring purePath = cmdStr.substr(firstQuote + 1, secondQuote - firstQuote - 1);
                            RegCloseKey(hKey);
                            return purePath;
                        }
                    }

                    // 如果沒有雙引號，截斷空格前的路徑
                    size_t firstSpace = cmdStr.find(L" ");
                    if (firstSpace != std::wstring::npos) {
                        RegCloseKey(hKey);
                        return cmdStr.substr(0, firstSpace);
                    }
                    RegCloseKey(hKey);
                    return cmdStr;
                }
                RegCloseKey(hKey);
            }
        }
        else {
            RegCloseKey(hKey);
        }
    }

    // 4. 極端防禦降級：如果用戶清空了預設關聯，強行返回系統自帶的 Edge 瀏覽器絕對路徑
    return L"C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe";
}


#include <intshcut.h> // 💡 必須引入此標頭檔

// 升級後的變體：支援自動修正缺失圖標的網頁快捷方式
bool ResolveUrlTarget(LPCWSTR urlFullPath, WCHAR* outExePath, int outPathBufSize, WCHAR* outIconPath, int outIconBufSize)
{
    *outExePath = 0;
    if (outIconPath) *outIconPath = 0;

    // 1. 讀取啟動協議路徑 (例如 https://google.com 或 steam://rungameid/xxx)
    GetPrivateProfileStringW(L"InternetShortcut", L"URL", L"", outExePath, outPathBufSize, urlFullPath);
    if (wcslen(outExePath) == 0)
    {
        return false; // 根本不是合法的 .url 檔案
    }

    // 2. 讀取圖標欄位
    if (outIconPath)
    {
        GetPrivateProfileStringW(L"InternetShortcut", L"IconFile", L"", outIconPath, outIconBufSize, urlFullPath);

        // 💥 【核心升級防禦】如果圖標路徑為空（代表這是一個標準的網頁連結快捷，不是 Steam 遊戲）
        if (wcslen(outIconPath) == 0)
        {
            // 自動去撈取當前電腦預設瀏覽器的 exe 路徑，拿它當作圖標來源！
            std::wstring defaultBrowser = GetDefaultBrowserExePath();
            wcsncpy_s(outIconPath, outIconBufSize, defaultBrowser.c_str(), _TRUNCATE);
        }
    }

    return true;
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