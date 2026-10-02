/*
 * USBCopyManager.Core - API C stabile.
 * Tutte le funzioni restituiscono UcmResult. Le stringhe sono UTF-16 (wchar_t).
 * Le callback vengono invocate da thread del Core: il client deve fare il marshaling
 * verso il proprio thread UI.
 */
#ifndef UCM_API_H
#define UCM_API_H

#include <stdint.h>
#include <wchar.h>

#ifdef _WIN32
  #ifdef UCM_EXPORTS
    #define UCM_API __declspec(dllexport)
  #else
    #define UCM_API __declspec(dllimport)
  #endif
#else
  #define UCM_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define UCM_API_VERSION 1

typedef struct UcmHandle UcmHandle;

/* ---- Codici di errore ---- */
typedef enum UcmResult {
    UCM_OK = 0,
    UCM_E_INVALID_ARGUMENT = 1,
    UCM_E_DEVICE_NOT_FOUND = 2,
    UCM_E_ALREADY_RUNNING = 3,
    UCM_E_NOT_RUNNING = 4,
    UCM_E_IO = 5,
    UCM_E_VERIFICATION = 6,
    UCM_E_INVALID_STATE = 7,
    UCM_E_CANCELLED = 8,
    UCM_E_BUFFER_TOO_SMALL = 9,
    UCM_E_NOT_FOUND = 10
} UcmResult;

typedef enum UcmCopyMode {
    UCM_MODE_SYNCHRONIZED = 0,
    UCM_MODE_BRUTAL = 1
} UcmCopyMode;

typedef enum UcmJobState {
    UCM_JOB_IDLE = 0,
    UCM_JOB_RUNNING = 1,
    UCM_JOB_COMPLETED = 2,
    UCM_JOB_CANCELLED = 3,
    UCM_JOB_INTERRUPTED = 4,
    UCM_JOB_FAILED = 5
} UcmJobState;

typedef enum UcmEventType {
    UCM_EVT_DEVICE_CONNECTED = 1,
    UCM_EVT_DEVICE_REMOVED = 2,
    UCM_EVT_JOB_STARTED = 3,
    UCM_EVT_WORK_STARTED = 4,
    UCM_EVT_PROGRESS = 5,
    UCM_EVT_WORK_COMPLETED = 6,
    UCM_EVT_JOB_COMPLETED = 7,
    UCM_EVT_JOB_CANCELLED = 8,
    UCM_EVT_JOB_INTERRUPTED = 9,
    UCM_EVT_JOB_FAILED = 10
} UcmEventType;

typedef enum UcmLogLevel {
    UCM_LOG_DEBUG = 0,
    UCM_LOG_INFO = 1,
    UCM_LOG_WARNING = 2,
    UCM_LOG_ERROR = 3
} UcmLogLevel;

/* ---- Strutture (gli uint64/double sono sempre per primi per evitare padding ambigui) ---- */
typedef struct UcmProfile {
    wchar_t id[40];            /* generato dal Core se vuoto al salvataggio */
    wchar_t name[64];
    wchar_t serial[64];        /* identita' stabile del dispositivo */
    wchar_t destination[260];
    wchar_t prefix[16];        /* es. "SN_" */
    int32_t digits;            /* es. 5 */
    int32_t mode;              /* UcmCopyMode */
    int32_t lastCopiedWork;    /* ultimo work completato e verificato (0 = nessuno) */
    int32_t verify;            /* 1 = rilettura e confronto hash della destinazione */
    int32_t autoStart;         /* 1 = avvia la copia al collegamento (con monitoraggio attivo) */
} UcmProfile;

typedef struct UcmDeviceInfo {
    uint64_t sizeBytes;
    int32_t diskNumber;
    int32_t busType;
    int32_t connected;         /* 1 se il volume e' montato e leggibile */
    wchar_t serial[64];
    wchar_t model[64];
    wchar_t physicalDrive[32]; /* \\.\PhysicalDriveN */
    wchar_t driveLetter[4];    /* "D:" (vuoto se nessun volume) */
} UcmDeviceInfo;

typedef struct UcmJobInfo {
    uint64_t fileSize;
    uint64_t fileCopied;
    uint64_t totalSize;
    uint64_t totalCopied;
    double   speedBytesPerSec;
    int32_t  state;            /* UcmJobState */
    int32_t  lastCompletedWork;
    int32_t  resultCode;       /* UcmResult dell'ultima terminazione */
    wchar_t  jobId[40];
    wchar_t  profileId[40];
    wchar_t  work[32];
    wchar_t  currentFile[260];
    wchar_t  message[128];
} UcmJobInfo;

typedef void (__cdecl *UcmLogCallback)(int32_t level, const wchar_t* message, void* user);
typedef void (__cdecl *UcmEventCallback)(int32_t eventType, const wchar_t* profileId, const UcmJobInfo* info, void* user);

/* ---- Ciclo di vita ---- */
UCM_API UcmResult ucm_create(UcmHandle** out);
UCM_API void      ucm_destroy(UcmHandle* h);
UCM_API UcmResult ucm_register_callbacks(UcmHandle* h, UcmLogCallback log, UcmEventCallback ev, void* user);
UCM_API const wchar_t* ucm_result_text(int32_t result);

/* ---- Profili ---- */
UCM_API UcmResult ucm_profile_count(UcmHandle* h, int32_t* count);
UCM_API UcmResult ucm_profile_get(UcmHandle* h, int32_t index, UcmProfile* out);
UCM_API UcmResult ucm_profile_save(UcmHandle* h, UcmProfile* inout);
UCM_API UcmResult ucm_profile_delete(UcmHandle* h, const wchar_t* profileId);

/* ---- Rilevamento dispositivi ---- */
UCM_API UcmResult ucm_enumerate_devices(UcmHandle* h, UcmDeviceInfo* buffer, int32_t capacity, int32_t* count);
UCM_API UcmResult ucm_find_device(UcmHandle* h, const wchar_t* serial, UcmDeviceInfo* out);
UCM_API UcmResult ucm_start_monitoring(UcmHandle* h, int32_t intervalMs);
UCM_API UcmResult ucm_stop_monitoring(UcmHandle* h);

/* ---- Work e job ---- */
UCM_API UcmResult ucm_discover_works(UcmHandle* h, const wchar_t* profileId, int32_t* works, int32_t capacity, int32_t* count);
UCM_API UcmResult ucm_start_copy(UcmHandle* h, const wchar_t* profileId);
UCM_API UcmResult ucm_cancel(UcmHandle* h, const wchar_t* profileId);
UCM_API UcmResult ucm_get_job_info(UcmHandle* h, const wchar_t* profileId, UcmJobInfo* out);

#ifdef __cplusplus
}
#endif
#endif /* UCM_API_H */
