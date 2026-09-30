$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('RelayCore-precommit-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Write-Fixture([string]$Path, [string]$Content) {
    [IO.File]::WriteAllText((Join-Path $fixtureRoot $Path), $Content, [Text.UTF8Encoding]::new($false))
}
function Invoke-Git([string[]]$Arguments) {
    & git @Arguments | Out-Null
    if ($LASTEXITCODE -ne 0) { throw ('Fixture Git command failed: ' + ($Arguments -join ' ')) }
}
function Run-Hook([bool]$Success) {
    $output = & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $projectRoot 'tools/pre-commit.ps1') 2>&1
    Assert (($LASTEXITCODE -eq 0) -eq $Success) ("Unexpected hook result: " + ($output -join "`n"))
    return $output -join "`n"
}
Copy-Item -LiteralPath (Join-Path $projectRoot '.clang-format') -Destination (Join-Path $fixtureRoot '.clang-format')
Push-Location $fixtureRoot
try {
    Invoke-Git @('init', '--quiet')
    Write-Fixture 'first.cpp' 'int first(){return 1;}'
    Write-Fixture 'second.cpp' 'int second(){return 2;}'
    Invoke-Git @('add', '--', 'first.cpp', 'second.cpp')
    $output = Run-Hook $false
    Assert ($output.Contains('Staged C/C++ files need formatting:') -and $output.Contains(' - first.cpp') -and $output.Contains(' - second.cpp')) 'Hook did not report all files needing formatting.'
    & clang-format -i --style=file -- first.cpp second.cpp
    Assert ($LASTEXITCODE -eq 0) 'Fixture formatting failed.'
    $null = Run-Hook $false
    Invoke-Git @('add', '--', 'first.cpp', 'second.cpp')
    $null = Run-Hook $true

    Write-Fixture 'changes.json' '{invalid'
    Invoke-Git @('add', '--', 'changes.json')
    Write-Fixture 'changes.json' '{"title":"Исправленный рабочий файл"}'
    $output = Run-Hook $false
    Assert ($output.Contains('Invalid JSON in staged files:')) 'Hook accepted invalid staged JSON after the working file was fixed.'
    Invoke-Git @('add', '--', 'changes.json')
    Write-Fixture 'changes.json' '{invalid working copy'
    $null = Run-Hook $true
    Remove-Item -LiteralPath (Join-Path $fixtureRoot 'changes.json')
    $null = Run-Hook $true

    Write-Fixture 'notes.txt' "First line`n<<<<<<< HEAD`nleft`n=======`nright`n>>>>>>> branch`n"
    Invoke-Git @('add', '--', 'notes.txt')
    Write-Fixture 'notes.txt' 'Resolved working copy'
    $output = Run-Hook $false
    Assert ($output.Contains('Conflict markers found in staged files:')) 'Hook missed conflict markers after the first line of staged text.'
    Invoke-Git @('add', '--', 'notes.txt')
    Write-Fixture 'notes.txt' "First line`n<<<<<<< working copy`n"
    $null = Run-Hook $true
    foreach ($emptyJson in @('', " `t`n")) {
        Write-Fixture 'changes.json' $emptyJson
        Invoke-Git @('add', '--', 'changes.json')
        $output = Run-Hook $false
        Assert ($output.Contains('Invalid JSON in staged files:')) 'Hook accepted an empty JSON document.'
    }
    Write-Fixture 'changes.json' 'null'
    Invoke-Git @('add', '--', 'changes.json')
    $null = Run-Hook $true
    [IO.File]::WriteAllText((Join-Path $fixtureRoot 'changes.json'), '{"title":"Проверка UTF-8"}', [Text.UTF8Encoding]::new($true))
    Invoke-Git @('add', '--', 'changes.json')
    $null = Run-Hook $true
    "pre_commit=passed; fixture=$fixtureRoot"
} finally { Pop-Location }
