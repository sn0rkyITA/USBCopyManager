#include "logger.h"

namespace ucm {

void Logger::SetCallback(UcmLogCallback cb, void* user) {
    std::lock_guard<std::mutex> lk(mu_);
    cb_ = cb; user_ = user;
}

void Logger::Log(int level, const wstring& message) {
    UcmLogCallback cb; void* user;
    {
        std::lock_guard<std::mutex> lk(mu_);
        cb = cb_; user = user_;
        static const wchar_t* names[] = { L"DBG", L"INF", L"WRN", L"ERR" };
        wstring line = NowText() + L" [" + names[level < 0 || level > 3 ? 1 : level] + L"] " + message + L"\r\n";
        wstring dir = Join(AppDataDir(), L"logs");
        EnsureDir(dir);
        HANDLE h = CreateFileW(Join(dir, L"ucm.log").c_str(), FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h != INVALID_HANDLE_VALUE) {
            std::string u = WToUtf8(line);
            DWORD wr = 0;
            WriteFile(h, u.data(), (DWORD)u.size(), &wr, nullptr);
            CloseHandle(h);
        }
    }
    if (cb) cb(level, message.c_str(), user);
}

}
