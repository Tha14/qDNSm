#include "GameList.hpp"

#ifndef QDNSM_HOST_TEST
#include "../Utils/FileSystem.hpp"
#endif

static const size_t MAX_GAME_FILE = 2 * 1024 * 1024;
static const size_t MAX_GAMES_PER_SECTION = 512;
static const size_t MAX_ALIASES = 16;
static const size_t MAX_GAME_LENGTH = 96;
static const size_t MAX_ALIAS_LENGTH = 64;
static const size_t MAX_NOTE_LENGTH = 512;

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

static bool RangeEqualsNoCase(const char* text, size_t length, const char* value)
{
	size_t valueLength = 0;

	while (value[valueLength] != '\0')
		valueLength++;

	if (length != valueLength)
		return false;

	for (size_t i = 0; i < length; i++)
	{
		char left = text[i];
		char right = value[i];

		if ((left >= 'A') && (left <= 'Z'))
			left = static_cast<char>(left - 'A' + 'a');

		if ((right >= 'A') && (right <= 'Z'))
			right = static_cast<char>(right - 'A' + 'a');

		if (left != right)
			return false;
	}

	return true;
}

static bool StartsWithNoCase(const char* text, size_t length, const char* prefix)
{
	size_t prefixLength = 0;

	while (prefix[prefixLength] != '\0')
		prefixLength++;

	if (length < prefixLength)
		return false;

	return RangeEqualsNoCase(text, prefixLength, prefix);
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

static uint32_t ParseGameTags(const char* text, size_t length)
{
	uint32_t tags = GameTag_None;
	size_t cursor = 0;

	while (cursor < length)
	{
		while ((cursor < length) && ((text[cursor] == ',') || (text[cursor] == ' ') ||
			(text[cursor] == '\t')))
		{
			cursor++;
		}

		size_t start = cursor;

		while ((cursor < length) && (text[cursor] != ',') && (text[cursor] != ' ') &&
			(text[cursor] != '\t'))
		{
			cursor++;
		}

		const char* token = text + start;
		size_t tokenLength = cursor - start;

		if (RangeEqualsNoCase(token, tokenLength, "requires-patch"))
			tags |= GameTag_RequiresPatch;
		else if (RangeEqualsNoCase(token, tokenLength, "requires-whitelist"))
			tags |= GameTag_RequiresWhitelist;
		else if (RangeEqualsNoCase(token, tokenLength, "discord-account-linking"))
			tags |= GameTag_DiscordAccountLinking;
	}

	return tags;
}

static void AppendGameNote(std::wstring& target, const char* text, size_t length)
{
	if ((length == 0) || !IsPrintable(text, length))
		return;

	size_t extra = (target.empty() ? 0 : 1) + length;

	if ((target.size() + extra) > MAX_NOTE_LENGTH)
		return;

	if (!target.empty())
		target += L'\n';

	target += Widen(text, length);
}

static void ParseGameMetadata(const char* line, size_t length, GameListSection& section,
	size_t lastGame)
{
	size_t cursor = 1;

	while ((cursor < length) && ((line[cursor] == ' ') || (line[cursor] == '\t')))
		cursor++;

	const char* text = line + cursor;
	size_t textLength = length - cursor;

	if (StartsWithNoCase(text, textLength, "tags:"))
	{
		if (lastGame == static_cast<size_t>(-1))
			return;

		section.games[lastGame].tags |= ParseGameTags(text + 5, textLength - 5);
		return;
	}

	if (!StartsWithNoCase(text, textLength, "note:"))
		return;

	const char* value = text + 5;
	size_t valueLength = textLength - 5;

	Trim(value, valueLength);

	if (lastGame != static_cast<size_t>(-1))
		AppendGameNote(section.games[lastGame].note, value, valueLength);
	else if (section.games.empty())
		AppendGameNote(section.note, value, valueLength);
}

static bool IsOpenSpyKey(const char* text, size_t length)
{
	return RangeEquals(text, length, kOpenSpyGameListKey);
}

static bool IsValidSectionKey(const char* text, size_t length)
{
	return IsValidIpv4(text, length) || IsOpenSpyKey(text, length);
}

static bool HasSection(const std::vector<GameListSection>& sections, const std::string& key)
{
	for (size_t i = 0; i < sections.size(); i++)
	{
		if (sections[i].key == key)
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
	size_t lastGame = static_cast<size_t>(-1);

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
		{
			if (current != static_cast<size_t>(-1))
				ParseGameMetadata(line, lineLength, out[current], lastGame);

			continue;
		}

		if (line[0] == '[')
		{
			if (line[lineLength - 1] != ']')
				return false;

			const char* section = line + 1;
			size_t sectionLength = lineLength - 2;

			Trim(section, sectionLength);

			if (!IsValidSectionKey(section, sectionLength))
				return false;

			std::string key(section, sectionLength);

			if (HasSection(out, key))
				return false;

			GameListSection entry{};
			entry.key = key;
			out.push_back(entry);
			aliasSeen.push_back(false);
			current = out.size() - 1;
			lastGame = static_cast<size_t>(-1);
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

			GameListGame game;
			game.name = Widen(value, valueLength);
			out[current].games.push_back(game);
			lastGame = out[current].games.size() - 1;
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

			lastGame = static_cast<size_t>(-1);
			continue;
		}

		return false;
	}

	return true;
}

bool ParseGameListSection(const char* data, size_t size, const std::string& key,
	GameListSection& out, bool& found)
{
	out = GameListSection{};
	found = false;

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

	bool inTarget = false;
	bool aliasSeen = false;
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

			if (!IsValidSectionKey(section, sectionLength))
				return false;

			std::string sectionKey(section, sectionLength);

			if (sectionKey == key)
			{
				if (found)
					return false;

				found = true;
				out.key = sectionKey;
				inTarget = true;
				aliasSeen = false;
			}
			else
				inTarget = false;

			continue;
		}

		if (!inTarget)
			continue;

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

		const char* name = line;
		size_t nameLength = separator;

		const char* value = line + separator + 1;
		size_t valueLength = lineLength - separator - 1;

		Trim(name, nameLength);
		Trim(value, valueLength);

		if (nameLength == 0)
			return false;

		if (RangeEquals(name, nameLength, "Game"))
		{
			if ((valueLength == 0) || (valueLength > MAX_GAME_LENGTH) ||
				!IsPrintable(value, valueLength))
			{
				return false;
			}

			if (out.games.size() >= MAX_GAMES_PER_SECTION)
				return false;

			GameListGame game;
			game.name = Widen(value, valueLength);
			out.games.push_back(game);
			continue;
		}

		if (RangeEquals(name, nameLength, "Aliases"))
		{
			if (aliasSeen)
				return false;

			aliasSeen = true;

			std::vector<std::wstring> aliases;
			size_t start = 0;

			for (size_t i = 0; i <= valueLength; i++)
			{
				if ((i != valueLength) && (value[i] != '|'))
					continue;

				const char* alias = value + start;
				size_t aliasLength = i - start;

				Trim(alias, aliasLength);

				if ((aliasLength == 0) || (aliasLength > MAX_ALIAS_LENGTH) ||
					!IsPrintable(alias, aliasLength))
				{
					return false;
				}

				aliases.push_back(Widen(alias, aliasLength));
				start = i + 1;
			}

			if ((aliases.size() > MAX_ALIASES) ||
				((out.aliases.size() + aliases.size()) > MAX_ALIASES))
			{
				return false;
			}

			for (size_t i = 0; i < aliases.size(); i++)
				out.aliases.push_back(aliases[i]);

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

		if ((sections[i].key == kOpenSpyGameListKey) && !sections[i].aliases.empty())
			return false;
	}

	return true;
}

const GameListSection* FindGameListSection(const std::vector<GameListSection>& sections,
	const std::string& key)
{
	for (size_t i = 0; i < sections.size(); i++)
	{
		if (sections[i].key == key)
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

GameListLoadResult LoadGameListSection(const std::string& path, const std::string& key,
	GameListSection& out)
{
	out = GameListSection{};

	if (!FileExist(path))
		return GameList_Missing;

	std::string content;

	if (!ReadFileText(path, content, MAX_GAME_FILE))
		return GameList_Unreadable;

	bool found = false;

	if (!ParseGameListSection(content.empty() ? nullptr : content.data(), content.size(),
		key, out, found))
	{
		return GameList_Invalid;
	}

	if (!found)
		out = GameListSection{};

	return GameList_Loaded;
}
#endif
