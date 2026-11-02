#pragma once

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>
#include <vsh/paf.hpp>

class GameListPanel
{
public:
	void SetHoveredIndex(int index);
	void SetOpenSpyHovered();
	void OnFrame(bool menuOpen, bool mainMenu);
	void Draw();
	void Reset();
	void Invalidate();

private:
	enum PanelState
	{
		Panel_Disabled,
		Panel_NoServer,
		Panel_MissingFile,
		Panel_Unreadable,
		Panel_Invalid,
		Panel_NoSection,
		Panel_EmptySection,
		Panel_Ready
	};

	void ResetData();
	void LoadForIndex(int index);
	void LoadForKey(const std::string& key);
	void RebuildAliases();
	void UpdateScroll();
	void HandleManualScroll();
	void HandleSelect();
	void UpdateVisibleRows();
	void DrawFrame(float x, float y, float width, float height, float fade);
	void DrawText(const std::wstring& text, float x, float y, float size,
		const paf::vec4& color, int horizontalAlign);
	std::wstring AliasLine() const;

	int m_Index = -1;
	PanelState m_State = Panel_NoServer;
	bool m_OpenSpy = false;

	std::string m_Key;
	std::wstring m_Name;

	std::vector<std::wstring> m_Games;
	std::vector<std::wstring> m_SectionAliases;
	std::vector<std::wstring> m_Aliases;

	size_t m_ScrollIndex = 0;
	size_t m_VisibleRows = 12;
	int m_ScrollDirection = 1;
	uint64_t m_LastTick = 0;
	uint64_t m_PauseUntil = 0;

	bool m_SelectWasDown = false;
	bool m_L1WasDown = false;
	bool m_R1WasDown = false;
};

extern GameListPanel g_GameListPanel;
