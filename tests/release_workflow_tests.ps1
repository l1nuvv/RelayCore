param([string]$Java)
$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$fixture = Join-Path $projectRoot ('build/release/script-tests-' + [guid]::NewGuid().ToString('N'))
foreach ($directory in @('scripts', 'tests', 'release', 'build/msvc-release/tests')) {
    New-Item -ItemType Directory -Path (Join-Path $fixture $directory) -Force | Out-Null
}
function Assert($Condition, [string]$Message) { if (-not $Condition) { throw $Message } }
function Write-Fixture([string]$Path, [string]$Content) {
    [IO.File]::WriteAllText((Join-Path $fixture $Path), $Content, [Text.UTF8Encoding]::new($false))
}
Copy-Item -LiteralPath (Join-Path $projectRoot 'scripts/release.ps1') -Destination (Join-Path $fixture 'scripts/release.ps1')
Copy-Item -LiteralPath (Join-Path $projectRoot 'scripts/release-functions.ps1') -Destination (Join-Path $fixture 'scripts/release-functions.ps1')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'release_scripts_tests.ps1') -Destination (Join-Path $fixture 'tests/release_scripts_tests.ps1')
Add-Content -LiteralPath (Join-Path $fixture 'scripts/release-functions.ps1') -Value 'function Import-ReleaseBuildEnvironment {}'
Write-Fixture 'VERSION' "1.3.2`n"
Write-Fixture 'vcpkg.json' '{"name":"relaycore","version-string":"1.3.2","builtin-baseline":"keep"}'
Write-Fixture 'release/changes.json' '{"title":"Тестовый выпуск","added":["Новое действие."],"changed":[],"fixed":[]}'
Write-Fixture 'runner.ps1' @'
param([switch]$Release, [switch]$Publish, [string]$Bump = 'Patch')
$ErrorActionPreference = 'Stop'
function cmake {
    Add-Content -LiteralPath (Join-Path $PSScriptRoot 'events.txt') -Value 'cmake'
    $global:LASTEXITCODE = 0
    if ($args -contains '--build' -and (Test-Path (Join-Path $PSScriptRoot 'fail-build'))) { $global:LASTEXITCODE = 1 }
}
function ctest {
    Add-Content -LiteralPath (Join-Path $PSScriptRoot 'events.txt') -Value 'ctest'
    $global:LASTEXITCODE = [int](Test-Path (Join-Path $PSScriptRoot 'fail-tests'))
}
& (Join-Path $PSScriptRoot 'scripts/release.ps1') -Bump $Bump -Release:$Release -Publish:$Publish
'@
Write-Fixture 'scripts/package-playtest.ps1' @'
param($Preset, [switch]$PassThru)
$root = Split-Path $PSScriptRoot
Add-Content -LiteralPath (Join-Path $root 'events.txt') -Value 'package'
$archive = Join-Path $root 'payload.zip'
[IO.File]::WriteAllText($archive, (Get-Content (Join-Path $root 'VERSION') -Raw))
[pscustomobject]@{ Archive=$archive; Sha256=(Get-FileHash $archive).Hash }
'@
Write-Fixture 'scripts/publish-release.ps1' @'
param($Archive, $Notes, [switch]$Publish)
if ($Publish) {
    $root = Split-Path $PSScriptRoot
    Add-Content -LiteralPath (Join-Path $root 'events.txt') -Value 'publish'
    if (Test-Path (Join-Path $root 'fail-publish')) { throw 'Simulated upload failure.' }
}
'@
Write-Fixture 'startup.cs' @'
using System;
using System.IO;
class Startup {
    static int Main(string[] args) {
        string root = Path.GetDirectoryName(args[0]);
        File.AppendAllText(Path.Combine(root, "events.txt"), "startup\n");
        return File.Exists(Path.Combine(root, "fail-startup")) ? 1 : 0;
    }
}
'@
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /target:exe ("/out:" + (Join-Path $fixture 'build/msvc-release/tests/relaycore_updater_tests.exe')) (Join-Path $fixture 'startup.cs')
Assert ($LASTEXITCODE -eq 0) 'Could not compile the fixture startup probe.'
function Run-Release([string[]]$Arguments, [bool]$Success = $true) {
    $output = & (Join-Path $PSHOME 'pwsh.exe') -NoProfile -File (Join-Path $fixture 'runner.ps1') @Arguments 2>&1
    Assert (($LASTEXITCODE -eq 0) -eq $Success) ("Unexpected release result: " + ($output -join "`n"))
}
function State { Get-Content (Join-Path $fixture 'build/release/candidate.json') -Raw | ConvertFrom-Json }
function Version { (Get-Content (Join-Path $fixture 'VERSION') -Raw).Trim() }
function Events { @(Get-Content (Join-Path $fixture 'events.txt') -ErrorAction SilentlyContinue) }
function Change-Notes([string]$Text) {
    Write-Fixture 'release/changes.json' (@{title=$Text; added=@($Text); changed=@(); fixed=@()} | ConvertTo-Json)
}

Write-Fixture 'fail-build' ''
Run-Release @('-Release') $false
Assert ((Version) -eq '1.3.3' -and (State).status -eq 'preparing') 'Failed build must retain the pending version.'
Assert ((Events) -notcontains 'publish') 'Failed build reached publication.'
Remove-Item -LiteralPath (Join-Path $fixture 'fail-build')
Write-Fixture 'fail-tests' ''
Run-Release @('-Release') $false
Assert ((Version) -eq '1.3.3' -and (Events) -notcontains 'publish') 'Failed acceptance tests published or incremented again.'
Remove-Item -LiteralPath (Join-Path $fixture 'fail-tests')
Write-Fixture 'fail-startup' ''
Run-Release @('-Release') $false
Assert ((Version) -eq '1.3.3' -and (Events) -notcontains 'publish') 'Failed startup published or incremented again.'
Remove-Item -LiteralPath (Join-Path $fixture 'fail-startup')
Write-Fixture 'fail-publish' ''
Run-Release @('-Release') $false
Assert ((State).status -eq 'prepared' -and (State).base_version -eq '1.3.2') 'Upload failure lost the prepared candidate or original version.'
$buildCount = @((Events) | Where-Object { $_ -eq 'cmake' }).Count
Remove-Item -LiteralPath (Join-Path $fixture 'fail-publish')
Run-Release @('-Release')
Assert ((State).status -eq 'published' -and (Version) -eq '1.3.3') 'One-button retry did not publish the pending release.'
Assert (@((Events) | Where-Object { $_ -eq 'cmake' }).Count -eq $buildCount) 'Upload retry rebuilt the verified archive.'
$manifest = Get-Content (Join-Path $fixture 'vcpkg.json') -Raw | ConvertFrom-Json
Assert ($manifest.'version-string' -eq '1.3.3' -and $manifest.'builtin-baseline' -eq 'keep') 'Manifest version or unrelated metadata changed incorrectly.'
$notes = Get-Content (State).notes -Raw
Assert ($notes.Contains('RelayCore 1.3.3') -and $notes.Contains('Новое действие.') -and -not $notes.Contains('## Исправления')) 'Release description did not use the version and authored changes.'
$eventCount = (Events).Count
Run-Release @('-Release')
Assert ((Events).Count -eq $eventCount -and (Version) -eq '1.3.3') 'Repeated click must not rebuild, publish or increment an unchanged release.'
Change-Notes 'Следующий выпуск'
Run-Release @('-Release')
Assert ((State).status -eq 'published' -and (Version) -eq '1.3.4') 'Changed notes did not start the next complete release.'

Change-Notes 'Кандидат для проверки'
Run-Release @()
Assert ((State).status -eq 'prepared' -and (Version) -eq '1.3.5') 'Prepare-only command changed its behavior.'
$eventCount = (Events).Count
Run-Release @('-Release', '-Bump', 'Minor') $false
Assert ((Version) -eq '1.3.5' -and (Events).Count -eq $eventCount) 'Changed increment published or replaced the pending candidate.'
$notesText = Get-Content (State).notes -Raw
[IO.File]::WriteAllText((State).notes, 'tampered')
Run-Release @('-Release') $false
Assert ((Events).Count -eq $eventCount) 'Tampered notes reached publication.'
[IO.File]::WriteAllText((State).notes, $notesText, [Text.UTF8Encoding]::new($false))
$archiveText = Get-Content (State).archive -Raw
Write-Fixture 'payload.zip' 'tampered'
$eventCount = (Events).Count
Run-Release @('-Release') $false
Assert ((State).status -eq 'prepared' -and (Events).Count -eq $eventCount) 'Tampered archive reached publication.'
Write-Fixture 'payload.zip' $archiveText
Write-Fixture 'vcpkg.json' '{"version-string":"9.0.0"}'
Run-Release @('-Publish') $false
Assert ((Events).Count -eq $eventCount) 'Mismatched version reached publication.'
Write-Fixture 'vcpkg.json' '{"version-string":"1.3.5"}'
Run-Release @('-Publish')
Assert ((State).status -eq 'published') 'Publish-only command changed its behavior.'
Change-Notes 'Ошибочный manifest'
Write-Fixture 'vcpkg.json' '{"name":"relaycore"}'
Run-Release @('-Release') $false
Assert ((Version) -eq '1.3.5') 'Invalid manifest changed VERSION before validation.'
Write-Fixture 'vcpkg.json' '{"version-string":"1.3.5"}'
Run-Release @('-Release')
Assert ((Version) -eq '1.3.6' -and (State).status -eq 'published') 'Manifest repair did not resume the pending version.'
$configuration = [xml](Get-Content (Join-Path $projectRoot '.run/RelayCore Release.run.xml') -Raw)
$options = @{}
foreach ($option in $configuration.component.configuration.option) { $options[$option.name] = $option.value }
$interpreter = $options.INTERPRETER_PATH.Replace('$USER_HOME$', [Environment]::GetFolderPath('UserProfile'))
$script = $options.SCRIPT_PATH.Replace('$PROJECT_DIR$', $fixture)
Assert (Test-Path -LiteralPath $interpreter) 'CLion release interpreter was not found.'
Assert (((Get-Item -LiteralPath $interpreter).Attributes -band [IO.FileAttributes]::ReparsePoint) -eq 0) 'CLion needs the actual interpreter executable, not a Windows app execution alias.'
if ($Java) {
    Write-Fixture 'CheckInterpreter.java' @'
import java.nio.file.Files;
import java.nio.file.Path;
class CheckInterpreter {
    public static void main(String[] args) {
        Path path = Path.of(args[0]);
        if (!Files.isRegularFile(path) || !Files.isExecutable(path)) {
            System.err.println("CLion Java runtime rejects interpreter: " + path);
            System.exit(1);
        }
    }
}
'@
    & $Java (Join-Path $fixture 'CheckInterpreter.java') $interpreter
    Assert ($LASTEXITCODE -eq 0) 'Interpreter failed the CLion Java executable check.'
}
Assert (Test-Path -LiteralPath $script) 'CLion release script was not found.'
Assert ($options.SCRIPT_WORKING_DIRECTORY -eq '$PROJECT_DIR$' -and $options.EXECUTE_IN_TERMINAL -eq 'false') 'CLion release working directory or console is incorrect.'
$eventCount = (Events).Count
& $interpreter @($options.INTERPRETER_OPTIONS.Split(' ')) $script $options.SCRIPT_OPTIONS
Assert ($LASTEXITCODE -eq 0 -and (Events).Count -eq $eventCount) 'Command configured in CLion did not run the idempotent release.'
"release_workflow=passed; fixture=$fixture"
