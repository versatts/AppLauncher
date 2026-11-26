#include "CustomUI.h"
#if IMGUI
 
CustomUI::FnFillCustom CustomUI::s_pFnFillCustom = nullptr;

std::string CustomUI::weekdays[7];
std::string CustomUI::unitYear;
std::string CustomUI::unitMonth;
std::string CustomUI::selecdataTips;
std::string CustomUI::searchTips;

#include <ctime>
#include <tchar.h>
#include <vector>
#include <cmath>

// =========================================================================
// 内部辅助函数 1：绘制右侧箭头图标（支持自定义 ImTextureID 与默认折线箭头）
// =========================================================================
static void DrawComboArrow(
	ImDrawList* draw_list,
	const ImVec2& rect_min,
	const ImVec2& rect_max,
	ImTextureID arrow_icon,
	const ImVec2& icon_size,
	bool is_open,
	bool is_hovered,
	bool is_active)
{
	const float margin = 4.0f;
	if (arrow_icon)
	{
		ImVec2 img_min(
			rect_max.x - margin - icon_size.x,
			std::floor((rect_min.y + rect_max.y - icon_size.y) * 0.5f)
		);
		ImVec2 img_max(img_min.x + icon_size.x, img_min.y + icon_size.y);

		ImVec4 btn_col;
		if (is_open || is_active)
		{
			btn_col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
			btn_col.w = 1.0f;
		}
		else if (is_hovered)
		{
			btn_col = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
			btn_col.w = 0.90f;
		}
		else
		{
			btn_col = ImGui::GetStyleColorVec4(ImGuiCol_Button);
			btn_col.w = 0.75f;
		}

		ImU32 tint_col = ImGui::ColorConvertFloat4ToU32(btn_col);
		ImVec2 uv0 = is_open ? ImVec2(0.0f, 1.0f) : ImVec2(0.0f, 0.0f);
		ImVec2 uv1 = is_open ? ImVec2(1.0f, 0.0f) : ImVec2(1.0f, 1.0f);

		draw_list->AddImage(arrow_icon, img_min, img_max, uv0, uv1, tint_col);
	}
	else
	{
		float arrow_r = 5.0f;
		ImVec2 center(rect_max.x - margin - icon_size.x * 0.5f, (rect_min.y + rect_max.y) * 0.5f);
		float dir = is_open ? -1.0f : 1.0f;
		ImVec2 pts[3] = {
			ImVec2(center.x - arrow_r, center.y - arrow_r * 0.5f * dir),
			ImVec2(center.x,           center.y + arrow_r * 0.5f * dir),
			ImVec2(center.x + arrow_r, center.y - arrow_r * 0.5f * dir)
		};
		draw_list->AddPolyline(pts, 3, IM_COL32(200, 200, 200, 255), ImDrawFlags_None, 1.5f);
	}
}

// =========================================================================
// 内部辅助函数 2：渲染下拉列表选项（统一处理重复项 ID 与空字符串防护）
// =========================================================================
static bool RenderComboItems(const char* const items[], int items_count, int* current_item, const char* current_text = nullptr)
{
	bool changed = false;
	for (int i = 0; i < items_count; i++)
	{
		ImGui::PushID(i);
		const char* str = items[i] ? items[i] : "";
		const bool is_selected = current_item ? (*current_item == i) : (current_text && strcmp(current_text, str) == 0);
		const char* item_label = (str[0] == '\0') ? "##empty" : str;

		if (ImGui::Selectable(item_label, is_selected))
		{
			if (current_item) *current_item = i;
			changed = true;
		}
		if (is_selected)
			ImGui::SetItemDefaultFocus();
		ImGui::PopID();
	}
	return changed;
}

bool CustomUI::IconTextButton(const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled)
{
    return IconTextButtonCore(label, btnSize, icon, iconSize, false, false, bEnabled);
}

bool CustomUI::IconTextToggleButton(bool* v, const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled)
{
    assert(v);
    const bool pressed = IconTextButtonCore(label, btnSize, icon, iconSize, true, *v, bEnabled);
    if (pressed)
        *v = !(*v);
    return pressed;
}

bool CustomUI::IconTextButtonCore(const char* label, const ImVec2& btnSize, ImTextureID icon, const ImVec2& iconSize, bool isToggledBtn, bool bToggled, bool bEnabled)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    if (!bEnabled)
        ImGui::BeginDisabled(true);

    const ImGuiStyle& style = ImGui::GetStyle();
    const float padX = style.FramePadding.x;
    const float padY = style.FramePadding.y;
	ImVec2 labelSize(0, 0);
	const char* textEnd = nullptr;
	if (label)
	{
		textEnd = ImGui::FindRenderedTextEnd(label);
		labelSize = ImGui::CalcTextSize(label, textEnd);
		if (iconSize.x > 0)
		{
			labelSize.x += 2.0f;
		}
	}

    // 1. 計算內容寬度與按鈕尺寸
    const float spacing = (labelSize.x > 1e-6 && iconSize.x > 1e-6) ? style.ItemInnerSpacing.x : 0.0f;
    float contentWidth = iconSize.x + spacing + labelSize.x;
    if (btnSize.x > 0.0f)
        contentWidth = ImMin(contentWidth, ImMax(0.0f, btnSize.x - padX * 2.0f));

    ImVec2 size = btnSize;
    if (size.x <= 0.0f)
        size.x = contentWidth + padX * 2.0f;
    if (size.y <= 0.0f)
        size.y = ImMax(iconSize.y, labelSize.y) + padY * 2.0f;

    // 2. 處理互動狀態與佈局註冊
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    // 分配 UI 空間，並計算排版游標
    ImGui::ItemSize(size);

    // 取得唯一的元件 ID，並加入到視窗裁剪判定中
	ImGuiID id;
	if (label)
		id = window->GetID(label);
	else
		id = window->GetID(icon);
    if (!ImGui::ItemAdd(bb, id))
    {
        if (!bEnabled) ImGui::EndDisabled();
        return false;
    }

    // 啟用此元件的導覽圖層，允許鍵盤選取
    window->DC.NavLayersActiveMaskNext |= (1 << window->DC.NavLayerCurrent);

    // 同時處理滑鼠與鍵盤按壓事件的核心函數
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

    // 3. 計算背景、文字與圖示顏色（不操作全域 Style Stack）
    const bool isDimmed = (isToggledBtn && !bToggled);
    const float contentAlpha = isDimmed ? 0.7f : 1.0f;

    ImVec4 textVec = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    textVec.w *= contentAlpha;
    const ImU32 textCol = ImGui::GetColorU32(textVec);
    const ImU32 iconTint = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, contentAlpha));

    const ImU32 bgCol = ImGui::GetColorU32(
        held ? ImGuiCol_ButtonActive : (
            hovered ? ImGuiCol_ButtonHovered :
            ((isToggledBtn && bToggled)) ? ImGuiCol_ButtonActive : ImGuiCol_Button)
    );

    // 4. 繪製背景與邊框
    ImDrawList* drawList = window->DrawList;
    const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
    float fRounding = style.FrameRounding;

    if (!s_pFnFillCustom)
    {
        drawList->AddRectFilled(pos, rectMax, bgCol, fRounding);
    }
    else
    {
        s_pFnFillCustom(drawList, pos, rectMax, hovered, held, pressed, isToggledBtn, bToggled, bEnabled);
        s_pFnFillCustom = nullptr;
    }

    if (style.FrameBorderSize > 0.0f)
        drawList->AddRect(pos, rectMax, ImGui::GetColorU32(ImGuiCol_Border), fRounding, 0, style.FrameBorderSize);

    // 【新增這段】當玩家用鍵盤 Tab 切換到此按鈕時，畫出選取高亮框
    if (ImGui::GetFocusID() == id)
    {
        ImGui::RenderNavHighlight(bb, id);
    }

    if (isToggledBtn && bToggled)
    {
        // 方案 A：使用 ImGui 預設的高亮藍色（推薦，會跟隨主題風格變換且支援 Disabled 透明度）
        const ImU32 lineCol = ImGui::GetColorU32(ImGuiCol_NavHighlight);

        // 方案 B：如果您想要強制指定特定藍色（例如純藍），請解開下方註解並替換 lineCol：
        // const ImU32 lineCol = ImGui::GetColorU32(ImVec4(0.0f, 0.47f, 0.83f, style.Alpha)); // 考慮了 Disabled 的 Alpha

        const float lineWidth = size.y * 0.05f;

        // 計算線段的起點與終點（位於按鈕的最底端）
        // 為了避免線段超出圓角，兩端各往內縮 1 像素（可依視覺需求調整）
        ImVec2 lineStart(pos.x, pos.y + size.y - lineWidth);
        ImVec2 lineEnd(pos.x + size.x, pos.y + size.y);

        drawList->AddRectFilled(lineStart, lineEnd, lineCol, fRounding, ImDrawFlags_RoundCornersBottom);
    }


    // 5. 計算置中起點並繪製圖示與文字
    float currentX = pos.x + (size.x - contentWidth) * 0.5f;

    if (icon != 0 && iconSize.x > 0.0f && iconSize.y > 0.0f)
    {
        const float iconY = pos.y + (size.y - iconSize.y) * 0.5f;
        drawList->AddImage(icon, ImVec2(currentX, iconY), ImVec2(currentX + iconSize.x, iconY + iconSize.y), ImVec2(0, 0), ImVec2(1, 1), iconTint);
    }
    currentX += iconSize.x + spacing;

	if (label)
    {
        const float textY = pos.y + (size.y - labelSize.y) * 0.5f;
        const ImVec4 clipRect(pos.x + padX, pos.y, rectMax.x - padX, rectMax.y);
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(currentX, textY),
            textCol,
            label,
            textEnd,
            0.0f,
            &clipRect
        );
    }

    if (!bEnabled)
        ImGui::EndDisabled();

    return pressed;
}


constexpr float kPadding = 16.0f;        // 外围内边距

// 视图模式
enum CalendarView {
	VIEW_DATE,    // 日期选择
	VIEW_YEAR,    // 年份选择
	VIEW_MONTH    // 月份选择
};

// 日期数据
static int g_dispYear = 2025, g_dispMonth = 10;  // 当前显示的年月
static CalendarView g_view = VIEW_DATE;

// 年份选择范围
static int g_yearStart = 2014, g_yearEnd = 2025;
static int g_yearPageStart = 2014;  // 当前页起始年份

// 弹窗状态
static bool g_showCalendar = false;
static ImVec2 g_calendarPos;
static int g_currentField = 0;  // 0=起始, 1=结束

constexpr float kBtnSize = 32.0f;
constexpr float kCellSize = 24.0f;
constexpr float kCellGap = 16.0f;
constexpr float kHeaderPaddingX = 8.0f;
constexpr float kHeaderPaddingY = 4.0f;
constexpr float kGridPadding = 16.0f;
constexpr float kSeparatorY = 8.0f;
constexpr int kCols = 7;
constexpr int kYearCols = 3;
constexpr int kMonthCols = 3;

// 弹窗宽度 = 16 + 7*24 + 6*16 + 16 = 296
constexpr float kCalendarWidth = kGridPadding * 2 + kCols * kCellSize + (kCols - 1) * kCellGap;

int GetDaysInMonth(int year, int month) {
	static const int days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
	if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
		return 29;
	return days[month - 1];
}

int GetWeekday(int year, int month, int day) {
	struct tm tm_info = {};
	tm_info.tm_year = year - 1900;
	tm_info.tm_mon = month - 1;
	tm_info.tm_mday = day;
	tm_info.tm_isdst = -1;
	mktime(&tm_info);
	int wday = tm_info.tm_wday;  // 0=周日
	return (wday == 0) ? 6 : wday - 1;  // 转为 0=周一
}

bool DrawNavButton(const char* label, ImVec2 pos, float size = kBtnSize) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 mouse = ImGui::GetIO().MousePos;

	bool hovered = (mouse.x >= pos.x && mouse.x <= pos.x + size &&
		mouse.y >= pos.y && mouse.y <= pos.y + size);
	bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	// 背景
	ImU32 bgColor = clicked ? IM_COL32(100, 200, 200, 255) :  // 绿色（点击）
		(hovered ? IM_COL32(80, 80, 100, 255) : IM_COL32(0, 0, 0, 0));  // 灰色（悬停）

	if (hovered || clicked) {
		dl->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), bgColor, 4.0f);
	}

	// 文字
	ImVec2 textSize = ImGui::CalcTextSize(label);
	ImVec2 textPos(pos.x + (size - textSize.x) * 0.5f,
		pos.y + (size - textSize.y) * 0.5f);
	dl->AddText(textPos, IM_COL32(255, 255, 255, 255), label);

	return clicked;
}

bool DrawDateCell(const char* label, ImVec2 pos, bool isSelected, bool isCurrentMonth) {
	if (!isCurrentMonth) return false;

	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 mouse = ImGui::GetIO().MousePos;

	bool hovered = (mouse.x >= pos.x && mouse.x <= pos.x + kCellSize &&
		mouse.y >= pos.y && mouse.y <= pos.y + kCellSize);
	bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	// 背景
	ImU32 bgColor;
	if (isSelected) {
		bgColor = IM_COL32(100, 200, 200, 255);  // 选中绿色
	}
	else if (clicked) {
		bgColor = IM_COL32(100, 200, 200, 255);  // 点击绿色
	}
	else if (hovered) {
		bgColor = IM_COL32(80, 80, 100, 255);    // 悬停灰色
	}
	else {
		return false;  // 无状态不绘制背景
	}

	dl->AddRectFilled(pos, ImVec2(pos.x + kCellSize, pos.y + kCellSize), bgColor);//, 3.0f

	// 文字
	ImVec2 textSize = ImGui::CalcTextSize(label);
	ImVec2 textPos(pos.x + (kCellSize - textSize.x) * 0.5f,
		pos.y + (kCellSize - textSize.y) * 0.5f);

	ImU32 textColor = isSelected ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
	dl->AddText(textPos, textColor, label);

	return clicked;
}

bool DrawYearCell(int year, ImVec2 pos, bool isSelected) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 mouse = ImGui::GetIO().MousePos;

	char buf[16];
	snprintf(buf, sizeof(buf), "%d", year);
	ImVec2 textSize = ImGui::CalcTextSize(buf);

	// 单元格区域（比文字大一些，方便点击）
	float cellW = 56.0f;
	float cellH = 24.0f;
// 	ImVec2 cellPos(pos.x - (cellW - textSize.x) * 0.5f,
// 		pos.y - (cellH - textSize.y) * 0.5f);
	ImVec2 cellPos(pos.x - (cellW) * 0.5f,
		pos.y - (cellH) * 0.5f);

	bool hovered = (mouse.x >= cellPos.x && mouse.x <= cellPos.x + cellW &&
		mouse.y >= cellPos.y && mouse.y <= cellPos.y + cellH);
	bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	ImU32 bgColor;
	if (isSelected) {
		bgColor = IM_COL32(100, 200, 200, 255);
	}
	else if (clicked) {
		bgColor = IM_COL32(100, 200, 200, 255);
	}
	else if (hovered) {
		bgColor = IM_COL32(80, 80, 100, 255);
	}
	else {
		bgColor = IM_COL32(55, 55, 61, 255);
	}

	dl->AddRectFilled(cellPos, ImVec2(cellPos.x + cellW, cellPos.y + cellH), bgColor);//, 3.0f

	ImVec2 textPos(pos.x - textSize.x * 0.5f, pos.y - textSize.y * 0.5f);
	ImU32 textColor = isSelected ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
	dl->AddText(textPos, textColor, buf);

	return clicked;
}


bool DrawMonthCell(int month, ImVec2 pos, bool isSelected) {
	ImDrawList* dl = ImGui::GetWindowDrawList();
	ImVec2 mouse = ImGui::GetIO().MousePos;

	char buf[16];
	snprintf(buf, sizeof(buf), u8"%d%s", month, CustomUI::unitMonth.c_str());
	ImVec2 textSize = ImGui::CalcTextSize(buf);

	float cellW = 56.0f;
	float cellH = 24.0f;
// 	ImVec2 cellPos(pos.x - (cellW - textSize.x) * 0.5f,
// 		pos.y - (cellH - textSize.y) * 0.5f);
	ImVec2 cellPos(pos.x - (cellW) * 0.5f,
		pos.y - (cellH) * 0.5f);

	bool hovered = (mouse.x >= cellPos.x && mouse.x <= cellPos.x + cellW &&
		mouse.y >= cellPos.y && mouse.y <= cellPos.y + cellH);
	bool clicked = hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

	ImU32 bgColor;
	if (isSelected) {
		bgColor = IM_COL32(100, 200, 200, 255);
	}
	else if (clicked) {
		bgColor = IM_COL32(100, 200, 200, 255);
	}
	else if (hovered) {
		bgColor = IM_COL32(80, 80, 100, 255);
	}
	else {
		bgColor = IM_COL32(55, 55, 61, 255);
	}

	dl->AddRectFilled(cellPos, ImVec2(cellPos.x + cellW, cellPos.y + cellH), bgColor);//, 3.0f

	ImVec2 textPos(pos.x - textSize.x * 0.5f, pos.y - textSize.y * 0.5f);
	ImU32 textColor = isSelected ? IM_COL32(0, 0, 0, 255) : IM_COL32(255, 255, 255, 255);
	dl->AddText(textPos, textColor, buf);

	return clicked;
}


bool DrawDateView(ImDrawList* dl, ImVec2 winPos, int* year, int* month, int* day) {
	bool date_changed = false;
	float x = winPos.x + kGridPadding;
	float y = winPos.y + kGridPadding + kCellSize + kCellGap;

	int daysInMonth = GetDaysInMonth(g_dispYear, g_dispMonth);
	int firstWeekday = GetWeekday(g_dispYear, g_dispMonth, 1);
	int prevMonthDays = GetDaysInMonth(
		g_dispMonth == 1 ? g_dispYear - 1 : g_dispYear,
		g_dispMonth == 1 ? 12 : g_dispMonth - 1);

	// 星期行
// 	const char* weekNames[] = { u8"一", u8"二", u8"三", u8"四", u8"五", u8"六", u8"日" };
	for (int dow = 0; dow < 7; dow++) {
		float cellX = x + dow * (kCellSize + kCellGap);
		ImVec2 textPos(cellX + (kCellSize - ImGui::CalcTextSize(CustomUI::weekdays[dow].c_str()).x) * 0.5f, y);
		ImU32 color = (dow >= 5) ? IM_COL32(255, 100, 100, 255) : IM_COL32(255, 255, 255, 255);
		dl->AddText(textPos, color, CustomUI::weekdays[dow].c_str());
	}

	y += kCellSize + kCellGap;

	// 日期网格
	int iday = 1;
	int nextMonthDay = 1;

	for (int week = 0; week < 6; week++) {
		for (int dow = 0; dow < 7; dow++) {
			float cellX = x + dow * (kCellSize + kCellGap);
			ImVec2 cellPos(cellX, y);

			char buf[8];
			ImU32 textColor;
			bool isCurrentMonth = false;
			int cellDay = 0;
			int cellIndex = week * 7 + dow;

			if (cellIndex < firstWeekday) {
				cellDay = prevMonthDays - firstWeekday + cellIndex + 1;
				snprintf(buf, sizeof(buf), "%d", cellDay);
				textColor = IM_COL32(120, 120, 140, 255);
			}
			else if (cellIndex < firstWeekday + daysInMonth) {
				cellDay = iday++;
				snprintf(buf, sizeof(buf), "%d", cellDay);
				isCurrentMonth = true;

				if (cellDay == *day  && g_dispMonth == *month && g_dispYear == *year) {//g_calDay g_calMonth g_calYear
					textColor = IM_COL32(0, 0, 0, 255);
				}
				else if (dow >= 5) {
					textColor = IM_COL32(255, 100, 100, 255);
				}
				else {
					textColor = IM_COL32(255, 255, 255, 255);
				}
			}
			else {
				cellDay = nextMonthDay++;
				snprintf(buf, sizeof(buf), "%d", cellDay);
				textColor = IM_COL32(120, 120, 140, 255);
			}

			// 绘制背景（悬停/选中）
			DrawDateCell(buf, cellPos,
				(isCurrentMonth && cellDay == *day && g_dispMonth == *month && g_dispYear == *year),//g_calDay
				isCurrentMonth);

			// 绘制文字（如果没有背景）
			ImVec2 textSize = ImGui::CalcTextSize(buf);
			ImVec2 textPos(cellPos.x + (kCellSize - textSize.x) * 0.5f,
				cellPos.y + (kCellSize - textSize.y) * 0.5f);
			dl->AddText(textPos, textColor, buf);

			// 点击
			ImVec2 mouse = ImGui::GetIO().MousePos;
			bool hovered = (mouse.x >= cellPos.x && mouse.x <= cellPos.x + kCellSize &&
				mouse.y >= cellPos.y && mouse.y <= cellPos.y + kCellSize);
			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && isCurrentMonth) {
				*day = cellDay;
				*month = g_dispMonth;
				*year = g_dispYear;
				// 通知外部
				date_changed = true;
			}
		}

		y += kCellSize + kCellGap;
		if (iday > daysInMonth && week >= 4) break;
	}

	return date_changed;
}


bool DrawYearView(ImDrawList* dl, ImVec2 winPos, int* paramY) {
	bool date_changed = false;
// 	float x = winPos.x + kGridPadding;
// 	float y = winPos.y + kGridPadding + 20;

	float x = winPos.x + 56 / 2 + 2;
	float y = winPos.y + kGridPadding + kGridPadding + kCellGap + 24 + 12 + 8; //kGridPadding + kCellSize + 

	int yearsPerPage = 12;  // 3列 × 4行
	int year = g_yearPageStart;

	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < kYearCols; col++) {
			if (year > g_yearEnd) break;

			float cellX = x + col * (kCalendarWidth - kGridPadding * 8) / kYearCols + kGridPadding * 2 * col + 56 / 2;
			ImVec2 cellPos(cellX, y);

			bool isSelected = (year == *paramY);//g_calYear
			if (DrawYearCell(year, cellPos, isSelected)) {
// 				g_calYear = year;
				g_dispYear = year;
				*paramY = year;
				g_view = VIEW_MONTH;  // 跳转到月份选择
				g_yearPageStart = year - (year % 12);  // 调整页起始
				date_changed = true;
			}

			year++;
		}
		y += 48;
	}

	return date_changed;
}

bool DrawMonthView(ImDrawList* dl, ImVec2 winPos, int* paramY, int* paramM) {
	bool date_changed = false;
// 	float x = winPos.x + kGridPadding + kGridPadding + 14;
// 	float y = winPos.y + kGridPadding + 20;
	float x = winPos.x + 56 / 2 + 2 + 32;
	float y = winPos.y + kGridPadding + kGridPadding + kCellGap + 24 + 12 + 8; //kGridPadding + kCellSize + 

	for (int row = 0; row < 4; row++) {
		for (int col = 0; col < kMonthCols; col++) {
			int month = row * kMonthCols + col + 1;

			float cellX = x + col * (kCalendarWidth - kGridPadding * 2) / kMonthCols;
			ImVec2 cellPos(cellX, y);

			bool isSelected = (month == *paramM && g_dispYear == *paramY);//g_calMonth g_calYear
			if (DrawMonthCell(month, cellPos, isSelected)) {
				g_dispMonth = month;
				*paramM = month;
				g_view = VIEW_DATE;  // 返回日期选择
				date_changed = true;
			}
		}
		y += 48;
	}
	return date_changed;
}

constexpr float kCalendarHeight = 288.0f;

// 日历弹窗
bool DrawCalendarPopup(const char* id, int* year, int* month, int* day) {
	bool date_changed = false;
	if (g_showCalendar) {
		ImGui::SetNextWindowPos(g_calendarPos);
		ImGui::SetNextWindowSize(ImVec2(kCalendarWidth, 0));
		ImGui::OpenPopup(id);//"##CalendarPopup"
		g_showCalendar = false;
	}

	ImGui::SetNextWindowSize(ImVec2(kCalendarWidth, kCalendarHeight));

	// 每帧都 Push
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, IMVEC4("#37373D"));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

	if (ImGui::BeginPopup(id, ImGuiWindowFlags_NoTitleBar)) {//"##CalendarPopup"
		ImGui::SetWindowSize(ImVec2(kCalendarWidth, 288));

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 winPos = ImGui::GetWindowPos();
		float winW = kCalendarWidth;

		// ===== 第一行：按钮 + 标题 =====
		float headerY = winPos.y + kHeaderPaddingY;

		// 左侧两个按钮（0间隔）
		ImVec2 btn1Pos(winPos.x + kHeaderPaddingX, headerY);
		ImVec2 btn2Pos(btn1Pos.x + kBtnSize, headerY);

		// 右侧两个按钮
		ImVec2 btn3Pos(winPos.x + winW - kHeaderPaddingX - kBtnSize * 2, headerY);
		ImVec2 btn4Pos(btn3Pos.x + kBtnSize, headerY);

		if (g_view == VIEW_DATE) {
			// 日期视图：4个按钮
			if (DrawNavButton(u8"<<", btn1Pos)) {
				(*year)--;
// 				g_yearPageStart = *year - (*year % 12);
				g_dispYear = *year;
				date_changed = true;
			}
			if (DrawNavButton(u8"<", btn2Pos)) {
				(*month)--;
// 				if (g_dispMonth < 1) { g_dispMonth = 12; g_dispYear--; }
				if (*month < 1) { *month = 12; *year--; }
				g_dispMonth = *month;
				date_changed = true;
			}
			if (DrawNavButton(u8">", btn3Pos)) {
				(*month)++;
// 				if (g_dispMonth > 12) { g_dispMonth = 1; g_dispYear++; }
				if (*month > 12) { *month = 1; *year++; }
				g_dispMonth = *month;
				date_changed = true;
			}
			if (DrawNavButton(u8">>", btn4Pos)) {
// 				g_dispYear++;
				(*year)++;
				g_dispYear = *year;
				date_changed = true;
			}

			// 标题居中
// 			char titleBuf[64];
// 			snprintf(titleBuf, sizeof(titleBuf), u8"%d年 %d月", g_dispYear, g_dispMonth);
// 			ImVec2 titleSize = ImGui::CalcTextSize(titleBuf);
// 			float titleX = winPos.x + (winW - titleSize.x) * 0.5f;
// 			float titleY = headerY + (kBtnSize - titleSize.y) * 0.5f;

			char yearBuf[16];
// 			snprintf(yearBuf, sizeof(yearBuf), u8"%d年", g_dispYear);
			snprintf(yearBuf, sizeof(yearBuf), u8"%d%s", *year, CustomUI::unitYear.c_str());
			char monthBuf[16];
			snprintf(monthBuf, sizeof(monthBuf), u8"%d%s", *month, CustomUI::unitMonth.c_str());

			ImVec2 yearSize = ImGui::CalcTextSize(yearBuf);
			ImVec2 monthSize = ImGui::CalcTextSize(monthBuf);
			float spaceW = kHeaderPaddingX;// ImGui::CalcTextSize(" ").x;  // 年和月之间的空格
			// 计算总宽度，居中
			float totalW = yearSize.x + spaceW + monthSize.x;
			float yearX = winPos.x + (winW - totalW) * 0.5f;
			float monthX = yearX + yearSize.x + spaceW;
			float titleY = headerY + (kBtnSize - yearSize.y) * 0.5f;
			//  年区域：点击区域 = 文字区域 + padding
			float padX = 4.0f, padY = 3.0f;
			ImVec2 yearBtnMin(yearX - padX, titleY - padY);
			ImVec2 yearBtnMax(yearX + yearSize.x + padX, titleY + yearSize.y + padY);

			ImVec2 monthBtnMin(monthX - padX, titleY - padY);
			ImVec2 monthBtnMax(monthX + monthSize.x + padX, titleY + monthSize.y + padY);

			ImVec2 mouse = ImGui::GetIO().MousePos;
			bool yearHovered = (mouse.x >= yearBtnMin.x && mouse.x <= yearBtnMax.x &&
				mouse.y >= yearBtnMin.y && mouse.y <= yearBtnMax.y);
			bool monthHovered = (mouse.x >= monthBtnMin.x && mouse.x <= monthBtnMax.x &&
				mouse.y >= monthBtnMin.y && mouse.y <= monthBtnMax.y);

			//  绘制年区域背景
// 			if (yearHovered) {
// 				dl->AddRectFilled(yearBtnMin, yearBtnMax,
// 					IM_COL32(80, 80, 100, 255), 3.0f);  // 灰色悬停
// 			}
// 			//  绘制月区域背景
// 			if (monthHovered) {
// 				dl->AddRectFilled(monthBtnMin, monthBtnMax,
// 					IM_COL32(80, 80, 100, 255), 3.0f);  // 灰色悬停
// 			}

			ImU32 yearTextColor = yearHovered ? IM_COL32(100, 200, 200, 255)
				: IM_COL32(255, 255, 255, 255);
			ImU32 monthTextColor = monthHovered ? IM_COL32(100, 200, 200, 255)
				: IM_COL32(255, 255, 255, 255);

			//  绘制年文字
			ImGui::PushStyleColor(ImGuiCol_Text, yearTextColor);
			ImGui::SetCursorScreenPos(ImVec2(yearX, titleY));
			ImGui::SetCursorScreenPos(ImVec2(yearX, titleY));
			ImGui::TextUnformatted(yearBuf);
			ImGui::PopStyleColor();

			//  绘制月文字
			ImGui::SetCursorScreenPos(ImVec2(monthX, titleY));
			ImGui::PushStyleColor(ImGuiCol_Text, monthTextColor);
			ImGui::SetCursorScreenPos(ImVec2(monthX, titleY));
			ImGui::TextUnformatted(monthBuf);
			ImGui::PopStyleColor();

			//  点击年 → 跳转年份选择
			if (yearHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				g_view = VIEW_YEAR;
// 				g_yearPageStart = g_dispYear - (g_dispYear % 12);
				g_yearPageStart = *year - (*year % 12);
			}

			//  点击月 → 跳转月份选择
			if (monthHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				g_view = VIEW_MONTH;
			}

// 			dl->AddText(ImVec2(titleX, titleY), IM_COL32(255, 255, 255, 255), titleBuf);
// 
// 			// 点击标题跳转到年份选择
// 			ImVec2 mouse = ImGui::GetIO().MousePos;
// 			if (mouse.x >= titleX && mouse.x <= titleX + titleSize.x &&
// 				mouse.y >= titleY && mouse.y <= titleY + titleSize.y) {
// 				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
// 					g_view = VIEW_YEAR;
// 					g_yearPageStart = g_dispYear - (g_dispYear % 12);
// 				}
// 			}

		}
		else if (g_view == VIEW_YEAR) {
			// 年份视图：2个按钮
			if (DrawNavButton(u8"<<", btn1Pos)) {
				g_yearPageStart -= 12;
				date_changed = true;
			}
			if (DrawNavButton(u8">>", btn4Pos)) {
				g_yearPageStart += 12;
				date_changed = true;
			}

			// 标题
			char titleBuf[64];
			snprintf(titleBuf, sizeof(titleBuf), u8"%d-%d", g_yearPageStart, g_yearPageStart + 11);
			ImVec2 titleSize = ImGui::CalcTextSize(titleBuf);
			float titleX = winPos.x + (winW - titleSize.x) * 0.5f;
			float titleY = headerY + (kBtnSize - titleSize.y) * 0.5f;
			dl->AddText(ImVec2(titleX, titleY), IM_COL32(255, 255, 255, 255), titleBuf);

		}
		else if (g_view == VIEW_MONTH) {
			// 月份视图：2个按钮
			if (DrawNavButton(u8"<<", btn1Pos)) {
// 				g_dispYear--;
				(*year)--;
				date_changed = true;
			}
			if (DrawNavButton(u8">>", btn4Pos)) {
// 				g_dispYear++;
				(*year)++;
				date_changed = true;
			}

			// 标题
			char titleBuf[64];

// 			snprintf(titleBuf, sizeof(titleBuf), u8"%d年", g_dispYear);
			snprintf(titleBuf, sizeof(titleBuf), u8"%d%s", *year, CustomUI::unitYear.c_str());
			ImVec2 titleSize = ImGui::CalcTextSize(titleBuf);
			float titleX = winPos.x + (winW - titleSize.x) * 0.5f;
			float titleY = headerY + (kBtnSize - titleSize.y) * 0.5f;
			dl->AddText(ImVec2(titleX, titleY), IM_COL32(255, 255, 255, 255), titleBuf);
		}

		// ===== 分隔线 =====
		float sepY = headerY + kBtnSize + kHeaderPaddingY;//kSeparatorY
		dl->AddLine(ImVec2(winPos.x, sepY), ImVec2(winPos.x + winW, sepY),
			IM_COL32(100, 100, 120, 255), 1.0f);

		// ===== 内容区域 =====
		if (g_view == VIEW_DATE) {
			if (date_changed)
			{
				DrawDateView(dl, winPos, year, month, day);
			}
			else
			{
				date_changed = DrawDateView(dl, winPos, year, month, day);
			}
		}
		else if (g_view == VIEW_YEAR) {
			if (date_changed)
			{
				DrawYearView(dl, winPos, year);
			}
			else
			{
				date_changed = DrawYearView(dl, winPos, year);
			}
		}
		else if (g_view == VIEW_MONTH) {
			if (date_changed)
			{
				DrawMonthView(dl, winPos, year, month);
			}
			else
			{
				date_changed = DrawMonthView(dl, winPos, year, month);
			}
		}

		ImGui::EndPopup();
	}

	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(2);

	return date_changed;
}

constexpr float kDateInputWidth = 160.0f;
constexpr float kInputGap = 4.0f;  // 输入框和图标之间的间距

void DrawDateInput(const char* label, std::string* text, int* year, int* month, int* day, int fieldIndex, ImTextureID icon) {
	float width = ImGui::CalcItemWidth();//  160.0f;
	float height = ImGui::GetFrameHeight();
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();
	
	ImVec2 cursor_pos = ImGui::GetCursorScreenPos();

	// ===== 左侧：文本输入框 =====
	float inputW = kDateInputWidth;
	ImGui::SetNextItemWidth(width);

	//  用 InputText 替代 InvisibleButton，允许用户手动输入
	char buf[64];
	strncpy_s(buf, sizeof(buf), text->c_str(), _TRUNCATE);

	if (ImGui::InputText(label, buf, sizeof(buf), ImGuiInputTextFlags_AutoSelectAll)) {
		*text = buf;
		// 尝试解析输入的日期
		int y, m, d;
		if (sscanf(buf, "%d/%d/%d", &y, &m, &d) == 3) {
			*year = y; *month = m; *day = d;
		}
		else if (sscanf(buf, "%d-%d-%d", &y, &m, &d) == 3) {
			*year = y; *month = m; *day = d;
		}
	}

	// 背景和边框
// 	dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height),
// 		IM_COL32(30, 30, 35, 255), 4.0f);
// 
	ImVec2 mouse = ImGui::GetIO().MousePos;
	bool isHovered = (mouse.x >= pos.x && mouse.x <= pos.x + width &&
		mouse.y >= pos.y && mouse.y <= pos.y + height);

	ImU32 borderColor = isHovered ? IM_COL32(100, 200, 200, 255) : ImGui::GetColorU32(ImGui::GetStyle().Colors[ImGuiCol_Border]);//IM_COL32(80, 80, 100, 255)
	dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
		borderColor);//, 4.0f, 0, isHovered ? 1.5f : 1.0f
// 
// 	// 日期文本
// 	float textY = pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f;
// 	if (*day > 0 && *month > 0 && *year > 0) {
// 		char buf[32];
// 		snprintf(buf, sizeof(buf), "%d/%02d/%02d", *year, *month, *day);
// 		dl->AddText(ImVec2(pos.x + 8, textY), IM_COL32(200, 200, 220, 255), buf);
// 	}
// 	else {
// 		dl->AddText(ImVec2(pos.x + 8, textY), IM_COL32(120, 120, 140, 255), u8"请选择日期");
// 	}

	 // ===== 右侧：日历图标按钮 =====
// 	ImGui::SameLine(0, kInputGap);
	ImGui::SameLine();

	ImVec2 mouse_pos = ImGui::GetIO().MousePos;
	// 绘制日历图标（右侧）
	
	float icon_size = 24.0f;
	float icon_padding = 4.0f;
	ImVec2 icon_pos(cursor_pos.x + width - icon_size - icon_padding,
		cursor_pos.y + (height - icon_size) * 0.5f);
	//
	float icon_x = pos.x + width - icon_size - 6;
	float icon_y = pos.y + (height - icon_size) * 0.5f;
	static int i = 0;
	float xx = 32;
	float y = 32;
	ImVec2 btnSize(32, 32);
	ImVec2 iconSize2(24, 24);

	ImGui::PushID(i);
	float padding = 1.0f;
	if (icon != 0)
	{
		dl->AddImage(
			icon,
			ImVec2(icon_pos.x + padding, icon_pos.y + padding),
			ImVec2(icon_pos.x + iconSize2.x - padding, icon_pos.y + iconSize2.y - padding),
			ImVec2(0, 0),
			ImVec2(1, 1),
			IM_COL32(255, 255, 255, 255)
		);
	}
	ImGui::PopID();
	i++;
	// 隐形按钮用于点击检测
	ImGui::SetCursorScreenPos(pos);
	ImGui::InvisibleButton(("##icon_" + std::string(label)).c_str(), ImVec2(width, height));
	// 检测日历图标区域点击
	bool icon_hovered = (mouse_pos.x >= icon_pos.x && mouse_pos.x <= icon_pos.x + icon_size &&
		mouse_pos.y >= icon_pos.y && mouse_pos.y <= icon_pos.y + icon_size);

	if (ImGui::IsMouseReleased(ImGuiMouseButton_Left) && icon_hovered) {//ImGui::IsItemClicked()
		g_currentField = fieldIndex;
		g_calendarPos = ImVec2(pos.x, pos.y + height + 4);

		// 如果输入框有日期，显示输入框的日期；否则显示当天
		if (!text->empty()) {
			int y, m, d;
			if (sscanf(text->c_str(), "%d/%d/%d", &y, &m, &d) == 3) {
				g_dispYear = y;
				g_dispMonth = m;
				*year = y;
				*month = m;
				*day = d;
			}
		}
		else if (*year > 0 && *month > 0 && *day > 0)
		{
		}
		else
		{
			// 跳转到当天
			time_t now = time(nullptr);
			struct tm* t = localtime(&now);
			g_dispYear = t->tm_year + 1900;
			g_dispMonth = t->tm_mon + 1;
			*year = t->tm_year + 1900;
			*month = t->tm_mon + 1;
			*day = t->tm_mday;
		}

		g_dispYear = *year > 0 ? *year : 2025;
		g_dispMonth = *month > 0 ? *month : 10;
		g_view = VIEW_DATE;
		g_showCalendar = true;
	}
}

void CustomUI::CalenderIconInputText(const char* label, ImTextureID icon, std::string* text, int* year, int* month, int* day) {
// 	static int startY = 0, startM = 0, startD = 0;
// 	static int endY = 0, endM = 0, endD = 0;

// 	ImGui::Text(u8"起始日期:");
// 	ImGui::SameLine(80);
// 	DrawDateInput("##start", &startY, &startM, &startD, 0);
// 
// 	ImGui::Text(u8"结束日期:");
// 	ImGui::SameLine(80);
// 	DrawDateInput("##end", &endY, &endM, &endD, 1);
	if (nullptr == text)
	{
		std::string strDate = "2020/02/02";
		DrawDateInput(label, &strDate, year, month, day, 1, icon);
	}
	else
	{
		DrawDateInput(label, text, year, month, day, 1, icon);
	}

// 	int year, month, day;
// 	DrawCalendarPopup("", &year, &month, &day);
}

bool CustomUI::CalenderIconInputText(const char* label, ImTextureID icon, int& year, int& month, int& day, const char* popup_id,bool& show_popup)
{
	char date_buf[32];
	snprintf(date_buf, sizeof(date_buf), "%d/%02d/%02d", year, month, day);
	ImVec2 pos = ImGui::GetCursorScreenPos();

	// 计算文本各部分的位置
	ImVec2 full_size = ImGui::CalcTextSize(date_buf);

			// 绘制日期文本
	ImVec2 cursor_pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	ImGuiStyle& style = ImGui::GetStyle();
	// 背景
	ImU32 bg_color = ImGui::GetColorU32(style.Colors[ImGuiCol_WindowBg]);
	ImU32 border_color = ImGui::GetColorU32(style.Colors[ImGuiCol_Border]);
	float height = ImGui::GetFrameHeight();
	float width = ImGui::CalcItemWidth();// 120.0f;
	float icon_size = 24.0f;
	float icon_padding = 4.0f;

// 	dl->AddRectFilled(cursor_pos, ImVec2(cursor_pos.x + width, cursor_pos.y + height), bg_color);
// 	dl->AddRect(cursor_pos, ImVec2(cursor_pos.x + width, cursor_pos.y + height), border_color);

	// 计算各部分文本位置
	char year_buf[8] = {""}, month_buf[8] = { "" }, day_buf[8] = { "" };
	if (year > 0)
	{
		snprintf(year_buf, sizeof(year_buf), "%d", year);
	}
	if (month > 0)
	{
		snprintf(month_buf, sizeof(month_buf), "%02d", month);
	}
	if (day > 0)
	{
		snprintf(day_buf, sizeof(day_buf), "%02d", day);
	}

	ImVec2 year_size = ImGui::CalcTextSize(year_buf);
	ImVec2 month_size = ImGui::CalcTextSize(month_buf);
	ImVec2 day_size = ImGui::CalcTextSize(day_buf);
	ImVec2 slash_size = ImGui::CalcTextSize("/");

	float text_y = pos.y + (height - ImGui::GetTextLineHeight()) * 0.5f;

	float x = pos.x + 8;
	// 各区域的 X 范围
	float year_start_x = x;
	float year_end_x = x + year_size.x;
	float slash1_end = year_end_x + slash_size.x;
	float month_start_x = slash1_end;
	float month_end_x = month_start_x + month_size.x;
	float slash2_end = month_end_x + slash_size.x;
	float day_start_x = slash2_end;
	float day_end_x = day_start_x + day_size.x;

	// 检测鼠标位置
	ImVec2 mouse = ImGui::GetIO().MousePos;

	bool isHovered = (mouse.x >= pos.x && mouse.x <= pos.x + width &&
		mouse.y >= pos.y && mouse.y <= pos.y + height);

	// 背景色
	ImU32 bgColor = IM_COL32(30, 30, 35, 255);
	dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg_color);

	// 边框颜色（hover 时显示青绿色）
	ImU32 borderColor = isHovered ?
		IM_COL32(100, 200, 200, 255) :  // 青绿色 #64C8C8
		border_color;     // 默认灰色 #505064
	float borderWidth = isHovered ? 1.5f : 1.0f;  // hover 时边框稍粗

	dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height),
		borderColor);//, 4.0f, 0, borderWidth

	//  判断是否已选择日期
	bool hasDate = (year > 0 && month > 0 && day > 0);

	if (!hasDate) {
		// 未选择日期：显示灰色提示文字
		TCHAR szTemp[128];
// 		const char* hint = strHint.c_str();
		ImVec2 hint_size = ImGui::CalcTextSize(selecdataTips.c_str());
		// 居中显示
		float hint_x = pos.x + (width - 30 - hint_size.x) * 0.5f;  // 30 是图标区域宽度
		dl->AddText(ImVec2(pos.x + 8, text_y), IM_COL32(120, 120, 140, 255), selecdataTips.c_str());
	}
	else {
		bool in_year = (mouse.x >= year_start_x && mouse.x <= year_end_x &&
			mouse.y >= pos.y && mouse.y <= pos.y + height);
		bool in_month = (mouse.x >= month_start_x && mouse.x <= month_end_x &&
			mouse.y >= pos.y && mouse.y <= pos.y + height);
		bool in_day = (mouse.x >= day_start_x && mouse.x <= day_end_x &&
			mouse.y >= pos.y && mouse.y <= pos.y + height);

		// 绘制年份
		ImU32 year_color = true ? IM_COL32(100, 180, 255, 255) :
			(in_year ? IM_COL32(150, 200, 255, 255) : IM_COL32(200, 200, 220, 255));
		dl->AddText(ImVec2(year_start_x, text_y), year_color, year_buf);

		// 绘制斜杠
		dl->AddText(ImVec2(year_end_x, text_y), IM_COL32(120, 120, 140, 255), "/");

		// 绘制月份
		ImU32 month_color = true ? IM_COL32(100, 180, 255, 255) :
			(in_month ? IM_COL32(150, 200, 255, 255) : IM_COL32(200, 200, 220, 255));
		dl->AddText(ImVec2(month_start_x, text_y), month_color, month_buf);

		// 绘制斜杠
		dl->AddText(ImVec2(month_end_x, text_y), IM_COL32(120, 120, 140, 255), "/");

		// 绘制日
		ImU32 day_color = true ? IM_COL32(100, 180, 255, 255) :
			(in_day ? IM_COL32(150, 200, 255, 255) : IM_COL32(200, 200, 220, 255));
		dl->AddText(ImVec2(day_start_x, text_y), day_color, day_buf);
	}

	// 		// 检测年份区域悬停
	ImVec2 mouse_pos = ImGui::GetIO().MousePos;
			// 绘制日历图标（右侧）
	ImVec2 icon_pos(cursor_pos.x + width - icon_size - icon_padding,
		cursor_pos.y + (height - icon_size) * 0.5f);

			//  日历图标（使用图片）
	// 		float icon_size = 20.0f;
	float icon_x = pos.x + width - icon_size - 6;
	float icon_y = pos.y + (height - icon_size) * 0.5f;
	static int i = 0;
	float xx = 32;
	float y = 32;
	ImVec2 btnSize(32, 32);
	ImVec2 iconSize2(24, 24);
	
	ImGui::PushID(i);
	float padding = 1.0f;
	if (icon != 0)
	{
		dl->AddImage(
			icon,
			ImVec2(icon_pos.x + padding, icon_pos.y + padding),
			ImVec2(icon_pos.x + iconSize2.x - padding, icon_pos.y + iconSize2.y - padding),
			ImVec2(0, 0),
			ImVec2(1, 1),
			IM_COL32(255, 255, 255, 255)
		);
	}
	ImGui::PopID();
	i++;
	// 隐形按钮用于点击检测
	ImGui::InvisibleButton(label, ImVec2(width, height));

	// 检测日历图标区域点击
	bool icon_hovered = (mouse_pos.x >= icon_pos.x && mouse_pos.x <= icon_pos.x + icon_size &&
		mouse_pos.y >= icon_pos.y && mouse_pos.y <= icon_pos.y + icon_size);
	if (ImGui::IsItemClicked() && icon_hovered) {
		show_popup = true;
		g_showCalendar = true;

		ImGui::SetNextWindowPos(ImVec2(pos.x, pos.y + height + 4));
// 		ImGui::SetNextWindowPos(ImVec2(544, 188 + 4));
		ImGui::SetNextWindowSize(ImVec2(kCalendarWidth, 0));

		pos = ImGui::GetCursorScreenPos();

		// 关键：内边距 16px
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kPadding, 4));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.22f, 0.22f, 0.27f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));

		ImGui::OpenPopup(popup_id);
	
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
	}


	return show_popup;
}

// 绘制放大镜图标
void DrawIconSearch(ImDrawList* dl, ImVec2 pos, float size, ImU32 color) {
	ImVec2 center(pos.x + size * 0.5f, pos.y + size * 0.5f);
	float radius = size * 0.4f;
	float thickness = size * 0.12f;

	// 圆形镜片
	dl->AddCircle(
		ImVec2(center.x - size * 0.05f, center.y - size * 0.05f),
		radius,
		color,
		0,
		thickness
	);

	// 手柄（斜线）
	dl->AddLine(
		ImVec2(center.x + size * 0.15f, center.y + size * 0.15f),
		ImVec2(center.x + size * 0.4f, center.y + size * 0.4f),
		color,
		thickness
	);
}

#include "misc/cpp/imgui_stdlib.h"

void CustomUI::SearchIconInputText(const char* id, std::string* text, float width)
{
	ImVec2 pos = ImGui::GetCursorScreenPos();
	ImDrawList* dl = ImGui::GetWindowDrawList();

	float icon_size = 20.0f;
	float icon_padding = 8.0f;
	float input_offset = icon_size + icon_padding * 2;

	ImGuiStyle& style = ImGui::GetStyle();
	// 绘制背景
	ImU32 bg_color = ImGui::GetColorU32(style.Colors[ImGuiCol_WindowBg]); 
	ImU32 border_color = ImGui::GetColorU32(style.Colors[ImGuiCol_Border]);
	ImU32 icon_color = ImGui::GetColorU32(style.Colors[ImGuiCol_Text]);

	// 输入框高度
	float height = ImGui::GetFrameHeight();

	// 绘制背景和边框
	dl->AddRectFilled(pos, ImVec2(pos.x + width, pos.y + height), bg_color);//, 4.0f
	dl->AddRect(pos, ImVec2(pos.x + width, pos.y + height), border_color);//, 4.0f, 0, 1.5f

	// 绘制放大镜图标
	ImVec2 icon_pos(pos.x + icon_padding, pos.y + (height - icon_size) * 0.5f);
	DrawIconSearch(dl, icon_pos, icon_size, icon_color);

	// 绘制输入框（偏移以避开图标）
	ImGui::SetCursorScreenPos(ImVec2(pos.x + input_offset, pos.y));
	ImGui::SetNextItemWidth(width - input_offset - 4);

	ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));

	ImGui::InputTextWithHint(id, searchTips.c_str(), text);//u8"请输入"

	ImGui::PopStyleColor(4);
}



bool CustomUI::ImageButtonNP(const char* id, ImTextureID icon, const ImVec2& size)
{
	// 1. 消除內距與邊框，讓按鈕大小與圖片 1:1 吻合
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

	// 2. 取得當前主題的 Hovered 與 Active 顏色，僅修改其 Alpha 透明度
	ImVec4 colHovered = ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered);
	ImVec4 colActive = ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive);
	colHovered.w = 0.25f; // Hover 保留 25% 不透明度（可依需求調整，例如 0.15f ~ 0.35f）
	colActive.w = 0.45f; // Active 保留 45% 不透明度

	// 3. 預設完全無背景，Hover / Active 顯示半透明主題色
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, colHovered);
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, colActive);

	bool clicked = ImGui::ImageButton(id, icon, size);

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);

	return clicked;
}

bool CustomUI::Combo(const char* label, int* current_item, const std::vector<std::string>& vStr,
	int popup_max_height_in_items, ImTextureID arrow_icon, const ImVec2& icon_size)
{
	std::vector<const char*> items;
	items.reserve(vStr.size());
	for (const auto& s : vStr)
	{
		items.push_back(s.c_str());
	}

	return Combo( label, current_item, items.data(), static_cast<int>(items.size()),
		popup_max_height_in_items, arrow_icon, icon_size );
}

bool CustomUI::Combo( const char* label, std::string& sSel, const std::vector<std::string>& vStr,
	int popup_max_height_in_items, ImTextureID arrow_icon, const ImVec2& icon_size)
{
	int current_item = -1;
	std::vector<const char*> items;
	items.reserve(vStr.size());

	// 1. 收集指標並比對 sSel 是否存在於 vStr 中
	for (size_t i = 0; i < vStr.size(); ++i)
	{
		items.push_back(vStr[i].c_str());
		if (current_item == -1 && !sSel.empty() && vStr[i] == sSel)
		{
			current_item = static_cast<int>(i);
		}
	}

	// 若傳入的 sSel 不在 vStr 中，確保未選中時 sSel 為空
	if (current_item == -1 && !sSel.empty())
	{
		sSel.clear();
	}

	// 2. 呼叫底層 C 陣列版本進行渲染與互動
	bool value_changed = Combo(
		label,
		&current_item,
		items.data(),
		static_cast<int>(items.size()),
		popup_max_height_in_items,
		arrow_icon,
		icon_size
	);

	// 3. 若用戶點選了新項目，將選中字串寫回 sSel
	if (value_changed && current_item >= 0 && current_item < static_cast<int>(vStr.size()))
	{
		sSel = vStr[current_item];
	}

	return value_changed;
}

CustomUI::LastItemInfo CustomUI::s_LastItem = {};
bool CustomUI::Combo2( const char* label, std::string& sSel, const std::vector<std::string>& vStr,
	int popup_max_height_in_items, ImTextureID arrow_icon, const ImVec2& icon_size)
{
	float item_w = ImGui::CalcItemWidth();

	ImGui::PushID(label);
	ImGui::BeginGroup();
	ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0.0f;

	// 1. 记录第一行文本的起始 Y 坐标
	float first_line_y = ImGui::GetCursorScreenPos().y;
	float label_right_x = ImGui::GetCursorScreenPos().x;

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::TextUnformatted(label, label_end);
		label_right_x = ImGui::GetItemRectMax().x; // 记录左侧标题结束位置，防止右侧文字过长时重叠
	}
	else
	{
		// 即使没有左侧标题，也保留一行高度占位，保证两行结构对齐
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
	}

	// 2. 第二行：Combo
	ImGui::SetNextItemWidth(item_w);
	bool value_changed = Combo("##combo", sSel, vStr, popup_max_height_in_items, arrow_icon, icon_size);

	// 捕获第二行 Combo 本体的矩形边界
	ImVec2 ctrl_min = ImGui::GetItemRectMin();
	ImVec2 ctrl_max = ImGui::GetItemRectMax();

	ImGui::EndGroup();
	ImGui::PopID();

	ImVec2 group_min = ImGui::GetItemRectMin();
	ImVec2 group_max = ImGui::GetItemRectMax();

	s_LastItem.min_x = group_min.x;
	s_LastItem.max_x = group_max.x;
	s_LastItem.text_y = first_line_y;
	s_LastItem.label_end_x = label_right_x;

	s_LastItem.ctrl_min = ctrl_min;
	s_LastItem.ctrl_max = ctrl_max;
	// Combo 右侧有 4px 右边距 + icon_size.x 的图标，因此让出这段宽度
	s_LastItem.right_reserve_w = icon_size.x + 4.0f;
	s_LastItem.valid = true;

	return value_changed;
}

void CustomUI::AppendText(const std::string& text, ImU32 color)
{
	if (!s_LastItem.valid || text.empty())
		return;

	ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size());

	// 计算右对齐起始 X：紧贴 Combo2 的右边缘 (max_x)
	float pos_x = s_LastItem.max_x - text_size.x;
	float pos_y = s_LastItem.text_y;

	// 防重叠保护：如果右侧文字太长，限制其最左不超过左侧 label 右侧 + 6px，并自动裁剪
	float min_allowed_x = s_LastItem.label_end_x + 6.0f;
	if (pos_x < min_allowed_x)
		pos_x = min_allowed_x;

	ImU32 draw_col = (color != 0) ? color : ImGui::GetColorU32(ImGuiCol_Text); // 默认使用全局文本色
	ImVec4 clip_rect(min_allowed_x, pos_y, s_LastItem.max_x, pos_y + text_size.y + 2.0f);

	ImGui::GetWindowDrawList()->AddText(
		ImGui::GetFont(),
		ImGui::GetFontSize(),
		ImVec2(pos_x, pos_y),
		draw_col,
		text.c_str(),
		text.c_str() + text.size(),
		0.0f,
		&clip_rect
	);
}

void CustomUI::AppendText2(const std::string& text, ImU32 color)
{
	if (!s_LastItem.valid || text.empty())
		return;

	const float margin = 4.0f;
	ImVec2 text_size = ImGui::CalcTextSize(text.c_str(), text.c_str() + text.size());

	// 计算单位文本的右边缘：控件右边界 - 让出的按钮宽度 - 4px 间距
	float right_edge = s_LastItem.ctrl_max.x - s_LastItem.right_reserve_w - margin;
	float pos_x = right_edge - text_size.x;
	// 垂直居中对齐第二行控件中心
	float pos_y = std::floor((s_LastItem.ctrl_min.y + s_LastItem.ctrl_max.y - text_size.y) * 0.5f);

	// 防止过长越出控件左边界，设置裁剪矩形
	float min_allowed_x = s_LastItem.ctrl_min.x + margin;
	if (pos_x < min_allowed_x)
		pos_x = min_allowed_x;

	ImU32 draw_col = (color != 0) ? color : ImGui::GetColorU32(ImGuiCol_Text);
	ImVec4 clip_rect(min_allowed_x, s_LastItem.ctrl_min.y, right_edge, s_LastItem.ctrl_max.y);

	ImGui::GetWindowDrawList()->AddText(
		ImGui::GetFont(),
		ImGui::GetFontSize(),
		ImVec2(pos_x, pos_y),
		draw_col,
		text.c_str(),
		text.c_str() + text.size(),
		0.0f,
		&clip_rect
	);
}

bool CustomUI::Combo(
	const char* label,
	int* current_item,
	const char* const items[],
	int items_count,
	int popup_max_height_in_items,
	ImTextureID arrow_icon,
	const ImVec2& icon_size)
{
	ImGui::PushID(label);

	const float margin = 4.0f;
	const float target_h = icon_size.y + margin * 2.0f;
	const float font_h = ImGui::GetFontSize();
	const float pad_y = std::floor((target_h > font_h) ? (target_h - font_h) * 0.5f : margin);

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(margin, pad_y));

	if (popup_max_height_in_items != -1)
	{
		float max_h = ImGui::GetTextLineHeightWithSpacing() * popup_max_height_in_items + margin * 2.0f;
		ImGui::SetNextWindowSizeConstraints(ImVec2(0.0f, 0.0f), ImVec2(FLT_MAX, max_h));
	}

	const char* preview_value = (*current_item >= 0 && *current_item < items_count) ? items[*current_item] : "";
	bool value_changed = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(margin, margin));
	bool is_open = ImGui::BeginCombo("##combo", "", ImGuiComboFlags_NoArrowButton);
	if (is_open)
	{
		value_changed = RenderComboItems(items, items_count, current_item);
		ImGui::EndCombo();
	}
	ImGui::PopStyleVar(); // WindowPadding

	ImVec2 rect_min = ImGui::GetItemRectMin();
	ImVec2 rect_max = ImGui::GetItemRectMax();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	// 垂直居中绘制预览文字
	if (preview_value && preview_value[0] != '\0')
	{
		ImVec2 text_size = ImGui::CalcTextSize(preview_value);
		float text_y = std::floor((rect_min.y + rect_max.y - text_size.y) * 0.5f);
		float clip_max_x = rect_max.x - margin - icon_size.x - margin;
		ImVec4 clip_rect(rect_min.x + margin, rect_min.y, clip_max_x, rect_max.y);

		draw_list->AddText(ImGui::GetFont(), font_h, ImVec2(rect_min.x + margin, text_y),
			ImGui::GetColorU32(ImGuiCol_Text), preview_value, nullptr, 0.0f, &clip_rect);
	}

	// 复用辅助函数绘制右侧箭头
	DrawComboArrow(draw_list, rect_min, rect_max, arrow_icon, icon_size,
		is_open, ImGui::IsItemHovered(), ImGui::IsItemActive());

	ImGui::PopStyleVar(3);
	ImGui::PopID();
	return value_changed;
}


// 供 ImGui::InputText 動態調整 std::string 容量的內部回呼函式
static int StdStringResizeCallback(ImGuiInputTextCallbackData* data)
{
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
	{
		std::string* str = static_cast<std::string*>(data->UserData);
		str->resize(data->BufTextLen);
		data->Buf = const_cast<char*>(str->c_str());
	}
	return 0;
}

bool CustomUI::EditableCombo(
	const char* label,
	std::string& sOut,
	const std::vector<std::string>& vStr,
	int popup_max_height_in_items,
	ImTextureID arrow_icon,
	const ImVec2& icon_size)
{
	ImGui::PushID(label);
	ImGui::BeginGroup();

	const float margin = 4.0f;
	const float target_h = icon_size.y + margin * 2.0f;
	const float font_h = ImGui::GetFontSize();
	const float pad_y = std::floor((target_h > font_h) ? (target_h - font_h) * 0.5f : margin);

	float total_w = ImGui::CalcItemWidth();
	float btn_w = icon_size.x + margin * 2.0f;
	float input_w = (total_w > btn_w) ? (total_w - btn_w) : 1.0f;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(margin, pad_y));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, ImGui::GetStyle().ItemSpacing.y));

	bool value_changed = false;
	const char* popup_id = "##EditableComboPopup";
	bool is_open = ImGui::IsPopupOpen(popup_id);

	if (sOut.capacity() == 0)
		sOut.reserve(32);

	ImGui::SetNextItemWidth(input_w);
	if (ImGui::InputText("##input", const_cast<char*>(sOut.c_str()), sOut.capacity() + 1,
		ImGuiInputTextFlags_CallbackResize, StdStringResizeCallback, &sOut))
	{
		value_changed = true;
	}

	ImVec2 input_min = ImGui::GetItemRectMin();
	ImVec2 input_max = ImGui::GetItemRectMax();

	ImGui::SameLine(0.0f, 0.0f);
	if (ImGui::InvisibleButton("##arrow_btn", ImVec2(btn_w, input_max.y - input_min.y)))
	{
		if (is_open) ImGui::CloseCurrentPopup();
		else         ImGui::OpenPopup(popup_id);
	}
	bool arrow_hovered = ImGui::IsItemHovered();
	bool arrow_active = ImGui::IsItemActive();

	ImVec2 full_min = input_min;
	ImVec2 full_max = ImGui::GetItemRectMax();
	ImDrawList* draw_list = ImGui::GetWindowDrawList();

	draw_list->AddRectFilled(ImGui::GetItemRectMin(), full_max,
		ImGui::GetColorU32(arrow_hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg));
	draw_list->AddRect(full_min, full_max, ImGui::GetColorU32(ImGuiCol_Border));

	// 复用辅助函数绘制右侧箭头
	DrawComboArrow(draw_list, full_min, full_max, arrow_icon, icon_size,
		is_open, arrow_hovered, arrow_active);

	// 准备指针数组以复用 RenderComboItems
	std::vector<const char*> items;
	items.reserve(vStr.size());
	for (const auto& s : vStr) items.push_back(s.c_str());

	ImGui::SetNextWindowPos(ImVec2(full_min.x, full_max.y));
	if (popup_max_height_in_items != -1)
	{
		float max_h = ImGui::GetTextLineHeightWithSpacing() * popup_max_height_in_items + margin * 2.0f;
		ImGui::SetNextWindowSizeConstraints(ImVec2(total_w, 0.0f), ImVec2(total_w, max_h));
	}
	else
	{
		ImGui::SetNextWindowSize(ImVec2(total_w, 0.0f));
	}

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(margin, margin));
	if (ImGui::BeginPopup(popup_id))
	{
		int selected_idx = -1;
		if (RenderComboItems(items.data(), static_cast<int>(items.size()), &selected_idx, sOut.c_str()))
		{
			if (selected_idx >= 0 && selected_idx < static_cast<int>(vStr.size()))
				sOut = vStr[selected_idx];
			value_changed = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}
	ImGui::PopStyleVar(); // WindowPadding

	ImGui::EndGroup();
	ImGui::PopStyleVar(4);
	ImGui::PopID();
	return value_changed;
}


struct InputText2_UserData
{
	std::string* Str;
	ImGuiInputTextCallback  ChainCallback;
	void* ChainCallbackUserData;
};

static int InputText2_Callback(ImGuiInputTextCallbackData* data)
{
	InputText2_UserData* user_data = static_cast<InputText2_UserData*>(data->UserData);
	if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
	{
		// 動態調整 std::string 大小並更新緩衝區指標
		std::string* str = user_data->Str;
		IM_ASSERT(data->Buf == str->c_str());
		str->resize(data->BufTextLen);
		data->Buf = const_cast<char*>(str->c_str());
	}
	else if (user_data->ChainCallback)
	{
		// 轉發給外部傳入的自訂回呼函式
		data->UserData = user_data->ChainCallbackUserData;
		return user_data->ChainCallback(data);
	}
	return 0;
}

bool CustomUI::InputText2( const char* label, std::string* str,
	ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data)
{
	IM_ASSERT(str != nullptr);

	// 1. 捕獲外部通過 ImGui::SetNextItemWidth() 設定的寬度
	float item_w = ImGui::CalcItemWidth();

	ImGui::PushID(label);
	ImGui::BeginGroup();
	ImGui::GetCurrentWindow()->DC.CurrLineTextBaseOffset = 0.0f;

	// 2. 第一行：顯示 label 中 "##" 左邊的可見文本，並記錄座標給 AppendText 使用
	float first_line_y = ImGui::GetCursorScreenPos().y;
	float label_right_x = ImGui::GetCursorScreenPos().x;

	const char* label_end = ImGui::FindRenderedTextEnd(label);
	if (label != label_end)
	{
		ImGui::TextUnformatted(label, label_end);
		label_right_x = ImGui::GetItemRectMax().x;
	}
	else
	{
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetTextLineHeight()));
	}

	// 3. 第二行：套用寬度並呼叫支援動態擴容的 InputText
	ImGui::SetNextItemWidth(item_w);

	flags |= ImGuiInputTextFlags_CallbackResize;
	InputText2_UserData cb_user_data;
	cb_user_data.Str = str;
	cb_user_data.ChainCallback = callback;
	cb_user_data.ChainCallbackUserData = user_data;

	ImGui::InputText(
		"##input",
		const_cast<char*>(str->c_str()),
		str->capacity() + 1,
		flags,
		InputText2_Callback,
		&cb_user_data
	);

	// 捕獲編輯完成事件（包含按下 Enter 確認，或修改內容後點擊外部失去焦點）
	bool edit_finished = ImGui::IsItemDeactivatedAfterEdit();

	// 捕获第二行 InputText 本体的矩形边界
	ImVec2 ctrl_min = ImGui::GetItemRectMin();
	ImVec2 ctrl_max = ImGui::GetItemRectMax();

	ImGui::EndGroup();
	ImGui::PopID();

	ImVec2 group_min = ImGui::GetItemRectMin();
	ImVec2 group_max = ImGui::GetItemRectMax();

	s_LastItem.min_x = group_min.x;
	s_LastItem.max_x = group_max.x;
	s_LastItem.text_y = first_line_y;
	s_LastItem.label_end_x = label_right_x;

	s_LastItem.ctrl_min = ctrl_min;
	s_LastItem.ctrl_max = ctrl_max;
	s_LastItem.right_reserve_w = 0.0f; // InputText 没有右侧下拉按钮，无需额外让位
	s_LastItem.valid = true;

	return edit_finished;
}

#endif
