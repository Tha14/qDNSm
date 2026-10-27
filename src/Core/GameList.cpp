#include "GameList.hpp"

#ifndef QDNSM_HOST_TEST
#include "../Utils/FileSystem.hpp"
#endif

static const size_t MAX_GAME_FILE = 2 * 1024 * 1024;
static const size_t MAX_GAMES_PER_SECTION = 512;
static const size_t MAX_ALIASES = 16;
static const size_t MAX_GAME_LENGTH = 96;
static const size_t MAX_ALIAS_LENGTH = 64;

static bool IsPrintable(const char* text, size_t length)
{
	for (size_t i = 0; i < length; i++)
	{
		unsigned char character = static_cast<unsigned char>(text[i]);
		if ((character < 0x20) || (character > 0x7E))
			return false;
	}

	return true;
}

static void Trim(const char*& text, size_t& length)
{
	size_t start = 0;
	size_t end = length;

	while ((start < end) && ((text[start] == ' ') || (text[start] == '\t')))
		start++;

	while ((end > start) && ((text[end - 1] == ' ') || (text[end - 1] == '\t')))
		end--;

	text += start;
	length = end - start;
}

static bool RangeEquals(const char* text, size_t length, const char* value)
{
	size_t valueLength = 0;

	while (value[valueLength] != '\0')
		valueLength++;

	if (length != valueLength)
		return false;

	for (size_t i = 0; i < length; i++)
	{
		if (text[i] != value[i])
			return false;
	}

	return true;
}

static bool IsValidIpv4(const char* text, size_t length)
{
	if ((length == 0) || (length > 15))
		return false;

	size_t octets = 0;
	size_t digits = 0;
	int value = 0;

	for (size_t i = 0; i < length; i++)
	{
		char character = text[i];

		if (character == '.')
		{
			if (digits == 0)
				return false;

			octets++;
			if (octets > 3)
				return false;

			digits = 0;
			value = 0;
			continue;
		}

		if ((character < '0') || (character > '9'))
			return false;

		digits++;
		if (digits > 3)
			return false;

		value = (value * 10) + (character - '0');
		if (value > 255)
			return false;
	}

	return (octets == 3) && (digits > 0);
}

static std::wstring Widen(const char* text, size_t length)
{
	std::wstring out;

	for (size_t i = 0; i < length; i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));

	return out;
}

static bool HasSection(const std::vector<GameListSection>& sections, const std::string& ip)
{
	for (size_t i = 0; i < sections.size(); i++)
	{
		if (sections[i].ip == ip)
			return true;
	}

	return false;
}

bool ParseGameList(const char* data, size_t size, std::vector<GameListSection>& out)
{
	out.clear();

	if ((data == nullptr) && (size != 0))
		return false;

	if ((size >= 3) &&
		(static_cast<unsigned char>(data[0]) == 0xEF) &&
		(static_cast<unsigned char>(data[1]) == 0xBB) &&
		(static_cast<unsigned char>(data[2]) == 0xBF))
	{
		data += 3;
		size -= 3;
	}

	std::vector<bool> aliasSeen;
	size_t current = static_cast<size_t>(-1);

	size_t cursor = 0;

	while (cursor < size)
	{
		size_t lineEnd = cursor;

		while ((lineEnd < size) && (data[lineEnd] != '\n'))
			lineEnd++;

		const char* line = data + cursor;
		size_t lineLength = lineEnd - cursor;

		if ((lineLength > 0) && (line[lineLength - 1] == '\r'))
			lineLength--;

		cursor = lineEnd + 1;

		Trim(line, lineLength);

		if (lineLength == 0)
			continue;

		if ((line[0] == '#') || (line[0] == ';'))
			continue;

		if (line[0] == '[')
		{
			if (line[lineLength - 1] != ']')
				return false;

			const char* section = line + 1;
			size_t sectionLength = lineLength - 2;

			Trim(section, sectionLength);

			if (!IsValidIpv4(section, sectionLength))
				return false;

			std::string ip(section, sectionLength);

			if (HasSection(out, ip))
				return false;

			GameListSection entry{};
			entry.ip = ip;
			out.push_back(entry);
			aliasSeen.push_back(false);
			current = out.size() - 1;
			continue;
		}

		if (current == static_cast<size_t>(-1))
			return false;

		size_t separator = lineLength;

		for (size_t i = 0; i < lineLength; i++)
		{
			if (line[i] == '=')
			{
				separator = i;
				break;
			}
		}

		if (separator == lineLength)
			return false;

		const char* key = line;
		size_t keyLength = separator;

		const char* value = line + separator + 1;
		size_t valueLength = lineLength - separator - 1;

		Trim(key, keyLength);
		Trim(value, valueLength);

		if (keyLength == 0)
			return false;

		if (RangeEquals(key, keyLength, "Game"))
		{
			if ((valueLength == 0) || (valueLength > MAX_GAME_LENGTH))
				return false;

			if (!IsPrintable(value, valueLength))
				return false;

			if (out[current].games.size() >= MAX_GAMES_PER_SECTION)
				return false;

			out[current].games.push_back(Widen(value, valueLength));
			continue;
		}

		if (RangeEquals(key, keyLength, "Aliases"))
		{
			if (aliasSeen[current])
				return false;

			aliasSeen[current] = true;

			std::vector<std::wstring> aliases;
			size_t start = 0;

			for (size_t i = 0; i <= valueLength; i++)
			{
				if ((i != valueLength) && (value[i] != '|'))
					continue;

				const char* alias = value + start;
				size_t aliasLength = i - start;

				Trim(alias, aliasLength);

				if ((aliasLength == 0) || (aliasLength > MAX_ALIAS_LENGTH))
					return false;

				if (!IsPrintable(alias, aliasLength))
					return false;

				aliases.push_back(Widen(alias, aliasLength));
				start = i + 1;
			}

			if ((aliases.size() > MAX_ALIASES) ||
				((out[current].aliases.size() + aliases.size()) > MAX_ALIASES))
			{
				return false;
			}

			for (size_t i = 0; i < aliases.size(); i++)
				out[current].aliases.push_back(aliases[i]);

			continue;
		}

		return false;
	}

	return true;
}

bool ValidateGameList(const std::vector<GameListSection>& sections)
{
	if (sections.empty())
		return false;

	for (size_t i = 0; i < sections.size(); i++)
	{
		if (sections[i].games.empty())
			return false;
	}

	return true;
}

const GameListSection* FindGameListSection(const std::vector<GameListSection>& sections,
	const std::string& ip)
{
	for (size_t i = 0; i < sections.size(); i++)
	{
		if (sections[i].ip == ip)
			return &sections[i];
	}

	return nullptr;
}

#ifndef QDNSM_HOST_TEST
GameListLoadResult LoadGameList(const std::string& path, std::vector<GameListSection>& out)
{
	out.clear();

	if (!FileExist(path))
		return GameList_Missing;

	std::string content;

	if (!ReadFileText(path, content, MAX_GAME_FILE))
		return GameList_Unreadable;

	if (!ParseGameList(content.empty() ? nullptr : content.data(), content.size(), out))
		return GameList_Invalid;

	return GameList_Loaded;
}
#endif
