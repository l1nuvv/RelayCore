param(
    [ValidateSet('Patch', 'Minor', 'Major')][string]$Bump = 'Patch',
    [switch]$Publish,
    [switch]$Release
)
$ErrorActionPreference = 'Stop'
if ($Publish -and $Release) { throw 'Use either -Publish or -Release.' }
. (Join-Path $PSScriptRoot 'release-functions.ps1')
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$stateDirectory = Join-Path $projectRoot 'build/release'
New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
try {
    $releaseLock = [IO.File]::Open((Join-Path $stateDirectory 'release.lock'), [IO.FileMode]::OpenOrCreate, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
} catch { throw 'Another release command is running, or build/release is not writable.' }
try {
$statePath = Join-Path $stateDirectory 'candidate.json'
$versionPath = Join-Path $projectRoot 'VERSION'
$changesPath = Join-Path $projectRoot 'release/changes.json'
$state = if (Test-Path -LiteralPath $statePath) { Get-Content -LiteralPath $statePath -Raw | ConvertFrom-Json } else { $null }

function Save-Candidate {
    $state | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath ($statePath + '.tmp') -Encoding utf8NoBOM
    Move-Item -LiteralPath ($statePath + '.tmp') -Destination $statePath -Force
}

function Publish-Candidate {
    if (-not $state -or $state.status -ne 'prepared') { throw 'Prepare a candidate first: pwsh -File scripts/release.ps1' }
    if ((Get-Content -LiteralPath $versionPath -Raw).Trim() -ne $state.version -or
        (Get-Content -LiteralPath (Join-Path $projectRoot 'vcpkg.json') -Raw | ConvertFrom-Json).'version-string' -ne $state.version) {
        throw 'VERSION or vcpkg.json differs from the prepared candidate.'
    }
    if ((Get-FileHash -LiteralPath $state.archive).Hash -ne $state.sha256 -or
        (Get-FileHash -LiteralPath $state.notes).Hash -ne $state.notes_sha256) { throw 'Prepared archive or notes changed. Prepare again before publishing.' }
    & (Join-Path $PSScriptRoot 'publish-release.ps1') -Archive $state.archive -Notes $state.notes -Publish
    $state.status = 'published'; Save-Candidate
    Write-Output "Published $($state.version). The source repository was not committed or pushed."
}

if ($Publish) {
    if ($state -and $state.status -eq 'published') {
        Write-Output "Candidate $($state.version) was already published. Update release/changes.json to prepare the next release."
        exit 0
    }
    Publish-Candidate
    exit 0
}

$current = (Get-Content -LiteralPath $versionPath -Raw).Trim()
$changesHash = (Get-FileHash -LiteralPath $changesPath).Hash
if ($Release -and $state -and $state.changes_sha256 -eq $changesHash) {
    if ($state.status -eq 'prepared') {
        if ($state.bump -ne $Bump) { throw "Pending candidate uses $($state.bump). Retry with that increment." }
        Publish-Candidate
        exit 0
    }
    if ($state.status -eq 'published' -and $current -eq $state.version) {
        Write-Output "Release $current was already published. Update release/changes.json for the next release."
        exit 0
    }
}
if ($state -and $state.status -ne 'published') {
    if ($current -notin @($state.base_version, $state.version)) { throw 'VERSION differs from the pending candidate. Resolve the version before retrying.' }
    if ($state.bump -ne $Bump) { throw "Pending candidate uses $($state.bump). Retry with that increment." }
    $next = $state.version
} else {
    if ($state -and $state.changes_sha256 -eq $changesHash) { throw 'Update release/changes.json with the next real changes before releasing again.' }
    $next = Get-NextReleaseVersion $current $Bump
}
$notes = New-ReleaseNotes (Get-Content -LiteralPath $changesPath -Raw | ConvertFrom-Json) $next
& (Join-Path $projectRoot 'tests/release_scripts_tests.ps1')
Import-ReleaseBuildEnvironment
New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
$notesPath = Join-Path $stateDirectory ("release-$next.md")
$baseVersion = if ($state -and $state.status -ne 'published') { $state.base_version } else { $current }
$state = [ordered]@{
    status = 'preparing'; base_version = $baseVersion; version = $next; bump = $Bump
    changes_sha256 = $changesHash; archive = ''; sha256 = ''; notes = $notesPath; notes_sha256 = ''
    timings_seconds = [ordered]@{}
}
Save-Candidate
Set-ReleaseVersion $projectRoot $next
[IO.File]::WriteAllText($notesPath, $notes, [Text.UTF8Encoding]::new($false))
Push-Location $projectRoot
try {
    $timer = [Diagnostics.Stopwatch]::StartNew()
    & cmake --preset msvc-release
    if ($LASTEXITCODE -ne 0) { throw 'Release configure failed. Run the same command to retry without another version increment.' }
    $state.timings_seconds.configure = [math]::Round($timer.Elapsed.TotalSeconds, 2)
    $timer.Restart()
    & cmake --build --preset demo-release --target relaycore_demo relaycore_updater_tests relaycore_content_validation
    if ($LASTEXITCODE -ne 0) { throw 'Release build failed. The pending version is retained for retry.' }
    $state.timings_seconds.build = [math]::Round($timer.Elapsed.TotalSeconds, 2)
    $timer.Restart()
    & ctest --test-dir build/msvc-release -R '^(relaycore\.updater$|validation\.)' --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Release acceptance tests failed. Nothing was published.' }
    $state.timings_seconds.tests = [math]::Round($timer.Elapsed.TotalSeconds, 2)
    $timer.Restart()
    $packaged = & (Join-Path $PSScriptRoot 'package-playtest.ps1') -Preset msvc-release -PassThru
    $package = $packaged | Where-Object { $_ -isnot [string] }
    $packaged | Where-Object { $_ -is [string] } | Write-Output
    if (@($package).Count -ne 1) { throw 'Packaging did not return one verified archive.' }
    $state.timings_seconds.package = [math]::Round($timer.Elapsed.TotalSeconds, 2)
    $timer.Restart()
    & (Join-Path $projectRoot 'build/msvc-release/tests/relaycore_updater_tests.exe') $package.Archive
    if ($LASTEXITCODE -ne 0) { throw 'Packaged game installation/startup failed. Nothing was published.' }
    $state.timings_seconds.restart = [math]::Round($timer.Elapsed.TotalSeconds, 2)
    $state.archive = $package.Archive; $state.sha256 = $package.Sha256
    $state.notes_sha256 = (Get-FileHash -LiteralPath $notesPath).Hash
    $state.status = 'prepared'; Save-Candidate
    & (Join-Path $PSScriptRoot 'publish-release.ps1') -Archive $state.archive -Notes $notesPath
    if ($Release) {
        Publish-Candidate
    } else {
        Write-Output "Prepared $next. Review $notesPath and $($state.archive)."
        Write-Output 'To publish this exact candidate: pwsh -NoProfile -File scripts/release.ps1 -Publish'
    }
} finally { Pop-Location }
} finally { $releaseLock.Dispose() }
