#pragma once
#include "util.h"
#include <mutex>
#include <algorithm>

namespace ucm {

struct Profile {
    wstring id, name, serial, destination;
    wstring prefix = L"SN_";
    int digits = 5;
    int mode = 0;               // UcmCopyMode
    int lastCopiedWork = 0;
    int verify = 1;
    int autoStart = 0;
};

// Persistenza: %LOCALAPPDATA%\USBCopyManager\config\<id>.profile (key=value, UTF-8)
class ProfileStore {
public:
    ProfileStore();
    std::vector<Profile> List();
    bool Get(const wstring& id, Profile& out);
    bool Save(Profile& p);                  // assegna l'id se vuoto
    bool Delete(const wstring& id);
    bool SetLastCopied(const wstring& id, int work);
private:
    bool SaveLocked(Profile& p);
    wstring dir_;
    std::mutex mu_;
};

}
