<#
.SYNOPSIS
Exports the driver binaries in a zip file format with all the bsp files in the right format.

.DESCRIPTION
Exports the driver binaries in a zip file format with all the bsp files in the right format the output zip file can be imported directly in the IoTCoreShell using the Import-IoTBSP command.

.PARAMETER OutputDir
Mandatory parameter, specifying the output directory for the zip file.

.PARAMETER IsDebug
Optional Switch parameter to package debug binaries. Default is Release binaries.

.EXAMPLE
binexport.ps1 "C:\Release" 

#>
param(
    [Parameter(Position = 0, Mandatory = $true)][ValidateNotNullOrEmpty()][String]$OutputDir,
    [Parameter(Position = 1, Mandatory = $false)][Switch]$IsDebug
)

$RootDir = "$PSScriptRoot\..\"
#$RootDir = Resolve-Path -Path $RootDir
if($IsDebug){
    $bindir = $RootDir + "build\bcm2836\ARM\Debug\"
} else {
    $bindir = $RootDir + "build\bcm2836\ARM\Release\"
}
if (!(Test-Path $bindir -PathType Container)){
    Write-Host "Error: $bindir not found." -ForegroundColor Red
    return
}
$bindir = Resolve-Path -Path $bindir

if (!(Test-Path "$OutputDir\RPi")) {
    New-Item "$OutputDir\RPi" -ItemType Directory | Out-Null
}

#Copy items
Copy-Item -Path "$RootDir\bspfiles\*" -Destination "$OutputDir\RPi\" -Recurse -Force | Out-Null
Copy-Item -Path "$bindir\*" -Destination "$OutputDir\RPi\Packages\RPi.Drivers\" -Include "*.sys","*.dll","*.inf" -Force | Out-Null

$curtime = Get-Date -Format "yyMMdd-HHmm"
$frags = $curtime.Split("-")
$OutputFile = "RPi_BSP_$($frags[0]).$($frags[1]).zip"
Write-Host "Output File: $OutputFile"

Compress-Archive -Path "$OutputDir\RPi\" -CompressionLevel "Fastest" -DestinationPath "$OutputDir\$OutputFile"
Remove-Item "$OutputDir\RPi\" -Recurse -Force | Out-Null

Write-Host "Done"
