#include "../GameList.hpp"
#include "../DnsList.hpp"
#include "../Language.hpp"
#include "../LanguageCodec.hpp"

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

int main()
{
	TestGameList();
	TestDnsList();
	TestMerge();
	TestLanguage();

	if (g_Failures != 0)
	{
		std::printf("%d failure(s)\n", g_Failures);
		return 1;
	}

	std::printf("all tests passed\n");
	return 0;
}
