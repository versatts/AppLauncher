#include "CustomUI.h"

bool CustomUI::IconTextButton(const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled)
{
#if 0
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
    const float spacing = labelSize.x > 0.0f ? style.ItemInnerSpacing.x : 0.0f;
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
#endif
    return IconTextButtonCore(label, btnSize, icon, iconSize, false, false, bEnabled);
}

bool CustomUI::IconTextToggleButton(bool* v, const char* label, ImTextureID icon, const ImVec2& btnSize, const ImVec2& iconSize, bool bEnabled)
{
    assert(v);
    const bool pressed = IconTextButtonCore(label, btnSize, icon, iconSize, true, *v, bEnabled);
    if (pressed)
        *v = !(*v);
    return pressed;
}

bool CustomUI::IconTextButtonCore(const char* label, const ImVec2& btnSize, ImTextureID icon, const ImVec2& iconSize, bool isToggledBtn, bool bToggled, bool bEnabled)
{
    assert(label);
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    if (!bEnabled)
        ImGui::BeginDisabled(true);

    const ImGuiStyle& style = ImGui::GetStyle();
    const float padX = style.FramePadding.x;
    const float padY = style.FramePadding.y;

    const char* textEnd = ImGui::FindRenderedTextEnd(label);
    ImVec2 labelSize = ImGui::CalcTextSize(label, textEnd);
    const bool hasLabel = (labelSize.x > 0.0f);
    if (hasLabel)
        labelSize.x += 2.0f;

    // 1. 計算內容寬度與按鈕尺寸
    const float spacing = (hasLabel && iconSize.x > 0.0f) ? style.ItemInnerSpacing.x : 0.0f;
    float contentWidth = iconSize.x + spacing + labelSize.x;
    if (btnSize.x > 0.0f)
        contentWidth = ImMin(contentWidth, ImMax(0.0f, btnSize.x - padX * 2.0f));

    ImVec2 size = btnSize;
    if (size.x <= 0.0f)
        size.x = contentWidth + padX * 2.0f;
    if (size.y <= 0.0f)
        size.y = ImMax(iconSize.y, labelSize.y) + padY * 2.0f;

    // 2. 處理互動狀態與佈局註冊
    const ImVec2 pos = window->DC.CursorPos;
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    // 分配 UI 空間，並計算排版游標
    ImGui::ItemSize(size);

    // 取得唯一的元件 ID，並加入到視窗裁剪判定中
    const ImGuiID id = window->GetID(label);
    if (!ImGui::ItemAdd(bb, id))
    {
        if (!bEnabled) ImGui::EndDisabled();
        return false;
    }

    // 啟用此元件的導覽圖層，允許鍵盤選取
    window->DC.NavLayersActiveMaskNext |= (1 << window->DC.NavLayerCurrent);

    // 同時處理滑鼠與鍵盤按壓事件的核心函數
    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

    // 3. 計算背景、文字與圖示顏色（不操作全域 Style Stack）
    const bool isDimmed = (isToggledBtn && !bToggled);
    const float contentAlpha = isDimmed ? 0.7f : 1.0f;

    ImVec4 textVec = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    textVec.w *= contentAlpha;
    const ImU32 textCol = ImGui::GetColorU32(textVec);
    const ImU32 iconTint = ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, contentAlpha));

    const ImU32 bgCol = ImGui::GetColorU32(
        held ? ImGuiCol_ButtonActive : (
        hovered ? ImGuiCol_ButtonHovered :
        ((isToggledBtn && bToggled)) ? ImGuiCol_ButtonActive : ImGuiCol_Button)
    );

    // 4. 繪製背景與邊框
    ImDrawList* drawList = window->DrawList;
    const ImVec2 rectMax(pos.x + size.x, pos.y + size.y);
    float fRounding = style.FrameRounding;

    drawList->AddRectFilled(pos, rectMax, bgCol, fRounding);

    if (style.FrameBorderSize > 0.0f)
        drawList->AddRect(pos, rectMax, ImGui::GetColorU32(ImGuiCol_Border), fRounding, 0, style.FrameBorderSize);

    // 【新增這段】當玩家用鍵盤 Tab 切換到此按鈕時，畫出選取高亮框
    if (ImGui::GetFocusID() == id)
    {
        ImGui::RenderNavHighlight(bb, id);
    }

    if (isToggledBtn && bToggled)
    {
        // 方案 A：使用 ImGui 預設的高亮藍色（推薦，會跟隨主題風格變換且支援 Disabled 透明度）
        const ImU32 lineCol = ImGui::GetColorU32(ImGuiCol_NavHighlight);

        // 方案 B：如果您想要強制指定特定藍色（例如純藍），請解開下方註解並替換 lineCol：
        // const ImU32 lineCol = ImGui::GetColorU32(ImVec4(0.0f, 0.47f, 0.83f, style.Alpha)); // 考慮了 Disabled 的 Alpha

        const float lineWidth = size.y * 0.07f;

        // 計算線段的起點與終點（位於按鈕的最底端）
        // 為了避免線段超出圓角，兩端各往內縮 1 像素（可依視覺需求調整）
        ImVec2 lineStart(pos.x, pos.y + size.y - lineWidth);
        ImVec2 lineEnd(pos.x + size.x, pos.y + size.y);

        drawList->AddRectFilled(lineStart, lineEnd, lineCol, fRounding, ImDrawFlags_RoundCornersBottom);
    }


    // 5. 計算置中起點並繪製圖示與文字
    float currentX = pos.x + (size.x - contentWidth) * 0.5f;

    if (icon != 0 && iconSize.x > 0.0f && iconSize.y > 0.0f)
    {
        const float iconY = pos.y + (size.y - iconSize.y) * 0.5f;
        drawList->AddImage(icon, ImVec2(currentX, iconY), ImVec2(currentX + iconSize.x, iconY + iconSize.y), ImVec2(0, 0), ImVec2(1, 1), iconTint);
    }
    currentX += iconSize.x + spacing;

    if (hasLabel)
    {
        const float textY = pos.y + (size.y - labelSize.y) * 0.5f;
        const ImVec4 clipRect(pos.x + padX, pos.y, rectMax.x - padX, rectMax.y);
        drawList->AddText(
            ImGui::GetFont(),
            ImGui::GetFontSize(),
            ImVec2(currentX, textY),
            textCol,
            label,
            textEnd,
            0.0f,
            &clipRect
        );
    }

    if (!bEnabled)
        ImGui::EndDisabled();

    return pressed;
}

