#include "work_discovery.h"
#include <algorithm>

namespace ucm {

bool ParseWorkName(const wstring& name, const wstring& prefix, int digits, int& number) {
    if (digits <= 0 || digits > 9) return false;
    if (name.size() != prefix.size() + (size_t)digits) return false;
    if (ToUpper(name.substr(0, prefix.size())) != ToUpper(prefix)) return false;
    long long v = 0;
    for (size_t i = prefix.size(); i < name.size(); ++i) {
        if (name[i] < L'0' || name[i] > L'9') return false;
        v = v * 10 + (name[i] - L'0');
    }
    number = (int)v;
    return true;
}

std::vector<Work> DiscoverWorks(const wstring& root, const wstring& prefix, int digits) {
    std::vector<Work> r;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(Join(root, L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) continue;
        int n = 0;
        if (!ParseWorkName(fd.cFileName, prefix, digits, n)) continue;
        Work w;
        w.number = n; w.name = fd.cFileName; w.path = Join(root, fd.cFileName);
        r.push_back(w);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(r.begin(), r.end(), [](const Work& a, const Work& b) { return a.number < b.number; });
    return r;
}

static bool Walk(const wstring& base, const wstring& rel, Tree& t) {
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(Join(Join(base, rel), L"*").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_NOT_FOUND;
    bool ok = true;
    do {
        wstring n = fd.cFileName;
        if (n == L"." || n == L"..") continue;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) continue;   // evita cicli/symlink
        wstring r = rel.empty() ? n : rel + L"\\" + n;
        if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            t.dirs.push_back(r);
            if (!Walk(base, r, t)) ok = false;
        } else {
            FileEntry e;
            e.rel = r;
            e.size = ((uint64_t)fd.nFileSizeHigh << 32) | fd.nFileSizeLow;
            t.files.push_back(e);
            t.total += e.size;
        }
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    return ok;
}

bool ListTree(const wstring& root, Tree& out) {
    out = Tree();
    bool ok = Walk(root, L"", out);
    std::sort(out.files.begin(), out.files.end(), [](const FileEntry& a, const FileEntry& b) { return ToUpper(a.rel) < ToUpper(b.rel); });
    return ok;
}

}
