#include "job.h"
#include "journal.h"
#include "work_discovery.h"
#include <algorithm>

namespace ucm {

Job::Job(Engine& e, const Profile& p, const DeviceInfo& d) : eng_(e), prof_(p), dev_(d) {
    memset(&info_, 0, sizeof(info_));
    CopyStr(info_.profileId, 40, p.id);
    info_.state = UCM_JOB_IDLE;
    info_.lastCompletedWork = p.lastCopiedWork;
}

Job::~Job() {
    cancel_ = true;
    if (th_.joinable()) th_.join();
}

void Job::Start() {
    active_ = true;
    th_ = std::thread([this] { Run(); });
}

UcmJobInfo Job::Snapshot() {
    std::lock_guard<std::mutex> lk(mu_);
    return info_;
}

void Job::Publish(int evt) {
    UcmJobInfo snap;
    { std::lock_guard<std::mutex> lk(mu_); snap = info_; lastPublish_ = GetTickCount64(); }
    eng_.Emit(evt, prof_.id, snap);
}

void Job::OnBytes(uint64_t delta) {
    bool publish = false;
    {
        std::lock_guard<std::mutex> lk(mu_);
        info_.fileCopied += delta;
        info_.totalCopied += delta;
        speedBytes_ += delta;
        ULONGLONG now = GetTickCount64();
        if (lastSpeedTick_ == 0) lastSpeedTick_ = now;
        ULONGLONG dt = now - lastSpeedTick_;
        if (dt >= 1000) {
            double inst = (double)speedBytes_ * 1000.0 / (double)dt;
            info_.speedBytesPerSec = info_.speedBytesPerSec == 0 ? inst : info_.speedBytesPerSec * 0.5 + inst * 0.5;
            speedBytes_ = 0; lastSpeedTick_ = now;
        }
        publish = now - lastPublish_ >= 250;
    }
    if (publish) Publish(UCM_EVT_PROGRESS);
}

void Job::Finish(int state, int result, const wstring& message) {
    int evt = UCM_EVT_JOB_FAILED, lvl = UCM_LOG_ERROR;
    switch (state) {
        case UCM_JOB_COMPLETED:   evt = UCM_EVT_JOB_COMPLETED;   lvl = UCM_LOG_INFO; break;
        case UCM_JOB_CANCELLED:   evt = UCM_EVT_JOB_CANCELLED;   lvl = UCM_LOG_WARNING; break;
        case UCM_JOB_INTERRUPTED: evt = UCM_EVT_JOB_INTERRUPTED; lvl = UCM_LOG_WARNING; break;
        default: break;
    }
    {
        std::lock_guard<std::mutex> lk(mu_);
        info_.state = state;
        info_.resultCode = result;
        info_.speedBytesPerSec = 0;
        CopyStr(info_.message, 128, message);
    }
    active_ = false;                 // prima dell'evento: il client puo' riavviare subito
    eng_.log.Log(lvl, L"Job terminato [" + prof_.name + L"]: " + message);
    Publish(evt);
}

void Job::Run() {
    try {
        RunImpl();
    } catch (const std::exception& ex) {
        Finish(UCM_JOB_FAILED, UCM_E_IO, L"Eccezione: " + Utf8ToW(ex.what()));
    } catch (...) {
        Finish(UCM_JOB_FAILED, UCM_E_IO, L"Errore interno sconosciuto");
    }
    active_ = false;
}

void Job::RunImpl() {
    Logger& log = eng_.log;
    const wstring root = dev_.driveLetter + L"\\";
    const wstring dest = prof_.destination;

    { std::lock_guard<std::mutex> lk(mu_); info_.state = UCM_JOB_RUNNING; }

    // Resume: ultimo journal del profilo ancora Running/Interrupted
    JournalData rj;
    bool haveResume = Journal::FindResumable(dev_.serial, prof_.id, rj);

    // Work discovery + strategia di selezione
    auto works = DiscoverWorks(root, prof_.prefix, prof_.digits);
    auto select = [&](bool useResume) {
        std::vector<Work> r;
        for (auto& w : works) {
            if (prof_.mode == UCM_MODE_SYNCHRONIZED && w.number <= prof_.lastCopiedWork) continue;
            if (useResume && w.number < rj.workNumber) continue;
            r.push_back(w);
        }
        return r;
    };
    auto todo = select(haveResume);
    if (haveResume && (todo.empty() || todo.front().number != rj.workNumber)) {
        log.Log(UCM_LOG_WARNING, L"Il work del job interrotto non e' piu' disponibile: nuovo job");
        haveResume = false;
        todo = select(false);
    }

    JournalData j;
    j.jobId = haveResume ? rj.jobId : NowStamp() + L"_" + NewGuid().substr(0, 8);
    j.serial = dev_.serial; j.profileId = prof_.id; j.source = root; j.destination = dest;
    j.state = UCM_JOB_RUNNING;
    { std::lock_guard<std::mutex> lk(mu_); CopyStr(info_.jobId, 40, j.jobId); }
    log.Log(UCM_LOG_INFO, L"Job " + j.jobId + (haveResume ? L" (ripreso)" : L" (nuovo)") + L" - modalita' " +
                           (prof_.mode == UCM_MODE_BRUTAL ? L"Brutal" : L"Synchronized") + L" - " + std::to_wstring(todo.size()) + L" work da copiare");

    if (!EnsureDir(dest)) { j.state = UCM_JOB_FAILED; Journal::Save(j); Finish(UCM_JOB_FAILED, UCM_E_IO, L"Destinazione non accessibile: " + dest); return; }

    // Piano: albero di file per ogni work + totale
    struct Plan { Work w; Tree t; };
    std::vector<Plan> plans;
    uint64_t total = 0;
    for (auto& w : todo) {
        Plan p; p.w = w;
        if (!ListTree(w.path, p.t)) { j.state = UCM_JOB_INTERRUPTED; Journal::Save(j); Finish(UCM_JOB_INTERRUPTED, UCM_E_IO, L"Impossibile leggere il work " + w.name); return; }
        total += p.t.total;
        plans.push_back(std::move(p));
    }
    { std::lock_guard<std::mutex> lk(mu_); info_.totalSize = total; info_.totalCopied = 0; }
    Journal::Save(j);
    Publish(UCM_EVT_JOB_STARTED);

    if (plans.empty()) {
        j.state = UCM_JOB_COMPLETED; Journal::Save(j);
        Finish(UCM_JOB_COMPLETED, UCM_OK, L"Nessun work nuovo da copiare");
        Journal::Prune(dev_.serial, 20);
        return;
    }

    bool resumeFileUsed = false;
    for (auto& plan : plans) {
        if (cancel_) { j.state = UCM_JOB_CANCELLED; Journal::Save(j); Finish(UCM_JOB_CANCELLED, UCM_E_CANCELLED, L"Copia annullata"); return; }

        const bool resumingThisWork = haveResume && plan.w.number == rj.workNumber;
        j.work = plan.w.name; j.workNumber = plan.w.number;
        j.completedFiles = resumingThisWork ? rj.completedFiles : std::vector<wstring>();
        j.currentFile.clear(); j.currentOffset = 0; j.hasHash = false;
        { std::lock_guard<std::mutex> lk(mu_); CopyStr(info_.work, 32, plan.w.name); }
        log.Log(UCM_LOG_INFO, L"Work " + plan.w.name + L": " + std::to_wstring(plan.t.files.size()) + L" file");
        Publish(UCM_EVT_WORK_STARTED);

        const wstring dstWork = Join(dest, plan.w.name);
        EnsureDir(dstWork);
        for (auto& d : plan.t.dirs) EnsureDir(Join(dstWork, d));

        for (auto& f : plan.t.files) {
            if (cancel_) { j.state = UCM_JOB_CANCELLED; Journal::Save(j); Finish(UCM_JOB_CANCELLED, UCM_E_CANCELLED, L"Copia annullata"); return; }

            const wstring dstFile = Join(dstWork, f.rel);
            // File gia' completato in un tentativo precedente (e ancora presente con la stessa dimensione)
            if (std::find(j.completedFiles.begin(), j.completedFiles.end(), f.rel) != j.completedFiles.end()) {
                WIN32_FILE_ATTRIBUTE_DATA ad{};
                if (GetFileAttributesExW(dstFile.c_str(), GetFileExInfoStandard, &ad) &&
                    (((uint64_t)ad.nFileSizeHigh << 32) | ad.nFileSizeLow) == f.size) {
                    std::lock_guard<std::mutex> lk(mu_);
                    info_.totalCopied += f.size;
                    continue;
                }
                j.completedFiles.erase(std::remove(j.completedFiles.begin(), j.completedFiles.end(), f.rel), j.completedFiles.end());
            }

            { std::lock_guard<std::mutex> lk(mu_); CopyStr(info_.currentFile, 260, f.rel); info_.fileSize = f.size; info_.fileCopied = 0; }

            CopyRequest rq;
            rq.src = Join(plan.w.path, f.rel);
            rq.dst = dstFile;
            rq.verify = prof_.verify != 0;
            rq.cancel = &cancel_;
            if (resumingThisWork && !resumeFileUsed && rj.currentFile == f.rel && rj.hasHash && rj.currentOffset > 0) {
                rq.hasResume = true;
                rq.resumeOffset = rj.currentOffset; rq.resumeSrcSize = rj.srcSize; rq.resumeSrcMtime = rj.srcMtime; rq.resumeHash = rj.hash;
                resumeFileUsed = true;
            }
            j.currentFile = f.rel; j.currentOffset = 0; j.hasHash = false;
            Journal::Save(j);

            rq.onBytes = [this](uint64_t n) { OnBytes(n); };
            rq.onCheckpoint = [&](const Checkpoint& c) {
                j.currentOffset = c.offset; j.srcSize = c.srcSize; j.srcMtime = c.srcMtime; j.hash = c.hash; j.hasHash = true;
                Journal::Save(j);
                log.Log(UCM_LOG_DEBUG, L"Checkpoint " + f.rel + L" @ " + std::to_wstring(c.offset));
            };
            rq.log = [&](int lvl, const wstring& m) { log.Log(lvl, m); };

            XferResult xr = CopyFileResumable(rq);
            switch (xr) {
            case XferResult::Ok:
                j.completedFiles.push_back(f.rel);
                j.currentFile.clear(); j.currentOffset = 0; j.hasHash = false;
                Journal::Save(j);
                break;
            case XferResult::Cancelled:
                j.state = UCM_JOB_CANCELLED; Journal::Save(j);
                Finish(UCM_JOB_CANCELLED, UCM_E_CANCELLED, L"Copia annullata");
                return;
            case XferResult::VerifyError:
                j.state = UCM_JOB_FAILED; Journal::Save(j);
                Finish(UCM_JOB_FAILED, UCM_E_VERIFICATION, rq.error + L": " + f.rel);
                return;
            default: {
                // Sorgente sparito => scollegamento: il job e' riprendibile. Altrimenti errore.
                bool gone = GetFileAttributesW(root.c_str()) == INVALID_FILE_ATTRIBUTES;
                j.state = gone ? UCM_JOB_INTERRUPTED : UCM_JOB_FAILED; Journal::Save(j);
                Finish(j.state, UCM_E_IO, rq.error + L": " + f.rel);
                return;
            }
            }
        }

        // Work completo e verificato: solo ora si aggiorna lo stato persistente
        if (prof_.mode == UCM_MODE_SYNCHRONIZED) {
            prof_.lastCopiedWork = plan.w.number;
            eng_.profiles.SetLastCopied(prof_.id, plan.w.number);
        }
        j.completedFiles.clear(); j.currentFile.clear();
        Journal::Save(j);
        { std::lock_guard<std::mutex> lk(mu_); info_.lastCompletedWork = plan.w.number; }
        log.Log(UCM_LOG_INFO, L"Work completato e verificato: " + plan.w.name);
        Publish(UCM_EVT_WORK_COMPLETED);
    }

    j.state = UCM_JOB_COMPLETED;
    Journal::Save(j);
    Journal::Prune(dev_.serial, 20);
    Finish(UCM_JOB_COMPLETED, UCM_OK, L"Copia completata");
}

}
