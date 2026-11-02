#include "NetUpdate.hpp"

#include "../Net/NetProbeProtocol.hpp"
#include "../Net/UpdateUrls.hpp"
#include "../Utils/FileSystem.hpp"
#include "Config.hpp"
#include "GameListPanel.hpp"
#include "Helpers.hpp"
#include "InputUi.hpp"
#include "Language.hpp"

#include <sys/ppu_thread.h>
#include <vsh/stdc.hpp>
#include <vsh/sys_prx_for_user.hpp>
#include <vsh/vshcommon.hpp>

static const char* const kDnsTempName = "qDNSm-dns-list.part";
static const char* const kGamesTempName = "qDNSm-server-games.part";
static const char* const kGameFileName = "qDNSm-server-games.ini";

static volatile bool s_Running = false;
static volatile bool s_ResultReady = false;
static bool s_GamesRequested = false;
static sys_ppu_thread_t s_ThreadId = SYS_PPU_THREAD_ID_INVALID;

struct UpdateResult
{
	bool dnsOk;
	bool gamesRequested;
	bool gamesOk;
	bool gamesInvalid;
};

static UpdateResult s_Result;

static UpdateResult FetchUpdate(const std::string& dnsUrl, const std::string& dnsPath,
	const std::string& gamesUrl, const std::string& gamesPath)
{
	UpdateResult result{};
	result.gamesRequested = !gamesUrl.empty();

	const std::string modulePath = GetCurrentDir() + "qDNSmNetLib.sprx";

	if (!FileExist(modulePath))
		return result;

	NetProbeRequest request{};
	request.magic = kNetProbeMagic;
	request.version = kNetProbeVersion;
	request.job = kNetProbeJobUpdate;

	vsh::strncpy(request.url, dnsUrl.c_str(), sizeof(request.url) - 1);
	vsh::strncpy(request.outputPath, dnsPath.c_str(), sizeof(request.outputPath) - 1);
	vsh::strncpy(request.configPath, g_Config.GetFilePath().c_str(),
		sizeof(request.configPath) - 1);
	vsh::strncpy(request.gamePath, (GetCurrentDir() + kGameFileName).c_str(),
		sizeof(request.gamePath) - 1);
	request.url[sizeof(request.url) - 1] = '\0';
	request.outputPath[sizeof(request.outputPath) - 1] = '\0';
	request.configPath[sizeof(request.configPath) - 1] = '\0';
	request.gamePath[sizeof(request.gamePath) - 1] = '\0';

	if (!gamesUrl.empty())
	{
		vsh::strncpy(request.url2, gamesUrl.c_str(), sizeof(request.url2) - 1);
		vsh::strncpy(request.outputPath2, gamesPath.c_str(),
			sizeof(request.outputPath2) - 1);
		request.url2[sizeof(request.url2) - 1] = '\0';
		request.outputPath2[sizeof(request.outputPath2) - 1] = '\0';
	}

	sys_prx_id_t id = sysPrxForUser::sys_prx_load_module(modulePath.c_str(), 0, nullptr);

	if (id < 0)
		return result;

	int startModuleResult = -1;
	int start = sysPrxForUser::_sys_prx_start_module(
		id, sizeof(request), &request, &startModuleResult, 0, nullptr);

	int stopModuleResult = -1;
	int stop = sysPrxForUser::_sys_prx_stop_module(
		id, sizeof(request), &request, &stopModuleResult, 0, nullptr);

	int unload = sysPrxForUser::_sys_prx_unload_module(id, 0, nullptr);

	if ((start != 0) || (startModuleResult != 0) ||
		(request.moduleStartSeen != 1) || (request.done != 1) ||
		(stop != 0) || (unload != 0))
	{
		return result;
	}

	result.dnsOk = (request.dnsOk == 1);
	result.gamesOk = (request.gamesOk == 1);
	result.gamesInvalid = (request.gamesInvalid == 1);

	return result;
}

static void NetUpdateThread(uint64_t)
{
	const std::string base = GetCurrentDir();
	const bool gamesRequested = s_GamesRequested;

	UpdateResult result{};
	result.gamesRequested = gamesRequested;

	std::string gamesUrl;
	std::string gamesPath;

	if (gamesRequested)
	{
		gamesUrl = kGameListUrl;
		gamesPath = base + kGamesTempName;
	}

	result = FetchUpdate(kDnsListUrl, base + kDnsTempName, gamesUrl, gamesPath);

	s_Result = result;
	__sync_synchronize();
	s_Running = false;
	s_ResultReady = true;
	sys_ppu_thread_exit(0);
}

bool StartNetUpdate()
{
	if (s_Running || s_ResultReady)
		return false;

	if (g_InputUi.IsActive())
		return false;

	if (s_ThreadId != SYS_PPU_THREAD_ID_INVALID)
	{
		uint64_t exitCode = 0;
		sys_ppu_thread_join(s_ThreadId, &exitCode);
		s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
	}

	s_GamesRequested = g_Config.m_Settings.showSupportedGames;
	s_Running = true;
	s_ResultReady = false;

	const int createResult = sys_ppu_thread_create(
		&s_ThreadId,
		NetUpdateThread,
		0,
		1059,
		0x10000,
		SYS_PPU_THREAD_CREATE_JOINABLE,
		"qDNSmUpdate");

	if (createResult != 0)
	{
		s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
		s_Running = false;
		return false;
	}

	return true;
}

bool IsNetUpdateBusy()
{
	return s_Running || s_ResultReady;
}

void PollNetUpdate()
{
	if (!s_ResultReady)
		return;

	__sync_synchronize();

	UpdateResult result = s_Result;
	s_Result = UpdateResult{};
	s_ResultReady = false;

	bool dnsApplied = false;
	bool gamesApplied = false;

	if (result.dnsOk)
	{
		g_Config.Load();
		dnsApplied = true;
	}

	if (result.gamesRequested && result.gamesOk)
		gamesApplied = true;

	if (dnsApplied || gamesApplied)
		g_GameListPanel.Invalidate();

	const wchar_t* message = nullptr;
	vsh::NotifyIcon icon = vsh::NotifyIcon::BlueVerifiedCheckmark;
	vsh::NotifySound sound = vsh::NotifySound::OK;

	if (dnsApplied && !result.gamesRequested)
	{
		message = T(TEXT_UPDATE_DNS_ONLY);
	}
	else if (dnsApplied && gamesApplied)
	{
		message = T(TEXT_UPDATE_BOTH);
	}
	else if (dnsApplied)
	{
		if (result.gamesInvalid)
			message = T(TEXT_UPDATE_GAME_INVALID);
		else
			message = T(TEXT_UPDATE_GAME_NOT_UPDATED);

		icon = vsh::NotifyIcon::Caution;
		sound = vsh::NotifySound::Error;
	}
	else if (gamesApplied)
	{
		message = T(TEXT_UPDATE_DNS_FAILED);
		icon = vsh::NotifyIcon::Caution;
		sound = vsh::NotifySound::Error;
	}
	else
	{
		message = T(TEXT_UPDATE_FAILED);
		icon = vsh::NotifyIcon::Caution;
		sound = vsh::NotifySound::Error;
	}

	if (icon == vsh::NotifyIcon::BlueVerifiedCheckmark)
		ShowSuccessNotification(std::wstring(message));
	else
		vsh::ShowNotificationWithIcon(message, icon, sound);
}

void StopNetUpdate()
{
	if (s_ThreadId != SYS_PPU_THREAD_ID_INVALID)
	{
		uint64_t exitCode = 0;
		sys_ppu_thread_join(s_ThreadId, &exitCode);
		s_ThreadId = SYS_PPU_THREAD_ID_INVALID;
	}

	s_Running = false;
	s_ResultReady = false;
}
