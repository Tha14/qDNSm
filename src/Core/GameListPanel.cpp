#include "GameListPanel.hpp"

#include "Config.hpp"
#include "GameList.hpp"
#include "Input.hpp"
#include "InputUi.hpp"
#include "Language.hpp"
#include "Renderer.hpp"
#include "Menu/Base.hpp"
#include "Utils/Std.hpp"
#include "Utils/Timers.hpp"
#include "../Utils/FileSystem.hpp"

#include <vsh/vshcommon.hpp>

GameListPanel g_GameListPanel;

static const float PANEL_WIDTH = 380.0f;
static const float PANEL_HEIGHT = 340.0f;
static const float PANEL_GAP = 12.0f;
static const float PANEL_MARGIN = 14.0f;
static const int VISIBLE_ROWS = 12;
static const uint64_t SCROLL_STEP_MS = 2000;
static const uint64_t SCROLL_PAUSE_MS = 5000;

static wchar_t LowerAscii(wchar_t character)
{
	if ((character >= L'A') && (character <= L'Z'))
		return static_cast<wchar_t>(character - L'A' + L'a');

	return character;
}

static bool EqualsNoCase(const std::wstring& left, const std::wstring& right)
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

static void Trim(std::wstring& text)
{
	size_t start = 0;
	size_t end = text.size();

	while ((start < end) && ((text[start] == L' ') || (text[start] == L'\t')))
		start++;

	while ((end > start) && ((text[end - 1] == L' ') || (text[end - 1] == L'\t')))
		end--;

	text = text.substr(start, end - start);
}

static std::wstring Truncate(const std::wstring& text, size_t maximum)
{
	if (text.size() <= maximum)
		return text;

	if (maximum <= 3)
		return text.substr(0, maximum);

	return text.substr(0, maximum - 3) + L"...";
}

static void NotifyError(const wchar_t* message)
{
	vsh::ShowNotificationWithIcon(message, vsh::NotifyIcon::Caution, vsh::NotifySound::Error);
}

void GameListPanel::ResetData()
{
	m_Index = -1;
	m_State = Panel_NoServer;
	m_Ip.clear();
	m_Name.clear();
	m_Games.clear();
	m_SectionAliases.clear();
	m_Aliases.clear();
	m_ScrollIndex = 0;
	m_ScrollDirection = 1;
	m_LastTick = 0;
	m_PauseUntil = 0;
}

void GameListPanel::Reset()
{
	ResetData();
	m_State = Panel_Disabled;
}

void GameListPanel::Invalidate()
{
	m_Index = -2;
	m_Ip.clear();
	m_LastTick = 0;
}

void GameListPanel::LoadForIndex(int index)
{
	(void)index;

	m_Games.clear();
	m_SectionAliases.clear();
	m_ScrollIndex = 0;
	m_ScrollDirection = 1;
	m_LastTick = 0;
	m_PauseUntil = 0;

	std::vector<GameListSection> sections;
	GameListLoadResult result = LoadGameList(GetCurrentDir() + "qDNSm-server-games.ini", sections);

	if (result == GameList_Missing)
	{
		m_State = Panel_MissingFile;
		RebuildAliases();
		return;
	}

	if (result == GameList_Unreadable)
	{
		m_State = Panel_Unreadable;
		RebuildAliases();
		return;
	}

	if (result == GameList_Invalid)
	{
		m_State = Panel_Invalid;
		RebuildAliases();
		return;
	}

	const GameListSection* section = FindGameListSection(sections, m_Ip);

	if (section == nullptr)
	{
		m_State = Panel_NoSection;
		RebuildAliases();
		return;
	}

	m_Games = section->games;
	m_SectionAliases = section->aliases;

	if (m_Games.empty())
		m_State = Panel_EmptySection;
	else
	{
		m_State = Panel_Ready;
		m_LastTick = GetTimeNow();
	}

	RebuildAliases();
}

void GameListPanel::SetHoveredIndex(int index)
{
	if (!g_Config.m_Settings.showSupportedGames)
	{
		Reset();
		return;
	}

	if ((index < 0) || (static_cast<size_t>(index) >= g_Config.GetServerCount()))
	{
		if ((m_State != Panel_NoServer) || (m_Index != -1))
			ResetData();

		return;
	}

	DnsServerEntry* entry = g_Config.GetServer(static_cast<size_t>(index));

	if (entry == nullptr)
	{
		ResetData();
		return;
	}

	const bool sameIp = (m_Ip == entry->primary) && !m_Ip.empty();

	if ((index == m_Index) && sameIp && (m_Name == entry->name))
		return;

	m_Index = index;
	m_Name = entry->name;

	if (sameIp)
	{
		m_ScrollIndex = 0;
		m_ScrollDirection = 1;
		m_PauseUntil = 0;
		m_LastTick = GetTimeNow();
		RebuildAliases();
		return;
	}

	m_Ip = entry->primary;
	LoadForIndex(index);
}

void GameListPanel::RebuildAliases()
{
	m_Aliases.clear();

	for (size_t i = 0; i < m_SectionAliases.size(); i++)
	{
		std::wstring alias = m_SectionAliases[i];
		Trim(alias);

		if (alias.empty() || EqualsNoCase(alias, m_Name))
			continue;

		bool duplicate = false;

		for (size_t j = 0; j < m_Aliases.size(); j++)
		{
			if (EqualsNoCase(m_Aliases[j], alias))
			{
				duplicate = true;
				break;
			}
		}

		if (!duplicate)
			m_Aliases.push_back(alias);
	}

	for (size_t i = 0; i < g_Config.m_DnsServers.size(); i++)
	{
		const DnsServerEntry& entry = g_Config.m_DnsServers[i];

		if ((entry.primary != m_Ip) || EqualsNoCase(entry.name, m_Name))
			continue;

		std::wstring alias = entry.name;
		Trim(alias);

		if (alias.empty())
			continue;

		bool duplicate = false;

		for (size_t j = 0; j < m_Aliases.size(); j++)
		{
			if (EqualsNoCase(m_Aliases[j], alias))
			{
				duplicate = true;
				break;
			}
		}

		if (!duplicate)
			m_Aliases.push_back(alias);
	}
}

std::wstring GameListPanel::AliasLine() const
{
	if (m_Aliases.empty())
		return std::wstring();

	std::wstring body = T(TEXT_GAMELIST_ALSO_USED_BY);
	body += L" ";
	std::wstring marker;

	const size_t shown = (m_Aliases.size() > 3) ? 3 : m_Aliases.size();

	for (size_t i = 0; i < shown; i++)
	{
		if (i != 0)
			body += L", ";

		body += m_Aliases[i];
	}

	if (m_Aliases.size() > shown)
	{
		marker = L" +";
		marker += to_wstring(static_cast<uint64_t>(m_Aliases.size() - shown));
	}

	const size_t maximum = 54;

	if ((body.size() + marker.size()) <= maximum)
		return body + marker;

	const size_t room = (maximum > marker.size() + 3) ? (maximum - marker.size() - 3) : 0;

	return body.substr(0, room) + L"..." + marker;
}

void GameListPanel::HandleSelect()
{
	const bool previous = g_Config.m_Settings.showSupportedGames;
	g_Config.m_Settings.showSupportedGames = !previous;

	if (!g_Config.Save())
	{
		g_Config.m_Settings.showSupportedGames = previous;
		NotifyError(T(TEXT_GAMELIST_SAVE_FAILED));
		return;
	}

	if (!g_Config.m_Settings.showSupportedGames)
		Reset();
}

void GameListPanel::OnFrame(bool menuOpen, bool mainMenu)
{
	const bool held = g_Input.IsButtonHeld(Input::BUTTON_SELECT);

	if (!menuOpen || !mainMenu || g_InputUi.IsActive())
	{
		ResetData();
		m_SelectWasDown = held;
		return;
	}

	if (g_Menu.IsInputLocked())
	{
		m_SelectWasDown = held;
		return;
	}

	if (!g_Config.m_Settings.showSupportedGames)
		Reset();
	else if (m_State == Panel_Disabled)
		m_State = Panel_NoServer;

	if (held && !m_SelectWasDown)
		HandleSelect();

	m_SelectWasDown = held;
}

void GameListPanel::HandleManualScroll()
{
	const bool l1Held = g_Input.IsButtonHeld(Input::BUTTON_L1);
	const bool r1Held = g_Input.IsButtonHeld(Input::BUTTON_R1);
	const bool l1Pressed = l1Held && !m_L1WasDown;
	const bool r1Pressed = r1Held && !m_R1WasDown;

	m_L1WasDown = l1Held;
	m_R1WasDown = r1Held;

	if ((m_State != Panel_Ready) || (m_Games.size() <= static_cast<size_t>(VISIBLE_ROWS)) ||
		g_Menu.IsInputLocked())
	{
		return;
	}

	const uint64_t now = GetTimeNow();

	if (l1Pressed)
	{
		m_ScrollIndex = (m_ScrollIndex > static_cast<size_t>(VISIBLE_ROWS))
			? (m_ScrollIndex - VISIBLE_ROWS) : 0;

		m_PauseUntil = now + SCROLL_PAUSE_MS;
		m_LastTick = now;
	}
	else if (r1Pressed)
	{
		const size_t maximum = m_Games.size() - VISIBLE_ROWS;

		m_ScrollIndex += VISIBLE_ROWS;

		if (m_ScrollIndex > maximum)
			m_ScrollIndex = maximum;

		m_PauseUntil = now + SCROLL_PAUSE_MS;
		m_LastTick = now;
	}
}

void GameListPanel::UpdateScroll()
{
	if ((m_State != Panel_Ready) || (m_Games.size() <= static_cast<size_t>(VISIBLE_ROWS)))
		return;

	const uint64_t now = GetTimeNow();

	if (now < m_PauseUntil)
		return;

	if (m_LastTick == 0)
	{
		m_LastTick = now;
		return;
	}

	if ((now - m_LastTick) < SCROLL_STEP_MS)
		return;

	m_LastTick = now;

	const size_t maximum = m_Games.size() - VISIBLE_ROWS;

	if (m_ScrollDirection > 0)
	{
		if (m_ScrollIndex >= maximum)
		{
			m_ScrollDirection = -1;
			return;
		}

		m_ScrollIndex++;
	}
	else
	{
		if (m_ScrollIndex == 0)
		{
			m_ScrollDirection = 1;
			return;
		}

		m_ScrollIndex--;
	}
}

void GameListPanel::DrawText(const std::wstring& text, float x, float y, float size,
	const paf::vec4& color, int horizontalAlign)
{
	g_Renderer.Text(text, paf::vec2(x, y), size, color,
		static_cast<Renderer::Align>(horizontalAlign), Renderer::Centered);
}

void GameListPanel::DrawFrame(float x, float y, float width, float height, float fade)
{
	g_Renderer.Rectangle(
		paf::vec2(x, y), paf::vec2(width, height),
		paf::vec4(0.0f, 0.0f, 0.0f, 0.88f * fade),
		Renderer::Centered, Renderer::Centered);

	g_Renderer.Rectangle(
		paf::vec2(x, y + height / 2.0f - 2.0f), paf::vec2(width - 28.0f, 2.0f),
		paf::vec4(g_Menu.colorMenu.r, g_Menu.colorMenu.g, g_Menu.colorMenu.b, 0.90f * fade),
		Renderer::Centered, Renderer::Centered);
}

void GameListPanel::Draw()
{
	if (!g_Config.m_Settings.showSupportedGames || !g_Menu.IsOpened() || !g_Menu.IsMainMenu())
		return;

	if ((m_State == Panel_Disabled) || (m_State == Panel_NoServer))
		return;

	const float fade = g_Menu.GetTextOpacity();

	if (fade <= 0.0f)
		return;

	HandleManualScroll();
	UpdateScroll();

	const float viewportWidth = g_Renderer.m_ViewportWidth;
	const float viewportHeight = g_Renderer.m_ViewportHeight;

	const float menuRight = g_Menu.position.x + g_Menu.sizeWidth / 2.0f;
	const float menuLeft = g_Menu.position.x - g_Menu.sizeWidth / 2.0f;

	const float rightX = menuRight + PANEL_GAP + PANEL_WIDTH / 2.0f;
	const float leftX = menuLeft - PANEL_GAP - PANEL_WIDTH / 2.0f;

	const float minX = -viewportWidth / 2.0f + PANEL_MARGIN + PANEL_WIDTH / 2.0f;
	const float maxX = viewportWidth / 2.0f - PANEL_MARGIN - PANEL_WIDTH / 2.0f;

	float x = rightX;

	if (x > maxX)
		x = leftX;

	if (x < minX)
		x = minX;

	if (x > maxX)
		x = maxX;

	float y = g_Menu.position.y;

	const float minY = -viewportHeight / 2.0f + PANEL_MARGIN + PANEL_HEIGHT / 2.0f;
	const float maxY = viewportHeight / 2.0f - PANEL_MARGIN - PANEL_HEIGHT / 2.0f;

	if (y < minY)
		y = minY;

	if (y > maxY)
		y = maxY;

	DrawFrame(x, y, PANEL_WIDTH, PANEL_HEIGHT, g_Menu.GetBackgroundOpacity());

	const float left = x - PANEL_WIDTH / 2.0f + PANEL_MARGIN;
	const float right = x + PANEL_WIDTH / 2.0f - PANEL_MARGIN;
	const float top = y + PANEL_HEIGHT / 2.0f;
	const float bottom = y - PANEL_HEIGHT / 2.0f;

	const paf::vec4 textColor(1.0f, 1.0f, 1.0f, fade);
	const paf::vec4 dimColor(1.0f, 1.0f, 1.0f, 0.66f * fade);

	DrawText(T(TEXT_GAMELIST_TITLE), left, top - 28.0f, 16.0f, textColor, Renderer::Left);

	if (m_State == Panel_Ready)
	{
		DrawText(to_wstring(static_cast<uint64_t>(m_Games.size())),
			right, top - 28.0f, 16.0f, textColor, Renderer::Right);
	}

	const std::wstring aliases = AliasLine();

	if (!aliases.empty())
		DrawText(aliases, left, top - 50.0f, 12.0f, dimColor, Renderer::Left);

	std::wstring message0;
	std::wstring message1;

	switch (m_State)
	{
		case Panel_MissingFile:
			message0 = T(TEXT_GAMELIST_MISSING_FILE);
			message1 = T(TEXT_GAMELIST_MISSING_HINT);
			break;

		case Panel_Unreadable:
			message0 = T(TEXT_GAMELIST_UNREADABLE);
			break;

		case Panel_Invalid:
			message0 = T(TEXT_GAMELIST_INVALID);
			break;

		case Panel_NoSection:
		case Panel_EmptySection:
			message0 = T(TEXT_GAMELIST_EMPTY);
			break;

		default:
			break;
	}

	if (!message0.empty())
		DrawText(message0, left, top - 108.0f, 15.0f, textColor, Renderer::Left);

	if (!message1.empty())
		DrawText(message1, left, top - 132.0f, 13.0f, dimColor, Renderer::Left);

	if (m_State == Panel_Ready)
	{
		const size_t first = m_ScrollIndex;
		const size_t last = (first + VISIBLE_ROWS < m_Games.size())
			? (first + VISIBLE_ROWS) : m_Games.size();

		for (size_t i = first; i < last; i++)
		{
			const float rowY = top - 58.0f - 10.0f - static_cast<float>(i - first) * 20.0f;
			DrawText(Truncate(m_Games[i], 48), left, rowY, 15.0f, textColor, Renderer::Left);
		}
	}

	const float footerY = bottom + 28.0f;

	DrawText(T(TEXT_GAMELIST_HIDE), left, footerY, 12.0f, dimColor, Renderer::Left);

	if ((m_State == Panel_Ready) && (m_Games.size() > static_cast<size_t>(VISIBLE_ROWS)))
	{
		const size_t first = m_ScrollIndex;
		const size_t last = (first + VISIBLE_ROWS < m_Games.size())
			? (first + VISIBLE_ROWS) : m_Games.size();

		std::wstring page = to_wstring(static_cast<uint64_t>(first + 1));
		page += L"-";
		page += to_wstring(static_cast<uint64_t>(last));
		page += L" / ";
		page += to_wstring(static_cast<uint64_t>(m_Games.size()));

		DrawText(T(TEXT_GAMELIST_PAGE), x, footerY, 11.0f, dimColor, Renderer::Centered);
		DrawText(page, right, footerY, 11.0f, dimColor, Renderer::Right);
	}
}
