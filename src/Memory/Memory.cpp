#include "Memory.hpp"

static volatile uint32_t s_MemoryLock = 0;

void LockGameMemory()
{
   while (__sync_val_compare_and_swap(&s_MemoryLock, 0, 1) != 0)
      sys_ppu_thread_yield();
}

void UnlockGameMemory()
{
   __sync_synchronize();
   s_MemoryLock = 0;
}

uint32_t GetCurrentToc()
{
   uint32_t* entry_point = *reinterpret_cast<uint32_t**>(0x1001C); // ElfHeader->e_entry
   return entry_point[1];
}

int WriteProcessMemory(uint32_t pid, void* address, const void* data, size_t size)
{
   static volatile bool useHen = false;

   LockGameMemory();

   int result = useHen ? -1 :
      sys_dbg_write_process_memory(pid, address, data, size);
   if (result != SUCCEEDED)
   {
      useHen = true;
      result = PS3MAPISetMemory(pid, address, data, size);
   }

   UnlockGameMemory();
   return result;
}

int ReadProcessMemory(uint32_t pid, void* address, void* data, size_t size)
{
   static volatile bool useHen = false;

   LockGameMemory();

   int result = useHen ? -1 :
      sys_dbg_read_process_memory(pid, address, data, size);
   if (result != SUCCEEDED)
   {
      useHen = true;
      result = PS3MAPIGetMemory(pid, address, data, size);
   }

   UnlockGameMemory();
   return result;
}
