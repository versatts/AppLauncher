#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <list>
#include <vector>
#include <string>

class TabHeadUI
{
public:
	void Render();
	int m_Idx = -1;
	std::vector<std::wstring> m_arrTab;
};

