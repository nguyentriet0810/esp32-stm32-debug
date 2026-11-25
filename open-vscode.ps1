# SPDX-License-Identifier: GPL-3.0-or-later
<#
.SYNOPSIS
Open the ESP32 firmware workspace using its dedicated VS Code profile.
.DESCRIPTION
Resolve the VS Code CLI, materialize local ESP-IDF paths from the workspace template
and open the workspace with the ESP32 Probe profile. Preserve unrelated local folder
settings. The generated settings file is ignored by Git and no extension is installed.
.EXAMPLE
.\open-vscode.ps1
.NOTES
Run again after relocating the repository to refresh absolute SDK paths. An absent
profile is created empty by VS Code; install the workspace's recommended extensions there.
#>

# Open this project's workspace with its dedicated VS Code profile.
$ErrorActionPreference = 'Stop'

$probeCodeCommand = Get-Command code.cmd -ErrorAction SilentlyContinue
if ($null -ne $probeCodeCommand) {
    $probeCodePath = $probeCodeCommand.Source
} else {
    $probeCodeCandidates = @(
        (Join-Path $env:LOCALAPPDATA 'Programs\Microsoft VS Code\bin\code.cmd'),
        (Join-Path $env:ProgramFiles 'Microsoft VS Code\bin\code.cmd')
    )
    $probeCodePath = $probeCodeCandidates |
        Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
        Select-Object -First 1
}

if (-not $probeCodePath) {
    throw 'VS Code was not found. Install VS Code or add code.cmd to PATH.'
}

$probeWorkspace = Join-Path $PSScriptRoot 'esp32-probe.code-workspace'

# ESP-IDF 2.3 does not expand ${workspaceFolder} inside customExtraVars objects.
# Materialize the workspace's environment template in ignored folder settings.
$probeSourceRoot = Join-Path $PSScriptRoot '01_source'
$probeWorkspaceConfig = Get-Content -LiteralPath $probeWorkspace -Raw | ConvertFrom-Json
$probeSettingsDirectory = Join-Path $probeSourceRoot '.vscode'
$probeSettingsPath = Join-Path $probeSettingsDirectory 'settings.json'
$probeLocalSettings = [PSCustomObject]@{}
if (Test-Path -LiteralPath $probeSettingsPath) {
    $probeLocalSettings = Get-Content -LiteralPath $probeSettingsPath -Raw | ConvertFrom-Json
}
$probeLocalVars = $probeLocalSettings.'idf.customExtraVars'
if ($null -eq $probeLocalVars) {
    $probeLocalVars = [PSCustomObject]@{}
}
foreach ($probeVariable in $probeWorkspaceConfig.settings.'idf.customExtraVars'.PSObject.Properties) {
    $probeValue = $probeVariable.Value.Replace('${workspaceFolder}', $probeSourceRoot)
    if ($probeVariable.Name -in @('IDF_PATH', 'IDF_TOOLS_PATH', 'IDF_PYTHON_ENV_PATH',
                                  'CCACHE_DIR', 'OPENOCD_SCRIPTS')) {
        # IDF compares Python's executable path as text, so use native separators consistently.
        $probeValue = [IO.Path]::GetFullPath($probeValue)
    }
    $probeLocalVars | Add-Member -NotePropertyName $probeVariable.Name -NotePropertyValue $probeValue -Force
}
$probeLocalSettings | Add-Member -NotePropertyName 'idf.currentSetup' -NotePropertyValue $probeLocalVars.IDF_PATH -Force
$probeLocalSettings | Add-Member -NotePropertyName 'idf.customExtraVars' -NotePropertyValue $probeLocalVars -Force
New-Item -ItemType Directory -Path $probeSettingsDirectory -Force | Out-Null
[IO.File]::WriteAllText($probeSettingsPath, ($probeLocalSettings | ConvertTo-Json -Depth 20), [Text.UTF8Encoding]::new($false))

& $probeCodePath --new-window --profile 'ESP32 Probe' $probeWorkspace
if ($LASTEXITCODE -ne 0) {
    throw "VS Code could not open ESP32 Probe (exit $LASTEXITCODE)."
}
