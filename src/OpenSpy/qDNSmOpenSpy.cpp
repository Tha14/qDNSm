#include <stddef.h>
#include <sys/prx.h>

#include "OpenSpyProtocol.hpp"
#include "OpenSpyWorker.hpp"

SYS_MODULE_INFO(qDNSmOpenSpy, 0, 1, 1);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

extern "C" int module_start(size_t args, void* argp)
{
	OpenSpyRequest* request = static_cast<OpenSpyRequest*>(argp);

	if ((request == nullptr) ||
		(args != sizeof(OpenSpyRequest)) ||
		(request->magic != kOpenSpyRequestMagic) ||
		(request->version != kOpenSpyRequestVersion))
	{
		return 0;
	}

	request->moduleMagic = kOpenSpyModuleMagic;
	request->moduleVersion = kOpenSpyRequestVersion;
	request->moduleStartSeen = 1;
	request->done = 0;

	request->outcome = RunOpenSpyWorker(request->pid, request->titleId,
		request->fallbackTitleId, &request->stop);

	request->done = 1;
	return 0;
}

extern "C" int module_stop(size_t, void* argp)
{
	OpenSpyRequest* request = static_cast<OpenSpyRequest*>(argp);

	if ((request != nullptr) && (request->magic == kOpenSpyRequestMagic))
		request->moduleStopSeen = 1;

	return 0;
}
