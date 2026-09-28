param(
    [Parameter(Mandatory)][string]$Archive,
    [Parameter(Mandatory)][string]$Notes,
    [switch]$Publish,
    [switch]$CreateRepository
)
$ErrorActionPreference = 'Stop'
$archivePath = (Resolve-Path -LiteralPath $Archive).Path
$notesPath = (Resolve-Path -LiteralPath $Notes).Path
$releaseNotes = Get-Content -LiteralPath $notesPath -Raw
$zip = [IO.Compression.ZipFile]::OpenRead($archivePath)
try {
    $entries = @($zip.Entries | Where-Object { $_.FullName.EndsWith('/updates.json') })
    if ($entries.Count -ne 1) { throw 'Expected one updates.json in the package.' }
    $reader = [IO.StreamReader]::new($entries[0].Open())
    try { $config = $reader.ReadToEnd() | ConvertFrom-Json } finally { $reader.Dispose() }
} finally { $zip.Dispose() }
if ($config.schema -ne 1 -or $config.version -notmatch '^\d{1,5}\.\d{1,5}\.\d{1,5}$' -or
    $config.repository -notmatch '^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$') { throw 'Invalid update channel.' }
$hash = (Get-FileHash -LiteralPath $archivePath).Hash.ToLowerInvariant()
$release = [ordered]@{
    repository = $config.repository
    tag = 'v' + $config.version
    asset = 'RelayCore-win-x64.zip'
    archive = $archivePath
    bytes = (Get-Item -LiteralPath $archivePath).Length
    sha256 = $hash
    notes = $releaseNotes
}
$descriptor = [ordered]@{
    schema = 1; channel = 'stable'; platform = 'windows-x64'
    repository = $config.repository; version = $config.version
    url = 'https://github.com/' + $config.repository + '/releases/download/v' + $config.version + '/RelayCore-win-x64.zip'
    bytes = $release.bytes; sha256 = $hash
    notes = ($releaseNotes -replace '(?m)^#{1,6} ', '' -replace '(?m)^- ', '• ')
} | ConvertTo-Json -Depth 3
$descriptorBytes = [Text.Encoding]::UTF8.GetBytes($descriptor)
[IO.File]::WriteAllBytes($archivePath + '.release.json', $descriptorBytes)
$release | ConvertTo-Json -Depth 3
if (-not $Publish) {
    Write-Output 'Preview only. Nothing uploaded. Use -Publish only after approving this exact package and repository.'
    return
}

# Credentials stay in memory and are sent only to GitHub. Never include them in a game package.
$start = [Diagnostics.ProcessStartInfo]::new('git', 'credential fill')
$start.UseShellExecute = $false
$start.CreateNoWindow = $true
$start.RedirectStandardInput = $true
$start.RedirectStandardOutput = $true
$start.RedirectStandardError = $true
$start.EnvironmentVariables['GIT_TERMINAL_PROMPT'] = '0'
$start.EnvironmentVariables['GCM_INTERACTIVE'] = 'never'
$credentialProcess = [Diagnostics.Process]::Start($start)
$credentialProcess.StandardInput.WriteLine('protocol=https')
$credentialProcess.StandardInput.WriteLine('host=github.com')
$credentialProcess.StandardInput.WriteLine()
$credentialProcess.StandardInput.Close()
$credentialText = $credentialProcess.StandardOutput.ReadToEnd()
$null = $credentialProcess.StandardError.ReadToEnd()
$credentialProcess.WaitForExit()
if ($credentialProcess.ExitCode -ne 0) { throw 'GitHub credentials are unavailable. Sign in to Git Credential Manager first.' }
$tokenLine = @($credentialText -split "`n" | Where-Object { $_.StartsWith('password=') })
if ($tokenLine.Count -ne 1) { throw 'GitHub credential was not found.' }
$headers = @{ Authorization = 'Bearer ' + $tokenLine[0].Substring(9).Trim(); 'User-Agent' = 'RelayCore-release-publisher'; Accept = 'application/vnd.github+json' }
$repo = $config.repository
$api = 'https://api.github.com/repos/' + $repo
$repository = $null
try { $repository = Invoke-RestMethod -Uri $api -Headers $headers }
catch { if ([int]$_.Exception.Response.StatusCode -ne 404) { throw } }
if ($null -eq $repository) {
    if (-not $CreateRepository) { throw 'Release repository does not exist. Creation requires explicit -CreateRepository.' }
    $user = Invoke-RestMethod -Uri 'https://api.github.com/user' -Headers $headers
    if ($repo.Split('/')[0] -ne $user.login) { throw 'Repository owner is not the authenticated user.' }
    $body = @{ name=$repo.Split('/')[1]; description='RelayCore Windows playtest downloads'; private=$false; auto_init=$true } | ConvertTo-Json
    $repository = Invoke-RestMethod -Method Post -Uri 'https://api.github.com/user/repos' -Headers $headers -ContentType 'application/json' -Body $body
}
if ($repository.private) { throw 'Automatic unauthenticated downloads require a public distribution repository. Visibility was not changed.' }
$existing = $null
try { $existing = Invoke-RestMethod -Uri ($api + '/releases/tags/' + $release.tag) -Headers $headers }
catch { if ([int]$_.Exception.Response.StatusCode -ne 404) { throw } }
$descriptorHash = (Get-FileHash -LiteralPath ($archivePath + '.release.json')).Hash.ToLowerInvariant()
if ($null -ne $existing -and ($existing.body -cne $releaseNotes -or $existing.prerelease)) {
    throw 'This version belongs to another release. Published files are never overwritten.'
}
if ($null -ne $existing) {
    $draft = $existing
} else {
    $body = @{ tag_name=$release.tag; name=('RelayCore ' + $config.version); body=$releaseNotes; draft=$true; prerelease=$false } | ConvertTo-Json
    $draft = Invoke-RestMethod -Method Post -Uri ($api + '/releases') -Headers $headers -ContentType 'application/json; charset=utf-8' -Body ([Text.Encoding]::UTF8.GetBytes($body))
}
$upload = 'https://uploads.github.com/repos/' + $repo + '/releases/' + $draft.id + '/assets?name=RelayCore-win-x64.zip'
$asset = @($draft.assets | Where-Object name -eq 'RelayCore-win-x64.zip')
if ($asset.Count -eq 0) {
    if (-not $draft.draft) { throw 'Published release is missing its archive. It will not be modified.' }
    $asset = Invoke-RestMethod -Method Post -Uri $upload -Headers $headers -ContentType 'application/zip' -InFile $archivePath
} elseif ($asset.Count -ne 1) { throw 'Duplicate release archives.' }
if ($asset.digest -ne ('sha256:' + $hash) -or $asset.size -ne $release.bytes) { throw 'Uploaded asset differs. Release remains a draft.' }
$descriptorUpload = 'https://uploads.github.com/repos/' + $repo + '/releases/' + $draft.id + '/assets?name=release.json'
$descriptorAsset = @($draft.assets | Where-Object name -eq 'release.json')
if ($descriptorAsset.Count -eq 0) {
    if (-not $draft.draft) { throw 'Published release is missing metadata. It will not be modified.' }
    $descriptorAsset = Invoke-RestMethod -Method Post -Uri $descriptorUpload -Headers $headers -ContentType 'application/json' -Body $descriptorBytes
} elseif ($descriptorAsset.Count -ne 1) { throw 'Duplicate release metadata.' }
if ($descriptorAsset.digest -ne ('sha256:' + $descriptorHash)) { throw 'Uploaded release metadata differs. Release remains a draft.' }
if (-not $draft.draft) {
    Write-Output ('Already published and verified: ' + $draft.html_url)
    return
}
$published = Invoke-RestMethod -Method Patch -Uri ($api + '/releases/' + $draft.id) -Headers $headers -ContentType 'application/json' -Body '{"draft":false,"make_latest":"true"}'
Write-Output ('Published: ' + $published.html_url)
