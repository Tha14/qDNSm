#include "OpenSpyPatchPlan.hpp"

#include <algorithm>
#include <string.h>

static bool SameEntry(const GameSpyPatchEntry& left, const GameSpyPatchEntry& right)
{
	return (left.address == right.address) &&
		(left.oldLength == right.oldLength) &&
		(left.newLength == right.newLength) &&
		(memcmp(left.oldBytes, right.oldBytes, left.oldLength) == 0) &&
		(memcmp(left.newBytes, right.newBytes, left.newLength) == 0);
}

GameSpyPatchPlanResult BuildGameSpyPatchPlan(
	const GameSpyReplacement* replacements,
	uint32_t replacementCount,
	const GameSpyReplacementMatches* matches,
	std::vector<GameSpyPatchEntry>& entries,
	bool& alreadyPatched,
	GameSpyPatchPlanTrace trace)
{
	const auto mark = [&](const char* stage)
	{
		if (trace)
			trace(stage);
	};

	mark("begin");
	entries.clear();
	mark("cleared");
	alreadyPatched = false;

	if (!replacements || !matches || (replacementCount == 0))
		return GameSpyPatchPlan_Invalid;

	bool allRequiredNew = true;

	for (uint32_t i = 0; i < replacementCount; i++)
	{
		const GameSpyReplacement& replacement = replacements[i];
		const GameSpyReplacementMatches& match = matches[i];

		if (!replacement.oldBytes || !replacement.newBytes ||
			(replacement.oldLength == 0) ||
			(replacement.oldLength > 256) ||
			(replacement.newLength == 0) ||
			(replacement.newLength > 256) ||
			(replacement.oldLength != replacement.newLength))
		{
			return GameSpyPatchPlan_Invalid;
		}

		if ((match.oldCount > kGameSpyMaxMatchesPerPattern) ||
			(match.newCount > kGameSpyMaxMatchesPerPattern))
		{
			return GameSpyPatchPlan_TooManyMatches;
		}

		if ((match.oldCount != 0) && (match.newCount != 0))
			return GameSpyPatchPlan_Mixed;

		if ((match.oldCount != 0) && !match.oldAddresses)
			return GameSpyPatchPlan_Invalid;

		if ((match.newCount != 0) && !match.newAddresses)
			return GameSpyPatchPlan_Invalid;

		if (replacement.required)
		{
			if ((match.oldCount == 0) && (match.newCount == 0))
				return GameSpyPatchPlan_MissingRequired;

			if (match.oldCount != 0)
				allRequiredNew = false;
		}

		if (match.oldCount == 0)
			continue;

		for (uint32_t j = 0; j < match.oldCount; j++)
		{
			GameSpyPatchEntry entry{};
			entry.address = match.oldAddresses[j];
			entry.oldLength = replacement.oldLength;
			entry.newLength = replacement.newLength;

			memcpy(entry.oldBytes, replacement.oldBytes,
				replacement.oldLength);
			memcpy(entry.newBytes, replacement.newBytes,
				replacement.newLength);

			entries.push_back(entry);

			if (entries.size() > kGameSpyMaxPatchEntries)
				return GameSpyPatchPlan_TooManyMatches;
		}
	}

	if (entries.empty())
	{
		if (allRequiredNew)
		{
			alreadyPatched = true;
			return GameSpyPatchPlan_AlreadyPatched;
		}

		return GameSpyPatchPlan_MissingRequired;
	}

	mark("entries built");

	std::sort(entries.begin(), entries.end(),
		[](const GameSpyPatchEntry& left, const GameSpyPatchEntry& right)
		{
			return left.address < right.address;
		});

	mark("sorted");

	size_t writeIndex = 0;

	for (size_t readIndex = 0; readIndex < entries.size(); readIndex++)
	{
		if ((writeIndex != 0) &&
			(entries[writeIndex - 1].address == entries[readIndex].address))
		{
			if (!SameEntry(entries[writeIndex - 1], entries[readIndex]))
				return GameSpyPatchPlan_Overlap;

			continue;
		}

		if (writeIndex != readIndex)
			entries[writeIndex] = entries[readIndex];

		writeIndex++;
	}

	entries.resize(writeIndex);

	mark("deduped");

	uint64_t previousEnd = 0;

	for (size_t i = 0; i < entries.size(); i++)
	{
		const uint64_t start = entries[i].address;
		const uint64_t end = start + entries[i].newLength;

		if (end > 0x100000000ULL)
			return GameSpyPatchPlan_Invalid;

		if ((i != 0) && (start < previousEnd))
			return GameSpyPatchPlan_Overlap;

		previousEnd = end;
	}

	mark("validated");
	return GameSpyPatchPlan_Ok;
}
