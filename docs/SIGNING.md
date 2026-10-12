# Signing qDNSm

## One output, and it covers everything

The build produces a single signed artifact:

```
bin/Release-PS3/qDNSm.sprx
```

That file loads on **CFW, PEX/D-PEX, DEX-mode CFW, and PS3HEN**. There is no
separate HEN build, and there is no reason for one.

## Why a single signature suffices

The three environments check signatures differently:

| environment | what it does with the signature |
|---|---|
| CFW | bypasses signature checks outright — anything loads |
| PEX / D-PEX / DEX-mode CFW | CEX kernel, some DEX modules swapped in; homebrew still runs CEX |
| PS3HEN | hooks the module loader to accept the **static pre-3.55** NPD keys |

So the 3.55-resigned signature is the one value that works everywhere. It is
the intersection of all three, which is why every major PS3 homebrew release
ships CEX-only: webMAN MOD, multiMAN, Irisman, all of them.

Real DEX hardware is a separate and much smaller story — around 30k developer
units ever built, almost none in homebrew hands, and HEN does not target it
anyway. If that ever changes, the fix is a second scetool run against the DEX
keyset in this same post-build block.

## How scetool picks keys

Nothing on the scetool command line selects a firmware. The key file is the
whole mechanism. scetool looks for its key store in `data/` relative to its own
executable, or on the `PS3` path environment variable.

The post-build step `cd`s into `vendor/scetool/bin` first, so the store it uses
is unambiguously:

```
vendor/scetool/bin/data/keys
```

## scetool's own output is obfuscated

`scetool.exe` cannot be introspected. Its PE sections are:

```
YOU    vsize=122880  rawsize=0
WISH   vsize=73728   rawsize=71168
.rsrc  vsize=4096    rawsize=1024
```

The `WISH` section holds packed data and `YOU` has no raw bytes at all; both are
filled in at startup. As a result **no option string is readable in the binary** —
`scetool`, `SELF`, `SPRX`, `SELF built`, `Data compressed` and
`--self-ctrl-flags` all occur zero times, despite the program printing them on
every run.

This is deliberate and it is why questions about scetool's capabilities have to be
answered by running `scetool.exe --help` on a machine that has it, rather than by
inspecting the file.

## The installer EBOOT is a different problem

An executable on the PS3 is a SELF, so `EBOOT.BIN` legitimately looks like this:

```
53 43 45 00 ...   SCE\0
```

The plugin, by contrast, is a raw ELF:

```
7f 45 4c 46 ...   \x7fELF
```

That difference drives everything below. scetool's `-e` takes a **raw ELF** and
produces a signed SELF. Hand it something that is already a SELF and it writes a
few hundred bytes of empty shell while printing `SELF built / Data compressed /
Data encrypted / written`. The build deletes `EBOOT.BIN` before signing and
verifies the result, so that cannot pass unnoticed.

### The chain, in the order it actually had to be solved

Each of these was found by comparing bytes, not by reading documentation.

**1. `module_start` needs `extern "C".** `SYS_MODULE_START` emits a reference to
the *unmangled* symbol name. A plain C++ definition mangles to
`_Z12module_startjPv`, the reference is left `UND`, and the link fails with:

```
ppu-lv2-prx-libgen.exe: "module_start" is undefine symbol. can't export.
```

qDNSm's `Main.cpp` has no `extern "C"` and still links, because the macro's own
declaration supplies the linkage there. `Installer.cpp` states it explicitly so
it does not depend on that.

**2. `Application` + `OutputFormat=PRXFile` is not an executable.** It does
produce a raw ELF, which is what scetool wants — but as a PRX image:

```
e_type  = 0xFFA4   (PRX)       a real EBOOT is 0x0002, ET_EXEC
e_entry = 0x0      (none)      a real EBOOT is 0x10000000
vaddr   = 0, entry hidden in paddr
```

A PRX is laid out for a module hosted by a PRX loader. Launched as a game it
fails with `80010007`. Setting `ConfigurationType=Application` makes no
difference while `PRXFile` is set — the linker's PRX layout overrides it.

**3. `Application` alone is correct, and its ELF is recoverable.** With no
`OutputFormat` override, `ps3ppuld -oformat=fself` emits a proper `ET_EXEC`
SELF. The raw ELF sits inside it at offset `0x90`, as it does in every
known-good EBOOT. `packaging/sign-eboot.py` locates it by magic, trims it to the
end of its own load segments, and hands that to scetool.

**4. NPDRM metadata is mandatory.** scetool's default output is a plain fake self
with no NPD block and no content ID. Fine for `qDNSm.sprx` — a plugin loaded
through `boot_plugins.txt` never goes through the EA check at launch. An EBOOT
does, and the console rejects it with `SCE_NP_DRM_ERROR_FORMAT` (`80029530`). A
known-good EBOOT carries:

```
4E 50 44 00  "NPD\0" ... "UP0001-NP0APOLLO_00-0000000000000000"
```

Three flags produce that: `-f` np-content-id, `-g` np-real-fname, `-j`
np-add-sig. Note `-3` is `--self-auth-id`, **not** the content ID.

**5. Three IDs must agree.**

```
EBOOT   -f         UP0001-BLUS72381_00-0000000000000000
PKG     contentid  UP0001-BLUS72381_00-0000000000000000
PARAM.SFO TITLE_ID BLUS72381
```

The final field must be 16 hex digits. It was once
`000000-BLUS72381_00-1GAME00000000000` — `G` and `M` are not hex — which is
itself an `SCE_NP_DRM_ERROR_FORMAT`.

**6. scetool must run from `vendor/scetool/bin`.** It resolves `data\keys`
relative to its *working directory*, not its own path. Run from anywhere else it
exits immediately with no output, no file and no error code.

### Still open

The layout is the last thing. Known-good homebrew EBOOTs carry a segment based at
`0x10000000` carrying the PS3-specific flag `0x00400000`, and set `e_entry` to
that base. SNC's toolset links this project with `elf64_lv2_prx.x` — the *PRX*
script — and produces two compact segments instead, with `e_entry` pointing into
the data segment. RPCS3 rejects that outright (`Invalid or unsupported PPU
executable format`); the console returns to the XMB without running anything.

`packaging/sign-eboot.py` therefore **fails the build** unless `e_type` is
`ET_EXEC`, `e_entry` is non-zero, and `e_entry` is `0x10000000`. It accepts both
known-good EBOOTs and rejects the current image by name. That gate exists
because every other signal — a green build, scetool's success message — looks
identical whether or not the result can boot.

`vendor/scetool/bin/data/keys` is **byte-identical to the `keys` file inside
TrueAncestor SELF Resigner**, once line endings are accounted for:

| file | size | lines |
|---|---|---|
| `vendor/scetool/bin/data/keys` | 24,549 | 916 |
| TrueAncestor `data/keys` | 25,465 | 916 |

After normalising CRLF to LF the two compare equal; the 916-byte difference is
exactly 916 `\r` characters. `ldr_curves` and `vsh_curves` are byte-identical
outright, and in any case are only consulted when *decrypting*, never signing.

Provenance is visible in the file itself:

```
[NP_sig]
type=OTHER
pub=E6792E446CEBA27BCADF374B99504FD8E80ADFEB3E66DE73FFE58D3291221C65018C038D3822C3C9
priv=000000000000000000000000000000000000000000
ctype=2
```

`priv` is all zeros and `ctype=2` — the homebrew / "free" NP Sig path, not a
retail signing key. That is the 3.55-and-lower set HEN needs.

There is no manual key step. If you ever need to verify:

```powershell
$a = [IO.File]::ReadAllText('vendor\scetool\bin\data\keys').Replace("`r`n","`n")
$b = [IO.File]::ReadAllText('here_are_keys\keys').Replace("`r`n","`n")
$a -ceq $b     # -> True
```

(`here_are_keys\` is a scratch drop-folder, gitignored. It is a redundant copy
and safe to delete.)

## The bug that used to be here

The build ran two signing steps:

```
scetool.exe    ... -e qDNSm.prx  qDNSm.sprx
SecureTool.exe -c 1 -k APP001 -ms qDNSm.sprx  qDNSm_HEN.sprx
```

`APP001` is the **licensed-application** key set, and `SecureTool.exe` is a
retail PKI signer — ECDSA/P-384 certificate machinery end to end, with no NPD
keyset anywhere in it. It wrote a *commercial* signature into the file named
`_HEN`.

So `qDNSm_HEN.sprx` was never HEN-signed. It was retail-signed, which is why it
loaded on some setups and silently failed on others — the one file whose name
promised HEN compatibility was the one file guaranteed not to have it.

Both steps are gone. `SecureTool` is no longer invoked anywhere in the project.

## Verifying

There is no way to tell from the `.sprx` whether a signature is the right kind —
a retail-signed file and a 3.55-signed file are indistinguishable from outside.
The only test is loading it:

1. Copy `qDNSm.sprx` to the console.
2. Point `boot_plugins.txt` at it.
3. Reboot.

If the menu does not appear, the signature is wrong. Nothing else in the build
can produce that failure, because the module compiles and runs identically
either way — only the signature differs.
