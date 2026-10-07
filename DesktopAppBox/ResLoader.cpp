#include "ResLoader.h"
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "user32.lib")


#include <shellapi.h> // 💡 確保有引入 ShellAPI 標頭檔
#include <shlobj.h>
#include <wrl/client.h>
#include <commoncontrols.h>

ResLoader::~ResLoader()
{
    for (auto a : m_vRes)
    {
        a->Release();
    }
}

// ============================================================
// ============================================================
// 🚀 高質量 area-average（box）縮放 32bpp BGRA 像素。
//    預乘 alpha 後加權平均，避免邊緣縮放產生暗暈/雜點。
//    等價於理想的 mip 推導：任意比例縮小都無走樣鋸齒。
// ============================================================
static bool ResampleRGBA32(const DWORD* src, int sw, int sh, DWORD* dst, int dw, int dh)
{
    if (!src || !dst || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return false;
    if (sw == dw && sh == dh) { memcpy(dst, src, (size_t)sw * sh * 4); return true; }

    for (int dy = 0; dy < dh; ++dy)
    {
        double sy0 = (double)dy * sh / dh;
        double sy1 = (double)(dy + 1) * sh / dh;
        int iy0 = (int)sy0;
        int iy1 = (sy1 > (double)(int)sy1) ? ((int)sy1 + 1) : (int)sy1;
        if (iy1 > sh) iy1 = sh;

        for (int dx = 0; dx < dw; ++dx)
        {
            double sx0 = (double)dx * sw / dw;
            double sx1 = (double)(dx + 1) * sw / dw;
            int ix0 = (int)sx0;
            int ix1 = (sx1 > (double)(int)sx1) ? ((int)sx1 + 1) : (int)sx1;
            if (ix1 > sw) ix1 = sw;

            double r = 0, g = 0, b = 0, a = 0, sumW = 0;
            for (int iy = iy0; iy < iy1; ++iy)
            {
                double wy = (sy1 < (double)(iy + 1) ? sy1 : (double)(iy + 1))
                          - (sy0 > (double)iy ? sy0 : (double)iy);
                if (wy <= 0) continue;
                const DWORD* row = src + (size_t)iy * sw;
                for (int ix = ix0; ix < ix1; ++ix)
                {
                    double wx = (sx1 < (double)(ix + 1) ? sx1 : (double)(ix + 1))
                              - (sx0 > (double)ix ? sx0 : (double)ix);
                    if (wx <= 0) continue;
                    double w = wx * wy;
                    DWORD p = row[ix];
                    double pa = (double)((p >> 24) & 0xFF);
                    r += (double)((p >> 16) & 0xFF) * pa * w; // 預乘 alpha
                    g += (double)((p >> 8) & 0xFF) * pa * w;
                    b += (double)(p & 0xFF) * pa * w;
                    a += pa * w;
                    sumW += w;
                }
            }

            DWORD& out = dst[(size_t)dy * dw + dx];
            if (a <= 0.0 || sumW <= 0.0) { out = 0; continue; }
            double da = a / sumW;                 // 平均 alpha
            DWORD dr = (DWORD)(r / a + 0.5);      // 預乘平均後除回 → 直通色
            DWORD dg = (DWORD)(g / a + 0.5);
            DWORD db = (DWORD)(b / a + 0.5);
            if (dr > 255) dr = 255; if (dg > 255) dg = 255; if (db > 255) db = 255;
            if (da > 255.0) da = 255.0;
            out = ((DWORD)da << 24) | (dr << 16) | (dg << 8) | db;
        }
    }
    return true;
}

// ============================================================
// 🚀 解析 .ico 目錄，挑出最適合的「實際存在」尺寸。
//    策略：優先取「≥desired 的最小尺寸」（下採樣品質好於上採樣），
//    若全部小於 desired 則取最大尺寸。返回 0 表示解析失敗。
//    目的：PrivateExtractIconsW 若請求的尺寸不存在，會把最接近的
//    小圖拉伸到請求尺寸（鋸齒來源）；按實際尺寸提取則零縮放。
// ============================================================
static UINT PickBestIcoSize(const wchar_t* icoPath, UINT desired)
{
    HANDLE hFile = ::CreateFileW(icoPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return 0;

    UINT result = 0;
    do
    {
        // ICONDIR: WORD reserved; WORD type(=1); WORD count;
        struct { WORD reserved, type, count; } hdr = {};
        DWORD bytesRead = 0;
        if (!::ReadFile(hFile, &hdr, sizeof(hdr), &bytesRead, nullptr) ||
            bytesRead != sizeof(hdr) || hdr.type != 1 || hdr.count == 0)
        {
            break;
        }

        // ICONDIRENTRY: BYTE bWidth, bHeight(0 代表 256), ...（後 12 位元組尺寸無關）
        struct Entry { BYTE w, h, c, r; WORD planes, bpp; DWORD bytes, offset; };
        UINT bestUp = 0;   // ≥ desired 的最小尺寸（下採樣，品質優先）
        UINT bestDown = 0; // < desired 的最大尺寸（無大圖時的退路）
        for (WORD i = 0; i < hdr.count; i++)
        {
            Entry e = {};
            if (!::ReadFile(hFile, &e, sizeof(e), &bytesRead, nullptr) || bytesRead != sizeof(e))
            {
                break;
            }
            UINT size = (e.w == 0) ? 256u : e.w; // 寬高一致，看寬即可
            if (size >= desired)
            {
                if (bestUp == 0 || size < bestUp) bestUp = size;
            }
            else
            {
                if (size > bestDown) bestDown = size;
            }
        }
        result = (bestUp != 0) ? bestUp : bestDown;
    } while (false);

    ::CloseHandle(hFile);
    return result;
}

// ============================================================
// 🚀 像素 → SRV：縮到目標尺寸（area-average）+ 建紋理 + mipmap。
//    供直讀 ICO 路徑使用，與 IconToD3D11SRV_Simple 的建資源邏輯一致。
// ============================================================
static ID3D11ShaderResourceView* CreateSRVFromPixels(ID3D11Device* pDevice,
    std::vector<DWORD>& pixels, int& outW, int& outH, int targetSize)
{
    if (targetSize > 0 && outW > targetSize && outH > targetSize)
    {
        int nw = targetSize, nh = targetSize;
        if (outW >= outH) nh = (outH * targetSize) / outW;
        else              nw = (outW * targetSize) / outH;
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        std::vector<DWORD> scaled((size_t)nw * nh);
        if (ResampleRGBA32(pixels.data(), outW, outH, scaled.data(), nw, nh))
        {
            pixels = std::move(scaled);
            outW = nw;
            outH = nh;
        }
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = outW;
    desc.Height = outH;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11ShaderResourceView* pSRV = nullptr;
    ID3D11Texture2D* pTex = nullptr;
    if (SUCCEEDED(pDevice->CreateTexture2D(&desc, nullptr, &pTex)))
    {
        ID3D11DeviceContext* pContext = nullptr;
        pDevice->GetImmediateContext(&pContext);
        if (pContext)
        {
            pContext->UpdateSubresource(pTex, 0, nullptr, pixels.data(), outW * 4, 0);
            pDevice->CreateShaderResourceView(pTex, nullptr, &pSRV);
            if (pSRV)
            {
                pContext->GenerateMips(pSRV);
            }
            pContext->Release();
        }
        pTex->Release();
    }
    return pSRV;
}

// ============================================================
// 🚀 直讀 ICO 檔案提取像素：繞開 GDI(HICON) 提取鏈。
//    GDI 鏈的問題：ICO 內低位深（4/8bpp 調色板）條目經系統轉換後
//    色階損失、1bit 透明 mask 硬邊全部保留——即「顏色 bit 數不夠」
//    的觀感來源。直讀則優先挑 PNG / 32bpp 高位深條目：
//      - PNG 條目：WIC 解碼，完整 32bpp RGBA 漸變；
//      - 32bpp DIB：手動展開（bottom-up 翻轉 + alpha 全 0 時套 AND mask）。
//    失敗返回 false，由調用方退回 GDI 舊路徑。
// ============================================================
static bool LoadIcoPixelsDirect(const wchar_t* icoPath, UINT desiredSize,
    std::vector<DWORD>& outPixels, UINT& outW, UINT& outH)
{
    HANDLE hFile = ::CreateFileW(icoPath, GENERIC_READ, FILE_SHARE_READ, nullptr,
        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    bool ok = false;
    do
    {
        struct { WORD reserved, type, count; } hdr = {};
        DWORD rd = 0;
        if (!::ReadFile(hFile, &hdr, sizeof(hdr), &rd, nullptr) || rd != sizeof(hdr) ||
            hdr.type != 1 || hdr.count == 0 || hdr.count > 64)
        {
            break;
        }

        // 收集全部條目信息（尺寸/位深/偏移）
        struct Entry { BYTE w, h, c, r; WORD planes, bpp; DWORD bytes, offset; };
        struct Info { UINT size, bpp; DWORD offset, bytes; };
        std::vector<Info> infos;
        for (WORD i = 0; i < hdr.count; ++i)
        {
            Entry e = {};
            if (!::ReadFile(hFile, &e, sizeof(e), &rd, nullptr) || rd != sizeof(e)) break;
            UINT size = (e.w == 0) ? 256u : e.w;
            infos.push_back({ size, e.bpp, e.offset, e.bytes });
        }
        if (infos.empty()) break;

        // 選條目：pass0 = 高位深(bpp==0 的 PNG 條目或 >=32)且尺寸足夠；
        //         pass1 = 高位深（尺寸不足也優先保位深）；
        //         pass2 = 任意（最大尺寸）。同 pass 內：≥desired 取最小，否則取最大。
        int best = -1;
        for (int pass = 0; pass < 3 && best < 0; ++pass)
        {
            UINT bestSize = 0;
            for (size_t i = 0; i < infos.size(); ++i)
            {
                const Info& f = infos[i];
                bool cond = (pass == 0) ? (f.bpp == 0 || f.bpp >= 32) && f.size >= desiredSize
                          : (pass == 1) ? (f.bpp == 0 || f.bpp >= 32)
                          : true;
                if (!cond) continue;

                if (best < 0) { best = (int)i; bestSize = f.size; continue; }
                bool better;
                if (f.size >= desiredSize) better = (bestSize < desiredSize) || (f.size < bestSize);
                else                       better = (bestSize < desiredSize) && (f.size > bestSize);
                if (better) { best = (int)i; bestSize = f.size; }
            }
        }
        if (best < 0) break;

        // 讀取選中條目的原始數據
        const Info& info = infos[best];
        if (info.bytes < 8) break;
        if (::SetFilePointer(hFile, info.offset, nullptr, FILE_BEGIN) == INVALID_SET_FILE_POINTER) break;
        std::vector<BYTE> data(info.bytes);
        if (!::ReadFile(hFile, data.data(), (DWORD)data.size(), &rd, nullptr) || rd != data.size()) break;

        if (data[0] == 0x89 && data[1] == 0x50 && data[2] == 0x4E && data[3] == 0x47)
        {
            // ===== PNG 條目：WIC 解碼為 32bpp BGRA（完整 alpha 漸變） =====
            IWICImagingFactory* pFactory = nullptr;
            if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&pFactory)))) break;
            IWICStream* pStream = nullptr;
            IWICBitmapDecoder* pDecoder = nullptr;
            IWICBitmapFrameDecode* pFrame = nullptr;
            IWICFormatConverter* pConv = nullptr;
            pFactory->CreateStream(&pStream);
            if (pStream)
            {
                pStream->InitializeFromMemory(data.data(), (DWORD)data.size());
                pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
            }
            if (pDecoder && SUCCEEDED(pDecoder->GetFrame(0, &pFrame)) && pFrame)
            {
                UINT w = 0, h = 0;
                pFrame->GetSize(&w, &h);
                if (w > 0 && h > 0)
                {
                    pFactory->CreateFormatConverter(&pConv);
                    if (pConv && SUCCEEDED(pConv->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA,
                        WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)))
                    {
                        outPixels.resize((size_t)w * h);
                        if (SUCCEEDED(pConv->CopyPixels(NULL, w * 4,
                            (UINT)(outPixels.size() * 4), (BYTE*)outPixels.data())))
                        {
                            outW = w; outH = h; ok = true;
                        }
                    }
                }
                pFrame->Release();
            }
            if (pConv) pConv->Release();
            if (pDecoder) pDecoder->Release();
            if (pStream) pStream->Release();
            pFactory->Release();
        }
        else
        {
            // ===== BMP(DIB) 條目：只支援 32bpp BI_RGB（低位深條目已被選擇階段過濾） =====
            if (data.size() < 40) break;
            BITMAPINFOHEADER* bih = (BITMAPINFOHEADER*)data.data();
            if (bih->biSize < 40 || bih->biSize > 124 || bih->biCompression != BI_RGB) break;
            int w = bih->biWidth;
            int hIcon = bih->biHeight / 2; // ICO 的 DIB 高度是圖標 2 倍（彩色 + AND mask）
            if (w <= 0 || hIcon <= 0) break;
            size_t pxOff = bih->biSize; // 調色板在 32bpp 下不存在
            if (data.size() < pxOff + (size_t)w * hIcon * 4) break;

            const DWORD* src = (const DWORD*)(data.data() + pxOff);
            outPixels.resize((size_t)w * hIcon);
            for (int y = 0; y < hIcon; ++y) // DIB bottom-up → 翻轉為 top-down
            {
                memcpy(&outPixels[(size_t)(hIcon - 1 - y) * w],
                    src + (size_t)y * w, (size_t)w * 4);
            }

            // 很多 32bpp ICO 的 alpha 通道全 0 → 用 AND mask 生成透明度
            bool hasAlpha = false;
            for (size_t i = 0; i < outPixels.size(); ++i)
            {
                if (((outPixels[i] >> 24) & 0xFF) != 0) { hasAlpha = true; break; }
            }
            if (!hasAlpha)
            {
                const BYTE* mask = data.data() + pxOff + (size_t)w * hIcon * 4;
                size_t maskPitch = (((size_t)w + 31) / 32) * 4; // DWORD 對齊
                if (data.size() >= pxOff + (size_t)w * hIcon * 4 + maskPitch * (size_t)hIcon)
                {
                    for (int y = 0; y < hIcon; ++y)
                    {
                        for (int x = 0; x < w; ++x)
                        {
                            size_t my = (size_t)(hIcon - 1 - y); // mask 也是 bottom-up
                            BYTE bit = mask[my * maskPitch + (size_t)x / 8] & (0x80 >> (x % 8));
                            if (!bit) outPixels[(size_t)y * w + x] |= 0xFF000000; // 0 = 不透明
                        }
                    }
                }
            }
            outW = (UINT)w;
            outH = (UINT)hIcon;
            ok = true;
        }
    } while (false);

    ::CloseHandle(hFile);
    return ok;
}

ID3D11ShaderResourceView* ResLoader::LoadHighestResIconSRV(ID3D11Device* pDevice, const wchar_t* exePath, UINT& outW, UINT& outH)
{
    if (!exePath || *exePath == L'\0') return nullptr;

    // =========================================================================
    // 🚀 核心升級：同時支援「本地目錄 (D:\mydir)」與「網路 UNC 路徑 (\\192.168.1.1\mydir)」
    // =========================================================================
    DWORD fileAttr = ::GetFileAttributesW(exePath);

    // 檢查 1：是否為標準本地/網路資料夾
    bool isDirectory = (fileAttr != INVALID_FILE_ATTRIBUTES && (fileAttr & FILE_ATTRIBUTE_DIRECTORY));

    // 檢查 2：是否為網路 UNC 路徑結構 (以 \\ 開頭且非單純檔案)
    // 💡 即使 GetFileAttributesW 因為權限或網路暫時斷開返回失敗，只要是 \\ 開頭我們就判定它是網路目錄
    bool isNetworkPath = (exePath[0] == L'\\' && exePath[1] == L'\\');

    if (isDirectory || isNetworkPath)
    {
        SHFILEINFOW sfi = { 0 };
        DWORD flags = SHGFI_ICON | SHGFI_LARGEICON;

        // 💥 【核心安全防禦】
        // 如果 GetFileAttributesW 失敗（代表網路不通或無權限），
        // 加上 SHGFI_USEFILEATTRIBUTES 可以強迫 Windows 走「虛擬緩衝解析」，
        // 依據 FILE_ATTRIBUTE_DIRECTORY 直接秒回精美的網路資料夾圖標，100% 絕不卡死介面！
        if (fileAttr == INVALID_FILE_ATTRIBUTES)
        {
            flags |= SHGFI_USEFILEATTRIBUTES;
        }

        DWORD_PTR result = ::SHGetFileInfoW(
            exePath,
            FILE_ATTRIBUTE_DIRECTORY, // 配合 SHGFI_USEFILEATTRIBUTES 使用的虛擬屬性
            &sfi,
            sizeof(sfi),
            flags
        );

        if (result != 0 && sfi.hIcon)
        {
            int w = 0, h = 0;
            // 完美對接您已經修復好 Alpha 通道全透明 Bug 的轉換函式
            ID3D11ShaderResourceView* pSRV = IconToD3D11SRV_Simple(pDevice, sfi.hIcon, w, h);

            ::DestroyIcon(sfi.hIcon); // 💡 務必釋放 ShellAPI 產生的 HICON

            outW = (UINT)w;
            outH = (UINT)h;
            return pSRV;
        }

        // 如果是網路路徑但上面沒撈成功，做最後的降級保底處理
        if (isNetworkPath) return nullptr;
    }

    // =========================================================================
    // 💡 原有攔截 ── 判斷是否為純 .ico 檔案 (保持不變)
    // =========================================================================
    size_t pathLen = wcslen(exePath);
    if (pathLen > 4 && _wcsicmp(exePath + pathLen - 4, L".ico") == 0)
    {
        // 🚀 直讀 ICO（首選）：優先挑 PNG / 32bpp 高位深條目，繞開 GDI(HICON)
        //    提取鏈——低位深調色板條目經 GDI 轉換會產生色階與硬邊透明
        //    （「顏色 bit 數不夠」的觀感來源）。直讀 + area-average 縮到 64。
        std::vector<DWORD> icoPx;
        UINT icoW = 0, icoH = 0;
        if (LoadIcoPixelsDirect(exePath, 64, icoPx, icoW, icoH))
        {
            int w = (int)icoW, h = (int)icoH;
            ID3D11ShaderResourceView* pSRV = CreateSRVFromPixels(pDevice, icoPx, w, h, 64);
            outW = (UINT)w;
            outH = (UINT)h;
            return pSRV;
        }

        // 退回 GDI 舊路徑（直讀失敗：非法 ICO / 系統特殊格式）
        // 解析 ICO 目錄，挑最適合提取的「實際存在」尺寸（優先 ≥64 的最小，
        //    下採樣品質好於上採樣），提取後 CPU 端高質量縮到 64（見 targetSize 參數）。
        UINT bestSize = PickBestIcoSize(exePath, 64);
        if (bestSize == 0) bestSize = 64; // 目錄解析失敗：退回直接請求 64

        HICON hIcon = nullptr;
        UINT iconId = 0;
        UINT nExtracted = PrivateExtractIconsW(exePath, 0, bestSize, bestSize, &hIcon, &iconId, 1, LR_DEFAULTCOLOR);
        if (nExtracted == 0 || !hIcon)
        {
            nExtracted = PrivateExtractIconsW(exePath, 0, 0, 0, &hIcon, &iconId, 1, LR_DEFAULTCOLOR);
        }

        if (hIcon)
        {
            int w = 0, h = 0;
            // targetSize=64：提取像素按長邊等比縮到 64，顯示 1:1 無走樣
            ID3D11ShaderResourceView* pSRV = IconToD3D11SRV_Simple(pDevice, hIcon, w, h, 64);
            DestroyIcon(hIcon);
            outW = (UINT)w;
            outH = (UINT)h;
            return pSRV;
        }
        return nullptr;
    }

    // =========================================================================
    // 💡 新增攔截 ── 判斷是否為純 .png 檔案：讀入後走 WIC 解碼（與 exe 內嵌 PNG 圖標同一條路）
    // =========================================================================
    if (pathLen > 4 && _wcsicmp(exePath + pathLen - 4, L".png") == 0)
    {
        HANDLE hFile = ::CreateFileW(exePath, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE)
            return nullptr;

        LARGE_INTEGER fileSize = {};
        ID3D11ShaderResourceView* pSRV = nullptr;
        if (::GetFileSizeEx(hFile, &fileSize) && fileSize.QuadPart > 0 && fileSize.QuadPart <= 64 * 1024 * 1024) // 64MB 防禦
        {
            std::vector<BYTE> pngBlob((size_t)fileSize.QuadPart);
            DWORD bytesRead = 0;
            if (::ReadFile(hFile, pngBlob.data(), (DWORD)pngBlob.size(), &bytesRead, nullptr) && bytesRead == pngBlob.size())
            {
                pSRV = CreateSRVFromPngBlob(pDevice, pngBlob, outW, outH);
            }
        }
        ::CloseHandle(hFile);
        return pSRV;
    }

    // =========================================================================
    // 💡 原有邏輯 ── 處理標準 .exe / .dll 內部資源 (保持不變)
    // =========================================================================
    std::vector<BYTE> iconBlob;
    UINT width = 0, height = 0;

    if (!LoadLargestIconResourceFromExe(exePath, iconBlob, width, height) || iconBlob.empty())
    {
        return nullptr;
    }

    bool isPng = false;
    if (iconBlob.size() > 4)
    {
        if (iconBlob[0] == 0x89 && iconBlob[1] == 0x50 && iconBlob[2] == 0x4E && iconBlob[3] == 0x47)
        {
            isPng = true;
        }
    }

    if (isPng)
    {
        return CreateSRVFromPngBlob(pDevice, iconBlob, outW, outH);
    }
    else
    {
        HICON hIcon = CreateIconFromResourceEx(iconBlob.data(), (DWORD)iconBlob.size(), TRUE, 0x00030000, width, height, LR_DEFAULTCOLOR);
        if (hIcon)
        {
            int w = 0, h = 0;
            ID3D11ShaderResourceView* pSRV = IconToD3D11SRV_Simple(pDevice, hIcon, w, h);
            DestroyIcon(hIcon);
            outW = (UINT)w;
            outH = (UINT)h;
            return pSRV;
        }
    }

    return nullptr;
}


ID3D11ShaderResourceView* ResLoader::CreateSRVFromPngBlob(ID3D11Device* pDevice, const std::vector<BYTE>& pngBlob, UINT& outW, UINT& outH)
{
    IWICImagingFactory* pFactory = nullptr;
    CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (!pFactory) return nullptr;

    IWICStream* pStream = nullptr;
    pFactory->CreateStream(&pStream);
    pStream->InitializeFromMemory(const_cast<BYTE*>(pngBlob.data()), static_cast<DWORD>(pngBlob.size()));

    IWICBitmapDecoder* pDecoder = nullptr;
    pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);

    IWICBitmapFrameDecode* pFrame = nullptr;
    if (!pDecoder || FAILED(pDecoder->GetFrame(0, &pFrame))) {
        // 💡 修正：安全釋放，避免早期退出時洩漏
        if (pDecoder) pDecoder->Release();
        if (pStream) pStream->Release();
        if (pFactory) pFactory->Release();
        return nullptr;
    }

    pFrame->GetSize(&outW, &outH);

    IWICFormatConverter* pConverter = nullptr;
    pFactory->CreateFormatConverter(&pConverter);
    pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);

    std::vector<BYTE> rgbaData(outW * outH * 4);
    pConverter->CopyPixels(NULL, outW * 4, static_cast<UINT>(rgbaData.size()), rgbaData.data());

    // 🚀 高質量縮到顯示尺寸 64：非 2 冪次尺寸的 PNG 不會生成 Mipmap，
    //    大圖直接靠 GPU 縮小顯示會走樣鋸齒 → CPU 端 area-average 縮放。
    //    小圖（≤64）不放大，保持原樣。
    if (outW > 64 || outH > 64)
    {
        int nw = 64, nh = 64;
        if (outW >= outH) nh = (outH * 64) / outW;
        else              nw = (outW * 64) / outH;
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        std::vector<BYTE> scaled((size_t)nw * nh * 4);
        if (ResampleRGBA32(reinterpret_cast<const DWORD*>(rgbaData.data()), (int)outW, (int)outH,
            reinterpret_cast<DWORD*>(scaled.data()), nw, nh))
        {
            rgbaData = std::move(scaled);
            outW = (UINT)nw;
            outH = (UINT)nh;
        }
    }

    ID3D11ShaderResourceView* pSRV = nullptr;

    // 💡 核心防禦：檢查當前顯示卡對此格式的硬體 Mip 支援
    UINT formatSupport = 0;
    pDevice->CheckFormatSupport(DXGI_FORMAT_B8G8R8A8_UNORM, &formatSupport);

    // 判斷硬體是否能在當前格式與尺寸下自動生成 Mip
    bool canAutogenMips = (formatSupport & D3D11_FORMAT_SUPPORT_MIP_AUTOGEN) != 0;

    // 如果寬或高不是 2 的冪次方，且硬體對 NPOT 的自動 Mip 支援不佳，則降級為不生成（避免黑畫面或崩潰）
    // 檢查是否為 2 的冪次方：(x & (x - 1)) == 0
    bool isPOT = ((outW & (outW - 1)) == 0) && ((outH & (outH - 1)) == 0);
    if (!isPOT) {
        canAutogenMips = false; // 非 2 冪次方強制關閉 Autogen 轉為單層
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = outW;
    desc.Height = outH;
    desc.MipLevels = canAutogenMips ? 0 : 1; // 💡 支援才開 0，不支援就只建 1 層
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;

    if (canAutogenMips) {
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }
    else {
        desc.Usage = D3D11_USAGE_IMMUTABLE; // 💡 降級方案：使用不可變紋理防止無效呼叫
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.MiscFlags = 0;
    }

    ID3D11Texture2D* pTex = nullptr;

    if (canAutogenMips) {
        if (SUCCEEDED(pDevice->CreateTexture2D(&desc, nullptr, &pTex))) {
            ID3D11DeviceContext* pContext = nullptr;
            pDevice->GetImmediateContext(&pContext);
            if (pContext) {
                pContext->UpdateSubresource(pTex, 0, nullptr, rgbaData.data(), outW * 4, 0);
                pDevice->CreateShaderResourceView(pTex, nullptr, &pSRV);
                if (pSRV) {
                    pContext->GenerateMips(pSRV);
                }
                pContext->Release();
            }
            pTex->Release();
        }
    }
    else {
        // 💡 降級建立：不支援自動 Mip 時的標準初始資料建立
        D3D11_SUBRESOURCE_DATA initData = {};
        initData.pSysMem = rgbaData.data();
        initData.SysMemPitch = outW * 4;
        if (SUCCEEDED(pDevice->CreateTexture2D(&desc, &initData, &pTex))) {
            pDevice->CreateShaderResourceView(pTex, nullptr, &pSRV);
            pTex->Release();
        }
    }

    pConverter->Release();
    pFrame->Release();
    pDecoder->Release();
    pStream->Release();
    pFactory->Release();

    m_vRes.push_back(pSRV);
    return pSRV;
}


ID3D11ShaderResourceView* ResLoader::IconToD3D11SRV_Simple(ID3D11Device* pDevice, HICON hIcon, int& outW, int& outH, int targetSize)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return nullptr;

    // RAII 守衛：不論後續任何提前 return，都保證釋放 GetIconInfo 產生的兩張 HBITMAP
    struct BmpGuard {
        HBITMAP hbmColor, hbmMask;
        ~BmpGuard() { if (hbmColor) DeleteObject(hbmColor); if (hbmMask) DeleteObject(hbmMask); }
    } guard = { iconInfo.hbmColor, iconInfo.hbmMask };

    // 💡 修正 1：檢查 GetObject 返回值，防範未初始化的 BITMAP 導致隨機寬高
    BITMAP bmColor = {};
    bool hasColor = iconInfo.hbmColor && GetObject(iconInfo.hbmColor, sizeof(bmColor), &bmColor) == (int)sizeof(bmColor);

    // 💡 修正 2：單色圖標沒有 hbmColor，尺寸從 mask 取（mask 高度是彩色圖的 2 倍：XOR + AND）
    BITMAP bmMask = {};
    bool hasMask = iconInfo.hbmMask && GetObject(iconInfo.hbmMask, sizeof(bmMask), &bmMask) == (int)sizeof(bmMask);

    if (!hasColor && !hasMask) return nullptr;

    if (hasColor)
    {
        outW = bmColor.bmWidth;
        outH = bmColor.bmHeight;
    }
    else
    {
        outW = bmMask.bmWidth;
        outH = bmMask.bmHeight / 2;
    }
    if (outW <= 0 || outH <= 0) return nullptr;

    std::vector<DWORD> pixels(outW * outH);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = outW;
    bmi.bmiHeader.biHeight = -outH;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    bool gotPixels = false;
    if (hasColor)
    {
        // 彩色圖標：直接從 hbmColor 提取 32bpp 像素（不選入 DC，符合 GetDIBits 文檔要求）
        HDC hdc = GetDC(NULL);
        gotPixels = (GetDIBits(hdc, iconInfo.hbmColor, 0, outH, pixels.data(), &bmi, DIB_RGB_COLORS) == outH);
        ReleaseDC(NULL, hdc);
    }

    if (!gotPixels)
    {
        // 💡 修正 3：單色圖標（hbmColor == NULL）或 GetDIBits 失敗的降級路徑：
        // 建立 32bpp DIB，用 DrawIconEx 走 GDI 標準繪製（自動套用 AND mask 透明），
        // 避免直接讀取不存在的彩色點陣圖造成越界崩潰
        HDC hdc = GetDC(NULL);
        VOID* pBits = nullptr;
        HBITMAP hDib = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, nullptr, 0);
        if (hDib && pBits)
        {
            memset(pBits, 0, (size_t)outW * outH * 4); // 先清為全透明
            HDC hMemDC = CreateCompatibleDC(hdc);
            HGDIOBJ hOldBmp = SelectObject(hMemDC, hDib);
            DrawIconEx(hMemDC, 0, 0, hIcon, outW, outH, 0, nullptr, DI_NORMAL);
            memcpy(pixels.data(), pBits, (size_t)outW * outH * 4);
            SelectObject(hMemDC, hOldBmp);
            DeleteDC(hMemDC);
            gotPixels = true;
        }
        if (hDib) DeleteObject(hDib);
        ReleaseDC(NULL, hdc);
    }
    if (!gotPixels) return nullptr;

    // ==================== 🚀 核心修復：檢查並修復全透明 Bug ====================
    bool hasAlpha = false;
    for (int i = 0; i < outW * outH; ++i)
    {
        // DWORD 內部格式通常為 0xAARRGGBB，右移 24 位元取得 Alpha
        if (((pixels[i] >> 24) & 0xFF) != 0)
        {
            hasAlpha = true;
            break; // 只要有一個像素帶有透明度，就說明這張圖自帶 Alpha
        }
    }

    // 如果整張圖的 Alpha 全部都是 0（完全透明），則強制將所有像素的 Alpha 設定為 255（不透明）
    if (!hasAlpha)
    {
        for (int i = 0; i < outW * outH; ++i)
        {
            pixels[i] |= 0xFF000000; // 強制將最高 8 位元填滿為 255
        }
    }
    // ========================================================================

    // 🚀 高質量縮到目標尺寸（如 64）：顯示端 1:1 呈現，不依賴 GPU 縮放/走樣。
    //    大圖（256/128）用 area-average 下採樣，邊緣平滑無鋸齒；
    //    小圖（32/48）不放大（targetSize 只下採樣，避免放大發糊）。
    if (targetSize > 0 && outW > targetSize && outH > targetSize)
    {
        int nw = targetSize, nh = targetSize;
        if (outW >= outH) nh = (outH * targetSize) / outW;
        else              nw = (outW * targetSize) / outH;
        if (nw < 1) nw = 1;
        if (nh < 1) nh = 1;
        std::vector<DWORD> scaled((size_t)nw * nh);
        if (ResampleRGBA32(pixels.data(), outW, outH, scaled.data(), nw, nh))
        {
            pixels = std::move(scaled);
            outW = nw;
            outH = nh;
        }
    }

    // 建立 D3D11 資源（包含前述的 Mipmaps 支援）
    ID3D11ShaderResourceView* pSRV = nullptr;
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = outW;
    desc.Height = outH;
    desc.MipLevels = 0;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
    desc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

    ID3D11Texture2D* pTex = nullptr;
    if (SUCCEEDED(pDevice->CreateTexture2D(&desc, nullptr, &pTex)))
    {
        ID3D11DeviceContext* pContext = nullptr;
        pDevice->GetImmediateContext(&pContext);

        if (pContext)
        {
            pContext->UpdateSubresource(pTex, 0, nullptr, pixels.data(), outW * 4, 0);
            pDevice->CreateShaderResourceView(pTex, nullptr, &pSRV);

            if (pSRV)
            {
                pContext->GenerateMips(pSRV);
            }
            pContext->Release();
        }
        pTex->Release();
    }

    m_vRes.push_back(pSRV);
    return pSRV;
}

#pragma pack(push, 1)
typedef struct
{
    WORD idReserved;
    WORD idType;
    WORD idCount;
} ICONGROUPHEADER;

// 這是 RT_GROUP_ICON 在記憶體中真正的結構！
typedef struct
{
    BYTE  bWidth;          // 寬
    BYTE  bHeight;         // 高
    BYTE  bColorCount;     // 顏色數
    BYTE  bReserved;       // 保留
    WORD  wPlanes;         // 色彩平面數
    WORD  wBitCount;       // 每像素位元數
    DWORD dwBytesInRes;    // 【關鍵缺失！】該圖標資源的大小 (4 bytes)
    WORD  wResourceId;     // 真正的資源 ID (2 bytes)
} GRPICONDIRENTRY;         // 修正名稱以符合微軟官方定義
#pragma pack(pop)

typedef GRPICONDIRENTRY* PGRPICONDIRENTRY;
typedef ICONGROUPHEADER* PICONGROUPHEADER;

// 建立一個能同時保存「數字ID」或「字串名稱」的容器
struct IconResourceIdent
{
    WORD id = 0;
    std::wstring name;
    bool isNameString = false;
};

BOOL CALLBACK EnumGroupIconCallback_Universal(HMODULE hMod, LPCWSTR lpszType, LPWSTR lpszName, LONG_PTR lParam)
{
    UNREFERENCED_PARAMETER(hMod);
    UNREFERENCED_PARAMETER(lpszType);

    IconResourceIdent* pOutIdent = (IconResourceIdent*)lParam;

    if (IS_INTRESOURCE(lpszName))
    {
        // 情況 A：是整數 ID
        pOutIdent->id = (WORD)((UINT_PTR)lpszName);
        pOutIdent->isNameString = false;
    }
    else
    {
        // 情況 B：是字串名稱（如 "MAINICON"），將其完整複製下來
        pOutIdent->name = lpszName;
        pOutIdent->isNameString = true;
    }

    return FALSE; // 👈 關鍵：不管是數字還是字串，只要拿到第一個圖標群組就立刻中斷列舉退出！
}

// 從 exePath 讀取 RT_GROUP_ICON，選出尺寸最大圖標，輸出原始資源 blob
// 返回 false 失敗；pBlob 輸出二進位，width/height 輸出圖標尺寸
bool ResLoader::LoadLargestIconResourceFromExe(const wchar_t* exePath, std::vector<BYTE>& pBlob, UINT& width, UINT& height)
{
    if (!exePath) return false;

    // 🎯 核心防禦：偵測是否為「此電腦」、「控制台」等 Shell 虛擬物件路徑
    bool isVirtual = (wcsncmp(exePath, L"::", 2) == 0 || wcsncmp(exePath, L"shell::", 7) == 0);

    if (isVirtual)
    {
        // 1. 將虛擬字串轉回 PIDL 識別碼
        PIDLIST_ABSOLUTE pIDL = nullptr;
        if (FAILED(SHParseDisplayName(exePath, nullptr, &pIDL, 0, nullptr)) || !pIDL)
            return false;

        HICON hIcon = nullptr;
        IImageList* pImgList = nullptr;

        // 取得系統超大圖標列表的介面 (IID_IImageList 在這裡被正確提取)
        if (SUCCEEDED(SHGetImageList(SHIL_JUMBO, IID_IImageList, (void**)&pImgList)))
        {
            SHFILEINFOW sfi = { 0 };

            // 🎯 關鍵修正：必須傳入 SHGFI_PIDL，告訴系統第一個參數傳入的是 pIDL 記憶體指標！
            // 並且同時使用 SHGFI_SYSICONINDEX 要求系統返回該物件在圖標庫中的真實 Index (填入 sfi.iIcon)
            SHGetFileInfoW((LPCWSTR)pIDL, 0, &sfi, sizeof(sfi), SHGFI_PIDL | SHGFI_SYSICONINDEX);

            // 🎯 核心防禦：利用剛剛查出來的真實 sfi.iIcon 索引，從 Jumbo 庫提取出 256x256 實體 HICON 
            pImgList->GetIcon(sfi.iIcon, ILD_TRANSPARENT, &hIcon);
            pImgList->Release();
        }
        CoTaskMemFree(pIDL);

        // 如果連 ImageList 都提不出 HICON，代表系統不支援或發生異常
        if (!hIcon) return false;

        // 3. 將提取成功的實體 hIcon 解包轉換為原始二進位 BMP 數據流填入 pBlob
        bool success = ConvertHIconToBlob(hIcon, pBlob, width, height);
        DestroyIcon(hIcon); // 記得釋放實體控制權
        return success;
    }

    HMODULE hModule = LoadLibraryExW(exePath, NULL, LOAD_LIBRARY_AS_DATAFILE/* | LOAD_LIBRARY_AS_IMAGE_RESOURCE*/);
    if (!hModule)
        return false;

    // 改用新版容器結構
    IconResourceIdent groupIdent;
    EnumResourceNamesW(hModule, RT_GROUP_ICON, EnumGroupIconCallback_Universal, (LONG_PTR)&groupIdent);

    // 檢查是否有成功拿到任何種類的識別名稱
    if (groupIdent.id == 0 && !groupIdent.isNameString)
    {
        FreeLibrary(hModule);
        return false;
    }

    // 2. 依據類型，安全地呼叫 FindResourceW
    HRSRC hGroupRsrc = nullptr;
    if (groupIdent.isNameString)
    {
        // 如果是字串，直接傳入字串指標
        hGroupRsrc = FindResourceW(hModule, groupIdent.name.c_str(), RT_GROUP_ICON);
    }
    else
    {
        // 如果是整數，使用 MAKEINTRESOURCEW 巨集轉型
        hGroupRsrc = FindResourceW(hModule, MAKEINTRESOURCEW(groupIdent.id), RT_GROUP_ICON);
    }

    if (!hGroupRsrc)
    {
        FreeLibrary(hModule);
        return false;
    }

    HGLOBAL hGlobal = LoadResource(hModule, hGroupRsrc);
    LPVOID pGroupData = LockResource(hGlobal);
    if (!pGroupData)
    {
        FreeLibrary(hModule);
        return false;
    }

    // 3. 解析群組標頭與圖標規格陣列
    PICONGROUPHEADER pIconGrpHdr = (PICONGROUPHEADER)pGroupData;

    // 正確指向修正後的結構體陣列開頭
    PGRPICONDIRENTRY pIconEntries = (PGRPICONDIRENTRY)((BYTE*)pGroupData + sizeof(ICONGROUPHEADER));

    int maxArea = -1;
    WORD targetIconId = 0;
    UINT finalWidth = 0;
    UINT finalHeight = 0;

    for (WORD i = 0; i < pIconGrpHdr->idCount; ++i)
    {
        // 256x256 圖標的寬高在結構體中會存為 0
        UINT w = pIconEntries[i].bWidth == 0 ? 256 : pIconEntries[i].bWidth;
        UINT h = pIconEntries[i].bHeight == 0 ? 256 : pIconEntries[i].bHeight;

        int area = w * h;
        if (area > maxArea)
        {
            maxArea = area;
            targetIconId = pIconEntries[i].wResourceId; // 現在這裡能精確讀到正確的 ID 了！
            finalWidth = w;
            finalHeight = h;
        }
    }

    if (targetIconId == 0)
    {
        FreeLibrary(hModule);
        return false;
    }


    // 5. 根據找出的最大圖標資源 ID，載入真正的單一圖標資源（RT_ICON）
    HRSRC hIconRsrc = FindResourceW(hModule, MAKEINTRESOURCEW(targetIconId), RT_ICON);
    if (!hIconRsrc)
    {
        DWORD err = GetLastError();
        FreeLibrary(hModule);
        return false;
    }

    DWORD rsrcSize = SizeofResource(hModule, hIconRsrc);
    HGLOBAL hIconGlobal = LoadResource(hModule, hIconRsrc);
    LPVOID pIconData = LockResource(hIconGlobal);

    if (!pIconData || rsrcSize == 0)
    {
        FreeLibrary(hModule);
        return false;
    }

    // 6. 將二進位原始數據（可能是 PNG 或 BMP 數據）複製到輸出 vector 中
    pBlob.assign((BYTE*)pIconData, (BYTE*)pIconData + rsrcSize);
    width = finalWidth;
    height = finalHeight;

    // 7. 釋放模組控制權並回傳成功
    FreeLibrary(hModule);
    return true;
}

// 輔助函式：將 HICON 轉換為原始的 HBITMAP / PNG 二進位數據 blob
bool ResLoader::ConvertHIconToBlob(HICON hIcon, std::vector<BYTE>& pBlob, UINT& width, UINT& height)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return false;

    // 確保釋放 iconInfo 中的 HBITMAP 避免記憶體洩漏
    struct BitmapGuard {
        HBITMAP hbmColor; HBITMAP hbmMask;
        ~BitmapGuard() { if (hbmColor) DeleteObject(hbmColor); if (hbmMask) DeleteObject(hbmMask); }
    } guard = { iconInfo.hbmColor, iconInfo.hbmMask };

    HBITMAP hBmp = iconInfo.hbmColor ? iconInfo.hbmColor : iconInfo.hbmMask;
    if (!hBmp) return false;

    BITMAP bmp;
    if (!GetObject(hBmp, sizeof(BITMAP), &bmp)) return false;

    width = bmp.bmWidth;
    height = bmp.bmHeight;

    // 計算 32-bit 彩色像素數據與 1-bit And 掩碼數據的大小
    DWORD dwColorSize = ((bmp.bmWidth * 32 + 31) / 32) * 4 * bmp.bmHeight;
    DWORD dwMaskSize = ((bmp.bmWidth * 1 + 31) / 32) * 4 * bmp.bmHeight;

    // 分配目標記憶體：BITMAPINFOHEADER (40 萬) + 彩色數據 + 遮罩數據
    DWORD dwTotalSize = sizeof(BITMAPINFOHEADER) + dwColorSize + dwMaskSize;
    pBlob.resize(dwTotalSize);

    // 1. 建立 32-bit RGBA 點陣圖資訊標頭 (BITMAPINFOHEADER)
    BITMAPINFOHEADER bi = { 0 };
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = bmp.bmWidth;
    // 🎯 核心設計：圖標資源（RT_ICON）的 biHeight 必須是彩色圖高的 2 倍
    bi.biHeight = bmp.bmHeight * 2;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;

    // 將 40 位元組的 BITMAPINFOHEADER 拷貝至 pBlob 開頭
    memcpy(pBlob.data(), &bi, sizeof(BITMAPINFOHEADER));

    HDC hDC = GetDC(nullptr);

    // 2. 提取彩色像素數據 (XOR 掩碼)
    // 建立一個安全、帶有足夠緩衝區的 BITMAPINFO 結構，避免 GetDIBits 踩壞 Stack
    struct {
        BITMAPINFOHEADER bmiHeader;
        RGBQUAD bmiColors[256]; // 預留最大顏色表，確保不論如何都不會 Stack Overflow
    } safeBmi;

    memset(&safeBmi, 0, sizeof(safeBmi));
    safeBmi.bmiHeader = bi;
    safeBmi.bmiHeader.biHeight = bmp.bmHeight; // 讀取數據時要用單倍高

    GetDIBits(hDC, hBmp, 0, bmp.bmHeight, pBlob.data() + sizeof(BITMAPINFOHEADER), (BITMAPINFO*)&safeBmi, DIB_RGB_COLORS);

    // 3. 🎯 核心修正：安全提取透明掩碼數據 (AND 掩碼)
    if (iconInfo.hbmMask)
    {
        // 重新設定為 1-bit 圖標資訊，並在 safeBmi 中自動保留了 1-bit 所需的 2 個色彩表空間
        safeBmi.bmiHeader.biBitCount = 1;
        safeBmi.bmiHeader.biCompression = BI_RGB;

        // 執行 GetDIBits，此時內部的色彩表寫入會安全地落在 safeBmi.bmiColors 內，絕對不會造成毀損
        GetDIBits(hDC, iconInfo.hbmMask, 0, bmp.bmHeight, pBlob.data() + sizeof(BITMAPINFOHEADER) + dwColorSize, (BITMAPINFO*)&safeBmi, DIB_RGB_COLORS);
    }
    else
    {
        // 如果原本就沒有 Mask，則全部填滿 0x00
        memset(pBlob.data() + sizeof(BITMAPINFOHEADER) + dwColorSize, 0, dwMaskSize);
    }

    ReleaseDC(nullptr, hDC);
    return true;
}