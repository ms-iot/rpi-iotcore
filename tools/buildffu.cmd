REM Build Image
@echo off
goto START

:Usage
echo Usage: buildffu [Retail]/[Test] 
echo    Retail........ Processes the RetailOEMInput.xml
echo    Test.......... Processes the TestOEMInput.xml
echo    Output FFU  : %OUTPUT_DIR%\FFU-Retail\RPiRetail.FFU for Retail
echo    Output logs : %PKGLOG_DIR%\RPiRetail.log for Retail
echo    Example:
echo        buildffu Retail 
echo        buildffu Test 

exit /b 1

:START

REM Input validation
if [%1] == [/?] goto Usage
if [%1] == [-?] goto Usage
if [%1] == [] goto Usage

pushd
if not defined PKG_CONFIG_XML (
    call SetupBuildEnv.cmd
    )
    
echo Processing "%REPO_BUILD_ROOT%\boards\bcm2836\OemInputSamples\%1OEMInput.xml"...
REM echo imggen.cmd "%OUTPUT_DIR%\FFU-%1\RPi%1.ffu" "%REPO_BUILD_ROOT%\boards\bcm2836\OemInputSamples\%1OEMInput.xml"  "%KITSROOT%MSPackages"
call imggen.cmd "%OUTPUT_DIR%\FFU-%1\RPi%1.ffu" "%REPO_BUILD_ROOT%\boards\bcm2836\OemInputSamples\%1OEMInput.xml"  "%KITSROOT%MSPackages" > %PKGLOG_DIR%\RPi%1.log

popd
exit /b
