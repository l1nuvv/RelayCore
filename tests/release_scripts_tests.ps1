$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot '../scripts/release-functions.ps1')
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Reject([scriptblock]$Action) {
    $rejected = $false
    try { & $Action | Out-Null } catch { $rejected = $true }
    Assert $rejected 'Invalid release input was accepted.'
}
Assert ((Get-NextReleaseVersion '1.3.1' Patch) -eq '1.3.2') 'Patch increment failed.'
Assert ((Get-NextReleaseVersion '1.3.9' Minor) -eq '1.4.0') 'Minor must reset patch.'
Assert ((Get-NextReleaseVersion '1.3.9' Major) -eq '2.0.0') 'Major must reset minor and patch.'
foreach ($bad in @('1.3', '1.3.1-preview', '01.3.1', '1.3.999999')) { Reject { Get-NextReleaseVersion $bad Patch } }
Reject { Get-NextReleaseVersion '1.3.65535' Patch }
$changes = [pscustomobject]@{ title='Проверка'; added=@('Новое действие.'); changed=@(); fixed=@('Исправлена ошибка.') }
$notes = New-ReleaseNotes $changes '1.3.2'
Assert ($notes.Contains('## Новое') -and $notes.Contains('## Исправления')) 'Sections missing.'
Assert (-not $notes.Contains('## Улучшения')) 'Empty section was generated.'
Assert ($notes.Contains('1.3.2') -and $notes.Contains('Исправлена ошибка.')) 'Version or authored change was lost.'
$changes.added=@(); $changes.fixed=@()
Reject { New-ReleaseNotes $changes '1.3.2' }
$changes.added=@("Text`nUnexpected section")
Reject { New-ReleaseNotes $changes '1.3.2' }
$changes.added=@(('a' * 2100))
Reject { New-ReleaseNotes $changes '1.3.2' }
'release_scripts=passed'
