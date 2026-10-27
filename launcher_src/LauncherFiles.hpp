#pragma once

#include <stddef.h>

#include "Small.hpp"

namespace LauncherFiles
{
    enum Status
    {
        Ok,
        SourceMissing,
        SourceTooSmall,
        SourceTooLarge,
        ReadFailed,
        WriteFailed,
        InternalError
    };

    Status EnsurePlugin(const Small::Str& sourceDir, const Small::Str& destination, bool& copied);
    Status EnsureGameList(const Small::Str& sourceDir, const Small::Str& destination, bool& copied);
    Status EnsureFile(const Small::Str& sourceDir, const char* sourceName,
                      const Small::Str& destination, size_t minSize, bool& copied);
}
