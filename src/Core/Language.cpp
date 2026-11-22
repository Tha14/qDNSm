#include "Language.hpp"
#include "LanguageCatalog.hpp"
#include "LanguageCodec.hpp"
#include "LanguageCompression.hpp"
#include "LanguageStorage.hpp"

static std::wstring g_Active[TEXT_COUNT];
static std::wstring g_Empty;
static std::string g_RequestedCode = "en";
static std::string g_ActiveCode = "en";

static const LanguageCatalogEntry* CatalogForCode(const std::string& code)
{
	for (int i = 0; i < kLanguageCatalogCount; i++)
	{
		if (code == kLanguageCatalogs[i].code)
			return &kLanguageCatalogs[i];
	}

	return nullptr;
}

static const LanguageCatalogEntry* NotificationCatalogForCode(
	const std::string& code)
{
	for (int i = 0; i < kLanguageNotificationCatalogCount; i++)
	{
		if (code == kLanguageNotificationCatalogs[i].code)
			return &kLanguageNotificationCatalogs[i];
	}

	return nullptr;
}

static const LanguageCatalogEntry* EnglishCatalog()
{
	return CatalogForCode("en");
}

static bool DecodeCatalogInto(const LanguageCatalogEntry& catalog,
	std::wstring* out)
{
	unsigned char* raw = new unsigned char[catalog.rawSize];

	if (raw == nullptr)
		return false;

	if (!Language_DecompressCatalog(catalog, raw, catalog.rawSize))
	{
		delete[] raw;
		return false;
	}

	const unsigned char* cursor = raw;
	const unsigned char* end = raw + catalog.rawSize;
	bool valid = true;

	for (int i = 0; i < TEXT_COUNT; i++)
	{
		if (cursor >= end)
		{
			valid = false;
			break;
		}

		const unsigned char* terminator = cursor;

		while ((terminator < end) && (*terminator != 0))
			terminator++;

		if (terminator >= end)
		{
			valid = false;
			break;
		}

		out[i] = DecodeUtf8(reinterpret_cast<const char*>(cursor));
		cursor = terminator + 1;
	}

	delete[] raw;
	return valid;
}

static bool DecodeIndex(const LanguageCatalogEntry& catalog, int index,
	std::wstring& out)
{
	unsigned char* raw = new unsigned char[catalog.rawSize];

	if (raw == nullptr)
		return false;

	if (!Language_DecompressCatalog(catalog, raw, catalog.rawSize))
	{
		delete[] raw;
		return false;
	}

	const unsigned char* cursor = raw;
	const unsigned char* end = raw + catalog.rawSize;

	for (int i = 0; i < index; i++)
	{
		while ((cursor < end) && (*cursor != 0))
			cursor++;

		if (cursor >= end)
		{
			delete[] raw;
			return false;
		}

		cursor++;
	}

	if (cursor >= end)
	{
		delete[] raw;
		return false;
	}

	const unsigned char* terminator = cursor;

	while ((terminator < end) && (*terminator != 0))
		terminator++;

	if (terminator >= end)
	{
		delete[] raw;
		return false;
	}

	out = DecodeUtf8(reinterpret_cast<const char*>(cursor));
	delete[] raw;
	return true;
}

static void ClearActive()
{
	for (int i = 0; i < TEXT_COUNT; i++)
	{
		std::wstring empty;
		empty.swap(g_Active[i]);
	}
}

static bool LoadEnglish()
{
	const LanguageCatalogEntry* catalog = EnglishCatalog();

	g_ActiveCode = "en";
	if (catalog == nullptr)
		return false;

	return DecodeCatalogInto(*catalog, g_Active);
}

bool Language_Init(const std::string& code)
{
	std::string requested = code;

	if ((requested == "auto") || !Language_IsValidCode(requested))
		requested = "en";

	std::string selected = requested;
	bool available = true;

	if (selected != "en")
	{
		available = Language_ExternalFileExists(selected);

		if (!available)
			selected = "en";
	}

	ClearActive();
	g_RequestedCode = requested;
	g_ActiveCode = selected;

	return available;
}

bool Language_LoadAll()
{
	ClearActive();

	const LanguageCatalogEntry* english = EnglishCatalog();

	if (english == nullptr)
		return false;

	if (g_RequestedCode == "en")
	{
		g_ActiveCode = "en";
		return DecodeCatalogInto(*english, g_Active);
	}

	if (!Language_ExternalFileExists(g_RequestedCode))
	{
		LoadEnglish();
		return true;
	}

	std::string text;

	if (!Language_ReadExternalFile(g_RequestedCode, text))
	{
		LoadEnglish();
		return false;
	}

	std::wstring parsed[TEXT_COUNT];
	bool present[TEXT_COUNT];

	if (!Language_ParseCatalog(text, parsed, present))
	{
		LoadEnglish();
		return false;
	}

	std::wstring fallback[TEXT_COUNT];

	if (!DecodeCatalogInto(*english, fallback))
	{
		LoadEnglish();
		return false;
	}

	for (int i = 0; i < TEXT_COUNT; i++)
	{
		if (present[i] && Language_TextsCompatible(fallback[i], parsed[i]))
			g_Active[i] = parsed[i];
		else
			g_Active[i] = fallback[i];
	}

	g_ActiveCode = g_RequestedCode;
	return true;
}

void Language_Unload()
{
	ClearActive();
}

const std::wstring& TS(TextId id)
{
	int index = static_cast<int>(id);

	if ((index < 0) || (index >= TEXT_COUNT))
		return g_Empty;

	if (g_Active[index].empty())
	{
		const LanguageCatalogEntry* catalog = EnglishCatalog();

		if (catalog != nullptr)
			DecodeIndex(*catalog, index, g_Active[index]);
	}

	return g_Active[index];
}

const wchar_t* T(TextId id)
{
	return TS(id).c_str();
}

static void ReplaceToken(std::wstring& text, const std::wstring& token,
	const std::wstring& argument)
{
	size_t position = 0;

	while ((position = text.find(token, position)) != std::wstring::npos)
	{
		text.replace(position, token.size(), argument);
		position += argument.size();
	}
}

std::wstring FormatText(TextId id, const std::wstring& argument)
{
	std::wstring text = TS(id);
	ReplaceToken(text, L"{0}", argument);
	return text;
}

std::wstring FormatText(TextId id, const std::wstring& first,
	const std::wstring& second)
{
	std::wstring text = TS(id);
	ReplaceToken(text, L"{0}", first);
	ReplaceToken(text, L"{1}", second);
	return text;
}

static int NotificationIndex(TextId id)
{
	for (int i = 0; i < kLanguageNotificationCount; i++)
	{
		if (kLanguageNotificationIds[i] == static_cast<int>(id))
			return i;
	}

	return -1;
}

std::wstring Language_NotifyText(TextId id)
{
	int index = NotificationIndex(id);

	if (index < 0)
		return std::wstring();

	const LanguageCatalogEntry* catalog =
		NotificationCatalogForCode(g_ActiveCode);

	if (catalog == nullptr)
		catalog = NotificationCatalogForCode("en");

	std::wstring text;

	if ((catalog == nullptr) || !DecodeIndex(*catalog, index, text))
		return std::wstring();

	return text;
}

std::wstring Language_FormatNotify(TextId id, const std::wstring& argument)
{
	std::wstring text = Language_NotifyText(id);
	ReplaceToken(text, L"{0}", argument);
	return text;
}

const std::string& Language_ActiveCode()
{
	return g_ActiveCode;
}
