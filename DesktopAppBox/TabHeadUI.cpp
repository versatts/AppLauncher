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

    if (CustomTab::BeginTabBar("AppBox_Category_Tabs"))
    {
        int i = 0;
        for (const auto& a : arrTab)
        {
            std::wstring displayName = (i == 0) ? a : StripOrderPrefix(a);
            std::string tabNameUtf8 = ToU8(displayName.c_str());

            ImGuiTabItemFlags flags = (m_Idx == i) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
            //CustomTab::SetNextItemWidth(40.0f);
            bool selected = CustomTab::BeginTabItem(tabNameUtf8.c_str(), nullptr, flags);

            // 記下剛才渲染完的這個 TAB 矩形（用於繪製垂直插入指示線）
            ImVec2 tabMin = ImGui::GetItemRectMin();
            ImVec2 tabMax = ImGui::GetItemRectMax();

            if (selected && m_Idx != i)
            {
                m_Idx = i;
            }

            if (i > 0 && i < (int)gFUD.gvFolder.size() && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("TAB_ORDER", &i, sizeof(int));
                ImGui::TextUnformatted(tabNameUtf8.c_str());
                ImGui::EndDragDropSource();
            }
            if (i > 0 && i < (int)gFUD.gvFolder.size() && ImGui::BeginDragDropTarget())
            {
                ImGuiDragDropFlags acceptFlags = ImGuiDragDropFlags_AcceptNoDrawDefaultRect | ImGuiDragDropFlags_AcceptBeforeDelivery;
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TAB_ORDER", acceptFlags))
                {
                    int from = *(const int*)payload->Data;
                    if (from > 0 && from != i)
                    {
                        float gapW = ImGui::GetStyle().ItemSpacing.x;
                        ImU32 dropCol = ImGui::GetColorU32(ImGuiCol_DragDropTarget);

                        // 直接計算填滿目標 TAB 左側或右側 ItemSpacing 間隙的實心矩形
                        ImVec2 rectMin = (from < i) ? ImVec2(tabMax.x, tabMin.y) : ImVec2(tabMin.x - gapW, tabMin.y);
                        ImVec2 rectMax = (from < i) ? ImVec2(tabMax.x + gapW, tabMax.y) : ImVec2(tabMin.x, tabMax.y);

                        ImGui::GetWindowDrawList()->AddRectFilled(rectMin, rectMax, dropCol);

                        if (payload->IsDelivery())
                        {
                            m_pendingTabMoveFrom = from;
                            m_pendingTabMoveTo = i;
                        }
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
