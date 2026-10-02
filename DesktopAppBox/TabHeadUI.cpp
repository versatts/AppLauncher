#include "TabHeadUI.h"
#include "link.h"     // StripOrderPrefix（TAB 顯示名去掉 "NNNN-" 排序前綴）
#include <windows.h>

void TabHeadUI::Render()
{
    if (m_arrTab.size() == 0 || (m_Idx < 0 && m_Idx >= m_arrTab.size()))
    {
        return;
    }
    // 3. 建立標籤列 (Tab Bar)
    //    💡 ID 帶版本號：TAB 重排後 m_tabBarVersion+1 換新 ID，強制 ImGui 重建
    //    內部 tab 陣列（其順序在首次創建時定格，之後不隨呼叫順序變化），
    //    新的 m_arrTab 順序才能即時反映到介面上
    char tabBarId[64];
    sprintf_s(tabBarId, "AppBox_Category_Tabs##v%d", m_tabBarVersion);
    if (ImGui::BeginTabBar(tabBarId, ImGuiTabBarFlags_None))
    {
        int i = 0;
        for (auto a : m_arrTab)
        {
            // 🚀 顯示名去掉 "NNNN-" 排序前綴：磁碟目錄名帶前綴僅用於持久化順序，
            //    介面上只顯示乾淨名稱（分頁 0 "Useful" 為固定顯示名，無前綴）
            std::wstring displayName = (i == 0) ? a : StripOrderPrefix(a);

            // 由於 folders 內是寬字元 (wchar_t)，我們需要將其轉成 UTF-8 供 ImGui 顯示
            std::string tabNameUtf8;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, displayName.c_str(), -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                tabNameUtf8.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, displayName.c_str(), -1, &tabNameUtf8[0], size_needed, NULL, NULL);
            }

            // 4. 繪製個別標籤頁按鈕
            // 當用戶點擊某個標籤時，BeginTabItem 會回傳 true
            // 💡 恢復期（m_restoreCountdown > 0）：TabBar 重建後 ImGui 會自動選中
            //    第一個 tab（Useful），照單回寫會把 m_Idx 覆蓋回 0。因此恢復期內
            //    1) 對 m_Idx 的 tab 持續傳 SetSelected，直到選中真正對齊；
            //    2) 不回寫 m_Idx（m_Idx = 被拖分頁的新位置）。
            //    其餘幀絕不傳 SetSelected——每幀強制會覆蓋使用者的點擊切換
            const bool restoring = (m_restoreCountdown > 0);
            bool selected = ImGui::BeginTabItem(tabNameUtf8.c_str(), nullptr,
                (restoring && i == m_Idx) ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None);
            if (selected)
            {
                if (restoring)
                {
                    // 選中狀態已對齊 m_Idx → 恢復完成，交還常規點擊切換
                    if (i == m_Idx)
                    {
                        m_restoreCountdown = 0;
                    }
                }
                else if (m_Idx != i)
                {
                    // 💡 如果此標籤被啟動，且目前的選中索引不等於 i，說明用戶剛剛切換了標籤
                    m_Idx = i;
                }
            }

            // 🚀 TAB 拖動排序：分頁 0（Useful，lnk 根目錄）鎖定第一位——
            //    不提供拖動源，也不作為放置目標
            if (i > 0 && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
            {
                ImGui::SetDragDropPayload("TAB_ORDER", &i, sizeof(int));
                ImGui::TextUnformatted(tabNameUtf8.c_str()); // 拖動時跟隨滑鼠的提示
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

    // 恢復期幀數倒數（超時保護：即使 SetSelected 遲遲未生效，最多 10 幀後
    // 恢復常規點擊切換，不會永久卡住）
    if (m_restoreCountdown > 0)
    {
        m_restoreCountdown--;
    }
}
