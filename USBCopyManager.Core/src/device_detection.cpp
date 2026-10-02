#include "device_detection.h"
#include <winioctl.h>

namespace ucm {

static wstring AsciiToW(const char* s) {
    wstring r;
    while (*s) r.push_back((wchar_t)(unsigned char)*s++);
    return Trim(r);
}

static bool QueryDisk(int n, DeviceInfo& d) {
    wstring path = L"\\\\.\\PhysicalDrive" + std::to_wstring(n);
    HANDLE h = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
    if (h == INVALID_HANDLE_VALUE) return false;

    STORAGE_PROPERTY_QUERY q{};
    q.PropertyId = StorageDeviceProperty;
    q.QueryType = PropertyStandardQuery;
    alignas(8) BYTE buf[2048] = {};
    DWORD ret = 0;
    bool ok = DeviceIoControl(h, IOCTL_STORAGE_QUERY_PROPERTY, &q, sizeof(q), buf, sizeof(buf) - 1, &ret, nullptr) != 0;
    if (ok) {
        auto* desc = reinterpret_cast<STORAGE_DEVICE_DESCRIPTOR*>(buf);
        d.diskNumber = n;
        d.physicalDrive = path;
        d.busType = (int)desc->BusType;
        if (desc->SerialNumberOffset > 0 && desc->SerialNumberOffset < sizeof(buf))
            d.serial = AsciiToW(reinterpret_cast<const char*>(buf) + desc->SerialNumberOffset);
        wstring vendor, product;
        if (desc->VendorIdOffset > 0 && desc->VendorIdOffset < sizeof(buf))
            vendor = AsciiToW(reinterpret_cast<const char*>(buf) + desc->VendorIdOffset);
        if (desc->ProductIdOffset > 0 && desc->ProductIdOffset < sizeof(buf))
            product = AsciiToW(reinterpret_cast<const char*>(buf) + desc->ProductIdOffset);
        d.model = Trim(vendor + L" " + product);

        GET_LENGTH_INFORMATION li{};
        DWORD r2 = 0;
        if (DeviceIoControl(h, IOCTL_DISK_GET_LENGTH_INFO, nullptr, 0, &li, sizeof(li), &r2, nullptr))
            d.size = (uint64_t)li.Length.QuadPart;
    }
    CloseHandle(h);
    return ok;
}

static wstring LetterForDisk(int disk) {
    DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if (!(mask & (1u << i))) continue;
        wchar_t root[4] = { (wchar_t)(L'A' + i), L':', L'\\', 0 };
        UINT t = GetDriveTypeW(root);
        if (t != DRIVE_REMOVABLE && t != DRIVE_FIXED) continue;
        wstring vol = L"\\\\.\\";
        vol += (wchar_t)(L'A' + i);
        vol += L':';
        HANDLE h = CreateFileW(vol.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h == INVALID_HANDLE_VALUE) continue;
        alignas(8) BYTE buf[1024] = {};
        DWORD ret = 0;
        bool match = false;
        if (DeviceIoControl(h, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, nullptr, 0, buf, sizeof(buf), &ret, nullptr)) {
            auto* e = reinterpret_cast<VOLUME_DISK_EXTENTS*>(buf);
            const DISK_EXTENT* ext = e->Extents;
            for (DWORD k = 0; k < e->NumberOfDiskExtents && k < 16; ++k)
                if ((int)ext[k].DiskNumber == disk) { match = true; break; }
        }
        CloseHandle(h);
        if (match && GetVolumeInformationW(root, nullptr, 0, nullptr, nullptr, nullptr, nullptr, 0))
            return wstring(1, (wchar_t)(L'A' + i)) + L":";
    }
    return L"";
}

std::vector<DeviceInfo> EnumerateDevices() {
    std::vector<DeviceInfo> v;
    for (int n = 0; n < 32; ++n) {
        DeviceInfo d;
        if (!QueryDisk(n, d)) continue;
        if (d.serial.empty()) continue;
        d.driveLetter = LetterForDisk(n);
        v.push_back(d);
    }
    return v;
}

bool FindDeviceIn(const std::vector<DeviceInfo>& devs, const wstring& serial, DeviceInfo& out) {
    wstring want = ToUpper(Trim(serial));
    for (const auto& d : devs)
        if (ToUpper(d.serial) == want) { out = d; return true; }
    return false;
}

bool FindDeviceBySerial(const wstring& serial, DeviceInfo& out) {
    return FindDeviceIn(EnumerateDevices(), serial, out);
}

}
