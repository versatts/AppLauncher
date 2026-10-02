#pragma once

#include <shobjidl_core.h>
#include <shlobj.h>
#include <wincodec.h>
#include <vector>
#include <string>
#pragma comment(lib,"comsuppw.lib")
#pragma comment(lib,"d3d11.lib")

std::string GetWindowsAccentColor();

bool CheckSpecificIcon(const std::wstring& exePath, std::wstring& sIcon);

std::string GetUtf8FileNameFromWstring(const std::wstring& wpath);

std::vector<std::wstring> EnumLnkFilesInAppDir(const std::wstring& subDirName, std::vector<std::wstring>& outSubDirList);

// 取得 exe 所在目錄下 lnk 資料夾的完整路徑（subDirName 為空 = 根目錄，非空 = lnk\子目錄）
std::wstring GetLnkDirForFolder(const std::wstring& subDirName);

// ===== 🚀 排序前綴工具（"NNNN-" 檔名前綴用於持久化顯示順序）=====
// 去掉名稱開頭的 "NNNN-" 前綴（4 位數字 + '-'），無前綴則原樣返回
std::wstring StripOrderPrefix(const std::wstring& name);
// 去掉完整路徑中「檔名部分」的 "NNNN-" 前綴（路徑其餘部分不變）
std::wstring StripOrderPrefixFromPath(const std::wstring& fullPath);
// 解析完整路徑檔名的 "NNNN-" 前綴數字；無前綴返回 false
bool ParseOrderPrefix(const std::wstring& fullPath, int& outNum);
// 統計目錄內 .lnk/.url 檔案數量（拖入新檔時分配下一個前綴序號用）
int CountLnkUrlFilesInDir(const std::wstring& dir);

// 解析 lnk：輸出目標路徑；outArguments（可選）輸出捷徑中的啟動參數
bool ResolveLnkTarget(LPCWSTR lnkFullPath, WCHAR* outExePath, int outPathBufSize,
                      WCHAR* outArguments = nullptr, int outArgsBufSize = 0);

bool ResolveUrlTarget(LPCWSTR urlFullPath, WCHAR* outExePath, int outPathBufSize, WCHAR* outIconPath, int outIconBufSize);

HICON ExtractExeMainIcon(LPCWSTR exePath);