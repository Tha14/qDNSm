#pragma once

#include <stddef.h>
#include <string>
#include <vector>

#include "../Core/Config.hpp"

struct RemoteDnsEntry
{
	std::wstring name;
	std::string primary;
};

bool ParseDnsList(const char* data, size_t size, std::vector<RemoteDnsEntry>& out,
	size_t* errorLine = nullptr, std::string* onlineVersion = nullptr,
	std::string* onlineUrl = nullptr);

bool MergeManagedDnsList(const std::vector<RemoteDnsEntry>& remote,
	const std::vector<DnsServerEntry>& current, std::vector<DnsServerEntry>& out,
	std::string* error = nullptr);
