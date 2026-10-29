#include "NetUpdate.hpp"

#include "../Net/NetProbeProtocol.hpp"
#include "../Net/UpdateUrls.hpp"
#include "../Utils/FileSystem.hpp"
#include "Config.hpp"
#include "DnsList.hpp"
#include "GameList.hpp"
#include "GameListPanel.hpp"
#include "InputUi.hpp"
#include "Language.hpp"

#include <cell/cell_fs.h>
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

struct FetchPair
{
	bool dnsOk;
	bool gamesOk;
};

struct UpdateResult
{
	bool dnsOk;
	bool gamesRequested;
	bool gamesOk;
	bool gamesInvalid;
	std::vector<RemoteDnsEntry> dnsEntries;
	std::string dnsTempPath;
	std::string gamesTempPath;
};

static UpdateResult s_Result;

static void RemoveFile(const std::string& path)
{
	cellFsUnlink(path.c_str());
}

static bool FetchSucceeded(int httpStatus, int error, uint32_t bytes,
	const std::string& path)
{
	if ((error != kNetProbeErrorNone) || (httpStatus != 200) || (bytes == 0))
		return false;

	CellFsStat stat;

	return (cellFsStat(path.c_str(), &stat) == CELL_FS_SUCCEEDED) &&
		(static_cast<uint32_t>(stat.st_size) == bytes);
}

static FetchPair FetchUpdate(const std::string& dnsUrl, const std::string& dnsPath,
	const std::string& gamesUrl, const std::string& gamesPath)
{
	FetchPair pair{};

	const std::string modulePath = GetCurrentDir() + "qDNSmNetLib.sprx";

	if (!FileExist(modulePath))
		return pair;

	NetProbeRequest request{};
	request.magic = kNetProbeMagic;
	request.version = kNetProbeVersion;
	request.job = kNetProbeJobUpdate;

	vsh::strncpy(request.url, dnsUrl.c_str(), sizeof(request.url) - 1);
	vsh::strncpy(request.outputPath, dnsPath.c_str(), sizeof(request.outputPath) - 1);
	request.url[sizeof(request.url) - 1] = '\0';
	request.outputPath[sizeof(request.outputPath) - 1] = '\0';

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
		return pair;

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
		return pair;
	}

	pair.dnsOk = FetchSucceeded((int)request.httpStatus, (int)request.error,
		(uint32_t)request.bytes, dnsPath);

	if (!gamesUrl.empty())
	{
		pair.gamesOk = FetchSucceeded((int)request.httpStatus2, (int)request.error2,
			(uint32_t)request.bytes2, gamesPath);
	}

	return pair;
}

static bool ApplyDns(const std::vector<RemoteDnsEntry>& entries)
{
	std::vector<DnsServerEntry> previous = g_Config.m_DnsServers;
	std::vector<DnsServerEntry> merged;
	std::string error;

	if (!MergeManagedDnsList(entries, g_Config.m_DnsServers, merged, &error))
		return false;

	g_Config.m_DnsServers.swap(merged);

	if (!g_Config.Save())
	{
		g_Config.m_DnsServers.swap(previous);
		return false;
	}

	return true;
}

static bool ApplyGames(const std::string& tempPath, const std::string& finalPath)
{
	std::string content;

	if (!ReadFileText(tempPath, content, 2 * 1024 * 1024))
		return false;

	std::string previous;
	const bool hadPrevious = ReadFileText(finalPath, previous, 2 * 1024 * 1024);

	if (SaveFile(finalPath, content.data(), content.size()))
		return true;

	if (hadPrevious)
		SaveFile(finalPath, previous.data(), previous.size());

	return false;
}

static void NetUpdateThread(uint64_t)
{
	const std::string base = GetCurrentDir();
	const std::string dnsTemp = base + kDnsTempName;
	const std::string gamesTemp = base + kGamesTempName;
	const bool gamesRequested = s_GamesRequested;

	UpdateResult result{};
	result.gamesRequested = gamesRequested;
	result.dnsTempPath = dnsTemp;
	result.gamesTempPath = gamesTemp;

	std::string gamesUrl;
	std::string gamesPath;

	if (gamesRequested)
	{
		gamesUrl = kGameListUrl;
		gamesPath = gamesTemp;
	}

	FetchPair pair = FetchUpdate(kDnsListUrl, dnsTemp, gamesUrl, gamesPath);

	if (pair.dnsOk)
	{
		std::vector<RemoteDnsEntry> entries;

		if ((LoadDnsList(dnsTemp, entries) == DnsList_Loaded) && !entries.empty())
		{
			result.dnsOk = true;
			result.dnsEntries.swap(entries);
		}
	}

	if (!result.dnsOk)
		RemoveFile(dnsTemp);

	if (gamesRequested)
	{
		if (pair.gamesOk)
		{
			std::vector<GameListSection> sections;

			if ((LoadGameList(gamesTemp, sections) == GameList_Loaded) &&
				ValidateGameList(sections))
			{
				result.gamesOk = true;
			}
			else
			{
				result.gamesInvalid = true;
			}
		}

		if (!result.gamesOk)
			RemoveFile(gamesTemp);
	}

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

	const std::string base = GetCurrentDir();
	RemoveFile(base + kDnsTempName);
	RemoveFile(base + kGamesTempName);

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
		dnsApplied = ApplyDns(result.dnsEntries);

	if (result.gamesRequested && result.gamesOk)
	{
		gamesApplied = ApplyGames(
			result.gamesTempPath, GetCurrentDir() + kGameFileName);
	}

	RemoveFile(result.dnsTempPath);

	if (result.gamesRequested)
		RemoveFile(result.gamesTempPath);

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
