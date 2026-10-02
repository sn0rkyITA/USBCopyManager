#pragma once
#include "util.h"
#include "sha256.h"
#include "ucm_api.h"

namespace ucm {

struct JournalData {
    wstring jobId, serial, profileId, source, destination, work, currentFile;
    int workNumber = 0;
    int state = UCM_JOB_RUNNING;                 // UcmJobState
    std::vector<wstring> completedFiles;         // file del work corrente gia' verificati e committati
    uint64_t currentOffset = 0;                  // ultimo checkpoint valido del file corrente
    uint64_t srcSize = 0, srcMtime = 0;
    bool hasHash = false;
    Sha256State hash{};
};

// Journal persistente separato dai dati:  journal\<device-id>\<job-id>.journal
class Journal {
public:
    static wstring PathFor(const wstring& serial, const wstring& jobId);
    static bool Save(const JournalData& j);
    static bool Load(const wstring& path, JournalData& j);
    // Il journal piu' recente del profilo, se ancora Running/Interrupted (Running = crash/scollegamento).
    static bool FindResumable(const wstring& serial, const wstring& profileId, JournalData& out);
    static void Prune(const wstring& serial, size_t keep);
};

}
