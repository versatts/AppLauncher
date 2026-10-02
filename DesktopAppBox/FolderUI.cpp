#include "FolderUI.h"
#include "FolderUIData.h"

// 來自 main.cpp：檔案拖入/拖出功能總開關
extern bool g_bEnableFileDragIO;
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

namespace
{
    // ============================================================
    // 網格排版參數與幾何計算結果
    // ============================================================
    struct GridLayout
    {
        // 基礎參數
        ImVec2 iconSize        = ImVec2(64.0f, 64.0f);
        float  itemSpacingX    = 5.0f;   // 圖標之間固定的橫向間距
        float  itemSpacingY    = 10.0f;  // 圖標單元之間的縱向間距
        float  textBlockHeight = 20.0f;  // 文字塊的固定高度
        float  textGap         = 4.0f;   // 圖標與下方文字塊之間的微小間距
        float  minPadding      = 10.0f;  // 左右最小 Padding
        float  topPadding      = 5.0f;   // 上邊沿固定的 Padding

        // 計算結果
        bool   bShowText       = true;
        float  buttonWidth     = 0.0f;
        float  buttonHeight    = 0.0f;
        float  totalCellHeight = 0.0f;   // 單個單元（含文字塊）總高
        float  dynamicPadding  = 0.0f;   // 水平置中 Padding
        int    maxItemsPerRow  = 1;
        ImVec2 startPos        = ImVec2(0.0f, 0.0f); // 內容繪製起始位置（已含 topPadding）
    };

    // 依當前子視窗寬度與項目數量，計算換行與置中所需的全部幾何
    GridLayout CalcGridLayout(bool bShowText, size_t itemCount)
    {
        ImGuiStyle& style = ImGui::GetStyle();

        GridLayout g;
        g.bShowText = bShowText;
        g.buttonWidth  = g.iconSize.x + style.FramePadding.x * 2.0f;
        g.buttonHeight = g.iconSize.y + style.FramePadding.y * 2.0f;
        g.totalCellHeight = g.buttonHeight;
        if (bShowText)
        {
            g.totalCellHeight = g.buttonHeight + g.textGap + g.textBlockHeight;
        }

        // 💡 取得當前「子視窗」內部真正的可用總寬度，防止計算換行時跑版
        float windowWidth = ImGui::GetContentRegionAvail().x;

        g.maxItemsPerRow = (int)((windowWidth - (g.minPadding * 2.0f) + g.itemSpacingX) / (g.buttonWidth + g.itemSpacingX));
        if (g.maxItemsPerRow < 1) g.maxItemsPerRow = 1;

        // 首行項目數（用於計算整體水平置中）
        int itemsInFirstRow = (int)itemCount;
        if (itemsInFirstRow > g.maxItemsPerRow) {
            itemsInFirstRow = g.maxItemsPerRow;
        }

        float totalItemsWidth = (itemsInFirstRow * g.buttonWidth) + ((itemsInFirstRow - 1) * g.itemSpacingX);
        if (itemsInFirstRow <= 1) {
            totalItemsWidth = g.buttonWidth;
        }

        g.dynamicPadding = (windowWidth - totalItemsWidth) / 2.0f;
        if (g.dynamicPadding < g.minPadding) g.dynamicPadding = g.minPadding;

        // 💡 先套用頂部間距，避免內容黏在子視窗最上緣
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + g.topPadding);
        g.startPos = ImGui::GetCursorPos();
        return g;
    }

    // UTF-8 安全截斷：超出 maxWidth 時保留完整多位元組字元並補 "..."
    std::string TruncateUtf8WithEllipsis(const std::string& text, float maxWidth)
    {
        const std::string ellipsis = "...";
        float ellipsisWidth = ImGui::CalcTextSize(ellipsis.c_str()).x;
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
        return result + ellipsis;
    }

    // ============================================================
    // 🚀 拖出狀態機：拖著項目離開視窗外後進入「待確認移除」狀態
    //    - 在視窗外鬆開滑鼠 → 直接刪除 lnk 檔案（不拖放到任何目錄）
    //    - 拖回視窗內鬆開 → 視為普通排序，移動到鬆開點所在格子
    //    - 按 ESC → 取消
    // ============================================================
    bool s_dragOutArmed = false;
    int  s_dragOutFromIdx = -1;
    std::wstring s_dragOutLnkPath;

    // ============================================================
    // 單個 App 項目：圖標按鈕 + 點擊啟動 + 拖曳排序
    // ============================================================
    void RenderAppButton(HWND hwnd, const ST_APP& app, const char* btnId, size_t itemIdx,
                         ImVec2 iconSize, int& moveFrom, int& moveTo)
    {
        if (ImGui::ImageButton(btnId, (ImTextureID)app.iconSrv, iconSize))
        {
            ShellExecuteW(hwnd, L"open", app.exePathBuf, nullptr, nullptr, SW_SHOW);
        }

        // 1. 拖曳來源 (Source)
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID))
        {
            int currentIdx = (int)itemIdx;
            ImGui::SetDragDropPayload("APP_GRID_ITEM", &currentIdx, sizeof(int));

            // 拖曳時跟隨滑鼠顯示的小浮動視窗內容
            ImGui::Text("移動: %s", app.sDisplay.c_str());

            // 🚀 拖出檢測：拖著項目離開視窗客戶區一定距離後，進入「待確認移除」狀態
            //    （窗外鬆開 = 刪除 lnk；拖回窗內鬆開 = 移位；ESC = 取消）
            //    注意：不能用 io.MousePos 判斷——滑鼠離開客戶區後視窗收不到 WM_MOUSEMOVE，
            //    io.MousePos 會停在邊緣不再更新，必須主動查詢全域游標位置。
            if (g_bEnableFileDragIO && !app.sLnkPath.empty())
            {
                POINT pt;
                ::GetCursorPos(&pt);
                ::ScreenToClient(hwnd, &pt);
                RECT rc;
                ::GetClientRect(hwnd, &rc);
                const int margin = 30; // 超出邊緣多少像素才觸發，避免貼著邊緣拖動排序時誤觸
                if (pt.x < -margin || pt.y < -margin || pt.x > rc.right + margin || pt.y > rc.bottom + margin)
                {
                    // 結束 ImGui 內部拖曳，改由 Render 尾部的拖出狀態機接管
                    ImGui::EndDragDropSource();
                    s_dragOutArmed = true;
                    s_dragOutFromIdx = (int)itemIdx;
                    s_dragOutLnkPath = app.sLnkPath;
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
    }

    // ============================================================
    // 文字塊：懸停時底色同按鈕懸停色；長文本懸停時跑馬燈滾動
    // ============================================================
    void DrawTextBlock(const std::string& displayName, size_t itemIdx, const GridLayout& g)
    {
        // 懸停滾動狀態（同一時間只有一個項被懸停，單份狀態即可）
        // s_hoverIdx: 當前懸停的項目索引；s_hoverStartTime: 該項目開始懸停的時刻
        static int s_hoverIdx = -1;
        static double s_hoverStartTime = 0.0;

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
        drawList->AddRectFilled(textBlockMin, textBlockMax, bgColor, 4.0f);

        const float textPaddingX = 4.0f;
        float maxTextWidth = g.buttonWidth - (textPaddingX * 2.0f);
        // 文字垂直置中於文字塊
        float textY = textBlockMin.y + (g.textBlockHeight - ImGui::GetTextLineHeight()) * 0.5f;
        ImU32 textColor = ImGui::GetColorU32(ImGuiCol_Text);

        ImVec2 textSize = ImGui::CalcTextSize(displayName.c_str());

        if (textSize.x <= maxTextWidth)
        {
            // 短文本：直接水平居中完整顯示
            float textRenderX = textBlockMin.x + (g.buttonWidth - textSize.x) * 0.5f;
            drawList->AddText(ImVec2(textRenderX, textY), textColor, displayName.c_str());
            return;
        }

        if (!isHoveredNow)
        {
            // 長文本 + 未懸停：截斷加 "..."
            std::string finalName = TruncateUtf8WithEllipsis(displayName, maxTextWidth);
            drawList->AddText(ImVec2(textBlockMin.x + textPaddingX, textY), textColor, finalName.c_str());
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
        drawList->AddText(ImVec2(textBlockMin.x + textPaddingX - scrollOffset, textY), textColor, displayName.c_str());
        drawList->PopClipRect();
    }

    // ============================================================
    // 懸停 Tooltip：顯示啟動目標路徑
    // ============================================================
    void DrawItemTooltip(const ST_APP& app, const GridLayout& g)
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
} // namespace

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

    if (ImGui::BeginChild(childWindowId, ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, 0))
    {
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
            float targetX = g.startPos.x + g.dynamicPadding + (col * (g.buttonWidth + g.itemSpacingX));
            float targetY = g.startPos.y + (row * (g.totalCellHeight + g.itemSpacingY));

            // 更新最後一行涵蓋的最大高度
            if (targetY + g.totalCellHeight > maxCalculatedY) {
                maxCalculatedY = targetY + g.totalCellHeight;
            }

            // 設定排版起點
            ImGui::SetCursorPosX(targetX);
            ImGui::SetCursorPosY(targetY);

            RenderAppButton(hwnd, a, szName, i, g.iconSize, moveFrom, moveTo);

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

            if (g.bShowText)
            {
                DrawTextBlock(a.sDisplay, i, g);
            }

            DrawItemTooltip(a, g);
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

                        float baseX = g.startPos.x + g.dynamicPadding;
                        float stepX = g.buttonWidth + g.itemSpacingX;
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
    ImGui::EndChild(); // 💡 結束子視窗
}

// ============================================================
// 🚀 Explorer 圖標拖入：把 .ico/.png 套用到「客戶區座標 pt 命中的項目」上
//    拷貝到該項目 lnk/url 的同目錄，並改名為同名覆蓋圖標（已存在則覆蓋）
// ============================================================

bool FolderUI::ApplyIconToItem(POINT clientPt, const std::wstring& srcIconPath)
{
    // ===== 命中 1（最可靠）：最近一幀懸停的項目 =====
    // 拖放鬆開前滑鼠必停在目標按鈕上，上一幀的 hover 即為使用者瞄準的項目；
    // 此判定完全在 ImGui 座標體系內，不受客戶區座標換算影響
    std::wstring targetLnk = m_hoverLnkPath;

    // ===== 命中 2：放置點落入哪個項目的格子矩形 =====
    if (targetLnk.empty())
    {
        for (const auto& r : m_dropRects)
        {
            if ((float)clientPt.x >= r.MinX && (float)clientPt.y >= r.MinY &&
                (float)clientPt.x <= r.MaxX && (float)clientPt.y <= r.MaxY)
            {
                targetLnk = r.sLnkPath;
                break;
            }
        }
    }

    // ===== 命中 3（兜底）：距離最近的格子 =====
    // 容忍格子間隙/座標小偏差：取中心距離最近者，但須在 60px 半徑內，
    // 避免拖到完全空白處時誤掛到別的項目
    if (targetLnk.empty() && !m_dropRects.empty())
    {
        const ItemRect* best = nullptr;
        float bestDist = 1e9f;
        for (const auto& r : m_dropRects)
        {
            float cx = (r.MinX + r.MaxX) * 0.5f;
            float cy = (r.MinY + r.MaxY) * 0.5f;
            float dx = (float)clientPt.x - cx;
            float dy = (float)clientPt.y - cy;
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
        return false; // 沒有命中任何項目
    }

    // 命中：解析該項目 lnk/url 的所在目錄與主檔名（含排序前綴，與磁碟一致）
    size_t slash = targetLnk.find_last_of(L"\\/");
    size_t dot = targetLnk.find_last_of(L'.');
    if (slash == std::wstring::npos || dot == std::wstring::npos || dot < slash)
    {
        return false;
    }
    std::wstring stem = targetLnk.substr(slash + 1, dot - slash - 1);
    std::wstring dir = targetLnk.substr(0, slash);

    // 源圖標的副檔名（.ico / .png），過短視為無效
    size_t sdot = srcIconPath.find_last_of(L'.');
    if (sdot == std::wstring::npos || sdot + 4 > srcIconPath.size())
    {
        return false;
    }
    std::wstring iconExt = srcIconPath.substr(sdot);

    // 🚀 保證「立即生效」：CheckSpecificIcon 的優先級是 .png > .ico，
    //    若項目已有另一格式的同名圖標（如拖 .ico 但已存在 .png），
    //    新拖的會被舊的搶先 → 這裡把互斥的另一格式刪掉，確保新圖標必定顯示
    const wchar_t* otherExt = (_wcsicmp(iconExt.c_str(), L".ico") == 0) ? L".png" : L".ico";
    std::wstring otherIcon = dir + L"\\" + stem + otherExt;
    DWORD otherAttr = ::GetFileAttributesW(otherIcon.c_str());
    if (otherAttr != INVALID_FILE_ATTRIBUTES && !(otherAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        ::DeleteFileW(otherIcon.c_str());
    }

    // 目標 = 同目錄 + 與 lnk/url 同主檔名的覆蓋圖標（同名存在則覆蓋）
    std::wstring dstPath = dir + L"\\" + stem + iconExt;
    return ::CopyFileW(srcIconPath.c_str(), dstPath.c_str(), FALSE) ? true : false;
}
