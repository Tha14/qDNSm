#pragma once

/*
 * Filesystem access for the qDNSm Installer EBOOT.
 *
 * Deliberately NOT the qDNSm plugin's Utils/FileSystem.cpp. That file depends
 * on the VirtualShell layer:
 *
 *     vsh::memset   in GetModuleInfo
 *     vsh::stat / vsh::fopen / vsh::fread / vsh::malloc
 *                  in ReadFile(), which its own header notes has "never once
 *                  been observed to succeed"
 *
 * Those symbols resolve against the VSH process export table. An EBOOT is
 * hosted by the game loader, so they must not be linked here. Everything below
 * is plain libc plus cellFs, which works from any process.
 *
 * The one behavioural difference that matters: every write is read back and
 * compared byte for byte. qDNSm's SaveFile() ignores errors entirely and
 * returns void. An installer that reports success without checking is worse
 * than no installer, because the user is left with a half-installed plugin and
 * no idea anything went wrong.
 *
 * No std:: anywhere in this header or its implementation. See Small.hpp for why:
 * an EBOOT cannot link the C++ allocator, so every buffer here is fixed-size
 * and caller-owned. Byte buffers arrive as a Small::Bytes view over storage the
 * caller supplies and guarantees outlives the call - the payload is 1 MB and
 * cannot live on the stack.
 */

#include <stdint.h>

#include "Small.hpp"

namespace InstFs
{
    /*! Directory containing the running EBOOT, with a trailing slash.
     *  Usually /dev_hdd0/game/NPUB72381/USRDIR/. The payload is read from here. */
    bool SelfDirectory(Small::Str& out);
    bool SelfDirectoryFor(const char* titleId, Small::Str& out);

    bool FileExists(const Small::Str& path);
    bool DirectoryExists(const Small::Str& path);

    /*! Size in bytes, or -1 if the file cannot be stat'd. */
    int64_t FileSize(const Small::Str& path);

    /*! Whole-file binary read into caller-supplied storage.
     *
     *  Fails unless the byte count read matches the size reported by stat, so a
     *  truncated read is an error rather than a short buffer. Also fails if the
     *  file is larger than the buffer's capacity - reported, never truncated,
     *  because installing a partial plugin is exactly the failure this installer
     *  must not have. */
    bool ReadBinary(const Small::Str& path, Small::Bytes& out);

    /*! Whole-file text read. BootPlugins.txt is small; anything that does not
     *  fit is treated as unreadable so it is left alone rather than rewritten. */
    bool ReadText(const Small::Str& path, Small::Bytes& out);

    /*! Write, then read back and compare byte for byte. Returns true only if
     *  the file on disk now matches the buffer exactly. */
    bool WriteBinaryVerified(const Small::Str& path, const uint8_t* data, size_t size);

    /*! Write text and verify. Not a separate operation - just a named spelling
     *  of the above, so call sites say what they mean. */
    inline bool WriteTextVerified(const Small::Str& path, const char* text, size_t size)
    {
        return WriteBinaryVerified(path, (const uint8_t*)text, size);
    }

    /*! Replace an existing file without ever leaving it half-written.
     *
     *  Writes path.new first and verifies it, backs up any existing file to
     *  path.bak, then overwrites and verifies. On failure the previous contents
     *  are restored from memory, and the backup is deliberately left on disk.
     *
     *  cellFsRename is not available, so there is no atomic replace; the backup
     *  is what makes a partial write recoverable. If it fails to restore, the
     *  backup path is where the user's old data still is.
     *
     *  The in-memory copy used for the restore is a file-scope buffer, not the
     *  caller's, because only this function needs it and it is megabytes. */
    bool WriteBinaryStaged(const Small::Str& path, const uint8_t* data, size_t size);

    /*! True if the file was removed, or was already absent. */
    bool DeleteFile(const Small::Str& path);

    /*! Create the directory and any missing parents. */
    bool EnsureDirectory(const Small::Str& path);
}
