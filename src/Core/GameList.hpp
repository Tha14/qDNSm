#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

static const char kOpenSpyGameListKey[] = "OpenSpy";

enum GameTag : uint32_t
{
	GameTag_None = 0,
	GameTag_RequiresPatch = 1u << 0,
	GameTag_RequiresWhitelist = 1u << 1,
	GameTag_DiscordAccountLinking = 1u << 2
};

struct GameListGame
{
	std::wstring name;
	uint32_t tags{};
	std::wstring note;
};

struct GameListSection
{
	std::string key;
	std::vector<GameListGame> games;
	std::vector<std::wstring> aliases;
	std::wstring note;
};

enum GameListLoadResult
{
	GameList_Loaded,
	GameList_Missing,
	GameList_Unreadable,
	GameList_Invalid
};

/*
 * Parses the catalogue without touching the file system, so the format can be tested on a
 * host. The parser is strict about the repository format: unknown keys, duplicate sections,
 * control bytes, empty values, and non-ASCII bytes are all errors. A section with no games
 * is accepted here because the panel has a distinct empty-section state; the validator
 * rejects it for the repository.
 */
bool ParseGameList(const char* data, size_t size, std::vector<GameListSection>& out);

/* Parse one complete section, including tags and notes. */
bool ParseGameListSection(const char* data, size_t size, const std::string& key,
	GameListSection& out, bool& found);

/* Load only the game names and aliases for the supported-games panel. */
bool ParseGameListSectionNames(const char* data, size_t size, const std::string& key,
	std::vector<std::wstring>& games, std::vector<std::wstring>& aliases, bool& found);

/* Repository-level validation: non-empty, no alias-only sections, no OpenSpy aliases. */
bool ValidateGameList(const std::vector<GameListSection>& sections);

GameListLoadResult LoadGameList(const std::string& path, std::vector<GameListSection>& out);
GameListLoadResult LoadGameListSection(const std::string& path, const std::string& key,
	GameListSection& out);
GameListLoadResult LoadGameListSectionNames(const std::string& path, const std::string& key,
	std::vector<std::wstring>& games, std::vector<std::wstring>& aliases, bool& found);

const GameListSection* FindGameListSection(const std::vector<GameListSection>& sections,
	const std::string& key);
