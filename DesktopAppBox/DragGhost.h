#pragma once
#include <windows.h>
#include <d3d11.h>
#include <string>
#include "imgui.h"

class ImGuiDragGhost
{
public:
    HWND m_hGhostWnd = nullptr;
    bool m_isVisible = false;

    void Init(HINSTANCE hInst)
    {
        if (m_hGhostWnd) return;
        WNDCLASSEXW wc = { sizeof(wc), 0, ::DefWindowProcW, 0, 0, hInst, nullptr, nullptr, nullptr, nullptr, L"AppBoxImGuiGhost", nullptr };
        ::RegisterClassExW(&wc);

        m_hGhostWnd = ::CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            wc.lpszClassName, L"", WS_POPUP,
            0, 0, 10, 10,
            nullptr, nullptr, hInst, nullptr
        );
    }

    // 直接使用主程式的 D3D11 裝置、ImGui 樣式、字體與圖示 SRV，渲染出與視窗內一模一樣的拖曳框
    void ShowFromImGui(ID3D11Device* device, ID3D11DeviceContext* ctx,
        ImTextureID iconSrv, const std::string& labelUtf8, bool isOutsideWarning = true)
    {
        if (!device || !ctx) return;
        if (!m_hGhostWnd) Init(::GetModuleHandleW(nullptr));

        // 1. 根據當前 ImGui 樣式與字體，精準計算與視窗內 Tooltip 完全一致的尺寸
        const ImGuiStyle& style = ImGui::GetStyle();
        const float iconSize = 24.0f;
        const float padX = style.WindowPadding.x;
        const float padY = style.WindowPadding.y;
        const float spacingX = style.ItemSpacing.x;

        std::string displayStr = (isOutsideWarning ? "移除: " : "移動: ") + labelUtf8;

        ImVec2 textSize = ImGui::CalcTextSize(displayStr.c_str());

        int w = (int)ceilf(padX * 2.0f + iconSize + spacingX + textSize.x) + 4;
        int h = (int)ceilf(padY * 2.0f + max(iconSize, textSize.y)) + 4;
        if (w < 60) w = 60;
        if (h < 32) h = 32;

        // 2. 建立獨立的 ImDrawList 繪製完全相同的圓角框、圖示與文字
        ImDrawList drawList(ImGui::GetDrawListSharedData());
        drawList._ResetForNewFrame();
        drawList.PushTextureID(ImGui::GetIO().Fonts->TexID);
        drawList.PushClipRect(ImVec2(0, 0), ImVec2((float)w, (float)h), false);

        // 背景色與邊框色（與 ImGui Tooltip 樣式同步，拖出視窗外時可微調為醒目暗紅邊框）
        ImU32 bgCol = isOutsideWarning ? IM_COL32(45, 20, 20, 235) : ImGui::GetColorU32(ImGuiCol_PopupBg);
        ImU32 borderCol = isOutsideWarning ? IM_COL32(220, 70, 70, 255) : ImGui::GetColorU32(ImGuiCol_Border);
        float rounding = style.PopupRounding > 0.0f ? style.PopupRounding : 6.0f;

        drawList.AddRectFilled(ImVec2(1, 1), ImVec2((float)w - 1, (float)h - 1), bgCol, rounding);
        drawList.AddRect(ImVec2(1, 1), ImVec2((float)w - 1, (float)h - 1), borderCol, rounding, 0, 1.2f);

        // 繪製完全相同的圖示紋理 (app.iconSrv)
        if (iconSrv)
        {
            ImVec2 imgMin(padX + 1.0f, ((float)h - iconSize) * 0.5f);
            ImVec2 imgMax(imgMin.x + iconSize, imgMin.y + iconSize);
            drawList.AddImage(iconSrv, imgMin, imgMax);
        }

        // 繪製完全相同的文字與字體
        ImVec2 textPos(padX + 1.0f + iconSize + spacingX, ((float)h - textSize.y) * 0.5f);
        drawList.AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), displayStr.c_str());

        drawList.PopClipRect();
        drawList.PopTextureID();

        // 3. 將此獨立 ImDrawList 離屏渲染到一張透明的 DX11 紋理並讀取像素
        RenderDrawListToLayeredWindow(device, ctx, &drawList, w, h);
    }

    void UpdatePosition()
    {
        if (!m_isVisible || !m_hGhostWnd) return;
        POINT pt;
        ::GetCursorPos(&pt);
        // 偏移量與 ImGui 預設 Tooltip 位置完全一致 (+16, +12)
        ::SetWindowPos(m_hGhostWnd, HWND_TOPMOST, pt.x + 16, pt.y + 12, 0, 0,
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOREDRAW);
    }

    void Hide()
    {
        if (m_isVisible && m_hGhostWnd)
        {
            ::ShowWindow(m_hGhostWnd, SW_HIDE);
            m_isVisible = false;
        }
    }

private:
    void RenderDrawListToLayeredWindow(ID3D11Device* device, ID3D11DeviceContext* ctx, ImDrawList* drawList, int w, int h)
    {
        // 建立離屏 RenderTarget 與可讀取的 Staging 紋理
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 直接匹配 Windows GDI LayeredWindow 像素格式
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;

        ID3D11Texture2D* pRTTexture = nullptr;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &pRTTexture))) return;

        ID3D11RenderTargetView* pRTV = nullptr;
        device->CreateRenderTargetView(pRTTexture, nullptr, &pRTV);

        // 備份主視窗原本的 RenderTarget 與 Viewport
        ID3D11RenderTargetView* pOldRTV = nullptr;
        ID3D11DepthStencilView* pOldDSV = nullptr;
        ctx->OMGetRenderTargets(1, &pOldRTV, &pOldDSV);
        D3D11_VIEWPORT oldVP;
        UINT numVP = 1;
        ctx->RSGetViewports(&numVP, &oldVP);

        // 清空離屏紋理為全透明並使用 ImGui DX11 後端渲染該小卡片
        const float clearCol[] = {0.0f, 0.0f, 0.0f, 0.0f};
        ctx->OMSetRenderTargets(1, &pRTV, nullptr);
        ctx->ClearRenderTargetView(pRTV, clearCol);

        ImDrawData fakeDrawData;
        fakeDrawData.Valid = true;
        fakeDrawData.CmdLists.push_back(drawList);
        fakeDrawData.CmdListsCount = 1;
        fakeDrawData.TotalIdxCount = drawList->IdxBuffer.Size;
        fakeDrawData.TotalVtxCount = drawList->VtxBuffer.Size;
        fakeDrawData.DisplayPos = ImVec2(0.0f, 0.0f);
        fakeDrawData.DisplaySize = ImVec2((float)w, (float)h);
        fakeDrawData.FramebufferScale = ImVec2(1.0f, 1.0f);

        // 呼叫 ImGui 原生 DX11 渲染器畫出這張小卡片
        extern void ImGui_ImplDX11_RenderDrawData(ImDrawData * draw_data);
        ImGui_ImplDX11_RenderDrawData(&fakeDrawData);

        // 還原主視窗的 RenderTarget 與 Viewport
        ctx->OMSetRenderTargets(1, &pOldRTV, pOldDSV);
        if (pOldRTV) pOldRTV->Release();
        if (pOldDSV) pOldDSV->Release();
        if (numVP > 0) ctx->RSSetViewports(1, &oldVP);

        // 拷貝到 CPU 可讀的 Staging Texture
        desc.Usage = D3D11_USAGE_STAGING;
        desc.BindFlags = 0;
        desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        ID3D11Texture2D* pStagingTex = nullptr;
        if (SUCCEEDED(device->CreateTexture2D(&desc, nullptr, &pStagingTex)))
        {
            ctx->CopyResource(pStagingTex, pRTTexture);
            D3D11_MAPPED_SUBRESOURCE mapped = {};
            if (SUCCEEDED(ctx->Map(pStagingTex, 0, D3D11_MAP_READ, 0, &mapped)))
            {
                // 建立 GDI DIBSection 並轉換為預乘 Alpha 送給 UpdateLayeredWindow
                HDC screenDC = ::GetDC(nullptr);
                HDC memDC = ::CreateCompatibleDC(screenDC);
                BITMAPINFO bmi = {};
                bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
                bmi.bmiHeader.biWidth = w;
                bmi.bmiHeader.biHeight = -h;
                bmi.bmiHeader.biPlanes = 1;
                bmi.bmiHeader.biBitCount = 32;
                bmi.bmiHeader.biCompression = BI_RGB;

                void* pBits = nullptr;
                HBITMAP hBmp = ::CreateDIBSection(memDC, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
                HBITMAP hOldBmp = (HBITMAP)::SelectObject(memDC, hBmp);

                for (int y = 0; y < h; ++y)
                {
                    unsigned int* srcRow = (unsigned int*)((unsigned char*)mapped.pData + y * mapped.RowPitch);
                    unsigned int* dstRow = (unsigned int*)pBits + y * w;
                    for (int x = 0; x < w; ++x)
                    {
                        unsigned int px = srcRow[x];
                        unsigned int a = (px >> 24) & 0xFF;
                        unsigned int r = ((px >> 16) & 0xFF) * a / 255;
                        unsigned int g = ((px >> 8) & 0xFF) * a / 255;
                        unsigned int b = (px & 0xFF) * a / 255;
                        dstRow[x] = (a << 24) | (r << 16) | (g << 8) | b;
                    }
                }

                POINT ptCursor;
                ::GetCursorPos(&ptCursor);
                POINT ptDst = { ptCursor.x + 16, ptCursor.y + 12 };
                SIZE szWnd = { w, h };
                POINT ptSrc = { 0, 0 };
                BLENDFUNCTION bf = { AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };

                ::UpdateLayeredWindow(m_hGhostWnd, screenDC, &ptDst, &szWnd, memDC, &ptSrc, 0, &bf, ULW_ALPHA);
                ::SetWindowPos(m_hGhostWnd, HWND_TOPMOST, ptDst.x, ptDst.y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                m_isVisible = true;

                ::SelectObject(memDC, hOldBmp);
                ::DeleteObject(hBmp);
                ::DeleteDC(memDC);
                ::ReleaseDC(nullptr, screenDC);
                ctx->Unmap(pStagingTex, 0);
            }
            pStagingTex->Release();
        }

        if (pRTV) pRTV->Release();
        if (pRTTexture) pRTTexture->Release();
    }
};

