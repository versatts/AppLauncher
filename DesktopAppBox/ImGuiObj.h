#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <string>
class ImGuiObj
{
public:
	static ImColor ToImClr(const std::string& hexStr);

	static ImVec4 ToImVec4(const std::string& hexStr, float alpha = 1.0f);
};

#define IMCLR ImGuiObj::ToImClr
#define IMVEC4 ImGuiObj::ToImVec4

