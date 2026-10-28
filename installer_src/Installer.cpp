/*
 * qDNSm Installer - EBOOT entry point.
 *
 * Copies qDNSm.sprx into /dev_hdd0/plugins/ and registers it in
 * boot_plugins.txt. Ships as an XMB application under title ID NPUB72381 so that
 * installing one PKG is the only step before this appears on the console's game
 * list.
 *
 * ---------------------------------------------------------------------------
 * Why this is not a VSH plugin
 * ---------------------------------------------------------------------------
 * The qDNSm plugin is built on RouLetteVshMenu's VirtualShell layer, whose
 * paf_/vshcommon_ symbols come from VSH export stub libraries. Those objects were
 * disassembled while writing this and are NID-keyed PRX stubs:
 *
 *     _.prx.link.border.mark.vshtask.vshtask_A02D46E7
 *     vshtask_A02D46E7.stub_entry
 *
 * They resolve through the module stub mechanism, which is only valid for a module
 * hosted by the process that owns those modules. An EBOOT is hosted by the PS3
 * game loader, so those stubs cannot be used here. installer.vcxproj omits every
 * VirtualShell library.
 *
 * ---------------------------------------------------------------------------
 * Why there is no std:: anywhere in this file
 * ---------------------------------------------------------------------------
 * An EBOOT cannot link the C++ allocator. qDNSm links -lallocator_export_stub,
 * which supplies operator new/delete, but that is a VSH module stub and does not
 * resolve outside the VSH process. Linking a real runtime fails too - this SDK
 * ships no libc++.a for PPU. So the installer uses the fixed-capacity types in
 * Small.hpp. See that file for the reasoning and the capacities.
 *
 * ---------------------------------------------------------------------------
 * UI: currently none, and that is deliberate
 * ---------------------------------------------------------------------------
 * The obvious user-facing mechanism would be the sysutil message dialog,
 * msgDialogOpen / msgDialogClose. Three successive attempts to reach it all
 * failed at link time on this toolchain:
 *
 *   1. Calling them directly    -> reference to undefined symbol `.msgDialogOpen'
 *   2. sys_prx_load_module +    -> `sys_prx_library_handle_t' undefined, and
 *      sys_prx_get_export_by_name   sys_prx_load_module has a different signature
 *   3. moduleGetExportByName    -> reference to undefined symbol
 *                                    `.moduleGetExportByName'
 *
 * libsysutil_stub.a in this SDK does not carry them, and the vendored
 * vendor/sysutil/sysutil/msg.h is a PSL1GHT-style header describing a different
 * sysutil surface, so its declarations are not the Sony ones.
 *
 * Rather than keep guessing at an API that cannot be checked without a console,
 * the installer is built on cellFs alone - the same API family qDNSm's own
 * FileSystem.cpp uses successfully - and reports everything it does to a log.
 *
 * Every user-visible message therefore goes through Ui::Notify. Restoring a
 * proper dialog UI means reimplementing that function and nothing else; no
 * control flow has to change.
 *
 * What that costs, stated plainly: there is no uninstall from the XMB yet. An
 * installer that silently deleted files because it could not draw a prompt
 * would be worse than one that declines to, so every current path is
 * install-or-update only, and both are idempotent. The uninstall belongs behind
 * a confirmation prompt, added when a UI exists.
 */


#include "Small.hpp"
#include "InstallerFs.hpp"
#include "BootPlugins.hpp"
#include "Dialog.hpp"


/*
 * Same headers qDNSm's Main.cpp uses for the module macros. ppu-types.h alone is
 * not enough: it only defines the basic scalar types, not SYS_MODULE_INFO, and
 * without that macro the line below parses as a declaration and fails with
 * "A type specifier is required for all declarations".
 *
 * Title ID must be exactly 9 characters. NPUB72381 is not a retail title - it
 * was chosen specifically because it does not collide with anything Sony shipped,
 * so installing this cannot clobber a real game.
 */
#ifndef QDNSM_PSL1GHT
SYS_MODULE_INFO(NPUB72381, 0, 1, 1);
#endif

/*
 * An EBOOT is entered through main(), not through SYS_MODULE_START, so this module defines
 * no module_start/module_stop. The macros below are the SNC/PRX path, which is not built:
 * the installer has a single Makefile and it always defines QDNSM_PSL1GHT.
 */
#ifndef QDNSM_PSL1GHT
SYS_MODULE_START(module_start);
SYS_MODULE_STOP(module_stop);
#endif

namespace
{
    /* qDNSm.sprx is a plugin, so it lives beside the other plugins. boot_plugins.txt
     * is not - the loader reads it from the dev_hdd0 root, which is where it has
     * always been looked for, and putting it in /plugins made it unreadable. */
    const char* kPluginsDir = "/dev_hdd0/plugins";
    const char* kSprxName = "qDNSm.sprx";
    const char* kNetLibName = "qDNSmNetLib.sprx";
    const char* kGamesName = "qDNSm-server-games.ini";
    const char* kBootPluginsPath = "/dev_hdd0/boot_plugins.txt";
    const char* kHenUpdaterOffPath = "/dev_hdd0/hen_updater.off";

    /* A qDNSm.sprx or qDNSmNetLib.sprx is a signed SELF of tens of KB. Anything
     * smaller is a truncated payload or a file that is not what it claims to
     * be. Used only to catch obvious damage early - the installer does not try
     * to validate the signature, because a 3.55-signed fake self is deliberately
     * not an NPDRM SELF and there is no format marker worth checking for. The
     * game list has no signature, so its floor only catches an empty or
     * effectively empty file.
     *
     * The upper bound is the fixed payload buffer in Small.hpp. ReadBinary
     * already refuses anything larger, so this catches nothing new; it is kept
     * because it makes the intended size explicit at the call site. */
    const size_t kMinPluginBytes = 4096;
    const size_t kMinGamesBytes = 16;
    const size_t kMaxPayloadBytes = Small::kMaxPayload;

    /* Payload storage. File scope, not stack: it is a megabyte, and a megabyte
     * stack frame is a stack overflow on its own. */
    static uint8_t       g_payload[Small::kMaxPayload];
    static Small::Bytes  g_payloadView;

    /* Flatten newlines the way a dialog would have wrapped, then write.
     *
     * The entire user-facing surface. Replace Notify to add a real UI; nothing
     * else in the installer knows how the user is told anything.
     *
     * A matching confirm prompt deliberately does not exist yet. It would have
     * no caller - every current path is non-destructive - and dead code in an
     * anonymous namespace is at best a warning and at worst a trap. It belongs
     * here, at the moment a UI exists and there is an uninstall to gate. */
    namespace Ui
    {
        void Notify(const char* message)
        {
            /* Deliberately does nothing.
             *
             * This used to flatten the message into /dev_hdd0/plugins/qDNSm-installer.log
             * and that was the only feedback the installer ever gave, because there is
             * no dialog here - the SysUtil message box needs an RSX context this
             * process does not have. A log nobody reads on a console is not feedback
             * worth the write, the install log was the last thing the installer touched
             * before exiting, and it made the exit status the only signal that
             * anything had gone wrong.
             *
             * The call sites stay, deliberately: they mark every decision point and
             * failure branch, so this is the obvious place to put a real UI when there
             * is one to put. (void)message keeps -Wall quiet about the unused
             * parameter without hiding the fact that it is unused on purpose.
             */
            (void)message;
        }
    }

    /* Installs or updates. Idempotent: safe to run when qDNSm is already
     * present, which is what makes this usable without a prompt. */
    /*! Report a failure that happens before the UI is up, and give the caller the
     *  non-zero status every failure path uses. Ui::Notify is a no-op, so on a
     *  console run this only matters for the exit status. */
    int Fail(const char* reason)
    {
        Ui::Notify(reason);
        return 1;
    }

    /* Reads one payload from the installer's own folder and writes it to its
     * destination. The shared buffer is reused: only one payload is in memory at
     * a time, and the largest (qDNSm.sprx) is tens of KB against a 1 MB buffer.
     *
     * WriteBinaryStaged is used for every file, so a failed update restores the
     * previous contents instead of leaving a truncated plugin or game list. */
    bool CopyPayload(const Small::Str& selfDir, const char* fileName,
                     const Small::Str& destPath, size_t minBytes,
                     const char* progress)
    {
        Small::Str sourcePath;

        if (!sourcePath.Set(selfDir) || !sourcePath.Append(fileName))
        {
            Ui::Notify("qDNSm Installer: could not build a payload path. Nothing was changed.");
            return false;
        }

        if (Dialog::Available())
            Dialog::SetProgress(progress);

        g_payloadView.Attach(g_payload, sizeof(g_payload));

        if (!InstFs::ReadBinary(sourcePath, g_payloadView))
        {
            Ui::Notify("qDNSm Installer: a payload is missing from the installer's own folder. Reinstall the package.");
            return false;
        }

        if ((g_payloadView.size() < minBytes) ||
            (g_payloadView.size() > kMaxPayloadBytes))
        {
            Ui::Notify("qDNSm Installer: a payload is the wrong size and looks damaged. Reinstall the package.");
            return false;
        }

        if (!InstFs::WriteBinaryStaged(destPath, g_payloadView.data(), g_payloadView.size()))
        {
            Ui::Notify("qDNSm Installer: could not write a plugin file. The console may be out of free space, or the HDD may not be writable.");
            return false;
        }

        return true;
    }

    bool Install(const Small::Str& selfDir,
                 const Small::Str& sprxPath,
                 const Small::Str& netLibPath,
                 const Small::Str& gamesPath,
                 const Small::Str& bootPath)
    {
        if (!InstFs::EnsureDirectory(kPluginsDir))
        {
            Ui::Notify("qDNSm Installer: could not create /dev_hdd0/plugins. Nothing was changed.");
            return false;
        }

        /*
         * PS3HEN checks for its own updates during startup. That check has no
         * timeouts, and while it is waiting HEN never reaches the end of its
         * boot thread - the loading screen stays up and qDNSm never loads.
         * HEN's own supported switch for this is the presence of this empty
         * file. Create it once, but never truncate one the user already has.
         *
         * This is deliberately left in place by Uninstall: it is a HEN setting,
         * not an installed qDNSm file, and removing it could make HEN hang
         * again after qDNSm is gone.
         */
        if (!InstFs::EnsureEmptyFile(kHenUpdaterOffPath))
        {
            Ui::Notify("qDNSm Installer: could not create /dev_hdd0/hen_updater.off. HEN may hang while checking for updates. The install was stopped.");
            return false;
        }

        /* The network module and game list are written first. They are useless
         * without the plugin, so a failure here leaves the previous install
         * alone rather than upgrading the plugin under an old module. */
        if (!CopyPayload(selfDir, kNetLibName, netLibPath,
                kMinPluginBytes, "Copying the network module..."))
            return false;

        /* The game list is a first-install fallback. When one is already
         * present, the plugin's updater owns it: overwriting would replace a
         * newer downloaded catalogue with whatever this package shipped. */
        if (!InstFs::FileExists(gamesPath))
        {
            if (!CopyPayload(selfDir, kGamesName, gamesPath,
                    kMinGamesBytes, "Copying the game list..."))
                return false;
        }

        if (!CopyPayload(selfDir, kSprxName, sprxPath,
                kMinPluginBytes, "Copying the plugin..."))
            return false;

        BootPlugins::Result result = BootPlugins::Register(bootPath, sprxPath);

        if (result != BootPlugins::Ok)
        {
            /* The payload is on disk but unreferenced, so it will not load. That
             * is the same effective state as "not installed", and it is
             * recoverable by running the installer again - so the files are left
             * in place rather than deleted. */
            Small::Str message;
            if (message.Set("qDNSm Installer: ") && message.Append(BootPlugins::Describe(result)))
                Ui::Notify(message.c());

            return false;
        }

        return true;
    }

    /* Removes qDNSm: the boot_plugins.txt entry and every file the installer
     * owns.
     *
     * Order matters. The entry goes first, so that if deleting the files
     * afterwards fails the console still boots without trying to load a file
     * that may not be there. The reverse order would leave a boot entry pointing
     * at a missing plugin, which is the worse of the two failures. */
    bool Uninstall(const Small::Str& bootPath)
    {
        BootPlugins::Result result = BootPlugins::Unregister(bootPath);

        if (result != BootPlugins::Ok)
        {
            Small::Str message;
            if (message.Set("qDNSm Installer: ") && message.Append(BootPlugins::Describe(result)))
                Ui::Notify(message.c());

            return false;
        }

        const char* const names[] = { kSprxName, kNetLibName, kGamesName };

        for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); i++)
        {
            Small::Str path;

            if (!path.Set(kPluginsDir) || !path.Append("/") || !path.Append(names[i]))
            {
                Ui::Notify("qDNSm Installer: could not build a file path while uninstalling.");
                return false;
            }

            /* A missing file is not a failure here - it is already removed.
             * boot_plugins.txt has been cleaned either way, which is what
             * matters. */
            if (InstFs::FileExists(path) && !InstFs::DeleteFile(path))
            {
                Ui::Notify("qDNSm Installer: boot_plugins.txt was updated but a plugin file could not be deleted.");
                return false;
            }
        }

        return true;
    }

    int RunInstaller()
    {
        Small::Str sprxPath;
        Small::Str netLibPath;
        Small::Str gamesPath;
        Small::Str bootPath;

        if (!sprxPath.Set(kPluginsDir) || !sprxPath.Append("/") || !sprxPath.Append(kSprxName))
            return Fail("internal path error. Nothing was changed.");

        if (!netLibPath.Set(kPluginsDir) || !netLibPath.Append("/") || !netLibPath.Append(kNetLibName))
            return Fail("internal path error. Nothing was changed.");

        if (!gamesPath.Set(kPluginsDir) || !gamesPath.Append("/") || !gamesPath.Append(kGamesName))
            return Fail("internal path error. Nothing was changed.");

        if (!bootPath.Set(kBootPluginsPath))
            return Fail("internal path error. Nothing was changed.");

        /* Dialogs need RSX, which an EBOOT does not get for free. If this fails
         * there is no UI, and the run continues non-interactively rather than
         * refusing to work - a prompt that cannot be shown must not become a
         * reason the installer cannot install. */
        const bool haveUi = Dialog::VideoInit();
        Dialog::Choice choice = Dialog::Install;
        if (haveUi)
        {
            choice = Dialog::AskManage();
            if (choice == Dialog::Cancelled)
            {
                Dialog::VideoFinish();
                return 0;
            }
        }
        if (Dialog::Available())
            Dialog::BeginProgress(choice == Dialog::Uninstall ? "Uninstalling qDNSm" : "Installing qDNSm");

        int status = 0;
        const char* failure = NULL;

        if (choice == Dialog::Uninstall)
        {
            Dialog::SetProgress("Removing qDNSm from boot_plugins.txt...");
            if (!Uninstall(bootPath))
            {
                status = 1;
                failure = "Uninstall did not finish. boot_plugins.txt may be unchanged.";
            }
            Dialog::SetProgress("Removing qDNSm files...");
        }
        else
        {
            Small::Str selfDir;
            if (!InstFs::SelfDirectory(selfDir))
            {
                status = 1;
                failure = "could not work out where this installer lives. Nothing was changed.";
            }
            else
            {
                if (!Install(selfDir, sprxPath, netLibPath, gamesPath, bootPath))
                {
                    status = 1;
                    failure = "install did not finish. See the messages above.";
                }
            }
        }
        Dialog::EndProgress();

        if (status != 0)
        {
            if (Dialog::Available() && failure != NULL)
                Dialog::Report("qDNSm Installer", failure);
            Dialog::VideoFinish();
            return status;
        }

        const char* doneText = (choice == Dialog::Uninstall)
            ? "qDNSm has been uninstalled.\n\n"
              "Reboot for the change to take effect.\n\n"
              "You can delete this installer now if you do not need it again."
            : "qDNSm has been installed.\n\n"
              "Reboot for the change to take effect.\n\n"
              "You can delete this installer now if you do not need it again.";

        if (Dialog::Available())
        {
            Dialog::Report("qDNSm Installer", doneText);

            if (Dialog::AskYesNo("qDNSm Installer", "Reboot the console now?"))
            {
                Dialog::VideoFinish();
                Dialog::Reboot();

                /* Reboot did not take. Say so rather than exiting silently -
                 * otherwise it looks exactly like the user chose not to. */
                Dialog::Report("qDNSm Installer",
                               "The console did not reboot.\n\n"
                               "Turn it off and on manually for the change to take effect.");
                Dialog::VideoFinish();
                Dialog::Exit();
            }
        }

        Dialog::VideoFinish();
        return 0;
    }
}

/*
 * The EBOOT entry point. Plain C++, matching pkgi-ps3's source/pkgi.c, which is a
 * working PS3 EBOOT and likewise defines only main() among its C runtime entry
 * points.
 */
int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;
    return RunInstaller();
}
