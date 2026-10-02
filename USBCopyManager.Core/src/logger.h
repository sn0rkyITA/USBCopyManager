#pragma once
#include "util.h"
#include "ucm_api.h"
#include <mutex>

namespace ucm {
class Logger {
public:
    void SetCallback(UcmLogCallback cb, void* user);
    void Log(int level, const wstring& message);
private:
    std::mutex mu_;
    UcmLogCallback cb_ = nullptr;
    void* user_ = nullptr;
};
}
