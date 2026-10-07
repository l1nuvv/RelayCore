param(
    [Parameter(Mandatory)][string]$Baker,
    [Parameter(Mandatory)][string]$Authoring
)
$ErrorActionPreference = 'Stop'
$tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\', '/')
$fixture = Join-Path $tempRoot ('RelayCore irradiance ' + [guid]::NewGuid().ToString('N'))
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
try {
    New-Item -ItemType Directory -Path $fixture | Out-Null
    $level = Join-Path $fixture 'level.toml'
    $product = Join-Path $fixture 'test.rcprobe'
    $source = @'
[[solid]]
origin = [0,-0.1,0]
size = [4,0.2,4]
material = "concrete"
[[entity]]
classname = "irradiance_volume"
product = "lighting/test.rcprobe"
mins = [-1,0.2,-1]
maxs = [1,1.2,1]
dimensions = [2,2,2]
rays = 64
[[entity]]
classname = "render_light"
origin = [0,1,0]
color = [1,0.5,0.2]
radius = 4
intensity = 2
'@
    Set-Content -LiteralPath $level -Value $source
    $output = (& $Baker $level $Authoring $product 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0 -and $output.Contains('baked 8 probes')) "First bake failed: $output"
    $hash = (Get-FileHash -LiteralPath $product).Hash
    (Get-Item -LiteralPath $product).LastWriteTimeUtc = [datetime]'2020-01-01T00:00:00Z'
    $timestamp = (Get-Item -LiteralPath $product).LastWriteTimeUtc
    $output = (& $Baker $level $Authoring $product 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0 -and $output.Contains('reused 8 probes')) "Matching product was rebaked: $output"
    Assert ((Get-FileHash -LiteralPath $product).Hash -eq $hash) 'Reuse changed product bytes.'
    Assert ((Get-Item -LiteralPath $product).LastWriteTimeUtc -eq $timestamp) 'Reuse rewrote the product.'
    Set-Content -LiteralPath $level -Value $source.Replace('intensity = 2', 'intensity = 3')
    $output = (& $Baker $level $Authoring $product 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0 -and $output.Contains('baked 8 probes')) "Stale product was reused: $output"
    Assert ((Get-FileHash -LiteralPath $product).Hash -ne $hash) 'Changed light did not change the product.'
    [IO.File]::WriteAllBytes($product, [byte[]](1, 2, 3))
    $output = (& $Baker $level $Authoring $product 2>&1) -join "`n"
    Assert ($LASTEXITCODE -ne 0 -and $output.Contains('invalid irradiance product size')) "Malformed product rejection differs: $output"
    Assert ((Get-Item -LiteralPath $product).Length -eq 3) 'Failed validation replaced the malformed product.'
    Write-Output 'irradiance asset reuse and invalidation passed'
} finally {
    Assert ((Split-Path -Parent ([IO.Path]::GetFullPath($fixture))) -eq $tempRoot) 'Unsafe fixture cleanup path.'
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture -Recurse -Force }
}
