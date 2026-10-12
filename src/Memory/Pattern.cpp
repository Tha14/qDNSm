#include "Pattern.hpp"

uint32_t ResolveBranch(uint32_t branchAddress)
{
   uint32_t instruction = *(uint32_t*)(branchAddress);
   int32_t offset = instruction & 0x3FFFFFC;

   if (offset & (1 << 25))
      offset |= ~0x03FFFFFF;

   return branchAddress + offset;
}
