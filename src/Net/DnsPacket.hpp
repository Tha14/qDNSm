#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string.h>

namespace DnsPacket
{
	static const size_t kHeaderSize = 12;
	static const size_t kMaxHostLength = 253;

	inline size_t BuildQuery(const char* host, uint16_t id, unsigned char* query,
		size_t capacity)
	{
		if ((host == nullptr) || (query == nullptr) || (capacity < (kHeaderSize + 5)))
			return 0;

		const size_t hostLength = strlen(host);

		if ((hostLength == 0) || (hostLength > kMaxHostLength))
			return 0;

		memset(query, 0, kHeaderSize);

		query[0] = static_cast<unsigned char>(id >> 8);
		query[1] = static_cast<unsigned char>(id);
		query[2] = 0x01;
		query[3] = 0x00;
		query[4] = 0x00;
		query[5] = 0x01;

		size_t offset = kHeaderSize;
		const char* label = host;

		for (;;)
		{
			const char* dot = label;

			while ((*dot != '\0') && (*dot != '.'))
				++dot;

			const size_t labelLength = static_cast<size_t>(dot - label);

			if ((labelLength == 0) || (labelLength > 63))
				return 0;

			if ((offset + 1 + labelLength + 1 + 4) > capacity)
				return 0;

			query[offset++] = static_cast<unsigned char>(labelLength);
			memcpy(query + offset, label, labelLength);
			offset += labelLength;

			if (*dot == '\0')
				break;

			label = dot + 1;
		}

		query[offset++] = 0x00;
		query[offset++] = 0x00;
		query[offset++] = 0x01;
		query[offset++] = 0x00;
		query[offset++] = 0x01;

		return offset;
	}

	inline bool SkipName(const unsigned char* data, size_t length, size_t& offset)
	{
		while (offset < length)
		{
			const unsigned char label = data[offset];

			if ((label & 0xC0) == 0xC0)
			{
				if ((offset + 1) >= length)
					return false;

				offset += 2;
				return true;
			}

			if ((label & 0xC0) != 0)
				return false;

			offset++;

			if (label == 0)
				return true;

			if ((offset + label) > length)
				return false;

			offset += label;
		}

		return false;
	}

	inline bool ParseResponse(const unsigned char* data, size_t length, uint16_t id,
		unsigned char address[4])
	{
		if ((data == nullptr) || (address == nullptr) || (length < kHeaderSize))
			return false;

		if ((data[0] != static_cast<unsigned char>(id >> 8)) ||
			(data[1] != static_cast<unsigned char>(id)))
		{
			return false;
		}

		const uint16_t flags = static_cast<uint16_t>((data[2] << 8) | data[3]);
		const uint16_t questions = static_cast<uint16_t>((data[4] << 8) | data[5]);
		const uint16_t answers = static_cast<uint16_t>((data[6] << 8) | data[7]);

		if (((flags & 0x8000) == 0) || ((flags & 0x000F) != 0))
			return false;

		size_t offset = kHeaderSize;

		for (uint16_t i = 0; i < questions; i++)
		{
			if (!SkipName(data, length, offset) || ((offset + 4) > length))
				return false;

			offset += 4;
		}

		for (uint16_t i = 0; i < answers; i++)
		{
			if (!SkipName(data, length, offset) || ((offset + 10) > length))
				return false;

			const uint16_t type =
				static_cast<uint16_t>((data[offset] << 8) | data[offset + 1]);
			const uint16_t addressClass =
				static_cast<uint16_t>((data[offset + 2] << 8) | data[offset + 3]);
			const uint16_t dataLength =
				static_cast<uint16_t>((data[offset + 8] << 8) | data[offset + 9]);

			if ((type == 1) && (addressClass == 1) && (dataLength == 4))
			{
				if ((offset + 14) > length)
					return false;

				memcpy(address, data + offset + 10, 4);
				return true;
			}

			if ((offset + 10 + dataLength) > length)
				return false;

			offset += 10 + dataLength;
		}

		return false;
	}
}
