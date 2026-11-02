#pragma once

#include <stdint.h>

enum class MemoryWarningLevel
{
	None,
	Warning,
	Critical
};

static const uint32_t kMemoryWarningBytes = 850 * 1024;
static const uint32_t kMemoryCriticalBytes = 700 * 1024;

inline MemoryWarningLevel ClassifyMemoryWarning(uint32_t availableBytes)
{
	if (availableBytes < kMemoryCriticalBytes)
		return MemoryWarningLevel::Critical;

	if (availableBytes <= kMemoryWarningBytes)
		return MemoryWarningLevel::Warning;

	return MemoryWarningLevel::None;
}
