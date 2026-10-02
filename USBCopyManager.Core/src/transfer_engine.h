#pragma once
#include "util.h"
#include "sha256.h"
#include <atomic>
#include <functional>

namespace ucm {

struct Checkpoint {
    uint64_t offset = 0, srcSize = 0, srcMtime = 0;
    Sha256State hash{};
};

struct CopyRequest {
    wstring src, dst;                       // dst = percorso definitivo (si scrive su dst + ".partial")
    bool verify = true;
    std::atomic<bool>* cancel = nullptr;
    // Dati di resume dal journal
    bool hasResume = false;
    uint64_t resumeOffset = 0, resumeSrcSize = 0, resumeSrcMtime = 0;
    Sha256State resumeHash{};
    // Callback
    std::function<void(uint64_t)> onBytes;                 // delta di byte (la ripresa riporta l'offset iniziale)
    std::function<void(const Checkpoint&)> onCheckpoint;
    std::function<void(int, const wstring&)> log;
    // Output
    wstring sha256Hex;
    wstring error;
};

enum class XferResult { Ok, Cancelled, IoError, VerifyError };

// Streaming a buffer limitato -> .partial -> checkpoint -> verifica -> commit atomico (rename).
XferResult CopyFileResumable(CopyRequest& rq);

}
