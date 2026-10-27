#pragma once

#include <stdint.h>

namespace LauncherSyscalls
{
    int GetVshPluginInfo(unsigned int slot, char* name, char* filename);
    int LoadVshPlugin(unsigned int slot, const char* path, void* arg, uint32_t argSize);
}
