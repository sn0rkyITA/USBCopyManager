#include "journal.h"
#include <algorithm>
#include <cwchar>

namespace ucm {

static wstring DirFor(const wstring& serial) { return Join(Join(AppDataDir(), L"journal"), SafeFileName(serial)); }

wstring Journal::PathFor(const wstring& serial, const wstring& jobId) {
    return Join(DirFor(serial), SafeFileName(jobId) + L".journal");
}

static wstring HashToText(const Sha256State& s) {
    wstring t;
    wchar_t b[16];
    for (int i = 0; i < 8; ++i) { swprintf(b, 16, L"%08x,", (unsigned)s.h[i]); t += b; }
    t += std::to_wstring(s.length);
    return t;
}

static bool HashFromText(const wstring& t, Sha256State& s) {
    auto parts = Split(t, L',');
    if (parts.size() != 9) return false;
    try {
        for (int i = 0; i < 8; ++i) s.h[i] = (uint32_t)std::stoul(parts[i], nullptr, 16);
        s.length = std::stoull(parts[8]);
    } catch (...) { return false; }
    return true;
}

bool Journal::Save(const JournalData& j) {
    wstring t;
    t += L"jobId=" + j.jobId + L"\n";
    t += L"serial=" + j.serial + L"\n";
    t += L"profileId=" + j.profileId + L"\n";
    t += L"source=" + j.source + L"\n";
    t += L"destination=" + j.destination + L"\n";
    t += L"work=" + j.work + L"\n";
    t += L"workNumber=" + std::to_wstring(j.workNumber) + L"\n";
    t += L"state=" + std::to_wstring(j.state) + L"\n";
    t += L"currentFile=" + j.currentFile + L"\n";
    t += L"completed=" + JoinList(j.completedFiles, L'|') + L"\n";
    t += L"offset=" + std::to_wstring(j.currentOffset) + L"\n";
    t += L"srcSize=" + std::to_wstring(j.srcSize) + L"\n";
    t += L"srcMtime=" + std::to_wstring(j.srcMtime) + L"\n";
    t += L"hash=" + (j.hasHash ? HashToText(j.hash) : wstring()) + L"\n";
    t += L"updated=" + NowText() + L"\n";
    return WriteAllTextAtomic(PathFor(j.serial, j.jobId), t);
}

bool Journal::Load(const wstring& path, JournalData& j) {
    wstring text;
    if (!ReadAllText(path, text)) return false;
    auto m = ParseKv(text);
    auto S = [&](const wchar_t* k) { auto it = m.find(k); return it == m.end() ? wstring() : it->second; };
    try {
        j = JournalData();
        j.jobId = S(L"jobId"); j.serial = S(L"serial"); j.profileId = S(L"profileId");
        j.source = S(L"source"); j.destination = S(L"destination");
        j.work = S(L"work"); j.currentFile = S(L"currentFile");
        j.workNumber = std::stoi(S(L"workNumber"));
        j.state = std::stoi(S(L"state"));
        j.completedFiles = Split(S(L"completed"), L'|');
        j.currentOffset = std::stoull(S(L"offset"));
        j.srcSize = std::stoull(S(L"srcSize"));
        j.srcMtime = std::stoull(S(L"srcMtime"));
        j.hasHash = HashFromText(S(L"hash"), j.hash);
    } catch (...) { return false; }
    return !j.jobId.empty();
}

static std::vector<wstring> ListJournals(const wstring& serial) {
    std::vector<wstring> r;
    WIN32_FIND_DATAW fd;
    HANDLE h = FindFirstFileW(Join(DirFor(serial), L"*.journal").c_str(), &fd);
    if (h == INVALID_HANDLE_VALUE) return r;
    do { r.push_back(fd.cFileName); } while (FindNextFileW(h, &fd));
    FindClose(h);
    std::sort(r.begin(), r.end());          // jobId inizia con il timestamp
    return r;
}

bool Journal::FindResumable(const wstring& serial, const wstring& profileId, JournalData& out) {
    auto files = ListJournals(serial);
    for (auto it = files.rbegin(); it != files.rend(); ++it) {
        JournalData j;
        if (!Load(Join(DirFor(serial), *it), j) || j.profileId != profileId) continue;
        if (j.state == UCM_JOB_RUNNING || j.state == UCM_JOB_INTERRUPTED) { out = j; return true; }
        return false;                        // l'ultimo job del profilo e' chiuso: niente resume
    }
    return false;
}

void Journal::Prune(const wstring& serial, size_t keep) {
    auto files = ListJournals(serial);
    if (files.size() <= keep) return;
    for (size_t i = 0; i < files.size() - keep; ++i) DeleteFileW(Join(DirFor(serial), files[i]).c_str());
}

}
