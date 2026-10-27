#include <stddef.h>
#include <string.h>
#include <sys/prx.h>

#include "NetProbeProtocol.hpp"
#include "TlsFetch.hpp"

SYS_MODULE_INFO(qDNSmNetLib, 0, 1, 1);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

extern "C" int module_start(size_t args, void* argp)
{
	NetProbeRequest* request = static_cast<NetProbeRequest*>(argp);

	if ((request != nullptr) &&
		(args == sizeof(NetProbeRequest)) &&
		(request->magic == kNetProbeMagic))
	{
		request->moduleMagic = kNetProbeModuleMagic;
		request->moduleVersion = kNetProbeVersion;
		request->moduleStartSeen = 1;
		request->done = 0;

		if (request->job == kNetProbeJobUpdate)
		{
			RunTlsProbe(*request);

			if (request->url2[0] != '\0')
			{
				NetProbeRequest second{};
				second.magic = kNetProbeMagic;
				second.version = kNetProbeVersion;
				second.job = kNetProbeJobUpdate;

				strncpy(second.url, request->url2, sizeof(second.url) - 1);
				strncpy(second.outputPath, request->outputPath2,
					sizeof(second.outputPath) - 1);
				second.url[sizeof(second.url) - 1] = '\0';
				second.outputPath[sizeof(second.outputPath) - 1] = '\0';

				RunTlsProbe(second);

				request->stage2 = second.stage;
				request->error2 = second.error;
				request->tlsError2 = second.tlsError;
				request->netError2 = second.netError;
				request->httpStatus2 = second.httpStatus;
				request->bytes2 = second.bytes;
			}
		}
		else
		{
			request->stage = kNetProbeStageStart;
			request->error = kNetProbeErrorArgs;
		}

		request->done = 1;
	}

	return 0;
}

extern "C" int module_stop(size_t args, void* argp)
{
	NetProbeRequest* request = static_cast<NetProbeRequest*>(argp);

	if ((request != nullptr) && (request->magic == kNetProbeMagic))
		request->moduleStopSeen = 1;

	return 0;
}
