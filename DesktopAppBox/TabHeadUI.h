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
	std::vector<std::wstring> m_arrTab;   // 磁碟目錄名（含 "NNNN-" 排序前綴，顯示時去掉）

	// 🚀 TAB 拖動排序：Render 偵測到拖放後記錄 from/to，由主循環消費
	//    （需同步重排 FolderUIData 的 gvFolder / gvFolderName，故不在此處直接執行）
	int m_pendingTabMoveFrom = -1;
	int m_pendingTabMoveTo = -1;

	// 🚀 TabBar 版本號：TAB 重排後 +1。ImGui 的 TabBar 內部會持久化 tab 順序
	//    （按 ID 存於內部陣列，首次創建時定格），BeginTabItem 的呼叫順序不會重排它；
	//    換 ID 可強制重建內部陣列，使新順序即時生效
	int m_tabBarVersion = 0;

	// 🚀 TabBar 重建後的「選中恢復期」剩餘幀數（含超時保護）：
	//    重建後 ImGui 內部選中狀態歸零、會自動選中第一個 tab（Useful），
	//    若照單接受 BeginTabItem 的回寫會把 m_Idx 覆蓋回 0。恢復期內：
	//    1) 每幀對 m_Idx 的 tab 傳 SetSelected，直到選中真正對齊；
	//    2) 不回寫 m_Idx，m_Idx 保持為被拖分頁的新位置。
	//    對齊成功或倒數歸零後恢復常規點擊切換。
	int m_restoreCountdown = 0;
};

