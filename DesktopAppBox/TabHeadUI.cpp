#include "TabHeadUI.h"
#include "link.h"     // StripOrderPrefix（TAB 顯示名去掉 "NNNN-" 排序前綴）
#include <windows.h>

void TabHeadUI::Render()
{
    if (m_arrTab.size() == 0 || (m_Idx < 0 && m_Idx >= m_arrTab.size()))
    {
        return;
    }

    char tabBarId[64];
    sprintf_s(tabBarId, "AppBox_Category_Tabs##v%d", m_tabBarVersion);
    if (ImGui::BeginTabBar(tabBarId, ImGuiTabBarFlags_None))
    {
        // 🟢 🎯 核心修正 1：建立一個簡單安全的靜態 Hover 快取陣列（支援多達 64 個分頁）
        // 用來記憶每一頁上一幀的滑鼠懸停狀態
        static bool s_tabHoveredCache[64] = { false };

        int i = 0;
        for (auto a : m_arrTab)
        {
            std::wstring displayName = (i == 0) ? a : StripOrderPrefix(a);

            std::string tabNameUtf8;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, displayName.c_str(), -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                tabNameUtf8.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, displayName.c_str(), -1, &tabNameUtf8[0], size_needed, NULL, NULL);
            }

            // 🟢 🎯 核心修正 2：讀取選中狀態與上一幀快取的 Hover 狀態
            bool isCurrentActive = (m_Idx == i);
            bool isHoveredLastFrame = (i < 64) ? s_tabHoveredCache[i] : false;

            // 🎯 如果是當前選中分頁，或者滑鼠「上一幀正在懸停」，則顯示 100% 完整亮度
            if (isCurrentActive || isHoveredLastFrame)
            {
                ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
            }
            else
            {
                // 只有既非當前頁、滑鼠又沒懸停時，標籤文字才單獨變暗
                ImVec4 dimmedColor = ImGui::GetStyleColorVec4(ImGuiCol_Text);
                dimmedColor.w = 0.7f;
                ImGui::PushStyleColor(ImGuiCol_Text, dimmedColor);
            }

            const bool restoring = (m_restoreCountdown > 0);
            bool selected = ImGui::BeginTabItem(tabNameUtf8.c_str(), nullptr,
                (restoring && i == m_Idx) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);

            // 執行完 BeginTabItem 立即釋放文字顏色，防禦內建元件被誤傷
            ImGui::PopStyleColor();

            // 🟢 🎯 核心修正 3：在標籤按鈕剛渲染完的這一瞬間，
            // 透過標準的 ImGui::IsItemHovered() 精準抓取當前幀的懸停狀態，並寫入快取供下一幀變色使用！
            if (i < 64)
            {
                s_tabHoveredCache[i] = ImGui::IsItemHovered();
            }

            if (selected)
            {
                if (restoring)
                {
                    if (i == m_Idx) m_restoreCountdown = 0;
                }
                else if (m_Idx != i)
                {
                    m_Idx = i;
                }
            }

            if (i > 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("TAB_ORDER", &i, sizeof(int));
                ImGui::TextUnformatted(tabNameUtf8.c_str());
                ImGui::EndDragDropSource();
            }
            if (i > 0 && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TAB_ORDER"))
                {
                    int from = *(const int*)payload->Data;
                    if (from > 0 && from != i)
                    {
                        m_pendingTabMoveFrom = from;
                        m_pendingTabMoveTo = i;
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (selected)
            {
                ImGui::EndTabItem();
            }
            i++;
        }
        ImGui::EndTabBar();
    }

    if (m_restoreCountdown > 0)
    {
        m_restoreCountdown--;
    }
}
