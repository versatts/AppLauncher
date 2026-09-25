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

