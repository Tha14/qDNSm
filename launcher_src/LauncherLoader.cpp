#include "LauncherLoader.hpp"

#include "LauncherSyscalls.hpp"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace
{
    const unsigned int kFirstSlot = 1;
    const unsigned int kLastSlot = 6;
    const char* const kModuleName = "qDNSm";
    const char* const kFileName = "qDNSm.sprx";

    const uint32_t kCellInvalid = 0x80010002u;
    const uint32_t kCellNoSys = 0x80010003u;
    const uint32_t kCellNoEntry = 0x80010006u;
    const uint32_t kCellResource = 0x80010011u;
    const uint32_t kPrxInvalid = 0x80011324u;
    const uint32_t kPrxUnsupported = 0x80011148u;

    bool IsCode(int code, uint32_t value)
    {
        return (uint32_t)code == value;
    }

    bool IsNoEntry(int code)
    {
        return IsCode(code, kCellNoEntry) || code == 2;
    }

    bool IsUnavailable(int code)
    {
        return IsCode(code, kCellNoSys) || code == 78;
    }

    bool IsInvalid(int code)
    {
        return IsCode(code, kCellInvalid) ||
               IsCode(code, kPrxInvalid) ||
               IsCode(code, kPrxUnsupported) ||
               code == 22;
    }

    bool IsResource(int code)
    {
        return IsCode(code, kCellResource);
    }

    bool BasenameEquals(const char* path, const char* expected)
    {
        if (path == NULL || expected == NULL)
            return false;

        const char* base = path;
        for (const char* cursor = path; *cursor != '\0'; ++cursor)
        {
            if (*cursor == '/')
                base = cursor + 1;
        }

        return strcmp(base, expected) == 0;
    }

    bool IsQdnsm(const char* name, const char* filename)
    {
        return (name != NULL && strcmp(name, kModuleName) == 0) ||
               BasenameEquals(filename, kFileName);
    }
}

namespace LauncherLoader
{
    ProbeResult Probe()
    {
        ProbeResult result = { Probe_InspectionFailed, 0, 0 };

        bool haveFreeSlot = false;
        unsigned int freeSlot = 0;
        int inspectionError = 0;

        for (unsigned int slot = kFirstSlot; slot <= kLastSlot; ++slot)
        {
            char name[64] = { 0 };
            char filename[512] = { 0 };
            const int code = LauncherSyscalls::GetVshPluginInfo(slot, name, filename);

            if (code == 0)
            {
                if (IsQdnsm(name, filename))
                {
                    result.status = Probe_AlreadyLoaded;
                    result.slot = slot;
                    return result;
                }

                continue;
            }

            if (IsNoEntry(code))
            {
                if (!haveFreeSlot)
                {
                    haveFreeSlot = true;
                    freeSlot = slot;
                }

                continue;
            }

            if (inspectionError == 0)
                inspectionError = code;
        }

        if (inspectionError != 0)
        {
            result.status = Probe_InspectionFailed;
            result.code = inspectionError;
            return result;
        }

        if (!haveFreeSlot)
        {
            result.status = Probe_NoFreeSlot;
            return result;
        }

        result.status = Probe_Ready;
        result.slot = freeSlot;
        return result;
    }

    LoadResult Load(unsigned int slot, const char* path)
    {
        LoadResult result = { Load_Failed, 0 };
        const int code = LauncherSyscalls::LoadVshPlugin(slot, path, NULL, 0);

        result.code = code;

        if (code == 0)
            result.status = Load_Ok;
        else if (IsUnavailable(code))
            result.status = Load_Unavailable;
        else if (IsInvalid(code))
            result.status = Load_Rejected;
        else if (IsNoEntry(code))
            result.status = Load_Missing;
        else if (IsResource(code))
            result.status = Load_NoFreeSlot;

        return result;
    }
}
