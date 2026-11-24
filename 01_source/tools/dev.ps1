<#
.SYNOPSIS
Run ESP-IDF actions for the separate VS Code Debug build.
.DESCRIPTION
Forwards arguments through idf.ps1 while selecting build-vscode, sdkconfig.vscode and its defaults
overlay. Reuses generated configuration until reconfigure is explicitly requested.
.EXAMPLE
.\01_source\tools\dev.ps1 build
.NOTES
File: dev.ps1. Project scripts use process-local environment and repository-local generated files.
#>

# SPDX-License-Identifier: GPL-3.0-or-later
# Build the VS Code development profile using the project's pinned ESP-IDF.
$ErrorActionPreference = 'Stop'
$probeDevSource = Split-Path -Parent $PSScriptRoot
$probeDevBuild = Join-Path $probeDevSource 'build-vscode'
$probeDevConfig = Join-Path $probeDevSource 'sdkconfig.vscode'
$probeDevDefaults = (Join-Path $probeDevSource 'sdkconfig.defaults') + ';' +
    (Join-Path $probeDevSource 'sdkconfig.vscode.defaults')
$probeDevActions = @($args)
if ($probeDevActions.Count -eq 0) {
    $probeDevActions = @('build')
}
$probeDevArguments = @('-B', $probeDevBuild)
if ((-not (Test-Path -LiteralPath (Join-Path $probeDevBuild 'CMakeCache.txt'))) -or
    ($probeDevActions -contains 'reconfigure')) {
    $probeDevArguments += @('-D', "SDKCONFIG=$probeDevConfig",
        '-D', "SDKCONFIG_DEFAULTS=$probeDevDefaults")
}
& (Join-Path $PSScriptRoot 'idf.ps1') @probeDevArguments @probeDevActions
exit $LASTEXITCODE
