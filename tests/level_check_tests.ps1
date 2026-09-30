param([Parameter(Mandatory)][string]$Executable)
$ErrorActionPreference = 'Stop'
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$projectRoot = Split-Path -Parent $PSScriptRoot
$env:RELAYCORE_ASSET_ROOT = Join-Path $projectRoot 'assets'
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('relaycore-level-check-' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture) | Out-Null
function Assert-Check([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
try {
    $map = Join-Path $projectRoot 'assets/levels/development_sandbox.toml'
    $report = (& $Executable $map) -join "`n" | ConvertFrom-Json
    Assert-Check ($LASTEXITCODE -eq 0 -and $report.valid) 'Valid sandbox must load without a display.'
    Assert-Check ($report.counts.enemies -eq 2 -and $report.counts.turrets -eq 5) 'Report must include prepared combat objects.'
    Assert-Check ($report.entities.Count -eq $report.counts.entities) 'Report omitted authored entities.'
    Assert-Check (@($report.entities | Where-Object { $_.source.line -gt 0 }).Count -eq $report.entities.Count) 'Report omitted source locations.'
    Assert-Check (@($report.bindings | Where-Object { $_.target -eq 'device_gate' -and $_.resolved_count -gt 0 }).Count -ge 1) 'Report omitted the resolved card reader link.'
    $entry = (& $Executable $map --entry arena) -join "`n" | ConvertFrom-Json
    Assert-Check ($LASTEXITCODE -eq 0 -and $entry.spawn[0] -eq 4 -and $entry.spawn[2] -eq 9) 'Named entry was not applied.'
    $invalidEntry = (& $Executable $map --entry missing) -join "`n" | ConvertFrom-Json
    Assert-Check ($LASTEXITCODE -eq 1 -and -not $invalidEntry.valid -and $invalidEntry.error -match 'missing level_entry') 'Missing entry needs a useful failure.'
    $invalid = Join-Path $fixture 'invalid.toml'
    [IO.File]::WriteAllText($invalid, "[[entity]]`nclassname = [")
    $parseError = (& $Executable $invalid) -join "`n" | ConvertFrom-Json
    Assert-Check ($LASTEXITCODE -eq 1 -and -not $parseError.valid -and $parseError.error) 'Invalid TOML must fail.'
    [IO.File]::WriteAllText($invalid, "[[entity]]`nclassname = 'render_light'`norigin = [0,2,0]`nrequired_target = 'missing_power'`n")
    $linkError = (& $Executable $invalid) -join "`n" | ConvertFrom-Json
    Assert-Check ($LASTEXITCODE -eq 1 -and -not $linkError.valid -and $linkError.error -match 'missing_power') 'Invalid activation link must fail.'
    [IO.File]::WriteAllText($invalid, "[[entity]]`nclassname = 'level_exit'`norigin = [0,0,0]`nsize = [2,2,2]`nnext_map = './absent.toml'`nnext_entry = 'entry'`n")
    $exitError = (& $Executable $invalid) -join "`n" | ConvertFrom-Json
    Assert-Check ($LASTEXITCODE -eq 1 -and -not $exitError.valid) 'Missing destination must fail before playing.'
    Write-Host 'level_check_tests: passed'
} finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
