#pragma once

/*
 * Reads and rewrites /dev_hdd0/plugins/boot_plugins.txt.
 *
 * This is the highest-risk code in the installer. boot_plugins.txt is shared
 * with every other homebrew plugin on the console. Losing a user's webMAN or
 * Irisman entry because they used a DNS installer would be a genuinely bad
 * outcome, so the rules below are not stylistic preferences:
 *
 *   1. Other plugins are never edited: every non-blank line that is not ours
 *      survives byte for byte, including comments, indentation and unusual line
 *      endings. Blank or whitespace-only lines are dropped on every rewrite,
 *      install and uninstall alike.
 *   2. Registering is idempotent and de-duplicating: any pre-existing qDNSm
 *      lines are removed and exactly one is inserted at the top, so running the
 *      installer twice does not produce two entries and a user who previously
 *      installed by hand at a custom path gets moved to the standard one. The
 *      top position is deliberate: webMAN MOD patches memory while it starts,
 *      and a VSH plugin listed after it can hang before module_start runs.
 *   3. A file that exists but cannot be read is never rewritten. "Could not
 *      read" must not become "silently replaced with a version that lost the
 *      plugins we could not see".
 *   4. Writes are staged and verified before the real file is touched, and the
 *      previous contents are kept alongside on failure so the user can recover
 *      by hand.
 *
 * No std:: - see Small.hpp for why an EBOOT cannot link the C++ allocator.
 * Lines are ranges into one static buffer rather than allocated strings, and
 * that buffer is reused across calls: only one file is being edited at a time.
 */

#include "Small.hpp"

namespace BootPlugins
{
    enum Result
    {
        Ok = 0,
        Unreadable,      /* the file exists but could not be read - not rewritten */
        TooLarge,        /* larger than the fixed buffer, so it is left alone */
        StageFailed,     /* could not write the staged copy */
        BackupFailed,    /* could not write the backup */
        WriteFailed      /* the real file could not be written */
    };

    struct State
    {
        bool exists;
        bool readable;
        int  qdnsLines;      /* how many lines reference qDNSm.sprx */
        bool usesCrlf;
    };

    /*! Inspect without modifying. Never fails hard: an unreadable file reports
     *  readable == false and exists == true, and the caller must not proceed. */
    void Inspect(const Small::Str& path, State& state);

    /*! Ensure exactly one qDNSm line exists, pointing at sprxPath. */
    Result Register(const Small::Str& path, const Small::Str& sprxPath);

    /*! Remove every qDNSm line, leaving everything else untouched. */
    Result Unregister(const Small::Str& path);

    /*! Human-readable form of a Result, for dialogs. */
    const char* Describe(Result result);
}
