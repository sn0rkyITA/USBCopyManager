#pragma once
#include "engine.h"
#include "transfer_engine.h"
#include <atomic>
#include <mutex>
#include <thread>

namespace ucm {

class Job {
public:
    Job(Engine& engine, const Profile& profile, const DeviceInfo& device);
    ~Job();
    void Start();
    void Cancel() { cancel_ = true; }
    bool IsActive() const { return active_.load(); }
    UcmJobInfo Snapshot();

private:
    void Run();
    void RunImpl();
    void Finish(int state, int result, const wstring& message);
    void Publish(int evt);
    void OnBytes(uint64_t delta);

    Engine& eng_;
    Profile prof_;
    DeviceInfo dev_;
    std::thread th_;
    std::atomic<bool> cancel_{false};
    std::atomic<bool> active_{false};

    std::mutex mu_;
    UcmJobInfo info_{};
    ULONGLONG lastPublish_ = 0;
    ULONGLONG lastSpeedTick_ = 0;
    uint64_t speedBytes_ = 0;
};

}
