#pragma once

#include <stddef.h>
#include <string>
#include <vector>

#include "Config.hpp"

struct RemoteDnsEntry
{
	std::wstring name;
	std::string primary;
};

enum DnsListLoadResult
{
	DnsList_Loaded,
	DnsList_Missing,
	DnsList_Unreadable,
	DnsList_Invalid
};

bool ParseDnsList(const char* data, size_t size, std::vector<RemoteDnsEntry>& out,
	size_t* errorLine = nullptr);

DnsListLoadResult LoadDnsList(const std::string& path, std::vector<RemoteDnsEntry>& out);

bool MergeManagedDnsList(const std::vector<RemoteDnsEntry>& remote,
	const std::vector<DnsServerEntry>& current, std::vector<DnsServerEntry>& out,
	std::string* error = nullptr);
