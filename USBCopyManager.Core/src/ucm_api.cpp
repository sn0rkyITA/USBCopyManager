#ifndef UCM_EXPORTS
#define UCM_EXPORTS
#endif
#include "ucm_api.h"
#include "engine.h"
#include <algorithm>

struct UcmHandle {
    ucm::Engine engine;
};

using namespace ucm;

#define UCM_TRY try {
#define UCM_CATCH } catch (...) { return UCM_E_IO; }

static void ToC(const Profile& p, UcmProfile& o) {
    memset(&o, 0, sizeof(o));
    CopyStr(o.id, 40, p.id); CopyStr(o.name, 64, p.name); CopyStr(o.serial, 64, p.serial);
    CopyStr(o.destination, 260, p.destination); CopyStr(o.prefix, 16, p.prefix);
    o.digits = p.digits; o.mode = p.mode; o.lastCopiedWork = p.lastCopiedWork; o.verify = p.verify; o.autoStart = p.autoStart;
}

static void FromC(const UcmProfile& o, Profile& p) {
    p.id = o.id; p.name = Trim(o.name); p.serial = ToUpper(Trim(o.serial)); p.destination = Trim(o.destination);
    p.prefix = o.prefix; p.digits = o.digits; p.mode = o.mode; p.lastCopiedWork = o.lastCopiedWork;
    p.verify = o.verify; p.autoStart = o.autoStart;
}

static void DevToC(const DeviceInfo& d, UcmDeviceInfo& o) {
    memset(&o, 0, sizeof(o));
    o.sizeBytes = d.size; o.diskNumber = d.diskNumber; o.busType = d.busType; o.connected = d.driveLetter.empty() ? 0 : 1;
    CopyStr(o.serial, 64, d.serial); CopyStr(o.model, 64, d.model);
    CopyStr(o.physicalDrive, 32, d.physicalDrive); CopyStr(o.driveLetter, 4, d.driveLetter);
}

extern "C" {

UCM_API UcmResult ucm_create(UcmHandle** out) {
    if (!out) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        *out = new UcmHandle();
        return UCM_OK;
    UCM_CATCH
}

UCM_API void ucm_destroy(UcmHandle* h) {
    try { delete h; } catch (...) {}
}

UCM_API UcmResult ucm_register_callbacks(UcmHandle* h, UcmLogCallback log, UcmEventCallback ev, void* user) {
    if (!h) return UCM_E_INVALID_ARGUMENT;
    h->engine.log.SetCallback(log, user);
    h->engine.SetEventCallback(ev, user);
    return UCM_OK;
}

UCM_API const wchar_t* ucm_result_text(int32_t r) {
    switch (r) {
    case UCM_OK: return L"OK";
    case UCM_E_INVALID_ARGUMENT: return L"Argomento non valido";
    case UCM_E_DEVICE_NOT_FOUND: return L"Dispositivo non trovato";
    case UCM_E_ALREADY_RUNNING: return L"Copia gia' in corso";
    case UCM_E_NOT_RUNNING: return L"Nessuna copia in corso";
    case UCM_E_IO: return L"Errore di I/O";
    case UCM_E_VERIFICATION: return L"Errore di verifica";
    case UCM_E_INVALID_STATE: return L"Stato non valido";
    case UCM_E_CANCELLED: return L"Annullato";
    case UCM_E_BUFFER_TOO_SMALL: return L"Buffer troppo piccolo";
    case UCM_E_NOT_FOUND: return L"Elemento non trovato";
    default: return L"Errore sconosciuto";
    }
}

UCM_API UcmResult ucm_profile_count(UcmHandle* h, int32_t* count) {
    if (!h || !count) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        *count = (int32_t)h->engine.profiles.List().size();
        return UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_profile_get(UcmHandle* h, int32_t index, UcmProfile* out) {
    if (!h || !out || index < 0) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        auto v = h->engine.profiles.List();
        if ((size_t)index >= v.size()) return UCM_E_NOT_FOUND;
        ToC(v[index], *out);
        return UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_profile_save(UcmHandle* h, UcmProfile* inout) {
    if (!h || !inout) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        Profile p;
        FromC(*inout, p);
        if (p.name.empty() || p.serial.empty() || p.digits < 1 || p.digits > 9 || (p.mode != UCM_MODE_SYNCHRONIZED && p.mode != UCM_MODE_BRUTAL))
            return UCM_E_INVALID_ARGUMENT;
        if (!h->engine.profiles.Save(p)) return UCM_E_IO;
        ToC(p, *inout);
        return UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_profile_delete(UcmHandle* h, const wchar_t* id) {
    if (!h || !id) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        h->engine.Cancel(id);
        return h->engine.profiles.Delete(id) ? UCM_OK : UCM_E_NOT_FOUND;
    UCM_CATCH
}

UCM_API UcmResult ucm_enumerate_devices(UcmHandle* h, UcmDeviceInfo* buffer, int32_t capacity, int32_t* count) {
    if (!h || !count || (capacity > 0 && !buffer)) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        auto devs = EnumerateDevices();
        *count = (int32_t)devs.size();
        for (int32_t i = 0; i < capacity && (size_t)i < devs.size(); ++i) DevToC(devs[i], buffer[i]);
        return (int32_t)devs.size() > capacity ? UCM_E_BUFFER_TOO_SMALL : UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_find_device(UcmHandle* h, const wchar_t* serial, UcmDeviceInfo* out) {
    if (!h || !serial || !out) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        DeviceInfo d;
        if (!FindDeviceBySerial(serial, d)) return UCM_E_DEVICE_NOT_FOUND;
        DevToC(d, *out);
        return UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_start_monitoring(UcmHandle* h, int32_t intervalMs) {
    if (!h) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        h->engine.StartMonitoring(intervalMs);
        return UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_stop_monitoring(UcmHandle* h) {
    if (!h) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        h->engine.StopMonitoring();
        return UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_discover_works(UcmHandle* h, const wchar_t* id, int32_t* works, int32_t capacity, int32_t* count) {
    if (!h || !id || !count || (capacity > 0 && !works)) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY
        std::vector<int> v;
        int r = h->engine.DiscoverWorkNumbers(id, v);
        if (r != UCM_OK) return (UcmResult)r;
        *count = (int32_t)v.size();
        for (int32_t i = 0; i < capacity && (size_t)i < v.size(); ++i) works[i] = v[i];
        return (int32_t)v.size() > capacity ? UCM_E_BUFFER_TOO_SMALL : UCM_OK;
    UCM_CATCH
}

UCM_API UcmResult ucm_start_copy(UcmHandle* h, const wchar_t* id) {
    if (!h || !id) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY return (UcmResult)h->engine.StartCopy(id); UCM_CATCH
}

UCM_API UcmResult ucm_cancel(UcmHandle* h, const wchar_t* id) {
    if (!h || !id) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY return (UcmResult)h->engine.Cancel(id); UCM_CATCH
}

UCM_API UcmResult ucm_get_job_info(UcmHandle* h, const wchar_t* id, UcmJobInfo* out) {
    if (!h || !id || !out) return UCM_E_INVALID_ARGUMENT;
    UCM_TRY return (UcmResult)h->engine.GetJobInfo(id, *out); UCM_CATCH
}

} // extern "C"
