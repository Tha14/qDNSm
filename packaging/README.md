# packaging

Builds the **qDNSm Installer** PKG — a one-click install of qDNSm.sprx for people
who don't want to edit `boot_plugins.txt` on the console themselves.

The installer installs itself as an XMB application under title ID `NPUB72381`,
so installing the PKG is the only step before it appears on the XMB.

## What gets built

Building the `installer` project produces a ready-to-package folder:

```
bin/Release-PS3/installer/
  installer.elf        linked module (intermediate)
  EBOOT.BIN            signed installer
  qDNSm.sprx           the payload it installs (signed)
  qDNSm-Installer.pkg  <- ship this
  pkgwork/             staging scratch, safe to delete
```

Everything else in that folder is scratch. **`qDNSm-Installer.pkg` is the only
file to distribute.**

## How it works

`make-installer-pkg.bat` stages a title folder and calls `make-pkg.py --no-stage`:

```
bin/Release-PS3/installer/pkgwork/NPUB72381/
  PARAM.SFO
  ICON0.PNG
  USRDIR/
    EBOOT.BIN
    qDNSm.sprx
```

Run standalone with:

```
packaging\make-installer-pkg.bat [Configuration] [Platform]
```

Defaults to `Release PS3`. It also runs automatically as the last step of the
`installer` project's post-build, so a normal solution build produces the PKG
with no extra step.

Before staging, the script refreshes `bin/<config>-<platform>/installer/qDNSm.sprx`
from the plugin build's `bin/<config>-<platform>/qDNSm.sprx`. That copy is what
gets packaged, so a qDNSm.sprx left in the installer folder by an earlier build
cannot be shipped by mistake. The installer EBOOT is taken from
`bin/<config>-<platform>/installer/EBOOT.BIN`; if only a Linux installer build
exists, the script falls back to `installer_src/build/pkg/USRDIR/EBOOT.BIN`.

It always exits `0`, even when packaging fails, so a packaging problem can't
block a successful code build. Failures print `[FAIL]` and leave the compiled
`EBOOT.BIN` in place.

## Two things it refuses to skip

**EBOOT size floor.** `scetool` reports success while writing an empty shell —
it prints `SELF built / Data compressed / Data encrypted / written` and emits a
few hundred bytes of header with no payload. So "EBOOT.BIN written" proves
nothing; size does. The installer module is around 10 KB, so anything under
8 KB is treated as a stub.

**Key fingerprint.** `scetool` signs silently with whatever store is present.
Because of that the packaging step checks the store before trusting the result:
a homebrew key set has a zeroed private key under `[NP_sig]`, a retail NPD store
has a real one. If that fingerprint is absent the script fails loudly instead of
producing a package that installs and then does nothing.

## The packager: why not `vendor/makepkg`

`makepkg1.exe` is a build of `make_package_npdrm`. **Every mode of it is NPDRM
packaging**, and it rejects our EBOOT:

```
>> !ERROR!: NPUB72381/USRDIR/EBOOT.BIN is not an NPDRM SELF.
Illegal Package: USRDIR/EBOOT.BIN is required.
```

That is not a defect in our EBOOT and no flag changes it. qDNSm is deliberately a
3.55 homebrew fake self, signed so it loads on CFW and HEN. NPDRM signing needs
Sony's NPDRM content keys, which exist nowhere outside Sony. **Homebrew and NPDRM
are mutually exclusive** — a packager does one or the other, and
`makepkg1.exe` is unambiguously the latter.

Worse, `makepkg` exits `0` while doing this and writes a `.pkg` with the EBOOT
silently omitted. A package with no EBOOT installs cleanly and then does nothing
when launched, which is the worst failure mode there is — it looks like success.

`vendor/makepkg` is left untouched.

## What is used instead

`make-pkg.py`, wrapping `vendor/PSL1GHT/tools/ps3py/pkg.py` — the toolchain
**pkgi-ps3** uses to package its own EBOOT. It never opens the EBOOT: it stats it
for size, tags the PKG entry as NPDRM in the flags, and writes a hardcoded
digest. See `fself.py`, whose `DigestTypeNPDRM` carries the `fileSHA1` bytes
`Bite Me Sony`. That is the whole point of the homebrew toolchain — it produces
packages that install and run, without NPDRM keys.

By default `make-pkg.py` also stages the current compiled binaries before
packaging, so on Linux it can package without a separate copy step:

```
python3 packaging/make-pkg.py --contentid=UP0001-NPUB72381_00-0000000000000000 \
    bin/Release-PS3/installer/pkgwork/NPUB72381/ \
    bin/Release-PS3/qDNSm-Installer.pkg
```

It copies `bin/Release-PS3/qDNSm.sprx` to `<staging>/USRDIR/qDNSm.sprx` and
`bin/Release-PS3/installer/EBOOT.BIN` to `<staging>/USRDIR/EBOOT.BIN`, and
fills in missing `ICON0.PNG` / `PARAM.SFO` from `packaging/assets`. Use
`--no-stage` when the staging tree is already assembled; the Windows batch
drivers do that because they stage explicitly.

### Why `make-pkg.py` exists rather than calling `pkg.py` directly

`pkg.py` does `import pkgcrypt` at module level, unconditionally. `pkgcrypt` is a
C extension that upstream PSL1GHT does not vendor, so `pkg.py` cannot be imported
at all without it.

`pkgcrypt` does one thing: the PS3 package obfuscation, used for the licensee
field, the metablock padding, and the main data block. `pkg.py` carries the
original pure-Python implementation directly below the C call — unreachable,
because that call returns first. `make-pkg.py` reimplements it, ported to Python 3,
and injects it as a stub. Vendored code is not modified.

The port is stateless per call, matching the C original: `pkg.py` passes
`listToString(key)`, a fresh copy, so `manipulate()` advanced the counter inside
`pkgcrypt` and never mutated the caller's list.

If upstream's real `pkgcrypt` is present, it is used and the port is ignored.

### The trailing slash is required

`pkg.py` derives each entry's in-package name by normalising separators and then
stripping the staging prefix off the full path. Two consequences:

- The staging directory must end in a separator, or every name keeps a leading
  slash and `os.path.join` then looks for `/ICON0.PNG` at filesystem root.
- An **absolute Windows path does not work** — the prefix no longer matches once
  backslashes become forward slashes.

So the batch script passes a **relative** path with a trailing slash, run from
the staging parent. PSL1GHT's own `ppu_rules` passes `$(BUILDDIR)/pkg/` for the
same reason. `make-pkg.py` rejects a missing trailing slash with a clear message
rather than letting `pkg.py` fail on `/ICON0.PNG`.

## Python

Python 3 is required, and the script finds it via `py -3`, `python`, or
`python3`, in that order. It reports which interpreter it used. If none is found
the build fails with a clear message; nothing else in the toolchain needs Python.

## assets/

| file | role |
|---|---|
| `PARAM.SFO` | XMB metadata. Hand-made, and the source of truth. |
| `ICON0.PNG` | 512x512 RGBA, the icon shown under the XMB entry. |

### Why PARAM.SFO is hand-made

`pkg.py` uses whatever `PARAM.SFO` is in the staging folder and never generates
one, so this file is not optional — but it is also not something the packager
could sensibly invent. It carries:

| key | value |
|---|---|
| `TITLE_ID` | `NPUB72381` |
| `TITLE` | `qDNSm Installer` |
| `CATEGORY` | `HG` |
| `BOOTABLE` | `1` |
| `PS3_SYSTEM_VER` | `03.4000` |

`CATEGORY = HG` is what makes the XMB treat this as a launchable homebrew app.
With a plain game category it would show as an icon that does nothing when
pressed. `BOOTABLE = 1` is what lets the XMB start it at all.

To use a different icon, replace `ICON0.PNG`; keep it a real PNG, not a JPEG.

## Signing

`EBOOT.BIN` is signed by the same `scetool` invocation and the same
`vendor/scetool/bin/data/keys` as the qDNSm plugin — a 3.55 key set, which is
the one signature that loads on both CFW and HEN. The payload this EBOOT
installs and the EBOOT itself therefore carry identical signatures, and there is
no manual key step anywhere in the build. See
[`../vendor/scetool/SIGNING.md`](../vendor/scetool/SIGNING.md).

The installer module is built as a `DynamicLibrary` with `OutputFormat` set to
`PRXFile`, because that is the only container `ps3ppuld` will emit as a raw ELF
for `scetool` to wrap. See `vendor/scetool/SIGNING.md` for why an `Application`
cannot produce one, and note that the `extern "C"` on `module_start` in
`Installer.cpp` is load-bearing.
