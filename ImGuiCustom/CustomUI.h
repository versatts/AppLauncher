#pragma once

#include "ImGuiObj.h"
class CustomUI
{
private:
	static bool IconTextButtonCore(const char* label, const ImVec2& btnSize, ImTextureID icon, const ImVec2& iconSize, bool isToggledBtn, bool bToggled, bool bEnabled);
public:
	static bool IconTextButton(const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled = true);
	static bool IconTextToggleButton(bool* v, const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled = true);
	
};


