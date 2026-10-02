#include "transfer_engine.h"
#include "ucm_api.h"
#include <algorithm>
#include <cstring>

namespace ucm {

namespace {

constexpr size_t kChunk = 4u * 1024 * 1024;                 // 4 MiB (multiplo di 64: hash state sempre allineato)
constexpr uint64_t kCheckpointEvery = 64ull * 1024 * 1024;   // checkpoint ogni 64 MiB

struct Handle {
    HANDLE h = INVALID_HANDLE_VALUE;
    ~Handle() { Close(); }
    void Close() { if (h != INVALID_HANDLE_VALUE) { CloseHandle(h); h = INVALID_HANDLE_VALUE; } }
    bool Valid() const { return h != INVALID_HANDLE_VALUE; }
};

uint64_t FileTimeToU64(const FILETIME& ft) { return ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime; }

bool Cancelled(const CopyRequest& rq) { return rq.cancel && rq.cancel->load(); }

wstring Err(const wchar_t* what, DWORD code) { return wstring(what) + L" (codice " + std::to_wstring(code) + L")"; }

bool ReadFull(HANDLE h, uint8_t* buf, DWORD want, DWORD& got) {
    got = 0;
    while (got < want) {
        DWORD r = 0;
        if (!ReadFile(h, buf + got, want - got, &r, nullptr)) return false;
        if (r == 0) break;
        got += r;
    }
    return true;
}

} // namespace

XferResult CopyFileResumable(CopyRequest& rq) {
    const wstring partial = rq.dst + L".partial";
    auto Log = [&](int lvl, const wstring& m) { if (rq.log) rq.log(lvl, m); };

    size_t slash = rq.dst.find_last_of(L"\\/");
    if (slash != wstring::npos) EnsureDir(rq.dst.substr(0, slash));

    // --- sorgente ---
    Handle src;
    src.h = CreateFileW(rq.src.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (!src.Valid()) { rq.error = Err(L"Impossibile aprire il sorgente", GetLastError()); return XferResult::IoError; }
    LARGE_INTEGER sz{};
    FILETIME mt{};
    if (!GetFileSizeEx(src.h, &sz) || !GetFileTime(src.h, nullptr, nullptr, &mt)) {
        rq.error = Err(L"Impossibile leggere gli attributi del sorgente", GetLastError());
        return XferResult::IoError;
    }
    const uint64_t srcSize = (uint64_t)sz.QuadPart;
    const uint64_t srcMtime = FileTimeToU64(mt);

    // --- decisione di resume ---
    uint64_t offset = 0;
    Sha256 hash;
    if (rq.hasResume) {
        bool valid = rq.resumeSrcSize == srcSize && rq.resumeSrcMtime == srcMtime && rq.resumeOffset > 0 &&
                     rq.resumeOffset <= srcSize && rq.resumeOffset % 64 == 0 && rq.resumeHash.length == rq.resumeOffset;
        if (!valid) {
            Log(UCM_LOG_WARNING, L"Resume non valido (sorgente modificato o checkpoint incoerente): riparto da zero: " + rq.src);
        } else {
            WIN32_FILE_ATTRIBUTE_DATA ad{};
            if (GetFileAttributesExW(partial.c_str(), GetFileExInfoStandard, &ad)) {
                uint64_t psz = ((uint64_t)ad.nFileSizeHigh << 32) | ad.nFileSizeLow;
                if (psz >= rq.resumeOffset) {
                    offset = rq.resumeOffset;
                    hash.SetState(rq.resumeHash);
                }
            }
            if (offset == 0) Log(UCM_LOG_WARNING, L".partial mancante o troppo corto: riparto da zero: " + rq.src);
        }
    }

    // --- destinazione .partial ---
    Handle dst;
    dst.h = CreateFileW(partial.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, offset ? OPEN_EXISTING : CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (!dst.Valid()) { rq.error = Err(L"Impossibile creare il file .partial", GetLastError()); return XferResult::IoError; }
    LARGE_INTEGER pos{};
    pos.QuadPart = (LONGLONG)offset;
    if (offset) {
        if (!SetFilePointerEx(dst.h, pos, nullptr, FILE_BEGIN) || !SetEndOfFile(dst.h) ||
            !SetFilePointerEx(src.h, pos, nullptr, FILE_BEGIN)) {
            rq.error = Err(L"Impossibile riposizionare i file per il resume", GetLastError());
            return XferResult::IoError;
        }
        Log(UCM_LOG_INFO, L"Resume da " + std::to_wstring(offset) + L" / " + std::to_wstring(srcSize) + L" byte: " + rq.src);
        if (rq.onBytes) rq.onBytes(offset);
    }

    // --- copia a streaming ---
    std::vector<uint8_t> buf(kChunk);
    uint64_t sinceCheckpoint = 0;
    while (offset < srcSize) {
        if (Cancelled(rq)) return XferResult::Cancelled;
        DWORD want = (DWORD)std::min<uint64_t>(kChunk, srcSize - offset), got = 0;
        if (!ReadFull(src.h, buf.data(), want, got)) { rq.error = Err(L"Errore di lettura dal sorgente", GetLastError()); return XferResult::IoError; }
        if (got == 0) { rq.error = L"Il sorgente e' stato troncato durante la copia"; return XferResult::IoError; }
        hash.Update(buf.data(), got);
        DWORD wr = 0;
        if (!WriteFile(dst.h, buf.data(), got, &wr, nullptr) || wr != got) { rq.error = Err(L"Errore di scrittura nella destinazione", GetLastError()); return XferResult::IoError; }
        offset += got;
        sinceCheckpoint += got;
        if (rq.onBytes) rq.onBytes(got);

        if (sinceCheckpoint >= kCheckpointEvery && offset < srcSize) {
            Checkpoint cp;
            if (hash.GetState(cp.hash)) {
                if (!FlushFileBuffers(dst.h)) { rq.error = Err(L"Flush fallito", GetLastError()); return XferResult::IoError; }
                cp.offset = offset; cp.srcSize = srcSize; cp.srcMtime = srcMtime;
                if (rq.onCheckpoint) rq.onCheckpoint(cp);
                sinceCheckpoint = 0;
            }
        }
    }
    if (Cancelled(rq)) return XferResult::Cancelled;

    uint8_t digest[32];
    hash.Final(digest);
    rq.sha256Hex = Sha256::Hex(digest, 32);
    if (!FlushFileBuffers(dst.h)) { rq.error = Err(L"Flush finale fallito", GetLastError()); return XferResult::IoError; }

    // --- verifica: dimensione + hash della destinazione riletta ---
    LARGE_INTEGER dsz{};
    if (!GetFileSizeEx(dst.h, &dsz) || (uint64_t)dsz.QuadPart != srcSize) {
        rq.error = L"Verifica fallita: dimensione della destinazione diversa dal sorgente";
        dst.Close(); DeleteFileW(partial.c_str());
        return XferResult::VerifyError;
    }
    if (rq.verify) {
        LARGE_INTEGER z{};
        if (!SetFilePointerEx(dst.h, z, nullptr, FILE_BEGIN)) { rq.error = Err(L"Seek di verifica fallito", GetLastError()); return XferResult::IoError; }
        Sha256 vh;
        uint64_t done = 0;
        while (done < srcSize) {
            if (Cancelled(rq)) return XferResult::Cancelled;
            DWORD want = (DWORD)std::min<uint64_t>(kChunk, srcSize - done), got = 0;
            if (!ReadFull(dst.h, buf.data(), want, got) || got == 0) { rq.error = Err(L"Errore di lettura in verifica", GetLastError()); return XferResult::IoError; }
            vh.Update(buf.data(), got);
            done += got;
        }
        uint8_t d2[32];
        vh.Final(d2);
        if (memcmp(digest, d2, 32) != 0) {
            rq.error = L"Verifica fallita: hash della destinazione diverso da quello del sorgente";
            dst.Close(); DeleteFileW(partial.c_str());
            return XferResult::VerifyError;
        }
    }

    // --- commit atomico ---
    SetFileTime(dst.h, nullptr, nullptr, &mt);
    dst.Close();
    if (!MoveFileExW(partial.c_str(), rq.dst.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        rq.error = Err(L"Commit (rename) fallito", GetLastError());
        return XferResult::IoError;
    }
    Log(UCM_LOG_DEBUG, L"File completato e verificato (sha256 " + rq.sha256Hex + L"): " + rq.dst);
    return XferResult::Ok;
}

}
