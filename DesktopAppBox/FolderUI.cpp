#include "FolderUI.h"
#include "FolderUIData.h"

bool IsPath(const std::string& inputStr)
{
    if (inputStr.empty()) return false;

    // 1. 🚀 核心升級：優先進行網路 UNC 路徑結構判斷 (例如 \\192.168.1.1\mydir)
    // 檢查是否以雙反斜線 "\\ " 開頭
    if (inputStr.size() >= 2 && inputStr[0] == '\\' && inputStr[1] == '\\')
    {
        // 💡 網路路徑防禦：
        // 只要是 \\ 開頭，不管尾端有沒有帶反斜線，它在本質上代表的都是一個「伺服器共享資料夾」或「目錄」
        // 我們直接將其視為路徑，這樣即使網路斷線或需要密碼，也絕對不會誤判或卡死！
        return true;
    }

    // 2. 💡 本地路徑攔截：處理末尾帶有斜槓的明確目錄 (例如 D:\abc\)
    char lastChar = inputStr.back();
    if (lastChar == '\\' || lastChar == '/')
    {
        return true;
    }

    // 3. 實體檔案系統檢查 (主要針對本地磁碟如 D:\mydir)
    DWORD attributes = ::GetFileAttributesA(inputStr.c_str());

    if (attributes != INVALID_FILE_ATTRIBUTES)
    {
        // 檢查是否含有資料夾旗標
        if (attributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            return true;
        }
    }

    return false; // 代表它是常規檔案 (例如 .exe, .txt) 或無效路徑
}
void FolderUI::Render(HWND hwnd, FolderUIData& fud)
{
    fud.gpvApp = &(fud.gvFolder[fud.gCurTab].vApp);

    // ==================== 💡 核心修正：動態生成唯一的子視窗 ID ====================
    // 根據當前的分頁索引（fud.gCurTab）來命名子視窗，例如 "AppGridRegion_0", "AppGridRegion_1"
    char childWindowId[64];
    sprintf_s(childWindowId, "AppGridRegion_%d", fud.gCurTab);

    // 使用動態產生的唯一 ID 進入子視窗
    if (ImGui::BeginChild(childWindowId, ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, 0))
    {
        ImGuiStyle& style = ImGui::GetStyle();

        int appIdx = 0;
        float fSize = 64;
        ImVec2 iconSize = ImVec2(fSize, fSize);

        // ==================== 0. 控制開關 ====================
        bool bShowText = fud.m_bShowName;

        // ==================== 1. 設定基礎參數 ====================
        const float minPadding = 10.0f;    // 左右最小 Padding
        const float topPadding = 5.0f;     // 上邊沿固定的 Padding
        const float itemSpacingX = 5.0f;   // 圖標之間固定的橫向間距
        const float itemSpacingY = 10.0f;  // 圖標單元之間的縱向間距

        const float textBlockHeight = 20.0f; // 文字塊的固定高度
        const float textGap = 4.0f;          // 圖標與下方文字塊之間的微小間距

        float buttonWidth = iconSize.x + style.FramePadding.x * 2.0f;
        float buttonHeight = iconSize.y + style.FramePadding.y * 2.0f;

        float totalCellHeight = buttonHeight;
        if (bShowText)
        {
            totalCellHeight = buttonHeight + textGap + textBlockHeight;
        }

        // 💡 修正 1：取得當前「子視窗」內部真正的可用總寬度，防止計算換行時跑版
        float windowWidth = ImGui::GetContentRegionAvail().x;

        // ==================== 2. 核心幾何計算 ====================
        int maxItemsPerRow = (int)((windowWidth - (minPadding * 2.0f) + itemSpacingX) / (buttonWidth + itemSpacingX));
        if (maxItemsPerRow < 1) maxItemsPerRow = 1;

        int itemsInFirstRow = (int)fud.gpvApp->size();
        if (itemsInFirstRow > maxItemsPerRow) {
            itemsInFirstRow = maxItemsPerRow;
        }

        float totalItemsWidth = (itemsInFirstRow * buttonWidth) + ((itemsInFirstRow - 1) * itemSpacingX);
        if (itemsInFirstRow <= 1) {
            totalItemsWidth = buttonWidth;
        }

        float dynamicPadding = (windowWidth - totalItemsWidth) / 2.0f;
        if (dynamicPadding < minPadding) dynamicPadding = minPadding;

        // ==================== 3. 動態渲染與換行 ====================
        // 💡 修正 2：在儲存起始位置前，先套用一個頂部間距，避免內容黏在子視窗最上緣
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + topPadding);
        ImVec2 startCursorPos = ImGui::GetCursorPos();

        float maxCalculatedY = startCursorPos.y; // 用於追蹤最後一行的最大高度位置

        for (size_t i = 0; i < fud.gpvApp->size(); ++i)
        {
            auto& a = (*fud.gpvApp)[i];

            char szName[64] = { 0 };
            sprintf_s(szName, "btn%d", appIdx);
            appIdx++;

            int col = (int)(i % maxItemsPerRow);
            int row = (int)(i / maxItemsPerRow);

            // 計算當前單元的絕對 X 與 Y 座標 (移除重複加的 topPadding)
            float targetX = startCursorPos.x + dynamicPadding + (col * (buttonWidth + itemSpacingX));
            float targetY = startCursorPos.y + (row * (totalCellHeight + itemSpacingY));

            // 更新最後一行涵蓋的最大高度
            if (targetY + totalCellHeight > maxCalculatedY) {
                maxCalculatedY = targetY + totalCellHeight;
            }

            // 設定排版起點
            ImGui::SetCursorPosX(targetX);
            ImGui::SetCursorPosY(targetY);

            // 解析路徑檔名
            std::string sTip;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                sTip.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, &sTip[0], size_needed, NULL, NULL);
            }

            std::string displayName = a.sDisplay;

            // A. 繪製 ImageButton
            if (ImGui::ImageButton(szName, (ImTextureID)a.iconSrv, iconSize))
            {
                ShellExecuteW(hwnd, L"open", a.exePathBuf, nullptr, nullptr, SW_SHOW);
            }

            // B. 繪製下方的文字塊
            if (bShowText)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                ImVec2 btnMin = ImGui::GetItemRectMin();
                ImVec2 btnMax = ImGui::GetItemRectMax();

                ImVec2 textBlockMin = ImVec2(btnMin.x, btnMax.y + textGap);
                ImVec2 textBlockMax = ImVec2(btnMax.x, textBlockMin.y + textBlockHeight);

                ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
                float cornerRadius = 4.0f;
                drawList->AddRectFilled(textBlockMin, textBlockMax, bgColor, cornerRadius);

                float textPaddingX = 4.0f;
                float maxTextWidth = buttonWidth - (textPaddingX * 2.0f);

                std::string finalRenderName = displayName;
                ImVec2 textSize = ImGui::CalcTextSize(finalRenderName.c_str());
                float textRenderX = textBlockMin.x + textPaddingX;

                if (textSize.x <= maxTextWidth)
                {
                    float remainingSpace = buttonWidth - textSize.x;
                    textRenderX = textBlockMin.x + (remainingSpace * 0.5f);
                }
                else
                {
                    finalRenderName = "";
                    std::string ellipsis = "...";
                    float ellipsisWidth = ImGui::CalcTextSize(ellipsis.c_str()).x;

                    if (ellipsisWidth < maxTextWidth)
                    {
                        std::string temp = "";
                        for (size_t charIdx = 0; charIdx < displayName.size(); ++charIdx)
                        {
                            unsigned char c = displayName[charIdx];
                            temp += displayName[charIdx];
                            if (c >= 0x80) {
                                while (charIdx + 1 < displayName.size() && (static_cast<unsigned char>(displayName[charIdx + 1]) & 0xC0) == 0x80) {
                                    charIdx++;
                                    temp += displayName[charIdx];
                                }
                            }

                            if (ImGui::CalcTextSize(temp.c_str()).x + ellipsisWidth > maxTextWidth)
                            {
                                break;
                            }
                            finalRenderName = temp;
                        }
                        finalRenderName += ellipsis;
                    }
                    else
                    {
                        finalRenderName = displayName;
                    }
                }

                ImVec2 textPos = ImVec2(textRenderX, textBlockMin.y + (textBlockHeight - ImGui::GetTextLineHeight()) * 0.5f);
                ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
                drawList->AddText(textPos, textColor, finalRenderName.c_str());
            }

            // C. 懸停 Tooltip
            float hoverHeightOffset = bShowText ? (textGap + textBlockHeight) : 0.0f;
            ImVec2 maxHoverPos = ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y + hoverHeightOffset);

            if (ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), maxHoverPos))
            {
                if (IsPath(sTip))
                {
                    ImGui::SetTooltip(sTip.c_str());
                }
            }
        }

        // 💡 修正 3：關鍵！因為你是手動 SetCursor 操作，ImGui 無法得知內容底標在哪裡。
        // 我們必須主動將光標移到最後一行底部，並放置一個 Dummy 空白元件，來告訴 Child 視窗「內容到底了」，進而安全地觸發內部滾動條。
        if (!fud.gpvApp->empty())
        {
            ImGui::SetCursorPosY(maxCalculatedY + itemSpacingY);
            ImGui::Dummy(ImVec2(0.0f, 1.0f));
        }
    }
    ImGui::EndChild(); // 💡 結束子視窗
}

#if 0
void FolderUI::Render(HWND hwnd, FolderUIData& fud)
{
    fud.gpvApp = &(fud.gvFolder[fud.gCurTab].vApp);

    {
        ImGuiStyle& style = ImGui::GetStyle();

        int appIdx = 0;
        float fSize = 64;
        ImVec2 iconSize = ImVec2(fSize, fSize);

        // ==================== 0. 控制開關 ====================
        // 💡 關鍵變數：控制是否顯示下方的 Text 區塊（可以改為 fud.bShowText 或全域變數）
        bool bShowText = fud.m_bShowName;

        // ==================== 1. 設定基礎參數 ====================
        const float minPadding = 10.0f;    // 左右最小 Padding
        const float topPadding = 5.0f;     // 上邊沿固定的 Padding
        const float itemSpacingX = 5.0f;   // 圖標之間固定的橫向間距
        const float itemSpacingY = 10.0f;  // 圖標單元之間的縱向間距

        const float textBlockHeight = 20.0f; // 文字塊的固定高度
        const float textGap = 4.0f;          // 圖標與下方文字塊之間的微小間距

        // 一個完整按鈕所需的固定總寬度（包含按鈕內襯 FramePadding）
        float buttonWidth = iconSize.x + style.FramePadding.x * 2.0f;
        float buttonHeight = iconSize.y + style.FramePadding.y * 2.0f;

        // 💡 核心修正：根據 bShowText 變數，動態計算一個「圖標單元」的總高度
        float totalCellHeight = buttonHeight;
        if (bShowText)
        {
            totalCellHeight = buttonHeight + textGap + textBlockHeight;
        }

        // 取得當前工作視窗的總可用寬度
        float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

        // ==================== 2. 核心幾何計算 ====================
        int maxItemsPerRow = (int)((windowWidth - (minPadding * 2.0f) + itemSpacingX) / (buttonWidth + itemSpacingX));
        if (maxItemsPerRow < 1) maxItemsPerRow = 1;

        int itemsInFirstRow = (int)fud.gpvApp->size();
        if (itemsInFirstRow > maxItemsPerRow) {
            itemsInFirstRow = maxItemsPerRow;
        }

        float totalItemsWidth = (itemsInFirstRow * buttonWidth) + ((itemsInFirstRow - 1) * itemSpacingX);
        if (itemsInFirstRow <= 1) {
            totalItemsWidth = buttonWidth;
        }

        float dynamicPadding = (windowWidth - totalItemsWidth) / 2.0f;
        if (dynamicPadding < minPadding) dynamicPadding = minPadding;

        // ==================== 3. 動態渲染與換行 ====================
        ImVec2 startCursorPos = ImGui::GetCursorPos();

        for (size_t i = 0; i < fud.gpvApp->size(); ++i)
        {
            auto& a = (*fud.gpvApp)[i];

            char szName[64] = { 0 };
            sprintf_s(szName, "btn%d", appIdx);
            appIdx++;

            int col = (int)(i % maxItemsPerRow);
            int row = (int)(i / maxItemsPerRow);

            // 計算當前單元的絕對 X 與 Y 座標
            float targetX = startCursorPos.x + dynamicPadding + (col * (buttonWidth + itemSpacingX));
            float targetY = startCursorPos.y + topPadding + (row * (totalCellHeight + itemSpacingY));

            // 設定排版起點
            ImGui::SetCursorPosX(targetX);
            ImGui::SetCursorPosY(targetY);

            // 解析路徑檔名
            std::string sTip;
            int size_needed = WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, NULL, 0, NULL, NULL);
            if (size_needed > 0) {
                sTip.resize(size_needed - 1);
                WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, &sTip[0], size_needed, NULL, NULL);
            }

            std::string displayName = a.sDisplay;

            // A. 繪製 ImageButton
            if (ImGui::ImageButton(szName, (ImTextureID)a.iconSrv, iconSize))
            {
                ShellExecuteW(hwnd, L"open", a.exePathBuf, nullptr, nullptr, SW_SHOW);
            }

            // B. 💡 繪製下方的文字塊（受到 bShowText 變數控制）
            if (bShowText)
            {
                ImDrawList* drawList = ImGui::GetWindowDrawList();

                ImVec2 btnMin = ImGui::GetItemRectMin();
                ImVec2 btnMax = ImGui::GetItemRectMax();

                ImVec2 textBlockMin = ImVec2(btnMin.x, btnMax.y + textGap);
                ImVec2 textBlockMax = ImVec2(btnMax.x, textBlockMin.y + textBlockHeight);

                // 1. 繪製文字塊的圓角矩形背景
                ImU32 bgColor = ImGui::GetColorU32(ImGuiCol_FrameBg);
                float cornerRadius = 4.0f;
                drawList->AddRectFilled(textBlockMin, textBlockMax, bgColor, cornerRadius);

                // 2. 計算文字的最大可用寬度
                float textPaddingX = 4.0f;
                float maxTextWidth = buttonWidth - (textPaddingX * 2.0f);

                // 3. 動態計算文字尺寸（置中或截斷）
                std::string finalRenderName = displayName;
                ImVec2 textSize = ImGui::CalcTextSize(finalRenderName.c_str());
                float textRenderX = textBlockMin.x + textPaddingX;

                if (textSize.x <= maxTextWidth)
                {
                    float remainingSpace = buttonWidth - textSize.x;
                    textRenderX = textBlockMin.x + (remainingSpace * 0.5f);
                }
                else
                {
                    finalRenderName = "";
                    std::string ellipsis = "...";
                    float ellipsisWidth = ImGui::CalcTextSize(ellipsis.c_str()).x;

                    if (ellipsisWidth < maxTextWidth)
                    {
                        std::string temp = "";
                        for (size_t charIdx = 0; charIdx < displayName.size(); ++charIdx)
                        {
                            unsigned char c = displayName[charIdx];
                            temp += displayName[charIdx];
                            if (c >= 0x80) {
                                while (charIdx + 1 < displayName.size() && (static_cast<unsigned char>(displayName[charIdx + 1]) & 0xC0) == 0x80) {
                                    charIdx++;
                                    temp += displayName[charIdx];
                                }
                            }

                            if (ImGui::CalcTextSize(temp.c_str()).x + ellipsisWidth > maxTextWidth)
                            {
                                break;
                            }
                            finalRenderName = temp;
                        }
                        finalRenderName += ellipsis;
                    }
                    else
                    {
                        finalRenderName = displayName;
                    }
                }

                // 4. 計算垂直居中座標並繪製
                ImVec2 textPos = ImVec2(textRenderX, textBlockMin.y + (textBlockHeight - ImGui::GetTextLineHeight()) * 0.5f);
                ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);
                drawList->AddText(textPos, textColor, finalRenderName.c_str());
            }

            // C. 懸停 Tooltip（動態計算滑鼠感應邊界，如果沒顯示文字塊，感應高度要縮小）
            float hoverHeightOffset = bShowText ? (textGap + textBlockHeight) : 0.0f;
            ImVec2 maxHoverPos = ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y + hoverHeightOffset);

            if (ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), maxHoverPos))
            {
                if (IsPath(sTip))
                {
                    ImGui::SetTooltip(sTip.c_str());
                }
            }
        }
    }
}
#endif
#if 0
void FolderUI::Render(HWND hwnd, FolderUIData& fud)
{
    fud.gpvApp = &(fud.gvFolder[fud.gCurTab].vApp);

    {
        ImGuiStyle& style = ImGui::GetStyle();

        int appIdx = 0;
        float fSize = 64;
        ImVec2 iconSize = ImVec2(fSize, fSize);

        // ==================== 1. 設定基礎參數 ====================
        const float minPadding = 10.0f;    // 左右最小 Padding
        const float topPadding = 5.0f;     // 💡 新增：上邊沿固定的 Padding
        const float itemSpacingX = 5.0f;   // 圖標之間固定的橫向間距
        const float itemSpacingY = 5.0f;   // 圖標之間固定的縱向間距

        float buttonWidth = iconSize.x + style.FramePadding.x * 2.0f;
        float buttonHeight = iconSize.y + style.FramePadding.y * 2.0f;

        // 取得當前工作視窗的總可用寬度
        float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

        // ==================== 2. 核心幾何計算 ====================
        int maxItemsPerRow = (int)((windowWidth - (minPadding * 2.0f) + itemSpacingX) / (buttonWidth + itemSpacingX));
        if (maxItemsPerRow < 1) maxItemsPerRow = 1;

        int itemsInFirstRow = (int)fud.gpvApp->size();
        if (itemsInFirstRow > maxItemsPerRow) {
            itemsInFirstRow = maxItemsPerRow;
        }

        float totalItemsWidth = (itemsInFirstRow * buttonWidth) + ((itemsInFirstRow - 1) * itemSpacingX);
        if (itemsInFirstRow <= 1) {
            totalItemsWidth = buttonWidth;
        }

        float dynamicPadding = (windowWidth - totalItemsWidth) / 2.0f;
        if (dynamicPadding < minPadding) dynamicPadding = minPadding;

        // ==================== 3. 動態渲染與換行 ====================
        // 備份最初的 CursorPos（這代表目前 UI 內容繪製的起點）
        ImVec2 startCursorPos = ImGui::GetCursorPos();

        for (size_t i = 0; i < fud.gpvApp->size(); ++i)
        {
            auto& a = (*fud.gpvApp)[i];

            char szName[64] = { 0 };
            sprintf_s(szName, "btn%d", appIdx);
            appIdx++;

            int col = (int)(i % maxItemsPerRow);
            int row = (int)(i / maxItemsPerRow);

            // 💡 完美的絕對座標排版
            if (col == 0)
            {
                // 設定橫向位置：起點 + 左邊動態 Padding
                ImGui::SetCursorPosX(startCursorPos.x + dynamicPadding);

                // 💡 修正縱向位置：每一行都要加上 topPadding，讓整體往下移 5 像素
                float targetY = startCursorPos.y + topPadding + (row * (buttonHeight + itemSpacingY));
                ImGui::SetCursorPosY(targetY);
            }
            else
            {
                ImGui::SameLine();

                // 設定橫向位置
                float targetX = startCursorPos.x + dynamicPadding + (col * (buttonWidth + itemSpacingX));
                ImGui::SetCursorPosX(targetX);

                // 💡 修正縱向位置：非首個圖標換行時，縱向也要精準對齊該行的 Y 軸
                float targetY = startCursorPos.y + topPadding + (row * (buttonHeight + itemSpacingY));
                ImGui::SetCursorPosY(targetY);
            }

            // 繪製 ImageButton
            if (ImGui::ImageButton(szName, (ImTextureID)a.iconSrv, iconSize))
            {
                ShellExecuteW(hwnd, L"open", a.exePathBuf, nullptr, nullptr, SW_SHOW);
            }

            // 懸停 Tooltip
            if (ImGui::IsItemHovered())
            {
                std::string sTip;
                int size_needed = WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, NULL, 0, NULL, NULL);
                if (size_needed > 0) {
                    sTip.resize(size_needed - 1);
                    WideCharToMultiByte(CP_UTF8, 0, a.exePathBuf, -1, &sTip[0], size_needed, NULL, NULL);
                }
                if (IsPath(sTip))
                {
                    ImGui::SetTooltip(sTip.c_str());
                }
            }
        }
    }
}
#endif

