#pragma once

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"
#include <windows.h>
class FolderUIData;
class FolderUI
{
public:
	void Render(HWND hwnd, FolderUIData&);
};

