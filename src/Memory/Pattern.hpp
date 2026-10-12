#pragma once
#include <stdint.h>

/*
 * Resolves a PPC branch instruction (b / bl) to its absolute target address.
 *
 * Core/Hooks.cpp needs this: the cellPadGetDataExtraInternal entry point inside sys_io is
 * reached through a branch instruction, and Detour has to patch the real destination.
 *
 * This header used to be a full pattern-scanning toolkit - FindPattern, DataCompare,
 * ReadHighLow, FindPatternInTextSegment, FindPatternHypervisor, FindPatternKernel and
 * FindPatternGameProcess. None of that was ever called: it is game-injection scaffolding from
 * the original menu, and qDNSm is only a DNS switcher. It has been removed, and only this one
 * helper is still live.
 */
uint32_t ResolveBranch(uint32_t branchAddress);
