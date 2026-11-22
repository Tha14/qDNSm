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

extern const LanguageCatalogEntry kLanguageNotificationCatalogs[];
extern const int kLanguageNotificationCatalogCount;
extern const int kLanguageNotificationIds[];
extern const int kLanguageNotificationCount;
extern const unsigned int kLanguageNotificationMaxRawSize;

bool Language_DecompressCatalog(const LanguageCatalogEntry& catalog, unsigned char* out, unsigned int outSize);
