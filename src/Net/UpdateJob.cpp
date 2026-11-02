#include "UpdateJob.hpp"

#include "DnsList.hpp"
#include "NetProbeProtocol.hpp"

#include "../Core/Config.hpp"
#include "../Core/GameList.hpp"
#include "../Core/Version.hpp"

#include <cell/cell_fs.h>
#include <stddef.h>
#include <string>
#include <vector>

static const size_t MAX_CONFIG_FILE = 64 * 1024;
static const size_t MAX_GAME_FILE = 2 * 1024 * 1024;

static const char* SECTION_SETTINGS = "Settings";
static const char* SECTION_DNS_SERVER_PREFIX = "DNSServer";

static const char* KEY_SECONDARY_DNS = "SecondaryDNS";
static const char* KEY_SHOW_NOTIFICATION = "ShowNotification";
static const char* KEY_AUTO_RECONNECT = "AutoReconnect";
static const char* KEY_SHOW_SUPPORTED_GAMES = "ShowSupportedGames";
static const char* KEY_LANGUAGE = "Language";
static const char* KEY_MANAGED_FORMAT = "ManagedFormat";
static const char* KEY_ONLINE_VERSION = "OnlineVersion";
static const char* KEY_ONLINE_URL = "OnlineUrl";
static const char* KEY_NAME = "Name";
static const char* KEY_PRIMARY = "Primary";
static const char* KEY_MANAGED = "Managed";

struct ConfigData
{
	std::string secondaryDns{"8.8.8.8"};
	bool showNotification{true};
	bool autoReconnect{true};
	bool showSupportedGames{true};
	std::string language{"auto"};
	bool managedFormat{};
	std::string onlineVersion;
	std::string onlineUrl{QDNSM_UPDATE_URL};
	bool hasOnlineUrl{};
	std::vector<DnsServerEntry> servers;
};

static bool ReadFileText(const char* path, std::string& out, size_t maxSize)
{
	CellFsStat stat;

	if (cellFsStat(path, &stat) != CELL_FS_SUCCEEDED)
		return false;

	if (static_cast<size_t>(stat.st_size) > maxSize)
		return false;

	int handle = -1;

	if (cellFsOpen(path, CELL_FS_O_RDONLY, &handle, nullptr, 0) != CELL_FS_SUCCEEDED)
		return false;

	if (stat.st_size == 0)
	{
		cellFsClose(handle);
		out.clear();
		return true;
	}

	out.resize(static_cast<size_t>(stat.st_size));

	uint64_t read = 0;
	const int result = cellFsRead(handle, &out[0], static_cast<uint64_t>(stat.st_size), &read);

	cellFsClose(handle);

	if (result != CELL_FS_SUCCEEDED)
		return false;

	out.resize(static_cast<size_t>(read));
	return true;
}

static bool WriteFileText(const char* path, const std::string& text)
{
	const int flags = CELL_FS_O_CREAT | CELL_FS_O_TRUNC | CELL_FS_O_WRONLY;

	cellFsChmod(path, 0777);

	int handle = -1;

	if (cellFsOpen(path, flags, &handle, nullptr, 0) != CELL_FS_SUCCEEDED)
		return false;

	uint64_t written = 0;
	const int result = cellFsWrite(handle, text.data(), text.size(), &written);

	cellFsClose(handle);

	if ((result != CELL_FS_SUCCEEDED) || (written != text.size()))
		return false;

	cellFsChmod(path, 0777);
	return true;
}

static void RemoveFile(const char* path)
{
	cellFsUnlink(path);
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

static bool RangeStartsWith(const char* text, size_t length, const char* value)
{
	size_t valueLength = 0;

	while (value[valueLength] != '\0')
		valueLength++;

	if (length < valueLength)
		return false;

	for (size_t i = 0; i < valueLength; i++)
	{
		if (text[i] != value[i])
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

static bool RangeToBool(const char* text, size_t length)
{
	if ((length == 1) && (text[0] == '1'))
		return true;

	return RangeEquals(text, length, "true") ||
		RangeEquals(text, length, "yes") ||
		RangeEquals(text, length, "on");
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
		const char character = text[i];

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

static void RangeToString(std::string& out, const char* text, size_t length)
{
	out.assign(text, length);
}

static void RangeToWString(std::wstring& out, const char* text, size_t length)
{
	out.clear();

	for (size_t i = 0; i < length; i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
}

static std::string WideToAscii(const std::wstring& text)
{
	std::string out;

	for (size_t i = 0; i < text.size(); i++)
	{
		const wchar_t character = text[i];

		if ((character >= 0x20) && (character <= 0x7E))
			out += static_cast<char>(character);
	}

	return out;
}

static bool ParseConfig(const std::string& content, ConfigData& config)
{
	enum
	{
		SectionNone,
		SectionSettings,
		SectionDnsServer,
	};

	config = ConfigData{};

	int section = SectionNone;
	bool sawSettings = false;
	DnsServerEntry pending{};
	bool hasName = false;
	bool hasPrimary = false;

	auto commit = [&]()
	{
		if (hasName && hasPrimary &&
			!pending.name.empty() && !pending.primary.empty() &&
			IsValidIpv4(pending.primary.c_str(), pending.primary.size()))
		{
			config.servers.push_back(pending);
		}

		pending = DnsServerEntry{};
		hasName = false;
		hasPrimary = false;
	};

	const char* data = content.empty() ? nullptr : content.data();
	size_t cursor = 0;

	while (cursor < content.size())
	{
		size_t lineEnd = cursor;

		while ((lineEnd < content.size()) && (data[lineEnd] != '\n'))
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

		if ((lineLength >= 2) && (line[0] == '[') && (line[lineLength - 1] == ']'))
		{
			const char* name = line + 1;
			const size_t nameLength = lineLength - 2;

			commit();

			if (RangeEquals(name, nameLength, SECTION_SETTINGS))
			{
				section = SectionSettings;
				sawSettings = true;
			}
			else if (RangeStartsWith(name, nameLength, SECTION_DNS_SERVER_PREFIX))
				section = SectionDnsServer;
			else
				section = SectionNone;

			continue;
		}

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
			continue;

		const char* key = line;
		size_t keyLength = separator;
		const char* value = line + separator + 1;
		size_t valueLength = lineLength - separator - 1;

		Trim(key, keyLength);
		Trim(value, valueLength);

		if (keyLength == 0)
			continue;

		if (section == SectionSettings)
		{
			if (RangeEquals(key, keyLength, KEY_SECONDARY_DNS))
				RangeToString(config.secondaryDns, value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_SHOW_NOTIFICATION))
				config.showNotification = RangeToBool(value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_AUTO_RECONNECT))
				config.autoReconnect = RangeToBool(value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_SHOW_SUPPORTED_GAMES))
				config.showSupportedGames = RangeToBool(value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_LANGUAGE))
				RangeToString(config.language, value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_MANAGED_FORMAT))
				config.managedFormat = RangeToBool(value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_ONLINE_VERSION))
				RangeToString(config.onlineVersion, value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_ONLINE_URL))
			{
				if (valueLength > 0)
				{
					RangeToString(config.onlineUrl, value, valueLength);
					config.hasOnlineUrl = true;
				}
			}
		}
		else if (section == SectionDnsServer)
		{
			if (RangeEquals(key, keyLength, KEY_NAME))
			{
				RangeToWString(pending.name, value, valueLength);
				hasName = true;
			}
			else if (RangeEquals(key, keyLength, KEY_PRIMARY))
			{
				RangeToString(pending.primary, value, valueLength);
				hasPrimary = true;
			}
			else if (RangeEquals(key, keyLength, KEY_MANAGED))
				pending.managed = RangeToBool(value, valueLength);
		}
	}

	commit();

	return sawSettings;
}

static void AppendUInt(std::string& out, size_t value)
{
	char digits[32];
	size_t count = 0;

	do
	{
		digits[count++] = static_cast<char>('0' + (value % 10));
		value /= 10;
	} while (value != 0);

	while (count > 0)
		out += digits[--count];
}

static std::string SerializeConfig(const ConfigData& config,
	const std::vector<DnsServerEntry>& servers, const std::string& onlineVersion,
	const std::string& onlineUrl)
{
	std::string out;

	out += "[";
	out += SECTION_SETTINGS;
	out += "]\n";
	out += KEY_SECONDARY_DNS;
	out += "=";
	out += config.secondaryDns;
	out += "\n";
	out += KEY_SHOW_NOTIFICATION;
	out += "=";
	out += config.showNotification ? "1" : "0";
	out += "\n";
	out += KEY_AUTO_RECONNECT;
	out += "=";
	out += config.autoReconnect ? "1" : "0";
	out += "\n";
	out += KEY_SHOW_SUPPORTED_GAMES;
	out += "=";
	out += config.showSupportedGames ? "1" : "0";
	out += "\n";
	out += KEY_LANGUAGE;
	out += "=";
	out += config.language;
	out += "\n";
	out += KEY_MANAGED_FORMAT;
	out += "=";
	out += config.managedFormat ? "1" : "0";
	out += "\n";
	out += KEY_ONLINE_VERSION;
	out += "=";
	out += onlineVersion;
	out += "\n";
	out += KEY_ONLINE_URL;
	out += "=";
	out += onlineUrl;
	out += "\n\n";

	for (size_t i = 0; i < servers.size(); i++)
	{
		out += "[";
		out += SECTION_DNS_SERVER_PREFIX;
		AppendUInt(out, i);
		out += "]\n";
		out += KEY_NAME;
		out += "=";
		out += WideToAscii(servers[i].name);
		out += "\n";
		out += KEY_PRIMARY;
		out += "=";
		out += servers[i].primary;
		out += "\n";
		out += KEY_MANAGED;
		out += "=";
		out += servers[i].managed ? "1" : "0";
		out += "\n\n";
	}

	return out;
}

static bool SameEntry(const DnsServerEntry& left, const DnsServerEntry& right)
{
	return (left.primary == right.primary) &&
		(left.name == right.name) &&
		(left.managed == right.managed);
}

static bool SameEntries(const std::vector<DnsServerEntry>& left,
	const std::vector<DnsServerEntry>& right)
{
	if (left.size() != right.size())
		return false;

	for (size_t i = 0; i < left.size(); i++)
	{
		if (!SameEntry(left[i], right[i]))
			return false;
	}

	return true;
}

static bool FetchSucceeded(int32_t error, int32_t httpStatus, uint32_t bytes)
{
	return (error == kNetProbeErrorNone) && (httpStatus == 200) && (bytes != 0);
}

static void ApplyDns(NetProbeRequest& request)
{
	request.dnsOk = 0;
	request.onlineVersionChanged = 0;

	if (!FetchSucceeded(request.error, request.httpStatus, request.bytes))
		return;

	std::string content;

	if (!ReadFileText(request.outputPath, content, MAX_CONFIG_FILE))
		return;

	std::vector<RemoteDnsEntry> remote;
	std::string onlineVersion;
	std::string onlineUrl;

	if (!ParseDnsList(content.empty() ? nullptr : content.data(), content.size(),
		remote, nullptr, &onlineVersion, &onlineUrl) || remote.empty())
	{
		return;
	}

	ConfigData config;
	std::string configContent;

	if (!ReadFileText(request.configPath, configContent, MAX_CONFIG_FILE) ||
		!ParseConfig(configContent, config))
	{
		return;
	}

	std::vector<DnsServerEntry> merged;
	std::string mergeError;

	if (!MergeManagedDnsList(remote, config.servers, merged, &mergeError))
		return;

	const std::string newVersion = onlineVersion.empty()
		? config.onlineVersion : onlineVersion;
	const std::string newUrl = onlineUrl.empty()
		? config.onlineUrl : onlineUrl;
	const bool versionChanged = (newVersion != config.onlineVersion);
	const bool urlChanged = (newUrl != config.onlineUrl);
	const bool dnsChanged = !SameEntries(merged, config.servers);

	if (versionChanged || urlChanged || !config.hasOnlineUrl || dnsChanged)
	{
		const std::string output = SerializeConfig(config, merged, newVersion, newUrl);

		if (output.size() > MAX_CONFIG_FILE)
			return;

		if (!WriteFileText(request.configPath, output))
			return;
	}

	request.dnsOk = 1;
	request.dnsChanged = dnsChanged ? 1 : 0;
	request.onlineVersionChanged = versionChanged ? 1 : 0;
}

static void ApplyGames(NetProbeRequest& request)
{
	request.gamesOk = 0;
	request.gamesChanged = 0;
	request.gamesInvalid = 0;

	if ((request.url2[0] == '\0') ||
		!FetchSucceeded(request.error2, request.httpStatus2, request.bytes2))
	{
		return;
	}

	std::string content;

	if (!ReadFileText(request.outputPath2, content, MAX_GAME_FILE))
		return;

	std::vector<GameListSection> sections;

	if (!ParseGameList(content.empty() ? nullptr : content.data(), content.size(),
		sections) || !ValidateGameList(sections))
	{
		request.gamesInvalid = 1;
		return;
	}

	std::string previous;
	const bool hadPrevious = ReadFileText(request.gamePath, previous, MAX_GAME_FILE);

	if (hadPrevious && (previous == content))
	{
		request.gamesOk = 1;
		return;
	}

	if (!WriteFileText(request.gamePath, content))
	{
		if (hadPrevious)
			WriteFileText(request.gamePath, previous);

		return;
	}

	request.gamesOk = 1;
	request.gamesChanged = 1;
}

void ApplyUpdateResult(NetProbeRequest& request)
{
	ApplyDns(request);
	ApplyGames(request);

	RemoveFile(request.outputPath);

	if (request.url2[0] != '\0')
		RemoveFile(request.outputPath2);
}
