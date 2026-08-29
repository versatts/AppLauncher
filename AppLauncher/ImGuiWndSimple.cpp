#include "pch.h"
#include "ImGuiWndSimple.h"

ImGuiWndSimple::ImGuiWndSimple(CWnd* pParent)
	:ImGuiWndBase(pParent)
{
}
void ImGuiWndSimple::ImGuiRender_Main_Pre()
{
	ImGuiWinBegin("Simple title bar");
}


void ImGuiWndSimple::ImGuiRender_Main_Post()
{
	ImGuiWinEnd();
}
