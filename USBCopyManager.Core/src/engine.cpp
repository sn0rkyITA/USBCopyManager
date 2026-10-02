#include "engine.h"
#include "job.h"
#include "work_discovery.h"

namespace ucm {

Engine::Engine() {}

Engine::~Engine() {
    StopMonitoring();
    std::lock_guard<std::mutex> lk(mu_);
    for (auto& kv : jobs_) kv.second->Cancel();
    jobs_.clear();                                   // il distruttore del Job attende il thread
}

void Engine::SetEventCallback(UcmEventCallback cb, void* user) {
    std::lock_guard<std::mutex> lk(cbMu_);
    evCb_ = cb; evUser_ = user;
}

void Engine::Emit(int evt, const wstring& profileId, const UcmJobInfo& info) {
    UcmEventCallback cb; void* user;
    { std::lock_guard<std::mutex> lk(cbMu_); cb = evCb_; user = evUser_; }
    if (cb) cb(evt, profileId.c_str(), &info, user);
}

int Engine::StartCopy(const wstring& id) {
    Profile p;
    if (!profiles.Get(id, p)) return UCM_E_NOT_FOUND;
    if (p.destination.empty() || p.serial.empty() || p.digits <= 0) return UCM_E_INVALID_ARGUMENT;
    DeviceInfo d;
    if (!FindDeviceBySerial(p.serial, d) || d.driveLetter.empty()) return UCM_E_DEVICE_NOT_FOUND;

    std::lock_guard<std::mutex> lk(mu_);
    auto it = jobs_.find(id);
    if (it != jobs_.end() && it->second->IsActive()) return UCM_E_ALREADY_RUNNING;
    jobs_[id] = std::make_unique<Job>(*this, p, d);   // l'eventuale job precedente (concluso) viene distrutto
    jobs_[id]->Start();
    return UCM_OK;
}

int Engine::Cancel(const wstring& id) {
    std::lock_guard<std::mutex> lk(mu_);
    auto it = jobs_.find(id);
    if (it == jobs_.end() || !it->second->IsActive()) return UCM_E_NOT_RUNNING;
    it->second->Cancel();
    return UCM_OK;
}

int Engine::GetJobInfo(const wstring& id, UcmJobInfo& out) {
    Profile p;
    if (!profiles.Get(id, p)) return UCM_E_NOT_FOUND;
    std::lock_guard<std::mutex> lk(mu_);
    auto it = jobs_.find(id);
    if (it != jobs_.end()) { out = it->second->Snapshot(); return UCM_OK; }
    memset(&out, 0, sizeof(out));
    out.state = UCM_JOB_IDLE;
    out.lastCompletedWork = p.lastCopiedWork;
    CopyStr(out.profileId, 40, id);
    return UCM_OK;
}

int Engine::DiscoverWorkNumbers(const wstring& id, std::vector<int>& out) {
    Profile p;
    if (!profiles.Get(id, p)) return UCM_E_NOT_FOUND;
    DeviceInfo d;
    if (!FindDeviceBySerial(p.serial, d) || d.driveLetter.empty()) return UCM_E_DEVICE_NOT_FOUND;
    for (auto& w : DiscoverWorks(d.driveLetter + L"\\", p.prefix, p.digits)) out.push_back(w.number);
    return UCM_OK;
}

void Engine::StartMonitoring(int ms) {
    std::lock_guard<std::mutex> lk(monMu_);
    if (monitor_.joinable()) return;
    if (ms < 500) ms = 500;
    stopMon_ = false;
    monitor_ = std::thread([this, ms] { MonitorLoop(ms); });
}

void Engine::StopMonitoring() {
    {
        std::lock_guard<std::mutex> lk(monMu_);
        stopMon_ = true;
    }
    monCv_.notify_all();
    if (monitor_.joinable()) monitor_.join();
}

void Engine::MonitorLoop(int ms) {
    log.Log(UCM_LOG_INFO, L"Monitoraggio dispositivi avviato");
    std::unique_lock<std::mutex> lk(monMu_);
    while (!stopMon_) {
        lk.unlock();
        try { Tick(); } catch (...) { log.Log(UCM_LOG_ERROR, L"Errore nel ciclo di monitoraggio"); }
        lk.lock();
        monCv_.wait_for(lk, std::chrono::milliseconds(ms), [this] { return stopMon_; });
    }
    log.Log(UCM_LOG_INFO, L"Monitoraggio dispositivi fermato");
}

void Engine::Tick() {
    auto devs = EnumerateDevices();
    for (auto& p : profiles.List()) {
        DeviceInfo d;
        bool now = FindDeviceIn(devs, p.serial, d) && !d.driveLetter.empty();
        bool was = present_[p.id];
        present_[p.id] = now;
        if (now == was) continue;
        UcmJobInfo info;
        GetJobInfo(p.id, info);
        if (now) {
            log.Log(UCM_LOG_INFO, L"Dispositivo collegato: " + p.name + L" (" + p.serial + L") su " + d.driveLetter);
            Emit(UCM_EVT_DEVICE_CONNECTED, p.id, info);
            if (p.autoStart) {
                int r = StartCopy(p.id);
                if (r != UCM_OK) log.Log(UCM_LOG_WARNING, L"Avvio automatico non riuscito (codice " + std::to_wstring(r) + L")");
            }
        } else {
            log.Log(UCM_LOG_INFO, L"Dispositivo scollegato: " + p.name + L" (" + p.serial + L")");
            Emit(UCM_EVT_DEVICE_REMOVED, p.id, info);
        }
    }
}

}
