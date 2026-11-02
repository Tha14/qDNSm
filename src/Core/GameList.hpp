#pragma once

#include <stddef.h>
#include <string>
#include <vector>

static const char kOpenSpyGameListKey[] = "OpenSpy";

struct GameListSection
{
	std::string key;
	std::vector<std::wstring> games;
	std::vector<std::wstring> aliases;
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

/* Repository-level validation: non-empty, no alias-only sections, no OpenSpy aliases. */
bool ValidateGameList(const std::vector<GameListSection>& sections);

GameListLoadResult LoadGameList(const std::string& path, std::vector<GameListSection>& out);

const GameListSection* FindGameListSection(const std::vector<GameListSection>& sections,
	const std::string& key);
