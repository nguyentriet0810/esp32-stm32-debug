# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Open extension development in a dedicated VS Code profile.
.PARAMETER InstallDependency
Install/enable the selected native debugger(s) in this profile, leaving the ESP32 profile alone.
#>
param([switch]$InstallDependency, [ValidateSet('cortex', 'st', 'both')][string]$Backend = 'both')
$ErrorActionPreference = 'Stop'
$probeExtensionRoot = Split-Path -Parent $PSScriptRoot
$probeExtensionProfile = 'MAGIC Probe Extension Dev'
$probeCodeCommand = Get-Command code.cmd -ErrorAction SilentlyContinue
if (-not $probeCodeCommand) { $probeCodeCommand = Get-Command code -ErrorAction Stop }
# Opening a named profile creates it; the extension-install CLI requires it to exist first.
& $probeCodeCommand.Source --new-window --profile $probeExtensionProfile `
    (Join-Path $probeExtensionRoot 'magic-probe-extension.code-workspace')
if ($LASTEXITCODE -ne 0) { throw 'Could not open the extension workspace.' }
if ($InstallDependency) {
    $probeDevDependencies = @()
    if ($Backend -in @('cortex', 'both')) { $probeDevDependencies += 'marus25.cortex-debug@1.12.1' }
    if ($Backend -in @('st', 'both')) { $probeDevDependencies += 'stmicroelectronics.stm32cube-ide-debug-generic-gdbserver@1.5.1' }
    foreach ($probeDevDependency in $probeDevDependencies) {
        & $probeCodeCommand.Source --profile $probeExtensionProfile --install-extension $probeDevDependency
        if ($LASTEXITCODE -ne 0) { throw "Could not prepare $probeDevDependency in the extension profile." }
    }
}
