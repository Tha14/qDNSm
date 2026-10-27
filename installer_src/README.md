# qDNSm installer

Source for the qDNSm installer EBOOT and its PKG. Self-contained: everything needed
to build is either here or in `vendor/PSL1GHT`.

The installer installs `qDNSm.sprx`, `qDNSmNetLib.sprx` and
`qDNSm-server-games.ini` into `/dev_hdd0/plugins/` and registers the plugin in
`/dev_hdd0/boot_plugins.txt`, with an option to uninstall or update.

## Build

Requires Docker with the `ps3dev/ps3dev:submodules` image. Nothing is installed on
the host.

```bash
docker run --rm -v "$PWD:/work" ps3dev/ps3dev:submodules bash -lc \
  'export PSL1GHT=/usr/local/ps3dev PS3DEV=/usr/local/ps3dev && cd /work/installer_src && make pkg'
```

Output lands in `../bin/Release-PS3/qDNSm-Installer.pkg`. The build also leaves
`installer_src/installer.pkg` as the intermediate.

Host tests, which do not need the container:

```bash
sh installer_src/tests/run-tests.sh
```

## What is in here

| File | |
|---|---|
| `Makefile` | build, and the `pkg` target |
| `Installer.cpp` | the flow: prompt, install/uninstall, report, reboot |
| `InstallerFs.cpp/.hpp` | file operations over sysFs, with staged writes |
| `BootPlugins.cpp/.hpp` | reading and editing `boot_plugins.txt`; install and uninstall both strip blank lines and the trailing newline, leaving the final line bare, and uninstall leaves the file untouched when nothing of ours is in it |
| `Dialog.cpp/.hpp` | RSX bring-up and the SysUtil dialogs |
| `Small.hpp` | a small string/buffer type — see the note in the file |
| `TargetCompat.hpp` | maps the SDK's file APIs across SNC and PSL1GHT |
| `tests/` | host tests for the non-UI logic |

## Building a PKG

`make pkg` does the whole chain in one pass, which matters:

```
strip → sprx_tool → make_self_npdrm (USRDIR/EBOOT.BIN) → sfo → pkg
```

Signing the ELF by hand instead of using this chain produces an EBOOT that will not
load, because `strip` and `sprx_tool` are part of it. **Do not call
`make_self_npdrm` directly.**

The payloads are copied from `../bin/Release-PS3/qDNSm.sprx`,
`../bin/Release-PS3/qDNSmNetLib.sprx` and `../data/qDNSm-server-games.ini`, so build
the plugin and its network module first — they are separate SNC/Visual Studio
projects (`qDNSm.sln`).

`APPID` and `CONTENTID` are set in the `Makefile` and must agree with each other and
with the `PARAM.SFO` the build generates.

## Before changing anything here

The toolchain and RSX facts below each cost a console reboot to establish, and several
look like obvious bugs but are not:

- a **single** display buffer tears; more than one is required
- the flip sequence must match `vendor/PSL1GHT/samples/sys/save` exactly
- `sysUtilCheckCallback()` is required or dialogs never receive input
- RPCS3 cannot load an NPDRM self at all, so it cannot test this artifact

## Testing

The host tests cover the file and `boot_plugins.txt` logic only. They pass whether or
not the dialogs work, and they cannot see RSX, rendering or input — all of that is
observable only on hardware or in RPCS3, and RPCS3 is not a valid test for the signed
EBOOT for the reason above.