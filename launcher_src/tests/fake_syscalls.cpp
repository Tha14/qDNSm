#include "LauncherSyscalls.hpp"

#include <string.h>
#include <string>

namespace FakeSys
{
    const int kNoEntry = (int)0x80010006u;

    struct Slot
    {
        int infoResult;
        std::string name;
        std::string filename;
    };

    Slot g_slots[7];

    int g_loadResult = 0;
    unsigned int g_loadSlot = 0;
    std::string g_loadPath;
    int g_loadCalls = 0;

    void Clear()
    {
        for (int slot = 0; slot < 7; ++slot)
        {
            g_slots[slot].infoResult = kNoEntry;
            g_slots[slot].name.clear();
            g_slots[slot].filename.clear();
        }

        g_loadResult = 0;
        g_loadSlot = 0;
        g_loadPath.clear();
        g_loadCalls = 0;
    }

    void SetPlugin(unsigned int slot, const char* name, const char* filename)
    {
        if (slot > 6)
            return;

        g_slots[slot].infoResult = 0;
        g_slots[slot].name = name;
        g_slots[slot].filename = filename;
    }

    void SetInfoError(unsigned int slot, int code)
    {
        if (slot > 6)
            return;

        g_slots[slot].infoResult = code;
        g_slots[slot].name.clear();
        g_slots[slot].filename.clear();
    }

    void SetLoadResult(int code)
    {
        g_loadResult = code;
    }

    int LoadCalls()
    {
        return g_loadCalls;
    }

    unsigned int LoadSlot()
    {
        return g_loadSlot;
    }

    const char* LoadPath()
    {
        return g_loadPath.c_str();
    }
}

namespace LauncherSyscalls
{
    int GetVshPluginInfo(unsigned int slot, char* name, char* filename)
    {
        if (slot < 1 || slot > 6)
            return (int)0x80010002u;

        FakeSys::Slot& state = FakeSys::g_slots[slot];
        if (state.infoResult != 0)
            return state.infoResult;

        memcpy(name, state.name.data(), state.name.size());
        memcpy(filename, state.filename.data(), state.filename.size());
        return 0;
    }

    int LoadVshPlugin(unsigned int slot, const char* path, void* arg, uint32_t argSize)
    {
        (void)arg;
        (void)argSize;

        FakeSys::g_loadSlot = slot;
        FakeSys::g_loadPath = path != NULL ? path : "";
        FakeSys::g_loadCalls++;
        return FakeSys::g_loadResult;
    }
}
