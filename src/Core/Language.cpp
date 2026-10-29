#include "Language.hpp"
#include "LanguageCodec.hpp"
#include "LanguageCompression.hpp"

static std::wstring g_Active[TEXT_COUNT];
static std::wstring g_Empty;
static const LanguageCatalogEntry* g_ActiveCatalog = nullptr;
static unsigned char* g_ActiveRaw = nullptr;
static unsigned int g_ActiveRawSize = 0;
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

static void FreeRaw()
{
	if (g_ActiveRaw != nullptr)
	{
		delete[] g_ActiveRaw;
		g_ActiveRaw = nullptr;
		g_ActiveRawSize = 0;
	}
}

static bool EnsureRaw()
{
	if (g_ActiveCatalog == nullptr)
		return false;

	if (g_ActiveRaw != nullptr)
		return true;

	unsigned char* raw = new unsigned char[g_ActiveCatalog->rawSize];

	if (!Language_DecompressCatalog(*g_ActiveCatalog, raw, g_ActiveCatalog->rawSize))
	{
		delete[] raw;
		return false;
	}

	g_ActiveRaw = raw;
	g_ActiveRawSize = g_ActiveCatalog->rawSize;
	return true;
}

static const char* RawTextForIndex(int index)
{
	if (g_ActiveRaw == nullptr)
		return nullptr;

	const unsigned char* cursor = g_ActiveRaw;
	const unsigned char* end = g_ActiveRaw + g_ActiveRawSize;

	for (int i = 0; i < index; i++)
	{
		while ((cursor < end) && (*cursor != 0))
			cursor++;

		if (cursor >= end)
			return nullptr;

		cursor++;
	}

	return reinterpret_cast<const char*>(cursor);
}

static const char* TextForIndex(int index)
{
	const char* text = nullptr;

	if (g_ActiveCatalog != nullptr)
		text = RawTextForIndex(index);
	else
		text = kLanguageTable_en[index];

	if ((text == nullptr) || (text[0] == '\0'))
		text = kLanguageTable_en[index];

	return text;
}

static void ClearActive()
{
	for (int i = 0; i < TEXT_COUNT; i++)
	{
		std::wstring empty;
		empty.swap(g_Active[i]);
	}
}

bool Language_Init(const std::string& code)
{
	std::string selected = code;

	if ((selected == "auto") || !Language_IsValidCode(selected))
		selected = "en";

	const LanguageCatalogEntry* catalog = CatalogForCode(selected);

	if ((selected != "en") && (catalog == nullptr))
		selected = "en";

	if (selected == "en")
		catalog = nullptr;

	ClearActive();
	FreeRaw();

	g_ActiveCatalog = catalog;
	g_ActiveCode = selected;

	return true;
}

void Language_LoadAll()
{
	bool needsDecode = false;

	for (int i = 0; i < TEXT_COUNT; i++)
	{
		if (g_Active[i].empty())
		{
			needsDecode = true;
			break;
		}
	}

	if (needsDecode && (g_ActiveCatalog != nullptr))
		EnsureRaw();

	for (int i = 0; i < TEXT_COUNT; i++)
	{
		if (g_Active[i].empty())
			g_Active[i] = DecodeUtf8(TextForIndex(i));
	}

	FreeRaw();
}

void Language_Unload()
{
	ClearActive();
	FreeRaw();
}

const std::wstring& TS(TextId id)
{
	int index = static_cast<int>(id);

	if ((index < 0) || (index >= TEXT_COUNT))
		return g_Empty;

	if (g_Active[index].empty())
	{
		if (g_ActiveCatalog != nullptr)
			EnsureRaw();

		g_Active[index] = DecodeUtf8(TextForIndex(index));
	}

	return g_Active[index];
}

const wchar_t* T(TextId id)
{
	return TS(id).c_str();
}

std::wstring FormatText(TextId id, const std::wstring& argument)
{
	std::wstring text = TS(id);
	const std::wstring token = L"{0}";
	size_t position = 0;

	while ((position = text.find(token, position)) != std::wstring::npos)
	{
		text.replace(position, token.size(), argument);
		position += argument.size();
	}

	return text;
}

const std::string& Language_ActiveCode()
{
	return g_ActiveCode;
}
