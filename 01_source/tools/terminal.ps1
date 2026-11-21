# SPDX-License-Identifier: GPL-3.0-or-later
# Activate the project's local ESP-IDF tools in the current PowerShell process.
# Dot-source this file; no user settings or permanent PATH entries are changed.
$probeTerminalPreviousErrorAction = $ErrorActionPreference
$ErrorActionPreference = 'Stop'
try {
    . (Join-Path $PSScriptRoot 'environment.ps1')
    Initialize-ProbeEnvironment

    $probeTerminalExports = & $probePython (Join-Path $probeIdfPath 'tools\idf_tools.py') export --format key-value
    if ($LASTEXITCODE -ne 0) {
        throw 'ESP-IDF toolchain activation failed. Run tools/setup.ps1 if tools are missing.'
    }
    foreach ($probeTerminalExport in $probeTerminalExports) {
        if ($probeTerminalExport -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {
            $probeTerminalEnvName = $Matches[1]
            $probeTerminalEnvValue = $Matches[2].Replace('%PATH%', $env:PATH)
            [Environment]::SetEnvironmentVariable($probeTerminalEnvName, $probeTerminalEnvValue, 'Process')
        }
    }
    $env:PATH = "$probePythonEnv\Scripts;$env:PATH"

    # Capture the absolute wrapper path so idf.py also works after changing directory.
    # Invoke the script with &, never dot-source it: its exit only ends that script.
    $probeTerminalIdfWrapper = Join-Path $PSScriptRoot 'dev.ps1'
    $probeTerminalIdfCommand = {
        try {
            & $probeTerminalIdfWrapper @args
            $global:LASTEXITCODE = $LASTEXITCODE
        } catch {
            $global:LASTEXITCODE = 1
            throw
        }
    }.GetNewClosure()
    Set-Item -LiteralPath 'Function:\global:idf.py' -Value $probeTerminalIdfCommand

    $global:LASTEXITCODE = 0
    Write-Host 'ESP32-S3 development terminal ready (ESP-IDF v6.1, build-vscode).'
    Write-Host 'Commands: idf.py build | idf.py menuconfig | idf.py -p COM9 flash monitor --no-reset'
} catch {
    $global:LASTEXITCODE = 1
    throw
} finally {
    $ErrorActionPreference = $probeTerminalPreviousErrorAction
}
