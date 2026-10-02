#pragma once
#include "ucm_api.h"
#include "logger.h"
#include "profile_store.h"
#include "device_detection.h"
#include <condition_variable>
#include <map>
#include <cstring>
#include <memory>
#include <mutex>
#include <thread>
#include <chrono>

namespace ucm {

class Job;

class Engine {
public:
    Engine();
    ~Engine();

    Logger log;
    ProfileStore profiles;

    void SetEventCallback(UcmEventCallback cb, void* user);
    void Emit(int evt, const wstring& profileId, const UcmJobInfo& info);

    int StartCopy(const wstring& profileId);
    int Cancel(const wstring& profileId);
    int GetJobInfo(const wstring& profileId, UcmJobInfo& out);
    int DiscoverWorkNumbers(const wstring& profileId, std::vector<int>& out);

    void StartMonitoring(int intervalMs);
    void StopMonitoring();

private:
    void MonitorLoop(int intervalMs);
    void Tick();

    std::mutex mu_;                                  // jobs_
    std::map<wstring, std::unique_ptr<Job>> jobs_;

    std::mutex cbMu_;
    UcmEventCallback evCb_ = nullptr;
    void* evUser_ = nullptr;

    std::thread monitor_;
    std::mutex monMu_;
    std::condition_variable monCv_;
    bool stopMon_ = false;
    std::map<wstring, bool> present_;                // solo thread monitor
};

}
