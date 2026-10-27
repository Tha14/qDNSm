#pragma once

#include <stdint.h>

/*
 * Shared by the main plugin and qDNSmNetLib for the update fetch.
 *
 * The module fetches url/outputPath and, when url2 is non-empty,
 * url2/outputPath2 in one module instance, so the loader is not cycled between
 * the two files. The main plugin calls the module from its own worker thread,
 * so the menu never blocks.
 */
struct NetProbeRequest
{
	uint32_t magic;
	uint32_t version;
	uint32_t job;

	volatile uint32_t moduleMagic;
	volatile uint32_t moduleVersion;
	volatile uint32_t moduleStartSeen;
	volatile uint32_t moduleStopSeen;
	volatile uint32_t done;

	volatile int32_t stage;
	volatile int32_t error;
	volatile int32_t tlsError;
	volatile int32_t netError;
	volatile int32_t httpStatus;
	volatile uint32_t bytes;

	volatile int32_t stage2;
	volatile int32_t error2;
	volatile int32_t tlsError2;
	volatile int32_t netError2;
	volatile int32_t httpStatus2;
	volatile uint32_t bytes2;

	char url[256];
	char outputPath[128];
	char url2[256];
	char outputPath2[128];
};

static const uint32_t kNetProbeMagic = 0x51444E50;         /* 'QDNP' */
static const uint32_t kNetProbeVersion = 3;
static const uint32_t kNetProbeModuleMagic = 0x4E45544C;   /* 'NETL' */

static const uint32_t kNetProbeJobUpdate = 0;

enum
{
	kNetProbeStageStart = 0,
	kNetProbeStageDns = 1,
	kNetProbeStageSocket = 2,
	kNetProbeStageConnect = 3,
	kNetProbeStageTlsConfig = 4,
	kNetProbeStageTlsHandshake = 5,
	kNetProbeStageHttp = 6,
	kNetProbeStageFile = 7,
	kNetProbeStageDone = 8
};

enum
{
	kNetProbeErrorNone = 0,
	kNetProbeErrorArgs = 1,
	kNetProbeErrorMemory = 2,
	kNetProbeErrorDns = 3,
	kNetProbeErrorSocket = 4,
	kNetProbeErrorConnect = 5,
	kNetProbeErrorTlsConfig = 6,
	kNetProbeErrorTlsHandshake = 7,
	kNetProbeErrorHttp = 8,
	kNetProbeErrorFile = 9
};
