#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <vector>
#include <string>

class TabHeadUI
{
public:
	void Render();
	int* selectedTabIdx = nullptr;
	std::vector<std::wstring>* folders = nullptr;
};

