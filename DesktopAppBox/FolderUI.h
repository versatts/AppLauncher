#pragma once
#include "ImGuiObj.h"
#include <windows.h>
#include <vector>
#include <string>

#define ICON_SIZE 64
class FolderUIData;
struct ST_APP;

    // ============================================================
    // 網格排版參數與幾何計算結果
    // ============================================================
    struct GridLayout
    {
        // 基礎參數
        ImVec2 iconSize        = ImVec2(ICON_SIZE, ICON_SIZE);
        float  itemSpacingX    = 4.0f;   // 圖標之間固定的橫向間距
        float  itemSpacingY    = 4.0f;  // 圖標單元之間的縱向間距
        float  textBlockHeight = 22.0f;  // 文字塊的固定高度
        float  textGap         = 4.0f;   // 圖標與下方文字塊之間的微小間距
        float  sidePadding      = 5.0f;  // 左右最小 Padding
        float  topPadding      = 5.0f;   // 上邊沿固定的 Padding

        // 計算結果
        bool   bShowText       = false;
        ImVec2 btnSize;
        float  totalCellHeight = 0.0f;   // 單個單元（含文字塊）總高
        float  dynamicPadding  = 0.0f;   // 水平置中 Padding
        int    maxItemsPerRow  = 1;
        ImVec2 startPos        = ImVec2(0.0f, 0.0f); // 內容繪製起始位置（已含 topPadding）
    };


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

public:
    int btnPadding = 6;

    // ============================================================
    // 🚀 拖出狀態機：拖著項目離開視窗外後進入「待確認移除」狀態
    //    - 在視窗外鬆開滑鼠 → 直接刪除 lnk 檔案（不拖放到任何目錄）
    //    - 拖回視窗內鬆開 → 視為普通排序，移動到鬆開點所在格子
    //    - 按 ESC → 取消
    // ============================================================
    bool s_dragOutArmed = false;
    int  s_dragOutFromIdx = -1;
    std::wstring s_dragOutLnkPath;

    bool m_bShowTip = false;
private:

    GridLayout CalcGridLayout(bool bShowText, size_t itemCount);

    std::string TruncateUtf8WithEllipsis(const std::string& text, float maxWidth);

    void RenderAppButton(HWND hwnd, const ST_APP& app, const char* btnId, size_t itemIdx,
        ImVec2 iconSize, int padding, int& moveFrom, int& moveTo);

    void ApplyItemRename(ST_APP& app, const char* newNameUtf8);

    void DrawTextBlock(ST_APP& app, size_t itemIdx, const GridLayout& g);

    void DrawItemTooltip(const ST_APP& app, const GridLayout& g);
 
};
