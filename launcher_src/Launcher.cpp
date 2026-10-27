#include "Dialog.hpp"
#include "InstallerFs.hpp"
#include "LauncherFiles.hpp"
#include "LauncherLoader.hpp"

#include <stdio.h>

namespace
{
    const char* const kTitle = "qDNSm Launcher";
    const char* const kTitleId = "NPUB72382";
    const char* const kPluginsDir = "/dev_hdd0/plugins";
    const char* const kPluginPath = "/dev_hdd0/plugins/qDNSm.sprx";
    const char* const kGameListPath = "/dev_hdd0/plugins/qDNSm-server-games.ini";

    int FinishSuccess()
    {
        Dialog::VideoFinish();
        return 0;
    }

    int Finish(int status, const char* message)
    {
        if (Dialog::Available())
            Dialog::Report(kTitle, message);

        Dialog::VideoFinish();
        return status;
    }

    int FinishCode(int status, const char* text, int code)
    {
        char message[512];
        snprintf(message, sizeof(message), "%s\n\nError: 0x%08X", text, (unsigned int)code);
        return Finish(status, message);
    }

    int FinishFile(LauncherFiles::Status status, const char* name)
    {
        char message[512];

        switch (status)
        {
            case LauncherFiles::SourceMissing:
                snprintf(message, sizeof(message), "%s is missing from the launcher package. Reinstall it.", name);
                break;

            case LauncherFiles::SourceTooSmall:
                snprintf(message, sizeof(message), "%s in the launcher package is damaged. Reinstall it.", name);
                break;

            case LauncherFiles::SourceTooLarge:
                snprintf(message, sizeof(message), "%s in the launcher package is too large.", name);
                break;

            case LauncherFiles::ReadFailed:
                snprintf(message, sizeof(message), "Could not read %s from the launcher package.", name);
                break;

            case LauncherFiles::WriteFailed:
                snprintf(message, sizeof(message), "Could not copy %s to /dev_hdd0/plugins.\n\nThe console may be out of free space.", name);
                break;

            case LauncherFiles::InternalError:
                snprintf(message, sizeof(message), "Could not build the path for %s.", name);
                break;

            default:
                snprintf(message, sizeof(message), "Could not prepare %s.", name);
                break;
        }

        return Finish(1, message);
    }
}

int main(int argc, const char* argv[])
{
    (void)argc;
    (void)argv;

    Dialog::VideoInit();

    if (Dialog::Available() &&
        !Dialog::AskYesNo(kTitle,
                          "Load qDNSm into the XMB now?\n\n"
                          "This does not change boot_plugins.txt.\n\n"
                          "After loading, press R1 + R2 + R3 to open the menu."))
    {
        Dialog::VideoFinish();
        return 0;
    }

    const LauncherLoader::ProbeResult probe = LauncherLoader::Probe();

    if (probe.status == LauncherLoader::Probe_AlreadyLoaded)
        return FinishSuccess();

    if (probe.status == LauncherLoader::Probe_InspectionFailed)
    {
        return FinishCode(1,
                          "Could not inspect the VSH plugin slots.\n\n"
                          "This launcher needs Cobra/Mamba CFW or PS3HEN with syscall 8 enabled.",
                          probe.code);
    }

    if (probe.status == LauncherLoader::Probe_NoFreeSlot)
    {
        return Finish(1,
                      "No free VSH plugin slot was found in slots 1-6.\n\n"
                      "Disable another plugin or reboot, then run this launcher again.");
    }

    Small::Str pluginsDir;
    Small::Str pluginPath;
    Small::Str gameListPath;
    Small::Str sourceDir;

    if (!pluginsDir.Set(kPluginsDir) ||
        !pluginPath.Set(kPluginPath) ||
        !gameListPath.Set(kGameListPath) ||
        !InstFs::SelfDirectoryFor(kTitleId, sourceDir))
    {
        return Finish(1, "Could not build the launcher paths.");
    }

    if (!InstFs::EnsureDirectory(pluginsDir))
        return Finish(1, "Could not create /dev_hdd0/plugins.");

    bool copiedPlugin = false;
    LauncherFiles::Status fileStatus =
        LauncherFiles::EnsurePlugin(sourceDir, pluginPath, copiedPlugin);
    if (fileStatus != LauncherFiles::Ok)
        return FinishFile(fileStatus, "qDNSm.sprx");

    bool copiedGameList = false;
    fileStatus = LauncherFiles::EnsureGameList(sourceDir, gameListPath, copiedGameList);
    if (fileStatus != LauncherFiles::Ok)
        return FinishFile(fileStatus, "qDNSm-server-games.ini");

    const LauncherLoader::LoadResult load = LauncherLoader::Load(probe.slot, pluginPath.c());

    switch (load.status)
    {
        case LauncherLoader::Load_Ok:
            return FinishSuccess();

        case LauncherLoader::Load_Unavailable:
            return FinishCode(1,
                              "The console does not expose the CFW/HEN VSH plugin loader.\n\n"
                              "This launcher needs Cobra/Mamba CFW or PS3HEN with syscall 8 enabled.",
                              load.code);

        case LauncherLoader::Load_Rejected:
            return FinishCode(1,
                              "The console rejected qDNSm.sprx.\n\n"
                              "The plugin may be corrupted or incorrectly signed.",
                              load.code);

        case LauncherLoader::Load_Missing:
            return FinishCode(1,
                              "qDNSm.sprx is missing from /dev_hdd0/plugins.",
                              load.code);

        case LauncherLoader::Load_NoFreeSlot:
            return FinishCode(1,
                              "The selected VSH plugin slot became occupied.",
                              load.code);

        default:
            return FinishCode(1, "qDNSm could not be loaded.", load.code);
    }
}
