param(
    [string]$PackageName = "Magpie-Experimental-x64",
    [string]$ReleaseDirectory,
    [string]$Configuration = "Release",
    [string]$Platform = "x64",
    [string]$Version,
    [ValidateRange(1, 64)]
    [int]$MaxCpuCount = 2,
    [switch]$AllowDirtySource,
    [switch]$ExcludeTensorRTDepthRuntime,
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

$sourceRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$workspaceRoot = Split-Path $sourceRoot -Parent
$releaseRoot = [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot "release"))
$buildOutput = Join-Path $sourceRoot "bin\$Platform\$Configuration"

function Find-MSBuild {
    $command = Get-Command msbuild.exe -ErrorAction SilentlyContinue
    if ($command) {
        $amd64Command = Join-Path (Split-Path $command.Source -Parent) "amd64\MSBuild.exe"
        if (Test-Path -LiteralPath $amd64Command) { return $amd64Command }
        return $command.Source
    }

    $vswhereCandidates = @(
        (Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"),
        (Join-Path $env:ProgramFiles "Microsoft Visual Studio\Installer\vswhere.exe")
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($vswhere in $vswhereCandidates) {
        $installationPath = & $vswhere -latest -products * `
            -requires Microsoft.Component.MSBuild -property installationPath |
            Select-Object -First 1
        if ($installationPath) {
            $amd64MsBuild = Join-Path $installationPath "MSBuild\Current\Bin\amd64\MSBuild.exe"
            if (Test-Path -LiteralPath $amd64MsBuild) { return $amd64MsBuild }
        }

        $result = & $vswhere -latest -products * -requires Microsoft.Component.MSBuild `
            -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
        if ($result -and (Test-Path -LiteralPath $result)) { return $result }
    }
    throw "MSBuild was not found. Install the Visual Studio C++ build tools."
}

function Add-ConanToPath {
    if (Get-Command conan -ErrorAction SilentlyContinue) { return }

    $roots = @(
        (Join-Path $env:LOCALAPPDATA "Programs\Python"),
        (Join-Path $env:APPDATA "Python")
    ) | Where-Object { $_ -and (Test-Path -LiteralPath $_) }

    foreach ($root in $roots) {
        $conan = Get-ChildItem -LiteralPath $root -Filter conan.exe -File -Recurse `
            -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($conan) {
            $env:Path = "$($conan.DirectoryName);$env:Path"
            return
        }
    }
    throw "Conan was not found. Install Conan 2 and make conan.exe available on PATH."
}

function Add-CMakeToPath {
    if (Get-Command cmake.exe -ErrorAction SilentlyContinue) { return }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $installations = & $vswhere -products * -property installationPath
        foreach ($installation in $installations) {
            $cmakeDir = Join-Path $installation "Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
            if (Test-Path -LiteralPath (Join-Path $cmakeDir "cmake.exe")) {
                $env:Path = "$cmakeDir;$env:Path"
                return
            }
        }
    }
    throw "CMake was not found. Install the Visual Studio CMake component or add cmake.exe to PATH."
}

function Find-Git {
    $command = Get-Command git.exe -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }

    $candidates = @((Join-Path $env:ProgramFiles "Git\cmd\git.exe"))
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        foreach ($installation in (& $vswhere -products * -property installationPath)) {
            $candidates += Join-Path $installation `
                "Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd\git.exe"
        }
    }
    return $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

$gitPath = Find-Git
function Invoke-Git([string[]]$Arguments) {
    if (!$gitPath) { return $null }
    $previousErrorAction = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $value = & $gitPath -c "safe.directory=$sourceRoot" -C $sourceRoot @Arguments 2>$null
        $exitCode = $LASTEXITCODE
    } finally {
        $ErrorActionPreference = $previousErrorAction
    }
    if ($exitCode -ne 0) { return $null }
    return ($value | Out-String).Trim()
}

$commit = Invoke-Git @("rev-parse", "HEAD")
$shortCommit = Invoke-Git @("rev-parse", "--short=12", "HEAD")
$commitTimeText = Invoke-Git @("show", "-s", "--format=%cI", "HEAD")
$exactTag = Invoke-Git @("describe", "--tags", "--exact-match", "HEAD")
$sourceDirty = [bool](Invoke-Git @("status", "--porcelain=v1", "--untracked-files=all"))
if ($sourceDirty -and !$AllowDirtySource) {
    throw "The source tree has uncommitted changes. Commit them first, or use -AllowDirtySource for a local test package."
}

if (!$Version) {
    if ($exactTag) {
        $Version = $exactTag -replace '^v', ''
    } elseif ($shortCommit) {
        $Version = "0.0.0-dev+$shortCommit"
    } else {
        $Version = "0.0.0-dev"
    }
}

$versionMatch = [regex]::Match($Version, '^(?:v)?(\d+)\.(\d+)\.(\d+)')
if (!$versionMatch.Success) {
    throw "Version must begin with major.minor.patch: $Version"
}

if (!$ReleaseDirectory) {
    $ReleaseDirectory = if ($Version.StartsWith('v', [System.StringComparison]::OrdinalIgnoreCase)) {
        $Version
    } else {
        "v$Version"
    }
}
$releaseContainer = [System.IO.Path]::GetFullPath((Join-Path $releaseRoot $ReleaseDirectory))
$stagingDir = [System.IO.Path]::GetFullPath((Join-Path $releaseContainer $PackageName))
$zipPath = [System.IO.Path]::GetFullPath((Join-Path $releaseContainer "$PackageName.zip"))

if (!$releaseContainer.StartsWith($releaseRoot + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Release container escaped the release directory."
}
if (!$stagingDir.StartsWith($releaseContainer + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase) -or
    !$zipPath.StartsWith($releaseContainer + [System.IO.Path]::DirectorySeparatorChar,
        [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Release path escaped the release directory."
}

function Stop-RunningMagpie {
    $running = Get-Process -Name "Magpie" -ErrorAction SilentlyContinue
    if (!$running) { return }

    try {
        $running | Stop-Process -Force -ErrorAction Stop
    } catch {
        $processIds = ($running.Id | ForEach-Object { [string][int]$_ }) -join ','
        $command = "Stop-Process -Id $processIds -Force"
        $elevated = Start-Process -FilePath "powershell.exe" -Verb RunAs -WindowStyle Hidden `
            -ArgumentList @("-NoProfile", "-Command", $command) -Wait -PassThru
        if ($elevated.ExitCode -ne 0) {
            throw "Elevated Magpie shutdown failed with exit code $($elevated.ExitCode)."
        }
    }

    Start-Sleep -Milliseconds 250
    if (Get-Process -Name "Magpie" -ErrorAction SilentlyContinue) {
        throw "Magpie is still running after the shutdown attempt."
    }
}

# Magpie and its PRI must come from the same complete build. Stop the running
# application before Rebuild so MSBuild never falls back to deploying a lone
# replacement executable around a locked output directory.
Stop-RunningMagpie

if (!$SkipBuild) {
    $msbuild = Find-MSBuild
    Add-ConanToPath
    Add-CMakeToPath

    $msbuildArgs = @(
        "Magpie.slnx", "/m:$MaxCpuCount", "/nr:false", "/v:minimal", "/t:Rebuild",
        "/p:Configuration=$Configuration", "/p:Platform=$Platform",
        "/p:MajorVersion=$($versionMatch.Groups[1].Value)",
        "/p:MinorVersion=$($versionMatch.Groups[2].Value)",
        "/p:PatchVersion=$($versionMatch.Groups[3].Value)",
        "/p:VersionString=$Version", "/p:CommitId=$shortCommit",
        "/p:PreferredToolArchitecture=x64",
        "/p:WindowsSdkDir=D:\Windows Kits\10\",
        "/p:SDKReferenceDirectoryRoot=D:\Windows Kits\10\Extension SDKs\",
        "/p:SDKExtensionDirectoryRoot=D:\Windows Kits\10\Extension SDKs\",
        "/p:UseMultiToolTask=false", "/p:CL_MPCount=1",
        "/p:DisablePDB=true", "/p:ReproducibleBuild=true"
    )

    Push-Location $sourceRoot
    try {
        & $msbuild @msbuildArgs
        if ($LASTEXITCODE -ne 0) {
            throw "Release build failed with exit code $LASTEXITCODE."
        }
    } finally {
        Pop-Location
    }
}

$requiredRuntimePaths = @(
    "Magpie.exe",
    "resources.pri",
    "Microsoft.UI.Xaml.dll",
    "TouchHelper.exe",
    "Updater.exe",
    "effects"
)
$missingRuntimePaths = @($requiredRuntimePaths | Where-Object {
    !(Test-Path -LiteralPath (Join-Path $buildOutput $_))
})
if ($missingRuntimePaths.Count -ne 0) {
    throw "Release runtime layout is incomplete in $buildOutput. Missing: $($missingRuntimePaths -join ', ')"
}

New-Item -ItemType Directory -Path $releaseContainer -Force | Out-Null
if (Test-Path -LiteralPath $stagingDir) {
    # Keep the staging root itself. Explorer, antivirus and recently exited
    # GUI processes can briefly retain a handle to the directory even after
    # every packaged file is released.
    Get-ChildItem -LiteralPath $stagingDir -Force |
        Remove-Item -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $stagingDir | Out-Null
}

Get-ChildItem -LiteralPath $buildOutput | Where-Object {
    $_.Extension -notin ".pdb", ".lib", ".exp" -and
    $_.Name -ne "Magpie.next.exe"
} | Copy-Item -Destination $stagingDir -Recurse

# Never package per-user runtime state left in bin/ by local test launches.
foreach ($runtimeStateName in @("cache", "logs")) {
    $runtimeStatePath = Join-Path $stagingDir $runtimeStateName
    if (Test-Path -LiteralPath $runtimeStatePath) {
        Remove-Item -LiteralPath $runtimeStatePath -Recurse -Force
    }
}

if ($ExcludeTensorRTDepthRuntime) {
    foreach ($optionalPath in @(
        "FrameGuidance\TensorRT",
        "NVIDIA-TensorRT-Runtime-Licenses"
    )) {
        $fullOptionalPath = Join-Path $stagingDir $optionalPath
        if (Test-Path -LiteralPath $fullOptionalPath) {
            Remove-Item -LiteralPath $fullOptionalPath -Recurse -Force
        }
    }
}

Copy-Item -LiteralPath (Join-Path $sourceRoot "LICENSE") `
    -Destination (Join-Path $stagingDir "LICENSE-Magpie.txt")
Copy-Item -LiteralPath (Join-Path $sourceRoot "docs\README-EXPERIMENTAL-RELEASE.txt") `
    -Destination (Join-Path $stagingDir "README-Experimental.txt")
$packagedReadmePath = Join-Path $stagingDir "README-Experimental.txt"
$packagedReadme = Get-Content -LiteralPath $packagedReadmePath -Raw -Encoding UTF8
$packagedReadme = $packagedReadme -replace '^Magpie Experimental v[^ ]+ x64', "Magpie Experimental v$Version x64"
Set-Content -LiteralPath $packagedReadmePath -Value $packagedReadme -Encoding UTF8 -NoNewline
Copy-Item -LiteralPath (Join-Path $sourceRoot "docs\THIRD_PARTY_AND_REDISTRIBUTION.md") `
    -Destination (Join-Path $stagingDir "THIRD-PARTY-NOTICES.md")

$featureOptions = [ordered]@{}
$userOptionsPath = Join-Path $sourceRoot "src\BuildOptions.props.user"
if (Test-Path -LiteralPath $userOptionsPath) {
    [xml]$userOptions = Get-Content -LiteralPath $userOptionsPath
    $properties = $userOptions.Project.PropertyGroup.ChildNodes | Where-Object {
        $_.Name -like "Enable*"
    }
    foreach ($property in $properties) {
        $featureOptions[$property.Name] = [string]$property.InnerText
    }
}

$fileRecords = Get-ChildItem -LiteralPath $stagingDir -File -Recurse | Sort-Object FullName | ForEach-Object {
    $relativePath = $_.FullName.Substring($stagingDir.Length).TrimStart('\', '/')
    [ordered]@{
        path = $relativePath.Replace('\', '/')
        bytes = $_.Length
        sha256 = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash
        fileVersion = $_.VersionInfo.FileVersion
        productVersion = $_.VersionInfo.ProductVersion
    }
}

$sourceDate = if ($commitTimeText) {
    [DateTimeOffset]::Parse($commitTimeText).UtcDateTime
} else {
    [DateTime]::SpecifyKind([DateTime]"1970-01-01", [DateTimeKind]::Utc)
}
$manifest = [ordered]@{
    schemaVersion = 1
    package = $PackageName
    version = $Version
    commit = $commit
    sourceDirty = $sourceDirty
    sourceDateUtc = $sourceDate.ToString("o")
    configuration = $Configuration
    platform = $Platform
    featureOptions = $featureOptions
    files = @($fileRecords)
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content `
    -LiteralPath (Join-Path $stagingDir "build-manifest.json") -Encoding utf8

# Normalize staging timestamps to the source commit time. Combined with the
# manifest this makes repeated packages comparable and avoids local wall-clock
# metadata in the archive.
Get-ChildItem -LiteralPath $stagingDir -Recurse -Force | ForEach-Object {
    $item = $_
    $normalized = $false
    for ($attempt = 1; $attempt -le 5; ++$attempt) {
        try {
            $item.LastWriteTimeUtc = $sourceDate
            $normalized = $true
            break
        } catch {
            if ($attempt -lt 5) { Start-Sleep -Milliseconds 200 }
        }
    }
    if (!$normalized) {
        Write-Warning "Could not normalize timestamp for $($item.FullName); ZIP entry time remains normalized."
    }
}
# The staging root can remain briefly open in Explorer or by a recently exited
# GUI process. ZIP entries are created from its children, so the root timestamp
# does not affect archive reproducibility.
try {
    (Get-Item -LiteralPath $stagingDir).LastWriteTimeUtc = $sourceDate
} catch [System.IO.IOException] {
    Write-Warning "Could not normalize the staging root timestamp: $($_.Exception.Message)"
}

if (Test-Path -LiteralPath $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [System.IO.Compression.ZipFile]::Open(
    $zipPath, [System.IO.Compression.ZipArchiveMode]::Create)
try {
    Get-ChildItem -LiteralPath $stagingDir -File -Recurse | Sort-Object FullName | ForEach-Object {
        $relativePath = $_.FullName.Substring($stagingDir.Length).TrimStart('\', '/').Replace('\', '/')
        $entryName = "$PackageName/$relativePath"
        $entry = $archive.CreateEntry(
            $entryName, [System.IO.Compression.CompressionLevel]::Optimal)
        $entry.LastWriteTime = [DateTimeOffset]$sourceDate

        $inputStream = [System.IO.File]::OpenRead($_.FullName)
        $outputStream = $entry.Open()
        try {
            $inputStream.CopyTo($outputStream)
        } finally {
            $outputStream.Dispose()
            $inputStream.Dispose()
        }
    }
} finally {
    $archive.Dispose()
}

$zip = Get-Item -LiteralPath $zipPath
$hash = Get-FileHash -LiteralPath $zipPath -Algorithm SHA256
Write-Host "Version:           $Version"
Write-Host "Commit:            $commit"
Write-Host "Release directory: $stagingDir"
Write-Host "Release ZIP:       $zipPath"
Write-Host "ZIP bytes:         $($zip.Length)"
Write-Host "SHA256:            $($hash.Hash)"
