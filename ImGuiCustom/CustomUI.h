#pragma once
#if IMGUI
#include "ImGuiObj.h"
#include <vector>
#include <string>

class CustomUI
{
private:
	using FnFillCustom = void (*)(ImDrawList* drawList, ImVec2 pos0, ImVec2 pos1, bool bHover, bool bHeld, bool bPressed, bool isToggledBtn, bool bToggled, bool bEnabled);
	static bool IconTextButtonCore(const char* label, const ImVec2& btnSize, ImTextureID icon, const ImVec2& iconSize, bool isToggledBtn, bool bToggled, bool bEnabled);

private:
	struct LastItemInfo
	{
		// 第一行（标题行）信息
		float min_x = 0.0f;
		float max_x = 0.0f;
		float text_y = 0.0f;
		float label_end_x = 0.0f;

		// 第二行（控件本体）信息，供 AppendText2 显示单位使用
		ImVec2 ctrl_min = ImVec2(0.0f, 0.0f);
		ImVec2 ctrl_max = ImVec2(0.0f, 0.0f);
		float right_reserve_w = 0.0f; // Combo2 需让出右侧图片按钮宽度；InputText2 为 0

		bool valid = false;
	};
	static LastItemInfo s_LastItem;
public:
	/************************** button ********************************/
	//图+文本的按钮
	static bool IconTextButton(const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled = true);
	//图+文本的按钮，同时有按下锁定功能
	static bool IconTextToggleButton(bool* v, const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled = true);
	static FnFillCustom s_pFnFillCustom;//自定义填充上面两种按钮

	//无padding的imagebutton, 图多大，按钮就多大
	static bool ImageButtonNP(const char* id, ImTextureID icon, const ImVec2& size);


	/************************** combo *********************************/
	static bool Combo(const char* label, int* current_item, const char* const items[], int items_count,
		int popup_max_height_in_items = -1, ImTextureID arrow_icon = 0, const ImVec2& icon_size = ImVec2(20.0f, 20.0f));
	static bool Combo(const char* label, int* current_item, const std::vector<std::string>& vStr,
		int popup_max_height_in_items = -1, ImTextureID arrow_icon = 0, const ImVec2& icon_size = ImVec2(20.0f, 20.0f));
	//：1 其中sSel，传入后，Combo中选中这一项，如果vStr中无sSel串，则不选中任何项。 2 sSel返回Combo当前选中的字符串，若未选中返回空。
	static bool Combo(const char* label, std::string& sSel, const std::vector<std::string>& vStr,
		int popup_max_height_in_items = -1, ImTextureID arrow_icon = 0, const ImVec2& icon_size = ImVec2(20.0f, 20.0f));
	//此控件为两行，两个控件，使用Group方式。label用于显示一个Text控件在第一行，第二行还是那个Combo。
	static bool Combo2(const char* label, std::string& sSel, const std::vector<std::string>& vStr,
		int popup_max_height_in_items = -1, ImTextureID arrow_icon = 0, const ImVec2& icon_size = ImVec2(20.0f, 20.0f));

	/*************************** input ********************************/
	// 两行版本的ImGui::InputText 完全一致，可直接無痛替換
	static bool InputText2( const char* label, std::string* str,
		ImGuiInputTextFlags flags = 0, ImGuiInputTextCallback callback = nullptr, void* user_data = nullptr );

	/*************************** 附加字符串 *****************************/
	// 在Combo2,InputText2之后使用，在第一行最右侧显示文本
	static void AppendText(const std::string& text, ImU32 color = 0);
	// 在Combo2,InputText2之后使用，在第二行最右侧显示文本
	static void AppendText2(const std::string& text, ImU32 color = 0);

	static bool EditableCombo( const char* label, std::string& sOut, const std::vector<std::string>& vStr,
		int popup_max_height_in_items = -1, ImTextureID arrow_icon = 0, const ImVec2& icon_size = ImVec2(20.0f, 20.0f));


	static bool CalenderIconInputText(const char* label, ImTextureID icon, int& year, int& month, int& day, const char* popup_id,bool& show_popup);
	static void CalenderIconInputText(const char* label, ImTextureID icon, std::string* text, int* year, int* month, int* day);

	static void SearchIconInputText(const char* id, std::string* text, float width);

	static std::string weekdays[7];
	static std::string unitYear;
	static std::string unitMonth;
	static std::string selecdataTips;
	static std::string searchTips;

	//static bool EditableCombo(const char* label, std::string* text,const std::vector<std::string>& items, float width = 200);

};

namespace CustomTab
{
	// 開始一個自訂分頁列（可選傳入圖示、按鈕高度等外觀參數）
	bool BeginTabBar(const char* str_id, ImGuiTabBarFlags flags = ImGuiTabBarFlags_None, bool bLine = true);

	// 渲染單個自訂分頁按鈕（內部封裝 CustomUI::IconTextToggleButton）
	// 支援傳入可選圖示 icon 與 iconSize，若不傳則為純文字分頁按鈕，與原生 BeginTabItem 參數完全相容！
	bool BeginTabItem(const char* label, bool* p_open = nullptr, ImGuiTabItemFlags flags = ImGuiTabItemFlags_None,
		ImTextureID icon = 0, const ImVec2& iconSize = ImVec2(0.0f, 0.0f));

	// 結束當前分頁項目（保持與原生 ImGui::EndTabItem() 呼叫對稱）
	void EndTabItem();

	// 結束自訂分頁列並畫出底部分隔線
	void EndTabBar();
}

#endif
