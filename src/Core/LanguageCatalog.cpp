#include "LanguageCatalog.hpp"
#include "LanguageCodec.hpp"

#include <algorithm>
#include <vector>

static bool IsSpace(char character)
{
	return (character == ' ') || (character == '\t') || (character == '\n') ||
		(character == '\r') || (character == '\f') || (character == '\v');
}

static std::string Trim(const std::string& text)
{
	size_t begin = 0;
	size_t end = text.size();

	while ((begin < end) && IsSpace(text[begin]))
		begin++;

	while ((end > begin) && IsSpace(text[end - 1]))
		end--;

	return text.substr(begin, end - begin);
}

static bool IsKeyStart(char character)
{
	return ((character >= 'A') && (character <= 'Z')) ||
		((character >= 'a') && (character <= 'z')) || (character == '_');
}

static bool IsKeyCharacter(char character)
{
	return IsKeyStart(character) || ((character >= '0') && (character <= '9'));
}

static bool IsValidKey(const std::string& key)
{
	if (key.empty() || !IsKeyStart(key[0]))
		return false;

	for (size_t i = 1; i < key.size(); i++)
	{
		if (!IsKeyCharacter(key[i]))
			return false;
	}

	return true;
}

static int HexValue(char character)
{
	if ((character >= '0') && (character <= '9'))
		return character - '0';
	if ((character >= 'A') && (character <= 'F'))
		return character - 'A' + 10;
	if ((character >= 'a') && (character <= 'f'))
		return character - 'a' + 10;
	return -1;
}

static bool ParseHex(const std::string& text, size_t begin, size_t count,
	unsigned int& value)
{
	value = 0;

	for (size_t i = 0; i < count; i++)
	{
		int digit = HexValue(text[begin + i]);

		if (digit < 0)
			return false;

		value = (value << 4) | static_cast<unsigned int>(digit);
	}

	return true;
}

static void AppendUtf8(std::string& out, unsigned int codepoint)
{
	if (codepoint <= 0x7F)
	{
		out += static_cast<char>(codepoint);
	}
	else if (codepoint <= 0x7FF)
	{
		out += static_cast<char>(0xC0 | (codepoint >> 6));
		out += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else if (codepoint <= 0xFFFF)
	{
		out += static_cast<char>(0xE0 | (codepoint >> 12));
		out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else
	{
		out += static_cast<char>(0xF0 | (codepoint >> 18));
		out += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		out += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		out += static_cast<char>(0x80 | (codepoint & 0x3F));
	}
}

static bool DecodeEscapes(const std::string& value, std::wstring& out)
{
	std::string encoded;
	size_t i = 0;

	while (i < value.size())
	{
		if (value[i] != '\\')
		{
			encoded += value[i++];
			continue;
		}

		i++;

		if (i >= value.size())
			return false;

		char escape = value[i++];

		if (escape == 'n')
			encoded += '\n';
		else if (escape == 'r')
			encoded += '\r';
		else if (escape == 't')
			encoded += '\t';
		else if (escape == '\\')
			encoded += '\\';
		else if (escape == '"')
			encoded += '"';
		else if ((escape == 'u') || (escape == 'U'))
		{
			const size_t digits = (escape == 'u') ? 4 : 8;
			unsigned int codepoint = 0;

			if ((i + digits) > value.size())
				return false;
			if (!ParseHex(value, i, digits, codepoint))
				return false;

			i += digits;

			if ((escape == 'u') && (codepoint >= 0xD800) &&
				(codepoint <= 0xDBFF))
			{
				unsigned int low = 0;

				if (((i + 6) > value.size()) || (value[i] != '\\') ||
					(value[i + 1] != 'u'))
					return false;
				if (!ParseHex(value, i + 2, 4, low))
					return false;
				if ((low < 0xDC00) || (low > 0xDFFF))
					return false;

				codepoint = 0x10000 + ((codepoint - 0xD800) << 10) +
					(low - 0xDC00);
				i += 6;
			}

			if ((codepoint > 0x10FFFF) ||
				((codepoint >= 0xD800) && (codepoint <= 0xDFFF)))
				return false;

			AppendUtf8(encoded, codepoint);
		}
		else
		{
			return false;
		}
	}

	out = DecodeUtf8(encoded.c_str());
	return true;
}

static int FindKey(const std::string& key)
{
	for (int i = 0; i < TEXT_COUNT; i++)
	{
		if (key == kLanguageKeys[i])
			return i;
	}

	return -1;
}

static bool SameTokens(const std::vector<std::wstring>& first,
	const std::vector<std::wstring>& second)
{
	if (first.size() != second.size())
		return false;

	std::vector<std::wstring> left = first;
	std::vector<std::wstring> right = second;
	std::sort(left.begin(), left.end());
	std::sort(right.begin(), right.end());
	return left == right;
}

static std::vector<std::wstring> CollectPlaceholders(const std::wstring& text)
{
	std::vector<std::wstring> tokens;

	for (size_t i = 0; i < text.size(); i++)
	{
		if (text[i] != L'{')
			continue;

		size_t end = i + 1;

		while ((end < text.size()) && (text[end] >= L'0') &&
			(text[end] <= L'9'))
			end++;

		if ((end > (i + 1)) && (end < text.size()) && (text[end] == L'}'))
		{
			tokens.push_back(text.substr(i, end - i + 1));
			i = end;
		}
	}

	return tokens;
}

static std::vector<std::wstring> CollectGlyphs(const std::wstring& text)
{
	std::vector<std::wstring> glyphs;

	for (size_t i = 0; i < text.size(); i++)
	{
		if ((text[i] >= static_cast<wchar_t>(0xE000)) &&
			(text[i] <= static_cast<wchar_t>(0xF8FF)))
			glyphs.push_back(std::wstring(1, text[i]));
	}

	return glyphs;
}

bool Language_TextsCompatible(const std::wstring& english,
	const std::wstring& translated)
{
	return SameTokens(CollectPlaceholders(english),
		CollectPlaceholders(translated)) &&
		SameTokens(CollectGlyphs(english), CollectGlyphs(translated));
}

bool Language_ParseCatalog(const std::string& text, std::wstring* values,
	bool* present)
{
	for (int i = 0; i < TEXT_COUNT; i++)
	{
		values[i].clear();
		present[i] = false;
	}

	size_t position = 0;

	if ((text.size() >= 3) &&
		(static_cast<unsigned char>(text[0]) == 0xEF) &&
		(static_cast<unsigned char>(text[1]) == 0xBB) &&
		(static_cast<unsigned char>(text[2]) == 0xBF))
		position = 3;

	bool found = false;

	while (position <= text.size())
	{
		size_t lineEnd = text.find('\n', position);

		if (lineEnd == std::string::npos)
			lineEnd = text.size();

		std::string line = text.substr(position, lineEnd - position);

		if (!line.empty() && (line[line.size() - 1] == '\r'))
			line.erase(line.size() - 1);

		std::string stripped = Trim(line);

		if (!stripped.empty() && (stripped[0] != '#') &&
			(stripped[0] != ';'))
		{
			size_t equals = line.find('=');

			if (equals == std::string::npos)
				return false;

			std::string key = Trim(line.substr(0, equals));

			if (!IsValidKey(key))
				return false;

			std::wstring decoded;

			if (!DecodeEscapes(Trim(line.substr(equals + 1)), decoded))
				return false;

			int index = FindKey(key);

			if (index >= 0)
			{
				if (present[index])
					return false;

				values[index] = decoded;
				present[index] = !decoded.empty();
				found = true;
			}
		}

		if (lineEnd == text.size())
			break;

		position = lineEnd + 1;
	}

	return found;
}
