[CmdletBinding()]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Configuration = "Release",

    [string]$ToolchainDirectory,

    [switch]$Clean,

    [switch]$Benchmark,

    [switch]$Test,

    [switch]$Run
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath($PSScriptRoot)
$buildDirectory = [System.IO.Path]::GetFullPath(
    (Join-Path -Path $repoRoot -ChildPath "build")
)

function Invoke-CMake {
    param(
        [Parameter(Mandatory)]
        [string[]]$Arguments
    )

    & $script:cmakeCommand.Source @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "CMake failed with exit code $LASTEXITCODE."
    }
}

if ($Clean -and (Test-Path -LiteralPath $buildDirectory)) {
    $rootPrefix = $repoRoot.TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar
    ) + [System.IO.Path]::DirectorySeparatorChar

    if (-not $buildDirectory.StartsWith(
        $rootPrefix,
        [System.StringComparison]::OrdinalIgnoreCase
    )) {
        throw "Refusing to clean a build directory outside the repository."
    }

    Write-Host "Removing $buildDirectory"
    Remove-Item -LiteralPath $buildDirectory -Recurse -Force
}

$script:cmakeCommand = Get-Command "cmake.exe" -ErrorAction Stop

if ($ToolchainDirectory) {
    $resolvedToolchainDirectory = [System.IO.Path]::GetFullPath($ToolchainDirectory)
    $compilerPath = Join-Path $resolvedToolchainDirectory "g++.exe"
    $makePath = Join-Path $resolvedToolchainDirectory "mingw32-make.exe"

    if (-not (Test-Path -LiteralPath $compilerPath -PathType Leaf)) {
        throw "g++.exe was not found in $resolvedToolchainDirectory"
    }
    if (-not (Test-Path -LiteralPath $makePath -PathType Leaf)) {
        throw "mingw32-make.exe was not found in $resolvedToolchainDirectory"
    }
}
else {
    $toolchains = foreach ($compilerCommand in (
        Get-Command "g++.exe" -All -ErrorAction Stop
    )) {
        $candidateDirectory = Split-Path -Parent $compilerCommand.Source
        $candidateMake = Join-Path $candidateDirectory "mingw32-make.exe"

        if (Test-Path -LiteralPath $candidateMake -PathType Leaf) {
            $versionText = (& $compilerCommand.Source -dumpfullversion -dumpversion |
                Select-Object -First 1)

            if ($LASTEXITCODE -eq 0) {
                $parsedVersion = $null
                if ([System.Version]::TryParse(
                    $versionText,
                    [ref]$parsedVersion
                )) {
                    [PSCustomObject]@{
                        Version = $parsedVersion
                        Compiler = $compilerCommand.Source
                        Make = $candidateMake
                    }
                }
            }
        }
    }

    $selectedToolchain = $toolchains |
        Sort-Object -Property Version -Descending |
        Select-Object -First 1

    if (-not $selectedToolchain) {
        throw "A MinGW-w64 toolchain with g++.exe and mingw32-make.exe was not found."
    }

    $compilerPath = $selectedToolchain.Compiler
    $makePath = $selectedToolchain.Make
    $resolvedToolchainDirectory = Split-Path -Parent $compilerPath
}

$env:PATH = "$resolvedToolchainDirectory;$env:PATH"
$compilerPath = $compilerPath.Replace("\", "/")
$makePath = $makePath.Replace("\", "/")

Write-Host "Configuring $Configuration build with $compilerPath"
Invoke-CMake -Arguments @(
    "-S", $repoRoot,
    "-B", $buildDirectory,
    "-G", "MinGW Makefiles",
    "-DCMAKE_BUILD_TYPE=$Configuration",
    "-DCMAKE_CXX_COMPILER:FILEPATH=$compilerPath",
    "-DCMAKE_MAKE_PROGRAM:FILEPATH=$makePath"
)

Write-Host "Building BallCollision"
Invoke-CMake -Arguments @(
    "--build", $buildDirectory,
    "--parallel"
)

$executable = Join-Path -Path $buildDirectory -ChildPath "bin/BallCollision.exe"
Write-Host "Build complete: $executable"

if ($Test) {
    Invoke-CMake -Arguments @(
        "--build", $buildDirectory,
        "--target", "test"
    )
}

if ($Benchmark) {
    $benchmarkExecutable = Join-Path `
        -Path $buildDirectory `
        -ChildPath "bin/BallCollisionBenchmark.exe"

    if (-not (Test-Path -LiteralPath $benchmarkExecutable -PathType Leaf)) {
        throw "The benchmark executable was not created: $benchmarkExecutable"
    }

    & $benchmarkExecutable
    if ($LASTEXITCODE -ne 0) {
        throw "BallCollisionBenchmark exited with code $LASTEXITCODE."
    }
}

if ($Run) {
    if (-not (Test-Path -LiteralPath $executable -PathType Leaf)) {
        throw "The expected executable was not created: $executable"
    }

    Push-Location $repoRoot
    try {
        & $executable
        if ($LASTEXITCODE -ne 0) {
            throw "BallCollision exited with code $LASTEXITCODE."
        }
    }
    finally {
        Pop-Location
    }
}
