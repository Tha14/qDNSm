#include "GameDirectory.hpp"

#include <algorithm>

#ifndef QDNSM_HOST_TEST
#include "../Utils/FileSystem.hpp"
#endif

static wchar_t LowerAscii(wchar_t character)
{
	if ((character >= L'A') && (character <= L'Z'))
		return static_cast<wchar_t>(character - L'A' + L'a');

	return character;
}

static void Trim(std::wstring& text)
{
	size_t start = 0;
	size_t end = text.size();

	while ((start < end) && ((text[start] == L' ') || (text[start] == L'\t')))
		start++;

	while ((end > start) && ((text[end - 1] == L' ') || (text[end - 1] == L'\t')))
		end--;

	text = text.substr(start, end - start);
}

static std::wstring Widen(const std::string& text)
{
	std::wstring out;

	for (size_t i = 0; i < text.size(); i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));

	return out;
}

static int CompareNoCase(const std::wstring& left, const std::wstring& right)
{
	size_t length = (left.size() < right.size()) ? left.size() : right.size();

	for (size_t i = 0; i < length; i++)
	{
		wchar_t leftCharacter = LowerAscii(left[i]);
		wchar_t rightCharacter = LowerAscii(right[i]);

		if (leftCharacter < rightCharacter)
			return -1;

		if (leftCharacter > rightCharacter)
			return 1;
	}

	if (left.size() < right.size())
		return -1;

	if (left.size() > right.size())
		return 1;

	return 0;
}

static bool EqualsNoCase(const std::wstring& left, const std::wstring& right)
{
	return CompareNoCase(left, right) == 0;
}

static bool ContainsNoCase(const std::vector<std::wstring>& values, const std::wstring& value)
{
	for (size_t i = 0; i < values.size(); i++)
	{
		if (EqualsNoCase(values[i], value))
			return true;
	}

	return false;
}

static bool ContainsToken(const std::wstring& text, const std::wstring& token)
{
	if (token.size() > text.size())
		return false;

	for (size_t i = 0; i + token.size() <= text.size(); i++)
	{
		size_t j = 0;
		while ((j < token.size()) && (LowerAscii(text[i + j]) == token[j]))
			j++;

		if (j == token.size())
			return true;
	}

	return false;
}

static bool ContainsToken(const std::string& text, const std::wstring& token)
{
	if (token.size() > text.size())
		return false;

	for (size_t i = 0; i + token.size() <= text.size(); i++)
	{
		size_t j = 0;
		while ((j < token.size()) &&
			(LowerAscii(static_cast<wchar_t>(static_cast<unsigned char>(text[i + j]))) == token[j]))
			j++;

		if (j == token.size())
			return true;
	}

	return false;
}

static bool ContainsToken(const std::vector<std::wstring>& values, const std::wstring& token)
{
	for (size_t i = 0; i < values.size(); i++)
	{
		if (ContainsToken(values[i], token))
			return true;
	}

	return false;
}

static bool RowLess(const GameDirectoryRow& left, const GameDirectoryRow& right)
{
	int compare = CompareNoCase(left.game, right.game);

	if (compare != 0)
		return compare < 0;

	std::wstring leftProject;
	if (!left.projects.empty())
		leftProject = left.projects[0];

	std::wstring rightProject;
	if (!right.projects.empty())
		rightProject = right.projects[0];

	compare = CompareNoCase(leftProject, rightProject);

	if (compare != 0)
		return compare < 0;

	std::string leftIp;
	if (!left.ips.empty())
		leftIp = left.ips[0];

	std::string rightIp;
	if (!right.ips.empty())
		rightIp = right.ips[0];

	return leftIp < rightIp;
}

static const size_t kMaxSearchRows = 64;

enum DirectoryTagMerge
{
	DirectoryTagMerge_Union,
	DirectoryTagMerge_Common,
	DirectoryTagMerge_First
};

/* Change this to DirectoryTagMerge_Common or DirectoryTagMerge_First to compare
 * grouped-row tag behaviour. */
static const DirectoryTagMerge kDirectoryTagMerge = DirectoryTagMerge_Union;

static uint32_t MergeGameTags(uint32_t current, uint32_t tags)
{
	switch (kDirectoryTagMerge)
	{
	case DirectoryTagMerge_Common:
		return current & tags;

	case DirectoryTagMerge_First:
		return current;

	default:
		return current | tags;
	}
}

static std::wstring JoinProjectNames(const std::vector<std::wstring>& projects)
{
	std::wstring out;

	for (size_t i = 0; i < projects.size(); i++)
	{
		if (i != 0)
			out += L" / ";

		out += projects[i];
	}

	return out;
}

static void AddNote(GameDirectoryRow& row, const std::vector<std::wstring>& projects,
	const std::wstring& text, bool server)
{
	if (text.empty())
		return;

	std::wstring project = JoinProjectNames(projects);

	for (size_t i = 0; i < row.notes.size(); i++)
	{
		if ((row.notes[i].server == server) && (row.notes[i].project == project) &&
			(row.notes[i].text == text))
		{
			return;
		}
	}

	GameDirectoryNote note;
	note.project = project;
	note.text = text;
	note.server = server;
	row.notes.push_back(note);
}

static void AddProjectNames(GameDirectoryRow& row, const std::vector<std::wstring>& values)
{
	for (size_t i = 0; i < values.size(); i++)
	{
		if (!ContainsNoCase(row.projects, values[i]))
			row.projects.push_back(values[i]);
	}
}

static void AddAliasNames(GameDirectoryRow& row, const std::vector<std::wstring>& values)
{
	for (size_t i = 0; i < values.size(); i++)
	{
		if (!ContainsNoCase(row.aliases, values[i]))
			row.aliases.push_back(values[i]);
	}
}

static void AddIp(GameDirectoryRow& row, const std::string& ip)
{
	for (size_t i = 0; i < row.ips.size(); i++)
	{
		if (row.ips[i] == ip)
			return;
	}

	row.ips.push_back(ip);
}

static bool SearchSection(const GameListSection& section,
	const std::vector<DnsServerEntry>& servers, const std::vector<std::wstring>& tokens,
	bool grouped, GameDirectoryRows& rows, bool& truncated)
{
	if (section.games.empty())
		return true;

	const bool openSpy = (section.key == kOpenSpyGameListKey);

	std::vector<std::wstring> projectNames;
	projectNames.reserve(servers.size() + (openSpy ? 1 : 0));

	if (openSpy)
		projectNames.push_back(Widen(section.key));
	else
	{
		for (size_t i = 0; i < servers.size(); i++)
		{
			if (servers[i].primary != section.key)
				continue;

			std::wstring name = servers[i].name;
			Trim(name);

			if (name.empty() || ContainsNoCase(projectNames, name))
				continue;

			projectNames.push_back(name);
		}
	}

	std::vector<std::wstring> aliases;
	aliases.reserve(section.aliases.size());

	for (size_t i = 0; i < section.aliases.size(); i++)
	{
		std::wstring alias = section.aliases[i];
		Trim(alias);

		if (alias.empty() || ContainsNoCase(projectNames, alias) ||
			ContainsNoCase(aliases, alias))
			continue;

		aliases.push_back(alias);
	}

	const std::vector<std::wstring>& projects = projectNames.empty() ? aliases : projectNames;

	for (size_t i = 0; i < section.games.size(); i++)
	{
		const GameListGame& game = section.games[i];
		bool duplicate = false;

		for (size_t j = 0; j < i; j++)
		{
			if (EqualsNoCase(section.games[j].name, game.name))
			{
				duplicate = true;
				break;
			}
		}

		if (duplicate)
			continue;

		bool match = true;

		for (size_t j = 0; j < tokens.size(); j++)
		{
			if (!ContainsToken(game.name, tokens[j]) &&
				!ContainsToken(projects, tokens[j]) &&
				!ContainsToken(aliases, tokens[j]) &&
				!ContainsToken(section.key, tokens[j]))
			{
				match = false;
				break;
			}
		}

		if (!match)
			continue;

		if (!grouped)
		{
			if (rows.size() >= kMaxSearchRows)
			{
				truncated = true;
				return false;
			}

			GameDirectoryRow row{};
			row.game = game.name;
			row.projects = projects;
			row.aliases = aliases;
			row.tags = game.tags;

			AddNote(row, projects, game.note, false);
			AddNote(row, projects, section.note, true);

			if (!openSpy)
				row.ips.push_back(section.key);

			rows.push_back(row);
			continue;
		}

		size_t index = rows.size();

		for (size_t j = 0; j < rows.size(); j++)
		{
			if (EqualsNoCase(rows[j].game, game.name))
			{
				index = j;
				break;
			}
		}

		if (index == rows.size())
		{
			if (rows.size() >= kMaxSearchRows)
			{
				truncated = true;
				continue;
			}

			GameDirectoryRow row{};
			row.game = game.name;
			row.tags = game.tags;

			AddNote(row, projects, game.note, false);
			AddNote(row, projects, section.note, true);

			rows.push_back(row);
			index = rows.size() - 1;
		}
		else
		{
			GameDirectoryRow& row = rows[index];
			row.tags = MergeGameTags(row.tags, game.tags);

			AddNote(row, projects, game.note, false);
			AddNote(row, projects, section.note, true);
		}

		GameDirectoryRow& row = rows[index];
		AddProjectNames(row, projects);
		AddAliasNames(row, aliases);

		if (!openSpy)
			AddIp(row, section.key);
	}

	return true;
}

static bool IsSectionHeader(const std::string& text, size_t start, size_t end)
{
	size_t i = start;

	if ((i == 0) && (text.size() >= 3) &&
		(static_cast<unsigned char>(text[0]) == 0xEF) &&
		(static_cast<unsigned char>(text[1]) == 0xBB) &&
		(static_cast<unsigned char>(text[2]) == 0xBF))
	{
		i += 3;
	}

	while ((i < end) && ((text[i] == ' ') || (text[i] == '\t')))
		i++;

	while ((end > i) && ((text[end - 1] == ' ') || (text[end - 1] == '\t') ||
		(text[end - 1] == '\r')))
	{
		end--;
	}

	return (i < end) && (text[i] == '[') && (text[end - 1] == ']');
}

GameListLoadResult GameDirectory_SearchText(const std::string& text,
	const std::vector<DnsServerEntry>& servers, const std::string& query, bool grouped,
	GameDirectoryRows& rows, bool& truncated)
{
	rows.clear();
	truncated = false;

	std::wstring queryText = Widen(query);
	std::vector<std::wstring> tokens;
	std::wstring token;

	for (size_t i = 0; i <= queryText.size(); i++)
	{
		if ((i == queryText.size()) || (queryText[i] == L' ') || (queryText[i] == L'\t'))
		{
			if (!token.empty())
			{
				tokens.push_back(token);
				token.clear();
			}

			continue;
		}

		token += LowerAscii(queryText[i]);
	}

	bool parsedAny = false;
	bool stop = false;
	size_t sectionStart = std::string::npos;
	size_t cursor = 0;

	while ((cursor <= text.size()) && !stop)
	{
		size_t lineEnd = text.find('\n', cursor);
		size_t lineStop = (lineEnd == std::string::npos) ? text.size() : lineEnd;

		if (IsSectionHeader(text, cursor, lineStop))
		{
			if (sectionStart != std::string::npos)
			{
				std::string chunk = text.substr(sectionStart, cursor - sectionStart);
				std::vector<GameListSection> parsed;

				if (ParseGameList(chunk.data(), chunk.size(), parsed) && !parsed.empty())
				{
					parsedAny = true;
					stop = !SearchSection(parsed[0], servers, tokens, grouped, rows, truncated);
				}
			}

			sectionStart = cursor;
		}

		if (lineEnd == std::string::npos)
			break;

		cursor = lineEnd + 1;
	}

	if (!stop && (sectionStart != std::string::npos))
	{
		std::string chunk = text.substr(sectionStart);
		std::vector<GameListSection> parsed;

		if (ParseGameList(chunk.data(), chunk.size(), parsed) && !parsed.empty())
		{
			parsedAny = true;
			SearchSection(parsed[0], servers, tokens, grouped, rows, truncated);
		}
	}

	std::sort(rows.begin(), rows.end(), RowLess);

	return parsedAny ? GameList_Loaded : GameList_Invalid;
}

#ifndef QDNSM_HOST_TEST
static const char kGameFileName[] = "qDNSm-server-games.ini";

static bool s_Available = false;
static bool s_Initialized = false;

void GameDirectory_RefreshAvailability()
{
	s_Available = FileExist(GetCurrentDir() + kGameFileName);
	s_Initialized = true;
}

bool GameDirectory_IsAvailable()
{
	if (!s_Initialized)
		GameDirectory_RefreshAvailability();

	return s_Available;
}

void GameDirectory_OnFrame(bool menuOpen)
{
	static bool s_WasOpen = false;

	if (menuOpen && !s_WasOpen)
		GameDirectory_RefreshAvailability();

	s_WasOpen = menuOpen;
}

GameListLoadResult GameDirectory_Search(const std::vector<DnsServerEntry>& servers,
	const std::string& query, bool grouped, GameDirectoryRows& rows, bool& truncated)
{
	rows.clear();
	truncated = false;

	std::string path = GetCurrentDir() + kGameFileName;

	if (!FileExist(path))
		return GameList_Missing;

	std::string text;

	if (!ReadFileText(path, text))
		return GameList_Unreadable;

	return GameDirectory_SearchText(text, servers, query, grouped, rows, truncated);
}
#endif
