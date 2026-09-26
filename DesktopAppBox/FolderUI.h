#pragma once
#include "ImGuiObj.h"
#include <windows.h>
class FolderUIData;
class FolderUI:public ImGuiObj
{
public:
	void Render(HWND hwnd, FolderUIData&);
};

