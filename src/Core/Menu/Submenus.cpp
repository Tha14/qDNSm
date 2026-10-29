#include "Submenus.hpp"
#include "Base.hpp"
#include "../Config.hpp"
#include "../Network.hpp"
#include "../InputUi.hpp"
#include "../GameListPanel.hpp"
#include "../NetUpdate.hpp"
#include "../Language.hpp"
#include "../LanguageCodec.hpp"
#include "../LanguageSystem.hpp"
#include "../Version.hpp"   // QDNSM_VERSION
#include <vsh/stdc.hpp>   // vsh::snprintf
#include <vector>

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

static void NotifyError(const std::wstring& message)
{
	vsh::ShowNotificationWithIcon(message, vsh::NotifyIcon::Caution, vsh::NotifySound::Error);
}

void ApplyDnsServer(size_t index)
{
	DnsServerEntry* entry = g_Config.GetServer(index);
	if (entry == nullptr)
		return;

	if (entry->primary.empty())
		return;

	if (!BeginDnsApply())
		return;

	const std::string& secondary = g_Config.m_Settings.secondaryDns;
	const std::string nameAscii = WideToAscii(entry->name);
	const std::string displayName = nameAscii.empty() ? entry->primary : nameAscii;
	const bool reconnect = g_Config.m_Settings.autoReconnect;

	if (reconnect)
		g_Menu.SetBusyState(true, false);

	ApplyDnsServers(entry->primary, secondary);
	int write = WriteDeviceNetworkConfig(0, entry->primary, secondary);

	if (write != 0)
	{
		EndDnsApply();
		g_Menu.SetBusyState(false, false);
		NotifyError(T(TEXT_NOTIFY_DNS_WRITE_FAILED));
		return;
	}

	if (!reconnect)
	{
		EndDnsApply();

		std::wstring displayWide = entry->name;

		if (displayWide.empty())
			AsciiToWide(displayWide, entry->primary);

		std::wstring message = FormatText(TEXT_NOTIFY_DNS_SET_MANUAL, displayWide);

		vsh::ShowNotificationWithIcon(message.c_str(), vsh::NotifyIcon::BlueVerifiedCheckmark,
			vsh::NotifySound::OK);
		return;
	}

	if (!RequestReconnect(displayName))
	{
		EndDnsApply();
		g_Menu.SetBusyState(false, false);
		NotifyError(T(TEXT_NOTIFY_RECONNECT_START_FAILED));
		return;
	}

	g_Menu.SetBusyState(true, true);
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
	g_InputUi.BeginText(T(TEXT_DIALOG_DNS_NAME_TITLE), std::string(), false);
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

	g_InputUi.BeginText(T(TEXT_DIALOG_DNS_NAME_TITLE), g_FlowName, false);
}

static void BeginRestoreDefaults()
{
	g_Flow = Flow_RestoreDefaults;
	g_FlowStep = 0;
	g_FlowIndex = 0;

	g_InputUi.BeginConfirm(T(TEXT_DIALOG_RESTORE_TITLE),
		T(TEXT_DIALOG_RESTORE_MESSAGE));
}

static void BeginDelete(unsigned int index)
{
	DnsServerEntry* entry = g_Config.GetServer(index);
	if (entry == nullptr)
		return;

	g_Flow = Flow_Delete;
	g_FlowStep = 0;
	g_FlowIndex = index;

	g_InputUi.BeginConfirm(T(TEXT_DIALOG_DELETE_TITLE),
		FormatText(TEXT_DIALOG_DELETE_MESSAGE, entry->name));
}

static void BeginEditSecondaryDns()
{
	g_Flow = Flow_SecondaryDns;
	g_FlowStep = 0;
	g_InputUi.BeginText(T(TEXT_DIALOG_SECONDARY_IP_TITLE), g_Config.m_Settings.secondaryDns, true);
}

static void SaveSettings()
{
	g_Config.Save();
}

static void PromptForPrimary()
{
	g_InputUi.BeginText(T(TEXT_DIALOG_PRIMARY_IP_TITLE), g_FlowPrimary, true);
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
			Notify(T(TEXT_NOTIFY_DNS_DELETED), vsh::NotifySound::OK);
		}

		return;
	}

	if (flow == Flow_RestoreDefaults)
	{
		g_Flow = Flow_None;
		g_FlowStep = 0;

		g_Config.ResetToDefaults();
		g_Config.Save();

		Notify(T(TEXT_NOTIFY_DEFAULTS_RESTORED), vsh::NotifySound::OK);

		return;
	}

	if (flow == Flow_SecondaryDns)
	{
		g_Flow = Flow_None;
		g_FlowStep = 0;

		if (!IsValidIpv4(text))
		{
			Notify(T(TEXT_NOTIFY_INVALID_IP), vsh::NotifySound::Error);
			return;
		}

		g_Config.m_Settings.secondaryDns = text;
		g_Config.Save();
		Notify(T(TEXT_NOTIFY_SECONDARY_UPDATED), vsh::NotifySound::OK);
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
		Notify(T(TEXT_NOTIFY_INVALID_IP), vsh::NotifySound::Error);
		return;
	}

	DnsServerEntry entry{};
	AsciiToWide(entry.name, g_FlowName);
	entry.primary = text;
	entry.managed = false;

	if (flow == Flow_Edit && g_FlowIndex < g_Config.m_DnsServers.size())
	{
		g_Config.m_DnsServers[g_FlowIndex] = entry;
		g_Config.Save();
		Notify(T(TEXT_NOTIFY_DNS_UPDATED), vsh::NotifySound::OK);
	}
	else
	{
		g_Config.m_DnsServers.push_back(entry);
		g_Config.Save();
		Notify(T(TEXT_NOTIFY_DNS_ADDED), vsh::NotifySound::OK);
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

	g_InputUi.BeginInfo(T(TEXT_HELP_TITLE), T(TEXT_HELP_BODY));
}

static void BeginAbout()
{
	/* Same as BeginInstructions: not a flow. */
	g_Flow = Flow_None;
	g_FlowStep = 0;

	/* Lines kept short deliberately: DrawInfo clips rather than wraps, because wrapping needs font
	 * metrics this module has no way to query. */
	g_InputUi.BeginInfo(T(TEXT_ABOUT_TITLE),
		FormatText(TEXT_ABOUT_BODY, QDNSM_VERSION),
		T(TEXT_ABOUT_CREDIT_LABEL),
		L"Tha_14",
		L"qDNSm",
		true);
}

static const wchar_t* const kLanguageNames[] =
{
	L"English",
	L"Deutsch",
	L"Espa\u00f1ol",
	L"Fran\u00e7ais",
	L"Italiano",
	L"Portugu\u00eas (Brasil)",
	L"Portugu\u00eas (Portugal)",
	L"\u0420\u0443\u0441\u0441\u043a\u0438\u0439",
	L"Polski",
	L"\u65e5\u672c\u8a9e",
	L"\u7b80\u4f53\u4e2d\u6587",
};

static std::wstring LanguageName(int index)
{
	if (index == 0)
		return T(TEXT_LANGUAGE_AUTO);

	const int nameCount = static_cast<int>(sizeof(kLanguageNames) / sizeof(kLanguageNames[0]));

	if ((index > 0) && (index <= nameCount))
		return kLanguageNames[index - 1];

	return T(TEXT_LANGUAGE_AUTO);
}

static void ApplyLanguage(int index)
{
	const char* code = Language_CodeAtIndex(index);
	std::string previous = g_Config.m_Settings.language;

	g_Config.m_Settings.language = code;

	if (!g_Config.Save())
	{
		g_Config.m_Settings.language = previous;
		NotifyError(T(TEXT_ERROR_SAVE_LANGUAGE));
		return;
	}

	if (!Language_Init(Language_ResolveConfiguredCode(code)))
	{
		g_Config.m_Settings.language = previous;
		g_Config.Save();
		Language_Init(Language_ResolveConfiguredCode(previous));
		Language_LoadAll();
		NotifyError(T(TEXT_ERROR_SAVE_LANGUAGE));
		return;
	}

	Language_LoadAll();
}

void OptionsSubmenu()
{
	std::wstring secondary;
	AsciiToWide(secondary, g_Config.m_Settings.secondaryDns);

	if (secondary.empty())
		secondary = T(TEXT_OPTIONS_NOT_SET);

	g_Menu.Option(T(TEXT_OPTIONS_SECONDARY_DNS)).RightText(secondary).Action([]
	{
		BeginEditSecondaryDns();
	});

	g_Menu.Option(T(TEXT_OPTIONS_STARTUP_NOTIFICATION))
		.Toggle(g_Config.m_Settings.showNotification, SaveSettings, SaveSettings);

	g_Menu.Option(T(TEXT_OPTIONS_AUTO_RECONNECT))
		.Toggle(g_Config.m_Settings.autoReconnect, SaveSettings, SaveSettings);

	g_Menu.Option(T(TEXT_OPTIONS_SHOW_GAMES))
		.Toggle(g_Config.m_Settings.showSupportedGames, SaveSettings, SaveSettings);

	int languageIndex = Language_IndexOfCode(g_Config.m_Settings.language);
	int previousLanguage = languageIndex;
	std::vector<std::wstring> languageNames;

	for (int i = 0; i < Language_CodeCount(); i++)
		languageNames.push_back(LanguageName(i));

	g_Menu.Option(T(TEXT_OPTIONS_LANGUAGE)).Strings(languageNames, languageIndex);

	if (languageIndex != previousLanguage)
		ApplyLanguage(languageIndex);

	g_Menu.Option(T(TEXT_OPTIONS_UPDATE_DNS))
		.RightText(IsNetUpdateBusy() ? T(TEXT_OPTIONS_UPDATING) : L"")
		.Action([]
	{
		if (IsNetUpdateBusy())
			return;

		if (!StartNetUpdate())
			NotifyError(T(TEXT_NOTIFY_UPDATE_START_FAILED));
	});

	g_Menu.Option(T(TEXT_OPTIONS_HOW_TO)).Action([]
	{
		BeginInstructions();
	});

	g_Menu.Option(T(TEXT_OPTIONS_ABOUT)).Action([]
	{
		BeginAbout();
	});

	g_Menu.Option(T(TEXT_OPTIONS_RESTORE)).Action([]
	{
		BeginRestoreDefaults();
	});
}

void MainSubmenu()
{
	if (g_InputUi.IsActive())
		return;

	int hoveredIndex = -1;

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
		}).WhileHovered([&hoveredIndex, i]
		{
			hoveredIndex = static_cast<int>(i);
		});
	}

	g_Menu.Option(std::wstring(ENTRY_PREFIX_ADD) + T(TEXT_MENU_ADD_SERVER)).Action([]
	{
		BeginAdd();
	});

	g_Menu.Option(std::wstring(ENTRY_PREFIX_OPTIONS) + T(TEXT_MENU_OPTIONS)).Submenu(OptionsSubmenu);

	g_GameListPanel.SetHoveredIndex(hoveredIndex);
	g_Menu.SetOverlay([]
	{
		g_GameListPanel.Draw();
	});
}