#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ucm {
using std::wstring;

wstring AppDataDir();                          // %LOCALAPPDATA%\USBCopyManager
wstring Join(const wstring& a, const wstring& b);
bool EnsureDir(const wstring& path);           // ricorsiva
wstring NewGuid();                             // 32 hex
wstring NowStamp();                            // yyyyMMddHHmmss
wstring NowText();                             // yyyy-MM-dd HH:mm:ss.mmm
wstring ToUpper(wstring s);
wstring Trim(wstring s);
wstring SafeFileName(wstring s);
wstring Utf8ToW(const std::string& s);
std::string WToUtf8(const wstring& s);
bool ReadAllText(const wstring& path, wstring& out);
bool WriteAllTextAtomic(const wstring& path, const wstring& text);
void CopyStr(wchar_t* dst, size_t capacity, const wstring& src);
std::map<wstring, wstring> ParseKv(const wstring& text);
std::vector<wstring> Split(const wstring& s, wchar_t sep);
wstring JoinList(const std::vector<wstring>& v, wchar_t sep);
}
