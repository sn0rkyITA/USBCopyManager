#pragma once
#include "util.h"

namespace ucm {

struct Work {
    int number = 0;
    wstring name;   // es. SN_00050
    wstring path;   // percorso completo sul dispositivo
};

struct FileEntry { wstring rel; uint64_t size = 0; };

struct Tree {
    std::vector<FileEntry> files;   // ordinati
    std::vector<wstring> dirs;      // sottocartelle (anche vuote)
    uint64_t total = 0;
};

// Valido: prefix + esattamente 'digits' cifre.
bool ParseWorkName(const wstring& name, const wstring& prefix, int digits, int& number);
std::vector<Work> DiscoverWorks(const wstring& root, const wstring& prefix, int digits);   // ordinati numericamente
bool ListTree(const wstring& root, Tree& out);

}
