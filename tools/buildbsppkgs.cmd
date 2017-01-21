@echo off
goto START

:Usage
echo Usage: buildbsppkgs [Release]/[Debug] [version]
echo    Release................... Picks the release binaries
echo    Debug..................... Picks the debug binaries
echo        One of the above should be specified
echo    [version]................. Package version.
echo    [/?]...................... Displays this usage string.
echo    Example:
echo        buildbsppkgs Release 10.0.1.0
echo        buildbsppkgs Debug 10.0.1.0


exit /b 1

:START

pushd
setlocal ENABLEDELAYEDEXPANSION

if not defined PKG_CONFIG_XML (
    call SetupBuildEnv.cmd
    )
set SIGN_OEM=1
set SIGN_WITH_TIMESTAMP=0
set RELEASE_DIR=%OUTPUT_DIR%\%1


if not exist %PKGLOG_DIR% ( mkdir %PKGLOG_DIR% )

REM Input validation
if [%1] == [/?] goto Usage
if [%1] == [-?] goto Usage
if [%1] == [] goto Usage
if [%2] == [] goto Usage



echo Building all packages under C:\github\bsp\
dir C:\github\bsp\*.pkg.xml /S /b > %PKGLOG_DIR%\packagelist.txt

call :SUB_PROCESSLIST %PKGLOG_DIR%\packagelist.txt %2

if exist %PKGLOG_DIR%\packagelist.txt ( del %PKGLOG_DIR%\packagelist.txt )
del %PKGBLD_DIR%\*.spkg >nul
endlocal
popd
exit /b

REM -------------------------------------------------------------------------------
REM
REM SUB_PROCESSLIST <filename>
REM
REM Processes the file list, calls pkggen for each item in the list
REM
REM -------------------------------------------------------------------------------
:SUB_PROCESSLIST

for /f "delims=" %%i in (%1) do (
   echo. Processing %%~nxi
   cd /D %%~dpi
   echo Creating %%i Package with version %PKG_VER%  > %PKGLOG_DIR%\%%~ni.log

    call pkggen.exe "%%i" /config:"%PKG_CONFIG_XML%" /output:"%PKGBLD_DIR%" /version:%2 /build:fre /cpu:ARM /variables:"_RELEASEDIR=%RELEASE_DIR%\;BSPARCH=%BSP_ARCH%;OEMNAME=%OEM_NAME%" /onecore >> %PKGLOG_DIR%\%%~ni.log
   
   if not errorlevel 0 ( echo. Error : Failed to create package. See %PKGLOG_DIR%\%%~ni.log )
   
)
exit /b