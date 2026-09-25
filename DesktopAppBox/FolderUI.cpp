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

        // 1. 計算一個完整按鈕所需的固定總寬度（包含按鈕內襯）
        float buttonWidth = iconSize.x + style.FramePadding.x * 2.0f;

        // 2. 取得當前視窗內容的可用總寬度
        float windowWidth = ImGui::GetWindowContentRegionMax().x - ImGui::GetWindowContentRegionMin().x;

        // 3. 計算最多能塞下幾個按鈕
        int maxItemsPerRow = (int)((windowWidth + style.ItemSpacing.x) / (buttonWidth + style.ItemSpacing.x));
        if (maxItemsPerRow < 1) maxItemsPerRow = 1;

        // 4. 動態計算「橫向等距間距」
        float dynamicSpacingX = style.ItemSpacing.x;
        if (maxItemsPerRow > 1 && fud.gpvApp->size() >= (size_t)maxItemsPerRow)
        {
            float totalButtonsWidth = maxItemsPerRow * buttonWidth;
            dynamicSpacingX = (windowWidth - totalButtonsWidth) / (maxItemsPerRow - 1);
        }

        // 5. 💡 關鍵：用 Style 統一注入橫向與縱向間距，讓 ImGui 自動處理行間距！
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(dynamicSpacingX, dynamicSpacingX));

        for (size_t i = 0; i < fud.gpvApp->size(); ++i)
        {
            auto& a = (*fud.gpvApp)[i];

            char szName[64] = { 0 };
            sprintf_s(szName, "btn%d", appIdx);
            appIdx++;

            // 6. 💡 核心排版：由 ImGui 決定換行，完全不使用 SetCursorPos！
            int col = (int)(i % maxItemsPerRow);
            if (i > 0 && col > 0)
            {
                // 如果不是一行的第一個按鈕，就強行並排，並帶入我們計算好的動態間距
                ImGui::SameLine(0.0f, dynamicSpacingX);
            }

            // 7. 繪製 ImageButton
            {
                if (ImGui::ImageButton(szName, (ImTextureID)a.iconSrv, iconSize))
                {
                    ShellExecuteW(hwnd, L"open", a.exePathBuf, nullptr, nullptr, SW_SHOW);
                }
            }

            // 8. 懸停 Tooltip
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

        // 9. 💡 記得彈出剛才 Push 的樣式變數
        ImGui::PopStyleVar();
    }
}

