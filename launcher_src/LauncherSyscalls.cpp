#include "LauncherSyscalls.hpp"

#include <ppu-lv2.h>

namespace LauncherSyscalls
{
    int GetVshPluginInfo(unsigned int slot, char* name, char* filename)
    {
        lv2syscall5(8, 0x7777, 0x0047, slot,
                    (uint64_t)(uintptr_t)name,
                    (uint64_t)(uintptr_t)filename);
        return_to_user_prog(int);
    }

    int LoadVshPlugin(unsigned int slot, const char* path, void* arg, uint32_t argSize)
    {
        lv2syscall5(8, 0x1EE7, slot,
                    (uint64_t)(uintptr_t)path,
                    (uint64_t)(uintptr_t)arg,
                    argSize);
        return_to_user_prog(int);
    }
}
