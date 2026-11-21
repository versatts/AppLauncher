#include "SettingUI.h"
#include "link.h"     // StripOrderPrefix（TAB 顯示名去掉 "NNNN-" 排序前綴）
#include <windows.h>
#include "AppConfig.h"
#include "Application.h"
#include "ResLoader.h"
extern ResLoader gRes;
void SettingUI::Render(bool& openSetModal)
{
     // 3. 觸發與渲染模態視窗（必須與 Button 分開處理）
    if (openSetModal)
    {
        ImGui::OpenPopup("SetModalPopup"); // 通知 ImGui 開啟指定 ID 的 Popup
        openSetModal = false;              // 隨即重設標記，避免重複觸發
    }

    // 開始繪製模態視窗
    ImGui::PushFont(theApp.m_vFont[0]);
    ImGui::SetNextWindowSize(ImVec2(500, 400), ImGuiCond_Appearing);
    if (ImGui::BeginPopupModal("SetModalPopup", NULL))
    {
        ImGui::Checkbox("启用拖动", &gCfg.m_bEnableFileDragIO);


        // ==================== 新增的字體選擇設定區域 ====================
        ImGui::Spacing();
        ImGui::TextDisabled("字體設定");

        // 1. 下拉選擇字體 (僅顯示 ttf)
        // 這裡利用 ImGui 的 Callback 模式來動態讀取 std::vector 中的字串
        auto FontGetter = [](void* data, int idx, const char** out_text) -> bool {
            auto& fonts = *static_cast<std::vector<std::string>*>(data);
            if (idx < 0 || idx >= (int)fonts.size()) return false;
            *out_text = fonts[idx].c_str();
            return true;
            };

        if (!gCfg.m_vSystemFonts.empty())
        {
            // 取得目前選中字體的名稱（如果索引超出範圍，則顯示提示文字）
            const char* current_item = (gCfg.m_nCurrentFontIndex >= 0 && gCfg.m_nCurrentFontIndex < (int)gCfg.m_vSystemFonts.size())
                ? gCfg.m_vSystemFonts[gCfg.m_nCurrentFontIndex].c_str()
                : "請選擇字體...";

            // 開始繪製下拉選單外框
            if (ImGui::BeginCombo("選擇字體", current_item))
            {
                for (int n = 0; n < (int)gCfg.m_vSystemFonts.size(); n++)
                {
                    bool is_selected = (gCfg.m_nCurrentFontIndex == n);

                    // 繪製每一個可選的字體項目
                    if (ImGui::Selectable(gCfg.m_vSystemFonts[n].c_str(), is_selected))
                    {
                        gCfg.m_nCurrentFontIndex = n;
                        gCfg.m_sEnFont = gCfg.m_vSystemFonts[n]; // 同步更新字體名稱
                    }

                    // 設定預設聚焦滾動到目前選中的項目
                    if (is_selected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo(); // 結束下拉選單內容繪製
            }
        }
        else
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "未偵測到系統字體！");
        }
        // 2. 字號設定 (使用 Slider 或 Drag 控制範圍，例如 8 ~ 72 級字)
        ImGui::SliderInt("字號大小", &gCfg.m_nFontSize, 8, 72, "%d px");

        // 3. 樣式設定 (粗體與斜體，排在同一行以節省空間)
        ImGui::Checkbox("粗體 (Bold)", &gCfg.m_bFontBold);
        ImGui::SameLine();
        ImGui::Checkbox("斜體 (Italic)", &gCfg.m_bFontItalic);
        // =============================================================


        ImGui::Separator();
        // 關閉視窗的按鈕
        if (ImGui::Button("確定", ImVec2(120, 0)))
        {
            ImGui::CloseCurrentPopup();
        }


        auto& g_ShellIcons = gRes.m_vSysIcon;
        float iconSize = 48.0f;
        float windowVisibleX2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;

        for (size_t i = 0; i < g_ShellIcons.size(); ++i)
        {
            const auto& icon = g_ShellIcons[i];
            ImGui::PushID((int)i);

            ImGui::Image((ImTextureID)icon.pSRV, ImVec2(iconSize, iconSize));
            if (ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("Index: %d | ID: %u (%ux%u)", icon.index, icon.resourceId, icon.width, icon.height);
            }

            float lastButtonX2 = ImGui::GetItemRectMax().x;
            float nextButtonX2 = lastButtonX2 + ImGui::GetStyle().ItemSpacing.x + iconSize;
            if (i + 1 < g_ShellIcons.size() && nextButtonX2 < windowVisibleX2)
            {
                ImGui::SameLine();
            }
            ImGui::PopID();
        }

        ImGui::EndPopup();
    }
    ImGui::PopFont();


}
