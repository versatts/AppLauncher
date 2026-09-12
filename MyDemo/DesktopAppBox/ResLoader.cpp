#include "ResLoader.h"
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "user32.lib")


ID3D11ShaderResourceView* LoadHighestResIconSRV(ID3D11Device* pDevice, const wchar_t* exePath, UINT& outW, UINT& outH)
{
    std::vector<BYTE> iconBlob;
    UINT width = 0, height = 0;

    // 1. 提取最大的圖標資源數據
    if (!LoadLargestIconResourceFromExe(exePath, iconBlob, width, height) || iconBlob.empty())
    {
        return nullptr;
    }

    // 2. 檢查這段 Blob 是不是 PNG 格式
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
        // 情況 A：如果是高解析度的原始 PNG 壓縮，直接走 WIC 解碼（畫質最完美）
        return CreateSRVFromPngBlob(pDevice, iconBlob, outW, outH);
    }
    else
    {
        // 情況 B：如果不是 PNG（是 BMP 格式），直接從記憶體 Blob 建立 HICON
        // 這是 Windows 專門用來處理 RT_ICON 記憶體數據的標準 API
        HICON hIcon = CreateIconFromResourceEx(
            iconBlob.data(),            // 資源數據指標
            (DWORD)iconBlob.size(),     // 資源大小
            TRUE,                       // TRUE 代表這是圖標 (Icon)，FALSE 代表是游標 (Cursor)
            0x00030000,                 // 固定的 Windows 圖標版本號 (0x00030000)
            width,                      // 期望的寬度
            height,                     // 期望的高度
            LR_DEFAULTCOLOR             // 載入標記
        );

        if (hIcon)
        {
            int w = 0, h = 0;
            // 呼叫您之前的 HICON 轉 SRV 函式
            ID3D11ShaderResourceView* pSRV = IconToD3D11SRV_Simple(pDevice, hIcon, w, h);

            // 💡 務必釋放記憶體中的 HICON
            DestroyIcon(hIcon);

            outW = (UINT)w;
            outH = (UINT)h;
            return pSRV;
        }
    }

    return nullptr;
}


ID3D11ShaderResourceView* CreateSRVFromPngBlob(ID3D11Device* pDevice, const std::vector<BYTE>& pngBlob, UINT& outW, UINT& outH)
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

    return pSRV;
}


ID3D11ShaderResourceView* IconToD3D11SRV_Simple(ID3D11Device* pDevice, HICON hIcon, int& outW, int& outH)
{
    ICONINFO iconInfo;
    if (!GetIconInfo(hIcon, &iconInfo)) return nullptr;

    BITMAP bm;
    GetObject(iconInfo.hbmColor, sizeof(bm), &bm);
    outW = bm.bmWidth;
    outH = bm.bmHeight;

    // 建立記憶體 DC 來讀取點陣圖顏色
    HDC hdc = GetDC(NULL);
    HDC hMemDC = CreateCompatibleDC(hdc);
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, iconInfo.hbmColor);

    std::vector<DWORD> pixels(outW * outH);
    BITMAPINFO bmi = {};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = outW;
    bmi.bmiHeader.biHeight = -outH; // 負數表示由上至下
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    GetDIBits(hMemDC, iconInfo.hbmColor, 0, outH, pixels.data(), &bmi, DIB_RGB_COLORS);

    // 這裡需要注意：部分舊圖標的 hbmColor 點陣圖中 Alpha 通道全是 0
    // 如果發現全透明，需要去 iconInfo.hbmMask 裡去撈 MASK 進行修復（篇幅限制此處簡化）

    // 建立 D3D11 資源 (邏輯同上)
        // 1. 設定紋理描述，開啟 Mipmaps 支援
    ID3D11ShaderResourceView* pSRV = nullptr; // 💡 在這裡補上定義！

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
                pContext->UpdateSubresource(pTex, 0, nullptr, pixels.data(), outW * 4, 0);
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
        initData.pSysMem = pixels.data();
        initData.SysMemPitch = outW * 4;
        if (SUCCEEDED(pDevice->CreateTexture2D(&desc, &initData, &pTex))) {
            pDevice->CreateShaderResourceView(pTex, nullptr, &pSRV);
            pTex->Release();
        }
    }

    // 清理 Windows GDI 資源
    SelectObject(hMemDC, hOldBmp);
    DeleteDC(hMemDC);
    ReleaseDC(NULL, hdc);
    DeleteObject(iconInfo.hbmColor);
    DeleteObject(iconInfo.hbmMask);

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
bool LoadLargestIconResourceFromExe(const wchar_t* exePath, std::vector<BYTE>& pBlob, UINT& width, UINT& height)
{
    HMODULE hModule = LoadLibraryExW(exePath, NULL, LOAD_LIBRARY_AS_DATAFILE | LOAD_LIBRARY_AS_IMAGE_RESOURCE);
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