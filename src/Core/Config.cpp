#include "Config.hpp"
#include "Utils/FileSystem.hpp"
#include "Utils/Std.hpp"
#include <vsh/stdc.hpp>
#include <vsh/allocator.hpp>

Config g_Config;

static const char* CONFIG_FILE_NAME = "qDNSm.ini";

static const char* SECTION_SETTINGS = "Settings";
static const char* SECTION_DNS_SERVER_PREFIX = "DNSServer";

static const char* KEY_SECONDARY_DNS = "SecondaryDNS";
static const char* KEY_SHOW_NOTIFICATION = "ShowNotification";
static const char* KEY_AUTO_RECONNECT = "AutoReconnect";
static const char* KEY_NAME = "Name";
static const char* KEY_PRIMARY = "Primary";

static const char* DEFAULT_SECONDARY_DNS = "8.8.8.8";
static const bool DEFAULT_SHOW_NOTIFICATION = true;
static const bool DEFAULT_AUTO_RECONNECT = true;

struct DefaultDnsServer
{
	const char* name;
	const char* primary;
};

static const DefaultDnsServer DEFAULT_DNS_SERVERS[] =
{
	{ "PS Rewired", "67.222.156.250" },
	{ "PSORG", "51.79.41.185" },
	{ "PS3 Reborn", "146.190.205.197" },
	{ "BF Reborn", "151.241.100.80" },
	{ "PS Restored", "165.227.128.12" },
	{ "PSONE", "185.194.142.4" },
	{ "Warehouse Wiki", "209.38.53.69" },
	{ "Uncharted Reloaded", "51.75.22.125" },
	{ "Warhawk Revived", "198.100.158.95" },
	{ "SaveConfrontation", "146.190.205.197" },
	{ "All-Stars Awakened", "129.153.39.127"},
	{ "PSHOME Revival", "52.86.120.101" },
	{ "Home Headquarters", "64.20.35.146" },
	{ "Armored Core 4 Update Data", "160.251.171.125" },
	{ "PS Restored", "165.227.128.12" },
	{ "Classics", "136.55.43.79" },
	{ "The Archstones", "142.93.245.186" },
	{ "Antigravity Racing Foundation", "147.135.213.57" },
	{ "Project Neptune", "128.140.0.23" },
	{ "MNR Revival", "45.32.179.33" },
	{ "Thirteen", "95.214.55.111" },
	{ "PES 2018 Online", "162.35.166.153" },
	{ "TDU World", "194.164.90.135" },
	{ "TLOU Factions", "51.255.40.74" },
	{ "UT3MOD/333Networks", "15.204.233.35" },
	{ "Twisted Metal Federation", "139.144.213.59" },
	{ "All-Stars Awakened", "139.144.213.59" },
	{ "Infamous Reborn", "152.53.194.56" },
	{ "Darksiders II Mail Server", "193.232.178.202" },
	{ "Project Nexus", "192.236.168.216" },
	{ "PSRevived", "167.233.79.183" },
	{ "Cloudflare DNS", "1.1.1.1" },
	{ "Google DNS", "8.8.8.8" },
};

static const size_t DEFAULT_DNS_SERVER_COUNT = sizeof(DEFAULT_DNS_SERVERS) / sizeof(DEFAULT_DNS_SERVERS[0]);

static void TrimRange(char*& text, size_t& length)
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
	size_t valueLength = vsh::strlen(value);
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
	size_t valueLength = vsh::strlen(value);
	if (length < valueLength)
		return false;

	for (size_t i = 0; i < valueLength; i++)
	{
		if (text[i] != value[i])
			return false;
	}

	return true;
}

static void RangeToString(std::string& out, const char* text, size_t length)
{
	out.clear();
	for (size_t i = 0; i < length; i++)
		out += text[i];
}

static void RangeToWString(std::wstring& out, const char* text, size_t length)
{
	out.clear();
	for (size_t i = 0; i < length; i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
}

std::string WideToAscii(const std::wstring& text)
{
	std::string out;
	for (size_t i = 0; i < text.size(); i++)
	{
		wchar_t character = text[i];
		if ((character < 0x20) || (character > 0x7E))
			continue;

		out += static_cast<char>(character);
	}

	return out;
}

void AsciiToWide(std::wstring& out, const std::string& text)
{
	out.clear();
	for (size_t i = 0; i < text.size(); i++)
		out += static_cast<wchar_t>(static_cast<unsigned char>(text[i]));
}

bool IsValidIpv4(const std::string& ip)
{
	if (ip.empty() || (ip.size() > 15))
		return false;

	size_t octetCount = 0;
	size_t digits = 0;
	int value = 0;

	for (size_t i = 0; i < ip.size(); i++)
	{
		char character = ip[i];

		if (character == '.')
		{
			if (digits == 0)
				return false;

			octetCount++;
			if (octetCount > 3)
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

	return (octetCount == 3) && (digits > 0);
}

static bool RangeToBool(const char* text, size_t length)
{
	if (length == 0)
		return false;

	if ((length == 1) && (text[0] == '1'))
		return true;

	if (RangeEquals(text, length, "true") || RangeEquals(text, length, "yes") || RangeEquals(text, length, "on"))
		return true;

	return false;
}

static bool ParseConfig(Config& config, char* data, size_t size)
{
	enum
	{
		SectionNone,
		SectionSettings,
		SectionDnsServer,
	};

	config.m_DnsServers.clear();

	bool autoReconnectPresent = false;

	int currentSection = SectionNone;
	DnsServerEntry pendingEntry{};
	bool pendingEntryValid = false;

	char* cursor = data;
	char* limit = data + size;

	while (cursor < limit)
	{
		char* lineEnd = cursor;
		while ((lineEnd < limit) && (*lineEnd != '\n'))
			lineEnd++;

		char* line = cursor;
		size_t lineLength = static_cast<size_t>(lineEnd - cursor);

		if ((lineLength > 0) && (line[lineLength - 1] == '\r'))
			lineLength--;

		cursor = lineEnd + 1;

		TrimRange(line, lineLength);
		if (lineLength == 0)
			continue;

		if ((line[0] == '#') || (line[0] == ';'))
			continue;

		if ((lineLength >= 2) && (line[0] == '[') && (line[lineLength - 1] == ']'))
		{
			char* sectionName = line + 1;
			size_t sectionLength = lineLength - 2;

			if (pendingEntryValid)
			{
				config.m_DnsServers.push_back(pendingEntry);
				pendingEntry = DnsServerEntry{};
				pendingEntryValid = false;
			}

			if (RangeEquals(sectionName, sectionLength, SECTION_SETTINGS))
				currentSection = SectionSettings;
			else if (RangeStartsWith(sectionName, sectionLength, SECTION_DNS_SERVER_PREFIX))
			{
				currentSection = SectionDnsServer;
				pendingEntry = DnsServerEntry{};
				pendingEntryValid = false;
			}
			else
				currentSection = SectionNone;

			continue;
		}

		size_t separatorIndex = lineLength;
		for (size_t i = 0; i < lineLength; i++)
		{
			if (line[i] == '=')
			{
				separatorIndex = i;
				break;
			}
		}

		if (separatorIndex == lineLength)
			continue;

		char* key = line;
		size_t keyLength = separatorIndex;

		char* value = line + separatorIndex + 1;
		size_t valueLength = lineLength - separatorIndex - 1;

		TrimRange(key, keyLength);
		TrimRange(value, valueLength);

		if (keyLength == 0)
			continue;

		if (currentSection == SectionSettings)
		{
			if (RangeEquals(key, keyLength, KEY_SECONDARY_DNS))
				RangeToString(config.m_Settings.secondaryDns, value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_SHOW_NOTIFICATION))
				config.m_Settings.showNotification = RangeToBool(value, valueLength);
			else if (RangeEquals(key, keyLength, KEY_AUTO_RECONNECT))
			{
				config.m_Settings.autoReconnect = RangeToBool(value, valueLength);
				autoReconnectPresent = true;
			}
		}
		else if (currentSection == SectionDnsServer)
		{
			if (RangeEquals(key, keyLength, KEY_NAME))
			{
				RangeToWString(pendingEntry.name, value, valueLength);
				pendingEntryValid = true;
			}
			else if (RangeEquals(key, keyLength, KEY_PRIMARY))
			{
				RangeToString(pendingEntry.primary, value, valueLength);
				pendingEntryValid = true;
			}
		}
	}

	if (pendingEntryValid)
		config.m_DnsServers.push_back(pendingEntry);

	return autoReconnectPresent;
}

void Config::ResetToDefaults()
{
	m_DnsServers.clear();

	for (size_t i = 0; i < DEFAULT_DNS_SERVER_COUNT; i++)
	{
		DnsServerEntry entry{};
		RangeToWString(entry.name, DEFAULT_DNS_SERVERS[i].name, vsh::strlen(DEFAULT_DNS_SERVERS[i].name));
		entry.primary = DEFAULT_DNS_SERVERS[i].primary;

		m_DnsServers.push_back(entry);
	}

	m_Settings.secondaryDns = DEFAULT_SECONDARY_DNS;
	m_Settings.showNotification = DEFAULT_SHOW_NOTIFICATION;
	m_Settings.autoReconnect = DEFAULT_AUTO_RECONNECT;
}

bool Config::Load()
{
	m_FilePath = GetCurrentDir() + CONFIG_FILE_NAME;

	bool exists = FileExist(m_FilePath);

	std::string content;

	bool read = ReadFileText(m_FilePath, content);
	size_t size = content.size();

	if (!read)
	{
		ResetToDefaults();

		/*
		 * Only write the defaults back when there was genuinely no file. If the file is
		 * there but could not be read, overwriting it would destroy the user's DNS list -
		 * and a transient failure at boot is exactly when that is most likely to happen.
		 */
		return exists ? true : Save();
	}

	bool autoReconnectPresent = ParseConfig(*this, &content[0], size);

	if (!autoReconnectPresent && (size > 0))
	{
		m_Settings.autoReconnect = DEFAULT_AUTO_RECONNECT;
		Save();
	}

	return true;
}

bool Config::Save()
{
	if (m_FilePath.empty())
		m_FilePath = GetCurrentDir() + CONFIG_FILE_NAME;

	std::string content;

	content += "[";
	content += SECTION_SETTINGS;
	content += "]\n";
	content += KEY_SECONDARY_DNS;
	content += "=";
	content += m_Settings.secondaryDns;
	content += "\n";
	content += KEY_SHOW_NOTIFICATION;
	content += "=";
	content += (m_Settings.showNotification ? "1" : "0");
	content += "\n";
	content += KEY_AUTO_RECONNECT;
	content += "=";
	content += (m_Settings.autoReconnect ? "1" : "0");
	content += "\n\n";

	for (size_t i = 0; i < m_DnsServers.size(); i++)
	{
		content += "[";
		content += SECTION_DNS_SERVER_PREFIX;
		content += to_string(static_cast<uint64_t>(i));
		content += "]\n";
		content += KEY_NAME;
		content += "=";
		content += WideToAscii(m_DnsServers[i].name);
		content += "\n";
		content += KEY_PRIMARY;
		content += "=";
		content += m_DnsServers[i].primary;
		content += "\n\n";
	}

	SaveFile(m_FilePath, content.data(), content.size());

	return true;
}

size_t Config::GetServerCount()
{
	return m_DnsServers.size();
}

DnsServerEntry* Config::GetServer(size_t index)
{
	if (index >= m_DnsServers.size())
		return nullptr;

	return &m_DnsServers[index];
}

std::string Config::GetFilePath()
{
	if (m_FilePath.empty())
		m_FilePath = GetCurrentDir() + CONFIG_FILE_NAME;

	return m_FilePath;
}
