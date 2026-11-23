#include "FontPreview.h"
#include <dwrite.h>
#include <cmath>

#pragma comment(lib, "dwrite.lib")

static std::wstring Utf8ToWide(const std::string& str)
{
    if (str.empty()) return L"";
    int len = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    if (len <= 0) return L"";
    std::wstring res(len - 1, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &res[0], len); // 必須是 &res[0]
    return res;
}
void FontPreview::Release()
{
    if (pTexture)
    {
        pTexture->Release();
        pTexture = nullptr;
    }
    texSize = ImVec2(0, 0);
}

bool FontPreview::CreatePreviewW(ID3D11Device* device, const std::string& fontNameOrPathUtf8, int fontSizePx,
    const wchar_t* previewText, bool isBold, bool isItalic)
{
    std::wstring wFont = Utf8ToWide(fontNameOrPathUtf8);

    if (wFont.size() > 4 && wFont.find(L'\\') == std::wstring::npos && wFont.find(L'/') == std::wstring::npos)
    {
        const wchar_t* ext = wFont.c_str() + wFont.size() - 4;
        if (_wcsicmp(ext, L".ttf") == 0 ||
            _wcsicmp(ext, L".ttc") == 0 ||
            _wcsicmp(ext, L".otf") == 0)
        {
            wFont = L"C:\\Windows\\Fonts\\" + wFont;
        }
    }

    return CreatePreview(device, wFont.c_str(), fontSizePx, previewText, isBold, isItalic);
}

bool FontPreview::CreatePreviewUTF8(ID3D11Device* device, const std::string& fontNameOrPathUtf8, int fontSizePx,
    const std::string& previewTextUtf8, bool isBold, bool isItalic)
{
    std::wstring wFont = Utf8ToWide(fontNameOrPathUtf8);
    std::wstring wText = Utf8ToWide(previewTextUtf8);

    // 若傳入的是純字體檔名（如 "SIMKAI.TTF" 或 "simkai.ttf"），自動補上 Windows 預設字體資料夾路徑
    if (wFont.size() > 4 && wFont.find(L'\\') == std::wstring::npos && wFont.find(L'/') == std::wstring::npos)
    {
        const wchar_t* ext = wFont.c_str() + wFont.size() - 4;
        if (_wcsicmp(ext, L".ttf") == 0 ||
            _wcsicmp(ext, L".ttc") == 0 ||
            _wcsicmp(ext, L".otf") == 0)
        {
            wFont = L"C:\\Windows\\Fonts\\" + wFont;
        }
    }

    return CreatePreview(device, wFont.c_str(), fontSizePx, wText.c_str(), isBold, isItalic);
}

bool FontPreview::CreatePreview(ID3D11Device* device, const wchar_t* fontNameOrPath, int fontSizePx,
    const wchar_t* previewText, bool isBold, bool isItalic)
{
    Release();
    if (!device || !fontNameOrPath || !previewText || *previewText == L'\0')
        return false;

    IDWriteFactory* pDWriteFactory = nullptr;
    if (FAILED(::DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(&pDWriteFactory))))
        return false;

    IDWriteFontFace* pFontFace = nullptr;
    bool isFilePath = (wcschr(fontNameOrPath, L'\\') != nullptr || wcschr(fontNameOrPath, L'/') != nullptr);

    // 情況 A：傳入的是實體字體檔案路徑 (.ttf / .ttc / .otf)
    if (isFilePath)
    {
        IDWriteFontFile* pFontFile = nullptr;
        if (SUCCEEDED(pDWriteFactory->CreateFontFileReference(fontNameOrPath, nullptr, &pFontFile)))
        {
            BOOL isSupported = FALSE;
            DWRITE_FONT_FILE_TYPE fileType;
            DWRITE_FONT_FACE_TYPE faceType;
            UINT32 numFaces = 0;

            if (SUCCEEDED(pFontFile->Analyze(&isSupported, &fileType, &faceType, &numFaces)) && isSupported)
            {
                DWRITE_FONT_SIMULATIONS sim = DWRITE_FONT_SIMULATIONS_NONE;
                if (isBold) sim |= DWRITE_FONT_SIMULATIONS_BOLD;
                if (isItalic) sim |= DWRITE_FONT_SIMULATIONS_OBLIQUE;

                pDWriteFactory->CreateFontFace(faceType, 1, &pFontFile, 0, sim, &pFontFace);
            }
            pFontFile->Release();
        }
    }
    // 情況 B：傳入的是系統已安裝的字體名稱 (例如 L"Microsoft YaHei" / L"Consolas")
    else
    {
        IDWriteFontCollection* pSysCollection = nullptr;
        if (SUCCEEDED(pDWriteFactory->GetSystemFontCollection(&pSysCollection)))
        {
            UINT32 index = 0;
            BOOL exists = FALSE;
            if (SUCCEEDED(pSysCollection->FindFamilyName(fontNameOrPath, &index, &exists)) && exists)
            {
                IDWriteFontFamily* pFamily = nullptr;
                if (SUCCEEDED(pSysCollection->GetFontFamily(index, &pFamily)))
                {
                    IDWriteFont* pFont = nullptr;
                    DWRITE_FONT_WEIGHT weight = isBold ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
                    DWRITE_FONT_STYLE style = isItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

                    if (SUCCEEDED(pFamily->GetFirstMatchingFont(weight, DWRITE_FONT_STRETCH_NORMAL, style, &pFont)))
                    {
                        pFont->CreateFontFace(&pFontFace);
                        pFont->Release();
                    }
                    pFamily->Release();
                }
            }
            pSysCollection->Release();
        }
    }

    if (!pFontFace)
    {
        pDWriteFactory->Release();
        return false;
    }

    // 開始透過 DirectWrite 軟體渲染目標繪製字形
    IDWriteGdiInterop* pGdiInterop = nullptr;
    if (SUCCEEDED(pDWriteFactory->GetGdiInterop(&pGdiInterop)))
    {
        DWRITE_FONT_METRICS fontMetrics;
        pFontFace->GetMetrics(&fontMetrics);
        float scale = (float)fontSizePx / (float)fontMetrics.designUnitsPerEm;
        float ascent = fontMetrics.ascent * scale;
        float descent = fontMetrics.descent * scale;

        // 支援 UTF-16 代理對 (Surrogate Pairs，如生僻字或 Emoji) 的字碼點轉換
        std::vector<UINT32> codePoints;
        for (const wchar_t* p = previewText; *p != L'\0'; ++p)
        {
            if (*p >= 0xD800 && *p <= 0xDBFF && *(p + 1) >= 0xDC00 && *(p + 1) <= 0xDFFF)
            {
                UINT32 high = *p - 0xD800;
                UINT32 low = *(p + 1) - 0xDC00;
                codePoints.push_back(0x10000 + ((high << 10) | low));
                ++p;
            }
            else
            {
                codePoints.push_back(*p);
            }
        }

        size_t glyphCount = codePoints.size();
        if (glyphCount > 0)
        {
            std::vector<UINT16> glyphIndices(glyphCount);
            pFontFace->GetGlyphIndices(codePoints.data(), (UINT32)glyphCount, glyphIndices.data());

            std::vector<DWRITE_GLYPH_METRICS> glyphMetrics(glyphCount);
            pFontFace->GetDesignGlyphMetrics(glyphIndices.data(), (UINT32)glyphCount, glyphMetrics.data(), FALSE);

            std::vector<FLOAT> glyphAdvances(glyphCount);
            float totalWidth = 0.0f;
            for (size_t i = 0; i < glyphCount; ++i)
            {
                glyphAdvances[i] = glyphMetrics[i].advanceWidth * scale;
                totalWidth += glyphAdvances[i];
            }

            int outW = (int)std::ceil(totalWidth) + 8;
            int outH = (int)std::ceil(ascent + descent) + 8;

            if (outW > 8 && outH > 8)
            {
                IDWriteBitmapRenderTarget* pRenderTarget = nullptr;
                if (SUCCEEDED(pGdiInterop->CreateBitmapRenderTarget(nullptr, outW, outH, &pRenderTarget)))
                {
                    pRenderTarget->SetPixelsPerDip(1.0f);
                    HDC memDC = pRenderTarget->GetMemoryDC();

                    RECT rcClear = { 0, 0, outW, outH };
                    ::FillRect(memDC, &rcClear, (HBRUSH)::GetStockObject(BLACK_BRUSH));

                    IDWriteRenderingParams* pRenderingParams = nullptr;
                    pDWriteFactory->CreateCustomRenderingParams(
                        1.8f, 0.0f, 0.0f, DWRITE_PIXEL_GEOMETRY_FLAT,
                        DWRITE_RENDERING_MODE_NATURAL_SYMMETRIC, &pRenderingParams);

                    DWRITE_GLYPH_RUN glyphRun = {};
                    glyphRun.fontFace = pFontFace;
                    glyphRun.fontEmSize = (FLOAT)fontSizePx;
                    glyphRun.glyphCount = (UINT32)glyphCount;
                    glyphRun.glyphIndices = glyphIndices.data();
                    glyphRun.glyphAdvances = glyphAdvances.data();

                    pRenderTarget->DrawGlyphRun(
                        4.0f, 4.0f + ascent,
                        DWRITE_MEASURING_MODE_NATURAL,
                        &glyphRun, pRenderingParams, RGB(255, 255, 255), nullptr);

                    HBITMAP hBmp = (HBITMAP)::GetCurrentObject(memDC, OBJ_BITMAP);
                    BITMAP bmp = {};
                    ::GetObject(hBmp, sizeof(BITMAP), &bmp);

                    if (bmp.bmBits)
                    {
                        std::vector<unsigned int> rgbaPixels(outW * outH);
                        unsigned int* srcPixels = (unsigned int*)bmp.bmBits;
                        for (int i = 0; i < outW * outH; ++i)
                        {
                            unsigned int px = srcPixels[i];
                            unsigned int r = (px >> 16) & 0xFF;
                            unsigned int g = (px >> 8) & 0xFF;
                            unsigned int b = px & 0xFF;
                            unsigned int alpha = (r + g + b) / 3;
                            rgbaPixels[i] = (alpha << 24) | 0x00FFFFFF;
                        }

                        D3D11_TEXTURE2D_DESC desc = {};
                        desc.Width = outW;
                        desc.Height = outH;
                        desc.MipLevels = 1;
                        desc.ArraySize = 1;
                        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                        desc.SampleDesc.Count = 1;
                        desc.Usage = D3D11_USAGE_DEFAULT;
                        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

                        D3D11_SUBRESOURCE_DATA subResource = {};
                        subResource.pSysMem = rgbaPixels.data();
                        subResource.SysMemPitch = outW * 4;

                        ID3D11Texture2D* pTex2D = nullptr;
                        if (SUCCEEDED(device->CreateTexture2D(&desc, &subResource, &pTex2D)))
                        {
                            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
                            srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                            srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                            srvDesc.Texture2D.MipLevels = 1;

                            if (SUCCEEDED(device->CreateShaderResourceView(pTex2D, &srvDesc, &pTexture)))
                            {
                                texSize = ImVec2((float)outW, (float)outH);
                            }
                            pTex2D->Release();
                        }
                    }

                    if (pRenderingParams) pRenderingParams->Release();
                    pRenderTarget->Release();
                }
            }
        }
        pGdiInterop->Release();
    }

    pFontFace->Release();
    pDWriteFactory->Release();
    return (pTexture != nullptr);
}

