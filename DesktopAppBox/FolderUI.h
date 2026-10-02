#pragma once
#include "ImGuiObj.h"
#include <windows.h>
#include <vector>
#include <string>

class FolderUIData;

class FolderUI:public ImGuiObj
{
public:
	void Render(HWND hwnd, FolderUIData&);

	// 🚀 Explorer 圖標拖入：把 .ico/.png 套用到「客戶區座標 pt 命中的項目」上
	//    （拷貝到該項目 lnk/url 同目錄，並改名為同名覆蓋圖標）
	//    返回 false = 沒有命中任何項目或拷貝失敗
	bool ApplyIconToItem(POINT clientPt, const std::wstring& srcIconPath);

private:
	// 每幀渲染時更新的項目格子矩形（客戶區絕對座標），供拖入命中測試
	struct ItemRect
	{
		float MinX = 0.0f, MinY = 0.0f, MaxX = 0.0f, MaxY = 0.0f;
		std::wstring sLnkPath;
	};
	std::vector<ItemRect> m_dropRects;

	// 最近一幀滑鼠懸停的項目（Explorer 拖放期間 hover 正常更新，
	// 作為矩形命中失敗時的兜底，不受座標換算影響）
	std::wstring m_hoverLnkPath;
};
