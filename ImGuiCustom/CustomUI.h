#pragma once
#if IMGUI
#include "ImGuiObj.h"
class CustomUI
{
private:
	using FnFillCustom = void (*)(ImDrawList* drawList, ImVec2 pos0, ImVec2 pos1, bool bHover, bool bHeld, bool bPressed, bool isToggledBtn, bool bToggled, bool bEnabled);
	static bool IconTextButtonCore(const char* label, const ImVec2& btnSize, ImTextureID icon, const ImVec2& iconSize, bool isToggledBtn, bool bToggled, bool bEnabled);
public:
	static bool IconTextButton(const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled = true);
	static bool IconTextToggleButton(bool* v, const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled = true);

	static FnFillCustom s_pFnFillCustom;//×Ô¶¨ÒåÌî³ä
	
	static bool CalenderIconInputText(const char* label, ImTextureID icon, int& year, int& month, int& day, const char* popup_id,bool& show_popup);

	static void SearchIconInputText(const char* id, std::string* text, float width);
};


#endif
