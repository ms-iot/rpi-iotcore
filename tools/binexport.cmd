@echo off
REM Export Script for exporting required binary files only
goto START

:Usage
echo Usage: binexport [Release]/[Debug] [TargetDir]
echo    Release................... Picks the release binaries
echo    Debug..................... Picks the debug binaries
echo        One of the above should be specified
echo    [TargetDir]............... Directory to export the files.
echo    [/?]...................... Displays this usage string.
echo    Example:
echo        binexport Release C:\bsp_v1.0_release
echo        binexport Debug C:\bsp_v1.0_debug


exit /b 1

:START

REM Input validation
if [%1] == [/?] goto Usage
if [%1] == [-?] goto Usage
if [%1] == [] goto Usage
if [%2] == [] goto Usage

pushd
setlocal ENABLEDELAYEDEXPANSION

REM
REM Set source root
REM
set REPO_BUILD_TOOL=%~dp0
cd /d "%REPO_BUILD_TOOL%.."
set REPO_SOURCE_ROOT=%cd%\

set OUTPUT_DIR=%REPO_SOURCE_ROOT%\build\bcm2836\ARM
set BINTYPE=%1
set TDIR=%2\

if not exist %TDIR% ( mkdir %TDIR% )
if not exist %OUTPUT_DIR%\%BINTYPE% (
    echo %BINTYPE% directory not found. Do %BINTYPE% build
    goto usage
    )

REM Export the built binaries
copy %OUTPUT_DIR%\%BINTYPE%\*.inf %TDIR% > nul
copy %OUTPUT_DIR%\%BINTYPE%\*.sys %TDIR% > nul
copy %OUTPUT_DIR%\%BINTYPE%\*.dll %TDIR% > nul

REM Export the pkgxml files
for /f "delims=" %%i in ('dir /s /b %REPO_SOURCE_ROOT%\drivers\*.pkg.xml') do (
    copy %%i %TDIR% > nul
)
copy %REPO_SOURCE_ROOT%\boards\bcm2836\Packages\SV.PlatExtensions\SV.PlatExtensions.pkg.xml %TDIR% > nul
copy %REPO_SOURCE_ROOT%\boards\bcm2836\Packages\build.txt %TDIR%\build.cmd > nul

popd
exit /b


