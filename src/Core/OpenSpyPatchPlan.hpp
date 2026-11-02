#pragma once

#include <stdint.h>
#include <vector>

static const uint32_t kGameSpyMaxMatchesPerPattern = 32;
static const uint32_t kGameSpyMaxPatchEntries = 96;

struct GameSpyReplacement
{
	const uint8_t* oldBytes;
	const uint8_t* newBytes;
	uint32_t oldLength;
	uint32_t newLength;
	bool required;
};

struct GameSpyReplacementMatches
{
	const uint32_t* oldAddresses;
	uint32_t oldCount;
	const uint32_t* newAddresses;
	uint32_t newCount;
};

struct GameSpyPatchEntry
{
	uint32_t address;
	uint8_t oldBytes[256];
	uint8_t newBytes[256];
	uint32_t oldLength;
	uint32_t newLength;
};

enum GameSpyPatchPlanResult
{
	GameSpyPatchPlan_Ok,
	GameSpyPatchPlan_AlreadyPatched,
	GameSpyPatchPlan_MissingRequired,
	GameSpyPatchPlan_Mixed,
	GameSpyPatchPlan_TooManyMatches,
	GameSpyPatchPlan_Overlap,
	GameSpyPatchPlan_Invalid
};

typedef void (*GameSpyPatchPlanTrace)(const char* stage);

GameSpyPatchPlanResult BuildGameSpyPatchPlan(
	const GameSpyReplacement* replacements,
	uint32_t replacementCount,
	const GameSpyReplacementMatches* matches,
	std::vector<GameSpyPatchEntry>& entries,
	bool& alreadyPatched,
	GameSpyPatchPlanTrace trace = nullptr);
