# src/Net

`qDNSmNetLib.sprx` is the update worker. It is loaded only while an update is
running, performs the HTTP/TLS downloads, and is unloaded again by the main
plugin. It must not import VSH, paf, or the C++ standard library.

## Files

| File | Role |
|---|---|
| `qDNSmNetLib.cpp` | Module entry points and update dispatch. |
| `TlsFetch.cpp` / `.hpp` | TLS 1.2 handshake, HTTPS GET, body decode, temp-file write. |
| `qDNSmNetLib.vcxproj` | SNC project. Builds and signs to `bin/<Config>-PS3/qDNSmNetLib.sprx`. |
| `NetProbeProtocol.hpp` | Request/response struct shared with the main plugin. |
| `UpdateUrls.hpp` | DNS and game-list URLs. |
| `mbedtls_config.h` | Minimal mbedTLS configuration for the module. |

## mbedTLS

The module uses **mbedTLS 2.28.10** from `vendor/mbedtls`. That tree is
gitignored; the restore command, SHA-256, and the reason for staying on the
2.28 LTS line are in `vendor/README.md`.

The C sources are compiled directly into this project. Do not add a prebuilt
mbedTLS library. The configuration belongs in `src/Net/mbedtls_config.h`, not
in `vendor/mbedtls`.

## Build

Focused build from the Windows VM:

```bat
set SCE_PS3_ROOT=C:/usr/local/cell
set SN_PS3_PATH=C:\Program Files (x86)\SN Systems\PS3
MSBuild.exe Z:\qDNSm\src\Net\qDNSmNetLib.vcxproj ^
  -p:Configuration=Release -p:Platform=PS3 -m:1
```

The project uses paths relative to its own directory, so it does not need
`SolutionDir` and writes to `bin/Release-PS3/`. It also signs the output with
the same scetool flags as the main plugin.

## Update flow

`Core/NetUpdate.cpp` loads the module on its worker thread and starts it with
the update job. The module fetches `url`/`outputPath` and, when `url2` is
non-empty, `url2`/`outputPath2` in one instance, so the loader is cycled once
per update rather than once per file. It writes each body to the requested
path and fills in the stage/error/status/bytes fields for each fetch.

Both files live in the plugin directory:

```text
/dev_hdd0/plugins/qDNSm.sprx
/dev_hdd0/plugins/qDNSmNetLib.sprx
```

`NetUpdate` validates the DNS list (`DnsList`) and the game catalogue
(`GameList`) before applying either. A failed fetch or an invalid file leaves
the previous file in place. No diagnostic files are written.

The module links mbedTLS from source (no prebuilt library), uses a 256 KiB
static memory-buffer arena, and gets entropy from `sys_get_random_number`.
The handshake currently uses `MBEDTLS_SSL_VERIFY_NONE`, so the certificate is
parsed but not verified. `SO_NBIO` plus `socketselect` bounds the TCP connect
to 5 s; blocking send/receive time out after 10 s.
