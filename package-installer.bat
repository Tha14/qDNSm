@echo off
setlocal enabledelayedexpansion

rem ============================================================================
rem  Packages the installer. Builds nothing, signs nothing, needs no Docker.
rem
rem  Takes the binaries already on disk and puts them in a PKG:
rem
rem    bin\Release-PS3\qDNSm.sprx            ->  USRDIR/qDNSm.sprx
rem    bin\Release-PS3\qDNSmNetLib.sprx      ->  USRDIR/qDNSmNetLib.sprx
rem    data\qDNSm-server-games.ini           ->  USRDIR/qDNSm-server-games.ini
rem    bin\Release-PS3\installer\EBOOT.BIN    ->  USRDIR/EBOOT.BIN
rem    packaging\assets\ICON0.PNG            ->  ICON0.PNG
rem    packaging\assets\PARAM.SFO            ->  PARAM.SFO
rem
rem  Requirements: Python 3 on PATH. That is all.
rem
rem  The EBOOT.BIN is already signed and this does NOT re-sign it. Do not add a
rem  make_self_npdrm call here: that skips the strip and sprx_tool steps the real
rem  build performs and produces an EBOOT that will not load.
rem
rem  Usage:  package-installer.bat
rem ============================================================================

set "ROOT=%~dp0"
if "%ROOT:~-1%"=="\" set "ROOT=%ROOT:~0,-1%"

set "SPRX=%ROOT%\bin\Release-PS3\qDNSm.sprx"
set "NETLIB=%ROOT%\bin\Release-PS3\qDNSmNetLib.sprx"
set "GAMES=%ROOT%\data\qDNSm-server-games.ini"
set "EBOOT=%ROOT%\bin\Release-PS3\installer\EBOOT.BIN"
set "ASSETS=%ROOT%\packaging\assets"
set "MAKEPKG=%ROOT%\packaging\make-pkg.py"
set "PKG_OUT=%ROOT%\bin\Release-PS3\qDNSm-Installer.pkg"
set "STAGE=%TEMP%\qdnsm-pkg"

set "APPID=NPUB72381"
set "CONTENTID=UP0001-%APPID%_00-0000000000000000"

echo.
echo ============================================================
echo   qDNSm installer - packaging
echo ============================================================
echo.

where python >nul 2>&1 || (
    echo ERROR: python not found on PATH. Install Python 3.
    goto :fail
)
if not exist "%SPRX%"   ( echo ERROR: missing %SPRX% & echo        Build the plugin first.   & goto :fail )
if not exist "%NETLIB%" ( echo ERROR: missing %NETLIB% & echo        Build the network module first. & goto :fail )
if not exist "%GAMES%"  ( echo ERROR: missing %GAMES% & goto :fail )
if not exist "%EBOOT%"  ( echo ERROR: missing %EBOOT% & echo        Build the installer EBOOT first. & goto :fail )
if not exist "%ASSETS%\ICON0.PNG" ( echo ERROR: missing %ASSETS%\ICON0.PNG & goto :fail )
if not exist "%ASSETS%\PARAM.SFO" ( echo ERROR: missing %ASSETS%\PARAM.SFO & goto :fail )
if not exist "%MAKEPKG%" ( echo ERROR: missing %MAKEPKG% & goto :fail )

rem The console reads TITLE_ID out of PARAM.SFO and compares it against the
rem content ID inside the EBOOT. A mismatch here is silent until it is not:
rem the checked-in SFO sat at the pre-rename title for a long time.
python -c "import sys; sys.exit(0 if sys.argv[2].encode() in open(sys.argv[1],'rb').read() else 1)" "%ASSETS%\PARAM.SFO" "%APPID%"
if errorlevel 1 (
    echo ERROR: %ASSETS%\PARAM.SFO does not mention %APPID%.
    echo        It must match the title ID inside the EBOOT, or the console
    echo        will reject the package. Regenerate it with sfo.py, or correct
    echo        it by hand.
    goto :fail
)

echo   qDNSm.sprx     !SPRX:~%ROOT%!
for %%A in ("%SPRX%") do echo                   %%~zA bytes
echo   qDNSmNetLib.sprx !NETLIB:~%ROOT%!
for %%A in ("%NETLIB%") do echo                   %%~zA bytes
echo   games list     !GAMES:~%ROOT%!
for %%A in ("%GAMES%") do echo                   %%~zA bytes
echo   EBOOT.BIN      !EBOOT:~%ROOT%!
for %%A in ("%EBOOT%") do echo                   %%~zA bytes
echo   TITLE_ID       %APPID%
echo.

rem --- stage -----------------------------------------------------------------
if exist "%STAGE%" rmdir /s /q "%STAGE%"
mkdir "%STAGE%\USRDIR" >nul 2>&1
copy /y "%SPRX%"  "%STAGE%\USRDIR\qDNSm.sprx" >nul || goto :copyfail
copy /y "%NETLIB%" "%STAGE%\USRDIR\qDNSmNetLib.sprx" >nul || goto :copyfail
copy /y "%GAMES%" "%STAGE%\USRDIR\qDNSm-server-games.ini" >nul || goto :copyfail
copy /y "%EBOOT%" "%STAGE%\USRDIR\EBOOT.BIN"  >nul || goto :copyfail
copy /y "%ASSETS%\ICON0.PNG" "%STAGE%\ICON0.PNG" >nul || goto :copyfail
copy /y "%ASSETS%\PARAM.SFO" "%STAGE%\PARAM.SFO" >nul || goto :copyfail

rem --- package ---------------------------------------------------------------
rem The trailing slash on the staging directory is required: pkg.py derives each
rem entry's in-package path by stripping the staging prefix off the full path.
echo   Building PKG...
python "%MAKEPKG%" --no-stage --contentid=%CONTENTID% "%STAGE%/" "%PKG_OUT%"
if errorlevel 1 (
    echo.
    echo ERROR: packaging failed - see the output above.
    rmdir /s /q "%STAGE%"
    goto :fail
)

rmdir /s /q "%STAGE%"
if not exist "%PKG_OUT%" (
    echo ERROR: packaging finished but no package was produced.
    goto :fail
)

echo.
echo ============================================================
echo   Done
echo ============================================================
echo   %PKG_OUT%
for %%A in ("%PKG_OUT%") do echo   %%~zA bytes
echo.
echo   The package is not byte-reproducible - re-running on unchanged inputs
echo   still gives a different file.
echo.
endlocal
exit /b 0

:copyfail
echo ERROR: could not stage the files into %STAGE%
rmdir /s /q "%STAGE%"
goto :fail

:fail
echo.
echo Packaging failed.
endlocal
exit /b 1