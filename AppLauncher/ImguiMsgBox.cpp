#include "ImguiMsgBox.h"
#include "ImguiPack.h"
#include <cstring>

static bool        g_bOpen = false;
static ImGuiMsgBoxType g_type;
static char        g_title[128] = { 0 };
static char        g_text[512] = { 0 };
static bool        g_retOk = false;

extern ImguiPack g_GUI;
void ImGuiMsgBox_Show(ImGuiMsgBoxType type, const char* title, const char* text)
{
	g_type = type;
	strncpy_s(g_title, title, sizeof(g_title) - 1);
	strncpy_s(g_text, text, sizeof(g_text) - 1);
	g_bOpen = true;
	g_retOk = false;
}
bool ImGuiMsgBox_Render()
{
	if (g_bOpen)
	{
		ImGui::OpenPopup("##ImGuiMsgBoxModal");
		g_bOpen = false;
	}

	bool clickedOk = false;

	ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize;
	// ========= 关键：限制弹窗最大宽高，消除弹出瞬间拉长闪烁 =========
	ImGui::SetNextWindowSizeConstraints(
		ImVec2(320, 120),
		ImVec2(520, 500)
	);
#if 1
		// ========= 弹窗局部颜色，只对本弹窗生效 =========
		// WindowBg：弹窗主背景；TitleBgActive：标题栏背景；PopupBg：模态遮罩层
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(30.f / 255.f, 32.f / 255.f, 40.f / 255.f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, ImVec4(45.f / 255.f, 48.f / 255.f, 60.f / 255.f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_PopupBg, ImVec4(10.f / 255.f, 10.f / 255.f, 15.f / 255.f, 160.f / 255.f));

#endif

	if (ImGui::BeginPopupModal("##ImGuiMsgBoxModal", nullptr, flags))
	{

		ImColor iconColor;
		const char* iconText;

		switch (g_type)
		{
		case ImGuiMsgBoxType::Info:
			iconColor = ImColor(80, 180, 255);
			iconText = "(i)";
			break;
		case ImGuiMsgBoxType::Warning:
			iconColor = ImColor(255, 190, 60);
			iconText = "(!)";
			break;
		case ImGuiMsgBoxType::Error:
			iconColor = ImColor(255, 85, 85);
			iconText = "[X]";
			break;
		case ImGuiMsgBoxType::Confirm:
			iconColor = ImColor(220, 220, 80);
			iconText = "?";
			break;
		default:
			iconColor = ImColor(200, 200, 200);
			iconText = "";
			break;
		}

		ImGui::Image((ImTextureID)g_GUI.m_srvInfo, ImVec2(24, 24));
		ImGui::SameLine(10.0f);
		ImGui::TextColored(iconColor, "%s  %s", iconText, g_title);
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// TextWrapped：会在窗口可用最大宽度自动换行
		ImGui::TextWrapped("%s", g_text);

		ImGui::Spacing();
		ImGui::Spacing();

		if (g_type == ImGuiMsgBoxType::Confirm)
		{
			if (ImGui::Button(u8"确定", ImVec2(110, 0)))
			{
				g_retOk = true;
				clickedOk = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SameLine(0, 12);
			if (ImGui::Button(u8"取消", ImVec2(110, 0)))
			{
				g_retOk = false;
				clickedOk = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
		}
		else
		{
			if (ImGui::Button(u8"确定", ImVec2(120, 0)))
			{
				g_retOk = true;
				clickedOk = true;
				ImGui::CloseCurrentPopup();
			}
			ImGui::SetItemDefaultFocus();
		}


		ImGui::EndPopup();

	}

		ImGui::PopStyleColor(3);
	return clickedOk;
}
