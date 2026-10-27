#include "LauncherFiles.hpp"
#include "LauncherLoader.hpp"
#include "Small.hpp"

#include <cstdio>
#include <string>

namespace FakeFs
{
    void Put(const std::string& path, const std::string& text);
    std::string Get(const std::string& path);
    bool Exists(const std::string& path);
    void Clear();

    extern bool g_FailWrites;
    extern int g_WriteCount;
}

namespace FakeSys
{
    void Clear();
    void SetPlugin(unsigned int slot, const char* name, const char* filename);
    void SetInfoError(unsigned int slot, int code);
    void SetLoadResult(int code);
    int LoadCalls();
    unsigned int LoadSlot();
    const char* LoadPath();
}

static int g_Pass = 0;
static int g_Fail = 0;

static void Check(const char* name, bool condition)
{
    if (condition)
    {
        g_Pass++;
        printf("  ok    %s\n", name);
        return;
    }

    g_Fail++;
    printf("  FAIL  %s\n", name);
}

static Small::Str Path(const char* text)
{
    Small::Str path;
    path.Set(text);
    return path;
}

static void ProbeTests()
{
    printf("\n-- probe --\n");

    {
        FakeSys::Clear();
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("empty slots: ready on slot 1",
              result.status == LauncherLoader::Probe_Ready && result.slot == 1);
    }

    {
        FakeSys::Clear();
        FakeSys::SetPlugin(3, "qDNSm", "/dev_hdd0/plugins/qDNSm.sprx");
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("module name match: already loaded on slot 3",
              result.status == LauncherLoader::Probe_AlreadyLoaded && result.slot == 3);
    }

    {
        FakeSys::Clear();
        FakeSys::SetPlugin(1, "other", "/dev_hdd0/plugins/qDNSm.sprx");
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("filename match: already loaded on slot 1",
              result.status == LauncherLoader::Probe_AlreadyLoaded && result.slot == 1);
    }

    {
        FakeSys::Clear();
        FakeSys::SetPlugin(1, "qDNSm2", "/dev_hdd0/plugins/qDNSm2.sprx");
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("similar name does not match",
              result.status == LauncherLoader::Probe_Ready && result.slot == 2);
    }

    {
        FakeSys::Clear();
        FakeSys::SetPlugin(1, "one", "/dev_hdd0/plugins/one.sprx");
        FakeSys::SetPlugin(2, "two", "/dev_hdd0/plugins/two.sprx");
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("first free slot is 3",
              result.status == LauncherLoader::Probe_Ready && result.slot == 3);
    }

    {
        FakeSys::Clear();
        for (unsigned int slot = 1; slot <= 6; ++slot)
            FakeSys::SetPlugin(slot, "other", "/dev_hdd0/plugins/other.sprx");
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("all slots occupied", result.status == LauncherLoader::Probe_NoFreeSlot);
    }

    {
        FakeSys::Clear();
        FakeSys::SetInfoError(2, (int)0x80010003u);
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("slot inspection error is reported",
              result.status == LauncherLoader::Probe_InspectionFailed &&
              result.code == (int)0x80010003u);
    }

    {
        FakeSys::Clear();
        FakeSys::SetInfoError(2, (int)0x80010003u);
        FakeSys::SetPlugin(4, "qDNSm", "/dev_hdd0/plugins/qDNSm.sprx");
        LauncherLoader::ProbeResult result = LauncherLoader::Probe();
        Check("loaded plugin wins over later inspection error",
              result.status == LauncherLoader::Probe_AlreadyLoaded && result.slot == 4);
    }
}

static void LoadTests()
{
    printf("\n-- load --\n");

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult(0);
        LauncherLoader::LoadResult result = LauncherLoader::Load(2, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("success",
              result.status == LauncherLoader::Load_Ok &&
              FakeSys::LoadCalls() == 1 &&
              FakeSys::LoadSlot() == 2 &&
              std::string(FakeSys::LoadPath()) == "/dev_hdd0/plugins/qDNSm.sprx");
    }

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult((int)0x80010003u);
        LauncherLoader::LoadResult result = LauncherLoader::Load(1, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("ENOSYS", result.status == LauncherLoader::Load_Unavailable);
    }

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult((int)0x80010002u);
        LauncherLoader::LoadResult result = LauncherLoader::Load(1, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("EINVAL", result.status == LauncherLoader::Load_Rejected);
    }

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult((int)0x80011324u);
        LauncherLoader::LoadResult result = LauncherLoader::Load(1, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("PRX invalid", result.status == LauncherLoader::Load_Rejected);
    }

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult((int)0x80010006u);
        LauncherLoader::LoadResult result = LauncherLoader::Load(1, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("ENOENT", result.status == LauncherLoader::Load_Missing);
    }

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult((int)0x80010011u);
        LauncherLoader::LoadResult result = LauncherLoader::Load(1, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("EKRESOURCE", result.status == LauncherLoader::Load_NoFreeSlot);
    }

    {
        FakeSys::Clear();
        FakeSys::SetLoadResult((int)0xDEADBEEFu);
        LauncherLoader::LoadResult result = LauncherLoader::Load(1, "/dev_hdd0/plugins/qDNSm.sprx");
        Check("unknown error", result.status == LauncherLoader::Load_Failed &&
              result.code == (int)0xDEADBEEFu);
    }
}

static void FileTests()
{
    printf("\n-- files --\n");

    const Small::Str sourceDir = Path("/pkg/");
    const Small::Str plugin = Path("/dev_hdd0/plugins/qDNSm.sprx");
    const Small::Str gameList = Path("/dev_hdd0/plugins/qDNSm-server-games.ini");
    const std::string payload(4096, 'P');

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm.sprx", payload);
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("missing plugin is copied",
              status == LauncherFiles::Ok && copied &&
              FakeFs::Get(plugin.c()) == payload);
    }

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm.sprx", payload);
        FakeFs::Put(plugin.c(), "existing");
        const int writes = FakeFs::g_WriteCount;
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("existing plugin is not overwritten",
              status == LauncherFiles::Ok && !copied &&
              FakeFs::Get(plugin.c()) == "existing" &&
              FakeFs::g_WriteCount == writes);
    }

    {
        FakeFs::Clear();
        FakeFs::Put(plugin.c(), "existing");
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("existing destination does not require a source",
              status == LauncherFiles::Ok && !copied &&
              FakeFs::Get(plugin.c()) == "existing");
    }

    {
        FakeFs::Clear();
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("missing plugin source", status == LauncherFiles::SourceMissing && !copied);
    }

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm.sprx", std::string(10, 'x'));
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("small plugin source", status == LauncherFiles::SourceTooSmall && !copied);
    }

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm.sprx", std::string(Small::kMaxFile + 1, 'x'));
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("large plugin source", status == LauncherFiles::SourceTooLarge && !copied);
    }

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm.sprx", payload);
        FakeFs::g_FailWrites = true;
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsurePlugin(sourceDir, plugin, copied);
        Check("write failure leaves no destination",
              status == LauncherFiles::WriteFailed && !copied &&
              !FakeFs::Exists(plugin.c()));
    }

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm-server-games.ini", "x");
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsureGameList(sourceDir, gameList, copied);
        Check("missing game list is copied",
              status == LauncherFiles::Ok && copied &&
              FakeFs::Get(gameList.c()) == "x");
    }

    {
        FakeFs::Clear();
        FakeFs::Put("/pkg/qDNSm-server-games.ini", "");
        bool copied = false;
        LauncherFiles::Status status = LauncherFiles::EnsureGameList(sourceDir, gameList, copied);
        Check("empty game list source", status == LauncherFiles::SourceTooSmall && !copied);
    }
}

int main()
{
    ProbeTests();
    LoadTests();
    FileTests();

    printf("\n%d passed, %d failed\n", g_Pass, g_Fail);
    return g_Fail == 0 ? 0 : 1;
}
