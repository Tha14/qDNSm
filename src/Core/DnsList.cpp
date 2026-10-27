#include "DnsList.hpp"

#ifndef QDNSM_HOST_TEST
#include "../Utils/FileSystem.hpp"
#endif

static const size_t MAX_DNS_FILE = 64 * 1024;
static const size_t MAX_DNS_ENTRIES = 256;

struct ParsedDnsEntry
{
	RemoteDnsEntry entry;
	size_t line;
	bool hasName;
	bool hasPrimary;
	bool hasManaged;
};

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

static wchar_t LowerAscii(wchar_t character)
{
	if ((character >= L'A') && (character <= L'Z'))
		return static_cast<wchar_t>(character - L'A' + L'a');

	return character;
}

static bool WideEqualsNoCase(const std::wstring& left, const std::wstring& right)
{
	if (left.size() != right.size())
		return false;

	for (size_t i = 0; i < left.size(); i++)
	{
		if (LowerAscii(left[i]) != LowerAscii(right[i]))
			return false;
	}

	return true;
}

bool ParseDnsList(const char* data, size_t size, std::vector<RemoteDnsEntry>& out,
	size_t* errorLine)
{
	out.clear();

	if (errorLine != nullptr)
		*errorLine = 0;

	auto fail = [&](size_t line) -> bool
	{
		if (errorLine != nullptr)
			*errorLine = line;

		out.clear();
		return false;
	};

	if ((data == nullptr) && (size != 0))
		return fail(0);

	if ((size >= 3) &&
		(static_cast<unsigned char>(data[0]) == 0xEF) &&
		(static_cast<unsigned char>(data[1]) == 0xBB) &&
		(static_cast<unsigned char>(data[2]) == 0xBF))
	{
		data += 3;
		size -= 3;
	}

	std::vector<ParsedDnsEntry> entries;
	ParsedDnsEntry current{};
	bool inSection = false;

	size_t cursor = 0;
	size_t lineNumber = 0;

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
		lineNumber++;

		Trim(line, lineLength);

		if (lineLength == 0)
			continue;

		if ((line[0] == '#') || (line[0] == ';'))
			continue;

		if (line[0] == '[')
		{
			if (line[lineLength - 1] != ']')
				return fail(lineNumber);

			const char* section = line + 1;
			size_t sectionLength = lineLength - 2;

			Trim(section, sectionLength);

			if (!RangeEquals(section, sectionLength, "DNS"))
				return fail(lineNumber);

			if (inSection)
			{
				if (!current.hasName || !current.hasPrimary || !current.hasManaged)
					return fail(current.line);

				entries.push_back(current);
			}

			current = ParsedDnsEntry{};
			current.line = lineNumber;
			inSection = true;
			continue;
		}

		if (!inSection)
			return fail(lineNumber);

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
			return fail(lineNumber);

		const char* key = line;
		size_t keyLength = separator;

		const char* value = line + separator + 1;
		size_t valueLength = lineLength - separator - 1;

		Trim(key, keyLength);
		Trim(value, valueLength);

		if (keyLength == 0)
			return fail(lineNumber);

		if (RangeEquals(key, keyLength, "Name"))
		{
			if (current.hasName || (valueLength == 0) || !IsPrintable(value, valueLength))
				return fail(lineNumber);

			current.entry.name = Widen(value, valueLength);
			current.hasName = true;
			continue;
		}

		if (RangeEquals(key, keyLength, "Primary"))
		{
			if (current.hasPrimary || !IsValidIpv4(value, valueLength))
				return fail(lineNumber);

			current.entry.primary = std::string(value, valueLength);
			current.hasPrimary = true;
			continue;
		}

		if (RangeEquals(key, keyLength, "Managed"))
		{
			if (current.hasManaged ||
				(valueLength != 1) || (value[0] != '1'))
			{
				return fail(lineNumber);
			}

			current.hasManaged = true;
			continue;
		}

		return fail(lineNumber);
	}

	if (!inSection)
		return fail(lineNumber);

	if (!current.hasName || !current.hasPrimary || !current.hasManaged)
		return fail(current.line);

	entries.push_back(current);

	if (entries.size() > MAX_DNS_ENTRIES)
		return fail(0);

	for (size_t i = 0; i < entries.size(); i++)
	{
		for (size_t j = 0; j < i; j++)
		{
			if ((entries[i].entry.primary == entries[j].entry.primary) &&
				WideEqualsNoCase(entries[i].entry.name, entries[j].entry.name))
			{
				return fail(entries[i].line);
			}
		}
	}

	for (size_t i = 0; i < entries.size(); i++)
		out.push_back(entries[i].entry);

	return true;
}

DnsListLoadResult LoadDnsList(const std::string& path, std::vector<RemoteDnsEntry>& out)
{
	out.clear();

#ifndef QDNSM_HOST_TEST
	if (!FileExist(path))
		return DnsList_Missing;

	std::string content;

	if (!ReadFileText(path, content, MAX_DNS_FILE))
		return DnsList_Unreadable;

	if (!ParseDnsList(content.empty() ? nullptr : content.data(), content.size(), out))
		return DnsList_Invalid;

	return DnsList_Loaded;
#else
	(void)path;
	return DnsList_Missing;
#endif
}

bool MergeManagedDnsList(const std::vector<RemoteDnsEntry>& remote,
	const std::vector<DnsServerEntry>& current, std::vector<DnsServerEntry>& out,
	std::string* error)
{
	std::vector<DnsServerEntry> merged;
	std::vector<DnsServerEntry> filtered;

	for (size_t i = 0; i < remote.size(); i++)
	{
		bool matchesUnmanaged = false;

		for (size_t j = 0; j < current.size(); j++)
		{
			if (!current[j].managed &&
				(current[j].primary == remote[i].primary) &&
				WideEqualsNoCase(current[j].name, remote[i].name))
			{
				matchesUnmanaged = true;
				break;
			}
		}

		if (!matchesUnmanaged)
		{
			DnsServerEntry entry{};
			entry.name = remote[i].name;
			entry.primary = remote[i].primary;
			entry.managed = true;
			filtered.push_back(entry);
		}
	}

	size_t nextRemote = 0;

	for (size_t i = 0; i < current.size(); i++)
	{
		if (!current[i].managed)
		{
			merged.push_back(current[i]);
			continue;
		}

		if (nextRemote < filtered.size())
		{
			merged.push_back(filtered[nextRemote]);
			nextRemote++;
		}
	}

	while (nextRemote < filtered.size())
	{
		merged.push_back(filtered[nextRemote]);
		nextRemote++;
	}

	if (merged.size() > MAX_DNS_ENTRIES)
	{
		if (error != nullptr)
			*error = "Merged DNS list would exceed 256 entries.";

		out.clear();
		return false;
	}

	out.swap(merged);
	return true;
}
