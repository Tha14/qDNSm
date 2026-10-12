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
