#pragma once

#include <windows.h>
#include <d3d11.h>
#include <string>
#include <vector>
#include "imgui.h"

class FontPreview
{
public:
    ID3D11ShaderResourceView* pTexture = nullptr;
    ImVec2 texSize = ImVec2(0, 0);

    ~FontPreview() { Release(); } // 析構時自動釋放顯存

    void Release();

    // 支援傳入字體名稱或完整路徑，並支援粗體/斜體
    bool CreatePreview(ID3D11Device* device, const wchar_t* fontNameOrPath, int fontSizePx,
        const wchar_t* previewText, bool isBold = false, bool isItalic = false);

    // 便利重載：直接接受 UTF-8 字串（無需在 UI 層手動轉換 wstring）
    bool CreatePreviewUTF8(ID3D11Device* device, const std::string& fontNameOrPathUtf8, int fontSizePx,
        const std::string& previewTextUtf8, bool isBold = false, bool isItalic = false);

    bool CreatePreviewW(ID3D11Device* device, const std::string& fontNameOrPathUtf8, int fontSizePx,
        const wchar_t* previewText, bool isBold = false, bool isItalic = false);
};

