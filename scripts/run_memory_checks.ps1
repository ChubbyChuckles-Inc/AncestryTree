[CmdletBinding()]
param(
    [string]$BuildDir = "build_windows",
    [string[]]$TestArgs = @()
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-DrMemoryCommand {
    if ($env:DRMEMORY_HOME) {
        $candidate = Join-Path $env:DRMEMORY_HOME "bin/drmemory.exe"
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    $command = Get-Command drmemory.exe -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    return $null
}

$drMemoryCmd = Resolve-DrMemoryCommand
if (-not $drMemoryCmd) {
    Write-Error "Dr. Memory was not found. Install it from https://github.com/DynamoRIO/drmemory/releases and set the DRMEMORY_HOME environment variable, or add drmemory.exe to PATH."
    exit 127
}

$testBinary = Join-Path $BuildDir "bin/ancestrytree_tests.exe"
if (-not (Test-Path -LiteralPath $testBinary)) {
    throw "Test binary not found at $testBinary. Build the tests before running memory checks."
}

Write-Host "Executing Dr. Memory against $testBinary" -ForegroundColor Cyan
$arguments = @("-batch", "-exit_code_if_errors", "1", $testBinary) + $TestArgs
& $drMemoryCmd @arguments
$exitCode = $LASTEXITCODE

if ($exitCode -ne 0) {
    Write-Error "Dr. Memory reported memory issues (exit code $exitCode)."
    exit $exitCode
}

Write-Host "No memory issues detected by Dr. Memory." -ForegroundColor Green
