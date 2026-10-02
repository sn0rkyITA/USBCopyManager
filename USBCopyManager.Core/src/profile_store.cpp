#include "profile_store.h"

namespace ucm {

ProfileStore::ProfileStore() : dir_(Join(AppDataDir(), L"config")) { EnsureDir(dir_); }

static int ToInt(const std::map<wstring, wstring>& m, const wchar_t* k, int def) {
    auto it = m.find(k);
    if (it == m.end()) return def;
    try { return std::stoi(it->second); } catch (...) { return def; }
}
static wstring ToStr(const std::map<wstring, wstring>& m, const wchar_t* k, const wstring& def = L"") {
    auto it = m.find(k);
    return it == m.end() ? def : it->second;
}

std::vector<Profile> ProfileStore::List() {
    std::lock_guard<std::mutex> lk(mu_);
    std::vector<Profile> r;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(Join(dir_, L"*.profile").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do {
        wstring text;
        if (!ReadAllText(Join(dir_, fd.cFileName), text)) continue;
        auto m = ParseKv(text);
        Profile p;
        p.id = ToStr(m, L"id");
        if (p.id.empty()) continue;
        p.name = ToStr(m, L"name");
        p.serial = ToStr(m, L"serial");
        p.destination = ToStr(m, L"destination");
        p.prefix = ToStr(m, L"prefix", L"SN_");
        p.digits = ToInt(m, L"digits", 5);
        p.mode = ToInt(m, L"mode", 0);
        p.lastCopiedWork = ToInt(m, L"lastCopiedWork", 0);
        p.verify = ToInt(m, L"verify", 1);
        p.autoStart = ToInt(m, L"autoStart", 0);
        r.push_back(p);
    } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(r.begin(), r.end(), [](const Profile& a, const Profile& b) { return ToUpper(a.name) < ToUpper(b.name); });
    return r;
}

bool ProfileStore::Get(const wstring& id, Profile& out) {
    for (auto& p : List()) if (p.id == id) { out = p; return true; }
    return false;
}

bool ProfileStore::SaveLocked(Profile& p) {
    if (p.id.empty()) p.id = NewGuid();
    wstring t;
    t += L"id=" + p.id + L"\n";
    t += L"name=" + p.name + L"\n";
    t += L"serial=" + p.serial + L"\n";
    t += L"destination=" + p.destination + L"\n";
    t += L"prefix=" + p.prefix + L"\n";
    t += L"digits=" + std::to_wstring(p.digits) + L"\n";
    t += L"mode=" + std::to_wstring(p.mode) + L"\n";
    t += L"lastCopiedWork=" + std::to_wstring(p.lastCopiedWork) + L"\n";
    t += L"verify=" + std::to_wstring(p.verify) + L"\n";
    t += L"autoStart=" + std::to_wstring(p.autoStart) + L"\n";
    return WriteAllTextAtomic(Join(dir_, SafeFileName(p.id) + L".profile"), t);
}

bool ProfileStore::Save(Profile& p) {
    std::lock_guard<std::mutex> lk(mu_);
    return SaveLocked(p);
}

bool ProfileStore::Delete(const wstring& id) {
    std::lock_guard<std::mutex> lk(mu_);
    return DeleteFileW(Join(dir_, SafeFileName(id) + L".profile").c_str()) != 0;
}

bool ProfileStore::SetLastCopied(const wstring& id, int work) {
    Profile p;
    if (!Get(id, p)) return false;
    std::lock_guard<std::mutex> lk(mu_);
    p.lastCopiedWork = work;
    return SaveLocked(p);
}

}
