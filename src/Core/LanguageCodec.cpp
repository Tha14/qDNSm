#include "LanguageCodec.hpp"

static const char* const kLanguageCodes[] =
{
	"auto",
	"en",
	"de",
	"es",
	"fr",
	"it",
	"pt_BR",
	"pt_PT",
	"ru",
	"pl",
	"ja",
	"zh_CN",
};

static const int kLanguageCodeCount =
	static_cast<int>(sizeof(kLanguageCodes) / sizeof(kLanguageCodes[0]));

static void AppendUtf8(std::wstring& out, unsigned int codepoint)
{
	out += static_cast<wchar_t>(codepoint);
}

std::wstring DecodeUtf8(const char* text)
{
	std::wstring out;

	if (text == nullptr)
		return out;

	const unsigned char* cursor = reinterpret_cast<const unsigned char*>(text);

	while (*cursor != 0)
	{
		unsigned int codepoint = 0;
		int continuationCount = 0;

		if (*cursor < 0x80)
		{
			codepoint = *cursor;
			cursor++;
		}
		else if ((*cursor & 0xE0) == 0xC0)
		{
			codepoint = *cursor & 0x1F;
			continuationCount = 1;
			cursor++;
		}
		else if ((*cursor & 0xF0) == 0xE0)
		{
			codepoint = *cursor & 0x0F;
			continuationCount = 2;
			cursor++;
		}
		else if ((*cursor & 0xF8) == 0xF0)
		{
			codepoint = *cursor & 0x07;
			continuationCount = 3;
			cursor++;
		}
		else
		{
			AppendUtf8(out, 0xFFFD);
			cursor++;
			continue;
		}

		bool valid = true;

		for (int i = 0; i < continuationCount; i++)
		{
			if ((*cursor & 0xC0) != 0x80)
			{
				valid = false;
				break;
			}

			codepoint = (codepoint << 6) | (*cursor & 0x3F);
			cursor++;
		}

		if (!valid || (codepoint > 0x10FFFF) ||
			((codepoint >= 0xD800) && (codepoint <= 0xDFFF)))
		{
			AppendUtf8(out, 0xFFFD);
			continue;
		}

		AppendUtf8(out, codepoint);
	}

	return out;
}

bool Language_IsValidCode(const std::string& code)
{
	for (int i = 0; i < kLanguageCodeCount; i++)
	{
		if (code == kLanguageCodes[i])
			return true;
	}

	return false;
}

const char* Language_MapSystemLanguage(int raw)
{
	switch (raw)
	{
		case 0:
			return "de";
		case 1:
			return "en";
		case 2:
			return "es";
		case 3:
			return "fr";
		case 4:
			return "it";
		case 6:
			return "pt_PT";
		case 7:
			return "ru";
		case 8:
			return "ja";
		case 11:
			return "zh_CN";
		case 16:
			return "pl";
		case 17:
			return "pt_BR";
		case 18:
			return "en";
		default:
			return "en";
	}
}

int Language_IndexOfCode(const std::string& code)
{
	for (int i = 0; i < kLanguageCodeCount; i++)
	{
		if (code == kLanguageCodes[i])
			return i;
	}

	return 0;
}

const char* Language_CodeAtIndex(int index)
{
	if ((index < 0) || (index >= kLanguageCodeCount))
		return "auto";

	return kLanguageCodes[index];
}

int Language_CodeCount()
{
	return kLanguageCodeCount;
}
