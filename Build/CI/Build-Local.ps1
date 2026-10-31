#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ROOT,
    [ValidateSet('Game', 'Server', 'All')]
    [string]$BuildKind = 'Game',
    [ValidateSet('Development', 'Shipping')]
    [string]$Configuration = 'Development',
    [string[]]$Maps = @('/Game/FirstPerson/Lvl_FirstPerson'),
    [string]$OutputDirectory,
    [switch]$PreflightOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$projectFile = Join-Path $projectRoot 'Project_FPS.uproject'

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    throw 'Pass -EngineRoot or set UE_ROOT to the UE 5.8.3 engine directory.'
}
$EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
$uat = Join-Path $EngineRoot 'Engine/Build/BatchFiles/RunUAT.bat'
$versionFile = Join-Path $EngineRoot 'Engine/Build/Build.version'
foreach ($requiredPath in @($projectFile, $uat, $versionFile)) {
    if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) {
        throw "Missing required file: $requiredPath"
    }
}
$version = Get-Content -LiteralPath $versionFile -Raw | ConvertFrom-Json
$engineVersion = "$($version.MajorVersion).$($version.MinorVersion).$($version.PatchVersion)"
if ($engineVersion -ne '5.8.3') {
    throw "Expected UE 5.8.3, found $engineVersion. Update the project baseline before changing the build version."
}
if ($BuildKind -in @('Server', 'All')) {
    if (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine/Build/InstalledBuild.txt')) {
        throw 'This pipeline requires a source-built UE engine for Server/All. The selected engine is an installed build; use Game until the source build is ready.'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Source/Project_FPSServer.Target.cs'))) {
        throw 'Project_FPSServer.Target.cs is required for Server/All.'
    }
}
foreach ($map in $Maps) {
    if ($map -notmatch '^/Game/[A-Za-z0-9_/]+$') {
        throw "Use a project map package path such as /Game/FirstPerson/Lvl_FirstPerson: $map"
    }
    $mapFile = Join-Path $projectRoot ("Content/" + $map.Substring(6) + '.umap')
    if (-not (Test-Path -LiteralPath $mapFile -PathType Leaf)) { throw "Missing map: $mapFile" }
}
if ($Maps.Count -eq 0) { throw 'At least one map must be selected.' }

Write-Host "Preflight passed: UE $engineVersion; $BuildKind; $Configuration."
Write-Host 'This verifies paths, version and target prerequisites; UAT will validate the compiler/SDK during the build.'
if ($PreflightOnly) { return }

if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path ([IO.Path]::GetTempPath()) ("ProjectFPS-" + [guid]::NewGuid().ToString('N'))
}
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (Test-Path -LiteralPath $OutputDirectory) {
    if (-not (Test-Path -LiteralPath $OutputDirectory -PathType Container)) { throw 'OutputDirectory must be a directory.' }
    if (@(Get-ChildItem -LiteralPath $OutputDirectory -Force).Count -gt 0) {
        throw 'OutputDirectory must be empty to avoid mixing old and new packages. Choose a new directory.'
    }
}
$null = New-Item -ItemType Directory -Path $OutputDirectory -Force
$logDirectory = Join-Path $OutputDirectory 'Logs'
$null = New-Item -ItemType Directory -Path $logDirectory
$common = @(
    'BuildCookRun', "-project=$projectFile", '-platform=Win64',
    '-build', '-cook', '-stage', '-pak', '-archive', '-unattended', '-nop4', '-utf8output',
    ('-map=' + ($Maps -join '+'))
)
$builds = if ($BuildKind -eq 'All') { @('Game', 'Server') } else { @($BuildKind) }
Push-Location $projectRoot
try {
    foreach ($kind in $builds) {
        $archive = Join-Path $OutputDirectory $kind
        $arguments = $common + @("-archivedirectory=$archive")
        if ($kind -eq 'Game') {
            $arguments += "-clientconfig=$Configuration"
        } else {
            $arguments += @('-server', '-noclient', '-serverplatform=Win64', "-serverconfig=$Configuration")
        }
        & $uat @arguments 2>&1 | Tee-Object -FilePath (Join-Path $logDirectory "$kind.log")
        if ($LASTEXITCODE -ne 0) { throw "$kind BuildCookRun failed (exit $LASTEXITCODE). See $logDirectory." }
        if (-not (Test-Path -LiteralPath $archive -PathType Container) -or
            @(Get-ChildItem -LiteralPath $archive -Recurse -File -Filter '*.exe').Count -eq 0) {
            throw "$kind did not produce an executable archive: $archive"
        }
    }
    $commit = & git rev-parse HEAD
    if ($LASTEXITCODE -ne 0) { throw 'Could not record the Git commit.' }
    $workingTree = & git status --porcelain
    if ($LASTEXITCODE -ne 0) { throw 'Could not record the working tree state.' }
    [ordered]@{
        commit = $commit
        dirty = [bool]$workingTree
        engine = $engineVersion
        configuration = $Configuration
        targets = $builds
        maps = $Maps
        builtAtUtc = [DateTime]::UtcNow.ToString('o')
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build-info.json') -Encoding utf8
} finally {
    Pop-Location
}
Write-Host "Packages and logs: $OutputDirectory"
Write-Host 'Packaging completed. Multiplayer behavior and rendering still require runtime verification.'
