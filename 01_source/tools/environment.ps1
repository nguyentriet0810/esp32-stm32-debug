# SPDX-License-Identifier: GPL-3.0-or-later

<#
.SYNOPSIS
Define and initialize the project-local ESP-IDF environment.
.DESCRIPTION
Dot-source this file before calling Initialize-ProbeEnvironment. Paths resolve from the script
location. Initialization updates only this process and generated files under 01_source.
.NOTES
File: environment.ps1. Project scripts use process-local environment and repository-local generated files.
#>

# Shared environment for build and setup scripts; paths are relative to 01_source.
$probeSourceRoot = Split-Path -Parent $PSScriptRoot
$probeRepositoryRoot = Split-Path -Parent $probeSourceRoot
$probeToolingRoot = Join-Path $probeSourceRoot '.tooling'
$probeIdfPath = Join-Path $probeToolingRoot 'esp-idf-v6.1'
$probeIdfTools = Join-Path $probeToolingRoot 'idf-tools'
$probePythonHome = Join-Path $probeToolingRoot 'python'
$probePythonBase = Join-Path $probePythonHome 'python.exe'
$probePythonEnv = Join-Path $probeToolingRoot 'python-env'
$probePython = Join-Path $probePythonEnv 'Scripts\python.exe'

<#
.SYNOPSIS
Validate bundled tooling and initialize process-local ESP-IDF variables.
.PARAMETER ForSetup
Allow a missing Python virtual environment while setup prepares it.
.DESCRIPTION
Checks required files, repairs a relocated venv base path and writes generated idf-env.json.
Throws on missing required tooling; callers decide whether to stop their workflow.
#>
function Initialize-ProbeEnvironment {
    param([switch]$ForSetup)

    foreach ($probeRequiredPath in @(
        $probePythonBase,
        (Join-Path $probeIdfPath 'tools\idf.py'),
        (Join-Path $probeIdfPath 'tools\idf_tools.py')
    )) {
        if (-not (Test-Path -LiteralPath $probeRequiredPath)) {
            throw "Missing local tooling: $probeRequiredPath. See 02_document/02_build_and_tooling.md."
        }
    }

    New-Item -ItemType Directory -Path $probeIdfTools -Force > $null
    $env:IDF_PATH = $probeIdfPath
    $env:IDF_TOOLS_PATH = $probeIdfTools
    $env:IDF_PYTHON_ENV_PATH = $probePythonEnv
    $env:PYTHONDONTWRITEBYTECODE = '1'
    $env:PYTHONUTF8 = '1'
    $env:CCACHE_DIR = Join-Path $probeSourceRoot '.cache\ccache'
    $env:PIP_CACHE_DIR = Join-Path $probeSourceRoot '.cache\pip'

    # A copied venv must use the bundled base Python, including after relocating the project.
    $probeVenvConfig = Join-Path $probePythonEnv 'pyvenv.cfg'
    if (Test-Path -LiteralPath $probePython) {
        $probeExpectedHome = "home = $probePythonHome"
        $probeCurrentConfig = if (Test-Path -LiteralPath $probeVenvConfig) {
            Get-Content -LiteralPath $probeVenvConfig
        } else { @() }
        if ($probeCurrentConfig -notcontains $probeExpectedHome) {
            $probeVenvLines = @(
                $probeExpectedHome
                'include-system-site-packages = false'
                "executable = $probePythonBase"
            )
            [IO.File]::WriteAllText($probeVenvConfig, ($probeVenvLines -join "`n") + "`n",
                [Text.UTF8Encoding]::new($false))
        }
    } elseif (-not $ForSetup) {
        throw 'Local Python environment is missing. Run 01_source/tools/setup.ps1.'
    }

    # idf_tools selects tools for the active SDK path. This file is local generated state.
    $probeIdfRecord = @{
        version = '6.1'
        path = $probeIdfPath
        features = @('core')
        targets = @('esp32s3')
    }
    $probeIdfState = @{ idfInstalled = @{ "$probeIdfPath-v6.1" = $probeIdfRecord } } |
        ConvertTo-Json -Depth 5
    [IO.File]::WriteAllText((Join-Path $probeIdfTools 'idf-env.json'), $probeIdfState + "`n",
        [Text.UTF8Encoding]::new($false))
}
