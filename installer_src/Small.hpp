#pragma once

/*
 * Fixed-capacity, non-allocating types for the qDNSm Installer.
 *
 * ---------------------------------------------------------------------------
 * Why these exist instead of std::string and std::vector
 * ---------------------------------------------------------------------------
 * The installer originally used std:: throughout. That fails to link as an EBOOT:
 *
 *   undefined reference to `_ZdlPv' == "operator delete(void*)"
 *   undefined reference to `_Znwjj' == "operator new(unsigned int, unsigned int)"
 *
 * qDNSm gets away with this because it links -lallocator_export_stub, which
 * supplies the C++ allocator. That is a VSH module stub: it only resolves inside
 * the VSH process. An EBOOT is hosted by the PS3 game loader, so it has to link a
 * real C++ runtime instead, and this SDK does not ship one for PPU:
 *
 *   L0032: could not open file "C:/usr/local/cell\target\ppu\lib\libc++.a"
 *
 * So the C++ standard library is simply unavailable here, and the installer is
 * written against plain arrays instead.
 *
 * The installer's actual workload makes this a fair trade rather than a
 * workaround that costs anything real:
 *
 *   - paths           - a handful of short strings, under 100 chars each
 *   - qDNSm.sprx      - ~45 KB, and 1 MB is generous headroom
 *   - boot_plugins.txt- a few KB in practice
 *   - line count      - tens, not thousands
 *
 * The capacities below are sized with margin. Every one of them is checked at
 * runtime: an operation that would overflow returns false rather than silently
 * truncating, because a silently truncated path or a truncated plugin file is
 * exactly the failure mode this installer must never have.
 *
 * ---------------------------------------------------------------------------
 * Cost
 * ---------------------------------------------------------------------------
 * Buffers are caller-owned and must outlive the view over them. The large ones
 * are static or file-scope, never on the stack - a 1 MB stack object would be a
 * stack overflow on its own. Bytes holds no storage, only a pointer.
 */

#include <stdint.h>
#include <stddef.h>
#include <string.h>

namespace Small
{
    /* Capacities. Paths leave room for a 9-character title ID, the longest
     * directory depth the PS3 filesystem allows, and the staged/backup suffixes
     * that get appended to them. */
    const size_t kMaxPath      = 512;
    const size_t kMaxText      = 64 * 1024;    /* boot_plugins.txt */
    const size_t kMaxPayload   = 1024 * 1024;  /* qDNSm.sprx, ~45 KB in practice */
    const size_t kMaxLines     = 512;

    const size_t kNpos = (size_t)-1;

    /* ------------------------------------------------------------------ Str */

    /*! A NUL-terminated string with a hard capacity. Never allocates.
     *
     *  Overflow is always reported, never silently absorbed: Set and Append
     *  return false and leave the string unchanged if it would not fit. A
     *  half-built path passed to cellFsOpen is worse than a clean failure. */
    class Str
    {
    public:
        Str() : m_len(0) { m_s[0] = '\0'; }

        /*! Implicit, so callers may pass string literals directly. */
        Str(const char* src) : m_len(0)
        {
            m_s[0] = '\0';
            if (src != NULL)
                Set(src);
        }

        void Clear()
        {
            m_s[0] = '\0';
            m_len = 0;
        }

        bool Set(const char* src)
        {
            m_s[0] = '\0';
            m_len = 0;
            if (src == NULL)
                return true;
            return Append(src);
        }

        bool Set(const Str& other)
        {
            return Set(other.c());
        }

        bool Append(const char* src)
        {
            if (src == NULL)
                return true;

            size_t add = strlen(src);
            if (m_len + add >= kMaxPath)
                return false;

            memcpy(m_s + m_len, src, add + 1);
            m_len += add;
            return true;
        }

        const char* c() const { return m_s; }
        size_t size() const { return m_len; }
        char at(size_t i) const { return m_s[i]; }

        /*! Keep [0, index), adding the NUL. False if index is out of range. */
        bool Truncate(size_t index)
        {
            if (index > m_len)
                return false;

            m_s[index] = '\0';
            m_len = index;
            return true;
        }

        /*! Copy [from, to) into out. False if the range is invalid or it would
         *  overflow out. Used to build parent directory paths. */
        bool CopyRange(const Str& from, size_t begin, size_t end, Str& out) const
        {
            if (begin > end || end > from.m_len)
                return false;

            out.Clear();
            size_t n = end - begin;
            if (n >= kMaxPath)
                return false;

            memcpy(out.m_s, from.m_s + begin, n);
            out.m_s[n] = '\0';
            out.m_len = n;
            return true;
        }

    private:
        char   m_s[kMaxPath];
        size_t m_len;
    };

    /* ----------------------------------------------------------------- Bytes */

    /*! A non-owning window over caller-provided storage.
     *
     *  Holds no buffer itself, so constructing one is free and it is safe to
     *  create on the stack - it only points at storage the caller guarantees
     *  outlives it. That is the whole reason this is not a fixed array: the
     *  payload buffers are megabytes and cannot go on the stack. */
    class Bytes
    {
    public:
        Bytes() : m_data(NULL), m_cap(0), m_size(0) {}

        /*! Bind to storage. capacity must be at least `capacity` bytes and the
         *  storage must outlive this object. */
        void Attach(uint8_t* storage, size_t capacity)
        {
            m_data = storage;
            m_cap  = storage != NULL ? capacity : 0;
            m_size = 0;
        }

        /*! Copy len bytes in. False if that would overflow the attached
         *  capacity, which is reported rather than truncated. */
        bool Assign(const void* src, size_t len)
        {
            if (len > m_cap)
            {
                m_size = 0;
                return false;
            }

            if (len > 0 && src != NULL)
                memcpy(m_data, src, len);

            m_size = len;
            return true;
        }

        void Clear() { m_size = 0; }

        /*! Publish len bytes that the caller has already written into the
         *  attached storage. Used by read paths, which fill the buffer directly
         *  rather than through Assign - going through Assign there would be a
         *  memcpy of a region onto itself. Fails if len exceeds capacity, so a
         *  miscomputed read length cannot silently become a valid shorter one. */
        bool Publish(size_t len)
        {
            if (len > m_cap)
            {
                m_size = 0;
                return false;
            }

            m_size = len;
            return true;
        }

        size_t size() const { return m_size; }
        size_t capacity() const { return m_cap; }

        const uint8_t* data() const { return m_data; }
        uint8_t* data() { return m_data; }

        /*! Byte-for-byte comparison, including length. */
        bool Equals(const void* other, size_t len) const
        {
            if (len != m_size)
                return false;

            if (len == 0)
                return true;

            return memcmp(m_data, other, len) == 0;
        }

    private:
        uint8_t* m_data;
        size_t   m_cap;
        size_t   m_size;
    };

    /* ----------------------------------------------------------------- Lines */

    /*! One line, as a range into the buffer it was split from. Ranges rather
     *  than copies, so splitting a 64 KB file costs no memory. */
    struct LineRef
    {
        uint32_t start;
        uint32_t len;
        uint32_t termStart;
        uint32_t termLen;
    };
}
