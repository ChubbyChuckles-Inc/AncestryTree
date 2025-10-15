[CmdletBinding()]
param(
    [string]$RaylibVersion = "5.0",
    [ValidateSet("msvc", "mingw")]
    [string]$Toolchain = "msvc",
    [string]$DestinationRoot,
    [string]$Generator = "Ninja",
    [string]$BuildDirName = "build_windows",
    [switch]$PersistEnvironment,
    [switch]$SkipBuild,
    [switch]$SkipRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Resolve-RepoRoot {
    $scriptRoot = Split-Path -Parent $PSCommandPath
    $repoCandidate = Resolve-Path (Join-Path $scriptRoot "..")
    return $repoCandidate.Path
}

function New-DirectoryIfMissing([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        New-Item -ItemType Directory -Path $Path | Out-Null
    }
    return (Resolve-Path -LiteralPath $Path).Path
}

function Get-RaylibUri([string]$Version, [string]$ToolchainName) {
    switch ($ToolchainName) {
        "msvc" { return "https://github.com/raysan5/raylib/releases/download/$Version/raylib-${Version}_win64_msvc16.zip" }
        "mingw" { return "https://github.com/raysan5/raylib/releases/download/$Version/raylib-${Version}_win64_mingw-w64.zip" }
        default { throw "Unsupported toolchain '$ToolchainName'." }
    }
}

function Invoke-FileDownload([string]$Uri, [string]$OutFile) {
    Write-Host "Downloading $Uri" -ForegroundColor Cyan
    Invoke-WebRequest -Uri $Uri -OutFile $OutFile -UseBasicParsing
}

function Install-Raylib {
    param(
        [string]$Version,
        [string]$ToolchainName,
        [string]$Destination
    )

    $raylibUri = Get-RaylibUri -Version $Version -ToolchainName $ToolchainName
    $tempZip = Join-Path ([IO.Path]::GetTempPath()) "raylib-$Version-$ToolchainName.zip"
    Invoke-FileDownload -Uri $raylibUri -OutFile $tempZip

    $tempExtract = Join-Path ([IO.Path]::GetTempPath()) "raylib-$Version-$ToolchainName"
    if (Test-Path -LiteralPath $tempExtract) {
        Remove-Item -LiteralPath $tempExtract -Recurse -Force
    }
    Expand-Archive -LiteralPath $tempZip -DestinationPath $tempExtract -Force
    Remove-Item -LiteralPath $tempZip -Force

    $extractedRoot = Get-ChildItem -Path $tempExtract -Directory | Select-Object -First 1
    if (-not $extractedRoot) {
        throw "raylib archive did not include an extracted directory."
    }

    $raylibTarget = Join-Path $Destination "raylib"
    if (Test-Path -LiteralPath $raylibTarget) {
        Remove-Item -LiteralPath $raylibTarget -Recurse -Force
    }
    Move-Item -LiteralPath $extractedRoot.FullName -Destination $raylibTarget
    Remove-Item -LiteralPath $tempExtract -Recurse -Force

    $includeTest = Join-Path $raylibTarget "include\raylib.h"
    if (-not (Test-Path -LiteralPath $includeTest)) {
        throw "raylib installation failed; include/raylib.h not found under $raylibTarget"
    }

    Write-Host "raylib ready at $raylibTarget" -ForegroundColor Green
    return $raylibTarget
}

function Install-Nuklear {
    param(
        [string]$RepoRoot
    )

    $externalDir = New-DirectoryIfMissing (Join-Path $RepoRoot "include\external")
    $targetPath = Join-Path $externalDir "nuklear.h"
    $nuklearUri = "https://raw.githubusercontent.com/Immediate-Mode-UI/Nuklear/master/src/nuklear.h"
    Invoke-FileDownload -Uri $nuklearUri -OutFile $targetPath
    Write-Host "nuklear.h saved to $targetPath" -ForegroundColor Green
    return $externalDir
}

function Invoke-BuildConfiguration {
    param(
        [string]$RepoRoot,
        [string]$BuildDir,
        [string]$GeneratorName
    )

    $cmakeArgs = @("-S", $RepoRoot, "-B", $BuildDir, "-DCMAKE_BUILD_TYPE=Debug")
    if ($GeneratorName) {
        if ($GeneratorName -eq "Ninja" -and -not (Get-Command ninja -ErrorAction SilentlyContinue)) {
            throw "Requested generator 'Ninja' but ninja.exe is not available in PATH."
        }
        $cmakeArgs += "-G"
        $cmakeArgs += $GeneratorName
    }
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configuration failed with exit code $LASTEXITCODE"
    }
}

function Invoke-ProjectBuild {
    param([string]$BuildDir)
    & cmake "--build" $BuildDir
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed with exit code $LASTEXITCODE"
    }
}

function Invoke-Executable {
    param([string]$BuildDir)
    $exePath = Join-Path $BuildDir "bin\ancestrytree.exe"
    if (-not (Test-Path -LiteralPath $exePath)) {
        Write-Warning "Executable not found at $exePath. Check build logs for failures."
        return
    }
    Write-Host "Launching $exePath" -ForegroundColor Cyan
    & $exePath
}

$repoRoot = Resolve-RepoRoot
if (-not $DestinationRoot) {
    $DestinationRoot = Join-Path $repoRoot "dependencies"
}
$destination = New-DirectoryIfMissing $DestinationRoot

$raylibHome = Install-Raylib -Version $RaylibVersion -ToolchainName $Toolchain -Destination $destination
$env:RAYLIB_HOME = $raylibHome

$nuklearInclude = Install-Nuklear -RepoRoot $repoRoot
$env:NUKLEAR_INCLUDE = $nuklearInclude

if ($PersistEnvironment.IsPresent) {
    Write-Host "Persisting environment variables" -ForegroundColor Yellow
    setx RAYLIB_HOME $raylibHome | Out-Null
    setx NUKLEAR_INCLUDE $nuklearInclude | Out-Null
}

if (-not $SkipBuild.IsPresent) {
    $buildDir = New-DirectoryIfMissing (Join-Path $repoRoot $BuildDirName)
    Invoke-BuildConfiguration -RepoRoot $repoRoot -BuildDir $buildDir -GeneratorName $Generator
    Invoke-ProjectBuild -BuildDir $buildDir
    $dllSource = Join-Path $raylibHome "lib\raylib.dll"
    if (Test-Path -LiteralPath $dllSource) {
        $dllTargetDir = Join-Path $buildDir "bin"
        New-DirectoryIfMissing $dllTargetDir | Out-Null
        Copy-Item -LiteralPath $dllSource -Destination $dllTargetDir -Force
    }
    if (-not $SkipRun.IsPresent) {
        Invoke-Executable -BuildDir $buildDir
    }
}
else {
    Write-Host "Skipping build step per user request." -ForegroundColor Yellow
}

Write-Host "Dependency setup complete." -ForegroundColor Green
