param(
    # Required on a fresh clone unless .tooling/python has been restored separately.
    [string]$PythonHome
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'environment.ps1')

if (-not (Test-Path -LiteralPath $probePythonBase)) {
    if (-not $PythonHome -or -not (Test-Path -LiteralPath (Join-Path $PythonHome 'python.exe'))) {
        throw 'Provide -PythonHome with a Python 3.11 Windows x64 installation to copy into .tooling/python.'
    }
    & (Join-Path $PythonHome 'python.exe') -c 'import sys; assert sys.version_info[:2] == (3, 11); assert sys.maxsize > 2**32'
    if ($LASTEXITCODE -ne 0) { throw 'Unsupported Python runtime.' }
    & robocopy $PythonHome $probePythonHome /E /XJ /R:1 /W:1 /NFL /NDL /NP
    if ($LASTEXITCODE -ge 8) { throw 'Could not copy the Python runtime.' }
}

& git -C $probeRepositoryRoot submodule update --init --recursive -- '01_source/.tooling/esp-idf-v6.1' '01_source/external/blackmagic'
if ($LASTEXITCODE -ne 0) { throw 'Could not restore the pinned SDK/backend submodules.' }
$probeLock = Get-Content -LiteralPath (Join-Path $probeSourceRoot 'tooling.lock.json') -Raw | ConvertFrom-Json
$probeSdkCommit = & git -C $probeIdfPath rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $probeSdkCommit -ne $probeLock.esp_idf.commit) {
    throw 'ESP-IDF checkout does not match tooling.lock.json.'
}
$probeBackendCommit = & git -C (Join-Path $probeSourceRoot $probeLock.blackmagic.path) rev-parse HEAD
if ($LASTEXITCODE -ne 0 -or $probeBackendCommit -ne $probeLock.blackmagic.commit) {
    throw 'Black Magic Debug checkout does not match tooling.lock.json.'
}

Initialize-ProbeEnvironment -ForSetup
& $probePythonBase (Join-Path $probeIdfPath 'tools\idf_tools.py') install --targets=esp32s3
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF tool installation failed.' }

if (-not (Test-Path -LiteralPath $probePython)) {
    & $probePythonBase -m venv $probePythonEnv
    if ($LASTEXITCODE -ne 0) { throw 'Could not create the local Python environment.' }
}
'6.1' | Set-Content -LiteralPath (Join-Path $probePythonEnv 'idf_version.txt') -Encoding ascii
Copy-Item -LiteralPath (Join-Path $probeSourceRoot 'tooling\espidf.constraints.v6.1.txt') -Destination $probeIdfTools -Force
& $probePython -m pip install --requirement (Join-Path $probeSourceRoot 'tooling\python-requirements.lock.txt')
if ($LASTEXITCODE -ne 0) { throw 'Could not install the pinned Python dependencies.' }
& $probePython (Join-Path $probeIdfPath 'tools\idf_tools.py') check-python-dependencies
if ($LASTEXITCODE -ne 0) { throw 'ESP-IDF Python dependency check failed.' }
Write-Output 'Local ESP-IDF v6.1 environment is ready. Run 01_source/tools/idf.ps1 build.'
