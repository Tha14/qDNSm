#pragma once

struct LanguageCatalogEntry
{
	const char* code;
	const unsigned char* data;
	unsigned short compressedSize;
	unsigned short rawSize;
};

extern const LanguageCatalogEntry kLanguageCatalogs[];
extern const int kLanguageCatalogCount;

bool Language_DecompressCatalog(const LanguageCatalogEntry& catalog, unsigned char* out, unsigned int outSize);
