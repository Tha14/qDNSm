@echo off
REM ===========================================================================
REM  make-installer-pkg.bat - build the qDNSm Installer PKG
REM
REM  ---------------------------------------------------------------------
REM  PACKAGING IS DONE IN PYTHON, NOT BY vendor\makepkg
REM  ---------------------------------------------------------------------
REM  vendor/makepkg/makepkg1.exe is a build of make_package_npdrm. Every mode
REM  of it is NPDRM packaging: it demands a real NPDRM SELF with valid EDATA
REM  digests and rejects ours with
REM
REM      >> !ERROR!: NPUB72381/USRDIR/EBOOT.BIN is not an NPDRM SELF.
REM
REM  That is not a defect in our EBOOT and no flag changes it. qDNSm is a 3.55
REM  homebrew fake self, signed so it loads on CFW and HEN. NPDRM signing needs
REM  Sony's NPDRM content keys, which exist nowhere outside Sony. Homebrew and
REM  NPDRM are mutually exclusive - a packager does one or the other.
REM
REM  packaging\make-pkg.py wraps vendor\PSL1GHT\tools\ps3py\pkg.py, which is what
REM  pkgi-ps3 uses to package its own EBOOT. pkg.py never opens the EBOOT: it
REM  stats it for size, tags the entry as NPDRM in the PKG flags, and writes a
REM  hardcoded digest. See packaging\README.md.
REM
REM  ---------------------------------------------------------------------
REM  BATCH RULES THIS FILE FOLLOWS - do not "tidy" these away.
REM
REM    * No goto inside a "( ... )" block. "goto :failed" nested in an if block
REM      has failed to resolve its own label in this script and aborted with
REM      "The system cannot find the batch label specified - failed". Every
REM      goto here is at top level; failures are accumulated in %ERR% and
REM      dispatched once.
REM
REM    * No parentheses in any echo string, and no if/else blocks. A stray
REM      "(" or ")" inside a block re-splits the command and the leftover text
REM      runs as a command.
REM
REM    * No "|" in any echo string. It becomes a pipe and the right-hand side
REM      is executed as a command name.
REM
REM    * Every prose line starts with REM. One that did not - a wrapped comment
REM      whose continuation line lost its "REM " prefix - ran as
REM      "PARAM.SFO, so nothing is lost by dropping it." and produced
REM      "'PARAM.SFO' is not recognized as an internal or external command".
REM
REM  Usage:  make-installer-pkg.bat [Configuration] [Platform]
REM          defaults: Release PS3
REM
REM  Exits 0 even when packaging fails. This runs from a post-build step, and a
REM  failed package should not be able to block a successful code build. The
REM  failure is printed loudly instead.
REM ===========================================================================

setlocal

set "TITLE_ID=NPUB72381"
REM Content ID format is NOT cosmetic. The PS3 DRM layer validates it and rejects
REM a malformed one with SCE_NP_DRM_ERROR_FORMAT (0x80029530), which the XMB
REM shows as "Corrupted Data".
REM
REM The final field must be exactly 16 hex digits. The previous value was
REM
REM     000000-NPUB72381_00-1GAME00000000000
REM
REM which is wrong twice over: 'G' and 'M' are not hex digits, and '000000' is
REM not a valid discriminator. UP0001 is what marks an installable package; the
REM trailing field is zeroes. Total length is 36 characters, matching pkgi-ps3,
REM which packages a working EBOOT with exactly this shape.
set "CONTENT_ID=UP0001-%TITLE_ID%_00-0000000000000000"
set "PKG_VERSION=01.00"
set "PKG_LABEL=qDNSm-Installer"

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"
set "PLATFORM=%~2"
if "%PLATFORM%"=="" set "PLATFORM=PS3"

set "ROOT=%~dp0.."
set "OUT_DIR=%ROOT%\bin\%CONFIG%-%PLATFORM%\installer"
REM The plugin build writes its signed payload to the bin root, not to the
REM installer folder. Keep both names here so the payload can be copied into
REM the installer output tree before staging.
set "SPRX=%ROOT%\bin\%CONFIG%-%PLATFORM%\qDNSm.sprx"
set "PRX=%ROOT%\bin\%CONFIG%-%PLATFORM%\qDNSm.prx"
set "NETLIB=%ROOT%\bin\%CONFIG%-%PLATFORM%\qDNSmNetLib.sprx"
set "NETLIBPRX=%ROOT%\bin\%CONFIG%-%PLATFORM%\qDNSmNetLib.prx"
set "GAMES=%ROOT%\data\qDNSm-server-games.ini"
set "WORK=%OUT_DIR%\pkgwork"
set "STAGE=%WORK%\%TITLE_ID%"
set "USRDIR=%STAGE%\USRDIR"
set "MAKERY=%~dp0make-pkg.py"
set "LOG=%WORK%\make-pkg.log"
set "ERR="

echo.
echo === qDNSm installer PKG =========================================
echo   configuration : %CONFIG%-%PLATFORM%
echo   title id      : %TITLE_ID%
echo.

REM --- preflight -------------------------------------------------------------
REM Each check only sets ERR if it is still empty, so the first real problem
REM is the one reported.

REM EBOOT.BIN has a size floor, and it exists because scetool reports success
REM while writing an empty shell.
REM
REM Handed something that is already a SELF - "SCE\0" magic - scetool finds no
REM raw ELF to wrap. It still prints SELF built / Data compressed / Data
REM encrypted / written, and emits a few hundred bytes of header with no payload.
REM So "EBOOT.BIN written" proves nothing. Size does. The installer module is
REM around 10 KB, so anything under 8 KB is a stub.
REM A Linux installer build leaves the signed EBOOT under installer_src\build.
REM Bring it into the installer output tree when the Windows build did not
REM produce one, so this script packages the compiled EBOOT regardless of
REM which platform built it.
if not exist "%OUT_DIR%" if exist "%ROOT%\installer_src\build\pkg\USRDIR\EBOOT.BIN" mkdir "%OUT_DIR%" >nul 2>&1
if not exist "%OUT_DIR%\EBOOT.BIN" if exist "%ROOT%\installer_src\build\pkg\USRDIR\EBOOT.BIN" copy /Y "%ROOT%\installer_src\build\pkg\USRDIR\EBOOT.BIN" "%OUT_DIR%\EBOOT.BIN" >nul 2>&1

set "EBOOTSIZE=0"
if exist "%OUT_DIR%\EBOOT.BIN" for %%f in ("%OUT_DIR%\EBOOT.BIN") do set "EBOOTSIZE=%%~zf"

if not exist "%OUT_DIR%\EBOOT.BIN" set "ERR=EBOOT.BIN missing - did the installer project build?"
if %EBOOTSIZE% LSS 8192 if not defined ERR set "ERR=EBOOT.BIN is only %EBOOTSIZE% bytes - scetool wrote an empty SELF shell. See vendor\scetool\SIGNING.md."

if not exist "%SPRX%" if not defined ERR set "ERR=qDNSm.sprx missing - did the qDNSm project build?"
if not exist "%NETLIB%" if not defined ERR set "ERR=qDNSmNetLib.sprx missing - did the qDNSmNetLib project build?"
if not exist "%GAMES%" if not defined ERR set "ERR=qDNSm-server-games.ini missing at %GAMES%"
if not exist "%~dp0assets\ICON0.PNG" if not defined ERR set "ERR=ICON0.PNG missing at %~dp0assets\ICON0.PNG"
if not exist "%~dp0assets\PARAM.SFO" if not defined ERR set "ERR=PARAM.SFO missing at %~dp0assets\PARAM.SFO"
if not exist "%MAKERY%" if not defined ERR set "ERR=make-pkg.py not found at %MAKERY%"

REM --- is the payload current? -------------------------------------------------
REM This exists because it has actually gone wrong. The solution declares the
REM installer as depending on qDNSm, but with parallel project builds the two
REM post-build steps still interleave, and the installer has been observed
REM packaging a qDNSm.sprx from a previous build:
REM
REM     17:55:35  bin\Release-PS3\installer\qDNSm.sprx   <- what got packaged
REM     18:08:07  qDNSm-Installer.pkg
REM     18:08:12  bin\Release-PS3\qDNSm.sprx             <- five seconds LATER
REM
REM 98.3% of the package's bytes differed from the sprx on disk. It was a valid
REM but stale payload, and nothing in the build said so.
REM
REM So: if the sprx is older than the prx it was signed from, it predates the
REM current code and the package would ship last build's plugin.
REM
REM Compared via PowerShell because cmd has no reliable way to order two file
REM timestamps: FOR's date modifiers are locale-dependent and would silently
REM misbehave across a date boundary. Do not name those modifiers anywhere above
REM either - cmd expands a tilde-parameter even inside a REM, which breaks the
REM script outright.
set "STALE="
if exist "%SPRX%" if exist "%PRX%" for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "if ((Get-Item -LiteralPath '%SPRX%').LastWriteTime -lt (Get-Item -LiteralPath '%PRX%').LastWriteTime) { 'STALE' }" 2^>nul`) do set "STALE=%%i"
if defined STALE if not defined ERR set "ERR=qDNSm.sprx is older than qDNSm.prx - the two projects built in parallel and this would package last build's plugin. Build the solution again, or build the qDNSm project first, then the installer."

set "STALENET="
if exist "%NETLIB%" if exist "%NETLIBPRX%" for /f "usebackq delims=" %%i in (`powershell -NoProfile -Command "if ((Get-Item -LiteralPath '%NETLIB%').LastWriteTime -lt (Get-Item -LiteralPath '%NETLIBPRX%').LastWriteTime) { 'STALE' }" 2^>nul`) do set "STALENET=%%i"
if defined STALENET if not defined ERR set "ERR=qDNSmNetLib.sprx is older than qDNSmNetLib.prx - this would package last build's network module. Build the qDNSmNetLib project again, then the installer."

REM Both the qDNSm plugin and the installer EBOOT are signed by scetool from
REM vendor\scetool\bin\data\keys, so both are signed with the same key set. That
REM set has to be the 3.55-and-lower homebrew one, because that is what PS3HEN
REM accepts.
REM
REM scetool does not complain if that store is wrong. It signs happily and the
REM result simply will not load, which is the same silent failure SecureTool
REM caused before it was removed. So the store is checked here instead, by the
REM one line that distinguishes a homebrew key set from a retail one: a retail
REM NPD store has a real private key under [NP_sig], a homebrew store has a
REM zeroed one.
set "KEYS=%ROOT%\vendor\scetool\bin\data\keys"
if not exist "%KEYS%" set "ERR=scetool key store missing at %KEYS%"
if not defined ERR findstr /C:"priv=000000000000000000000000000000000000000000" "%KEYS%" >nul 2>&1
if not defined ERR if errorlevel 1 set "ERR=scetool\bin\data\keys is not a homebrew key set - a retail store would produce an EBOOT that HEN cannot load"

REM --- find a python ---------------------------------------------------------
REM Prefer the py launcher, which can ask for a specific version. "py -3" is
REM stored as a two-word command, so it is kept in one variable and expanded
REM unquoted later - quoting it would pass "-3" as a single argument.
set "PY="
where py.exe >nul 2>&1
if not errorlevel 1 set "PY=py -3"
if not defined PY where python.exe >nul 2>&1
if not defined PY if not errorlevel 1 set "PY=python"
if not defined PY where python3.exe >nul 2>&1
if not defined PY if not errorlevel 1 set "PY=python3"

if not defined PY set "ERR=no Python found. Install Python 3, or run 'py -3', 'python' or 'python3' on the PATH."

REM Prove the interpreter actually runs before doing any staging. A wrong py.exe
REM on the PATH that cannot execute produces a confusing error much later.
if not defined ERR %PY% -c "import sys" >nul 2>&1
if not defined ERR if errorlevel 1 set "ERR=%PY% is on the PATH but does not run. Check the Python install."

if defined ERR goto :failed

%PY% -c "import sys; print(sys.version)" > "%LOG%" 2>&1
for /f "tokens=*" %%v in ('findstr /B "Python" "%LOG%"') do echo   python        : %%v

REM --- stage the title -------------------------------------------------------
if exist "%WORK%" rd /S /Q "%WORK%" >nul 2>&1
mkdir "%USRDIR%" >nul 2>&1
if not exist "%USRDIR%" set "ERR=could not create staging directory %USRDIR%"
if defined ERR goto :failed

REM Refresh the installer-folder payload from the plugin build before staging.
REM The installer project can finish before qDNSm.sprx is re-signed, which is
REM exactly how a previous build's plugin ended up in the package.
copy /Y "%SPRX%" "%OUT_DIR%\qDNSm.sprx" >nul
if errorlevel 1 set "ERR=could not copy %SPRX% to %OUT_DIR%\qDNSm.sprx"
if defined ERR goto :failed

copy /Y "%OUT_DIR%\EBOOT.BIN" "%USRDIR%\EBOOT.BIN" >nul
if errorlevel 1 set "ERR=could not stage EBOOT.BIN"
if defined ERR goto :failed

copy /Y "%OUT_DIR%\qDNSm.sprx" "%USRDIR%\qDNSm.sprx" >nul
if errorlevel 1 set "ERR=could not stage qDNSm.sprx"
if defined ERR goto :failed

copy /Y "%NETLIB%" "%USRDIR%\qDNSmNetLib.sprx" >nul
if errorlevel 1 set "ERR=could not stage qDNSmNetLib.sprx"
if defined ERR goto :failed

copy /Y "%GAMES%" "%USRDIR%\qDNSm-server-games.ini" >nul
if errorlevel 1 set "ERR=could not stage qDNSm-server-games.ini"
if defined ERR goto :failed

copy /Y "%~dp0assets\ICON0.PNG" "%STAGE%\ICON0.PNG" >nul
if errorlevel 1 set "ERR=could not stage ICON0.PNG"
if defined ERR goto :failed

REM The hand-made PARAM.SFO is the source of truth: it carries CATEGORY=HG and
REM BOOTABLE=1, which is what makes the XMB list this as a launchable app rather
REM than a dead icon.
copy /Y "%~dp0assets\PARAM.SFO" "%STAGE%\PARAM.SFO" >nul
if errorlevel 1 set "ERR=could not stage PARAM.SFO"
if defined ERR goto :failed

echo   PARAM.SFO    : hand-made, from packaging\assets
echo   staging dir  : %STAGE%
echo.

REM --- build -----------------------------------------------------------------
REM Run from %WORK% and pass the title folder as a RELATIVE path with a trailing
REM slash. Two reasons, both of which produce a broken package otherwise:
REM
REM   * pkg.py derives each entry's in-package name by normalising separators and
REM     then stripping the staging prefix off the full path. An absolute Windows
REM     path does not survive that - the prefix no longer matches after the
REM     backslashes are turned into forward slashes.
REM   * Without the trailing slash, every name keeps its leading slash, and
REM     os.path.join then discards the base and looks for /ICON0.PNG at
REM     filesystem root.
REM
REM PSL1GHT's own ppu_rules passes $(BUILDDIR)/pkg/ for the same reason.
REM
REM Output path is absolute because the cwd has moved.
pushd "%WORK%"
%PY% "%MAKERY%" --no-stage --contentid=%CONTENT_ID% %TITLE_ID%/ "%OUT_DIR%\%PKG_LABEL%.pkg" > "%LOG%" 2>&1
set "RC=%ERRORLEVEL%"
popd

type "%LOG%"
echo.

if not "%RC%"=="0" set "ERR=make-pkg.py exited with %RC% - see %LOG%"
if defined ERR goto :failed

if not exist "%OUT_DIR%\%PKG_LABEL%.pkg" set "ERR=make-pkg.py reported success but no .pkg appeared - see %LOG%"
if defined ERR goto :failed

REM A package without the EBOOT installs cleanly and does nothing when launched,
REM which is the worst failure mode there is - it looks like success. So the
REM output is verified to contain a real EBOOT rather than trusting the exit code.
for %%f in ("%OUT_DIR%\%PKG_LABEL%.pkg") do set "PKGSIZE=%%~zf"
if not exist "%OUT_DIR%\%PKG_LABEL%.pkg" set "PKGSIZE=0"

REM PARAM.SFO alone is about 1 KB and ICON0.PNG about 95 KB, so a package under
REM 100 KB cannot contain the sprx and the EBOOT as well.
if %PKGSIZE% LSS 102400 if not defined ERR set "ERR=package is only %PKGSIZE% bytes - expected over 100 KB with the icon, the payload and the EBOOT inside"

if defined ERR goto :failed

echo   package      : %OUT_DIR%\%PKG_LABEL%.pkg
echo   size         : %PKGSIZE% bytes
echo.
echo === done ============================================================
echo.
endlocal
exit /b 0

:failed
echo.
echo [FAIL] %ERR%
echo.
echo   Left in place on purpose so it can be diffed:
echo     log      : %LOG%
echo     staging  : %STAGE%
echo.
echo   The compiled EBOOT.BIN is still in:
echo     %OUT_DIR%
echo.
endlocal
exit /b 0
