#!/usr/bin/env python3
"""
Build the qDNSm Installer PKG.

A wrapper around vendor/PSL1GHT/tools/ps3py/pkg.py - the tool pkgi-ps3 uses to
package its own EBOOT.

Why pkg.py and not vendor/makepkg
---------------------------------
makepkg1.exe is a build of make_package_npdrm. Every mode of it is NPDRM
packaging: it demands a real NPDRM SELF with valid EDATA digests and refuses
anything else with

    >> !ERROR!: NPUB72381/USRDIR/EBOOT.BIN is not an NPDRM SELF.

That is not a defect in our EBOOT, and no flag changes it. qDNSm is deliberately
a 3.55 homebrew fake self, signed so it loads on CFW and HEN. NPDRM signing
needs Sony's NPDRM content keys, which exist nowhere outside Sony. Homebrew and
NPDRM are mutually exclusive; a packager can do one or the other.

pkg.py never opens the EBOOT. It stats it for size, tags the PKG entry as NPDRM,
and writes a hardcoded digest - see fself.py, whose DigestTypeNPDRM carries the
fileSHA1 bytes "Bite Me Sony". That is the point of the homebrew toolchain: it
produces packages that install and run, without NPDRM keys.

Why this file exists instead of calling pkg.py directly
------------------------------------------------------
pkg.py runs `import pkgcrypt` at module level, unconditionally, and pkgcrypt is a
C extension that upstream PSL1GHT does not vendor. Without it pkg.py cannot even
be imported.

pkgcrypt does one thing: the PS3 package obfuscation, used for the licensee
field, the metablock padding, and the main data block. pkg.py carries the
original pure-Python implementation directly below the C call - unreachable,
because the call returns first. This module reimplements that, ported to Python 3.

Usage
-----
    python make-pkg.py --contentid=<id> <staging-dir>/ <out.pkg>

The trailing slash on the staging directory is REQUIRED. pkg.py derives each
entry's in-package path by stripping the staging prefix off the full path, so
without the trailing separator every name keeps a leading slash and
os.path.join then discards the base and looks for /ICON0.PNG at filesystem root.
PSL1GHT's own ppu_rules passes $(BUILDDIR)/pkg/ for the same reason.
"""

import hashlib
import os
import runpy
import struct
import sys
import types

HERE = os.path.dirname(os.path.abspath(__file__))
PKG_PY_DIR = os.path.normpath(
    os.path.join(HERE, '..', 'vendor', 'PSL1GHT', 'tools', 'ps3py'))
PKG_PY = os.path.join(PKG_PY_DIR, 'pkg.py')

_CONTEXT_BYTES = 0x40
_COUNTER_AT = 0x38
_BLOCK = 0x10


def _pkgcrypt(key, inbuf, length):
    """PS3 package obfuscation.

    Ported from the pure-Python implementation in pkg.py, which sits immediately
    below the pkgcrypt call it replaces and is unreachable because that call
    returns unconditionally.

    Stateless per call, exactly as the C original was: pkg.py passes
    listToString(key), a fresh copy, so manipulate() advanced the counter inside
    pkgcrypt and never mutated the caller's list. Reproducing that means each call
    starts from the counter carried in the incoming context, which is what this
    does.
    """
    context = bytearray(key)
    source = bytes(inbuf)
    out = bytearray()

    offset = 0
    while length > 0:
        chunk = _BLOCK if length > _BLOCK else length
        keystream = hashlib.sha1(bytes(context)).digest()
        for i in range(chunk):
            out.append(keystream[i] ^ source[offset + i])
        offset += chunk
        length -= chunk

        counter = struct.unpack('>Q', bytes(context[_COUNTER_AT:_CONTEXT_BYTES]))[0]
        counter = (counter + 1) & 0xFFFFFFFFFFFFFFFF
        context[_COUNTER_AT:_CONTEXT_BYTES] = struct.pack('>Q', counter)

    return bytes(out)


def _install_pkgcrypt():
    """Provide pkgcrypt. Prefer upstream's if it is present."""
    try:
        import pkgcrypt  # noqa: F401
        return 'upstream'
    except ImportError:
        pass

    stub = types.ModuleType('pkgcrypt')
    stub.pkgcrypt = _pkgcrypt
    stub.register_sha1_callback = lambda callback: None
    sys.modules['pkgcrypt'] = stub
    return 'pure-python (vendored port)'


def main():
    if not os.path.isfile(PKG_PY):
        sys.stderr.write('pkg.py not found at %s\n' % PKG_PY)
        return 2

    args = sys.argv[1:]

    # Reject the failure mode above before pkg.py fails with a confusing
    # FileNotFoundError for /ICON0.PNG.
    for arg in args:
        if arg.endswith('.pkg') or arg.startswith('--'):
            continue
        if os.path.isdir(arg) and not arg.endswith(('/', '\\')):
            sys.stderr.write(
                'staging directory needs a trailing slash: %s -> %s/\n'
                % (arg, arg))
            return 2

    sys.path.insert(0, PKG_PY_DIR)
    mode = _install_pkgcrypt()

    sys.argv = ['pkg.py'] + args
    sys.stderr.write('[make-pkg] pkgcrypt: %s\n' % mode)

    try:
        runpy.run_path(PKG_PY, run_name='__main__')
    except SystemExit as exc:
        return exc.code if isinstance(exc.code, int) else 0
    return 0


if __name__ == '__main__':
    sys.exit(main())
