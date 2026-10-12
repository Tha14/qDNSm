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

#include "TargetCompat.hpp"

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
 * Both entry points are defined, and both are needed.
 *
 * The PRX-format linker script - which is the only container scetool's input can
 * be - places the module around SYS_MODULE_START/module_stop, so those symbols
 * have to exist for the code to be laid out at all. The PS3 game loader, on the
 * other hand, calls main() on an EBOOT.
 *
 * Both delegate to the same RunInstaller(), so the installer logic is reachable
 * from either entry and cannot be stripped as unreachable.
 *
 * The extern "C" below is load-bearing, not stylistic. See the comment on the
 * module_start definition.
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
    const char* kBootPluginsPath = "/dev_hdd0/boot_plugins.txt";

    /* A qDNSm.sprx is a signed SELF of roughly 45 KB. Anything smaller is a
     * truncated payload or a file that is not what it claims to be. Used only to
     * catch obvious damage early - the installer does not try to validate the
     * signature, because a 3.55-signed fake self is deliberately not an
     * NPDRM SELF and there is no format marker worth checking for.
     *
     * The upper bound is the fixed payload buffer in Small.hpp. ReadBinary
     * already refuses anything larger, so this catches nothing new; it is kept
     * because it makes the intended size explicit at the call site. */
    const size_t kMinPayloadBytes = 4096;
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

    bool Install(const Small::Bytes& payload,
                 const Small::Str& sprxPath,
                 const Small::Str& bootPath)
    {
        if (!InstFs::EnsureDirectory(kPluginsDir))
        {
            Ui::Notify("qDNSm Installer: could not create /dev_hdd0/plugins. Nothing was changed.");
            return false;
        }

        /* Staged write: if this is an update over a working install, a failed
         * overwrite must not leave a truncated plugin behind. */
        if (!InstFs::WriteBinaryStaged(sprxPath, payload.data(), payload.size()))
        {
            Ui::Notify("qDNSm Installer: could not write qDNSm.sprx. The console may be out of free space, or the HDD may not be writable.");
            return false;
        }

        BootPlugins::Result result = BootPlugins::Register(bootPath, sprxPath);

        if (result != BootPlugins::Ok)
        {
            /* The payload is on disk but unreferenced, so it will not load. That
             * is the same effective state as "not installed", and it is
             * recoverable by running the installer again - so the sprx is left in
             * place rather than deleted. */
            Small::Str message;
            if (message.Set("qDNSm Installer: ") && message.Append(BootPlugins::Describe(result)))
                Ui::Notify(message.c());

            return false;
        }

        return true;
    }

    /* Removes qDNSm: the plugin file and its boot_plugins.txt entry.
     *
     * Order matters. The entry goes first, so that if deleting the plugin
     * afterwards fails the console still boots without trying to load a file
     * that may not be there. The reverse order would leave a boot entry pointing
     * at a missing plugin, which is the worse of the two failures. */
    bool Uninstall(const Small::Str& sprxPath, const Small::Str& bootPath)
    {
        BootPlugins::Result result = BootPlugins::Unregister(bootPath);

        if (result != BootPlugins::Ok)
        {
            Small::Str message;
            if (message.Set("qDNSm Installer: ") && message.Append(BootPlugins::Describe(result)))
                Ui::Notify(message.c());

            return false;
        }

        /* A missing plugin is not a failure here - it is already uninstalled.
         * boot_plugins.txt has been cleaned either way, which is what matters. */
        if (InstFs::FileExists(sprxPath) && !InstFs::DeleteFile(sprxPath))
        {
            Ui::Notify("qDNSm Installer: boot_plugins.txt was updated but qDNSm.sprx could not be deleted.");
            return false;
        }

        return true;
    }

    int RunInstaller()
    {
        Small::Str sprxPath;
        Small::Str bootPath;

        if (!sprxPath.Set(kPluginsDir) || !sprxPath.Append("/") || !sprxPath.Append(kSprxName))
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
            if (!Uninstall(sprxPath, bootPath))
            {
                status = 1;
                failure = "Uninstall did not finish. boot_plugins.txt may be unchanged.";
            }
            Dialog::SetProgress("Removing qDNSm.sprx...");
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
                Small::Str payloadPath;
                if (!payloadPath.Set(selfDir) || !payloadPath.Append(kSprxName))
                {
                    status = 1;
                    failure = "could not build the payload path. Nothing was changed.";
                }
                else
                {
                    Dialog::SetProgress("Reading qDNSm.sprx...");
                    g_payloadView.Attach(g_payload, sizeof(g_payload));

                    if (!InstFs::ReadBinary(payloadPath, g_payloadView))
                    {
                        status = 1;
                        failure = "qDNSm.sprx is missing from the installer's own folder. Reinstall the package.";
                    }
                    else if (g_payloadView.size() < kMinPayloadBytes || g_payloadView.size() > kMaxPayloadBytes)
                    {
                        status = 1;
                        failure = "qDNSm.sprx is the wrong size and looks damaged. Reinstall the package.";
                    }
                    else
                    {
                        Dialog::SetProgress("Copying the plugin...");
                        if (!Install(g_payloadView, sprxPath, bootPath))
                        {
                            status = 1;
                            failure = "install did not finish. See the messages above.";
                        }
                    }
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
 * extern "C" is required on module_start and module_stop. SYS_MODULE_START emits
 * a reference to the unmangled symbol name; a plain C++ definition mangles to
 * _Z12module_startjPv, the reference is left undefined, and the link fails with:
 *
 *   ppu-lv2-prx-libgen.exe: "module_start" is undefine symbol. can't export.
 *
 * qDNSm's Main.cpp has no extern "C" and still links, because the macro's own
 * declaration supplies the linkage there. Stating it here does not depend on that,
 * and it is not safe to remove - the symptom is an opaque link error, not a
 * warning.
 */
extern "C" int module_start(unsigned int args, void* argp)
{
    (void)args;
    (void)argp;
    return RunInstaller();
}

extern "C" int module_stop(unsigned int args, void* argp)
{
    (void)args;
    (void)argp;
    return 0;
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
