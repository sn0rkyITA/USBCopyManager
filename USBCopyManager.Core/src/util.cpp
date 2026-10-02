#include "util.h"
#include <objbase.h>
#include <cstdio>
#include <cwchar>

namespace ucm {

wstring AppDataDir() {
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    wstring base = (n > 0 && n < MAX_PATH) ? wstring(buf) : L"C:\\ProgramData";
    return Join(base, L"USBCopyManager");
}

wstring Join(const wstring& a, const wstring& b) {
    if (a.empty()) return b;
    if (b.empty()) return a;
    wchar_t last = a.back();
    if (last == L'\\' || last == L'/') return a + b;
    return a + L"\\" + b;
}

bool EnsureDir(const wstring& path) {
    if (path.empty()) return false;
    DWORD a = GetFileAttributesW(path.c_str());
    if (a != INVALID_FILE_ATTRIBUTES) return (a & FILE_ATTRIBUTE_DIRECTORY) != 0;
    size_t pos = path.find_last_of(L"\\/");
    if (pos != wstring::npos && pos > 2) EnsureDir(path.substr(0, pos));
    if (CreateDirectoryW(path.c_str(), nullptr)) return true;
    return GetLastError() == ERROR_ALREADY_EXISTS;
}

wstring NewGuid() {
    GUID g{};
    CoCreateGuid(&g);
    wchar_t b[40];
    swprintf(b, 40, L"%08lx%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
             (unsigned long)g.Data1, (unsigned)g.Data2, (unsigned)g.Data3,
             (unsigned)g.Data4[0], (unsigned)g.Data4[1], (unsigned)g.Data4[2], (unsigned)g.Data4[3],
             (unsigned)g.Data4[4], (unsigned)g.Data4[5], (unsigned)g.Data4[6], (unsigned)g.Data4[7]);
    return b;
}

wstring NowStamp() {
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t b[32];
    swprintf(b, 32, L"%04d%02d%02d%02d%02d%02d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    return b;
}

wstring NowText() {
    SYSTEMTIME t; GetLocalTime(&t);
    wchar_t b[40];
    swprintf(b, 40, L"%04d-%02d-%02d %02d:%02d:%02d.%03d", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
    return b;
}

wstring ToUpper(wstring s) {
    for (auto& c : s) c = (wchar_t)towupper(c);
    return s;
}

wstring Trim(wstring s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == L' ' || s[b] == L'\t' || s[b] == L'\r' || s[b] == L'\n' || s[b] == 0)) ++b;
    while (e > b && (s[e - 1] == L' ' || s[e - 1] == L'\t' || s[e - 1] == L'\r' || s[e - 1] == L'\n' || s[e - 1] == 0)) --e;
    return s.substr(b, e - b);
}

wstring SafeFileName(wstring s) {
    for (auto& c : s) if (wcschr(L"<>:\"/\\|?*", c) || c < 32) c = L'_';
    return s.empty() ? L"_" : s;
}

wstring Utf8ToW(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

std::string WToUtf8(const wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, 0);
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

bool ReadAllText(const wstring& path, wstring& out) {
    HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz{};
    GetFileSizeEx(h, &sz);
    std::string data((size_t)sz.QuadPart, 0);
    DWORD rd = 0;
    bool ok = sz.QuadPart == 0 || ReadFile(h, &data[0], (DWORD)data.size(), &rd, nullptr);
    CloseHandle(h);
    if (!ok) return false;
    data.resize(rd);
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF) data.erase(0, 3);
    out = Utf8ToW(data);
    return true;
}

bool WriteAllTextAtomic(const wstring& path, const wstring& text) {
    size_t pos = path.find_last_of(L"\\/");
    if (pos != wstring::npos) EnsureDir(path.substr(0, pos));
    wstring tmp = path + L".tmp";
    HANDLE h = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;
    std::string u = WToUtf8(text);
    DWORD wr = 0;
    bool ok = u.empty() || WriteFile(h, u.data(), (DWORD)u.size(), &wr, nullptr);
    if (ok) ok = FlushFileBuffers(h) != 0;
    CloseHandle(h);
    if (!ok) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

void CopyStr(wchar_t* dst, size_t capacity, const wstring& src) {
    if (capacity == 0) return;
    size_t n = src.size() < capacity - 1 ? src.size() : capacity - 1;
    wmemcpy(dst, src.c_str(), n);
    dst[n] = 0;
}

std::map<wstring, wstring> ParseKv(const wstring& text) {
    std::map<wstring, wstring> m;
    size_t i = 0;
    while (i < text.size()) {
        size_t e = text.find(L'\n', i);
        if (e == wstring::npos) e = text.size();
        wstring line = text.substr(i, e - i);
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t eq = line.find(L'=');
        if (eq != wstring::npos && eq > 0) m[line.substr(0, eq)] = line.substr(eq + 1);
        i = e + 1;
    }
    return m;
}

std::vector<wstring> Split(const wstring& s, wchar_t sep) {
    std::vector<wstring> r;
    if (s.empty()) return r;
    size_t i = 0;
    while (true) {
        size_t e = s.find(sep, i);
        if (e == wstring::npos) { r.push_back(s.substr(i)); break; }
        r.push_back(s.substr(i, e - i));
        i = e + 1;
    }
    return r;
}

wstring JoinList(const std::vector<wstring>& v, wchar_t sep) {
    wstring r;
    for (size_t i = 0; i < v.size(); ++i) { if (i) r.push_back(sep); r += v[i]; }
    return r;
}

} // namespace ucm
