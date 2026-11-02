#include "MemoryWarning.hpp"

#include "../Utils/SystemCalls.hpp"

MemoryWarningLevel GetMemoryWarningLevel()
{
	VshMemoryInfo info{};
	GetVshMemoryInfo(&info);

	if (info.total == 0)
		return MemoryWarningLevel::None;

	return ClassifyMemoryWarning(info.available);
}
