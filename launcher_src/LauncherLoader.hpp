#pragma once

namespace LauncherLoader
{
    enum ProbeStatus
    {
        Probe_Ready,
        Probe_AlreadyLoaded,
        Probe_NoFreeSlot,
        Probe_InspectionFailed
    };

    enum LoadStatus
    {
        Load_Ok,
        Load_Unavailable,
        Load_Rejected,
        Load_Missing,
        Load_NoFreeSlot,
        Load_Failed
    };

    struct ProbeResult
    {
        ProbeStatus status;
        unsigned int slot;
        int code;
    };

    struct LoadResult
    {
        LoadStatus status;
        int code;
    };

    ProbeResult Probe();
    LoadResult Load(unsigned int slot, const char* path);
}
