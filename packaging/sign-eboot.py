#!/usr/bin/env python3
"""
Sign the qDNSm Installer EBOOT.

SNC's ConfigurationType=Application emits a complete, correctly-linked ET_EXEC
SELF, but scetool cannot wrap one: its -e input has to be a raw ELF. Handed an
SCE\\0 file it writes a few hundred bytes of empty shell and still reports
"SELF built / Data encrypted / written".

SNC's other option, DynamicLibrary + OutputFormat=PRXFile, does produce a raw
ELF - but as a PRX image, which is not loadable as a game:

    e_type  = 0xFFA4   PRX-specific        (a real EBOOT is 0x0002, ET_EXEC)
    e_entry = 0x0      no entry point      (a real EBOOT is 0x10000000)

The console maps that image, finds no entry to jump to, and fails with
SCE_NP_DRM_ERROR_FORMAT 80010007.

So this takes the middle path: build as an Application, cut the raw ELF back out
of the SELF that ps3ppuld produced, sign that, and verify the result.

The ELF sits at a fixed offset inside the SELF - 0x90 in every known-good EBOOT
and in ours - so it is recovered by locating the ELF magic rather than trusting a
constant, then trimmed to the end of its own load segments so no trailing NPDRM
data rides along.

After signing, the result is checked and the build fails if the image would not
boot. That check is the point of this script: e_type and e_entry cannot be seen
from a green build log, and an installer that produces an unloadable EBOOT looks
exactly like success until someone puts it on a console.

Usage:
    python sign-eboot.py --self <path> --out <path> [--scetool <path>]
"""

import argparse
import os
import struct
import subprocess
import sys

# --- NPDRM parameters for the installer -------------------------------------
#
# These must agree with packaging/make-installer-pkg.bat's CONTENT_ID and with
# PARAM.SFO's TITLE_ID. The console compares the EBOOT's embedded content ID
# against the package's; if they drift you get a mismatch error instead of a
# format error, which is much harder to read.
CONTENT_ID = 'UP0001-NPUB72381_00-0000000000000000'
AUTH_ID    = '1010000001000003'   # scetool's own documented example
VENDOR_ID  = '01000002'
# scetool's key revision, which lands verbatim at SELF header offset 0x08.
# 0A reads as "matches the shipped key store" but that store only supplies
# the AES keys - the revision selects which key slot is used. Both known-good
# homebrew EBOOTs carry 01, and RPCS3 resolves its NPD keyset by revision,
# so an 0A SELF fails to decrypt and it reports the result as
# "Invalid or unsupported PPU executable format". 01 is the revision the
# console and RPCS3 both expect for this kind of homebrew SELF.
KEY_REV    = '01'
FW_VER     = '0003004000000000'
APP_VER    = '0001000000000000'
CTRL_FLAGS = ('4000000000000000000000000000000000000000000000000000000000000002')

# A loadable EBOOT image. e_type was checked against two known-good EBOOTs.
REQUIRED_E_TYPE = 0x0002          # ET_EXEC

# A loadable EBOOT image. e_type and the executable base were both checked
# against two known-good EBOOTs.
REQUIRED_E_TYPE = 0x0002          # ET_EXEC
EXEC_BASE = 0x10000000            # conventional PS3 executable address

PF_X = 0x1                        # executable segment


class Failure(Exception):
    pass


def _find_elf(blob):
    off = blob.find(b'\x7fELF')
    if off < 0:
        raise Failure('no ELF magic anywhere in the file')
    return off


def extract_raw_elf(sce_bytes):
    """Cut the raw ELF out of an SNC SELF.

    The ELF is located by its magic rather than assumed to sit at a fixed
    offset, then trimmed to the end of its own program headers so trailing
    NPDRM metadata, signatures and padding in the SELF are not handed to scetool
    as if they were part of the module.
    """
    off = _find_elf(sce_bytes)
    e = sce_bytes[off:]

    if len(e) < 0x40:
        raise Failure('ELF truncated: only %d bytes' % len(e))

    e_type = struct.unpack('>H', e[0x10:0x12])[0]
    phoff = struct.unpack('>Q', e[0x20:0x28])[0]
    shoff = struct.unpack('>Q', e[0x28:0x30])[0]
    phentsize = struct.unpack('>H', e[0x36:0x38])[0]
    phnum = struct.unpack('>H', e[0x38:0x3A])[0]
    shentsize = struct.unpack('>H', e[0x3A:0x3C])[0]
    shnum = struct.unpack('>H', e[0x3C:0x3E])[0]

    end = 0x40
    for i in range(phnum):
        o = phoff + i * phentsize
        if o + 0x38 > len(e):
            raise Failure('program header %d runs past the end of the ELF' % i)
        p_offset = struct.unpack('>Q', e[o + 0x08:o + 0x10])[0]
        p_filesz = struct.unpack('>Q', e[o + 0x20:o + 0x28])[0]
        end = max(end, p_offset + p_filesz)

    if shoff and shnum:
        end = max(end, shoff + shnum * shentsize)

    if end > len(e):
        end = len(e)

    print('[sign-eboot] extracted ELF at 0x%X, %d bytes  (e_type=0x%04X)'
          % (off, end, e_type))
    return e[:end]


def _segments(elf_bytes):
    """(vaddr, memsz, flags) for each program header that describes memory."""
    phoff = struct.unpack('>Q', elf_bytes[0x20:0x28])[0]
    phentsize = struct.unpack('>H', elf_bytes[0x36:0x38])[0]
    phnum = struct.unpack('>H', elf_bytes[0x38:0x3A])[0]

    out = []
    for i in range(phnum):
        o = phoff + i * phentsize
        if o + 0x38 > len(elf_bytes):
            break
        # Elf64_Phdr: p_type@0x00 p_flags@0x04 p_offset@0x08 p_vaddr@0x10
        #             p_paddr@0x18 p_filesz@0x20 p_memsz@0x28 p_align@0x30
        p_type = struct.unpack('>I', elf_bytes[o:o + 4])[0]
        flags = struct.unpack('>I', elf_bytes[o + 4:o + 8])[0]
        vaddr = struct.unpack('>Q', elf_bytes[o + 0x10:o + 0x18])[0]
        memsz = struct.unpack('>Q', elf_bytes[o + 0x28:o + 0x30])[0]
        filesz = struct.unpack('>Q', elf_bytes[o + 0x20:o + 0x28])[0]
        out.append((p_type, vaddr, filesz, memsz, flags))
    return out


def verify_image(elf_bytes, stage):
    """Refuse to continue if the image is not loadable as a game.

    Two requirements, and only the two the ground truth actually supports:

      * e_type must be ET_EXEC (0x0002). Both known-good EBOOTs are 0x0002; a
        PRX image is 0xFFA4, laid out for a module hosted by a PRX loader - vaddr
        0 with the entry hidden in paddr - and a game EBOOT is not one.

      * e_entry must be non-zero. Both known-good EBOOTs have 0x10000000.

    Note what is deliberately NOT checked: that e_entry falls inside an executable
    segment. It was tried, and both known-good EBOOTs fail it - their entry
    points at 0x10000000, the base of a segment that is not marked executable.
    So on this platform e_entry is not simply "where the code is", and asserting
    it lands in PF_X would reject files that demonstrably boot. Only the two
    properties above are checked, because those are the only ones evidence
    supports.

    The previous PRX-image build had e_type 0xFFA4 and e_entry 0x0, and failed to
    boot with 80010007.
    """
    e_type = struct.unpack('>H', elf_bytes[0x10:0x12])[0]
    e_entry = struct.unpack('>Q', elf_bytes[0x18:0x20])[0]

    segs = _segments(elf_bytes)
    print('[sign-eboot]   %s: e_type=0x%04X e_entry=0x%X  %d segments'
          % (stage, e_type, e_entry, len(segs)))
    for p_type, vaddr, filesz, memsz, flags in segs:
        if memsz:
            print('[sign-eboot]       type=0x%-9x vaddr=0x%08X filesz=%-7d memsz=%-9d %s'
                  % (p_type, vaddr, filesz, memsz, 'X' if flags & PF_X else '-'))

    if e_type != REQUIRED_E_TYPE:
        raise Failure(
            '%s has e_type 0x%04X but a loadable EBOOT needs 0x%04X (ET_EXEC). '
            'A PRX image leaves vaddr at 0 and hides the entry in paddr.'
            % (stage, e_type, REQUIRED_E_TYPE))

    if e_entry == 0:
        raise Failure(
            '%s has e_entry 0. The loader has no address to transfer control to. '
            'A PRX-format build leaves it at zero.' % stage)

    # Both known-good EBOOTs carry a segment based at the conventional PS3
    # executable address and set e_entry to exactly that base.
    #
    # Getting there is not something the SNC Application template does on its own:
    # left alone, ppu-lv2-gcc injects its own -T ELF64_LV2_PRX.X, which merges text
    # and shared read-only data into one segment and leaves the shared rodata
    # segment empty. That produces a compact two-segment image with code at 0x10000
    # and e_entry pointing into the data segment. RPCS3 rejects that outright
    # ("Invalid or unsupported PPU executable format") and the console silently
    # returns to the XMB without running anything. Sony's own TTYEcho example
    # reproduces the same compact layout from its shipped .vcxproj, so this is the
    # SNC driver's normal output rather than a fault in any one project.
    #
    # passes -mno-prxfixup so the driver stops injecting its script. That script
    # reserves .rodata at 0x10000000 through ALIGN(256M) and names that point
    # _ppu_exec_base for ENTRY, which is what produces e_entry == 0x10000000.
    if e_entry != EXEC_BASE:
        raise Failure(
            '%s has e_entry 0x%X. Both known-good EBOOTs use 0x%X, the '
            'conventional PS3 executable base, and set e_entry to it.\n'
            '       This is a link-layout problem, not a signing one. The SNC '
            'link needed -mno-prxfixup -mlv2-stub plus -Wl,-T pointing at '
            'ps3-exec.ld; both are gone now (TODO.md item 19), so a wrong e_entry '
            'here means this ELF was not built by installer_src/Makefile.'
            % (stage, e_entry, EXEC_BASE))


def scetool_command(scetool, elf_path, out_path):
    return [
        scetool,
        '-0', 'SELF',
        '-1', 'TRUE',
        '-s', 'FALSE',
        # Left on scetool's default (add ELF section headers) our SELF carried a
        # 9th metadata section - type 01, unencrypted, uncompressed, trailing the
        # image - where both known-good EBOOTs have 8. scetool -i reports this
        # as Section Count 9 vs 8. RPCS3 walks the section table to decrypt, so
        # the stray block is the best remaining candidate for it rejecting the
        # image outright.
        '-7', 'FALSE',
        '-2', KEY_REV,
        '-3', AUTH_ID,
        '-4', VENDOR_ID,
        '-5', 'NPDRM',
        '-6', FW_VER,
        '-A', APP_VER,
        '-b', 'FREE',
        '-c', 'EXEC',
        '-f', CONTENT_ID,
        '-g', 'EBOOT.BIN',
        '-j', 'TRUE',
        '--self-ctrl-flags', CTRL_FLAGS,
        '-e', elf_path,
        out_path,
    ]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--self', required=True, help='SNC output (SCE\\0)')
    ap.add_argument('--out', required=True, help='EBOOT.BIN to write')
    ap.add_argument('--scetool',
                    default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                         '..', 'vendor', 'scetool', 'bin', 'scetool.exe'))
    args = ap.parse_args()

    if not os.path.isfile(args.self):
        print('[sign-eboot] input not found: %s' % args.self)
        return 1
    if not os.path.isfile(args.scetool):
        print('[sign-eboot] scetool not found: %s' % args.scetool)
        return 1

    with open(args.self, 'rb') as fp:
        sce = fp.read()

    if sce[:4] == b'SCE\x00':
        raw = extract_raw_elf(sce)
    elif sce[:4] == b'\x7fELF':
        print('[sign-eboot] input is already a raw ELF, using as-is')
        raw = sce
    else:
        raise Failure('input is neither a SELF nor an ELF: magic %r' % sce[:4])

    # Written before verification so a rejected build still leaves the extracted
    # module on disk to inspect, rather than only an error message.
    tmp = os.path.join(os.path.dirname(os.path.abspath(args.out)), 'installer.raw.elf')
    with open(tmp, 'wb') as fp:
        fp.write(raw)
    print('[sign-eboot] wrote %s' % tmp)

    verify_image(raw, 'extracted')

    # Never let a stale file masquerade as a fresh successful sign.
    if os.path.exists(args.out):
        os.remove(args.out)

    print('[sign-eboot] running scetool...')
    # scetool resolves data\keys relative to its working directory, not relative
    # to its own executable, so it has to be run from vendor\scetool\bin. Run from
    # anywhere else and it exits without a word - no output, no file, no error.
    scetool_dir = os.path.dirname(os.path.abspath(args.scetool))
    proc = subprocess.run(scetool_command(args.scetool, tmp, args.out),
                          cwd=scetool_dir, capture_output=True, text=True)
    for line in (proc.stdout or '').splitlines():
        print('  ' + line)
    for line in (proc.stderr or '').splitlines():
        print('  ' + line)

    if not os.path.exists(args.out):
        raise Failure(
            'scetool wrote no %s (exit code %s). It was run from %s. If that is '
            'not vendor\\scetool\\bin it cannot find data\\keys and fails '
            'silently.'
            % (args.out, proc.returncode, scetool_dir))

    with open(args.out, 'rb') as fp:
        signed = fp.read()

    if signed[:4] != b'SCE\x00':
        raise Failure('signed output is not a SELF: magic %r' % signed[:4])

    off = _find_elf(signed)
    verify_image(signed[off:], 'signed EBOOT')

    print('[sign-eboot] OK  %s  %d bytes' % (args.out, len(signed)))
    return 0


if __name__ == '__main__':
    try:
        sys.exit(main())
    except Failure as exc:
        print('[sign-eboot] FAIL  %s' % exc)
        sys.exit(1)
