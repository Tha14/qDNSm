# vendor

Third-party components, committed directly rather than as git submodules.

`VirtualShell` came from https://github.com/TheRouletteBoi/VirtualShell.git

It used to be declared in `.gitmodules`, but the directory was never a real submodule checkout -
there was no nested `.git`, so the declaration was inert and would have left the repo with a
broken gitlink that `git submodule update --init` could not resolve.

These are committed as plain files on purpose: the build needs the stub libraries and headers
directly, and a self-contained repo always builds without a network fetch or an init step.
`PSL1GHT`, `scetool`, `securetool` and `sysutil` came from the Sony SDK.

`makepkg` (`makepkg1.exe`, `makepkg2.exe`, `do.bat`) is a PS3 homebrew packaging tool. It is not
used to build the plugin or the installer EBOOT - only to wrap the finished `EBOOT.BIN` into an
installable PKG. Our wrapper and the differences from the stock `do.bat` are documented in
`packaging/README.md`; `do.bat` itself is left untouched as shipped.

`securetool` is no longer invoked by the build. It is a retail PKI signer and was being used to
write a commercial `APP001` signature into the file named `qDNSm_HEN.sprx` - the one artifact that
therefore could not load on HEN. See `scetool/SIGNING.md`.

## mbedTLS

`mbedtls` is **mbedTLS 2.28.10**, the last LTS release on the 2.x API. Only `include/`,
`library/`, `LICENSE`, `README.md` and `ChangeLog` are kept; tests, programs, scripts and the
upstream documentation tree are removed. The module compiles the C files it needs from
`vendor/mbedtls/library` directly - there is no prebuilt library.

It is deliberately **ignored by git** (see `.gitignore`): 6.3 MB of unmodified upstream source is
re-obtainable from a single download, and nothing here patches it. The configuration is ours and
lives in `src/Net/mbedtls_config.h`; upgrades must be re-checked against that file and against the
PS3 entropy and socket hooks in `src/Net`.

Source (SHA-256 `19e5b81fdac0fe22009b9e2bdcd52d7dcafbf62bc67fc59cf0a76b5b5540d149`):

```text
https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-2.28.10/mbedtls-2.28.10.tar.bz2
```

Restore from a fresh clone:

```bash
curl -L -o /tmp/mbedtls-2.28.10.tar.bz2 \
  https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-2.28.10/mbedtls-2.28.10.tar.bz2
sha256sum /tmp/mbedtls-2.28.10.tar.bz2
mkdir -p vendor/mbedtls
tar -xjf /tmp/mbedtls-2.28.10.tar.bz2 -C vendor/mbedtls --strip-components=1 \
  mbedtls-2.28.10/include mbedtls-2.28.10/library \
  mbedtls-2.28.10/LICENSE mbedtls-2.28.10/README.md mbedtls-2.28.10/ChangeLog
```

Do not move to mbedTLS 3.x casually: it replaces the 2.x API with PSA crypto, and the module's
C-only configuration and entropy callback are written for 2.28.
