@echo off

set game=
set ptch=

for /d %%v in (NP???????) do set game=%%v
for /d %%v in (BL???????) do set ptch=%%v
for /d %%v in (BC???????) do set ptch=%%v

if "%game%" equ "" goto failure
if "%ptch%" equ "" goto failure

echo Found folders
echo  Game: %game%
echo  Patch: %ptch%

echo.

echo Creating GAME pkg...

echo contentid = 000000-%game:~0,9%_00-1GAME00000000000>package.conf
echo klicensee = 0x00000000000000000000000000000000>>package.conf
echo drmtype = Free>>package.conf
echo contenttype = GameExec>>package.conf
echo packagetype = HDDGamePatch>>package.conf
echo installdirectory = %game%>>package.conf
echo packageversion = 01.00>>package.conf

makepkg1.exe -n package.conf %game%>nul

if exist %game%\LICDIR xcopy "%game%\LICDIR\*.*" "%game%-LIC\LICDIR\*.*">nul
if exist %game%\INSDIR xcopy "%game%\INSDIR\*.*" "%game%-LIC\INSDIR\*.*">nul

echo Creating PATCH pkg...

echo contentid = 000000-%game:~0,9%_00-2PATCH0000000000>package.conf
echo klicensee = 0x00000000000000000000000000000000>>package.conf
echo drmtype = Free>>package.conf
echo contenttype = GameData>>package.conf
echo packagetype = DiscGamePatch>>package.conf
echo installdirectory = %ptch%>>package.conf
echo packageversion = 01.00>>package.conf

makepkg1.exe -n package.conf %ptch%>nul

if exist %game%-LIC (
	echo Creating LIC pkg...
	
	echo contentid = 000000-%game:~0,9%_00-3LIC000000000000>package.conf
	echo klicensee = 0x00000000000000000000000000000000>>package.conf
	echo drmtype = Free>>package.conf
	echo contenttype = GameExec>>package.conf
	echo packagetype = HDDGamePatch>>package.conf
	echo installdirectory = %game%>>package.conf
	echo packageversion = 01.00>>package.conf
	
	makepkg2.exe -n package.conf %game%-LIC>nul
	
	rd /S /Q "%game%-LIC">nul
)

del package.conf>nul

goto success

:success

echo.

echo Successfully created pkgs.

goto end

:failure

echo Some folders are missing.

goto end

:end

echo Press any key to exit.

pause>nul
