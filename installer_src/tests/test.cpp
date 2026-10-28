// Host-side tests for the boot_plugins.txt editor. The real BootPlugins.cpp is
// compiled unmodified against an in-memory filesystem, so this exercises the
// code that actually ships - not a reimplementation of it.
#include <cstdio>
#include <string>
#include <vector>

#include "BootPlugins.hpp"
#include "InstallerFs.hpp"

namespace FakeFs
{
    void Put(const std::string& path, const std::string& text);
    std::string Get(const std::string& path);
    bool Exists(const std::string& path);
    void Clear();

    extern bool g_FailWrites;
    extern bool g_FailBackup;
    extern bool g_FailStaged;
    extern bool g_CorruptNextWrite;
    extern bool g_TruncateNextWrite;
    extern int g_WriteCount;
    extern std::vector<std::string> g_WriteLog;
}

static const char* kBoot = "/dev_hdd0/plugins/boot_plugins.txt";
static const char* kSprx = "/dev_hdd0/plugins/qDNSm.sprx";

static int g_Pass = 0;
static int g_Fail = 0;

static void ShowEscaped(const std::string& s)
{
    for (size_t i = 0; i < s.size(); i++)
    {
        if (s[i] == '\n')
            printf("\\n");
        else if (s[i] == '\r')
            printf("\\r");
        else
            putchar(s[i]);
    }
}

static void Check(const char* name, const std::string& got, const std::string& want)
{
    if (got == want)
    {
        g_Pass++;
        printf("  ok    %s\n", name);
        return;
    }

    g_Fail++;
    printf("  FAIL  %s\n", name);
    printf("          expected: ");
    ShowEscaped(want);
    printf("\n          actual  : ");
    ShowEscaped(got);
    printf("\n");
}

static void CheckBool(const char* name, bool got, bool want)
{
    if (got == want) { g_Pass++; printf("  ok    %s\n", name); return; }
    g_Fail++;
    printf("  FAIL  %s (expected %s)\n", name, want ? "true" : "false");
}

int main()
{
    printf("\n=== boot_plugins.txt editor ===\n\n");

    printf("-- register --\n");
    {
        FakeFs::Clear();
        BootPlugins::Result r = BootPlugins::Register(kBoot, kSprx);
        CheckBool("missing file: result Ok", r == BootPlugins::Ok, true);
        Check("missing file: created with one bare line",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("LF: other plugin preserved, final line bare",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\r\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("CRLF: separator matched, final line bare",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\r\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "# my plugins\n"
                    "\n"
                    "  /dev_hdd0/plugins/webMAN.sprx  \n"
                    "/dev_hdd0/plugins/Irisman.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("comments and padding preserved, blank line removed",
              FakeFs::Get(kBoot),
              "# my plugins\n"
              "  /dev_hdd0/plugins/webMAN.sprx  \n"
              "/dev_hdd0/plugins/Irisman.sprx\n"
              "/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("already registered: still exactly one entry",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "/dev_hdd0/plugins/qDNSm.sprx\n"
                    "/dev_hdd0/plugins/webMAN.sprx\n"
                    "  /dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("duplicates collapsed, indented copy matched by basename",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx");
        BootPlugins::Register(kBoot, kSprx);
        Check("no trailing newline: separator inserted, lines not concatenated",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_usb000/somewhere/qDNSm.sprx\n/dev_hdd0/plugins/webMAN.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("custom-path entry moved to the standard path",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\n\n/dev_hdd0/plugins/Irisman.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("blank lines between plugins removed",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/Irisman.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "\n"
                    "  \n"
                    "/dev_hdd0/plugins/webMAN.sprx\n"
                    "\t\n"
                    "\n"
                    "/dev_hdd0/plugins/Irisman.sprx\n"
                    "\n"
                    "   \n");
        BootPlugins::Register(kBoot, kSprx);
        Check("install removes leading, middle and trailing blank lines",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n"
              "/dev_hdd0/plugins/Irisman.sprx\n"
              "/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\r\n\r\n\t\r\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("CRLF blank lines removed, separator kept, final line bare",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\r\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "/dev_hdd0/plugins/qDNSm.sprx\n"
                    "\n"
                    "/dev_hdd0/plugins/webMAN.sprx\n"
                    "\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("old entry and blanks removed, one entry appended at the end",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/qDNSm.sprx\n\n\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("reinstall with trailing blanks leaves one bare line",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/qDNSm.sprx");
    }

    printf("\n-- unregister --\n");
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "/dev_hdd0/plugins/webMAN.sprx\n"
                    "/dev_hdd0/plugins/qDNSm.sprx\n"
                    "/dev_hdd0/plugins/Irisman.sprx\n");
        BootPlugins::Result r = BootPlugins::Unregister(kBoot);
        CheckBool("result Ok", r == BootPlugins::Ok, true);
        Check("only our line removed, order of the rest kept, no trailing newline",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/Irisman.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "/dev_hdd0/plugins/webMAN.sprx\r\n"
                    "/dev_hdd0/plugins/qDNSm.sprx\r\n");
        BootPlugins::Unregister(kBoot);
        Check("CRLF kept line written without its terminator",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::Unregister(kBoot);
        Check("sole entry: file left present and empty",
              FakeFs::Get(kBoot), "");
        CheckBool("sole entry: file not deleted", FakeFs::Exists(kBoot), true);
    }
    {
        FakeFs::Clear();
        BootPlugins::Result r = BootPlugins::Unregister(kBoot);
        CheckBool("missing file: Ok", r == BootPlugins::Ok, true);
        CheckBool("missing file: nothing created", FakeFs::Exists(kBoot), false);
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "# keep me\n"
                    "   /dev_hdd0/plugins/qDNSm.sprx   \n"
                    "/dev_usb000/qDNSm.sprx\n"
                    "/dev_hdd0/plugins/webMAN.sprx\n");
        BootPlugins::Unregister(kBoot);
        Check("indented and alternate-path entries both removed",
              FakeFs::Get(kBoot),
              "# keep me\n/dev_hdd0/plugins/webMAN.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\n   /dev_hdd0/plugins/qDNSm.sprx");
        BootPlugins::Unregister(kBoot);
        Check("removal takes the kept last line's terminator too",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/webMAN.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/qDNSm2.sprx\n/dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::Unregister(kBoot);
        Check("similarly named plugin NOT removed",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/qDNSm2.sprx");
    }

    printf("\n-- unregister: blank lines --\n");
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "/dev_hdd0/plugins/webMAN.sprx\n"
                    "\n"
                    "\n"
                    "/dev_hdd0/plugins/qDNSm.sprx\n"
                    "/dev_hdd0/plugins/Irisman.sprx\n");
        BootPlugins::Unregister(kBoot);
        Check("blank lines around our entry removed",
              FakeFs::Get(kBoot),
              "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/Irisman.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx\n\n");
        BootPlugins::Unregister(kBoot);
        Check("trailing blank removed, bottom bare",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/webMAN.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "/dev_hdd0/plugins/webMAN.sprx\n"
                    "   \n"
                    "\t\n"
                    "/dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::Unregister(kBoot);
        Check("whitespace-only lines count as empty",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/webMAN.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\r\n\r\n/dev_hdd0/plugins/qDNSm.sprx\r\n");
        BootPlugins::Unregister(kBoot);
        Check("CRLF blank removed, CRLF kept line bare",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/webMAN.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/qDNSm.sprx\n\n");
        BootPlugins::Unregister(kBoot);
        Check("ours and only blanks: left present and empty",
              FakeFs::Get(kBoot), "");
        CheckBool("ours and only blanks: file not deleted",
                  FakeFs::Exists(kBoot), true);
    }
    {
        /* Not-installed must stay a no-op, blank lines and all. */
        FakeFs::Clear();
        const std::string untouched = "/dev_hdd0/plugins/webMAN.sprx\n\n/dev_hdd0/plugins/Irisman.sprx\n";
        FakeFs::Put(kBoot, untouched);
        BootPlugins::Result r = BootPlugins::Unregister(kBoot);
        CheckBool("not-installed uninstall: Ok", r == BootPlugins::Ok, true);
        Check("not-installed uninstall: file byte for byte unchanged",
              FakeFs::Get(kBoot), untouched);
    }
    {
        /* The comment line survives the blank sweep: it is not empty. */
        FakeFs::Clear();
        FakeFs::Put(kBoot,
                    "# webMAN\n"
                    "\n"
                    "/dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::Unregister(kBoot);
        Check("comment line kept, blank line gone",
              FakeFs::Get(kBoot), "# webMAN");
    }

    printf("\n-- inspect --\n");
    {
        FakeFs::Clear();
        BootPlugins::State s;
        BootPlugins::Inspect(kBoot, s);
        CheckBool("missing: exists false", s.exists, false);
        CheckBool("missing: qdnsLines 0", s.qdnsLines == 0, true);
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/qDNSm.sprx\n/dev_hdd0/plugins/qDNSm.sprx\n");
        BootPlugins::State s;
        BootPlugins::Inspect(kBoot, s);
        CheckBool("present: exists true", s.exists, true);
        CheckBool("present: readable true", s.readable, true);
        CheckBool("present: counts 2", s.qdnsLines == 2, true);
        CheckBool("present: detects LF", s.usesCrlf == false, true);
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/qDNSm.sprx\r\n");
        BootPlugins::State s;
        BootPlugins::Inspect(kBoot, s);
        CheckBool("present: detects CRLF", s.usesCrlf == true, true);
    }

    printf("\n-- empty and zero-length files --\n");
    {
        /* Regression: an existing but empty boot_plugins.txt used to be reported
         * unreadable, which made the installer refuse to touch the console. */
        FakeFs::Clear();
        FakeFs::Put(kBoot, "");
        BootPlugins::State s;
        BootPlugins::Inspect(kBoot, s);
        CheckBool("empty file: exists true", s.exists, true);
        CheckBool("empty file: readable true", s.readable, true);
        CheckBool("empty file: qdnsLines 0", s.qdnsLines == 0, true);
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "");
        BootPlugins::Result r = BootPlugins::Register(kBoot, kSprx);
        CheckBool("register onto empty file: Ok", r == BootPlugins::Ok, true);
        Check("register onto empty file: just our bare line",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/qDNSm.sprx");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "");
        BootPlugins::Result r = BootPlugins::Unregister(kBoot);
        CheckBool("unregister empty file: Ok", r == BootPlugins::Ok, true);
        CheckBool("unregister empty file: still present", FakeFs::Exists(kBoot), true);
        Check("unregister empty file: still empty", FakeFs::Get(kBoot), "");
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "\n\n");
        BootPlugins::Register(kBoot, kSprx);
        Check("file of blank lines: cleaned to just our bare line",
              FakeFs::Get(kBoot), "/dev_hdd0/plugins/qDNSm.sprx");
    }

    printf("\n-- HEN updater flag --\n");
    {
        FakeFs::Clear();
        const Small::Str path("/dev_hdd0/hen_updater.off");
        CheckBool("missing flag: created", InstFs::EnsureEmptyFile(path), true);
        CheckBool("missing flag: exists", FakeFs::Exists(path.c()), true);
        Check("missing flag: empty", FakeFs::Get(path.c()), "");
    }
    {
        FakeFs::Clear();
        const Small::Str path("/dev_hdd0/hen_updater.off");
        FakeFs::Put(path.c(), "keep me");
        CheckBool("existing flag: reports success", InstFs::EnsureEmptyFile(path), true);
        Check("existing flag: contents untouched", FakeFs::Get(path.c()), "keep me");
    }
    {
        FakeFs::Clear();
        const Small::Str path("/dev_hdd0/hen_updater.off");
        FakeFs::g_FailWrites = true;
        CheckBool("write failure: reported", InstFs::EnsureEmptyFile(path), false);
        FakeFs::g_FailWrites = false;
        CheckBool("write failure: file not created", FakeFs::Exists(path.c()), false);
    }

    printf("\n-- failure handling --\n");
    {
        FakeFs::Clear();
        const std::string original = "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/qDNSm.sprx\n";
        FakeFs::Put(kBoot, original);

        FakeFs::g_FailWrites = true;
        BootPlugins::Result r = BootPlugins::Register(kBoot, kSprx);
        FakeFs::g_FailWrites = false;

        CheckBool("staged write fails: reported, not silently Ok", r != BootPlugins::Ok, true);
        Check("staged write fails: original untouched", FakeFs::Get(kBoot), original);
    }
    {
        FakeFs::Clear();
        const std::string original = "/dev_hdd0/plugins/webMAN.sprx\n";
        FakeFs::Put(kBoot, original);

        FakeFs::g_FailBackup = true;
        BootPlugins::Result r = BootPlugins::Register(kBoot, kSprx);
        FakeFs::g_FailBackup = false;

        CheckBool("backup fails: reported", r == BootPlugins::BackupFailed, true);
        Check("backup fails: original untouched", FakeFs::Get(kBoot), original);
    }
    {
        /* Corrupt exactly the final real write: the backup restore must put the
         * user's plugins back. */
        FakeFs::Clear();
        const std::string original = "/dev_hdd0/plugins/webMAN.sprx\n/dev_hdd0/plugins/Irisman.sprx\n";
        FakeFs::Put(kBoot, original);

        FakeFs::g_WriteCount = 0;
        FakeFs::g_CorruptNextWrite = true;
        BootPlugins::Result r = BootPlugins::Register(kBoot, kSprx);
        FakeFs::g_CorruptNextWrite = false;

        CheckBool("final write does not verify: reported", r != BootPlugins::Ok, true);
        Check("final write does not verify: plugins restored",
              FakeFs::Get(kBoot), original);
    }
    {
        FakeFs::Clear();
        const std::string original = "/dev_hdd0/plugins/webMAN.sprx\n";
        FakeFs::Put(kBoot, original);

        FakeFs::g_TruncateNextWrite = true;
        BootPlugins::Result r = BootPlugins::Register(kBoot, kSprx);
        FakeFs::g_TruncateNextWrite = false;

        CheckBool("truncated final write: reported", r != BootPlugins::Ok, true);
        Check("truncated final write: plugins restored", FakeFs::Get(kBoot), original);
    }
    {
        FakeFs::Clear();
        FakeFs::Put(kBoot, "/dev_hdd0/plugins/webMAN.sprx\n");
        BootPlugins::Register(kBoot, kSprx);
        CheckBool("no scratch files left behind after success",
                  FakeFs::Exists(std::string(kBoot) + ".qdnsm-new") ||
                  FakeFs::Exists(std::string(kBoot) + ".qdnsm-bak"), false);
    }

    printf("\n%d passed, %d failed\n\n", g_Pass, g_Fail);
    return g_Fail == 0 ? 0 : 1;
}
