#pragma once

#include <windows.h>
#include <vector>

struct MonInfo
{
    HMONITOR hMon;
    RECT rcMonitor;    // 显示器完整屏幕坐标（包含任务栏区域）
    RECT rcWork;       // 工作区（扣除任务栏）
    bool isPrimary;
};

static BOOL CALLBACK EnumMonCallback(HMONITOR hMon, HDC hdc, LPRECT pRect, LPARAM lParam)
{
    std::vector<MonInfo>* pOut = (std::vector<MonInfo>*)lParam;
    MONITORINFOEX mi;
    ZeroMemory(&mi, sizeof(mi));
    mi.cbSize = sizeof(MONITORINFOEX);
    GetMonitorInfo(hMon, &mi);

    MonInfo info;
    info.hMon = hMon;
    info.rcMonitor = mi.rcMonitor;
    info.rcWork = mi.rcWork;
    info.isPrimary = !!(mi.dwFlags & MONITORINFOF_PRIMARY);
    pOut->push_back(info);
    return TRUE;
}

// 获取全部显示器列表
std::vector<MonInfo> GetAllMonitors()
{
    std::vector<MonInfo> list;
    EnumDisplayMonitors(NULL, NULL, EnumMonCallback, (LPARAM)&list);
    return list;
}
