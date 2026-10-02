#pragma once
#include "util.h"

namespace ucm {

struct DeviceInfo {
    wstring serial, model, physicalDrive, driveLetter;   // driveLetter es. "D:"
    int diskNumber = -1;
    int busType = 0;
    uint64_t size = 0;
};

// Enumera \\.\PhysicalDrive0..N e legge il seriale hardware; associa la lettera dinamicamente.
std::vector<DeviceInfo> EnumerateDevices();
bool FindDeviceBySerial(const wstring& serial, DeviceInfo& out);                 // volume montato richiesto
bool FindDeviceIn(const std::vector<DeviceInfo>& devs, const wstring& serial, DeviceInfo& out);

}
