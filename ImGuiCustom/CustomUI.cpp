#include "CustomUI.h"
#if IMGUI
 
CustomUI::FnFillCustom CustomUI::s_pFnFillCustom = nullptr;

#include <ctime>

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

// 获取某月1号是星期几 (0=周日, 1=周一, ..., 6=周六)
int GetFirstDayOfWeek(int year, int month) {
	if (month < 3) {
		month += 12;
		year--;
	}
	int k = year % 100;
	int j = year / 100;
	int h = (1 + (13 * (month + 1)) / 5 + k + k / 4 + j / 4 - 2 * j) % 7;
	return (h + 6) % 7;  // 转换为周日=0
}

// 获取某月的天数
int GetDaysInMonth(int year, int month) {
	static const int days[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (month == 2 && ((year % 4 == 0 && year % 100 != 0) || year % 400 == 0))
		return 29;
	return days[month - 1];
}

// 日历弹窗
bool DrawCalendarPopup(const char* id, int* year, int* month, int* day) {
	bool date_changed = false;
	float popup_width = 300.0f;
	float cell_size = 38.0f;

	ImGui::SetNextWindowSize(ImVec2(popup_width, 0));

	if (ImGui::BeginPopup(id)) {
		// 顶部：年月切换
		float avail_w = ImGui::GetContentRegionAvail().x;

		if (ImGui::Button(u8"<<", ImVec2(35, 25)))
		{
			(*year)--;
			date_changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(u8"<", ImVec2(30, 25))) {
			(*month)--;
			if (*month < 1) { *month = 12; (*year)--; }
			date_changed = true;
		}
		ImGui::SameLine();

		char title[64];
		snprintf(title, sizeof(title), u8"%d年 %02d月", *year, *month);
		ImVec2 title_size = ImGui::CalcTextSize(title);
		float title_x = (avail_w - title_size.x) * 0.5f;
		ImGui::SetCursorPosX(title_x);
		ImGui::Text("%s", title);

		ImGui::SameLine(avail_w - 65);
		if (ImGui::Button(u8">", ImVec2(30, 25))) {
			(*month)++;
			if (*month > 12) { *month = 1; (*year)++; }
			date_changed = true;
		}
		ImGui::SameLine();
		if (ImGui::Button(u8">>", ImVec2(35, 25)))
		{
			(*year)++;
			date_changed = true;
		}

		ImGui::Separator();
		ImGui::Spacing();

		// 星期标题
		float start_x = ImGui::GetCursorPosX();
		const char* weekdays[] = { u8"日", u8"一", u8"二", u8"三", u8"四", u8"五", u8"六" };

		for (int i = 0; i < 7; i++) {
			ImGui::SameLine();
			ImVec2 text_size = ImGui::CalcTextSize(weekdays[i]);
			float offset = (cell_size - text_size.x) * 0.5f;
			ImGui::SetCursorPosX(start_x + i * cell_size + offset);

			if (i == 0 || i == 6) {
				ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", weekdays[i]);
			}
			else {
				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.8f, 1.0f), "%s", weekdays[i]);
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// 日期网格
		int days_in_month = GetDaysInMonth(*year, *month);
		int first_day = GetFirstDayOfWeek(*year, *month);

		int day_num = 1;
		int total_cells = first_day + days_in_month;
		int total_rows = (total_cells + 6) / 7;

		for (int row = 0; row < total_rows; row++) {
			for (int col = 0; col < 7; col++) {
				int cell_index = row * 7 + col;

				if (cell_index < first_day || day_num > days_in_month) {
					ImGui::SetCursorPosX(start_x + col * cell_size);
					ImGui::Dummy(ImVec2(cell_size - 4, cell_size - 4));
					if (col < 6) ImGui::SameLine();
					continue;
				}

				bool is_selected = (*day == day_num);
				bool is_weekend = (col == 0 || col == 6);

				ImGui::SetCursorPosX(start_x + col * cell_size + 2);

				if (is_selected) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.5f, 0.9f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.6f, 1.0f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.4f, 0.8f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
				}
				else if (is_weekend) {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.2f, 0.2f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.25f, 0.15f, 0.15f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.25f, 0.3f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.2f, 0.2f, 0.25f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.95f, 1.0f));
				}

				char day_label[8];
				snprintf(day_label, sizeof(day_label), "%d", day_num);

				if (ImGui::Button(day_label, ImVec2(cell_size - 4, cell_size - 4))) {
					*day = day_num;
					date_changed = true;
					ImGui::CloseCurrentPopup();
				}

				ImGui::PopStyleColor(4);
				if (col < 6) ImGui::SameLine();
				day_num++;
			}
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		float btn_w = (popup_width - 30) / 2.0f;
		if (ImGui::Button("Today", ImVec2(btn_w, 28))) {
			time_t now = time(nullptr);
			struct tm* tm_info = localtime(&now);
			*year = tm_info->tm_year + 1900;
			*month = tm_info->tm_mon + 1;
			*day = tm_info->tm_mday;
			date_changed = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("close", ImVec2(btn_w, 28))) {
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}

	return date_changed;
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

	dl->AddRectFilled(cursor_pos, ImVec2(cursor_pos.x + width, cursor_pos.y + height), bg_color);
	dl->AddRect(cursor_pos, ImVec2(cursor_pos.x + width, cursor_pos.y + height), border_color);

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
		ImGui::OpenPopup(popup_id);
	}

	// 年份选择弹窗
	if (ImGui::BeginPopup("YearSelect")) {
		ImGui::Text("YearSelect");
		ImGui::Separator();

		for (int y = year - 5; y <= year + 5; y++) {
			char year_label[8];
			snprintf(year_label, sizeof(year_label), "%d", y);
			bool selected = (y == year);
			if (ImGui::Selectable(year_label, selected)) {
				year = y;
				// 确保月日有效
				int max_days = GetDaysInMonth(year, month);
				if (day > max_days) day = max_days;
			}
			if (selected) ImGui::SetItemDefaultFocus();
		}

		ImGui::EndPopup();
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

#include <misc/cpp/imgui_stdlib.h>

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

	ImGui::InputTextWithHint(id, u8"请输入", text);

	ImGui::PopStyleColor(4);
}
#endif
