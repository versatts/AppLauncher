#pragma once

#include <shobjidl_core.h>
#include <shlobj.h>
#include <wincodec.h>
#include <vector>
#include <string>
#pragma comment(lib,"comsuppw.lib")
#pragma comment(lib,"d3d11.lib")


std::vector<std::wstring> EnumLnkFilesInAppDir(const std::wstring& subDirName, std::vector<std::wstring>& outSubDirList);

bool ResolveLnkTarget(LPCWSTR lnkFullPath, WCHAR* outExePath, int outPathBufSize);

bool ResolveUrlTarget(LPCWSTR urlFullPath, WCHAR* outExePath, int outPathBufSize, WCHAR* outIconPath, int outIconBufSize);

HICON ExtractExeMainIcon(LPCWSTR exePath);