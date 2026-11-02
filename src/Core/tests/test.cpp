#include "../GameList.hpp"
#include "../../Net/DnsList.hpp"
#include "../Language.hpp"
#include "../LanguageCodec.hpp"
#include "../MemoryWarningPolicy.hpp"
#include "../OpenSpyPatchPlan.hpp"
#include "../OpenSpySupportedTitles.hpp"
#include "../Version.hpp"
#include "../../Memory/AobScan.hpp"

#include <cstdio>

static int g_Failures = 0;

static void Check(bool condition, const char* what)
{
	if (condition)
		return;

	std::printf("  FAIL: %s\n", what);
	g_Failures++;
}

static std::wstring W(const char* text)
{
	std::wstring out;

	while ((text != nullptr) && (*text != '\0'))
		out += static_cast<wchar_t>(static_cast<unsigned char>(*text++));

	return out;
}

static void TestGameList()
{
	std::vector<GameListSection> sections;
	const char* valid =
		"# comment\n"
		"[146.190.205.197]\n"
		"Aliases=PS3 Reborn|SaveConfrontation\n"
		"Game=Gran Turismo 5\n"
		"Game=Gran Turismo 5\n"
		"Game=SOCOM: Confrontation\n"
		"\n"
		"[67.222.156.250]\n"
		"Game=Gran Turismo 6 (EU) (BCES01893)\n";

	Check(ParseGameList(valid, std::char_traits<char>::length(valid), sections), "valid game file parses");
	Check(sections.size() == 2, "two sections");
	Check(sections[0].games.size() == 3, "duplicate games preserved");
	Check(sections[0].aliases.size() == 2, "aliases parsed");
	Check(sections[1].games.size() == 1, "second section parsed");

	const char* openSpy =
		"[OpenSpy]\n"
		"Game=50 Cent: Blood on the Sand\n";
	Check(ParseGameList(openSpy, std::char_traits<char>::length(openSpy), sections), "OpenSpy section parses");
	Check((sections.size() == 1) && (sections[0].key == kOpenSpyGameListKey), "OpenSpy section key parsed");
	Check(FindGameListSection(sections, kOpenSpyGameListKey) != nullptr, "OpenSpy section found");
	Check(ValidateGameList(sections), "OpenSpy section validates");

	const char* openSpyAliases =
		"[OpenSpy]\n"
		"Aliases=Other\n"
		"Game=50 Cent: Blood on the Sand\n";
	Check(ParseGameList(openSpyAliases, std::char_traits<char>::length(openSpyAliases), sections), "OpenSpy aliases tolerated by runtime parser");
	Check(!ValidateGameList(sections), "OpenSpy aliases rejected by validator");

	const char* bom =
		"\xEF\xBB\xBF[1.1.1.1]\r\n"
		"Game=Game A\r\n";
	Check(ParseGameList(bom, std::char_traits<char>::length(bom), sections), "BOM and CRLF accepted");

	const char* emptySection = "[1.1.1.1]\n";
	Check(ParseGameList(emptySection, std::char_traits<char>::length(emptySection), sections), "empty section tolerated by runtime parser");

	const char* duplicateSection =
		"[1.1.1.1]\nGame=A\n"
		"[1.1.1.1]\nGame=B\n";
	Check(!ParseGameList(duplicateSection, std::char_traits<char>::length(duplicateSection), sections), "duplicate section rejected");

	const char* badIp = "[PS Rewired]\nGame=A\n";
	Check(!ParseGameList(badIp, std::char_traits<char>::length(badIp), sections), "non-IPv4 section rejected");

	const char* emptyGame = "[1.1.1.1]\nGame=\n";
	Check(!ParseGameList(emptyGame, std::char_traits<char>::length(emptyGame), sections), "empty game rejected");

	const char* emptyAlias = "[1.1.1.1]\nAliases=\nGame=A\n";
	Check(!ParseGameList(emptyAlias, std::char_traits<char>::length(emptyAlias), sections), "empty alias rejected");

	const char* emptyAliasPart = "[1.1.1.1]\nAliases=A||B\nGame=A\n";
	Check(!ParseGameList(emptyAliasPart, std::char_traits<char>::length(emptyAliasPart), sections), "empty alias part rejected");

	const char* duplicateAlias = "[1.1.1.1]\nAliases=A\nAliases=B\nGame=A\n";
	Check(!ParseGameList(duplicateAlias, std::char_traits<char>::length(duplicateAlias), sections), "duplicate alias key rejected");

	const char* unknown = "[1.1.1.1]\nAll=1\nGame=A\n";
	Check(!ParseGameList(unknown, std::char_traits<char>::length(unknown), sections), "unknown key rejected");

	const char* noSection = "Game=A\n";
	Check(!ParseGameList(noSection, std::char_traits<char>::length(noSection), sections), "key before section rejected");

	const char* nonAscii = "[1.1.1.1]\nGame=Caf\xC3\xA9\n";
	Check(!ParseGameList(nonAscii, std::char_traits<char>::length(nonAscii), sections), "non-ASCII rejected");

	const char* headerOnly = "# header\n";
	Check(ParseGameList(headerOnly, std::char_traits<char>::length(headerOnly), sections), "header-only file parses");
	Check(sections.empty(), "header-only file has no sections");

	Check(!ValidateGameList(sections), "empty catalogue rejected by validator");

	const char* aliasesOnly =
		"[1.1.1.1]\n"
		"Aliases=Shared Name\n";
	Check(ParseGameList(aliasesOnly, std::char_traits<char>::length(aliasesOnly), sections), "aliases-only section parses");
	Check(!ValidateGameList(sections), "aliases-only section rejected by validator");

	const char* validForUpdate =
		"[1.1.1.1]\n"
		"Game=Game A\n"
		"[2.2.2.2]\n"
		"Aliases=Shared Name\n"
		"Game=Game B\n";
	Check(ParseGameList(validForUpdate, std::char_traits<char>::length(validForUpdate), sections), "valid update catalogue parses");
	Check(ValidateGameList(sections), "valid update catalogue accepted by validator");
}

static void TestDnsList()
{
	std::vector<RemoteDnsEntry> entries;
	size_t line = 0;

	const char* valid =
		"# list\n"
		"[DNS]\n"
		"Name=PS3 Reborn\n"
		"Primary=146.190.205.197\n"
		"Managed=1\n"
		"\n"
		"[DNS]\n"
		"Name=SaveConfrontation\n"
		"Primary=146.190.205.197\n"
		"Managed=1\n";

	Check(ParseDnsList(valid, std::char_traits<char>::length(valid), entries), "valid DNS list parses");
	Check(entries.size() == 2, "two DNS entries");

	const char* managedZero =
		"[DNS]\nName=A\nPrimary=1.1.1.1\nManaged=0\n";
	Check(!ParseDnsList(managedZero, std::char_traits<char>::length(managedZero), entries, &line), "Managed=0 rejected");
	Check(line == 4, "Managed error line");

	const char* missingManaged =
		"[DNS]\nName=A\nPrimary=1.1.1.1\n";
	Check(!ParseDnsList(missingManaged, std::char_traits<char>::length(missingManaged), entries), "missing Managed rejected");

	const char* duplicatePair =
		"[DNS]\nName=A\nPrimary=1.1.1.1\nManaged=1\n"
		"[DNS]\nName=A\nPrimary=1.1.1.1\nManaged=1\n";
	Check(!ParseDnsList(duplicatePair, std::char_traits<char>::length(duplicatePair), entries), "duplicate pair rejected");

	const char* sameIp =
		"[DNS]\nName=A\nPrimary=1.1.1.1\nManaged=1\n"
		"[DNS]\nName=B\nPrimary=1.1.1.1\nManaged=1\n";
	Check(ParseDnsList(sameIp, std::char_traits<char>::length(sameIp), entries), "same IP different names allowed");

	const char* emptyName =
		"[DNS]\nName=\nPrimary=1.1.1.1\nManaged=1\n";
	Check(!ParseDnsList(emptyName, std::char_traits<char>::length(emptyName), entries), "empty Name rejected");

	const char* unknown =
		"[DNS]\nName=A\nPrimary=1.1.1.1\nManaged=1\nExtra=1\n";
	Check(!ParseDnsList(unknown, std::char_traits<char>::length(unknown), entries), "unknown DNS key rejected");
}

static void TestVersion()
{
	Check(IsValidVersion("1"), "single version component is valid");
	Check(IsValidVersion("1.41"), "dotted version is valid");
	Check(!IsValidVersion(""), "empty version is rejected");
	Check(!IsValidVersion("1."), "trailing dot is rejected");
	Check(!IsValidVersion(".1"), "leading dot is rejected");
	Check(!IsValidVersion("1..2"), "consecutive dots are rejected");
	Check(!IsValidVersion("1.a"), "non-numeric version is rejected");
	Check(!IsValidVersion("1.1234567890"), "oversized component is rejected");

	Check(IsVersionNewer("1.41", "1.4"), "1.41 is newer than 1.4");
	Check(IsVersionNewer("1.10", "1.9"), "1.10 is newer than 1.9");
	Check(IsVersionNewer("2", "1.99"), "major version wins");
	Check(!IsVersionNewer("1.4", "1.4"), "equal version is not newer");
	Check(!IsVersionNewer("1.4", "1.41"), "older version is not newer");
	Check(!IsVersionNewer("bad", "1.4"), "invalid candidate is not newer");
}

static void TestMemoryWarningPolicy()
{
	Check(ClassifyMemoryWarning(kMemoryWarningBytes + 1) == MemoryWarningLevel::None,
		"memory above the warning threshold is normal");
	Check(ClassifyMemoryWarning(kMemoryWarningBytes) == MemoryWarningLevel::Warning,
		"memory at the warning threshold is a warning");
	Check(ClassifyMemoryWarning(kMemoryCriticalBytes) == MemoryWarningLevel::Warning,
		"memory at the critical threshold is not critical");
	Check(ClassifyMemoryWarning(kMemoryCriticalBytes - 1) == MemoryWarningLevel::Critical,
		"memory below the critical threshold is critical");
}

static void TestDnsVersionComment()
{
	std::vector<RemoteDnsEntry> entries;
	std::string version;

	const char* valid =
		"# OnlineVersion=1.41\n"
		"[DNS]\n"
		"Name=A\n"
		"Primary=1.1.1.1\n"
		"Managed=1\n";
	Check(ParseDnsList(valid, std::char_traits<char>::length(valid), entries, nullptr, &version),
		"DNS list with version comment parses");
	Check(version == "1.41", "version comment is extracted");

	const char* spaced =
		"; OnlineVersion = 2.0\r\n"
		"[DNS]\r\n"
		"Name=A\r\n"
		"Primary=1.1.1.1\r\n"
		"Managed=1\r\n";
	Check(ParseDnsList(spaced, std::char_traits<char>::length(spaced), entries, nullptr, &version),
		"spaced CRLF version comment parses");
	Check(version == "2.0", "spaced version comment is extracted");

	const char* invalid =
		"# OnlineVersion=1..2\n"
		"[DNS]\n"
		"Name=A\n"
		"Primary=1.1.1.1\n"
		"Managed=1\n";
	Check(ParseDnsList(invalid, std::char_traits<char>::length(invalid), entries, nullptr, &version),
		"invalid version does not reject DNS list");
	Check(version.empty(), "invalid version is ignored");

	const char* duplicate =
		"# OnlineVersion=1.0\n"
		"# OnlineVersion=2.0\n"
		"[DNS]\n"
		"Name=A\n"
		"Primary=1.1.1.1\n"
		"Managed=1\n";
	Check(ParseDnsList(duplicate, std::char_traits<char>::length(duplicate), entries, nullptr, &version),
		"duplicate version comments parse");
	Check(version == "2.0", "last valid version wins");

	version = "stale";
	const char* missing =
		"[DNS]\n"
		"Name=A\n"
		"Primary=1.1.1.1\n"
		"Managed=1\n";
	Check(ParseDnsList(missing, std::char_traits<char>::length(missing), entries, nullptr, &version),
		"DNS list without version parses");
	Check(version.empty(), "missing version clears output");

	std::string url;
	const char* withUrl =
		"# OnlineVersion=1.41\n"
		"# OnlineUrl=https://example.test/qDNSm\n"
		"[DNS]\n"
		"Name=A\n"
		"Primary=1.1.1.1\n"
		"Managed=1\n";
	Check(ParseDnsList(withUrl, std::char_traits<char>::length(withUrl), entries, nullptr,
		&version, &url), "DNS list with URL comment parses");
	Check(url == "https://example.test/qDNSm", "URL comment is extracted");

	const char* emptyUrl =
		"# OnlineUrl=\n"
		"[DNS]\n"
		"Name=A\n"
		"Primary=1.1.1.1\n"
		"Managed=1\n";
	url = "stale";
	Check(ParseDnsList(emptyUrl, std::char_traits<char>::length(emptyUrl), entries, nullptr,
		nullptr, &url), "empty URL comment does not reject DNS list");
	Check(url.empty(), "empty URL is ignored");
}

static DnsServerEntry Entry(const char* name, const char* primary, bool managed)
{
	DnsServerEntry entry{};
	entry.name = W(name);
	entry.primary = primary;
	entry.managed = managed;
	return entry;
}

static bool EntriesAre(const std::vector<DnsServerEntry>& entries,
	const char* name0, const char* primary0, bool managed0,
	const char* name1 = nullptr, const char* primary1 = nullptr, bool managed1 = false)
{
	if (entries.size() < 1)
		return false;

	if ((entries[0].name != W(name0)) || (entries[0].primary != primary0) || (entries[0].managed != managed0))
		return false;

	if (name1 == nullptr)
		return entries.size() == 1;

	if (entries.size() != 2)
		return false;

	return (entries[1].name == W(name1)) && (entries[1].primary == primary1) && (entries[1].managed == managed1);
}

static void TestMerge()
{
	std::vector<DnsServerEntry> current;
	current.push_back(Entry("Managed A", "1.1.1.1", true));
	current.push_back(Entry("Unmanaged X", "2.2.2.2", false));
	current.push_back(Entry("Managed B", "3.3.3.3", true));

	std::vector<RemoteDnsEntry> remote;
	RemoteDnsEntry a{};
	a.name = W("Managed A");
	a.primary = "1.1.1.1";
	remote.push_back(a);

	RemoteDnsEntry c{};
	c.name = W("Managed C");
	c.primary = "4.4.4.4";
	remote.push_back(c);

	std::vector<DnsServerEntry> merged;
	Check(MergeManagedDnsList(remote, current, merged, nullptr), "merge succeeds");
	Check(merged.size() == 3, "merged size");
	Check((merged[0].name == W("Managed A")) && merged[0].managed, "first managed entry");
	Check((merged[1].name == W("Unmanaged X")) && !merged[1].managed, "unmanaged entry preserved");
	Check((merged[2].name == W("Managed C")) && merged[2].managed, "remote C at managed B's slot");

	std::vector<DnsServerEntry> unmanagedOnly;
	unmanagedOnly.push_back(Entry("User", "5.5.5.5", false));
	Check(MergeManagedDnsList(remote, unmanagedOnly, merged, nullptr), "merge with no managed entries");
	Check(merged.size() == 3, "remote entries appended after unmanaged");

	std::vector<RemoteDnsEntry> matchingRemote;
	RemoteDnsEntry user{};
	user.name = W("User");
	user.primary = "5.5.5.5";
	matchingRemote.push_back(user);
	Check(MergeManagedDnsList(matchingRemote, unmanagedOnly, merged, nullptr), "merge matching unmanaged");
	Check(EntriesAre(merged, "User", "5.5.5.5", false), "matching unmanaged not duplicated");

	std::vector<DnsServerEntry> stale;
	stale.push_back(Entry("Uncharted Reloaded", "51.75.22.125", false));
	stale.push_back(Entry("Uncharted Reloaded", "51.255.40.74", true));

	std::vector<RemoteDnsEntry> currentUncharted;
	RemoteDnsEntry uncharted{};
	uncharted.name = W("Uncharted Reloaded");
	uncharted.primary = "51.255.40.74";
	currentUncharted.push_back(uncharted);

	Check(MergeManagedDnsList(currentUncharted, stale, merged, nullptr), "merge stale duplicate");
	Check(EntriesAre(merged, "Uncharted Reloaded", "51.255.40.74", true), "stale unmanaged duplicate removed");

	std::vector<DnsServerEntry> staleManaged;
	staleManaged.push_back(Entry("Managed A", "1.1.1.1", true));
	staleManaged.push_back(Entry("Managed B", "3.3.3.3", true));

	std::vector<RemoteDnsEntry> onlyA;
	onlyA.push_back(a);
	Check(MergeManagedDnsList(onlyA, staleManaged, merged, nullptr), "merge stale managed");
	Check(EntriesAre(merged, "Managed A", "1.1.1.1", true), "stale managed entry removed");

	std::vector<DnsServerEntry> full;
	for (int i = 0; i < 256; i++)
	{
		char ip[32];
		std::snprintf(ip, sizeof(ip), "10.0.0.%d", i);
		full.push_back(Entry("User", ip, false));
	}

	std::vector<RemoteDnsEntry> extra;
	RemoteDnsEntry one{};
	one.name = W("Extra");
	one.primary = "11.0.0.1";
	extra.push_back(one);

	std::vector<DnsServerEntry> overflow;
	std::string error;
	Check(!MergeManagedDnsList(extra, full, overflow, &error), "overflow rejected");
	Check(!error.empty(), "overflow has an error");
}

static void TestLanguage()
{
	Check(DecodeUtf8("qDNSm") == std::wstring(L"qDNSm"), "UTF-8 ASCII decodes");
	Check(DecodeUtf8("\xC3\xA9") == std::wstring(L"\u00e9"), "UTF-8 two-byte decodes");
	Check(DecodeUtf8("\xEF\xA2\x8B") == std::wstring(L"\uF88B"), "UTF-8 glyph decodes");
	Check(DecodeUtf8("\xE2\x28\xA1").find(static_cast<wchar_t>(0xFFFD)) != std::wstring::npos,
		"invalid UTF-8 becomes replacement character");

	Check(Language_IsValidCode("auto"), "auto is a valid language code");
	Check(Language_IsValidCode("pt_BR"), "pt_BR is a valid language code");
	Check(Language_IsValidCode("ru"), "ru is a valid language code");
	Check(Language_IsValidCode("pl"), "pl is a valid language code");
	Check(Language_IsValidCode("ja"), "ja is a valid language code");
	Check(Language_IsValidCode("zh_CN"), "zh_CN is a valid language code");
	Check(!Language_IsValidCode("xx"), "unknown language code is rejected");
	Check(std::string(Language_MapSystemLanguage(0)) == "de", "system language 0 maps to German");
	Check(std::string(Language_MapSystemLanguage(7)) == "ru", "system language 7 maps to Russian");
	Check(std::string(Language_MapSystemLanguage(8)) == "ja", "system language 8 maps to Japanese");
	Check(std::string(Language_MapSystemLanguage(11)) == "zh_CN", "system language 11 maps to Simplified Chinese");
	Check(std::string(Language_MapSystemLanguage(16)) == "pl", "system language 16 maps to Polish");
	Check(std::string(Language_MapSystemLanguage(17)) == "pt_BR", "system language 17 maps to Brazilian Portuguese");
	Check(std::string(Language_MapSystemLanguage(99)) == "en", "unknown system language falls back to English");
	Check(Language_IndexOfCode("fr") == 4, "French index");

	Check(Language_Init("en"), "English initializes");
	Check(std::wstring(T(TEXT_INPUT_YES)) == std::wstring(L"Yes"), "English Yes");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"No"), "English No");
	const std::wstring updateNotice = FormatText(TEXT_UPDATE_AVAILABLE, L"9.9",
		L"https://example.test/qDNSm");
	Check(updateNotice.find(L"v9.9") != std::wstring::npos, "update version is formatted");
	Check(updateNotice.find(L"https://example.test/qDNSm") != std::wstring::npos,
		"update URL is formatted");

	Check(Language_Init("de"), "German initializes");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"Nein"), "German No");
	Check(std::wstring(T(TEXT_INPUT_YES)) == std::wstring(L"Ja"), "German Yes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Auto"), "German Auto");
	Check(FormatText(TEXT_NOTIFY_DNS_SET_MANUAL, L"X").find(L"DNS auf X gesetzt.") == 0,
		"German formatted notification");

	Check(Language_Init("es"), "Spanish initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Autom\u00e1tico"), "Spanish Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"No"), "Spanish No");

	Check(Language_Init("fr"), "French initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Automatique"), "French Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"Non"), "French No");

	Check(Language_Init("it"), "Italian initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Automatico"), "Italian Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"No"), "Italian No");

	Check(Language_Init("pt_BR"), "Brazilian Portuguese initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Autom\u00e1tico"), "Brazilian Portuguese Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"N\u00e3o"), "Brazilian Portuguese No");

	Check(Language_Init("pt_PT"), "European Portuguese initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Autom\u00e1tico"), "European Portuguese Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"N\u00e3o"), "European Portuguese No");

	Check(Language_Init("ru"), "Russian initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"\u0410\u0432\u0442\u043e\u043c\u0430\u0442\u0438\u0447\u0435\u0441\u043a\u0438"), "Russian Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"\u041d\u0435\u0442"), "Russian No");

	Check(Language_Init("pl"), "Polish initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"Auto"), "Polish Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"Nie"), "Polish No");

	Check(Language_Init("ja"), "Japanese initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"\u81ea\u52d5"), "Japanese Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"\u3044\u3044\u3048"), "Japanese No");

	Check(Language_Init("zh_CN"), "Simplified Chinese initializes");
	Check(std::wstring(T(TEXT_LANGUAGE_AUTO)) == std::wstring(L"\u81ea\u52a8"), "Simplified Chinese Auto");
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"\u5426"), "Simplified Chinese No");

	Check(Language_Init("pt_PT"), "Portuguese reselected for unload test");
	Language_Unload();
	Check(std::wstring(T(TEXT_INPUT_NO)) == std::wstring(L"N\u00e3o"), "language reloads after unload");
	Language_LoadAll();
	Check(std::wstring(T(TEXT_INPUT_YES)) == std::wstring(L"Sim"), "language loads all after unload");

	for (int language = 1; language < Language_CodeCount(); language++)
	{
		Check(Language_Init(Language_CodeAtIndex(language)), "catalog initializes");
		Language_LoadAll();

		for (int text = 0; text < TEXT_COUNT; text++)
			Check(!TS(static_cast<TextId>(text)).empty(), "catalog entry is not empty");
	}
}

static void TestAobScan()
{
	const uint8_t data[] = { 'x', 'a', 'b', 'c', 'a', 'b', 'c', 'x' };
	const uint8_t bytes[] = { 'a', 'b', 'c' };
	const AobPattern pattern = { bytes, nullptr, sizeof(bytes) };
	AobMatch matches[4];

	Check(FindAllMatches(data, sizeof(data), 0x1000, pattern, matches, 4) == 2,
		"scanner finds two matches");
	Check((matches[0].address == 0x1001) && (matches[1].address == 0x1004),
		"scanner reports match addresses");

	Check(FindAllMatches(data, sizeof(data), 0x1000, pattern, matches, 4, 4) == 1,
		"scanner start limit suppresses overlap duplicate");
	Check(matches[0].address == 0x1001, "scanner start limit keeps first match");

	Check(FindAllMatches(data, sizeof(data), 0x1000, pattern, matches, 4, 5) == 2,
		"scanner start limit includes matches before the boundary");

	AobScanSegment segments[2];

	Check(BuildScanSegments(0x1000, 0x2000, 0x1800, 255, segments) == 2,
		"scanner splits range into two segments");
	Check((segments[0].start == 0x1000) &&
		(segments[0].reportEnd == 0x1800) &&
		(segments[0].readEnd == 0x18FF),
		"first scan segment keeps overlap");
	Check((segments[1].start == 0x1800) &&
		(segments[1].reportEnd == 0x2000) &&
		(segments[1].readEnd == 0x2000),
		"second scan segment owns the tail");

	Check(BuildScanSegments(0x1000, 0x2000, 0x1000, 255, segments) == 1,
		"split at range start stays one segment");
	Check(BuildScanSegments(0x1000, 0x2000, 0x2000, 255, segments) == 1,
		"split at range end stays one segment");
	Check(BuildScanSegments(0x1000, 0x2000, 0x3000, 255, segments) == 1,
		"split beyond range end stays one segment");
	Check(BuildScanSegments(0x1000, 0x2000, 0x1F00, 0x200, segments) == 2,
		"scanner clamps overlap to range end");
	Check(segments[0].readEnd == 0x2000, "scanner overlap clamps at range end");
	Check(BuildScanSegments(0x1000, 0x1000, 0x1000, 255, segments) == 0,
		"empty range produces no segments");

	uint8_t boundaryData[0x40];
	for (size_t i = 0; i < sizeof(boundaryData); i++)
		boundaryData[i] = 'x';

	const uint8_t boundaryNeedle[] = { 'a', 'b', 'c', 'd' };
	const uint32_t boundaryOffset = 0x1D;
	for (size_t i = 0; i < sizeof(boundaryNeedle); i++)
		boundaryData[boundaryOffset + i] = boundaryNeedle[i];

	const AobPattern boundaryPattern = {
		boundaryNeedle, nullptr, sizeof(boundaryNeedle)
	};
	AobMatch firstSegmentMatches[4];
	AobMatch secondSegmentMatches[4];
	AobMatch fullMatches[4];

	Check(BuildScanSegments(0x1000, 0x1040, 0x1020, 4, segments) == 2,
		"boundary range splits into two segments");

	const size_t firstCount = FindAllMatches(boundaryData,
		segments[0].readEnd - segments[0].start, segments[0].start,
		boundaryPattern, firstSegmentMatches, 4,
		segments[0].reportEnd - segments[0].start);
	const size_t secondCount = FindAllMatches(
		boundaryData + (segments[1].start - 0x1000),
		segments[1].readEnd - segments[1].start, segments[1].start,
		boundaryPattern, secondSegmentMatches, 4,
		segments[1].reportEnd - segments[1].start);

	Check((firstCount == 1) && (secondCount == 0),
		"scanner reports boundary match only in first segment");
	Check(firstSegmentMatches[0].address == 0x101D,
		"scanner keeps boundary match address");

	Check(FindAllMatches(boundaryData, sizeof(boundaryData), 0x1000,
		boundaryPattern, fullMatches, 4) == 1,
		"scanner finds boundary match in one pass");
	Check(fullMatches[0].address == 0x101D,
		"scanner boundary match matches full scan");
}

static void TestOpenSpySupportedTitles()
{
	char normalized[10];

	Check(NormalizeTitleId("bles01893", normalized), "lowercase title ID normalizes");
	Check(std::char_traits<char>::compare(normalized, "BLES01893", 9) == 0,
		"normalized title ID is uppercase");

	const char* const list[] = { "BLES01893", "BCUS98174" };

	Check(TitleIdInList("bles01893", list, 2), "title ID list match");
	Check(!TitleIdInList("BLES01894", list, 2), "title ID list miss");
	Check(!NormalizeTitleId("B1ES01893", normalized), "digit in title prefix rejected");
	Check(!NormalizeTitleId("BLES0189A", normalized), "letter in title suffix rejected");
	Check(!NormalizeTitleId("BLES0189", normalized), "short title ID rejected");
	Check(!NormalizeTitleId("BLES018930", normalized), "long title ID rejected");
	Check(IsSupportedTitleId("BLES00472"), "generated title list accepts known ID");
	Check(IsSupportedTitleId("BLES00219"), "generated title list accepts Condemned 2 EU");
	Check(IsSupportedTitleId("BLUS30115"), "generated title list accepts Condemned 2 NA");
	Check(IsSupportedTitleId("BLES00526"), "generated title list accepts Damnation EU");
	Check(IsSupportedTitleId("BLUS30304"), "generated title list accepts Damnation NA");
	Check(IsSupportedTitleId("BLES00464"), "generated title list accepts F.E.A.R. 2 EU");
	Check(IsSupportedTitleId("BLUS30251"), "generated title list accepts F.E.A.R. 2 NA");
	Check(IsSupportedTitleId("BLJM60160"), "generated title list accepts F.E.A.R. 2 JP");
	Check(IsSupportedTitleId("NPUB30252"), "generated title list accepts Gotham City Impostors NA");
	Check(IsSupportedTitleId("NPEB00239"), "generated title list accepts Gotham City Impostors EU");
	Check(IsGothamCityImpostorsTitleId("NPUB30252"), "Gotham special case accepts NA");
	Check(IsGothamCityImpostorsTitleId("NPEB00239"), "Gotham special case accepts EU");
	Check(!IsGothamCityImpostorsTitleId("BLUS30115"), "Gotham special case rejects other titles");
	Check(IsSupportedTitleId("BLUS30154"), "generated title list accepts Beijing 2008 NA");
	Check(IsSupportedTitleId("BLES00286"), "generated title list accepts Beijing 2008 EU");
	Check(IsSupportedTitleId("NPUB30161"), "generated title list accepts Section 8 NA");
	Check(IsSupportedTitleId("NPEB00386"), "generated title list accepts Section 8: Prejudice EU");
	Check(IsSupportedTitleId("NPUB30381"), "generated title list accepts Section 8: Prejudice NA");
	Check(!IsSupportedTitleId("NPUB30033"), "generated title list rejects Age of Booty");
	Check(!IsSupportedTitleId("BLUS30130"), "generated title list rejects Civilization Revolution NA");
	Check(!IsSupportedTitleId("BLES00238"), "generated title list rejects Civilization Revolution EU");
	Check(IsSupportedTitleId("BLUS30165"), "generated title list accepts Brothers in Arms NA");
	Check(IsSupportedTitleId("BLES00319"), "generated title list accepts Brothers in Arms EU");
	Check(IsSupportedTitleId("NPUB30565"), "generated title list accepts Brothers in Arms NA PSN");
	Check(IsSupportedTitleId("BLUS30406"), "generated title list accepts Hunted NA");
	Check(IsSupportedTitleId("BLES01310"), "generated title list accepts Hunted EU");
	Check(IsSupportedTitleId("BLUS30090"), "generated title list accepts TimeShift NA");
	Check(IsSupportedTitleId("BLES00159"), "generated title list accepts TimeShift EU");
	Check(!IsSupportedTitleId("BLES00182"), "generated title list rejects Conflict EU");
	Check(!IsSupportedTitleId("BLUS30107"), "generated title list rejects Conflict NA");
	Check(!IsSupportedTitleId("BLES01893"), "generated title list rejects unknown ID");
}

static void TestPatchPlan()
{
	const uint8_t oldBytes[] = { 'A', 'A', 'A', 'A' };
	const uint8_t newBytes[] = { 'B', 'B', 'B', 'B' };
	const uint8_t oldAuth[] = { 'h', 't', 't', 'p', 's', ':', '/', '/', '%', 's', '.', 'a', 'u', 't', 'h', '.', 'p', 'u', 'b', 's', 'v', 's' };
	const uint8_t newAuth[] = { 'h', 't', 't', 'p', ':', '/', '/', '%', 's', '.', 'a', 'u', 't', 'h', '.', 'p', 'u', 'b', 's', 'v', 's', '\0' };

	GameSpyReplacement replacements[2] = {};
	replacements[0].oldBytes = oldBytes;
	replacements[0].newBytes = newBytes;
	replacements[0].oldLength = sizeof(oldBytes);
	replacements[0].newLength = sizeof(newBytes);
	replacements[0].required = true;

	replacements[1].oldBytes = oldAuth;
	replacements[1].newBytes = newAuth;
	replacements[1].oldLength = sizeof(oldAuth);
	replacements[1].newLength = sizeof(newAuth);
	replacements[1].required = false;

	const uint32_t firstOld[] = { 0x1000, 0x2000 };
	const uint32_t secondOld[] = { 0x1100, 0x2100 };
	const uint32_t firstNew[] = { 0x1000, 0x2000 };
	const uint32_t secondNew[] = { 0x1100, 0x2100 };

	GameSpyReplacementMatches matches[2] = {};
	matches[0].oldAddresses = firstOld;
	matches[0].oldCount = 2;
	matches[1].oldAddresses = secondOld;
	matches[1].oldCount = 2;

	std::vector<GameSpyPatchEntry> entries;
	bool alreadyPatched = false;

	Check(BuildGameSpyPatchPlan(replacements, 2, matches, entries, alreadyPatched) == GameSpyPatchPlan_Ok,
		"patch plan accepts complete match set");
	Check(entries.size() == 4, "patch plan emits every match");
	Check(!alreadyPatched, "patch plan is not already patched");

	matches[0].oldCount = 0;
	matches[1].oldCount = 0;
	matches[0].newAddresses = firstNew;
	matches[0].newCount = 2;
	matches[1].newAddresses = secondNew;
	matches[1].newCount = 2;

	Check(BuildGameSpyPatchPlan(replacements, 2, matches, entries, alreadyPatched) == GameSpyPatchPlan_AlreadyPatched,
		"patch plan detects already patched game");
	Check(alreadyPatched && entries.empty(), "already patched plan has no writes");

	matches[0].oldAddresses = firstOld;
	matches[0].oldCount = 2;
	matches[0].newCount = 0;
	matches[1].newCount = 0;
	matches[1].newAddresses = nullptr;
	Check(BuildGameSpyPatchPlan(replacements, 2, matches, entries, alreadyPatched) == GameSpyPatchPlan_Ok,
		"optional auth absence does not block required replacement");

	matches[0].newAddresses = firstNew;
	matches[0].newCount = 1;
	Check(BuildGameSpyPatchPlan(replacements, 2, matches, entries, alreadyPatched) == GameSpyPatchPlan_Mixed,
		"mixed old and new state rejected");

	matches[0].newCount = 0;
	matches[0].newAddresses = nullptr;
	matches[1].oldAddresses = secondOld;
	matches[1].oldCount = 2;
	const uint32_t overlapFirst[] = { 0x1000 };
	const uint32_t overlapSecond[] = { 0x1002 };
	matches[0].oldAddresses = overlapFirst;
	matches[0].oldCount = 1;
	matches[1].oldAddresses = overlapSecond;
	matches[1].oldCount = 1;
	Check(BuildGameSpyPatchPlan(replacements, 2, matches, entries, alreadyPatched) == GameSpyPatchPlan_Overlap,
		"overlapping patch entries rejected");

	matches[0].oldCount = 0;
	matches[1].oldCount = 0;
	Check(BuildGameSpyPatchPlan(replacements, 2, matches, entries, alreadyPatched) == GameSpyPatchPlan_MissingRequired,
		"required pattern missing is rejected");
}

int main()
{
	TestGameList();
	TestDnsList();
	TestVersion();
	TestMemoryWarningPolicy();
	TestDnsVersionComment();
	TestMerge();
	TestLanguage();
	TestAobScan();
	TestOpenSpySupportedTitles();
	TestPatchPlan();

	if (g_Failures != 0)
	{
		std::printf("%d failure(s)\n", g_Failures);
		return 1;
	}

	std::printf("all tests passed\n");
	return 0;
}
