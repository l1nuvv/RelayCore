param(
    [ValidateSet('msvc-relwithdebinfo', 'msvc-release')][string]$Preset = 'msvc-relwithdebinfo',
    [switch]$PassThru
)
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$source = Join-Path $projectRoot ("build/$Preset/src/relaygame")
$buildPreset = if ($Preset -eq 'msvc-release') { 'demo-release' } else { 'demo-relwithdebinfo' }
$packageName = 'RelayCore-playtest-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')
$outputRoot = Join-Path $projectRoot 'build/playtest'
$package = Join-Path $outputRoot $packageName
$archive = Join-Path $outputRoot ($packageName + '.zip')

foreach ($required in @('RelayCore.exe', 'RelayCoreUpdater.exe', 'updates.json', 'assets/config/engine.toml', 'assets/levels/power_restoration.toml')) {
    if (-not (Test-Path -LiteralPath (Join-Path $source $required) -PathType Leaf)) {
        throw "Missing playtest file: $required. Build with: cmake --build --preset $buildPreset"
    }
}

$builtVersion = (Get-Content -LiteralPath (Join-Path $source 'updates.json') -Raw | ConvertFrom-Json).version
if ($builtVersion -ne (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()) {
    throw "Build version is stale. Build with: cmake --build --preset $buildPreset"
}

$authoredRoot = Join-Path $projectRoot 'assets'
$authoredAssets = @(Get-ChildItem -LiteralPath $authoredRoot -Recurse -File)
$shaderRoot = Join-Path $projectRoot "build/$Preset/generated/shaders"
$runtimeShaders = @(Get-Content -LiteralPath (Join-Path $shaderRoot 'runtime-shaders.txt'))
$stagedAssets = @(Get-ChildItem -LiteralPath (Join-Path $source 'assets') -Recurse -File)
if ($runtimeShaders.Count -eq 0 -or $authoredAssets.Count + $runtimeShaders.Count -ne $stagedAssets.Count) {
    throw "Staged assets are stale. Build with: cmake --build --preset $buildPreset"
}
foreach ($shader in $runtimeShaders) {
    $compiled = Join-Path $shaderRoot $shader
    $staged = Join-Path $source "assets/shaders/vulkan/$shader"
    if (-not (Test-Path -LiteralPath $compiled -PathType Leaf) -or
        -not (Test-Path -LiteralPath $staged -PathType Leaf) -or
        (Get-FileHash -LiteralPath $compiled).Hash -ne (Get-FileHash -LiteralPath $staged).Hash) {
        throw "Staged shader differs: $shader. Build with: cmake --build --preset $buildPreset"
    }
}
foreach ($file in $authoredAssets) {
    $relative = $file.FullName.Substring($authoredRoot.Length + 1)
    $staged = Join-Path (Join-Path $source 'assets') $relative
    if (-not (Test-Path -LiteralPath $staged -PathType Leaf) -or
        (Get-FileHash -LiteralPath $file.FullName).Hash -ne (Get-FileHash -LiteralPath $staged).Hash) {
        throw "Staged asset differs: $relative. Build with: cmake --build --preset $buildPreset"
    }
}

New-Item -ItemType Directory -Path $package | Out-Null
Copy-Item -LiteralPath (Join-Path $source 'RelayCore.exe') -Destination $package
Copy-Item -LiteralPath (Join-Path $source 'RelayCoreUpdater.exe') -Destination $package
Copy-Item -LiteralPath (Join-Path $source 'updates.json') -Destination $package
Get-ChildItem -LiteralPath $source -Filter '*.dll' -File | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $package
}
Copy-Item -LiteralPath (Join-Path $source 'assets') -Destination $package -Recurse
Copy-Item -LiteralPath (Join-Path $projectRoot 'docs/PLAYTEST_GUIDE.txt') -Destination $package

$payload = @(Get-ChildItem -LiteralPath $package -Recurse -File | Sort-Object FullName | ForEach-Object {
    $relative = $_.FullName.Substring($package.Length + 1)
    $original = if ($relative -eq 'PLAYTEST_GUIDE.txt') {
        Join-Path $projectRoot 'docs/PLAYTEST_GUIDE.txt'
    } else {
        Join-Path $source $relative
    }
    $hash = (Get-FileHash -LiteralPath $_.FullName).Hash
    if ($hash -ne (Get-FileHash -LiteralPath $original).Hash) {
        throw "Packaged file differs: $relative"
    }
    [ordered]@{ path = $relative.Replace('\', '/'); bytes = $_.Length; sha256 = $hash }
})
$revision = git -C $projectRoot rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not record source revision.' }
$changes = @(git -C $projectRoot status --short --untracked-files=all)
if ($LASTEXITCODE -ne 0) { throw 'Could not record working-tree state.' }
$sourcePaths = @(git -c core.quotepath=false -C $projectRoot ls-files --cached --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'Could not enumerate source state.' }
$sourceHashes = foreach ($relative in ($sourcePaths | Sort-Object -Unique)) {
    $path = Join-Path $projectRoot $relative
    if (Test-Path -LiteralPath $path -PathType Leaf) {
        $relative + ' ' + (Get-FileHash -LiteralPath $path).Hash
    } else {
        $relative + ' MISSING'
    }
}
$hasher = [Security.Cryptography.SHA256]::Create()
try {
    $sourceDigest = [BitConverter]::ToString($hasher.ComputeHash(
        [Text.Encoding]::UTF8.GetBytes(($sourceHashes -join "`n")))).Replace('-', '')
} finally { $hasher.Dispose() }
[ordered]@{
    candidate = $packageName
    created_utc = [DateTime]::UtcNow.ToString('o')
    source_revision = $revision
    source_state_sha256 = $sourceDigest
    source_changes = $changes
    build_preset = $buildPreset
    version = $builtVersion
    files = $payload
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $package 'BUILD_MANIFEST.json') -Encoding utf8

Compress-Archive -LiteralPath $package -DestinationPath $archive
$zip = [IO.Compression.ZipFile]::OpenRead($archive)
$hasher = [Security.Cryptography.SHA256]::Create()
try {
    $packageFiles = @(Get-ChildItem -LiteralPath $package -Recurse -File)
    $entries = @($zip.Entries | Where-Object { $_.Name -ne '' })
    if ($entries.Count -ne $packageFiles.Count) { throw 'ZIP file count differs from package.' }
    foreach ($file in $packageFiles) {
        $relative = $file.FullName.Substring($package.Length + 1).Replace('\', '/')
        $entry = $zip.GetEntry($packageName + '/' + $relative)
        if ($null -eq $entry) { throw "ZIP is missing $relative" }
        $stream = $entry.Open()
        try { $hash = [BitConverter]::ToString($hasher.ComputeHash($stream)).Replace('-', '') }
        finally { $stream.Dispose() }
        if ($hash -ne (Get-FileHash -LiteralPath $file.FullName).Hash) {
            throw "ZIP content differs: $relative"
        }
    }
} finally { $hasher.Dispose(); $zip.Dispose() }
$archiveHash = (Get-FileHash -LiteralPath $archive).Hash
Set-Content -LiteralPath ($archive + '.sha256') -Value ($archiveHash + '  ' + [IO.Path]::GetFileName($archive))
Write-Output "Playtest directory: $package"
Write-Output "Playtest archive: $archive"
Write-Output "Verified files: $($packageFiles.Count); SHA256: $archiveHash"
if ($PassThru) { [pscustomobject]@{ Archive = $archive; Directory = $package; Sha256 = $archiveHash; Files = $packageFiles.Count } }
