function Get-NextReleaseVersion([string]$Current, [string]$Bump) {
    if ($Current -notmatch '^(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})\.(0|[1-9][0-9]{0,4})$') {
        throw 'VERSION must contain a canonical MAJOR.MINOR.PATCH number.'
    }
    $parts = @($Current.Split('.') | ForEach-Object { [int]$_ })
    switch ($Bump) {
        'Patch' { $parts[2]++ }
        'Minor' { $parts[1]++; $parts[2] = 0 }
        'Major' { $parts[0]++; $parts[1] = 0; $parts[2] = 0 }
        default { throw 'Unknown version increment.' }
    }
    if (@($parts | Where-Object { $_ -gt 65535 }).Count) { throw 'Version component exceeds 65535.' }
    return $parts -join '.'
}

function New-ReleaseNotes($Changes, [string]$Version) {
    if ($Changes.title -isnot [string] -or $Changes.title.Trim().Length -eq 0 -or $Changes.title -match '[\r\n]') {
        throw 'release/changes.json needs a one-line title.'
    }
    $lines = [Collections.Generic.List[string]]::new()
    $lines.Add("# RelayCore $Version — $($Changes.title.Trim())")
    $count = 0
    foreach ($section in @(@('added', 'Новое'), @('changed', 'Улучшения'), @('fixed', 'Исправления'))) {
        $items = $Changes.($section[0])
        if ($null -eq $items -or $items -isnot [array]) { throw "Expected an array: $($section[0])" }
        if ($items.Count -eq 0) { continue }
        $lines.Add(''); $lines.Add('## ' + $section[1])
        foreach ($item in $items) {
            if ($item -isnot [string] -or $item.Trim().Length -eq 0 -or $item -match '[\r\n]') {
                throw 'Each change must be a nonempty single-line description for the player.'
            }
            $lines.Add('- ' + $item.Trim()); $count++
        }
    }
    if ($count -eq 0) { throw 'Describe at least one real change before preparing a release.' }
    $lines.Add(''); $lines.Add('Windows 11 x64. Сохранения и настройки остаются на месте. Для первого запуска распакуйте ZIP; следующие обновления доступны из меню игры.')
    $markdown = ($lines -join "`n") + "`n"
    if ($markdown.Length -gt 2000) { throw 'Release notes exceed the game limit of 2000 characters. Shorten the descriptions.' }
    return $markdown
}

function Set-ReleaseVersion([string]$ProjectRoot, [string]$Version) {
    $manifestPath = Join-Path $ProjectRoot 'vcpkg.json'
    $manifestText = [IO.File]::ReadAllText($manifestPath)
    $manifest = $manifestText | ConvertFrom-Json
    $versionPattern = '("version-string"\s*:\s*")[^"]+(")'
    if ($manifest.'version-string' -isnot [string] -or [regex]::Matches($manifestText, $versionPattern).Count -ne 1) {
        throw 'Expected one version-string in vcpkg.json.'
    }
    $manifestText = [regex]::Replace($manifestText, $versionPattern, { param($match) $match.Groups[1].Value + $Version + $match.Groups[2].Value })
    [IO.File]::WriteAllText($manifestPath, $manifestText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText((Join-Path $ProjectRoot 'VERSION'), $Version + "`n", [Text.UTF8Encoding]::new($false))
}

function Import-ReleaseBuildEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) { throw 'Visual Studio Installer (vswhere.exe) was not found.' }
    $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    if ($LASTEXITCODE -ne 0 -or -not $installation) { throw 'Install Visual Studio C++ x64 build tools.' }
    $developer = Join-Path $installation 'Common7/Tools/VsDevCmd.bat'
    $environment = & cmd /d /s /c ('"' + $developer + '" -no_logo -arch=x64 -host_arch=x64 >nul && set')
    if ($LASTEXITCODE -ne 0) { throw 'Visual Studio environment initialization failed.' }
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') { [Environment]::SetEnvironmentVariable($Matches[1], $Matches[2], 'Process') }
    }
    if (-not $env:VCPKG_ROOT) { $env:VCPKG_ROOT = Join-Path $installation 'VC/vcpkg' }
    if (-not (Test-Path -LiteralPath (Join-Path $env:VCPKG_ROOT 'scripts/buildsystems/vcpkg.cmake'))) {
        throw 'Set VCPKG_ROOT to an existing vcpkg installation.'
    }
}
