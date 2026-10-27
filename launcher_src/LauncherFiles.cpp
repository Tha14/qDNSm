#include "LauncherFiles.hpp"

#include "InstallerFs.hpp"

namespace
{
    const char* const kPluginSource = "qDNSm.sprx";
    const char* const kGameListSource = "qDNSm-server-games.ini";
    const size_t kMinPluginSize = 4096;
    const size_t kMinGameListSize = 1;

    uint8_t g_file[Small::kMaxFile];
    Small::Bytes g_fileView;
}

namespace LauncherFiles
{
    Status EnsureFile(const Small::Str& sourceDir, const char* sourceName,
                      const Small::Str& destination, size_t minSize, bool& copied)
    {
        copied = false;

        if (InstFs::FileExists(destination))
            return Ok;

        Small::Str source;
        if (!source.Set(sourceDir) || !source.Append(sourceName))
            return InternalError;

        const int64_t size = InstFs::FileSize(source);
        if (size < 0)
            return SourceMissing;
        if (size < (int64_t)minSize)
            return SourceTooSmall;
        if (size > (int64_t)Small::kMaxFile)
            return SourceTooLarge;

        g_fileView.Attach(g_file, sizeof(g_file));
        if (!InstFs::ReadBinary(source, g_fileView))
            return ReadFailed;
        if (g_fileView.size() != (size_t)size)
            return ReadFailed;

        if (!InstFs::WriteBinaryVerified(destination, g_fileView.data(), g_fileView.size()))
        {
            InstFs::DeleteFile(destination);
            return WriteFailed;
        }

        copied = true;
        return Ok;
    }

    Status EnsurePlugin(const Small::Str& sourceDir, const Small::Str& destination, bool& copied)
    {
        return EnsureFile(sourceDir, kPluginSource, destination, kMinPluginSize, copied);
    }

    Status EnsureGameList(const Small::Str& sourceDir, const Small::Str& destination, bool& copied)
    {
        return EnsureFile(sourceDir, kGameListSource, destination, kMinGameListSize, copied);
    }
}
