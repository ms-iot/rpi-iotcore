::
::  Copyright (c) Microsoft Corporation. All rights reserved.
::
@echo off

REM
REM Set source root
REM
set REPO_BUILD_TOOL=%~dp0
cd /d "%REPO_BUILD_TOOL%.."
set REPO_BUILD_ROOT=%cd%\
cd /d "%REPO_BUILD_ROOT%.."
set REPO_SOURCE_ROOT=%cd%\

REM
REM Reused the same script to reliable find ADK install location
REM Query the 32-bit and 64-bit Registry hive for KitsRoot
REM
set regKeyPathFound=1
set wowRegKeyPathFound=1
set KitsRootRegValueName=KitsRoot10

REG QUERY "HKLM\Software\Wow6432Node\Microsoft\Windows Kits\Installed Roots" /v %KitsRootRegValueName% 1>NUL 2>NUL || set wowRegKeyPathFound=0
REG QUERY "HKLM\Software\Microsoft\Windows Kits\Installed Roots" /v %KitsRootRegValueName% 1>NUL 2>NUL || set regKeyPathFound=0

if %wowRegKeyPathFound% EQU 0 (
  if %regKeyPathFound% EQU 0 (
    @echo KitsRoot not found, can't set common path for Deployment Tools
    goto :EOF 
  ) else (
    set regKeyPath=HKLM\Software\Microsoft\Windows Kits\Installed Roots
  )
) else (
    set regKeyPath=HKLM\Software\Wow6432Node\Microsoft\Windows Kits\Installed Roots
)

  
FOR /F "skip=2 tokens=2*" %%i IN ('REG QUERY "%regKeyPath%" /v %KitsRootRegValueName%') DO (set KitsRoot=%%j)

REM
REM Setup basic enviroment path
REM
set PATH=%KITSROOT%tools\bin\i386;%REPO_BUILD_TOOL%;%PATH%
set AKROOT=%KITSROOT%
set WPDKCONTENTROOT=%KITSROOT%
set PKG_CONFIG_XML=%KITSROOT%Tools\bin\i386\pkggen.cfg.xml
set OEM_NAME=Contoso

set OUTPUT_DIR=%REPO_BUILD_ROOT%\build\bcm2836\ARM
set PKGBLD_DIR=%REPO_BUILD_ROOT%\build\bcm2836\ARM\pkgs
set PKGLOG_DIR=%REPO_BUILD_ROOT%\build\bcm2836\ARM\logs


REM
REM Setup Deployment and Imaging Tools Environment
REM
call "%KITSROOT%\Assessment and Deployment Kit\Deployment Tools\DandISetEnv.bat"



