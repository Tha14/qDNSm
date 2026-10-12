#pragma once
#include <string>
#include <vector>

struct DnsServerEntry
{
	std::wstring name;
	std::string primary;
};

struct ConfigSettings
{
	std::string secondaryDns;
	bool showNotification{};
};

class Config
{
public:
	Config() = default;

	bool Load();
	bool Save();
	void ResetToDefaults();

	size_t GetServerCount();
	DnsServerEntry* GetServer(size_t index);

	std::string GetFilePath();

public:
	std::vector<DnsServerEntry> m_DnsServers{};
	ConfigSettings m_Settings{};

private:
	std::string m_FilePath{};
};

extern Config g_Config;

std::string WideToAscii(const std::wstring& text);
void AsciiToWide(std::wstring& out, const std::string& text);
bool IsValidIpv4(const std::string& ip);