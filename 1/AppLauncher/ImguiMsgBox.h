#pragma once
#include "imgui.h"

enum class ImGuiMsgBoxType
{
	Info,
	Warning,
	Error,
	Confirm
};

void ImGuiMsgBox_Show(ImGuiMsgBoxType type, const char* title, const char* text);
// 返回值：true=点击OK；false=点击Cancel；仅Confirm类型有效
bool ImGuiMsgBox_Render();
