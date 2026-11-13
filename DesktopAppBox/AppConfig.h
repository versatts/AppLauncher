#pragma once
#include <string>
#include <vector>
class AppConfig
{
public:
	AppConfig();
	bool m_bEnableFileDragIO = true;
	std::string m_sEnFont;


	void Save(FILE* f);


	std::vector<std::string> m_vSystemFonts;
	void LoadSystemFonts();

	int m_nCurrentFontIndex = 0; // 當前選擇的字體索引
	bool m_bFontBold = false;      // 是否粗體
	bool m_bFontItalic = false;    // 是否斜體
	int m_nFontSize = 18;          // 字號（預設 18）
};


extern AppConfig gCfg;
