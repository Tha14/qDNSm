#include "AobScan.hpp"
#include <string.h>

size_t BuildScanSegments(uint32_t start, uint32_t end, uint32_t split,
	uint32_t overlap, AobScanSegment* out)
{
	if (!out || (start >= end))
		return 0;

	const bool splitRange = (split > start) && (split < end);
	const uint64_t readEnd = static_cast<uint64_t>(split) + overlap;

	out[0].start = start;
	out[0].reportEnd = splitRange ? split : end;
	out[0].readEnd = splitRange && (readEnd < end)
		? static_cast<uint32_t>(readEnd) : end;

	if (!splitRange)
		return 1;

	out[1].start = split;
	out[1].reportEnd = end;
	out[1].readEnd = end;
	return 2;
}

size_t FindAllMatches(const uint8_t* data, size_t size, uint32_t baseAddress,
	const AobPattern& pattern, AobMatch* out, size_t maxMatches,
	size_t startLimit)
{
	if (!data || !pattern.bytes || (pattern.length == 0) ||
		(pattern.length > size) || !out || (maxMatches == 0))
	{
		return 0;
	}

	const size_t limit = (startLimit == 0 || startLimit > size)
		? size : startLimit;
	const size_t lastStart = size - pattern.length;
	size_t count = 0;

	for (size_t offset = 0; (offset <= lastStart) && (offset < limit); offset++)
	{
		const uint8_t* candidate = data + offset;
		bool match;

		if (pattern.mask)
		{
			match = true;

			for (size_t i = 0; i < pattern.length; i++)
			{
				if ((candidate[i] ^ pattern.bytes[i]) & pattern.mask[i])
				{
					match = false;
					break;
				}
			}
		}
		else
		{
			match = (candidate[0] == pattern.bytes[0]) &&
				(memcmp(candidate + 1, pattern.bytes + 1,
					pattern.length - 1) == 0);
		}

		if (!match)
			continue;

		out[count].address = baseAddress + static_cast<uint32_t>(offset);
		out[count].offset = static_cast<uint32_t>(offset);

		if (++count == maxMatches)
			break;
	}

	return count;
}
