#include "OpenSpyPatch.hpp"
#include "InputUi.hpp"

#include "../OpenSpy/OpenSpyProtocol.hpp"
#include "../Utils/FileSystem.hpp"

#include <string.h>
#include <sys/ppu_thread.h>
#include <sys/process.h>
#include <vsh/paf.hpp>
#include <vsh/plugins/game_plugin.hpp>
#include <vsh/sys_prx_for_user.hpp>
#include <vsh/vshmain.hpp>

static const uint32_t kPatchWorkerStackSize = 0x20000;
static const char kOpenSpyModulePath[] = "qDNSm_modules/qDNSmOpenSpy.sprx";

static bool s_ModuleAvailable = false;
static bool s_ModuleAvailabilityKnown = false;
static volatile bool s_Running = false;
static volatile bool s_ResultReady = false;
static volatile bool s_GameExited = false;
static sys_ppu_thread_t s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
static sys_pid_t s_GamePid = 0;
static OpenSpyPatchOutcome s_Outcome = OpenSpyPatch_ReadFailed;
static OpenSpyRequest s_Request;

static bool ReadGameTitleId(char titleId[10], char fallbackTitleId[10])
{
	titleId[0] = '\0';
	fallbackTitleId[0] = '\0';

	paf::View* view = paf::View::Find("game_plugin");
	if (!view)
		return false;

	vsh::GamePluginInterface* game =
		view->GetInterface<vsh::GamePluginInterface*>(1);
	if (!game)
		return false;

	bool found = false;

	uint8_t info[0x120];
	memset(info, 0, sizeof(info));

	if (game->GameInfo)
	{
		game->GameInfo(info);
		memcpy(titleId, info + 0x04, 9);
		titleId[9] = '\0';
		found = true;
	}

	uint8_t fallback[0x20];
	memset(fallback, 0, sizeof(fallback));

	if (game->GetTitleId)
	{
		game->GetTitleId(fallback);
		memcpy(fallbackTitleId, fallback, 9);
		fallbackTitleId[9] = '\0';
		found = true;
	}

	return found;
}

static void RefreshModuleAvailability()
{
	s_ModuleAvailable = FileExist(GetCurrentDir() + kOpenSpyModulePath);
	s_ModuleAvailabilityKnown = true;
}

bool IsOpenSpyModuleAvailable()
{
	if (!s_ModuleAvailabilityKnown)
		RefreshModuleAvailability();

	return s_ModuleAvailable;
}

void OpenSpyModule_OnFrame(bool menuOpen)
{
	static bool s_WasOpen = false;

	if (menuOpen && !s_WasOpen)
		RefreshModuleAvailability();

	s_WasOpen = menuOpen;
}

static void JoinPatchThread()
{
	if (s_ThreadId == SYS_PPU_THREAD_ID_INVALID)
		return;

	uint64_t exitCode = 0;
	sys_ppu_thread_join(s_ThreadId, &exitCode);
	s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
}

static void OpenSpyPatchThread(uint64_t)
{
	OpenSpyPatchOutcome outcome = OpenSpyPatch_ReadFailed;
	const std::string modulePath = GetCurrentDir() + kOpenSpyModulePath;

	if (FileExist(modulePath))
	{
		sys_prx_id_t id = sysPrxForUser::sys_prx_load_module(
			modulePath.c_str(), 0, nullptr);

		if (id >= 0)
		{
			int startModuleResult = -1;
			const int start = sysPrxForUser::_sys_prx_start_module(
				id, sizeof(s_Request), &s_Request, &startModuleResult, 0,
				nullptr);

			int stopModuleResult = -1;
			const int stop = sysPrxForUser::_sys_prx_stop_module(
				id, sizeof(s_Request), &s_Request, &stopModuleResult, 0,
				nullptr);

			const int unload = sysPrxForUser::_sys_prx_unload_module(
				id, 0, nullptr);

			if ((start == 0) && (startModuleResult == 0) &&
				(s_Request.moduleMagic == kOpenSpyModuleMagic) &&
				(s_Request.moduleVersion == kOpenSpyRequestVersion) &&
				(s_Request.moduleStartSeen == 1) &&
				(s_Request.done == 1) && (stop == 0) && (unload == 0))
			{
				outcome = static_cast<OpenSpyPatchOutcome>(
					s_Request.outcome);
			}
		}
	}

	if (s_GameExited && (outcome != OpenSpyPatch_Success) &&
		(outcome != OpenSpyPatch_AlreadyPatched))
	{
		outcome = OpenSpyPatch_GameExited;
	}

	s_Outcome = outcome;
	__sync_synchronize();
	s_Running = false;
	s_ResultReady = true;
	sys_ppu_thread_exit(0);
}

OpenSpyPatchStart StartOpenSpyPatch()
{
	if (s_Running || s_ResultReady)
		return OpenSpyPatch_AlreadyRunning;

	if (s_ThreadId != SYS_PPU_THREAD_ID_INVALID)
		JoinPatchThread();

	if (g_InputUi.IsActive())
		return OpenSpyPatch_StartFailed;

	char titleId[10] = { 0 };
	char fallbackTitleId[10] = { 0 };
	const sys_pid_t pid = vsh::GetGameProcessId();

	if ((pid <= 0) || !ReadGameTitleId(titleId, fallbackTitleId))
		return OpenSpyPatch_NoGame;

	memset(&s_Request, 0, sizeof(s_Request));
	s_Request.magic = kOpenSpyRequestMagic;
	s_Request.version = kOpenSpyRequestVersion;
	s_Request.pid = pid;
	memcpy(s_Request.titleId, titleId, sizeof(s_Request.titleId));
	memcpy(s_Request.fallbackTitleId, fallbackTitleId,
		sizeof(s_Request.fallbackTitleId));

	s_GamePid = pid;
	s_GameExited = false;
	s_Outcome = OpenSpyPatch_ReadFailed;
	s_Running = true;
	s_ResultReady = false;

	const int createResult = sys_ppu_thread_create(
		&s_ThreadId,
		OpenSpyPatchThread,
		0,
		1059,
		kPatchWorkerStackSize,
		SYS_PPU_THREAD_CREATE_JOINABLE,
		"qDNSmOpenSpy");

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
	if (s_Running && !s_GameExited && (s_GamePid > 0) &&
		(vsh::GetGameProcessId() != s_GamePid))
	{
		s_GameExited = true;
		s_Request.stop = 1;
	}

	if (!s_ResultReady)
		return;

	__sync_synchronize();
	JoinPatchThread();

	const OpenSpyPatchOutcome outcome = s_Outcome;
	s_ResultReady = false;
	s_GamePid = 0;

	ShowOpenSpyPatchResult(outcome);
}

void StopOpenSpyPatch()
{
	s_Request.stop = 1;
	JoinPatchThread();
	s_Running = false;
	s_ResultReady = false;
	s_GamePid = 0;
}
