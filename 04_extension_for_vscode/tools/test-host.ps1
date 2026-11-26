# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Run VS Code Extension Host tests in isolated workspace-local user data and extension folders.
.DESCRIPTION
Copies only the selected native debugger dependency tree into a backend-specific test cache.
ProbeHost enables live tests; FlashLaunch explicitly enables programming the supplied F103 test ELF.
.PARAMETER TrustedTestWorkspace
Run the official trusted-test baseline only for the fixed cache workspace. Review that folder first.
#>
param(
    [ValidateSet('cortex', 'st', 'both')][string]$Backend = 'both',
    [switch]$FlashLaunch,
    [switch]$Packaged,
    [string]$ProbeHost = '',
    [string]$ElfPath = '',
    [string]$GdbPath = '',
    [switch]$TrustedTestWorkspace,
    [string]$CodePath = (Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\Code.exe')
)
$ErrorActionPreference = 'Stop'
$probeExtensionRoot = Split-Path -Parent $PSScriptRoot
$probeHostCache = Join-Path $probeExtensionRoot '.cache\vscode-test'
$probeHostExtensions = Join-Path $probeHostCache "extensions-$Backend"
$probeHostWorkspace = Join-Path $probeHostCache 'workspace'
$probeHostControlWorkspace = Join-Path $probeHostCache 'control-workspace'
$probeHostWorkspaceFile = Join-Path $probeHostCache 'native-test.code-workspace'
$probeHostUserData = Join-Path $probeHostCache "user data $Backend"
$probeHostReport = Join-Path $probeHostCache ('result-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.json')
if ($ProbeHost -and (-not $ElfPath -or -not (Test-Path -LiteralPath $ElfPath -PathType Leaf))) {
    throw 'Live testing requires -ElfPath pointing to the STM32 ELF already running on the target.'
}
New-Item -ItemType Directory -Force -Path $probeHostExtensions, $probeHostWorkspace, $probeHostControlWorkspace, $probeHostUserData > $null
@{ folders = @(@{ name = 'Target project'; path = $probeHostWorkspace }, @{ name = 'Unrelated project'; path = $probeHostControlWorkspace }) } |
    ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $probeHostWorkspaceFile -Encoding utf8
$probeInstalledRoot = Join-Path $env:USERPROFILE '.vscode\extensions'
$probeDependencyQueue = [System.Collections.Generic.Queue[string]]::new()
if ($Backend -in @('cortex', 'both')) { $probeDependencyQueue.Enqueue('marus25.cortex-debug') }
if ($Backend -in @('st', 'both')) { $probeDependencyQueue.Enqueue('stmicroelectronics.stm32cube-ide-debug-generic-gdbserver') }
$probeCopied = [System.Collections.Generic.HashSet[string]]::new()
while ($probeDependencyQueue.Count -gt 0) {
    $probeDependencyId = $probeDependencyQueue.Dequeue()
    if (-not $probeCopied.Add($probeDependencyId)) { continue }
    $probeDependencyFolder = Get-ChildItem -LiteralPath $probeInstalledRoot -Directory |
        Where-Object { $_.Name -like "$probeDependencyId-*" } | Sort-Object Name -Descending | Select-Object -First 1
    if (-not $probeDependencyFolder) { throw "Install $probeDependencyId before running the host test." }
    Copy-Item -LiteralPath $probeDependencyFolder.FullName -Destination $probeHostExtensions -Recurse -Force
    $probeDependencyManifest = Get-Content -Raw -LiteralPath (Join-Path $probeDependencyFolder.FullName 'package.json') | ConvertFrom-Json
    foreach ($probeDependency in $probeDependencyManifest.extensionDependencies) { $probeDependencyQueue.Enqueue($probeDependency) }
}
$env:MAGIC_PROBE_TEST_REPORT = $probeHostReport
$env:MAGIC_PROBE_TEST_HOST = $ProbeHost
$env:MAGIC_PROBE_TEST_ELF = $ElfPath
$env:MAGIC_PROBE_TEST_GDB = $GdbPath
$env:MAGIC_PROBE_TEST_BACKEND = $Backend
$env:MAGIC_PROBE_TEST_NODE = (Get-Command node -ErrorAction Stop).Source
$env:MAGIC_PROBE_TEST_FLASH = if ($FlashLaunch) { '1' } else { '0' }
$env:MAGIC_PROBE_TEST_PACKAGE_ROOT = if ($Packaged) { $probeHostExtensions } else { '' }
# The Code CLI uses this variable, but the Extension Host needs the Electron application.
Remove-Item Env:ELECTRON_RUN_AS_NODE -ErrorAction SilentlyContinue
$probeDevelopmentPath = $probeExtensionRoot
if ($Packaged) {
    $probeVersion = (Get-Content -LiteralPath (Join-Path $probeExtensionRoot 'package.json') -Raw | ConvertFrom-Json).version
    $probeVsix = Join-Path $probeExtensionRoot "dist\magic-debugger-tool-$probeVersion.vsix"
    if (-not (Test-Path -LiteralPath $probeVsix -PathType Leaf)) { throw 'Run npm run package before -Packaged.' }
    $probeCli = Get-Command code.cmd -ErrorAction Stop
    & $probeCli.Source --user-data-dir $probeHostUserData --extensions-dir $probeHostExtensions --install-extension $probeVsix --force
    if ($LASTEXITCODE -ne 0) { throw 'Could not install the local VSIX in the test profile.' }
    $probeDevelopmentPath = Join-Path $probeHostCache 'test-driver'
    New-Item -ItemType Directory -Force -Path $probeDevelopmentPath > $null
    '{"name":"magic-probe-test-driver","publisher":"magic-probe","version":"0.0.0","engines":{"vscode":"^1.90.0"},"main":"extension.js"}' |
        Set-Content -LiteralPath (Join-Path $probeDevelopmentPath 'package.json') -Encoding utf8
    'exports.activate = () => {};' | Set-Content -LiteralPath (Join-Path $probeDevelopmentPath 'extension.js') -Encoding utf8
}
$probeHostArguments = @('--new-window', '--skip-welcome', '--skip-release-notes',
    '--user-data-dir', $probeHostUserData, '--extensions-dir', $probeHostExtensions,
    "--extensionDevelopmentPath=$probeDevelopmentPath", "--extensionTestsPath=$(Join-Path $probeExtensionRoot 'test\host.js')",
    $probeHostWorkspaceFile)
# Official trusted-test baseline. Applies only to this process and the fixed cache workspace;
# no user/profile trust setting is changed. Use only after reviewing that test workspace.
if ($TrustedTestWorkspace) { $probeHostArguments += '--disable-workspace-trust' }
$probeHostQuotedArguments = $probeHostArguments | ForEach-Object { '"' + $_ + '"' }
$probeHostProcess = Start-Process -FilePath $CodePath -ArgumentList $probeHostQuotedArguments -WindowStyle Hidden -PassThru `
    -RedirectStandardOutput (Join-Path $probeHostCache 'stdout.log') -RedirectStandardError (Join-Path $probeHostCache 'stderr.log')
$probeHostDeadline = (Get-Date).AddSeconds($(if ($FlashLaunch) { 240 } else { 100 }))
while (-not (Test-Path -LiteralPath $probeHostReport) -and -not $probeHostProcess.HasExited -and (Get-Date) -lt $probeHostDeadline) {
    Start-Sleep -Milliseconds 250
}
if (-not (Test-Path -LiteralPath $probeHostReport)) {
    if (-not $probeHostProcess.HasExited) { Stop-Process -Id $probeHostProcess.Id }
    throw "Extension Host did not produce a report. Inspect $probeHostCache."
}
Start-Sleep -Milliseconds 200
$probeHostResult = Get-Content -Raw -LiteralPath $probeHostReport | ConvertFrom-Json
$probeHostResult | Select-Object trusted, passed, error, extensionPath, backends, folderScope, failedBuildCleanup, connection, disconnected | ConvertTo-Json -Depth 7
if (-not $probeHostProcess.HasExited -and -not $probeHostProcess.WaitForExit(10000)) { Stop-Process -Id $probeHostProcess.Id }
if (-not $probeHostResult.passed) { throw 'Extension Host test failed; see the JSON report.' }
Write-Output "PASS isolated Extension Host: $probeHostReport"
