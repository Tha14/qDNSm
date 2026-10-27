#include "Hooks.hpp"
#include "Memory/Pattern.hpp"
#include "Menu/Base.hpp"
#include "Menu/Submenus.hpp"   // UpdateDnsFlow
#include "GameListPanel.hpp"
#include "Network.hpp"
#include "NetUpdate.hpp"
#include "InputUi.hpp"
#include <cell/pad/pad_codes.h>

/*
 * The two hooks the menu needs, and nothing else.
 *
 * The netctl_main and sys_net detours that used to live here are gone. They existed only to watch
 * what the XMB's connection toggle calls into the network control area, and that question is now
 * answered: during a full manual toggle it calls nothing in either library. Whatever the toggle
 * does, it is not reachable from here.
 */

ImportExportDetour* pafFrameworkBeginHk;
Detour* cellPadGetDataExtraInternalHk;

void pafFrameworkBeginHook(void* framework, float frameTime)
{
    pafFrameworkBeginHk->GetOriginal<void>(framework, frameTime);

    g_Helpers.OnUpdate();
    g_Renderer.OnUpdate();
    g_Menu.SetBusyState(IsDnsApplyBusy(), GetDnsApplyPhase() == DnsApply_Reconnecting);
    g_GameListPanel.OnFrame(g_Menu.IsOpened(), g_Menu.IsMainMenu());
    g_Menu.OnUpdate();

    /*
     * Pumped here rather than from MainSubmenu(): if a flow finishes while the user is inside the
     * options submenu, MainSubmenu is not the active menu function and its result would never be
     * applied.
     */
    UpdateDnsFlow();
    PollNetUpdate();

    /*
     * No network calls here, deliberately.
     *
     * This hook runs on the XMB's own frame loop, and blocking work inside it froze this console
     * twice: once with the libnetctl calls, and once with a sampler rewriting a log file several
     * times in a single frame.
     */
}

int cellPadGetDataExtraInternalHook(void* r3, uint32_t portNum, uint32_t* deviceType, CellPadData* data, bool canReturnWithoutClearing)
{
    int returnValue = cellPadGetDataExtraInternalHk->GetOriginal<int>(r3, portNum, deviceType, data, canReturnWithoutClearing);

    if (portNum)
        return returnValue;

    g_Input.SetInputData(data);

    if (g_Menu.IsOpened() && !g_Helpers.m_GameRunning)
    {
        /*
         * Always mask while the menu is up, including while the input UI owns the screen - the XMB
         * must not navigate its own focus behind the dialog. g_Input was filled from the untouched
         * data above, so the UI still receives every press.
         *
         * This used to be `if (!g_InputUi.IsActive())`, which stopped masking exactly when it was
         * most needed and let input through to the XMB.
         *
         * Not while a game is running. The XMB takes the menu's close animation with it when a
         * game starts, so the menu never finishes closing and the mask would otherwise stay on
         * for the rest of the session, and the game would get no buttons at all.
         */
        data->button[CELL_PAD_BTN_OFFSET_DIGITAL1] &= ~(CELL_PAD_CTRL_LEFT | CELL_PAD_CTRL_DOWN | CELL_PAD_CTRL_RIGHT | CELL_PAD_CTRL_UP | CELL_PAD_CTRL_SELECT);
        data->button[CELL_PAD_BTN_OFFSET_DIGITAL2] &= ~(CELL_PAD_CTRL_CROSS | CELL_PAD_CTRL_CIRCLE | CELL_PAD_CTRL_SQUARE | CELL_PAD_CTRL_TRIANGLE | CELL_PAD_CTRL_L1 | CELL_PAD_CTRL_R1);

        if (g_Menu.IsMoving())
        {
            data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] = 128;
            data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] = 128;
        }
    }

    return returnValue;
}

void InstallHooks()
{
    pafFrameworkBeginHk = new ImportExportDetour(ImportExportDetour::Export, "paf", 0x59BDA198, (uintptr_t)pafFrameworkBeginHook);
    cellPadGetDataExtraInternalHk = new Detour(ResolveBranch(FindExportByName("sys_io", 0x3733EA3C)->func + 0x6C), (uintptr_t)cellPadGetDataExtraInternalHook);
}

void RemoveHooks()
{
    delete pafFrameworkBeginHk;
    delete cellPadGetDataExtraInternalHk;
}
