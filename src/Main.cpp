#include <sys/prx.h>
#include <sys/ppu_thread.h>
#include <cell/cell_fs.h>
#include <vsh/newDelete.hpp>
#include <vsh/vshmain.hpp>
#include "Utils/SystemCalls.hpp"
#include "Utils/Timers.hpp"
#include "Core/Input.hpp"
#include "Core/Menu/Submenus.hpp"
#include "Core/Hooks.hpp"
#include "Core/Config.hpp"
#include "Core/Network.hpp"   // StartManualDnsWorker

// Macros defining our module informations as well as our entry/exit point
SYS_MODULE_INFO(qDNSm, 0, 1, 1);
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);

/***
* 
* PS Vita Vsh Menu, they hook into sceDisplaySetFrameBuf to draw text. This will allow drawing in XMB and In Game if done on PS3
* https://github.com/joel16/PSV-VSH-Menu/blob/master/source/vsh.c
* 
* 
* TODO:
* BUG: when exiting a game after StartPayload will result in a crash due to fail to free memory from ps3mapi_process_page_allocate. To fix we need to implement a new syscall ps3mapi_process_page_free, But then there is the caveat when we don't know when the user wan't to exit the game.
* BUG: `bool Menu::IsLeftPressed()` is broken when using the second parameter 'delay'
* Enhancement: Find a way to load eboot alongside it's own sprx (kernel -> load_process)
* Enhancement: add LUA scripting support
* Enhancement: add config to load your favorite menu
* Enhancement: cache Ida2Mem using std::unordered_map. Caveat if user reloads game or if user loads sprx using PS3ToolBox. To fix try a auto refresh or re-cache all modules on function call
* 
* 
* 
* ----------------------
* DEBUG
* ----------------------
* paf_BE04476B(paf::PhFont::GlyphType, paf::SurfaceRCPtr<paf::Surface> &, int, int, wchar_t const*, unsigned int, paf::PhFont const&, paf::PhTextLetterSpace const*);
* paf_F21655F3(); View_Find
* paf_794CEACB(); View_FindWidget
* paf_3A8454FC(); View_GetTexture
* paf_8ABAE2F3(void*, void*, int _0);  _ZN3paf10PhPlaneDivC1EPNS_8PhWidgetEPNS_8PhAppearE | paf::PhPlaneDiv::PhPlaneDiv(paf::PhWidget*, paf::PhAppear*)
* paf_7F0930C6(); PhText_Constructor
* paf_23AFB290(); View_GetInterface
* paf_2CBA5A33(); PhHandler_KillTimerCB
* paf_B93AFE7E(); View_PlaySound
* paf_EAA28B8(); Framework_End
* paf_85D1D23B(); Framework_Run
* paf_0C74837D();
* paf_CC51D56(); paf::Surface::Copy
* paf_C9826818();
* paf_4192B349();
* vshcommon_F1918912(); // NavigationMessageSetDuration
* vshnet_FC7303C5(); // sceNpGetNpId
* 
*/ 

sys_ppu_thread_t gVshMenuPpuThreadId = SYS_PPU_THREAD_ID_INVALID;

static const uint64_t kHenSettleMs = 3000;
static const uint64_t kXmbStableMs = 3000;
static const uint64_t kHenWaitMaxMs = 30000;

static bool HenPluginIsRunning()
{
   CellFsStat stat;
   return cellFsStat("/dev_hdd0/HENplugin.sprx", &stat) == CELL_FS_SUCCEEDED;
}

static bool IsXmbReady()
{
   return paf::View::Find("explore_plugin") &&
          paf::View::Find("xmb_plugin") &&
          paf::View::Find("system_plugin") &&
          vsh::GetCooperationMode() == vsh::CooperationMode::XMB;
}

static void WaitForStableXmb()
{
   const bool isHen = IsConsoleHen();

   /*
    * HEN starts HENplugin.sprx and then loads boot plugins, so qDNSm can run
    * while HEN's own thread is still working. HEN unlinks that file near the
    * end of its thread. Wait for it before touching paf at all; if it is gone
    * already (CFW, or a HEN version that does not use it), the stable-XMB
    * window below is the fallback.
    */
   if (isHen && HenPluginIsRunning())
   {
      const uint64_t start = GetTimeNow();

      while (HenPluginIsRunning() &&
             (GetTimeNow() - start) < kHenWaitMaxMs)
      {
         Sleep(500);
      }
   }

   while (!paf::View::Find("explore_plugin"))
      Sleep(1000);

   /*
    * Only HEN needs the extra wait. On CFW the explore view is the same
    * readiness signal the old code used, so behavior there is unchanged.
    */
   if (!isHen)
      return;

   const uint64_t start = GetTimeNow();
   uint64_t readySince = 0;

   while ((GetTimeNow() - start) < kHenWaitMaxMs)
   {
      if (!IsXmbReady())
      {
         readySince = 0;
         Sleep(250);
         continue;
      }

      if (readySince == 0)
         readySince = GetTimeNow();

      const uint64_t now = GetTimeNow();

      if ((now - start) >= kHenSettleMs &&
          (now - readySince) >= kXmbStableMs)
      {
         return;
      }

      Sleep(250);
   }
}

CDECL_BEGIN
int module_start(unsigned int args, void* argp)
{
   sys_ppu_thread_create(&gVshMenuPpuThreadId, [](uint64_t arg) -> void
   {
      WaitForStableXmb();

      g_Config.Load();

      g_Input = Input();
      g_Renderer = Renderer();
      g_Helpers = Helpers();
      g_Menu = Menu(MainSubmenu);

       InstallHooks();

      /*
       * That is the whole startup.
       *
       * One worker thread, for one job: making sure the console is set to manual DNS after an apply.
       * With automatic DNS the service takes its servers from DHCP and ignores what qDNSm just
       * wrote, which looks identical to qDNSm failing. It is on its own thread because the original
       * author left that call commented out with the note "Causes XMB to freeze", and nothing on
       * the XMB's frame loop should be able to freeze the XMB's frame loop.
       *
       * Everything else that used to start here is gone: a diagnostic logger that rewrote a 64 KB
       * file on every line, a sampler thread, a netctl/sys_net detour set, a sweep across
       * netctl_main's undocumented exports, and a worker that tried to cycle the connection after a
       * DNS apply. The connection worker went last - the XMB's own "Test Network Connection" does a
       * full interface restart, but only from a healthy stack, and a disconnect performed from here
       * leaves it unhealthy enough that the test refuses with "An Ethernet Cable is not connected."
       *
       * Nothing here touches the network stack any more beyond writing settings.
       */
      StartManualDnsWorker();

	  if (g_Config.m_Settings.showNotification)
	  {
		  const wchar_t* message = vsh::wva(L"qDNSm Loaded.\nPress \uF88B + \uF88C + \uF88D to open the menu.");
		  vsh::ShowNotificationWithIcon(message, vsh::NotifyIcon::Info, vsh::NotifySound::None);
	  }

      sys_ppu_thread_exit(0);

   }, 0, 1059, 4096, SYS_PPU_THREAD_CREATE_JOINABLE, "qDNSmStart");

   // Exit thread using directly the syscall and not the user mode library or else we will crash
   _sys_ppu_thread_exit(0);

   return 0;
}

int module_stop(unsigned int args, void* argp)
{
   sys_ppu_thread_t stopPpuThreadId;
   int ret = sys_ppu_thread_create(&stopPpuThreadId, [](uint64_t arg) -> void
   {
      RemoveHooks();

      g_Renderer.~Renderer();

      // Prevent unload too fast (give time to other threads to finish)
      sys_ppu_thread_yield();
      Sleep(1000);

      if (gVshMenuPpuThreadId != SYS_PPU_THREAD_ID_INVALID)
      {
         uint64_t exitCode;
         sys_ppu_thread_join(gVshMenuPpuThreadId, &exitCode);
      }

      sys_ppu_thread_exit(0);
   },
   0, 2816, 1024, SYS_PPU_THREAD_CREATE_JOINABLE, "qDNSmStop");

   if (ret == SUCCEEDED)
   {
      uint64_t exitCode;
      sys_ppu_thread_join(stopPpuThreadId, &exitCode);
   }

   Sleep(5);

   // unloading prx from memory
   sys_prx_id_t prxId = _sys_prx_get_my_module_id();
   uint64_t meminfo[5]{ 0x28, 2, 0, 0, 0 };
   _sys_prx_stop_module(prxId, 0, meminfo, NULL, 0, NULL);

   // Exit thread using directly the syscall and not the user mode library or else we will crash
   _sys_ppu_thread_exit(0);

   return 0;
}
CDECL_END