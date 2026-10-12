#include "Submenus.hpp"
#include "Base.hpp"
#include "../Config.hpp"
#include "../Network.hpp"
#include "../InputUi.hpp"
#include "../Version.hpp"   // QDNSM_VERSION_ASCII
#include <sys/ppu_thread.h>
#include <vsh/stdc.hpp>   // vsh::snprintf

extern "C" {
	#include "../../Utils/xregistry.h"
}

/*
 * All user input goes through our own InputUi, drawn with g_Renderer. The PS3 sysutil
 * dialogs do not work here: cellMsgDialogOpen2 never renders and cellOskDialogLoadAsync
 * needs a memory container this process cannot create (CELL_ENOMEM). They also block,
 * and blocking inside pafFrameworkBeginHook froze the XMB's own frame loop and crashed
 * the console. InputUi is a mode over frames, so nothing blocks and no worker thread
 * is needed.
 */

/* DeviceDnsText used to live here, rendering one device's DNS for the notification and
 * distinguishing "empty" from "read failed". It was only ever called by the diagnostic
 * notification that printed setter codes and a config dump, and SNC warning 178 correctly
 * reported it as unreferenced once that went. The distinction it drew was real and worth
 * keeping if anything ever needs to render a device's DNS again - the logic is in git history
 * and in ReadDeviceNetworkConfig's own logging.
 *
 * Kept out rather than deleted wholesale so the warning does not come back if someone re-adds a
 * diagnostic build. */

void ApplyDnsServer(size_t index)
{
	DnsServerEntry* entry = g_Config.GetServer(index);
	if (entry == nullptr)
		return;

	if (entry->primary.empty())
		return;

	const std::string& secondary = g_Config.m_Settings.secondaryDns;

	ApplyDnsServers(entry->primary, secondary);

	/* The reload attempt: write the whole device block back through the device-specific setter,
	 * changing only the two DNS fields. Same destination as the per-field setters above - the
	 * readback proved those already stored correctly - so this is not about where the value lands
	 * but about the code path, which is the only documented one that touches the entire
	 * configuration at once and so may carry validation and notification a single-field write
	 * skips. Read-modify-write, so the wireless and proxy fields survive untouched.
	 *
	 * Return value deliberately discarded: it used to be printed in the notification, and a
	 * shipping module has no business reporting setter codes to a user who picked a DNS server. */
	WriteDeviceNetworkConfig(0, entry->primary, secondary);

	/*
	 * With automatic DNS the console ignores the servers we just wrote and takes its own from DHCP,
	 * which looks exactly like qDNSm failing. Raised as a flag, done on the worker thread.
	 */
	RequestManualDnsFlag();

	/*
	 * Removed from this flow, and it should stay removed:
	 *
	 *   ApplyDnsToResolver() - sys_net_set_lib_name_server. This is the per-process library
	 *       nameserver, not the console's. The matching getter returned 2 immediately after a
	 *       successful-looking write, so it never even confirmed the value. It touched nothing the
	 *       user wanted changed and had a side effect.
	 *
	 *   RequestReconnect() - dropped the link after every DNS change via sceNetCtlDisconnectVsh. It
	 *       worked, reliably, and that was the problem: the console stopped assigning DHCP
	 *       addresses, and the XMB's own network test then refused with "An Ethernet Cable is not
	 *       connected." The link could be dropped but never brought back from here, so the user
	 *       was left worse off than before pressing anything.
	 *
	 * Neither is recoverable from inside the module. Both are in TODO.md with the reasoning.
	 */

	/* WideToAscii first because DnsServerEntry::name is a std::wstring, and the notification
	 * needs the narrow form. */
	const std::string nameAscii = WideToAscii(entry->name);

	/*
	 * The servers are stored at this point, and that is everything this module can do about it.
	 *
	 * The console reads its DNS settings when it builds a connection, so the new servers do not
	 * take effect until one is built. That is the one thing qDNSm has not managed to do for the
	 * user, and a long investigation into the network stack's internals failed to find a way - see
	 * TODO.md, and in particular the two functions documented there that can break a console's
	 * networking outright.
	 *
	 * So the message gives the exact gesture. Both halves matter: the console has to take the link
	 * down and then rebuild it, and either one alone does not do it.
	 */
	std::string message = "DNS set to ";
	message += nameAscii;
	message += ".\nDisable, then Enable Internet Connection.";

	std::wstring wideMessage;
	AsciiToWide(wideMessage, message);

	vsh::ShowNotificationWithIcon(wideMessage, vsh::NotifyIcon::BlueVerifiedCheckmark,
		vsh::NotifySound::OK);
}

static void Notify(const std::wstring& message, vsh::NotifySound sound)
{
	vsh::ShowNotificationWithIcon(message, vsh::NotifyIcon::BlueVerifiedCheckmark, sound);
}

/* Which flow the UI is currently driving. */
enum
{
	Flow_None,
	Flow_Add,
	Flow_Edit,
	Flow_Delete,
	Flow_SecondaryDns,
	Flow_RestoreDefaults
};

static int g_Flow = Flow_None;

/* Add and Edit each need two prompts (name, then IP). g_Flow stays set across both and
 * this counter says which one just finished - clearing g_Flow between prompts is what
 * made Edit silently fall through to the append path. */
static int g_FlowStep = 0;
static unsigned int g_FlowIndex = 0;
static std::string g_FlowName;
static std::string g_FlowPrimary;

static void BeginAdd()
{
	g_Flow = Flow_Add;
	g_FlowStep = 0;
	g_InputUi.BeginText(L"DNS server name", std::string(), false);
}

static void BeginEdit(unsigned int index)
{
	DnsServerEntry* entry = g_Config.GetServer(index);
	if (entry == nullptr)
		return;

	g_Flow = Flow_Edit;
	g_FlowStep = 0;
	g_FlowIndex = index;
	g_FlowName = WideToAscii(entry->name);
	g_FlowPrimary = entry->primary;

	g_InputUi.BeginText(L"DNS server name", g_FlowName, false);
}

static void BeginRestoreDefaults()
{
	g_Flow = Flow_RestoreDefaults;
	g_FlowStep = 0;
	g_FlowIndex = 0;

	g_InputUi.BeginConfirm(L"Restore defaults?",
		"Replace every DNS server and setting with the defaults.");
}

static void BeginDelete(unsigned int index)
{
	DnsServerEntry* entry = g_Config.GetServer(index);
	if (entry == nullptr)
		return;

	g_Flow = Flow_Delete;
	g_FlowStep = 0;
	g_FlowIndex = index;

	g_InputUi.BeginConfirm(L"Delete DNS server?", WideToAscii(entry->name));
}

static void BeginEditSecondaryDns()
{
	g_Flow = Flow_SecondaryDns;
	g_FlowStep = 0;
	g_InputUi.BeginText(L"Secondary DNS IP address", g_Config.m_Settings.secondaryDns, true);
}

static void SaveSettings()
{
	g_Config.Save();
}

static void PromptForPrimary()
{
	g_InputUi.BeginText(L"Primary DNS IP address", g_FlowPrimary, true);
}

/* Called every frame on the XMB thread; turns a finished UI into a config change. */
void UpdateDnsFlow()
{
	if (!g_InputUi.HasResult())
		return;

	InputUi::Result result = g_InputUi.GetResult();
	std::string text = g_InputUi.GetText();

	g_InputUi.ConsumeResult();

	int flow = g_Flow;

	/*
	 * A read-only screen was dismissed. It is not a step in any flow and its result is not an answer
	 * to anything, so consume it and touch nothing - not even the flow state.
	 *
	 * This must come before the cancel check below. A dismissed info screen used to report
	 * Result_Confirmed, which read as "a flow step finished", and since no flow matched it fell
	 * through to the Add/Edit branch and opened the Primary DNS prompt.
	 */
	if (result == InputUi::Result_Info)
		return;

	// Cancel ends whichever step we were on.
	if (result != InputUi::Result_Confirmed)
	{
		g_Flow = Flow_None;
		g_FlowStep = 0;
		return;
	}

	if (flow == Flow_Delete)
	{
		g_Flow = Flow_None;
		g_FlowStep = 0;

		if (g_FlowIndex < g_Config.m_DnsServers.size())
		{
			g_Config.m_DnsServers.erase(g_Config.m_DnsServers.begin() + g_FlowIndex);
			g_Config.Save();
			Notify(L"DNS server deleted.", vsh::NotifySound::OK);
		}

		return;
	}

	if (flow == Flow_RestoreDefaults)
	{
		g_Flow = Flow_None;
		g_FlowStep = 0;

		g_Config.ResetToDefaults();
		g_Config.Save();

		Notify(L"Defaults restored.", vsh::NotifySound::OK);

		return;
	}

	if (flow == Flow_SecondaryDns)
	{
		g_Flow = Flow_None;
		g_FlowStep = 0;

		if (!IsValidIpv4(text))
		{
			Notify(L"Invalid IP address.", vsh::NotifySound::Error);
			return;
		}

		g_Config.m_Settings.secondaryDns = text;
		g_Config.Save();
		Notify(L"Secondary DNS updated.", vsh::NotifySound::OK);
		return;
	}

	/*
	 * A confirmed result with no flow is not a step, and must do nothing.
	 *
	 * This branch used to be unguarded - it was reached by any Result_Confirmed that did not match
	 * Delete, RestoreDefaults or SecondaryDns, including Flow_None. That is how closing a read-only
	 * screen opened the Primary DNS prompt. Caught by Result_Info now, but the guard stays: a
	 * fallthrough that depends on nothing else having gone wrong is not a branch anyone should have
	 * to reason about to trust.
	 */
	if (flow != Flow_Add && flow != Flow_Edit)
		return;

	/* Add and Edit share this branch: step 0 is the name, step 1 is the IP. g_Flow stays
	 * set across both prompts, which is what keeps Edit on the update path instead of
	 * falling through to the append path. */
	if (g_FlowStep == 0)
	{
		g_FlowName = text;
		g_FlowStep = 1;
		PromptForPrimary();
		return;
	}

	/* the primary IP prompt finished, so the whole flow is done */
	g_Flow = Flow_None;
	g_FlowStep = 0;

	if (!IsValidIpv4(text))
	{
		Notify(L"Invalid IP address.", vsh::NotifySound::Error);
		return;
	}

	DnsServerEntry entry{};
	AsciiToWide(entry.name, g_FlowName);
	entry.primary = text;

	if (flow == Flow_Edit && g_FlowIndex < g_Config.m_DnsServers.size())
	{
		g_Config.m_DnsServers[g_FlowIndex] = entry;
		g_Config.Save();
		Notify(L"DNS server updated.", vsh::NotifySound::OK);
	}
	else
	{
		g_Config.m_DnsServers.push_back(entry);
		g_Config.Save();
		Notify(L"DNS server added.", vsh::NotifySound::OK);
	}
}

/*
 * Prefix glyph for the utility entries. Kept as a named constant so it is a one-line
 * change if the codepoint is not what was intended.
 */
#define ENTRY_PREFIX_ADD L"\uF8A6 "
#define ENTRY_PREFIX_OPTIONS L"\uF5B6 "

/*
 * On styling the credit.
 *
 * Wanted: the PS3 "Pop" system face, so the name reads in the same face the XMB uses for its own
 * display text. paf selects faces through PhFont::GlyphType, passed to the text widget by
 * Renderer::Text - and the vendored paf.hpp declares PhFont nowhere. It appears only in demangled
 * comments, so the enum's values cannot be read from this SDK, and a guessed one renders the wrong
 * face or nothing at all. Renderer::Text carries a GlyphSet parameter for the day that is
 * recoverable; nothing sets it to anything but the default.
 *
 * What works today, and what is actually shipped: the name is drawn about twice the size of the
 * body text, in the accent colour the rest of the UI already uses for values, centred on its own
 * line at the foot of the card. One word set that way reads as a credit without needing a face
 * nobody here can name.
 */

static void BeginInstructions()
{
	/* An info screen is not a flow and never was one. Clearing the flow state here means it cannot
	 * be waiting on a step that this screen will never complete, whatever happened before it. */
	g_Flow = Flow_None;
	g_FlowStep = 0;

	g_InputUi.BeginInfo(L"How to use qDNSm",
		"1. Pick a DNS server from the list.\n"
		"\n"
		"2. Go to Settings > Network > Internet\n"
		"   Connection Settings.\n"
		"\n"
		"3. Select Disable Internet Connection.\n"
		"\n"
		"4. Select Enable Internet Connection.\n"
		"\n"
		"The console only reads its DNS settings\n"
		"while it is building a connection, so\n"
		"both steps are needed. A change that looks\n"
		"like it did nothing has almost always been\n"
		"skipped step 3 or step 4.\n"
		"\n"
		"Also make sure Set Manually is selected.\n"
		"With automatic DNS the console takes its\n"
		"servers from DHCP and ignores the ones\n"
		"qDNSm writes. qDNSm turns this on for you,\n"
		"but the XMB has to be allowed to save it.");
}

static void BeginAbout()
{
	/* Same as BeginInstructions: not a flow. */
	g_Flow = Flow_None;
	g_FlowStep = 0;

	/* Lines kept short deliberately: DrawInfo clips rather than wraps, because wrapping needs font
	 * metrics this module has no way to query. */
	g_InputUi.BeginInfo(L"About",
		"qDNSm\n"
		"\n"
		"A DNS switcher for the PlayStation 3.\n"
		"\n"
		"Version " QDNSM_VERSION_ASCII "\n"
		"\n"
		"Free to use.\n"
		"https://github.com/Tha14/qDNSm"
		"\n",
		L"Developed by",
		L"Tha_14");
}

void OptionsSubmenu()
{
	std::wstring secondary;
	AsciiToWide(secondary, g_Config.m_Settings.secondaryDns);

	if (secondary.empty())
		secondary = L"Not set";

	g_Menu.Option(L"Secondary DNS").RightText(secondary).Action([]
	{
		BeginEditSecondaryDns();
	});

	g_Menu.Option(L"Startup notification")
		.Toggle(g_Config.m_Settings.showNotification, SaveSettings, SaveSettings);

	g_Menu.Option(L"How to use").Action([]
	{
		BeginInstructions();
	});

	g_Menu.Option(L"About").Action([]
	{
		BeginAbout();
	});

	g_Menu.Option(L"Restore defaults").Action([]
	{
		BeginRestoreDefaults();
	});

#ifdef ENABLE_GLYPH_BROWSER
	/* Temporary tool for finding icon codepoints. Off by default. */
	g_Menu.Option(L"Glyph browser").Action([]
	{
		g_InputUi.BeginGlyphs(0xF000);
	});
#endif
}

void MainSubmenu()
{
	if (g_InputUi.IsActive())
		return;

	for (size_t i = 0; i < g_Config.GetServerCount(); i++)
	{
		DnsServerEntry* entry = g_Config.GetServer(i);
		if (entry == nullptr)
			continue;

		unsigned int index = static_cast<unsigned int>(i);

		g_Menu.Option(entry->name).Action([index]
		{
			ApplyDnsServer(index);
		}).EditAction([index]
		{
			BeginEdit(index);
		}).DeleteAction([index]
		{
			BeginDelete(index);
		});
	}

	g_Menu.Option(L"" ENTRY_PREFIX_ADD L"Add new DNS server").Action([]
	{
		BeginAdd();
	});

	g_Menu.Option(L"" ENTRY_PREFIX_OPTIONS L"qDNSm options").Submenu(OptionsSubmenu);
}