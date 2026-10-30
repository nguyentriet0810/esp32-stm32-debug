# Forward all arguments directly to the project's local ESP-IDF installation.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'environment.ps1')
Initialize-ProbeEnvironment

$probeExports = & $probePython (Join-Path $probeIdfPath 'tools\idf_tools.py') export --format key-value
if ($LASTEXITCODE -ne 0) {
    throw 'ESP-IDF toolchain activation failed. Run tools/setup.ps1 if tools are missing.'
}
foreach ($probeExport in $probeExports) {
    if ($probeExport -match '^([A-Za-z_][A-Za-z0-9_]*)=(.*)$') {
        $probeEnvName = $Matches[1]
        $probeEnvValue = $Matches[2].Replace('%PATH%', $env:PATH)
        [Environment]::SetEnvironmentVariable($probeEnvName, $probeEnvValue, 'Process')
    }
}
$env:PATH = "$probePythonEnv\Scripts;$env:PATH"

$probeArguments = @($args)
if ($probeArguments.Count -eq 0) {
    $probeArguments = @('build')
}

# ESP-IDF v6.1 resolves a submodule's relative .git pointer from its current directory
# when printing the SDK version. Start in the SDK and select the firmware with -C.
Push-Location -LiteralPath $probeIdfPath
try {
    & $probePython (Join-Path $probeIdfPath 'tools\idf.py') -C $probeSourceRoot @probeArguments
    $probeExitCode = $LASTEXITCODE
} finally {
    Pop-Location
}
exit $probeExitCode
