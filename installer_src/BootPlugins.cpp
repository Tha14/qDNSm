#include "BootPlugins.hpp"
#include "InstallerFs.hpp"

#include <string.h>

namespace BootPlugins
{
namespace
{
    const char* kTargetBaseName = "qDNSm.sprx";
    const char* kStagedSuffix = ".qdnsm-new";
    const char* kBackupSuffix = ".qdnsm-bak";

    /* Static working set. One file is edited at a time and the installer is
     * single-threaded, so these are reused rather than allocated. They are file
     * scope rather than function scope because kMaxText is 64 KB - two of those
     * on the stack would be an overflow. */
    static char           g_text[Small::kMaxText];
    static char           g_updated[Small::kMaxText];
    static Small::Bytes   g_textView;
    static Small::LineRef g_lines[Small::kMaxLines];
    static Small::LineRef g_kept[Small::kMaxLines];

    static void BindText()
    {
        g_textView.Attach((uint8_t*)g_text, sizeof(g_text));
    }

    bool IsSpace(char c)
    {
        return c == ' ' || c == '\t' || c == '\r';
    }

    /*! Split g_text into lines, keeping each line's exact terminator as its own
     *  range.
     *
     *  Storing the terminator per line rather than re-joining everything with one
     *  detected style is what makes "every other line survives byte for byte"
     *  true even for a file that has a stray LF in a sea of CRLF.
     *
     *  Returns false if the file has more lines than the fixed array holds. The
     *  caller must treat that as unreadable and leave the file alone - keeping
     *  only the first kMaxLines would delete the user's plugins. */
    bool SplitLines(size_t len, size_t& count, bool& usesCrlf)
    {
        count = 0;
        usesCrlf = false;

        /* CRLF detection looks only at the first newline, matching the behaviour
         * the tests were written against. */
        for (size_t i = 0; i < len; i++)
        {
            if (g_text[i] == '\n')
            {
                if (i > 0 && g_text[i - 1] == '\r')
                    usesCrlf = true;
                break;
            }
        }

        size_t pos = 0;
        while (pos <= len)
        {
            size_t nl = Small::kNpos;
            for (size_t i = pos; i < len; i++)
            {
                if (g_text[i] == '\n')
                {
                    nl = i;
                    break;
                }
            }

            if (count >= Small::kMaxLines)
                return false;

            Small::LineRef& line = g_lines[count];

            if (nl == Small::kNpos)
            {
                /* Trailing fragment with no newline. Preserved as-is; it stays
                 * the final line after the qDNSm entry is inserted above it. */
                if (pos >= len)
                    break;

                line.start     = (uint32_t)pos;
                line.len       = (uint32_t)(len - pos);
                line.termStart = (uint32_t)len;
                line.termLen   = 0;
                count++;
                break;
            }

            size_t end = nl;
            if (end > pos && g_text[end - 1] == '\r')
                end--;

            line.start     = (uint32_t)pos;
            line.len       = (uint32_t)(end - pos);
            line.termStart = (uint32_t)end;
            line.termLen   = (uint32_t)(nl - end + 1);
            count++;

            pos = nl + 1;
        }

        return true;
    }

    /*! Append src to dst at *out. False if it would overflow g_updated, which
     *  the caller treats as a failure rather than writing a short file - a
     *  truncated boot_plugins.txt would drop the user's other plugins. */
    bool Emit(char* dst, size_t& out, const char* src, size_t len)
    {
        if (out + len > Small::kMaxText)
            return false;

        if (len > 0)
            memcpy(dst + out, src, len);

        out += len;
        return true;
    }

    /*! A line is ours when its final path component is exactly qDNSm.sprx.
     *
     *  Leading and trailing whitespace is tolerated because launchers and users
     *  both indent. Matching the basename rather than the whole path is
     *  deliberate: it catches an entry pointing anywhere - a custom directory, a
     *  removable drive - so those get cleaned up instead of left behind. */
    bool IsQdnsLine(const Small::LineRef& line)
    {
        size_t begin = line.start;
        size_t end   = line.start + line.len;

        while (begin < end && IsSpace(g_text[begin]))
            begin++;

        while (end > begin && IsSpace(g_text[end - 1]))
            end--;

        if (begin >= end)
            return false;

        /* Last path component. Backslash is accepted as a separator so a
         * hand-edited Windows-style path is cleaned up rather than left as a
         * dead reference. */
        size_t base = begin;
        for (size_t i = end; i > begin; i--)
        {
            if (g_text[i - 1] == '/' || g_text[i - 1] == '\\')
            {
                base = i;
                break;
            }
        }

        size_t nameLen = end - base;
        size_t wantLen = strlen(kTargetBaseName);

        return nameLen == wantLen && memcmp(g_text + base, kTargetBaseName, wantLen) == 0;
    }

    /*! Content is nothing but whitespace, an empty line included. */
    bool BlankLine(const Small::LineRef& line)
    {
        for (size_t i = line.start; i < line.start + line.len; i++)
        {
            if (!IsSpace(g_text[i]))
                return false;
        }

        return true;
    }

    int CountQdnsLines(const Small::LineRef* lines, size_t count)
    {
        int found = 0;
        for (size_t i = 0; i < count; i++)
        {
            if (IsQdnsLine(lines[i]))
                found++;
        }
        return found;
    }

    /*! Writes updated to a scratch file and verifies it, so that by the time the
     *  real boot_plugins.txt is opened for truncation we already know the bytes
     *  can be written and read back on this filesystem.
     *
     *  cellFsRename is not available, so there is no atomic replace. The backup
     *  is what makes a partial write recoverable. */
    Result StageAndSwap(const Small::Str& path,
                        const char* original, size_t originalLen,
                        bool originalExisted,
                        const char* updated, size_t updatedLen)
    {
        Small::Str staged;
        Small::Str backup;

        if (!staged.Set(path) || !staged.Append(kStagedSuffix))
            return StageFailed;

        if (!backup.Set(path) || !backup.Append(kBackupSuffix))
            return StageFailed;

        if (!InstFs::WriteTextVerified(staged, updated, updatedLen))
        {
            InstFs::DeleteFile(staged);
            return StageFailed;
        }

        if (originalExisted)
        {
            if (!InstFs::WriteTextVerified(backup, original, originalLen))
            {
                InstFs::DeleteFile(staged);
                return BackupFailed;
            }
        }

        if (!InstFs::WriteTextVerified(path, updated, updatedLen))
        {
            /* Put the user's plugins back. If even this fails the .bak on disk
             * still holds them, and the dialog tells the user where it is. */
            if (originalExisted)
                InstFs::WriteTextVerified(path, original, originalLen);

            InstFs::DeleteFile(staged);
            return WriteFailed;
        }

        InstFs::DeleteFile(staged);

        /* Only remove the backup once the real file is known good. */
        if (originalExisted)
            InstFs::DeleteFile(backup);

        return Ok;
    }

    /*! Shared body of Register and Unregister. lineToAppend == NULL means remove
     *  only; otherwise it is inserted at the top of the list. */
    Result Rewrite(const Small::Str& path, const char* lineToAppend)
    {
        State state;
        Inspect(path, state);

        /* Nothing to register into and nothing to unregister from. Creating an
         * empty boot_plugins.txt here would be a side effect on a console that
         * deliberately has none, and launchers do distinguish "missing" from
         * "present but empty". */
        if (!state.exists && lineToAppend == NULL)
            return Ok;

        if (state.exists && !state.readable)
            return Unreadable;

        BindText();

        size_t originalLen = 0;
        if (state.exists)
        {
            if (!InstFs::ReadText(path, g_textView))
                return Unreadable;

            originalLen = g_textView.size();
        }

        size_t count = 0;
        bool usesCrlf = state.usesCrlf;

        if (state.exists && !SplitLines(originalLen, count, usesCrlf))
            return TooLarge;

        /* Nothing of ours in the file: no side effects, leave it alone. */
        const bool removing = (lineToAppend == NULL);

        if (removing && CountQdnsLines(g_lines, count) == 0)
            return Ok;

        size_t keptCount = 0;
        for (size_t i = 0; i < count; i++)
        {
            if (IsQdnsLine(g_lines[i]))
                continue;

            if (BlankLine(g_lines[i]))
                continue;

            if (keptCount >= Small::kMaxLines)
                return TooLarge;

            g_kept[keptCount++] = g_lines[i];
        }

        const char* term    = usesCrlf ? "\r\n" : "\n";
        size_t      termLen = strlen(term);

        /* Rebuild into g_updated. qDNSm always goes first: webMAN MOD patches
         * memory while it starts, and VSH plugins listed after it can hang
         * before module_start runs. */
        size_t out = 0;

        if (lineToAppend != NULL)
        {
            if (!Emit(g_updated, out, lineToAppend, strlen(lineToAppend)))
                return TooLarge;

            if (keptCount > 0)
            {
                if (!Emit(g_updated, out, term, termLen))
                    return TooLarge;
            }
        }

        for (size_t i = 0; i < keptCount; i++)
        {
            const Small::LineRef& line = g_kept[i];

            if (!Emit(g_updated, out, g_text + line.start, line.len))
                return TooLarge;

            /* The final line is always left bare, install and uninstall alike. */
            const bool isFinal = (i + 1 == keptCount);

            if ((line.termLen > 0) && !isFinal)
            {
                if (!Emit(g_updated, out, g_text + line.termStart, line.termLen))
                    return TooLarge;
            }
        }

        return StageAndSwap(path, g_text, originalLen, state.exists, g_updated, out);
    }
}

void Inspect(const Small::Str& path, State& state)
{
    state.exists = false;
    state.readable = false;
    state.qdnsLines = 0;
    state.usesCrlf = false;

    if (!InstFs::FileExists(path))
        return;

    state.exists = true;

    BindText();

    if (!InstFs::ReadText(path, g_textView))
        return;

    state.readable = true;

    size_t count = 0;
    if (!SplitLines(g_textView.size(), count, state.usesCrlf))
        return;

    state.qdnsLines = CountQdnsLines(g_lines, count);
}

Result Register(const Small::Str& path, const Small::Str& sprxPath)
{
    return Rewrite(path, sprxPath.c());
}

Result Unregister(const Small::Str& path)
{
    return Rewrite(path, NULL);
}

const char* Describe(Result result)
{
    switch (result)
    {
        case Ok:
            return "Done.";
        case Unreadable:
            return "boot_plugins.txt could not be read, so it was left alone.";
        case TooLarge:
            return "boot_plugins.txt is larger than this installer can safely rewrite, so it was left alone.";
        case StageFailed:
            return "Could not write to /dev_hdd0/plugins.";
        case BackupFailed:
            return "Could not back up boot_plugins.txt. Nothing was changed.";
        case WriteFailed:
            return "Could not update boot_plugins.txt. The previous contents were restored from boot_plugins.txt.qdnsm-bak.";
    }
    return "Unknown error.";
}

}
