#pragma once

#include <stddef.h>
#include <stdint.h>

struct AobPattern
{
	const uint8_t* bytes;
	const uint8_t* mask;
	size_t length;
};

struct AobMatch
{
	uint32_t address;
	uint32_t offset;
};

struct AobScanSegment
{
	uint32_t start;
	uint32_t reportEnd;
	uint32_t readEnd;
};

/* Finds matches in a read buffer; startLimit prevents duplicate reports across chunks. */
size_t FindAllMatches(const uint8_t* data, size_t size, uint32_t baseAddress,
	const AobPattern& pattern, AobMatch* out, size_t maxMatches,
	size_t startLimit = 0);

size_t BuildScanSegments(uint32_t start, uint32_t end, uint32_t split,
	uint32_t overlap, AobScanSegment* out);
