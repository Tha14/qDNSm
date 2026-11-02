#ifndef QDNSM_OPENSPY_LOGGING
#define QDNSM_OPENSPY_LOGGING 0
#endif

#include "OpenSpyPatch.hpp"
#include "OpenSpyPatchPlan.hpp"
#include "OpenSpySupportedTitles.hpp"
#include "InputUi.hpp"

#include "../Memory/AobScan.hpp"
#include "../Memory/Memory.hpp"
#include "../Utils/SystemCalls.hpp"

#if QDNSM_OPENSPY_LOGGING
#include "../Utils/FileSystem.hpp"
#include "../Utils/Timers.hpp"
#endif

#include <vsh/paf.hpp>
#include <vsh/plugins/game_plugin.hpp>
#include <vsh/vshmain.hpp>
#include <vsh/stdc.hpp>

#include <sys/ppu_thread.h>
#include <sys/process.h>

#if QDNSM_OPENSPY_LOGGING
#include <stdarg.h>
#endif

#include <algorithm>
#include <vector>

static const uint8_t kOldDomainText[] = "gamespy.com";
static const uint8_t kNewDomainText[] = "openspy.net";

static const uint8_t kOldKeyText[] =
	"BF05D63E93751AD4A59A4A7389CF0BE8A22CCDEEA1E7F12C062D6E194472EFDA"
	"5184CCECEB4FBADF5EB1D7ABFE91181453972AA971F624AF9BA8F0F82E2869FB7"
	"D44BDE8D56EE50977898F3FEE75869622C4981F07506248BD3D092E8EA05C12B2"
	"FA37881176084C8F8B8756C4722CDC57D2AD28ACD3AD85934FB48D6B2D2027";

static const uint8_t kNewKeyText[] =
	"afb5818995b3708d0656a5bdd20760aee76537907625f6d23f40bf17029e5680"
	"8d36966c0804e1d797e310fedd8c06e6c4121d963863d765811fc9baeb2315c9"
	"a6eaeb125fad694d9ea4d4a928f223d9f4514533f18a5432dd0435c5c6ac8e27"
	"6cf29489cb5ac880f16b0d7832ee927d4e27d622d6a450cd1560d7fa882c6c13";

static const uint8_t kOldAuthPrefixText[] = "https://%s";
static const uint8_t kNewAuthPrefixText[] = "http://%s";
static const uint8_t kAuthServiceText[] = "AuthService";

// Gotham City Impostors gates WBID login behind an auto-skip check. Flip the
// immediate from 0 to 1, but only for Gotham titles.
static const uint8_t kGothamWbidOldBytes[] = {
	0x00, 0x98, 0x7F, 0x00, 0xCE, 0x81, 0x79, 0x00,
	0x00, 0x38, 0xE0, 0x00, 0x00, 0x80, 0xBE, 0x81,
	0xB0, 0x7D, 0x63, 0x5B, 0x78, 0x81, 0x2B, 0x00,
	0x00
};

static const uint8_t kGothamWbidNewBytes[] = {
	0x00, 0x98, 0x7F, 0x00, 0xCE, 0x81, 0x79, 0x00,
	0x00, 0x38, 0xE0, 0x00, 0x01, 0x80, 0xBE, 0x81,
	0xB0, 0x7D, 0x63, 0x5B, 0x78, 0x81, 0x2B, 0x00,
	0x00
};

static const uint32_t kGothamWbidLength =
	sizeof(kGothamWbidOldBytes);

static_assert(sizeof(kOldDomainText) - 1 == 11, "domain length");
static_assert(sizeof(kNewDomainText) - 1 == 11, "new domain length");
static_assert(sizeof(kOldKeyText) - 1 == 256, "old key length");
static_assert(sizeof(kNewKeyText) - 1 == 256, "new key length");
static_assert(sizeof(kOldAuthPrefixText) - 1 == 10, "old auth prefix length");
static_assert(sizeof(kNewAuthPrefixText) - 1 == 9, "new auth prefix length");
static_assert(sizeof(kGothamWbidOldBytes) == sizeof(kGothamWbidNewBytes),
	"WBID patch length");

static const uint8_t kGothamWbidGateOldBytes[] = {
	0xF8, 0x21, 0xFF, 0x71, 0x7C, 0x08, 0x02, 0xA6,
	0xFB, 0xC1, 0x00, 0x80, 0x83, 0xC2, 0x2C, 0x88,
	0xFB, 0xE1, 0x00, 0x88
};

static const uint8_t kGothamWbidGateNewBytes[] = {
	0x38, 0x60, 0x00, 0x01, 0x4E, 0x80, 0x00, 0x20,
	0xFB, 0xC1, 0x00, 0x80, 0x83, 0xC2, 0x2C, 0x88,
	0xFB, 0xE1, 0x00, 0x88
};

static const uint32_t kGothamWbidGateLength =
	sizeof(kGothamWbidGateOldBytes);

static_assert(sizeof(kGothamWbidGateOldBytes) ==
	sizeof(kGothamWbidGateNewBytes), "WBID gate patch length");

static const uint8_t kGothamWbidLoggedInOldBytes[] = {
	0x7D, 0x80, 0x00, 0x26, 0xF8, 0x21, 0xFD, 0x21,
	0xFB, 0xC1, 0x02, 0xD0, 0x83, 0xC2, 0x2C, 0x88,
	0xFB, 0x61, 0x02, 0xB8, 0xFB, 0x21, 0x02, 0xA8,
	0xFB, 0x41, 0x02, 0xB0, 0xFB, 0x81, 0x02, 0xC0,
	0x83, 0x7E, 0x80, 0x00, 0x7C, 0x79, 0x1B, 0x78,
	0xFB, 0xA1, 0x02, 0xC8, 0xFB, 0xE1, 0x02, 0xD8,
	0xFA, 0x81, 0x02, 0x80, 0xFA, 0xA1, 0x02, 0x88,
	0xFA, 0xC1, 0x02, 0x90, 0xFA, 0xE1, 0x02, 0x98,
	0xFB, 0x01, 0x02, 0xA0, 0x7C, 0x9D, 0x23, 0x78,
	0x7C, 0x7F, 0x1B, 0x78, 0x38, 0x81, 0x00, 0x70,
	0x7C, 0xBC, 0x2B, 0x78, 0x7F, 0xBA, 0xEB, 0x78,
	0x91, 0x81, 0x02, 0xE8, 0x7C, 0x08, 0x02, 0xA6,
	0xF8, 0x01, 0x02, 0xF0, 0x81, 0x5B, 0x00, 0x00,
	0x7D, 0x43, 0x53, 0x78, 0x81, 0x2A, 0x00, 0x00,
	0x81, 0x69, 0x01, 0x90, 0x80, 0x0B, 0x00, 0x00,
	0xF8, 0x41, 0x00, 0x28, 0x7C, 0x09, 0x03, 0xA6,
	0x80, 0x4B, 0x00, 0x04, 0x4E, 0x80, 0x04, 0x21,
	0xE8, 0x41, 0x00, 0x28, 0x88, 0x01, 0x00, 0x70,
	0x2F, 0x80, 0x00, 0x00, 0x41, 0x9E, 0x01, 0xB4,
	0x81, 0x3E, 0x80, 0x50, 0x8B, 0x09, 0x6B, 0xB8,
	0x2F, 0x98, 0x00, 0xFF, 0x41, 0x9E, 0x01, 0xAC,
	0x82, 0xFB, 0x00, 0x00, 0x2E, 0x1D, 0x00, 0x00,
	0x3A, 0x81, 0x00, 0x78, 0x81, 0x37, 0x00, 0x00,
	0x92, 0x81, 0x00, 0x74, 0x82, 0xC9, 0x01, 0xB4
};

static const uint8_t kGothamWbidLoggedInNewBytes[] = {
	0x38, 0x60, 0x00, 0x01, 0x4E, 0x80, 0x00, 0x20,
	0xFB, 0xC1, 0x02, 0xD0, 0x83, 0xC2, 0x2C, 0x88,
	0xFB, 0x61, 0x02, 0xB8, 0xFB, 0x21, 0x02, 0xA8,
	0xFB, 0x41, 0x02, 0xB0, 0xFB, 0x81, 0x02, 0xC0,
	0x83, 0x7E, 0x80, 0x00, 0x7C, 0x79, 0x1B, 0x78,
	0xFB, 0xA1, 0x02, 0xC8, 0xFB, 0xE1, 0x02, 0xD8,
	0xFA, 0x81, 0x02, 0x80, 0xFA, 0xA1, 0x02, 0x88,
	0xFA, 0xC1, 0x02, 0x90, 0xFA, 0xE1, 0x02, 0x98,
	0xFB, 0x01, 0x02, 0xA0, 0x7C, 0x9D, 0x23, 0x78,
	0x7C, 0x7F, 0x1B, 0x78, 0x38, 0x81, 0x00, 0x70,
	0x7C, 0xBC, 0x2B, 0x78, 0x7F, 0xBA, 0xEB, 0x78,
	0x91, 0x81, 0x02, 0xE8, 0x7C, 0x08, 0x02, 0xA6,
	0xF8, 0x01, 0x02, 0xF0, 0x81, 0x5B, 0x00, 0x00,
	0x7D, 0x43, 0x53, 0x78, 0x81, 0x2A, 0x00, 0x00,
	0x81, 0x69, 0x01, 0x90, 0x80, 0x0B, 0x00, 0x00,
	0xF8, 0x41, 0x00, 0x28, 0x7C, 0x09, 0x03, 0xA6,
	0x80, 0x4B, 0x00, 0x04, 0x4E, 0x80, 0x04, 0x21,
	0xE8, 0x41, 0x00, 0x28, 0x88, 0x01, 0x00, 0x70,
	0x2F, 0x80, 0x00, 0x00, 0x41, 0x9E, 0x01, 0xB4,
	0x81, 0x3E, 0x80, 0x50, 0x8B, 0x09, 0x6B, 0xB8,
	0x2F, 0x98, 0x00, 0xFF, 0x41, 0x9E, 0x01, 0xAC,
	0x82, 0xFB, 0x00, 0x00, 0x2E, 0x1D, 0x00, 0x00,
	0x3A, 0x81, 0x00, 0x78, 0x81, 0x37, 0x00, 0x00,
	0x92, 0x81, 0x00, 0x74, 0x82, 0xC9, 0x01, 0xB4
};

static const uint8_t kGothamWbidVerifiedOldBytes[] = {
	0xF8, 0x21, 0xFF, 0x71, 0x7C, 0x08, 0x02, 0xA6,
	0xFB, 0xC1, 0x00, 0x80, 0xF8, 0x01, 0x00, 0xA0,
	0x88, 0x03, 0x00, 0x14
};

static const uint8_t kGothamWbidVerifiedNewBytes[] = {
	0x38, 0x60, 0x00, 0x01, 0x4E, 0x80, 0x00, 0x20,
	0xFB, 0xC1, 0x00, 0x80, 0xF8, 0x01, 0x00, 0xA0,
	0x88, 0x03, 0x00, 0x14
};

static const uint8_t kGothamWbidReadyOldBytes[] = {
	0xF8, 0x21, 0xFF, 0x71, 0x7C, 0x08, 0x02, 0xA6,
	0xFB, 0xC1, 0x00, 0x80, 0x83, 0xC2, 0x2C, 0x88,
	0xFB, 0xA1, 0x00, 0x78, 0xF8, 0x01, 0x00, 0xA0,
	0xFB, 0xE1, 0x00, 0x88, 0x81, 0x3E, 0x80, 0x4C,
	0x83, 0xBE, 0x80, 0x50, 0x80, 0x69, 0x00, 0x00,
	0x88, 0x9D, 0x6B, 0xB8, 0x38, 0x63, 0x03, 0x40,
	0x78, 0x63, 0x00, 0x20, 0x48, 0x11, 0x86, 0xE1,
	0x60, 0x00, 0x00, 0x00, 0x7C, 0x7F, 0x1B, 0x78
};

static const uint8_t kGothamWbidReadyNewBytes[] = {
	0x38, 0x60, 0x00, 0x01, 0x4E, 0x80, 0x00, 0x20,
	0xFB, 0xC1, 0x00, 0x80, 0x83, 0xC2, 0x2C, 0x88,
	0xFB, 0xA1, 0x00, 0x78, 0xF8, 0x01, 0x00, 0xA0,
	0xFB, 0xE1, 0x00, 0x88, 0x81, 0x3E, 0x80, 0x4C,
	0x83, 0xBE, 0x80, 0x50, 0x80, 0x69, 0x00, 0x00,
	0x88, 0x9D, 0x6B, 0xB8, 0x38, 0x63, 0x03, 0x40,
	0x78, 0x63, 0x00, 0x20, 0x48, 0x11, 0x86, 0xE1,
	0x60, 0x00, 0x00, 0x00, 0x7C, 0x7F, 0x1B, 0x78
};

struct WbidStatePatch
{
	const uint8_t* oldBytes;
	const uint8_t* newBytes;
	uint32_t length;
	const char* name;
};

static const WbidStatePatch kWbidStatePatches[] = {
	{ kGothamWbidLoggedInOldBytes, kGothamWbidLoggedInNewBytes,
		sizeof(kGothamWbidLoggedInOldBytes), "wbid-logged-in" },
	{ kGothamWbidVerifiedOldBytes, kGothamWbidVerifiedNewBytes,
		sizeof(kGothamWbidVerifiedOldBytes), "wbid-verified" },
	{ kGothamWbidReadyOldBytes, kGothamWbidReadyNewBytes,
		sizeof(kGothamWbidReadyOldBytes), "wbid-ready" }
};

static const uint32_t kWbidStatePatchCount =
	sizeof(kWbidStatePatches) / sizeof(kWbidStatePatches[0]);

static_assert(sizeof(kGothamWbidLoggedInOldBytes) ==
	sizeof(kGothamWbidLoggedInNewBytes), "WBID logged-in patch length");
static_assert(sizeof(kGothamWbidVerifiedOldBytes) ==
	sizeof(kGothamWbidVerifiedNewBytes), "WBID verified patch length");
static_assert(sizeof(kGothamWbidReadyOldBytes) ==
	sizeof(kGothamWbidReadyNewBytes), "WBID ready patch length");

static const uint32_t kReplacementCount = 4;
static const uint32_t kScanChunkSize = 16 * 1024;
static const uint32_t kScanOverlap = 255;
static const uint32_t kScanBufferSize = kScanChunkSize + kScanOverlap + 1;
static const uint32_t kScanThreadStackSize = 0x8000;
static const uint32_t kPatchThreadStackSize = 0x8000;
static const uint32_t kScanPageSize = 4 * 1024;
static const uint32_t kMatchScanCap = kGameSpyMaxMatchesPerPattern + 1;
static const uint32_t kProbeStart = 0x00010000;
static const uint32_t kProbeEnd = 0x40000000;
static const uint32_t kProbeStep = 0x00010000;
static const uint32_t kProbeReadSize = 4;
static const uint32_t kPatchIoChunkSize = 64;
static const uint32_t kFallbackMainStart = 0x00010000;
static const uint32_t kFallbackMainEnd = 0x0FFFFFFF;
static const uint32_t kFallbackModuleStart = 0x10000000;
static const uint32_t kFallbackModuleEnd = 0x1FFFFFFF;

#ifndef QDNSM_OPENSPY_SCAN_THREADS
#define QDNSM_OPENSPY_SCAN_THREADS 2
#endif

static const uint32_t kScanThreadCount = QDNSM_OPENSPY_SCAN_THREADS;
static_assert((kScanThreadCount == 1) || (kScanThreadCount == 2),
	"OpenSpy scanner supports one or two threads");

static int ReadPatchMemory(sys_pid_t pid, uint32_t address,
	uint8_t* data, size_t size);
static int WritePatchMemory(sys_pid_t pid, uint32_t address,
	const uint8_t* data, size_t size);

struct MemoryRange
{
	uint32_t start;
	uint32_t end;
};

struct ScanResult
{
	std::vector<AobMatch> oldMatches[kReplacementCount];
	std::vector<AobMatch> newMatches[kReplacementCount];
	std::vector<AobMatch> gateOldMatches;
	std::vector<AobMatch> gateNewMatches;
	std::vector<AobMatch> wbidStateOldMatches[kWbidStatePatchCount];
	std::vector<AobMatch> wbidStateNewMatches[kWbidStatePatchCount];
	bool tooMany;

	ScanResult() : tooMany(false) {}
};

struct ScanJob
{
	std::vector<AobScanSegment> segments;
	ScanResult result;
	uint8_t* buffer;
	sys_pid_t pid;
	bool scanGothamWbid;
	bool success;
};

static volatile bool s_Running = false;
static volatile bool s_ResultReady = false;
static volatile bool s_StopRequested = false;
static volatile bool s_GameExited = false;
static sys_ppu_thread_t s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
static sys_pid_t s_GamePid = 0;
static bool s_GothamWbidPatch = false;
static OpenSpyPatchOutcome s_Outcome = OpenSpyPatch_NotFound;

struct PatchPattern
{
	const uint8_t* oldBytes;
	const uint8_t* newBytes;
	uint32_t oldLength;
	uint32_t newLength;
};

static const PatchPattern kPatchPatterns[kReplacementCount] = {
	{ kOldDomainText, kNewDomainText,
		sizeof(kOldDomainText) - 1, sizeof(kNewDomainText) - 1 },
	{ kOldKeyText, kNewKeyText,
		sizeof(kOldKeyText) - 1, sizeof(kNewKeyText) - 1 },
	{ kOldAuthPrefixText, kNewAuthPrefixText,
		sizeof(kOldAuthPrefixText) - 1, sizeof(kNewAuthPrefixText) - 1 },
	{ kGothamWbidOldBytes, kGothamWbidNewBytes,
		kGothamWbidLength, kGothamWbidLength }
};

static const char* PatternName(uint32_t index)
{
	static const char* const names[kReplacementCount] = {
		"domain", "key", "auth", "gotham-wbid"
	};

	return (index < kReplacementCount) ? names[index] : "unknown";
}

#if QDNSM_OPENSPY_LOGGING
static std::string OpenSpyLogPath()
{
	return GetCurrentDir() + "qDNSm-openspy.log";
}

static void AppendLog(const char* data, size_t length)
{
	int handle = -1;
	const std::string path = OpenSpyLogPath();

	if (cellFsOpen(path.c_str(),
		CELL_FS_O_CREAT | CELL_FS_O_WRONLY | CELL_FS_O_APPEND,
		&handle, NULL, 0) != CELL_FS_SUCCEEDED)
	{
		return;
	}

	uint64_t written = 0;
	cellFsWrite(handle, data, static_cast<uint64_t>(length), &written);
	cellFsClose(handle);
}

static void OpenSpyLogBegin()
{
	const char header[] = "qDNSm OpenSpy patch log\n";
	AppendLog(header, sizeof(header) - 1);
}

static void OpenSpyLog(const char* format, ...)
{
	char line[512];
	va_list args;
	va_start(args, format);
	int length = vsh::vsnprintf(line, sizeof(line) - 2, format, args);
	va_end(args);

	if (length < 0)
		return;

	if (length > static_cast<int>(sizeof(line) - 2))
		length = static_cast<int>(sizeof(line) - 2);

	line[length++] = '\n';
	line[length] = '\0';

	AppendLog(line, static_cast<size_t>(length));
}

static const char* OutcomeName(OpenSpyPatchOutcome outcome)
{
	static const char* const names[] = {
		"success", "already-patched", "not-found", "read-failed",
		"write-failed", "verify-failed", "rollback-failed", "game-exited",
		"too-many-matches"
	};

	return (outcome >= OpenSpyPatch_Success &&
		outcome <= OpenSpyPatch_TooManyMatches) ? names[outcome] : "unknown";
}

static const char* PlanName(GameSpyPatchPlanResult result)
{
	static const char* const names[] = {
		"ok", "already-patched", "missing-required", "mixed",
		"too-many-matches", "overlap", "invalid"
	};

	return (result >= GameSpyPatchPlan_Ok &&
		result <= GameSpyPatchPlan_Invalid) ? names[result] : "unknown";
}
#endif

#if QDNSM_OPENSPY_LOGGING
#define QDNSM_OPENSPY_LOG(...) OpenSpyLog(__VA_ARGS__)
#define QDNSM_OPENSPY_LOG_BEGIN() OpenSpyLogBegin()
#define QDNSM_OPENSPY_PLAN_TRACE PlanTrace
#else
#define QDNSM_OPENSPY_LOG(...) ((void)0)
#define QDNSM_OPENSPY_LOG_BEGIN() ((void)0)
#define QDNSM_OPENSPY_PLAN_TRACE nullptr
#endif

static bool ReadRunningTitleId(char titleId[10])
{
	paf::View* view = paf::View::Find("game_plugin");
	if (!view)
	{
		QDNSM_OPENSPY_LOG("title-id: game_plugin view not found");
		return false;
	}

	vsh::GamePluginInterface* game =
		view->GetInterface<vsh::GamePluginInterface*>(1);
	if (!game)
	{
		QDNSM_OPENSPY_LOG("title-id: GamePluginInterface not found");
		return false;
	}

	uint8_t info[0x120];
	vsh::memset(info, 0, sizeof(info));

	if (game->GameInfo)
	{
		game->GameInfo(info);

		char candidate[10];
		vsh::memcpy(candidate, info + 0x04, 9);
		candidate[9] = '\0';

		QDNSM_OPENSPY_LOG("title-id: GameInfo candidate=\"%.9s\"", candidate);

		if (NormalizeTitleId(candidate, titleId))
		{
			QDNSM_OPENSPY_LOG("title-id: GameInfo normalized=\"%.9s\"", titleId);
			return true;
		}
	}
	else
	{
		QDNSM_OPENSPY_LOG("title-id: GameInfo callback missing");
	}

	uint8_t fallback[0x20];
	vsh::memset(fallback, 0, sizeof(fallback));

	if (game->GetTitleId)
	{
		game->GetTitleId(fallback);

		char candidate[10];
		vsh::memcpy(candidate, fallback, 9);
		candidate[9] = '\0';

		QDNSM_OPENSPY_LOG("title-id: GetTitleId candidate=\"%.9s\"", candidate);

		if (NormalizeTitleId(candidate, titleId))
		{
			QDNSM_OPENSPY_LOG("title-id: GetTitleId normalized=\"%.9s\"", titleId);
			return true;
		}
	}
	else
	{
		QDNSM_OPENSPY_LOG("title-id: GetTitleId callback missing");
	}

	QDNSM_OPENSPY_LOG("title-id: no valid title ID");
	return false;
}

static void ScanPattern(const uint8_t* data, size_t size, uint32_t baseAddress,
	size_t startLimit, const uint8_t* bytes, uint32_t length,
	const char* name, std::vector<AobMatch>& matches, bool& tooMany)
{
	AobMatch found[kMatchScanCap];
	const AobPattern pattern = { bytes, nullptr, length };

	const size_t count = FindAllMatches(data, size, baseAddress, pattern,
		found, kMatchScanCap, startLimit);

	if (count >= kMatchScanCap)
	{
		QDNSM_OPENSPY_LOG("scan: too many %s base=0x%08X count=%u",
			name, baseAddress, static_cast<unsigned int>(count));
		tooMany = true;
		return;
	}

	if ((matches.size() + count) > kGameSpyMaxMatchesPerPattern)
	{
		QDNSM_OPENSPY_LOG("scan: too many %s base=0x%08X count=%u total=%u",
			name, baseAddress, static_cast<unsigned int>(count),
			static_cast<unsigned int>(matches.size()));
		tooMany = true;
		return;
	}

	for (size_t i = 0; i < count; i++)
		matches.push_back(found[i]);
}

static void ScanAuthCandidates(const uint8_t* data, size_t size,
	uint32_t baseAddress, size_t startLimit,
	std::vector<AobMatch>& matches, bool& tooMany)
{
	const uint32_t prefixLength = kPatchPatterns[2].oldLength;
	const uint32_t serviceLength = sizeof(kAuthServiceText) - 1;
	const size_t maxStringLength = 192;

	if (!data || (size < prefixLength))
		return;

	if ((startLimit == 0) || (startLimit > size))
		startLimit = size;

	const size_t lastStart = size - prefixLength;

	for (size_t offset = 0; (offset <= lastStart) && (offset < startLimit);
		offset++)
	{
		if (vsh::memcmp(data + offset, kOldAuthPrefixText,
			prefixLength) != 0)
		{
			continue;
		}

		size_t limit = size - offset;
		if (limit > maxStringLength)
			limit = maxStringLength;

		size_t length = 0;
		while ((length < limit) && (data[offset + length] != 0))
			length++;

		if (length == limit)
			continue;

		bool hasAuthService = false;

		for (size_t i = prefixLength;
			(i + serviceLength) <= length; i++)
		{
			if (vsh::memcmp(data + offset + i, kAuthServiceText,
				serviceLength) == 0)
			{
				hasAuthService = true;
				break;
			}
		}

		if (!hasAuthService)
			continue;

		if (matches.size() >= kGameSpyMaxMatchesPerPattern)
		{
			QDNSM_OPENSPY_LOG("scan: too many auth base=0x%08X total=%u",
				baseAddress, static_cast<unsigned int>(matches.size()));
			tooMany = true;
			return;
		}

		matches.push_back(AobMatch{ baseAddress + static_cast<uint32_t>(offset),
			static_cast<uint32_t>(offset) });
	}
}

static void ScanBuffer(uint32_t baseAddress, const uint8_t* data, size_t size,
	size_t startLimit, ScanResult& result, bool scanGothamWbid)
{
	for (uint32_t i = 0; (i < kReplacementCount) && !result.tooMany; i++)
	{
		if ((i == 3) && !scanGothamWbid)
			continue;

		if (i == 2)
		{
			ScanAuthCandidates(data, size, baseAddress, startLimit,
				result.oldMatches[2], result.tooMany);
			continue;
		}

		const PatchPattern& pattern = kPatchPatterns[i];

		ScanPattern(data, size, baseAddress, startLimit, pattern.oldBytes,
			pattern.oldLength, PatternName(i), result.oldMatches[i],
			result.tooMany);

		if (result.tooMany)
			return;

		ScanPattern(data, size, baseAddress, startLimit, pattern.newBytes,
			pattern.newLength, PatternName(i), result.newMatches[i],
			result.tooMany);
	}

	if (!scanGothamWbid || result.tooMany)
		return;

	ScanPattern(data, size, baseAddress, startLimit,
		kGothamWbidGateOldBytes, kGothamWbidGateLength, "wbid-gate",
		result.gateOldMatches, result.tooMany);

	if (result.tooMany)
		return;

	ScanPattern(data, size, baseAddress, startLimit,
		kGothamWbidGateNewBytes, kGothamWbidGateLength, "wbid-gate",
		result.gateNewMatches, result.tooMany);

	for (uint32_t i = 0; (i < kWbidStatePatchCount) && !result.tooMany; i++)
	{
		const WbidStatePatch& patch = kWbidStatePatches[i];

		ScanPattern(data, size, baseAddress, startLimit, patch.oldBytes,
			patch.length, patch.name, result.wbidStateOldMatches[i],
			result.tooMany);

		if (result.tooMany)
			return;

		ScanPattern(data, size, baseAddress, startLimit, patch.newBytes,
			patch.length, patch.name, result.wbidStateNewMatches[i],
			result.tooMany);
	}
}

static bool ReadAndScan(sys_pid_t pid, uint32_t address, uint8_t* buffer,
	uint32_t readSize, uint32_t startLimit, ScanResult& result,
	bool scanGothamWbid)
{
	if (readSize == 0)
		return true;

	if (ReadProcessMemory(pid, reinterpret_cast<void*>(address), buffer,
		readSize) != 0)
	{
		return false;
	}

	ScanBuffer(address, buffer, readSize, startLimit, result, scanGothamWbid);
	return true;
}

static bool ScanRegion(sys_pid_t pid, uint32_t start, uint32_t reportEnd,
	uint32_t readEnd, uint8_t* buffer, ScanResult& result,
	bool scanGothamWbid)
{
	if (readEnd < reportEnd)
		readEnd = reportEnd;

	uint32_t address = start;

	while (address < reportEnd)
	{
		if (s_StopRequested)
			return false;

		LockGameMemory();
		const sys_pid_t currentPid = vsh::GetGameProcessId();
		UnlockGameMemory();
		if (currentPid != pid)
		{
			s_GameExited = true;
			return false;
		}

		const uint32_t chunkReportEnd = std::min(address + kScanChunkSize,
			reportEnd);
		const uint32_t chunkReadEnd = std::min(chunkReportEnd + kScanOverlap,
			readEnd);
		const uint32_t startLimit = chunkReportEnd - address;

		if (!ReadAndScan(pid, address, buffer, chunkReadEnd - address,
			startLimit, result, scanGothamWbid))
		{
			uint32_t page = address;

			while (page < chunkReportEnd)
			{
				if (s_StopRequested)
					return false;

				const uint32_t pageReportEnd = std::min(page + kScanPageSize,
					chunkReportEnd);
				const uint32_t pageReadEnd = std::min(pageReportEnd + kScanOverlap,
					readEnd);

				ReadAndScan(pid, page, buffer, pageReadEnd - page,
					pageReportEnd - page, result, scanGothamWbid);

				if (result.tooMany)
					return false;

				page = pageReportEnd;
			}
		}

		if (result.tooMany)
			return false;

		address = chunkReportEnd;
	}

	return true;
}

static AobScanSegment FullSegment(const MemoryRange& range)
{
	return AobScanSegment{ range.start, range.end, range.end };
}

static size_t BuildScanJobs(sys_pid_t pid,
	const std::vector<MemoryRange>& ranges, ScanJob* jobs, size_t maxJobs,
	bool scanGothamWbid)
{
	if (!jobs || (maxJobs == 0) || ranges.empty())
		return 0;

	for (size_t i = 0; i < maxJobs; i++)
	{
		jobs[i].segments.clear();
		jobs[i].result = ScanResult();
		jobs[i].buffer = nullptr;
		jobs[i].pid = pid;
		jobs[i].scanGothamWbid = scanGothamWbid;
		jobs[i].success = false;
	}

	uint64_t totalBytes = 0;

	for (size_t i = 0; i < ranges.size(); i++)
		totalBytes += ranges[i].end - ranges[i].start;

	if (totalBytes == 0)
		return 0;

	if ((maxJobs < 2) || (totalBytes < 2))
	{
		for (size_t i = 0; i < ranges.size(); i++)
			jobs[0].segments.push_back(FullSegment(ranges[i]));

		return 1;
	}

	uint64_t splitTarget = totalBytes / 2;
	bool split = false;

	for (size_t i = 0; i < ranges.size(); i++)
	{
		const uint64_t rangeSize = ranges[i].end - ranges[i].start;

		if (!split && (splitTarget <= rangeSize))
		{
			if (splitTarget < rangeSize)
			{
				const uint32_t splitAddress = ranges[i].start +
					static_cast<uint32_t>(splitTarget);
				AobScanSegment segments[2];
				const size_t segmentCount = BuildScanSegments(ranges[i].start,
					ranges[i].end, splitAddress, kScanOverlap, segments);

				if (segmentCount == 2)
				{
					jobs[0].segments.push_back(segments[0]);
					jobs[1].segments.push_back(segments[1]);
				}
				else
				{
					const size_t job = (splitAddress <= ranges[i].start) ? 1 : 0;
					jobs[job].segments.push_back(segments[0]);
				}
			}
			else
			{
				jobs[0].segments.push_back(FullSegment(ranges[i]));
			}

			split = true;
		}
		else if (split)
		{
			jobs[1].segments.push_back(FullSegment(ranges[i]));
		}
		else
		{
			jobs[0].segments.push_back(FullSegment(ranges[i]));
			splitTarget -= rangeSize;
		}
	}

	if (jobs[0].segments.empty())
	{
		jobs[0].segments.swap(jobs[1].segments);
		jobs[1].segments.clear();
		return 1;
	}

	if (jobs[1].segments.empty())
		return 1;

	return 2;
}

static bool RunScanJob(ScanJob& job)
{
	for (size_t i = 0; i < job.segments.size(); i++)
	{
		const AobScanSegment& segment = job.segments[i];

		if (!ScanRegion(job.pid, segment.start, segment.reportEnd,
			segment.readEnd, job.buffer, job.result, job.scanGothamWbid))
		{
			return false;
		}

		if (job.result.tooMany)
			return false;
	}

	return true;
}

static void ScanJobThread(uint64_t arg)
{
	ScanJob* job = reinterpret_cast<ScanJob*>(
		static_cast<uintptr_t>(arg));
	uint8_t buffer[kScanBufferSize];

	if (job)
	{
		job->buffer = buffer;
		job->success = RunScanJob(*job);
	}

	sys_ppu_thread_exit(0);
}

static size_t RunScanJobs(ScanJob* jobs, size_t jobCount)
{
	if (!jobs || (jobCount == 0))
		return 0;

	uint8_t primaryBuffer[kScanBufferSize];
	const size_t workerCount = jobCount < 2 ? jobCount : 2;
	sys_ppu_thread_t threadIds[2] = {
		SYS_PPU_THREAD_ID_INVALID,
		SYS_PPU_THREAD_ID_INVALID
	};
	bool created[2] = { false, false };
	size_t usedThreads = 1;

	for (size_t i = 1; i < workerCount; i++)
	{
		const int createResult = sys_ppu_thread_create(&threadIds[i],
			ScanJobThread,
			static_cast<uint64_t>(reinterpret_cast<uintptr_t>(&jobs[i])),
			1059, kScanThreadStackSize, SYS_PPU_THREAD_CREATE_JOINABLE, "qDNSmScan");

		if (createResult == 0)
		{
			created[i] = true;
			usedThreads++;
		}
	}

	jobs[0].buffer = primaryBuffer;
	jobs[0].success = RunScanJob(jobs[0]);

	for (size_t i = 1; i < workerCount; i++)
	{
		if (created[i])
		{
			uint64_t exitCode = 0;
			sys_ppu_thread_join(threadIds[i], &exitCode);
		}
		else
		{
			jobs[i].buffer = primaryBuffer;
			jobs[i].success = RunScanJob(jobs[i]);
		}
	}

	return usedThreads;
}

static void MergeScanResult(ScanResult& destination, const ScanResult& source)
{
	if (source.tooMany)
	{
		destination.tooMany = true;
		return;
	}

	const size_t maxMatches = kGameSpyMaxMatchesPerPattern;

	for (uint32_t i = 0; (i < kReplacementCount) && !destination.tooMany; i++)
	{
		if (((destination.oldMatches[i].size() +
			source.oldMatches[i].size()) > maxMatches) ||
			((destination.newMatches[i].size() +
				source.newMatches[i].size()) > maxMatches))
		{
			destination.tooMany = true;
			return;
		}

		destination.oldMatches[i].insert(destination.oldMatches[i].end(),
			source.oldMatches[i].begin(), source.oldMatches[i].end());
		destination.newMatches[i].insert(destination.newMatches[i].end(),
			source.newMatches[i].begin(), source.newMatches[i].end());
	}

	if (((destination.gateOldMatches.size() +
		source.gateOldMatches.size()) > maxMatches) ||
		((destination.gateNewMatches.size() +
			source.gateNewMatches.size()) > maxMatches))
	{
		destination.tooMany = true;
		return;
	}

	destination.gateOldMatches.insert(destination.gateOldMatches.end(),
		source.gateOldMatches.begin(), source.gateOldMatches.end());
	destination.gateNewMatches.insert(destination.gateNewMatches.end(),
		source.gateNewMatches.begin(), source.gateNewMatches.end());

	for (uint32_t i = 0; (i < kWbidStatePatchCount) && !destination.tooMany;
		i++)
	{
		if (((destination.wbidStateOldMatches[i].size() +
			source.wbidStateOldMatches[i].size()) > maxMatches) ||
			((destination.wbidStateNewMatches[i].size() +
				source.wbidStateNewMatches[i].size()) > maxMatches))
		{
			destination.tooMany = true;
			return;
		}

		destination.wbidStateOldMatches[i].insert(
			destination.wbidStateOldMatches[i].end(),
			source.wbidStateOldMatches[i].begin(),
			source.wbidStateOldMatches[i].end());
		destination.wbidStateNewMatches[i].insert(
			destination.wbidStateNewMatches[i].end(),
			source.wbidStateNewMatches[i].begin(),
			source.wbidStateNewMatches[i].end());
	}

}

static void SortAndMergeRanges(std::vector<MemoryRange>& ranges)
{
	if (ranges.empty())
		return;

	std::sort(ranges.begin(), ranges.end(),
		[](const MemoryRange& left, const MemoryRange& right)
		{
			return left.start < right.start;
		});

	size_t writeIndex = 1;

	for (size_t i = 1; i < ranges.size(); i++)
	{
		MemoryRange& previous = ranges[writeIndex - 1];

		if (ranges[i].start > previous.end)
			ranges[writeIndex++] = ranges[i];
		else if (ranges[i].end > previous.end)
			previous.end = ranges[i].end;
	}

	ranges.resize(writeIndex);
}

static bool CollectReadableRanges(sys_pid_t pid, std::vector<MemoryRange>& ranges)
{
	uint8_t probe[kProbeReadSize];
#if QDNSM_OPENSPY_LOGGING
	uint32_t probedBlocks = 0;
	uint32_t readableBlocks = 0;
#endif

	for (uint32_t address = kProbeStart; address < kProbeEnd;
		address += kProbeStep)
	{
		if (s_StopRequested)
		{
			QDNSM_OPENSPY_LOG("ranges: probe stopped at 0x%08X", address);
			return false;
		}

		const sys_pid_t currentPid = vsh::GetGameProcessId();
		if (currentPid != pid)
		{
			QDNSM_OPENSPY_LOG("ranges: game exited during probe at 0x%08X (pid=%d, now=%d)",
				address, static_cast<int>(pid), static_cast<int>(currentPid));
			s_GameExited = true;
			return false;
		}

#if QDNSM_OPENSPY_LOGGING
		probedBlocks++;
#endif

		if (ReadProcessMemory(pid, reinterpret_cast<void*>(address), probe,
			sizeof(probe)) == 0)
		{
			ranges.push_back(MemoryRange{ address, address + kProbeStep });
#if QDNSM_OPENSPY_LOGGING
			readableBlocks++;
#endif
		}
	}

	QDNSM_OPENSPY_LOG("ranges: probe blocks=%u readable=%u",
		static_cast<unsigned int>(probedBlocks),
		static_cast<unsigned int>(readableBlocks));

	SortAndMergeRanges(ranges);
	return !ranges.empty();
}

static void AddFallbackRanges(std::vector<MemoryRange>& ranges)
{
	ranges.clear();
	ranges.push_back(MemoryRange{ kFallbackMainStart, kFallbackMainEnd });
	ranges.push_back(MemoryRange{ kFallbackModuleStart, kFallbackModuleEnd });
}

static bool AddressInRanges(uint32_t address,
	const std::vector<MemoryRange>& ranges)
{
	for (size_t i = 0; i < ranges.size(); i++)
	{
		if ((address >= ranges[i].start) && (address < ranges[i].end))
			return true;
	}

	return false;
}

static void RemoveMatchesInRanges(std::vector<AobMatch>& matches,
	const std::vector<MemoryRange>& ranges)
{
	matches.erase(std::remove_if(matches.begin(), matches.end(),
		[&](const AobMatch& match)
		{
			return AddressInRanges(match.address, ranges);
		}), matches.end());
}

static bool BuildAuthPatchEntry(sys_pid_t pid, uint32_t address,
	std::vector<GameSpyPatchEntry>& entries,
	std::vector<MemoryRange>& ranges)
{
	uint8_t oldAuth[192];
	const uint32_t oldPrefixLength = kPatchPatterns[2].oldLength;

	if (ReadPatchMemory(pid, address, oldAuth,
		sizeof(oldAuth)) != 0)
	{
		QDNSM_OPENSPY_LOG("worker: auth full read failed at 0x%08X", address);
		return false;
	}

	uint32_t oldLength = 0;
	while ((oldLength < sizeof(oldAuth)) && (oldAuth[oldLength] != 0))
		oldLength++;

	if (oldLength == sizeof(oldAuth))
	{
		QDNSM_OPENSPY_LOG("worker: auth string unterminated at 0x%08X", address);
		return false;
	}

	if ((oldLength < oldPrefixLength) ||
		(vsh::memcmp(oldAuth, kOldAuthPrefixText, oldPrefixLength) != 0))
	{
		QDNSM_OPENSPY_LOG("worker: auth prefix mismatch at 0x%08X", address);
		return false;
	}

	bool hasAuthService = false;
	const uint32_t authServiceLength = sizeof(kAuthServiceText) - 1;

	for (uint32_t i = oldPrefixLength;
		(i + authServiceLength) <= oldLength; i++)
	{
		if (vsh::memcmp(oldAuth + i, kAuthServiceText,
			authServiceLength) == 0)
		{
			hasAuthService = true;
			break;
		}
	}

	if (!hasAuthService)
	{
		QDNSM_OPENSPY_LOG("worker: auth candidate skipped at 0x%08X len=%u",
			address, oldLength);
		return true;
	}

	uint8_t newAuth[192];
	vsh::memcpy(newAuth, oldAuth, oldLength);
	vsh::memmove(newAuth + 7, newAuth + 8, oldLength - 8);
	vsh::memcpy(newAuth, "http://", 7);
	newAuth[oldLength - 1] = 0;

	const uint32_t oldDomainLength = sizeof(kOldDomainText) - 1;
	const uint32_t newDomainLength = sizeof(kNewDomainText) - 1;
	const uint32_t newTextLength = oldLength - 1;

	for (uint32_t i = 0; (i + oldDomainLength) <= newTextLength;)
	{
		if (vsh::memcmp(newAuth + i, kOldDomainText, oldDomainLength) == 0)
		{
			vsh::memcpy(newAuth + i, kNewDomainText, newDomainLength);
			i += oldDomainLength;
		}
		else
		{
			i++;
		}
	}

	GameSpyPatchEntry entry;
	vsh::memset(&entry, 0, sizeof(entry));
	entry.address = address;
	entry.oldLength = oldLength;
	entry.newLength = oldLength;
	vsh::memcpy(entry.oldBytes, oldAuth, oldLength);
	vsh::memcpy(entry.newBytes, newAuth, oldLength);
	entries.push_back(entry);

	ranges.push_back(MemoryRange{ address, address + oldLength });

	QDNSM_OPENSPY_LOG("worker: auth url address=0x%08X len=%u",
		address, oldLength);
	return true;
}

static int ReadPatchMemory(sys_pid_t pid, uint32_t address,
	uint8_t* data, size_t size)
{
	for (size_t done = 0; done < size;)
	{
		const size_t chunk = std::min<size_t>(size - done, kPatchIoChunkSize);
		const uint32_t current = address + static_cast<uint32_t>(done);

		int result = sys_dbg_read_process_memory(pid,
			reinterpret_cast<void*>(current), data + done, chunk);
		QDNSM_OPENSPY_LOG("read: sys_dbg addr=0x%08X len=%u rc=%d",
			current, static_cast<unsigned int>(chunk), result);

		if (result != SUCCEEDED)
		{
			result = PS3MAPIGetMemory(pid, reinterpret_cast<void*>(current),
				data + done, chunk);
			QDNSM_OPENSPY_LOG("read: ps3mapi addr=0x%08X len=%u rc=%d",
				current, static_cast<unsigned int>(chunk), result);
		}

		if (result != SUCCEEDED)
			return result;

		done += chunk;
	}

	return SUCCEEDED;
}

static int WritePatchMemory(sys_pid_t pid, uint32_t address,
	const uint8_t* data, size_t size)
{
	for (size_t done = 0; done < size;)
	{
		const size_t chunk = std::min<size_t>(size - done, kPatchIoChunkSize);
		const uint32_t current = address + static_cast<uint32_t>(done);

		int result = sys_dbg_write_process_memory(pid,
			reinterpret_cast<void*>(current), data + done, chunk);
		QDNSM_OPENSPY_LOG("write: sys_dbg addr=0x%08X len=%u rc=%d",
			current, static_cast<unsigned int>(chunk), result);

		if (result != SUCCEEDED)
		{
			result = PS3MAPISetMemory(pid, reinterpret_cast<void*>(current),
				data + done, chunk);
			QDNSM_OPENSPY_LOG("write: ps3mapi addr=0x%08X len=%u rc=%d",
				current, static_cast<unsigned int>(chunk), result);
		}

		if (result != SUCCEEDED)
			return result;

		done += chunk;
	}

	return SUCCEEDED;
}

static bool RollbackPatches(sys_pid_t pid,
	const std::vector<GameSpyPatchEntry>& entries, size_t count)
{
	bool ok = true;

	for (size_t i = count; i > 0; i--)
	{
		const GameSpyPatchEntry& entry = entries[i - 1];

		if (WritePatchMemory(pid, entry.address,
			entry.oldBytes, entry.oldLength) != 0)
		{
			QDNSM_OPENSPY_LOG("rollback: write failed address=0x%08X len=%u",
				entry.address, entry.oldLength);
			ok = false;
		}
	}

	return ok;
}

static OpenSpyPatchOutcome FailWithRollback(sys_pid_t pid,
	const std::vector<GameSpyPatchEntry>& entries, size_t count,
	OpenSpyPatchOutcome outcome)
{
	return RollbackPatches(pid, entries, count) ? outcome :
		OpenSpyPatch_RollbackFailed;
}

static OpenSpyPatchOutcome ApplyPatchPlan(sys_pid_t pid,
	const std::vector<GameSpyPatchEntry>& entries)
{
	uint8_t current[256];

	QDNSM_OPENSPY_LOG("apply: begin entries=%u",
		static_cast<unsigned int>(entries.size()));

	for (size_t i = 0; i < entries.size(); i++)
	{
		const GameSpyPatchEntry& entry = entries[i];

		if (ReadPatchMemory(pid, entry.address,
			current, entry.oldLength) != 0)
		{
			QDNSM_OPENSPY_LOG("apply: old read failed index=%u address=0x%08X len=%u",
				static_cast<unsigned int>(i), entry.address, entry.oldLength);
			return OpenSpyPatch_ReadFailed;
		}

		if (vsh::memcmp(current, entry.oldBytes, entry.oldLength) != 0)
		{
			QDNSM_OPENSPY_LOG("apply: old bytes mismatch index=%u address=0x%08X len=%u",
				static_cast<unsigned int>(i), entry.address, entry.oldLength);
			return OpenSpyPatch_VerifyFailed;
		}
	}

	for (size_t i = 0; i < entries.size(); i++)
	{
		const sys_pid_t currentPid = vsh::GetGameProcessId();
		if ((s_StopRequested) || (currentPid != pid))
		{
			QDNSM_OPENSPY_LOG("apply: abort before write index=%u stop=%d pid=%d now=%d",
				static_cast<unsigned int>(i), s_StopRequested ? 1 : 0,
				static_cast<int>(pid), static_cast<int>(currentPid));

			return FailWithRollback(pid, entries, i,
				OpenSpyPatch_WriteFailed);
		}

		const GameSpyPatchEntry& entry = entries[i];

		QDNSM_OPENSPY_LOG("apply: writing index=%u address=0x%08X len=%u",
			static_cast<unsigned int>(i), entry.address, entry.newLength);

		if (WritePatchMemory(pid, entry.address,
			entry.newBytes, entry.newLength) != 0)
		{
			QDNSM_OPENSPY_LOG("apply: write failed index=%u address=0x%08X len=%u",
				static_cast<unsigned int>(i), entry.address, entry.newLength);

			return FailWithRollback(pid, entries, i + 1,
				OpenSpyPatch_WriteFailed);
		}

		QDNSM_OPENSPY_LOG("apply: wrote index=%u address=0x%08X len=%u",
			static_cast<unsigned int>(i), entry.address, entry.newLength);
	}

	for (size_t i = 0; i < entries.size(); i++)
	{
		const GameSpyPatchEntry& entry = entries[i];

		if ((ReadPatchMemory(pid, entry.address,
			current, entry.newLength) != 0) ||
			(vsh::memcmp(current, entry.newBytes, entry.newLength) != 0))
		{
			QDNSM_OPENSPY_LOG("apply: new bytes verify failed index=%u address=0x%08X len=%u",
				static_cast<unsigned int>(i), entry.address, entry.newLength);

			return FailWithRollback(pid, entries, entries.size(),
				OpenSpyPatch_VerifyFailed);
		}
	}

	QDNSM_OPENSPY_LOG("apply: success entries=%u",
		static_cast<unsigned int>(entries.size()));
	return OpenSpyPatch_Success;
}

#if QDNSM_OPENSPY_LOGGING
static void PlanTrace(const char* stage)
{
	QDNSM_OPENSPY_LOG("worker: plan trace %s", stage);
}
#endif

static OpenSpyPatchOutcome RunPatchWorker(sys_pid_t pid, bool gothamWbid)
{
	QDNSM_OPENSPY_LOG("worker: begin pid=%d gotham-wbid=%d",
		static_cast<int>(pid), gothamWbid ? 1 : 0);

#if QDNSM_OPENSPY_LOGGING
	const uint64_t probeStart = GetTimeNow();
#endif
	std::vector<MemoryRange> ranges;
	if (!CollectReadableRanges(pid, ranges))
	{
		if (s_GameExited)
			return OpenSpyPatch_GameExited;

		if (s_StopRequested)
			return OpenSpyPatch_ReadFailed;

		QDNSM_OPENSPY_LOG("worker: no readable ranges from probe, using fallback");
		AddFallbackRanges(ranges);
	}

	QDNSM_OPENSPY_LOG("worker: probe ticks=%llu ranges=%u",
		static_cast<unsigned long long>(GetTimeNow() - probeStart),
		static_cast<unsigned int>(ranges.size()));

	for (size_t i = 0; i < ranges.size(); i++)
	{
		QDNSM_OPENSPY_LOG("worker: range[%u]=0x%08X-0x%08X",
			static_cast<unsigned int>(i), ranges[i].start, ranges[i].end);
	}

	std::vector<GameSpyPatchEntry> entries;
	std::vector<GameSpyPatchEntry> authEntries;
	std::vector<MemoryRange> authRanges;

	ScanJob jobs[kScanThreadCount];
	const size_t jobCount = BuildScanJobs(pid, ranges, jobs, kScanThreadCount,
		gothamWbid);

	if (jobCount == 0)
	{
		QDNSM_OPENSPY_LOG("worker: no scan jobs");
		return OpenSpyPatch_ReadFailed;
	}

	std::vector<MemoryRange>().swap(ranges);

	QDNSM_OPENSPY_LOG("worker: scan begin threads=%u jobs=%u",
		static_cast<unsigned int>(kScanThreadCount),
		static_cast<unsigned int>(jobCount));

#if QDNSM_OPENSPY_LOGGING
	const uint64_t scanStart = GetTimeNow();
#endif
	const size_t usedThreads = RunScanJobs(jobs, jobCount);
#if QDNSM_OPENSPY_LOGGING
	const uint64_t scanTicks = GetTimeNow() - scanStart;
#else
	(void)usedThreads;
#endif

	for (size_t i = 0; i < jobCount; i++)
		std::vector<AobScanSegment>().swap(jobs[i].segments);

	ScanResult scan;
	scan.tooMany = false;
	bool scanOk = true;

	for (size_t i = 0; i < jobCount; i++)
	{
		if (!jobs[i].success)
			scanOk = false;

		QDNSM_OPENSPY_LOG("worker: job[%u] success=%d tooMany=%d old=%u/%u/%u/%u new=%u/%u/%u/%u",
			static_cast<unsigned int>(i), jobs[i].success ? 1 : 0,
			jobs[i].result.tooMany ? 1 : 0,
			static_cast<unsigned int>(jobs[i].result.oldMatches[0].size()),
			static_cast<unsigned int>(jobs[i].result.oldMatches[1].size()),
			static_cast<unsigned int>(jobs[i].result.oldMatches[2].size()),
			static_cast<unsigned int>(jobs[i].result.oldMatches[3].size()),
			static_cast<unsigned int>(jobs[i].result.newMatches[0].size()),
			static_cast<unsigned int>(jobs[i].result.newMatches[1].size()),
			static_cast<unsigned int>(jobs[i].result.newMatches[2].size()),
			static_cast<unsigned int>(jobs[i].result.newMatches[3].size()));

		MergeScanResult(scan, jobs[i].result);
	}

	QDNSM_OPENSPY_LOG("worker: scan end ticks=%llu threads=%u jobs=%u ok=%d tooMany=%d",
		static_cast<unsigned long long>(scanTicks),
		static_cast<unsigned int>(usedThreads),
		static_cast<unsigned int>(jobCount), scanOk ? 1 : 0,
		scan.tooMany ? 1 : 0);

	if (!scanOk)
	{
		if (s_GameExited)
		{
			QDNSM_OPENSPY_LOG("worker: game exited during scan");
			return OpenSpyPatch_GameExited;
		}

		if (scan.tooMany)
		{
			QDNSM_OPENSPY_LOG("worker: too many matches during scan");
			return OpenSpyPatch_TooManyMatches;
		}

		QDNSM_OPENSPY_LOG("worker: read failed during scan");
		return OpenSpyPatch_ReadFailed;
	}

	if (scan.tooMany)
	{
		QDNSM_OPENSPY_LOG("worker: too many matches after scan");
		return OpenSpyPatch_TooManyMatches;
	}

	authEntries.reserve(4);

	for (size_t i = 0; i < scan.oldMatches[2].size(); i++)
	{
		if (!BuildAuthPatchEntry(pid, scan.oldMatches[2][i].address,
			authEntries, authRanges))
		{
			if (vsh::GetGameProcessId() != pid)
			{
				s_GameExited = true;
				return OpenSpyPatch_GameExited;
			}

			return OpenSpyPatch_ReadFailed;
		}
	}

	if (!authRanges.empty())
	{
		RemoveMatchesInRanges(scan.oldMatches[0], authRanges);
		RemoveMatchesInRanges(scan.newMatches[0], authRanges);
	}

	uint32_t oldAddresses[kReplacementCount][kMatchScanCap];
	uint32_t newAddresses[kReplacementCount][kMatchScanCap];
	GameSpyReplacement replacements[kReplacementCount];
	GameSpyReplacementMatches matches[kReplacementCount];

	for (uint32_t i = 0; i < kReplacementCount; i++)
	{
		const uint32_t oldCount = scan.oldMatches[i].size();
		const uint32_t newCount = scan.newMatches[i].size();

		for (uint32_t j = 0; j < oldCount; j++)
			oldAddresses[i][j] = scan.oldMatches[i][j].address;

		for (uint32_t j = 0; j < newCount; j++)
			newAddresses[i][j] = scan.newMatches[i][j].address;

		const PatchPattern& pattern = kPatchPatterns[i];
		replacements[i].oldBytes = pattern.oldBytes;
		replacements[i].newBytes = pattern.newBytes;
		replacements[i].oldLength = pattern.oldLength;
		replacements[i].newLength = pattern.newLength;
		replacements[i].required = (i == 0);

		matches[i].oldAddresses = oldAddresses[i];
		matches[i].oldCount = oldCount;
		matches[i].newAddresses = newAddresses[i];
		matches[i].newCount = newCount;

		QDNSM_OPENSPY_LOG("matches[%u] %s old=%u new=%u oldLen=%u newLen=%u",
			static_cast<unsigned int>(i), PatternName(i), oldCount, newCount,
			replacements[i].oldLength, replacements[i].newLength);

		for (uint32_t j = 0; j < oldCount; j++)
		{
			QDNSM_OPENSPY_LOG("matches[%u] old[%u]=0x%08X",
				static_cast<unsigned int>(i), j, oldAddresses[i][j]);
		}

		for (uint32_t j = 0; j < newCount; j++)
		{
			QDNSM_OPENSPY_LOG("matches[%u] new[%u]=0x%08X",
				static_cast<unsigned int>(i), j, newAddresses[i][j]);
		}

		if ((i == 1) && (oldCount == 0) && (newCount == 0))
		{
			QDNSM_OPENSPY_LOG("worker: public key not present, skipping key replacement");
		}
	}

	bool alreadyPatched = false;

	QDNSM_OPENSPY_LOG("worker: plan begin authEntries=%u",
		static_cast<unsigned int>(authEntries.size()));

	size_t expectedEntries = authEntries.size();

	for (uint32_t i = 0; i < 2; i++)
		expectedEntries += scan.oldMatches[i].size() + scan.newMatches[i].size();

	if (gothamWbid)
	{
		expectedEntries += scan.oldMatches[3].size();
		expectedEntries += scan.gateOldMatches.size();

		for (uint32_t i = 0; i < kWbidStatePatchCount; i++)
			expectedEntries += scan.wbidStateOldMatches[i].size();
	}

	if (expectedEntries > kGameSpyMaxPatchEntries)
		expectedEntries = kGameSpyMaxPatchEntries;

	entries.reserve(expectedEntries);

	const GameSpyPatchPlanResult plan = BuildGameSpyPatchPlan(replacements,
		2, matches, entries, alreadyPatched, QDNSM_OPENSPY_PLAN_TRACE);

	QDNSM_OPENSPY_LOG("worker: plan=%s alreadyPatched=%d entries=%u",
		PlanName(plan), alreadyPatched ? 1 : 0,
		static_cast<unsigned int>(entries.size()));

	if (plan == GameSpyPatchPlan_MissingRequired)
		return OpenSpyPatch_NotFound;

	if (plan == GameSpyPatchPlan_TooManyMatches)
		return OpenSpyPatch_TooManyMatches;

	if ((plan != GameSpyPatchPlan_Ok) &&
		(plan != GameSpyPatchPlan_AlreadyPatched))
		return OpenSpyPatch_ReadFailed;

	if (gothamWbid)
	{
		QDNSM_OPENSPY_LOG("worker: gotham-wbid old=%u new=%u",
			static_cast<unsigned int>(scan.oldMatches[3].size()),
			static_cast<unsigned int>(scan.newMatches[3].size()));

		for (size_t i = 0; i < scan.oldMatches[3].size(); i++)
		{
			GameSpyPatchEntry entry{};
			entry.address = scan.oldMatches[3][i].address;
			entry.oldLength = kGothamWbidLength;
			entry.newLength = kGothamWbidLength;
			vsh::memcpy(entry.oldBytes, kGothamWbidOldBytes,
				kGothamWbidLength);
			vsh::memcpy(entry.newBytes, kGothamWbidNewBytes,
				kGothamWbidLength);
			entries.push_back(entry);
		}

		QDNSM_OPENSPY_LOG("worker: gotham-wbid-gate old=%u new=%u",
			static_cast<unsigned int>(scan.gateOldMatches.size()),
			static_cast<unsigned int>(scan.gateNewMatches.size()));

		for (size_t i = 0; i < scan.gateOldMatches.size(); i++)
		{
			GameSpyPatchEntry entry{};
			entry.address = scan.gateOldMatches[i].address;
			entry.oldLength = kGothamWbidGateLength;
			entry.newLength = kGothamWbidGateLength;
			vsh::memcpy(entry.oldBytes, kGothamWbidGateOldBytes,
				kGothamWbidGateLength);
			vsh::memcpy(entry.newBytes, kGothamWbidGateNewBytes,
				kGothamWbidGateLength);
			entries.push_back(entry);

			QDNSM_OPENSPY_LOG("worker: gotham-wbid-gate address=0x%08X",
				entry.address);
		}

		for (size_t i = 0; i < scan.gateNewMatches.size(); i++)
		{
			QDNSM_OPENSPY_LOG("worker: gotham-wbid-gate already at 0x%08X",
				scan.gateNewMatches[i].address);
		}

		for (uint32_t i = 0; i < kWbidStatePatchCount; i++)
		{
			const WbidStatePatch& patch = kWbidStatePatches[i];

			QDNSM_OPENSPY_LOG("worker: %s old=%u new=%u", patch.name,
				static_cast<unsigned int>(
					scan.wbidStateOldMatches[i].size()),
				static_cast<unsigned int>(
					scan.wbidStateNewMatches[i].size()));

			for (size_t j = 0; j < scan.wbidStateOldMatches[i].size(); j++)
			{
				GameSpyPatchEntry entry{};
				entry.address = scan.wbidStateOldMatches[i][j].address;
				entry.oldLength = patch.length;
				entry.newLength = patch.length;
				vsh::memcpy(entry.oldBytes, patch.oldBytes, patch.length);
				vsh::memcpy(entry.newBytes, patch.newBytes, patch.length);
				entries.push_back(entry);

				QDNSM_OPENSPY_LOG("worker: %s address=0x%08X",
					patch.name, entry.address);
			}

			for (size_t j = 0; j < scan.wbidStateNewMatches[i].size(); j++)
			{
				QDNSM_OPENSPY_LOG("worker: %s already at 0x%08X",
					patch.name, scan.wbidStateNewMatches[i][j].address);
			}
		}
	}

	if ((plan == GameSpyPatchPlan_AlreadyPatched) &&
		entries.empty() && authEntries.empty())
		return OpenSpyPatch_AlreadyPatched;

	entries.insert(entries.end(), authEntries.begin(), authEntries.end());

	std::sort(entries.begin(), entries.end(),
		[](const GameSpyPatchEntry& left, const GameSpyPatchEntry& right)
		{
			return left.address < right.address;
		});

	uint64_t previousEnd = 0;

	for (size_t i = 0; i < entries.size(); i++)
	{
		const uint64_t start = entries[i].address;
		const uint64_t end = start + entries[i].newLength;

		if ((i != 0) && (start < previousEnd))
		{
			QDNSM_OPENSPY_LOG("worker: overlap at entry[%u] address=0x%08X",
				static_cast<unsigned int>(i), entries[i].address);
			return OpenSpyPatch_ReadFailed;
		}

		previousEnd = end;
	}

	for (size_t i = 0; i < entries.size(); i++)
	{
		QDNSM_OPENSPY_LOG("worker: entry[%u] address=0x%08X oldLen=%u newLen=%u",
			static_cast<unsigned int>(i), entries[i].address,
			entries[i].oldLength, entries[i].newLength);
	}

	if (entries.empty())
		return OpenSpyPatch_AlreadyPatched;

	return ApplyPatchPlan(pid, entries);
}

static void OpenSpyPatchThread(uint64_t)
{
	QDNSM_OPENSPY_LOG("thread: begin pid=%d", static_cast<int>(s_GamePid));

	const OpenSpyPatchOutcome outcome = RunPatchWorker(s_GamePid,
		s_GothamWbidPatch);

	QDNSM_OPENSPY_LOG("thread: outcome=%s (%d) stop=%d",
		OutcomeName(outcome), outcome, s_StopRequested ? 1 : 0);

	if (!s_StopRequested)
	{
		s_Outcome = outcome;
		__sync_synchronize();
		s_ResultReady = true;
	}

	s_Running = false;
	sys_ppu_thread_exit(0);
}

static void JoinPatchThread()
{
	if (s_ThreadId == SYS_PPU_THREAD_ID_INVALID)
		return;

	uint64_t exitCode = 0;
	sys_ppu_thread_join(s_ThreadId, &exitCode);
	s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
}

OpenSpyPatchStart StartOpenSpyPatch()
{
	if (s_Running || s_ResultReady)
		return OpenSpyPatch_AlreadyRunning;

	QDNSM_OPENSPY_LOG_BEGIN();
	QDNSM_OPENSPY_LOG("start: tick=%llu path=\"%s\"",
		static_cast<unsigned long long>(GetTimeNow()),
		OpenSpyLogPath().c_str());

	char titleId[10] = { 0 };

	const sys_pid_t pid = vsh::GetGameProcessId();
	if ((pid <= 0) || !ReadRunningTitleId(titleId))
	{
		QDNSM_OPENSPY_LOG("start: no game pid=%d title=\"%.9s\"",
			static_cast<int>(pid), titleId);
		return OpenSpyPatch_NoGame;
	}

	QDNSM_OPENSPY_LOG("start: pid=%d title=\"%.9s\"",
		static_cast<int>(pid), titleId);

	if (!IsSupportedTitleId(titleId))
	{
		QDNSM_OPENSPY_LOG("start: unsupported title=\"%.9s\"", titleId);
		return OpenSpyPatch_Unsupported;
	}

	if (s_ThreadId != SYS_PPU_THREAD_ID_INVALID)
	{
		QDNSM_OPENSPY_LOG("start: joining previous thread");
		JoinPatchThread();
	}

	if (g_InputUi.IsActive())
	{
		QDNSM_OPENSPY_LOG("start: input dialog active");
		return OpenSpyPatch_StartFailed;
	}

	// Gotham City Impostors only: WBID auto-skip.
	s_GothamWbidPatch = IsGothamCityImpostorsTitleId(titleId);
	QDNSM_OPENSPY_LOG("start: gotham-wbid=%d", s_GothamWbidPatch ? 1 : 0);

	s_GamePid = pid;
	s_StopRequested = false;
	s_GameExited = false;
	s_Running = true;
	s_ResultReady = false;

	const int createResult = sys_ppu_thread_create(
		&s_ThreadId,
		OpenSpyPatchThread,
		0,
		1059,
		kPatchThreadStackSize,
		SYS_PPU_THREAD_CREATE_JOINABLE,
		"qDNSmPatch");

	QDNSM_OPENSPY_LOG("start: sys_ppu_thread_create=%d", createResult);

	if (createResult != 0)
	{
		s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
		s_Running = false;
		return OpenSpyPatch_StartFailed;
	}

	return OpenSpyPatch_Started;
}

bool IsOpenSpyPatchBusy()
{
	return s_Running || s_ResultReady;
}

void PollOpenSpyPatch()
{
	if (!s_ResultReady)
		return;

	__sync_synchronize();
	JoinPatchThread();

	const OpenSpyPatchOutcome outcome = s_Outcome;
	s_ResultReady = false;

	QDNSM_OPENSPY_LOG("poll: outcome=%s (%d)", OutcomeName(outcome), outcome);
	ShowOpenSpyPatchResult(outcome);
}

void StopOpenSpyPatch()
{
	QDNSM_OPENSPY_LOG("stop: requested");
	s_StopRequested = true;
	JoinPatchThread();
	s_Running = false;
	s_ResultReady = false;
	QDNSM_OPENSPY_LOG("stop: complete");
}
