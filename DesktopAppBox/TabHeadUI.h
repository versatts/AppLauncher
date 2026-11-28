#pragma once
#include "ImGuiObj.h"
#include <list>
#include <vector>
#include <string>

class TabHeadUI: public ImGuiObj
{
public:
	void Render();
	int m_Idx = -1;

	// 🚀 TAB 拖動排序：Render 偵測到拖放後記錄 from/to，由主循環消費
	//    （需同步重排 FolderUIData 的 gvFolder / gvFolderName，故不在此處直接執行）
	int m_pendingTabMoveFrom = -1;
	int m_pendingTabMoveTo = -1;
};

