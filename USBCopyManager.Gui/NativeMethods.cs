using System.Runtime.InteropServices;

namespace USBCopyManager.Gui;

// Mirror di include/ucm_api.h. Layout identico alle struct C.

public static class Ucm
{
    // UcmResult
    public const int Ok = 0;
    // UcmJobState
    public const int JobIdle = 0, JobRunning = 1, JobCompleted = 2, JobCancelled = 3, JobInterrupted = 4, JobFailed = 5;
    // UcmEventType
    public const int EvtDeviceConnected = 1, EvtDeviceRemoved = 2, EvtJobStarted = 3, EvtWorkStarted = 4, EvtProgress = 5,
        EvtWorkCompleted = 6, EvtJobCompleted = 7, EvtJobCancelled = 8, EvtJobInterrupted = 9, EvtJobFailed = 10;
    // UcmCopyMode
    public const int ModeSynchronized = 0, ModeBrutal = 1;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct UcmProfile
{
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)] public string Id;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Name;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Serial;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string Destination;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 16)] public string Prefix;
    public int Digits;
    public int Mode;
    public int LastCopiedWork;
    public int Verify;
    public int AutoStart;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct UcmDeviceInfo
{
    public ulong SizeBytes;
    public int DiskNumber;
    public int BusType;
    public int Connected;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Serial;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)] public string Model;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string PhysicalDrive;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 4)] public string DriveLetter;
}

[StructLayout(LayoutKind.Sequential, CharSet = CharSet.Unicode)]
public struct UcmJobInfo
{
    public ulong FileSize;
    public ulong FileCopied;
    public ulong TotalSize;
    public ulong TotalCopied;
    public double SpeedBytesPerSec;
    public int State;
    public int LastCompletedWork;
    public int ResultCode;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)] public string JobId;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 40)] public string ProfileId;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)] public string Work;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 260)] public string CurrentFile;
    [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 128)] public string Message;
}

[UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
public delegate void UcmLogCallback(int level, [MarshalAs(UnmanagedType.LPWStr)] string message, IntPtr user);

[UnmanagedFunctionPointer(CallingConvention.Cdecl, CharSet = CharSet.Unicode)]
public delegate void UcmEventCallback(int eventType, [MarshalAs(UnmanagedType.LPWStr)] string profileId, ref UcmJobInfo info, IntPtr user);

internal static class Native
{
    private const string Dll = "USBCopyManager.Core.dll";

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int ucm_create(out IntPtr h);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern void ucm_destroy(IntPtr h);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int ucm_register_callbacks(IntPtr h, UcmLogCallback? log, UcmEventCallback? ev, IntPtr user);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern IntPtr ucm_result_text(int result);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int ucm_profile_count(IntPtr h, out int count);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_profile_get(IntPtr h, int index, out UcmProfile p);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_profile_save(IntPtr h, ref UcmProfile p);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_profile_delete(IntPtr h, string id);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_enumerate_devices(IntPtr h, [In, Out] UcmDeviceInfo[] buffer, int capacity, out int count);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_find_device(IntPtr h, string serial, out UcmDeviceInfo d);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int ucm_start_monitoring(IntPtr h, int intervalMs);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl)] public static extern int ucm_stop_monitoring(IntPtr h);

    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_discover_works(IntPtr h, string profileId, [In, Out] int[] works, int capacity, out int count);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_start_copy(IntPtr h, string profileId);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_cancel(IntPtr h, string profileId);
    [DllImport(Dll, CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Unicode)] public static extern int ucm_get_job_info(IntPtr h, string profileId, out UcmJobInfo info);
}

/// <summary>Wrapper gestito sopra l'API C del Core. Le callback arrivano su thread del Core.</summary>
public sealed class UcmCore : IDisposable
{
    private IntPtr _h;
    private readonly UcmLogCallback _logCb;      // campi: evitano che il GC raccolga i delegate
    private readonly UcmEventCallback _evCb;

    public event Action<int, string>? LogReceived;
    public event Action<int, string, UcmJobInfo>? EventReceived;

    public UcmCore()
    {
        Check(Native.ucm_create(out _h));
        _logCb = (lvl, msg, _) => LogReceived?.Invoke(lvl, msg);
        _evCb = (int evt, string pid, ref UcmJobInfo info, IntPtr _) => EventReceived?.Invoke(evt, pid, info);
        Check(Native.ucm_register_callbacks(_h, _logCb, _evCb, IntPtr.Zero));
    }

    public static string Text(int result) => Marshal.PtrToStringUni(Native.ucm_result_text(result)) ?? result.ToString();
    private static void Check(int r) { if (r != Ucm.Ok) throw new InvalidOperationException(Text(r)); }

    public void StartMonitoring(int ms = 2000) => Check(Native.ucm_start_monitoring(_h, ms));

    public List<UcmProfile> GetProfiles()
    {
        var list = new List<UcmProfile>();
        Native.ucm_profile_count(_h, out int n);
        for (int i = 0; i < n; i++)
            if (Native.ucm_profile_get(_h, i, out var p) == Ucm.Ok) list.Add(p);
        return list;
    }

    public UcmProfile SaveProfile(UcmProfile p) { Check(Native.ucm_profile_save(_h, ref p)); return p; }
    public void DeleteProfile(string id) => Native.ucm_profile_delete(_h, id);

    public List<UcmDeviceInfo> EnumerateDevices()
    {
        var buf = new UcmDeviceInfo[32];
        Native.ucm_enumerate_devices(_h, buf, buf.Length, out int n);
        return buf.Take(Math.Min(n, buf.Length)).ToList();
    }

    public UcmDeviceInfo? FindDevice(string serial) =>
        Native.ucm_find_device(_h, serial, out var d) == Ucm.Ok ? d : null;

    public int StartCopy(string profileId) => Native.ucm_start_copy(_h, profileId);
    public int Cancel(string profileId) => Native.ucm_cancel(_h, profileId);

    public UcmJobInfo? GetJob(string profileId) =>
        Native.ucm_get_job_info(_h, profileId, out var j) == Ucm.Ok ? j : null;

    public int[] DiscoverWorks(string profileId)
    {
        var buf = new int[4096];
        return Native.ucm_discover_works(_h, profileId, buf, buf.Length, out int n) == Ucm.Ok
            ? buf.Take(Math.Min(n, buf.Length)).ToArray() : Array.Empty<int>();
    }

    public void Dispose()
    {
        if (_h == IntPtr.Zero) return;
        var h = _h; _h = IntPtr.Zero;
        Native.ucm_register_callbacks(h, null, null, IntPtr.Zero);
        Native.ucm_destroy(h);
    }
}
