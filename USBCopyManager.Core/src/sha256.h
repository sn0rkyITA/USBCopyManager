#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

namespace ucm {

struct Sha256State {
    uint32_t h[8];
    uint64_t length;   // byte elaborati (multiplo di 64 quando il buffer e' vuoto)
};

class Sha256 {
public:
    Sha256();
    void Update(const uint8_t* data, size_t len);
    void Final(uint8_t out[32]);
    bool GetState(Sha256State& s) const;   // false se il buffer interno non e' vuoto
    void SetState(const Sha256State& s);
    static std::wstring Hex(const uint8_t* d, size_t n);
private:
    void Transform(const uint8_t* block);
    uint32_t h_[8];
    uint64_t len_;
    uint8_t buf_[64];
    size_t bufLen_;
};

}
