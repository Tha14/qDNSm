#pragma once
#include <stdint.h>

/* Resolves a PPC b/bl branch to its absolute target address. */
uint32_t ResolveBranch(uint32_t branchAddress);
