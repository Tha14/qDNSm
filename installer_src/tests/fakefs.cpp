// In-memory stand-in for the cellFs layer, so BootPlugins' logic can be exercised
// on a host machine with no PS3 and no console. Only the four functions
// BootPlugins.cpp actually calls need real behaviour; the rest just satisfy the
// linker.
//
// std:: is fine in here. This file never ships - it is only compiled by
// run-tests.sh with the host g++, where the allocator exists. The constraint
// Small.hpp exists to satisfy applies to code that goes into the EBOOT, which is
// why only the InstFs signatures below had to change.
#include <map>
#include <string>
#include <vector>
#include <stdint.h>
#include <cstring>
#include <cstdio>

#include "InstallerFs.hpp"

namespace FakeFs
{
    std::map<std::string, std::vector<uint8_t> > g_Fs;

    // Fault injection, so the rollback paths can be driven deliberately.
    bool g_FailWrites = false;
    bool g_FailBackup = false;
    bool g_FailStaged = false;
    bool g_CorruptNextWrite = false;
    bool g_TruncateNextWrite = false;
    int  g_WriteCount = 0;
    std::vector<std::string> g_WriteLog;

    void Put(const std::string& path, const std::string& text)
    {
        g_Fs[path] = std::vector<uint8_t>(text.begin(), text.end());
    }

    std::string Get(const std::string& path)
    {
        std::map<std::string, std::vector<uint8_t> >::iterator it = g_Fs.find(path);
        if (it == g_Fs.end())
            return std::string();
        return std::string(it->second.begin(), it->second.end());
    }

    bool Exists(const std::string& path) { return g_Fs.find(path) != g_Fs.end(); }

    void Clear()
    {
        g_Fs.clear();
        g_FailWrites = false;
        g_FailBackup = false;
        g_FailStaged = false;
        g_CorruptNextWrite = false;
        g_TruncateNextWrite = false;
        g_WriteCount = 0;
        g_WriteLog.clear();
    }

    static bool EndsWith(const std::string& s, const std::string& suffix)
    {
        return s.size() >= suffix.size() &&
               s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    static bool Write(const std::string& path, const void* data, size_t size)
    {
        if (EndsWith(path, ".qdnsm-bak") && g_FailBackup)
            return false;

        if (EndsWith(path, ".qdnsm-new") && g_FailStaged)
            return false;

        if (g_FailWrites)
            return false;

        g_WriteCount++;
        g_WriteLog.push_back(path);

        std::vector<uint8_t> bytes((const uint8_t*)data, (const uint8_t*)data + size);

        if (g_TruncateNextWrite && bytes.size() > 1)
            bytes.resize(bytes.size() / 2);

        if (g_CorruptNextWrite && !bytes.empty())
            bytes[bytes.size() - 1] ^= 0xFF;

        g_Fs[path] = bytes;
        return true;
    }
}

namespace InstFs
{
    bool FileExists(const Small::Str& path) { return FakeFs::Exists(path.c()); }

    int64_t FileSize(const Small::Str& path)
    {
        if (!FakeFs::Exists(path.c()))
            return -1;
        return (int64_t)FakeFs::g_Fs[path.c()].size();
    }

    bool ReadBinary(const Small::Str& path, Small::Bytes& out)
    {
        if (!FakeFs::Exists(path.c()))
            return false;

        /* Mirrors the real ReadBinary: a file larger than the destination buffer
         * is refused rather than partially loaded. */
        const std::vector<uint8_t>& src = FakeFs::g_Fs[path.c()];
        if (src.size() > out.capacity())
            return false;

        return out.Assign(src.empty() ? NULL : &src[0], src.size());
    }

    bool ReadText(const Small::Str& path, Small::Bytes& out)
    {
        return ReadBinary(path, out);
    }

    bool WriteBinaryVerified(const Small::Str& path, const uint8_t* data, size_t size)
    {
        if (!FakeFs::Write(path.c(), data, size))
            return false;

        /* Verify against the stored bytes rather than via ReadBinary, so the
         * corruption and truncation faults stay observable: ReadBinary would
         * faithfully return the damaged file and the check would then pass. */
        std::map<std::string, std::vector<uint8_t> >::iterator it = FakeFs::g_Fs.find(path.c());
        if (it == FakeFs::g_Fs.end())
            return false;

        if (it->second.size() != size)
            return false;

        if (size == 0)
            return true;

        return memcmp(&it->second[0], data, size) == 0;
    }

    bool DeleteFile(const Small::Str& path)
    {
        return FakeFs::g_Fs.erase(path.c()) != 0 || true;
    }

    bool EnsureEmptyFile(const Small::Str& path)
    {
        if (FakeFs::Exists(path.c()))
            return true;

        static const uint8_t kNothing = 0;
        return WriteBinaryVerified(path, &kNothing, 0);
    }
}
