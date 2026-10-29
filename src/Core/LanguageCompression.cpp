#include "LanguageCompression.hpp"

bool Language_DecompressCatalog(const LanguageCatalogEntry& catalog, unsigned char* out, unsigned int outSize)
{
	if ((catalog.data == nullptr) || (out == nullptr) || (outSize < catalog.rawSize))
		return false;

	const unsigned char* src = catalog.data;
	const unsigned char* end = catalog.data + catalog.compressedSize;
	unsigned int outPos = 0;

	while (outPos < catalog.rawSize)
	{
		if (src >= end)
			return false;

		unsigned char flags = *src++;

		for (int bit = 0; (bit < 8) && (outPos < catalog.rawSize); bit++)
		{
			if ((flags & (1 << bit)) == 0)
			{
				if (src >= end)
					return false;

				out[outPos++] = *src++;
				continue;
			}

			if ((end - src) < 2)
				return false;

			unsigned int low = *src++;
			unsigned int high = *src++;
			unsigned int distance = ((high & 0x0F) << 8) + low + 1;
			unsigned int length = (high >> 4) + 3;

			if (distance > outPos)
				return false;

			for (unsigned int i = 0; (i < length) && (outPos < catalog.rawSize); i++)
			{
				out[outPos] = out[outPos - distance];
				outPos++;
			}
		}
	}

	return true;
}
