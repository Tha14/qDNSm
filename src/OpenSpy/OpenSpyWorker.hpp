#pragma once

#include <sys/process.h>
#include <stdint.h>

#include "OpenSpyProtocol.hpp"

OpenSpyPatchOutcome RunOpenSpyWorker(sys_pid_t pid, const char* titleId,
	const char* fallbackTitleId, volatile uint32_t* stop);
