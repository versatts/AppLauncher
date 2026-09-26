#pragma once
#include "ImGuiObj.h"
#include <list>
#include <vector>
#include <string>

class TabHeadUI: public ImGuiObj
{
public:
	void Render();
	int m_Idx = -1;
	std::vector<std::wstring> m_arrTab;
};

