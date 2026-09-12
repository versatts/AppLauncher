#pragma once

#include <shobjidl_core.h>
#include <shlobj.h>
#include <wincodec.h>
#include <vector>
#include <string>
#pragma comment(lib,"comsuppw.lib")
#pragma comment(lib,"d3d11.lib")

// lnkFullPath: L"C:\\xxx\\xxx.lnk"

std::vector<std::wstring> EnumLnkFilesInAppDir();

bool ResolveLnkTarget(LPCWSTR lnkFullPath, WCHAR* outExePath, int outPathBufSize);

HICON ExtractExeMainIcon(LPCWSTR exePath);