#pragma once
#include "ImGuiWndBase.h"


class ImGuiWndSimple : public ImGuiWndBase 
{
public:
	ImGuiWndSimple(CWnd* pParent = nullptr);	// 标准构造函数
protected:
	virtual void ImGuiRender_Main_Pre();
	virtual void ImGuiRender_Main_Post();
};

