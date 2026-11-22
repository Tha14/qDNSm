#pragma once

#include <stddef.h>
#include <stdint.h>
#include <deque>
#include <string>
#include <vector>
#include "Config.hpp"
#include "GameList.hpp"

struct GameDirectoryNote
{
	std::wstring project;
	std::wstring text;
	bool server{};
};

struct GameDirectoryRow
{
	std::wstring game;
	std::vector<std::wstring> projects;
	std::vector<std::wstring> aliases;
	std::vector<std::string> ips;
	uint32_t tags{};
	std::vector<GameDirectoryNote> notes;
};

typedef std::deque<GameDirectoryRow> GameDirectoryRows;

GameListLoadResult GameDirectory_SearchText(const std::string& text,
	const std::vector<DnsServerEntry>& servers, const std::string& query, bool grouped,
	GameDirectoryRows& rows, bool& truncated);

#ifndef QDNSM_HOST_TEST
GameListLoadResult GameDirectory_Search(const std::vector<DnsServerEntry>& servers,
	const std::string& query, bool grouped, GameDirectoryRows& rows, bool& truncated);

bool GameDirectory_IsAvailable();
void GameDirectory_RefreshAvailability();
void GameDirectory_OnFrame(bool menuOpen);
#endif
