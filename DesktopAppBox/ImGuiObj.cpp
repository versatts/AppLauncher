#include "ImGuiObj.h"

ImColor ImGuiObj::ToImClr(const std::string& hexStr)
{
    // 移除可能存在的 '#' 前缀
    std::string cleanHex = hexStr;
    if (!cleanHex.empty() && cleanHex[0] == '#') {
        cleanHex = cleanHex.substr(1);
    }

    // 默认返回黑色（如果字符串格式不正确）
    unsigned int rgba = 0x000000FF;

    try {
        if (cleanHex.length() == 6) {
            // 只有 RGB，默认 Alpha 为 255 (FF)
            unsigned int rgb = std::stoul(cleanHex, nullptr, 16);
            rgba = (rgb << 8) | 0xFF;
        }
        else if (cleanHex.length() == 8) {
            // 包含 RGBA
            rgba = std::stoul(cleanHex, nullptr, 16);
        }
    }
    catch (...) {
        // 解析失败时返回默认颜色
        return ImColor(0, 0, 0, 255);
    }

    // 提取通道并返回 ImColor
    int r = (rgba >> 24) & 0xFF;
    int g = (rgba >> 16) & 0xFF;
    int b = (rgba >> 8) & 0xFF;
    int a = rgba & 0xFF;

    return ImColor(r, g, b, a);
}

ImVec4 ImGuiObj::ToImVec4(const std::string& hexStr, float alpha) {
    // 移除可能存在的 '#' 前綴
    std::string cleanHex = hexStr;
    if (!cleanHex.empty() && cleanHex[0] == '#') {
        cleanHex = cleanHex.substr(1);
    }

    // 預設返回黑色不透明 (0, 0, 0, alpha)
    unsigned int rgb = 0x000000;

    try {
        if (cleanHex.length() >= 6) {
            // 取前 6 位進行 RGB 解析
            rgb = std::stoul(cleanHex.substr(0, 6), nullptr, 16);
        }
    }
    catch (...) {
        // 解析失敗時返回帶自訂透明度的黑色
        return ImVec4(0.0f, 0.0f, 0.0f, alpha);
    }

    // 提取通道並將 0-255 對映到 0.0f-1.0f
    float r = static_cast<float>((rgb >> 16) & 0xFF) / 255.0f;
    float g = static_cast<float>((rgb >> 8) & 0xFF) / 255.0f;
    float b = static_cast<float>(rgb & 0xFF) / 255.0f;

    return ImVec4(r, g, b, alpha);
}
