#pragma once

#include <stdint.h>
#include <string>

/*
 * The one place the version is written down.
 *
 * It was hard-coded twice - once in the menu header, once on the About screen - and the two copies
 * disagreed: the header said 0.92 while About said 1.1. Two copies of one fact drift, and that was
 * the proof.
 *
 * Two forms because the two callers need two forms. The menu header draws a wide string with its own
 * "v" prefix; the About body is a narrow literal inside a larger block. Adjacent literals concatenate
 * at compile time, so both can be built from the same number without a runtime format.
 *
 * Changing the version means editing this file and nothing else.
 */
#define QDNSM_VERSION        L"1.4"
#define QDNSM_VERSION_ASCII  "1.4"
#define QDNSM_UPDATE_URL     "store.brewology.com"

inline bool ParseVersionParts(const std::string& version, uint64_t (&parts)[8],
	size_t& count)
{
	count = 0;
	uint64_t value = 0;
	size_t digits = 0;

	if (version.empty() || (version.size() > 32))
		return false;

	for (size_t i = 0; i < version.size(); i++)
	{
		const char character = version[i];

		if (character == '.')
		{
			if ((digits == 0) || (count >= 8))
				return false;

			parts[count++] = value;
			value = 0;
			digits = 0;
			continue;
		}

		if ((character < '0') || (character > '9') || (digits >= 9))
			return false;

		value = (value * 10) + static_cast<uint64_t>(character - '0');
		digits++;
	}

	if ((digits == 0) || (count >= 8))
		return false;

	parts[count++] = value;
	return true;
}

inline bool IsValidVersion(const std::string& version)
{
	uint64_t parts[8]{};
	size_t count = 0;
	return ParseVersionParts(version, parts, count);
}

inline bool IsVersionNewer(const std::string& candidate, const std::string& current)
{
	uint64_t candidateParts[8]{};
	uint64_t currentParts[8]{};
	size_t candidateCount = 0;
	size_t currentCount = 0;

	if (!ParseVersionParts(candidate, candidateParts, candidateCount) ||
		!ParseVersionParts(current, currentParts, currentCount))
	{
		return false;
	}

	for (size_t i = 0; i < 8; i++)
	{
		if (candidateParts[i] > currentParts[i])
			return true;

		if (candidateParts[i] < currentParts[i])
			return false;
	}

	return false;
}
