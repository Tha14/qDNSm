#include "InstallerFs.hpp"

#include "TargetCompat.hpp"
#include <string.h>

namespace
{
    /* cellFsOpen/Write/Read all report through return codes and out-params. */
    const int kFlagsRead = CELL_FS_O_RDONLY;
    const int kFlagsWrite = CELL_FS_O_WRONLY | CELL_FS_O_CREAT | CELL_FS_O_TRUNC;

    /* Scratch used only by WriteBinaryStaged to hold the previous contents for
     * rollback. File scope, not the caller's: it is a megabyte, so it cannot go
     * on the stack, and only one staged write is ever in flight. */
    static uint8_t g_rollback[Small::kMaxPayload];
    static Small::Bytes g_rollbackView;

    /* Storage for the read-back verification in WriteBinaryVerified.
     *
     * Never named directly: g_verifyView.Attach below takes its address, and
     * SNC warning 178 only fires for variables with no reference at all. */
    static uint8_t g_verifyBuf[Small::kMaxPayload];
    static Small::Bytes g_verifyView;

    bool WriteRaw(const Small::Str& path, const void* data, size_t size)
    {
        /* cellFsWrite still wants a valid pointer even for a zero-length write,
         * so hand it a real address rather than whatever the caller passed. */
        static const uint8_t kNothing = 0;
        if (data == NULL)
            data = &kNothing;

        int handle = 0;
        if (cellFsOpen(path.c(), kFlagsWrite, &handle, NULL, 0) != CELL_FS_SUCCEEDED)
            return false;

        /* chmod after the open, not before. On a file that does not exist yet -
         * the normal case for the install log on a first run - chmod reports
         * CELL_ENOENT, and when the directory is also missing the open fails and
         * there is no file to chmod at all. Once the open has succeeded the file
         * exists, and this is what lets a later run truncate a read-only leftover
         * from an install that was interrupted partway. */
        cellFsChmod(path.c(), 0777);

        uint64_t written = 0;
        CellFsErrno err = cellFsWrite(handle, data, (uint64_t)size, &written);
        cellFsClose(handle);

        if (err != CELL_FS_SUCCEEDED)
            return false;

        /* A short write is the classic symptom of a full HDD. Treat it as
         * failure: the file exists now but is truncated, and the user would be
         * told the install worked. */
        return written == (uint64_t)size;
    }
}

namespace InstFs
{
    bool SelfDirectory(Small::Str& out)
    {
        /*
         * Where this installer lives, known rather than asked for.
         *
         * The previous implementation asked the loader via sys_prx_get_module_info,
         * on the assumption that module id 0 meant "self". That is wrong: the call
         * comes back CELL_PRX_ERROR_UNKNOWN_MODULE, so the installer could not find
         * its own payload and bailed out with status 1.
         *
         * An EBOOT installed from a PKG always lands in the same place, and the
         * title id is fixed at NPUB72381, so there is nothing to discover. Asking
         * the loader also needed a workaround for a real bug in the PSL1GHT headers,
         * where sys/prx.h and lv2/prx.h have circular include guards and whichever
         * order is used one side loses its declarations; see TargetCompat.hpp.
         */
        out.Clear();
        return out.Set("/dev_hdd0/game/NPUB72381/USRDIR/");
    }

    bool FileExists(const Small::Str& path)
    {
        CellFsStat stat;
        if (cellFsStat(path.c(), &stat) != CELL_FS_SUCCEEDED)
            return false;
        return CELL_FS_S_ISREG(stat.st_mode) != 0;
    }

    bool DirectoryExists(const Small::Str& path)
    {
        CellFsStat stat;
        if (cellFsStat(path.c(), &stat) != CELL_FS_SUCCEEDED)
            return false;
        return CELL_FS_S_ISDIR(stat.st_mode) != 0;
    }

    int64_t FileSize(const Small::Str& path)
    {
        CellFsStat stat;
        if (cellFsStat(path.c(), &stat) != CELL_FS_SUCCEEDED)
            return -1;
        return (int64_t)stat.st_size;
    }

    bool ReadBinary(const Small::Str& path, Small::Bytes& out)
    {
        CellFsStat stat;
        if (cellFsStat(path.c(), &stat) != CELL_FS_SUCCEEDED)
            return false;

        /* No st_size < 0 test here. CellFsStat::st_size is unsigned, so that
         * comparison is always false - SNC warning 187, "pointless comparison of
         * unsigned integer with zero". It was left over from when this read was
         * expected to fail on an empty file, which it no longer should. */

        /* A file larger than the caller's buffer is reported as unreadable, never
         * partially loaded. Truncating here would mean installing half a plugin
         * and reporting success. */
        if ((uint64_t)stat.st_size > (uint64_t)out.capacity())
        {
            out.Clear();
            return false;
        }

        /* A zero-length file is a successful read of zero bytes, not an error.
         *
         * This matters: boot_plugins.txt is legitimately empty on a console whose
         * last plugin was just uninstalled, and an earlier version of this
         * function reported such a file as unreadable. BootPlugins::Inspect then
         * set readable = false, and Installer refused to touch the file at all -
         * so a console in that state could never be repaired by the installer.
         *
         * Callers that need non-empty content, such as the payload load in
         * Installer.cpp, check the size themselves. */
        if (stat.st_size == 0)
        {
            out.Clear();
            return true;
        }

        int handle = 0;
        if (cellFsOpen(path.c(), kFlagsRead, &handle, NULL, 0) != CELL_FS_SUCCEEDED)
            return false;

        uint64_t read = 0;
        CellFsErrno err = cellFsRead(handle, out.data(), (uint64_t)stat.st_size, &read);
        cellFsClose(handle);

        if (err != CELL_FS_SUCCEEDED)
        {
            out.Clear();
            return false;
        }

        /* Short read means the file changed under us or the medium lied. Either
         * way the buffer is not the file, so refuse it rather than installing a
         * truncated plugin. */
        if (read != (uint64_t)stat.st_size)
        {
            out.Clear();
            return false;
        }

        /* The bytes are already in the buffer; all that remains is to publish how
         * many of them are valid. Publish re-checks the capacity, so a length
         * that somehow exceeded it cannot become a valid shorter buffer. */
        return out.Publish((size_t)stat.st_size);
    }

    bool ReadText(const Small::Str& path, Small::Bytes& out)
    {
        return ReadBinary(path, out);
    }

    bool WriteBinaryVerified(const Small::Str& path, const uint8_t* data, size_t size)
    {
        if (!WriteRaw(path, data, size))
            return false;

        /* Verify by reading back. cellFs reports success on operations that
         * quietly did less than asked, so a return code alone is not evidence
         * that the bytes landed.
         *
         * The buffer is bound here rather than at file scope so that it is
         * genuinely the same storage g_verifyView refers to. */
        g_verifyView.Attach(g_verifyBuf, sizeof(g_verifyBuf));

        if (g_verifyView.capacity() < size)
            return false;

        if (!ReadBinary(path, g_verifyView))
            return false;

        if (g_verifyView.size() != size)
            return false;

        /* Zero-length is a legitimate result - it is what boot_plugins.txt
         * becomes once the last entry is removed. Sizes already match. */
        if (size == 0)
            return true;

        return g_verifyView.Equals(data, size);
    }

    bool WriteBinaryStaged(const Small::Str& path, const uint8_t* data, size_t size)
    {
        Small::Str staged;
        Small::Str backup;

        if (!staged.Set(path) || !staged.Append(".qdnsm-new"))
            return false;

        if (!backup.Set(path) || !backup.Append(".qdnsm-bak"))
            return false;

        /* 1. Prove the new bytes are writable on this filesystem before the
         *    existing file is opened for truncation. */
        if (!WriteBinaryVerified(staged, data, size))
        {
            DeleteFile(staged);
            return false;
        }

        /* 2. Preserve whatever is there now, if anything. */
        bool hadOriginal = FileExists(path);
        g_rollbackView.Attach(g_rollback, sizeof(g_rollback));

        if (hadOriginal)
        {
            if (!ReadBinary(path, g_rollbackView))
            {
                DeleteFile(staged);
                return false;
            }

            if (!WriteBinaryVerified(backup, g_rollbackView.data(), g_rollbackView.size()))
            {
                DeleteFile(staged);
                return false;
            }
        }

        /* 3. Overwrite. */
        if (!WriteBinaryVerified(path, data, size))
        {
            if (hadOriginal && g_rollbackView.size() > 0)
                WriteBinaryVerified(path, g_rollbackView.data(), g_rollbackView.size());

            DeleteFile(staged);

            /* Backup intentionally left behind: if the restore above also
             * failed, that file is the user's only copy of what was there. */
            return false;
        }

        DeleteFile(staged);
        if (hadOriginal)
            DeleteFile(backup);

        return true;
    }

    bool DeleteFile(const Small::Str& path)
    {
        if (!FileExists(path))
            return true;

        return cellFsUnlink(path.c()) == CELL_FS_SUCCEEDED;
    }

    bool EnsureDirectory(const Small::Str& path)
    {
        if (DirectoryExists(path))
            return true;

        /* Walk down to the root collecting each level, then create root-first.
         * cellFsMkdir does not create parents. */
        Small::Str current;
        if (!current.Set(path))
            return false;

        while (current.size() > 1 && current.at(current.size() - 1) == '/')
        {
            if (!current.Truncate(current.size() - 1))
                return false;
        }

        if (current.size() == 0 || current.at(0) != '/')
            return false;

        size_t pos = 1;
        for (;;)
        {
            size_t slash = Small::kNpos;
            for (size_t i = pos; i < current.size(); i++)
            {
                if (current.at(i) == '/')
                {
                    slash = i;
                    break;
                }
            }

            if (slash == Small::kNpos)
                break;

            Small::Str level;
            if (!current.CopyRange(current, 0, slash, level))
                return false;

            if (!DirectoryExists(level))
            {
                if (cellFsMkdir(level.c(), CELL_FS_S_IFDIR | 0777) != CELL_FS_SUCCEEDED)
                    return false;
            }

            pos = slash + 1;
        }

        if (!DirectoryExists(current))
        {
            if (cellFsMkdir(current.c(), CELL_FS_S_IFDIR | 0777) != CELL_FS_SUCCEEDED)
                return false;
        }

        return true;
    }
}
