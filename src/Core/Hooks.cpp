#include "Hooks.hpp"
#include "Memory/Pattern.hpp"
#include "Menu/Base.hpp"
#include "Menu/Submenus.hpp"   // UpdateDnsFlow
#include "GameDirectory.hpp"
#include "GameListPanel.hpp"
#include "Network.hpp"
#include "NetUpdate.hpp"
#include "OpenSpyPatch.hpp"
#include "InputUi.hpp"
#include <vsh/vshmain.hpp>
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

static bool s_WasGameRunning = false;

void pafFrameworkBeginHook(void* framework, float frameTime)
{
    pafFrameworkBeginHk->GetOriginal<void>(framework, frameTime);

    g_Helpers.OnUpdate();

    const bool gameRunning =
        vsh::GetCooperationMode() != vsh::CooperationMode::XMB;

    if (gameRunning && !s_WasGameRunning && g_Menu.IsOpened())
        g_Menu.ForceClose();

    s_WasGameRunning = gameRunning;

    g_Renderer.OnUpdate();
    g_Menu.SetBusyState(IsDnsApplyBusy() || IsOpenSpyPatchBusy(),
        GetDnsApplyPhase() == DnsApply_Reconnecting, IsOpenSpyPatchBusy());
    g_GameListPanel.OnFrame(g_Menu.IsOpened(), g_Menu.IsMainMenu());
    GameDirectory_OnFrame(g_Menu.IsOpened());
    OpenSpyModule_OnFrame(g_Menu.IsOpened());
    g_Menu.OnUpdate();

    /*
     * Pumped here rather than from MainSubmenu(): if a flow finishes while the user is inside the
     * options submenu, MainSubmenu is not the active menu function and its result would never be
     * applied.
     */
    UpdateDnsFlow();
    PollNetUpdate();
    PollOpenSpyPatch();

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

    if (g_Menu.IsOpened() || g_InputUi.IsActive())
    {
        /* The raw copy above belongs to qDNSm; every VSH reader gets an idle pad. */
        vsh::memset(data->button, 0, sizeof(data->button));
        data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_X] = 128;
        data->button[CELL_PAD_BTN_OFFSET_ANALOG_RIGHT_Y] = 128;
        data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_X] = 128;
        data->button[CELL_PAD_BTN_OFFSET_ANALOG_LEFT_Y] = 128;
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
