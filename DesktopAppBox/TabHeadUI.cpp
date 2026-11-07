#include "TabHeadUI.h"
#include "link.h"     // StripOrderPrefix（TAB 顯示名去掉 "NNNN-" 排序前綴）
#include <windows.h>
#include "SettingUI.h"
#include "FolderUIData.h"
bool CustomButton(const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize)
{
    assert(label);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    const ImGuiID id = window->GetID(label);
    const float padX = style.FramePadding.x;
    const float padY = style.FramePadding.y;
    ImVec2 labelSize = ImGui::CalcTextSize(label, nullptr, true);
    labelSize.x += 2;//实际宽要大一些

    // 1. 若傳入的 btnSize 為 0，則根據內容自動計算合適尺寸
    const float spacing = (icon != 0 && labelSize.x > 0.0f) ? style.ItemInnerSpacing.x : 0.0f;
    float contentWidth = iconSize.x + spacing + labelSize.x;//不考虑icon为0的情况。按iconSize进行控制
    if (btnSize.x > 0)
    {
        contentWidth = ImMin(contentWidth, btnSize.x - padX * 2.0f);
    }

    ImVec2 size = btnSize;
    if (size.x <= 0.0f)
        size.x = contentWidth + padX * 2.0f;
    if (size.y <= 0.0f)
        size.y = ImMax(iconSize.y, labelSize.y) + padY * 2.0f;

    // 2. 使用 InvisibleButton 處理所有滑鼠碰撞、點擊、Hover 與 DragDrop 狀態
    const ImVec2 pos = window->DC.CursorPos;
    const bool pressed = ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();

    // 3. 取得對應狀態的主題顏色並繪製背景
    ImDrawList* drawList = window->DrawList;
    const ImU32 bgCol = ImGui::GetColorU32(
        held ? ImGuiCol_ButtonActive :
        hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button
    );
    const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
    drawList->AddRectFilled(pos, rectMax, bgCol, style.FrameRounding);

    // 若樣式有設定邊框，繪製外框
    if (style.FrameBorderSize > 0.0f)
    {
        drawList->AddRect(pos, rectMax, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding, 0, style.FrameBorderSize);
    }

    // 4. 計算「圖示 + 間距 + 文字」整體在按鈕內的置中起點
    float currentX = pos.x + (size.x - contentWidth) * 0.5f;

    // 5. 繪製圖示 (垂直置中)
    if (icon != 0)
    {
        const float iconY = pos.y + (size.y - iconSize.y) * 0.5f;
        drawList->AddImage(icon, ImVec2(currentX, iconY), ImVec2(currentX + iconSize.x, iconY + iconSize.y));
    }
    currentX += iconSize.x + spacing;

    // 6. 繪製文字 (垂直置中，並使用 ClipRect 防止文字超出按鈕邊界)
    {
        const float textY = pos.y + (size.y - labelSize.y) * 0.5f;
        const ImVec4 clipRect(pos.x + padX, pos.y, rectMax.x - padX, rectMax.y);
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(currentX, textY),
            ImGui::GetColorU32(ImGuiCol_Text),
            label,
            ImGui::FindRenderedTextEnd(label), // 支援 "Text##HiddenID" 語法
            0.0f,
            &clipRect
        );
    }

    return pressed;
}

extern FolderUIData gFUD;

void TabHeadUI::Render()
{
    auto& arrTab = gFUD.gvFolderName;
    if (arrTab.size() == 0 || (m_Idx < 0 && m_Idx >= arrTab.size()))
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
        for (auto a : arrTab)
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

            //ImGui::SetNextItemWidth(80);
           
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

    // btn set
    ImVec2 tabBarSize = ImGui::GetItemRectSize();

    float windowPaddingX = ImGui::GetStyle().WindowPadding.x;
    //float buttonWidth = 80.0f; // 這裡可以自訂你的按鈕寬度
    float buttonWidth = 16 + ImGui::GetStyle().FramePadding.x * 2;
    ImVec2 btnSize(buttonWidth, tabBarSize.y - 3.0f);
    float targetPosX = ImGui::GetWindowWidth() - windowPaddingX - btnSize.x;
    ImGui::SameLine();
    ImGui::SetCursorPosX(targetPosX);

    static bool openSetModal = false;
    //if (ImGui::Button("Set", ImVec2(buttonWidth, tabBarSize.y - 3.0f)))
    //if (CustomButton("Set", gFUD.m_texSet, btnSize, ImVec2(16, 16)))
    if (ImGui::ImageButton("Set", gFUD.m_texSet, ImVec2(16, 16)))
    {
        openSetModal = true;
    }
    

    SettingUI::Render(openSetModal);

    if (m_restoreCountdown > 0)
    {
        m_restoreCountdown--;
    }
}
