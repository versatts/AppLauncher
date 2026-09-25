#include "TabHeadUI.h"
#include <windows.h>
void TabHeadUI::Render()
{
    if (!folders || !selectedTabIdx)
    {
        return;
    }
    // 3. 建立標籤列 (Tab Bar)
    if (ImGui::BeginTabBar("AppBox_Category_Tabs", ImGuiTabBarFlags_None))
    {
        for (int i = 0; i < (int)(*folders).size(); ++i)
        {
            // 由於 folders 內是寬字元 (wchar_t)，我們需要將其轉成 UTF-8 供 ImGui 顯示
            std::string tabNameUtf8;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, (*folders)[i].c_str(), -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                tabNameUtf8.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, (*folders)[i].c_str(), -1, &tabNameUtf8[0], size_needed, NULL, NULL);
            }

            // 4. 繪製個別標籤頁按鈕
            // 當用戶點擊某個標籤時，BeginTabItem 會回傳 true
            if (ImGui::BeginTabItem(tabNameUtf8.c_str()))
            {
                // 💡 如果此標籤被啟動，且目前的選中索引不等於 i，說明用戶剛剛切換了標籤
                if (*selectedTabIdx != i)
                {
                    *selectedTabIdx = i;

                    // 【可以在這裡觸發重新載入邏輯】
                    // 例如：gvApp.clear();
                    // 重新呼叫 EnumLnkFilesInAppDir 並載入對應分類的圖標 SRV
                }

                ImGui::EndTabItem();
            }
        }
        ImGui::EndTabBar();
    }
}
