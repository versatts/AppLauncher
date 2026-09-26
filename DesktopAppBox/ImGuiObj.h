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
};

#define IMCLR ImGuiObj::ToImClr

