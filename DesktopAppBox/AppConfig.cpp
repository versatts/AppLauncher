#include "AppConfig.h"
#include <windows.h>
#include <TCHAR.h>
#include "ImGuiObj.h"

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

	// 1. 取得 Windows 系統目錄 (通常是 C:\Windows)
	char szWinDir[MAX_PATH];
	if (GetWindowsDirectoryA(szWinDir, MAX_PATH) == 0)
	{
		return; // 取得失敗則返回
	}

	// 2. 拼接出字體資料夾的搜尋路徑 (例如 C:\Windows\Fonts\*.ttf)
	std::string sFontSearchPath = std::string(szWinDir) + "\\Fonts\\*.ttf";

	// 3. 開始搜尋檔案
	WIN32_FIND_DATAA findData;
	HANDLE hFind = FindFirstFileA(sFontSearchPath.c_str(), &findData);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			// 排除資料夾，只處理檔案
			if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				// 僅儲存檔名 (例如: Arial.ttf)
				m_vSystemFonts.push_back(findData.cFileName);

				// 如果需要完整路徑，可以改用下面這行：
				// m_vSystemFonts.push_back(std::string(szWinDir) + "\\Fonts\\" + findData.cFileName);
			}
		} while (FindNextFileA(hFind, &findData));

		FindClose(hFind); // 關閉搜尋句柄
	}
}
