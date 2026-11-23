#include "AppConfig.h"
#include <windows.h>
#include <TCHAR.h>
#include "ImGuiObj.h"
#include <shlobj.h> // 用於取得使用者字體目錄 (可選)
#include <algorithm>
void AppConfig::Save(FILE* f)
{
    fprintf(f, "\n[STYLE]\n");
    fprintf(f, "EnFont=%s\n", gCfg.m_sEnFont.c_str());
    fprintf(f, "EnableDrag=%d\n", gCfg.m_bEnableFileDragIO);

	fprintf(f, "FontBold=%d\n", gCfg.m_bFontBold ? 1 : 0);
	fprintf(f, "FontItalic=%d\n", gCfg.m_bFontItalic ? 1 : 0);
	fprintf(f, "FontSize=%d\n", gCfg.m_nFontSize);
}
AppConfig::AppConfig()
{
    LoadSystemFonts();

    // 1. 取得當前 exe 所在目錄
    WCHAR exeFullPath[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, exeFullPath, MAX_PATH);

    WCHAR appDir[MAX_PATH] = { 0 };
    WCHAR* pSlash = wcsrchr(exeFullPath, L'\\');
    
    size_t dirLen = pSlash - exeFullPath;
    wcsncpy_s(appDir, exeFullPath, dirLen);
    appDir[dirLen] = L'\0';

    std::wstring sIni = appDir;
    sIni += _T("\\imgui.ini");
	TCHAR szTemp[256];
	GetPrivateProfileString(_T("STYLE"), _T("EnFont"), _T("tahomabd.ttf"), szTemp, 255, sIni.c_str());
	m_sEnFont = ImGuiObj::ToU8(szTemp);

    int i = GetPrivateProfileInt(_T("STYLE"), _T("EnableDrag"), 1, sIni.c_str());
    m_bEnableFileDragIO = i;

	m_bFontBold = GetPrivateProfileInt(_T("STYLE"), _T("FontBold"), 0, sIni.c_str()) != 0;
	m_bFontItalic = GetPrivateProfileInt(_T("STYLE"), _T("FontItalic"), 0, sIni.c_str()) != 0;
	m_nFontSize = GetPrivateProfileInt(_T("STYLE"), _T("FontSize"), 18, sIni.c_str());

	// ====== 新增：根據讀取到的 EnFont 自動還原 m_nCurrentFontIndex ======
	m_nCurrentFontIndex = 0; // 預設值
	for (size_t idx = 0; idx < m_vSystemFonts.size(); ++idx)
	{
		if (m_vSystemFonts[idx] == m_sEnFont)
		{
			m_nCurrentFontIndex = static_cast<int>(idx);
			break;
		}
	}
}

void AppConfig::LoadSystemFonts()
{
    m_vSystemFonts.clear();

    // 輔助 Lambda：掃描指定資料夾中的所有 .ttf / .ttc / .otf 字體檔，並轉為 UTF-8 存入列表
    auto ScanFontDir = [this](const std::wstring& dirPath)
        {
            std::wstring searchPath = dirPath + L"\\*.*";
            WIN32_FIND_DATAW findData;
            HANDLE hFind = ::FindFirstFileW(searchPath.c_str(), &findData);
            if (hFind == INVALID_HANDLE_VALUE)
                return;

            do
            {
                if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    continue;

                std::wstring fileName(findData.cFileName);
                if (fileName.size() <= 4)
                    continue;

                // 檢查副檔名（不分大小寫支援 .ttf, .ttc, .otf）
                const wchar_t* ext = fileName.c_str() + fileName.size() - 4;
                if (_wcsicmp(ext, L".ttf") == 0 ||
                    _wcsicmp(ext, L".ttc") == 0 ||
                    _wcsicmp(ext, L".otf") == 0)
                {
                    // 將寬字元檔名轉為 UTF-8，確保包含中文字元的字體檔名在 ImGui 下拉選單正常顯示
                    int utf8Len = ::WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, nullptr, 0, nullptr, nullptr);
                    if (utf8Len > 1)
                    {
                        std::string utf8Name(utf8Len - 1, '\0');
                        ::WideCharToMultiByte(CP_UTF8, 0, fileName.c_str(), -1, &utf8Name[0], utf8Len, nullptr, nullptr);
                        m_vSystemFonts.push_back(utf8Name);
                    }
                }
            } while (::FindNextFileW(hFind, &findData));

            ::FindClose(hFind);
        };

    // 1. 掃描系統字體目錄 (C:\Windows\Fonts)
    wchar_t szWinDir[MAX_PATH] = { 0 };
    if (::GetWindowsDirectoryW(szWinDir, MAX_PATH) > 0)
    {
        ScanFontDir(std::wstring(szWinDir) + L"\\Fonts");
    }

    // 2. 掃描當前使用者個人安裝的字體目錄 (Win10/11 雙擊安裝字體的預設位置)
    wchar_t szLocalAppData[MAX_PATH] = { 0 };
    if (SUCCEEDED(::SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, szLocalAppData)))
    {
        ScanFontDir(std::wstring(szLocalAppData) + L"\\Microsoft\\Windows\\Fonts");
    }

    // 3. 依字母順序排序，方便在下拉選單中尋找（不分大小寫）
    std::sort(m_vSystemFonts.begin(), m_vSystemFonts.end(), [](const std::string& a, const std::string& b) {
        return _stricmp(a.c_str(), b.c_str()) < 0;
        });
}
