#include "TabHeadUI.h"
#include "link.h"     // StripOrderPrefix（TAB 顯示名去掉 "NNNN-" 排序前綴）
#include <windows.h>
#include "SettingUI.h"
#include "FolderUIData.h"
#include "CustomUI.h"
extern FolderUIData gFUD;

void TabHeadUI::Render()
{
    auto& arrTab = gFUD.m_vTabName;
    if (arrTab.empty() || m_Idx < 0 || m_Idx >= (int)arrTab.size())
        return;

    // 甚至不再需要 m_tabBarVersion 強制重建，也不再需要 s_tabHoveredCache！
    if (CustomTab::BeginTabBar("AppBox_Category_Tabs"))
    {
        int i = 0;
        for (const auto& a : arrTab)
        {
            std::wstring displayName = (i == 0) ? a : StripOrderPrefix(a);

            std::string tabNameUtf8 = ToU8(displayName.c_str());

            // 確保外部修改 m_Idx（例如重排資料夾後）時同步選中狀態
            ImGuiTabItemFlags flags = (m_Idx == i) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;

            // 無縫替換！如果最後一個是「設定」分頁，還可以順便傳入設定圖示！
            bool selected = CustomTab::BeginTabItem(tabNameUtf8.c_str(), nullptr, flags);

            if (selected && m_Idx != i)
            {
                m_Idx = i;
            }

            // 原拖曳排序程式碼 100% 保留，無需任何修改！
            if (i > 0 && i < (int)gFUD.gvFolder.size() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("TAB_ORDER", &i, sizeof(int));
                ImGui::TextUnformatted(tabNameUtf8.c_str());
                ImGui::EndDragDropSource();
            }
            if (i > 0 && i < (int)gFUD.gvFolder.size() && ImGui::BeginDragDropTarget())
            {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TAB_ORDER", ImGuiDragDropFlags_AcceptNoDrawDefaultRect))
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
                CustomTab::EndTabItem();
            }
            i++;
        }

        CustomTab::EndTabBar();
    }
}
