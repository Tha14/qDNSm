#pragma once

#include <stdint.h>

/*
 * Shared by the main plugin and qDNSmOpenSpy. The main plugin owns the game
 * lookup and stop flag; the module only receives a pid and title IDs, then runs
 * the patch synchronously in module_start.
 */
struct OpenSpyRequest
{
	uint32_t magic;
	uint32_t version;
	int32_t pid;

	volatile uint32_t moduleMagic;
	volatile uint32_t moduleVersion;
	volatile uint32_t moduleStartSeen;
	volatile uint32_t moduleStopSeen;
	volatile uint32_t done;
	volatile int32_t outcome;
	volatile uint32_t stop;

	char titleId[10];
	char fallbackTitleId[10];
};

static const uint32_t kOpenSpyRequestMagic = 0x51444E4F; /* 'QDNO' */
static const uint32_t kOpenSpyRequestVersion = 1;
static const uint32_t kOpenSpyModuleMagic = 0x4F535059;  /* 'OSPY' */

enum OpenSpyPatchOutcome
{
	OpenSpyPatch_Success,
	OpenSpyPatch_AlreadyPatched,
	OpenSpyPatch_NotFound,
	OpenSpyPatch_ReadFailed,
	OpenSpyPatch_WriteFailed,
	OpenSpyPatch_VerifyFailed,
	OpenSpyPatch_RollbackFailed,
	OpenSpyPatch_GameExited,
	OpenSpyPatch_TooManyMatches,
	OpenSpyPatch_Unsupported
};
