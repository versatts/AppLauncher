#include "FolderUI.h"
#include "FolderUIData.h"
#include "link.h"    // StripOrderPrefix（改名時保留排序前綴）
#include "AppConfig.h"
#include "Application.h"
// 來自 main.cpp：檔案拖入/拖出功能總開關
extern AppConfig gCfg;
// 來自 main.cpp：拖出確認後刪除 lnk 檔案，並排隊幀末熱重載當前分頁
void AppRemoveDragOutItem(const std::wstring& lnkPath);

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

void MoveAppElement(std::vector<ST_APP>& vApp, int fromIndex, int toIndex)
{
    if (fromIndex == toIndex || fromIndex < 0 || toIndex < 0 ||
        fromIndex >= (int)vApp.size() || toIndex >= (int)vApp.size()) {
        return;
    }

    // 儲存被拖曳的項目
    ST_APP targetApp = std::move(vApp[fromIndex]);

    // 從原位置刪除
    vApp.erase(vApp.begin() + fromIndex);

    // 插入到新位置
    vApp.insert(vApp.begin() + toIndex, std::move(targetApp));
}

    
    // 依當前子視窗寬度與項目數量，計算換行與置中所需的全部幾何
GridLayout FolderUI::CalcGridLayout(bool bShowText, size_t itemCount)
{
    ImGuiStyle& style = ImGui::GetStyle();
    GridLayout g;
    g.bShowText = bShowText;
    float w = g.iconSize.x + btnPadding * 2.0f;
    float h = g.iconSize.y + btnPadding * 2.0f;
    g.btnSize.x = w;
    g.btnSize.y = h;
    g.totalCellHeight = h;
    if (bShowText)
    {
        g.totalCellHeight = h + g.textGap + g.textBlockHeight;
    }

    // 💡 穩定寬度：GetContentRegionAvail().x 是動態值（滾動條出現時自動扣減），
    //    這裡改用子視窗自身總寬 GetWindowSize().x（恆定，滾動條僅懸浮不佔位）
    float windowWidth = ImGui::GetWindowSize().x;
    float oneItemW = w + 2 * g.itemSpacingX;
    g.maxItemsPerRow = (int)((windowWidth - 2 * g.sidePadding) / oneItemW);
    if (g.maxItemsPerRow < 1) g.maxItemsPerRow = 1;

    float totalItemsWidth = g.maxItemsPerRow * oneItemW - 2 * g.itemSpacingX;

    g.dynamicPadding = (windowWidth - totalItemsWidth) / 2.0f;

    // 💡 先套用頂部間距，避免內容黏在子視窗最上緣
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + g.topPadding);
    g.startPos = ImGui::GetCursorPos();
    return g;
}

    // UTF-8 安全截斷：超出 maxWidth 時保留完整多位元組字元並補 "..."
    std::string FolderUI::TruncateUtf8WithEllipsis(const std::string& text, float maxWidth)
    {
        const std::string ellipsis3 = "...";
        const std::string ellipsis2 = "..";
        float ellipsisWidth = ImGui::CalcTextSize(ellipsis3.c_str()).x;
        if (ellipsisWidth >= maxWidth)
        {
            return text; // 連 "..." 都放不下時，維持原字串
        }

        std::string result;
        size_t charIdx = 0;
        // 用 while 完整控制 UTF-8 多位元組字元的追蹤，避免斷字造成亂碼
        while (charIdx < text.size())
        {
            size_t byteCount = 1;
            unsigned char c = text[charIdx];

            if (c >= 0x80) {
                if ((c & 0xE0) == 0xC0) byteCount = 2;
                else if ((c & 0xF0) == 0xE0) byteCount = 3;
                else if ((c & 0xF8) == 0xF0) byteCount = 4;
            }

            if (charIdx + byteCount > text.size()) break;

            std::string nextChar = text.substr(charIdx, byteCount);
            if (ImGui::CalcTextSize((result + nextChar).c_str()).x + ellipsisWidth > maxWidth)
            {
                break;
            }

            result += nextChar;
            charIdx += byteCount;
        }
        return result + ellipsis2;
    }


    static int s_editingIdx = -1;
    static char s_renameBuf[256] = {0};
    static bool s_justStartedEdit = false;

    void FolderUI::RenderAppButton(HWND hwnd, ST_APP& app, const char* btnId, size_t itemIdx,
        GridLayout& g, int padding, int& moveFrom, int& moveTo)
    {
        ImGui::PushID((int)itemIdx);

        bool isEditing = (s_editingIdx == (int)itemIdx);
        bool textDoubleClicked = false;

        // 編輯態時，按鈕本身不畫文字（show_text = false），改由下方 InputText 接手
        bool clicked = ImageTextButton(
            btnId,
            (ImTextureID)app.iconSrv,
            g.iconSize,
            padding,
            g.bShowText && !isEditing,
            app.sDisplay.c_str(),
            theApp.m_vFont[2],
            padding * 1.3f,
            &textDoubleClicked
        );

        // 雙擊文字區：進入編輯態（不觸發程式啟動）
        if (textDoubleClicked)
        {
            s_editingIdx = (int)itemIdx;
            s_justStartedEdit = true;
            strncpy_s(s_renameBuf, sizeof(s_renameBuf), app.sDisplay.c_str(), _TRUNCATE);
        }
        // 單擊按鈕（且非雙擊文字）：啟動目標程式
        else if (clicked)
        {
            s_editingIdx = -1;

            const wchar_t* pArgs = (app.exeArgsBuf != L'\0') ? app.exeArgsBuf : nullptr;
            const wchar_t* pSlash = wcsrchr(app.exePathBuf, L'\\');
            if (pSlash)
            {
                std::wstring workDir(app.exePathBuf, pSlash - app.exePathBuf);
                ShellExecuteW(hwnd, L"open", app.sLnkPath.c_str(), pArgs, workDir.c_str(), SW_SHOW);
            }
            else
            {
                ShellExecuteW(hwnd, L"open", app.exePathBuf, pArgs, nullptr, SW_SHOW);
            }
        }

        // 1. 拖曳來源 (Source) —— 直接綁定在上面的 ImageTextButton 上
        if (gCfg.m_bEnableFileDragIO && ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            int currentIdx = (int)itemIdx;
            ImGui::SetDragDropPayload("APP_GRID_ITEM", &currentIdx, sizeof(int));

            // 拖曳時同時顯示圖示與文字
            ImVec2 dragIconSize(24.0f, 24.0f);
            ImGui::Image((ImTextureID)app.iconSrv, dragIconSize);
            ImGui::SameLine();
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (dragIconSize.y - ImGui::GetTextLineHeight()) * 0.5f);
            ImGui::Text("移動: %s", app.sDisplay.c_str());

            if (gCfg.m_bEnableFileDragIO && !app.sLnkPath.empty())
            {
                POINT pt;
                ::GetCursorPos(&pt);
                ::ScreenToClient(hwnd, &pt);
                RECT rc;
                ::GetClientRect(hwnd, &rc);
                const int margin = 30;
                if (pt.x < -margin || pt.y < -margin || pt.x > rc.right + margin || pt.y > rc.bottom + margin)
                {
                    ImGui::EndDragDropSource();
                    s_dragOutArmed = true;
                    s_dragOutFromIdx = (int)itemIdx;
                    s_dragOutLnkPath = app.sLnkPath;
                    ImGui::PopID();
                    return;
                }
            }

            ImGui::EndDragDropSource();
        }

        // 2. 接收目標 (Target)
        if (ImGui::BeginDragDropTarget())
        {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("APP_GRID_ITEM"))
            {
                moveFrom = *(const int*)payload->Data;
                moveTo = (int)itemIdx;
            }
            ImGui::EndDragDropTarget();
        }

        // 3. 若處於編輯態，在圖示下方直接渲染 InputText
        if (g.bShowText && isEditing)
        {
            ImGui::PushFont(theApp.m_vFont[2]);
            ImGui::SetNextItemWidth(g.iconSize.x + padding * 2.0f);

            if (s_justStartedEdit)
            {
                ImGui::SetKeyboardFocusHere();
                s_justStartedEdit = false;
            }

            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
            bool enterPressed = ImGui::InputText("##rename_input", s_renameBuf, sizeof(s_renameBuf), flags);
            bool escPressed = ImGui::IsKeyPressed(ImGuiKey_Escape);
            bool deactivated = ImGui::IsItemDeactivated();

            if (enterPressed || (deactivated && !escPressed))
            {
                if (s_renameBuf != '\0')
                {
                    app.sDisplay = s_renameBuf;
                }
                s_editingIdx = -1;
            }
            else if (escPressed)
            {
                s_editingIdx = -1;
            }
            ImGui::PopFont();
        }

        ImGui::PopID();
    }




    // ============================================================
    // 文字塊：懸停時底色同按鈕懸停色；長文本懸停時跑馬燈滾動
    // ============================================================
    // ============================================================
    // 🚀 雙擊文字塊改名：把新名稱寫回磁碟（自動保留 "NNNN-" 排序前綴）
    //    同名覆蓋圖標（.png/.ico）跟著一起改名，並同步更新記憶體
    // ============================================================
    void FolderUI::ApplyItemRename(ST_APP& app, const char* newNameUtf8)
    {
        if (!newNameUtf8 || newNameUtf8[0] == '\0') return; // 空：視為取消（含 ESC 清空）
        if (app.sLnkPath.empty()) return;

        // UTF-8 → 寬字元
        int wlen = MultiByteToWideChar(CP_UTF8, 0, newNameUtf8, -1, NULL, 0);
        if (wlen <= 1) return;
        std::wstring newName((size_t)wlen - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, newNameUtf8, -1, &newName[0], wlen);

        // 解析舊路徑：dir / 舊主檔名（含前綴）/ 副檔名
        size_t slash = app.sLnkPath.find_last_of(L"\\/");
        if (slash == std::wstring::npos) return;
        std::wstring dir = app.sLnkPath.substr(0, slash);
        std::wstring fileName = app.sLnkPath.substr(slash + 1);
        size_t dot = fileName.find_last_of(L'.');
        if (dot == std::wstring::npos || dot == 0) return;
        std::wstring oldStem = fileName.substr(0, dot); // 如 "0003-微信"
        std::wstring ext = fileName.substr(dot);        // 如 ".lnk" / ".url"

        // 新主檔名 = 舊排序前綴 + 新名稱（前綴保持原順序；無前綴則不加）
        std::wstring stripped = StripOrderPrefix(oldStem);
        std::wstring newStem = (stripped == oldStem)
            ? newName
            : (oldStem.substr(0, oldStem.size() - stripped.size()) + newName);

        std::wstring newPath = dir + L"\\" + newStem + ext;
        if (_wcsicmp(newPath.c_str(), app.sLnkPath.c_str()) == 0) return; // 沒變

        // 目標已存在（與其他項目撞名）→ 放棄，避免覆蓋
        if (::GetFileAttributesW(newPath.c_str()) != INVALID_FILE_ATTRIBUTES) return;

        if (!::MoveFileExW(app.sLnkPath.c_str(), newPath.c_str(), 0)) return;

        // 同名覆蓋圖標跟著改名（存在才改），否則改名後圖標會失聯
        const wchar_t* iconExts[] = { L".png", L".ico" };
        for (const wchar_t* e : iconExts)
        {
            std::wstring oldIcon = dir + L"\\" + oldStem + e;
            std::wstring newIcon = dir + L"\\" + newStem + e;
            DWORD attr = ::GetFileAttributesW(oldIcon.c_str());
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                ::MoveFileExW(oldIcon.c_str(), newIcon.c_str(), 0);
            }
        }

        // 同步記憶體（不重載：圖標 SRV 不受影響，命中矩形每幀重建）
        app.sLnkPath = newPath;
        app.sDisplay = newNameUtf8;
    }
    void FolderUI::DrawTextBlock(ST_APP& app, size_t itemIdx, const GridLayout& g, float fRound)
    {
        // 懸停滾動狀態（同一時間只有一個項被懸停，單份狀態即可）
        // s_hoverIdx: 當前懸停的項目索引；s_hoverStartTime: 該項目開始懸停的時刻
        static int s_hoverIdx = -1;
        static double s_hoverStartTime = 0.0;

        // 🚀 雙擊改名：編輯狀態（同一時間只有一個項在編輯，單份狀態即可）
        static bool s_editing = false;
        static int  s_editIdx = -1;
        static char s_editBuf[128];
        static int  s_editFrames = 0;   // 進入編輯後的幀數（跳過首幀的失焦判定）

        ImDrawList* drawList = ImGui::GetWindowDrawList();

        ImVec2 btnMin = ImGui::GetItemRectMin();
        ImVec2 btnMax = ImGui::GetItemRectMax();

        // ===== 懸停檢測：按鈕本體 + 下方文字塊區域 =====
        bool hovered = ImGui::IsItemHovered() ||
            ImGui::IsMouseHoveringRect(btnMin, ImVec2(btnMax.x, btnMax.y + g.textGap + g.textBlockHeight));
        if (hovered)
        {
            // 剛開始懸停此項：記錄起始時刻（滾動延遲從此刻算起）
            if (s_hoverIdx != (int)itemIdx)
            {
                s_hoverIdx = (int)itemIdx;
                s_hoverStartTime = ImGui::GetTime();
            }
        }
        else if (s_hoverIdx == (int)itemIdx)
        {
            // 滑鼠離開：重置，下次懸停重新從頭計時
            s_hoverIdx = -1;
        }
        bool isHoveredNow = (s_hoverIdx == (int)itemIdx);

        // ===== 懸停時文字塊底色與按鈕懸停色保持一致 =====
        ImVec2 textBlockMin = ImVec2(btnMin.x, btnMax.y + g.textGap);
        ImVec2 textBlockMax = ImVec2(btnMax.x, textBlockMin.y + g.textBlockHeight);

        ImU32 bgColor = ImGui::GetColorU32(isHoveredNow ? ImGuiCol_ButtonHovered : ImGuiCol_FrameBg);
       // drawList->AddRectFilled(textBlockMin, textBlockMax, bgColor, fRound);

        const float textPaddingX = 1.0f;
        float maxTextWidth = g.btnSize.x - (textPaddingX * 2.0f);
        // 文字垂直置中於文字塊
        float textY = textBlockMin.y;// +(g.textBlockHeight - ImGui::GetTextLineHeight()) * 0.5f;
        ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

        // ===== 🚀 雙擊文字塊進入編輯 =====
        if (!s_editing && ImGui::IsMouseHoveringRect(textBlockMin, textBlockMax) &&
            ImGui::IsMouseDoubleClicked(0))
        {
            s_editing = true;
            s_editIdx = (int)itemIdx;
            strncpy_s(s_editBuf, app.sDisplay.c_str(), _TRUNCATE); // 編輯起點 = 目前顯示名（無前綴）
            s_editFrames = 0;
        }

        // ===== 🚀 編輯中：在文字塊位置渲染輸入框，其餘項目照常 =====
        if (s_editing && s_editIdx == (int)itemIdx)
        {
            bool apply = false;

            ImGui::SetCursorScreenPos(textBlockMin);
            ImGui::SetNextItemWidth(g.btnSize.x);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 2.0f));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            if (s_editFrames == 0)
            {
                ImGui::SetKeyboardFocusHere(0); // 首幀搶焦點
            }
            ImGuiInputTextFlags flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
            if (ImGui::InputText("##rename_item", s_editBuf, sizeof(s_editBuf), flags))
            {
                apply = true; // 回車
            }
            ImGui::PopStyleColor(1);
            ImGui::PopStyleVar(1);

            // 失焦（點擊其他地方）→ 應用。跳過首幀：焦點尚未就緒會誤判失焦。
            // ESC 會清空緩衝區，空名稱在 ApplyItemRename 中視為取消。
            if (!apply && s_editFrames > 0 && ImGui::IsItemDeactivated())
            {
                apply = true;
            }

            if (apply)
            {
                s_editing = false;
                s_editIdx = -1;
                ApplyItemRename(app, s_editBuf);
            }
            else
            {
                s_editFrames++;
            }
            return; // 編輯中：不繪製普通文字（底色已繪製）
        }

        ImVec2 textSize = ImGui::CalcTextSize(app.sDisplay.c_str());

        if (textSize.x <= maxTextWidth)
        {
            // 短文本：直接水平居中完整顯示
            float textRenderX = textBlockMin.x + (g.btnSize.x - textSize.x) * 0.5f;
            drawList->AddText(ImVec2(textRenderX, textY), textColor, app.sDisplay.c_str());
            return;
        }

        if (!isHoveredNow)
        {
            // 長文本 + 未懸停：截斷加 "..."
            std::string finalName = TruncateUtf8WithEllipsis(app.sDisplay, maxTextWidth);
            drawList->AddText(ImVec2(textBlockMin.x + textPaddingX + 2, textY), textColor, finalName.c_str());
            return;
        }

        // ===== 🚀 長文本 + 懸停：跑馬燈滾動顯示完整名稱 =====
        const float holdTime = 0.6f;    // 頭尾停留時長（秒）
        const float scrollSpeed = 30.0f; // 滾動速度（像素/秒）

        float maxScroll = textSize.x - maxTextWidth;
        float scrollDur = maxScroll / scrollSpeed;
        float cycle = holdTime * 2.0f + scrollDur;

        // 從懸停開始計時，取模循環：頭部停留 → 滾動 → 尾部停留 → 從頭再來
        float t = (float)(ImGui::GetTime() - s_hoverStartTime);
        t -= ImFloor(t / cycle) * cycle;

        float scrollOffset;
        if (t < holdTime)
        {
            scrollOffset = 0.0f;                          // 頭部停留：顯示開頭
        }
        else if (t < holdTime + scrollDur)
        {
            scrollOffset = (t - holdTime) * scrollSpeed;  // 勻速滾動
        }
        else
        {
            scrollOffset = maxScroll;                    // 尾部停留：顯示結尾
        }

        // 裁剪在文字塊內繪製（offset 增大 = 文字向左移動 = 逐漸顯示尾部）
        drawList->PushClipRect(textBlockMin, textBlockMax, true);
        drawList->AddText(ImVec2(textBlockMin.x + textPaddingX - scrollOffset, textY), textColor, app.sDisplay.c_str());
        drawList->PopClipRect();

    }

    // ============================================================
    // 懸停 Tooltip：顯示啟動目標路徑
    // ============================================================
    void FolderUI::DrawItemTooltip(const ST_APP& app, const GridLayout& g)
    {
        float hoverHeightOffset = g.bShowText ? (g.textGap + g.textBlockHeight) : 0.0f;
        ImVec2 maxHoverPos = ImVec2(ImGui::GetItemRectMax().x, ImGui::GetItemRectMax().y + hoverHeightOffset);

        if (ImGui::IsItemHovered() || ImGui::IsMouseHoveringRect(ImGui::GetItemRectMin(), maxHoverPos))
        {
            if (IsPath(app.sPath))
            {
                ImGui::SetTooltip(app.sPath.c_str());
            }
        }
    }
// ============================================================
// 主渲染流程：換行排版 -> 逐項繪製（按鈕 / 文字塊 / Tooltip）-> 拖曳套用
// ============================================================
void FolderUI::Render(HWND hwnd, FolderUIData& fud)
{
    if (fud.m_pIdx == nullptr)
    {
        return;
    }

    int idx = *fud.m_pIdx;
    if (idx < 0 || idx >= fud.gvFolder.size())
    {
        return;
    }
    fud.gpvApp = &(fud.gvFolder[idx].vApp);

    // 💡 根據當前的分頁索引命名子視窗，例如 "AppGridRegion_0", "AppGridRegion_1"
    char childWindowId[64];
    sprintf_s(childWindowId, "AppGridRegion_%d", idx);

    // 🚀 自動隱藏滾動條：僅在「滾動後短時間內」或「滑鼠懸停在滾動條上」時顯示，
    //    其餘時間 0.25 秒淡出至完全透明。顯示狀態在子視窗內部更新，下一幀生效。
    static float s_sbIdle = 99.0f;   // 距離上次滾動經過的秒數
    static bool  s_sbHover = false;  // 滑鼠上一幀是否懸停在滾動條區域
    float sbAlpha = 1.0f;
    if (!s_sbHover && s_sbIdle > 1.5f)
    {
        sbAlpha = 1.0f - (s_sbIdle - 1.5f) * 4.0f; // 停止滾動 1.5s 後開始，0.25s 內淡出
        if (sbAlpha < 0.0f) sbAlpha = 0.0f;
    }
    ImVec4 cBg     = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarBg);          cBg.w *= sbAlpha;
    ImVec4 cGrab   = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);        cGrab.w *= sbAlpha;
    ImVec4 cGrabH  = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrabHovered); cGrabH.w *= sbAlpha;
    ImVec4 cGrabA  = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrabActive);  cGrabA.w *= sbAlpha;
    ImGui::PushStyleColor(ImGuiCol_ScrollbarBg, cBg);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrab, cGrab);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabHovered, cGrabH);
    ImGui::PushStyleColor(ImGuiCol_ScrollbarGrabActive, cGrabA);

    ImGuiWindowFlags win_flags = 0;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    if (ImGui::BeginChild(childWindowId, ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, win_flags))
    {
        // 🚀 更新滾動條顯示狀態（供下一幀 sbAlpha 使用）
        {
            static float s_lastScrollY = -1.0f;
            float curScrollY = ImGui::GetScrollY();
            if (curScrollY != s_lastScrollY)
            {
                s_sbIdle = 0.0f;          // 滾動位置變化 → 重新點亮計時
                s_lastScrollY = curScrollY;
            }
            s_sbIdle += ImGui::GetIO().DeltaTime;

            // 滑鼠是否懸停在滾動條區域（子視窗右緣 ScrollbarSize 寬的縱向條帶）
            s_sbHover = false;
            if (ImGui::GetScrollMaxY() > 0.0f)
            {
                ImVec2 wPos = ImGui::GetWindowPos();
                ImVec2 wSize = ImGui::GetWindowSize();
                ImVec2 m = ImGui::GetIO().MousePos;
                s_sbHover = (m.x >= wPos.x + wSize.x - ImGui::GetStyle().ScrollbarSize) &&
                            (m.x <= wPos.x + wSize.x) &&
                            (m.y >= wPos.y) && (m.y <= wPos.y + wSize.y);
            }
        }

        GridLayout g = CalcGridLayout(fud.m_bShowName, fud.gpvApp->size());

        float maxCalculatedY = g.startPos.y; // 用於追蹤最後一行涵蓋的最大高度位置
        int moveFrom = -1;
        int moveTo = -1;
        char szName[64];

        // 每幀重建項目命中矩形（Explorer 拖入 .ico/.png 時的落點判定用）
        m_dropRects.clear();
        m_hoverLnkPath.clear();

        for (size_t i = 0; i < fud.gpvApp->size(); ++i)
        {
            auto& a = (*fud.gpvApp)[i];

            sprintf_s(szName, "btn%d", (int)i);

            int col = (int)(i % g.maxItemsPerRow);
            int row = (int)(i / g.maxItemsPerRow);

            // 計算當前單元的絕對 X 與 Y 座標
            // X 用視窗座標（dynamicPadding 已是按鈕塊左緣的視窗座標，絕對居中）
            float targetX = g.dynamicPadding + (col * (g.btnSize.x + g.itemSpacingX * 2));
            float targetY = g.startPos.y + (row * (g.totalCellHeight + g.itemSpacingY * 2));

            // 更新最後一行涵蓋的最大高度
            if (targetY + g.totalCellHeight > maxCalculatedY) {
                maxCalculatedY = targetY + g.totalCellHeight;
            }

            // 設定排版起點
            ImGui::SetCursorPosX(targetX);
            ImGui::SetCursorPosY(targetY);

            float fPadding = (g.btnSize.x - g.iconSize.x) / 2;
            RenderAppButton(hwnd, a, szName, i, g, fPadding, moveFrom, moveTo);

            // 🚀 記錄該項目的格子矩形（按鈕+文字塊，與懸停判定範圍一致；客戶區絕對座標）
            {
                ItemRect r;
                ImVec2 bMin = ImGui::GetItemRectMin();
                ImVec2 bMax = ImGui::GetItemRectMax();
                r.MinX = bMin.x;
                r.MinY = bMin.y;
                r.MaxX = bMax.x;
                r.MaxY = bMax.y + (g.bShowText ? (g.textGap + g.textBlockHeight) : 0.0f);
                r.sLnkPath = a.sLnkPath;
                m_dropRects.push_back(r);
            }

            // 🚀 記錄當前懸停的項目（拖放命中兜底：Explorer 拖檔懸停時 hover 正常工作）
            if (ImGui::IsItemHovered())
            {
                m_hoverLnkPath = a.sLnkPath;
            }

            
            if (m_bShowTip)
            {
                DrawItemTooltip(a, g);
            }
        }

        if (moveFrom != -1 && moveTo != -1)
        {
            MoveAppElement(*fud.gpvApp, moveFrom, moveTo);
        }

        // ===== 🚀 拖出狀態機：鬆開滑鼠後判定「移除」或「移回排序」 =====
        if (s_dragOutArmed)
        {
            if (::GetAsyncKeyState(VK_ESCAPE) & 0x8000)
            {
                s_dragOutArmed = false; // ESC：取消，不做任何事
            }
            else if (!(::GetAsyncKeyState(VK_LBUTTON) & 0x8000)) // 左鍵已鬆開
            {
                s_dragOutArmed = false;

                POINT pt;
                ::GetCursorPos(&pt);
                ::ScreenToClient(hwnd, &pt);
                RECT rc;
                ::GetClientRect(hwnd, &rc);

                if (pt.x >= 0 && pt.y >= 0 && pt.x <= rc.right && pt.y <= rc.bottom)
                {
                    // ===== 拖回視窗內鬆開：視為普通排序，移到鬆開點所在格子 =====
                    if (s_dragOutFromIdx >= 0 && s_dragOutFromIdx < (int)fud.gpvApp->size())
                    {
                        // 子視窗絕對座標 = 客戶區座標（主視口 Pos 為 0,0）
                        ImVec2 winPos = ImGui::GetWindowPos();
                        float lx = (float)pt.x - winPos.x;
                        float ly = (float)pt.y - winPos.y;

                        float baseX = g.dynamicPadding;
                        float stepX = g.btnSize.x + g.itemSpacingX;
                        float stepY = g.totalCellHeight + g.itemSpacingY;

                        // 四捨五入到最近的格子（負值一律夾到 0）
                        int col = (int)((lx - baseX) / stepX + 0.5f);
                        int row = (int)((ly - g.startPos.y) / stepY + 0.5f);
                        if (col < 0) col = 0;
                        if (row < 0) row = 0;
                        if (col > g.maxItemsPerRow - 1) col = g.maxItemsPerRow - 1;

                        int to = row * g.maxItemsPerRow + col;
                        if (to > (int)fud.gpvApp->size() - 1) to = (int)fud.gpvApp->size() - 1;

                        MoveAppElement(*fud.gpvApp, s_dragOutFromIdx, to);
                    }
                }
                else
                {
                    // ===== 在視窗外鬆開：直接刪除該捷徑（不拖放到任何目錄）=====
                    if (!s_dragOutLnkPath.empty())
                    {
                        AppRemoveDragOutItem(s_dragOutLnkPath);
                    }
                }

                s_dragOutFromIdx = -1;
                s_dragOutLnkPath.clear();
            }
        }

        // 我們必須主動將光標移到最後一行底部，並放置一個 Dummy 空白元件，
        // 來告訴 Child 視窗「內容到底了」，進而安全地觸發內部滾動條。
        ImGui::SetCursorPosY(maxCalculatedY);
        ImGui::Dummy(ImVec2(0.0f, 1.0f));
    }
    ImGui::PopStyleVar();

    ImGui::EndChild(); // 💡 結束子視窗
    ImGui::PopStyleColor(4); // 🚀 彈出自動隱藏滾動條的透明色
}

// ============================================================
// 🚀 Explorer 圖標拖入：把 .ico/.png 套用到「客戶區座標 pt 命中的項目」上
//    拷貝到該項目 lnk/url 的同目錄，並改名為同名覆蓋圖標（已存在則覆蓋）
// ============================================================
bool FolderUI::ApplyIconToItem(POINT screenPt, const std::wstring& srcIconPath)
{
    // ===== 命中 1（最可靠）：最近一幀懸停的項目 =====
    std::wstring targetLnk = m_hoverLnkPath;

    // ===== 命中 2：放置點落入哪個項目的格子矩形 =====
    if (targetLnk.empty())
    {
        for (const auto& r : m_dropRects)
        {
            // 🎯 這裡的 r.Min/Max 來自 ImGui 的螢幕絕對座標，現在與傳入的 screenPt 完美匹配！
            if ((float)screenPt.x >= r.MinX && (float)screenPt.y >= r.MinY &&
                (float)screenPt.x <= r.MaxX && (float)screenPt.y <= r.MaxY)
            {
                targetLnk = r.sLnkPath;
                break;
            }
        }
    }

    // ===== 命中 3（兜底）：距離最近的格子 =====
    if (targetLnk.empty() && !m_dropRects.empty())
    {
        const ItemRect* best = nullptr;
        float bestDist = 1e9f;
        for (const auto& r : m_dropRects)
        {
            float cx = (r.MinX + r.MaxX) * 0.5f;
            float cy = (r.MinY + r.MaxY) * 0.5f;
            float dx = (float)screenPt.x - cx;
            float dy = (float)screenPt.y - cy;
            float d = dx * dx + dy * dy;
            if (d < bestDist)
            {
                bestDist = d;
                best = &r;
            }
        }
        if (best && bestDist <= 60.0f * 60.0f)
        {
            targetLnk = best->sLnkPath;
        }
    }

    if (targetLnk.empty())
    {
        return false;
    }

    size_t slash = targetLnk.find_last_of(L"\\/");
    size_t dot = targetLnk.find_last_of(L'.');
    if (slash == std::wstring::npos || dot == std::wstring::npos || dot < slash)
    {
        return false;
    }
    std::wstring stem = targetLnk.substr(slash + 1, dot - slash - 1);
    std::wstring dir = targetLnk.substr(0, slash);

    size_t sdot = srcIconPath.find_last_of(L'.');
    if (sdot == std::wstring::npos || sdot + 4 > srcIconPath.size())
    {
        return false;
    }
    std::wstring iconExt = srcIconPath.substr(sdot);

    const wchar_t* otherExt = (_wcsicmp(iconExt.c_str(), L".ico") == 0) ? L".png" : L".ico";
    std::wstring otherIcon = dir + L"\\" + stem + otherExt;
    DWORD otherAttr = ::GetFileAttributesW(otherIcon.c_str());
    if (otherAttr != INVALID_FILE_ATTRIBUTES && !(otherAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ::DeleteFileW(otherIcon.c_str());
    }

    std::wstring dstPath = dir + L"\\" + stem + iconExt;
    return ::CopyFileW(srcIconPath.c_str(), dstPath.c_str(), FALSE) ? true : false;
}

bool FolderUI::ImageTextButton(
    const char* str_id,
    ImTextureID user_texture_id,
    const ImVec2& icon_size,
    int padding,
    bool show_text,
    const char* label,
    ImFont* text_font,
    float text_gap,
    bool* out_text_double_clicked)
{
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    const ImGuiStyle& style = ImGui::GetStyle();
    const float btnWidth = icon_size.x + padding * 2.0f;
    const float btnHeight = icon_size.y + padding * 2.0f;

    // 1. 計算文字高度（僅在開啟文字且非編輯態時納入高度）
    float textHeight = 0.0f;
    ImVec2 textSize(0.0f, 0.0f);
    if (show_text && label != nullptr)
    {
        if (text_font) ImGui::PushFont(text_font);
        // 限制文字最大寬度與按鈕同寬，避免過長撐開佈局
        textSize = ImGui::CalcTextSize(label, nullptr, false, btnWidth);
        textHeight = text_gap + ImGui::GetTextLineHeight(); // 或是使用 textSize.y 支援多行
        if (text_font) ImGui::PopFont();
    }

    // 2. 計算整體透明按鈕的尺寸：關閉文字時與原 ImageButton 完全相同
    ImVec2 totalSize(btnWidth, btnHeight + textHeight);
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // 3. 提交唯一的 InvisibleButton 統一掌管排版佔位、Hover、Active、點擊與拖曳
    bool pressed = ImGui::InvisibleButton(str_id, totalSize);
    bool hovered = ImGui::IsItemHovered();
    bool active = ImGui::IsItemActive();

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // 4. 繪製上方圖示按鈕的背景框（Hover 整個區域時都會高亮）
    ImVec2 btnMax(pos.x + btnWidth, pos.y + btnHeight);
    ImU32 bgCol = ImGui::GetColorU32(
        active ? ImGuiCol_ButtonActive :
        hovered ? ImGuiCol_ButtonHovered :
        ImGuiCol_Button
    );
    drawList->AddRectFilled(pos, btnMax, bgCol, (float)(padding * 2));

    // 5. 繪製應用程式圖示
    ImVec2 imgMin(pos.x + padding, pos.y + padding);
    ImVec2 imgMax(imgMin.x + icon_size.x, imgMin.y + icon_size.y);
    drawList->AddImage(user_texture_id, imgMin, imgMax);

    // 6. 繪製下方文字與偵測文字區雙擊
    if (out_text_double_clicked) *out_text_double_clicked = false;

    bool isMouseInText = false;
    if (show_text && label != nullptr)
    {
        if (text_font) ImGui::PushFont(text_font);

        ImVec2 textAreaMin(pos.x, btnMax.y + text_gap);
        ImVec2 textAreaMax(pos.x + btnWidth, pos.y + totalSize.y);

        // 水平置中計算（若文字超過按鈕寬度則靠左並裁切）
        float drawX = pos.x + (btnWidth - textSize.x) * 0.5f;
        if (drawX < pos.x) drawX = pos.x;

        ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
        // 使用 ClipRect 防止超長檔名溢出到隔壁格子
        drawList->PushClipRect(textAreaMin, textAreaMax, true);
        drawList->AddText(ImVec2(drawX, textAreaMin.y), textCol, label);
        drawList->PopClipRect();

        isMouseInText = ImGui::IsMouseHoveringRect(textAreaMin, textAreaMax);
        // 偵測滑鼠是否在「下方文字區」雙擊左鍵
        if (out_text_double_clicked && hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        {
            if (isMouseInText)
            {
                *out_text_double_clicked = true;
            }
        }

        if (text_font) ImGui::PopFont();
    }

    // 關鍵修正：只有點擊「圖示按鈕區域」才回傳 pressed = true；
// 點擊或雙擊「下方文字區域」不會觸發啟動程式！
    return pressed && !isMouseInText;
}
