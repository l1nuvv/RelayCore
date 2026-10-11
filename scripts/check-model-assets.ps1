param(
    [Parameter(Mandatory)][string]$ProjectRoot,
    [Parameter(Mandatory)][string]$BuildRoot,
    [Parameter(Mandatory)][string]$RuntimeAssets
)
$ErrorActionPreference = 'Stop'
$manifest = Join-Path $BuildRoot 'generated/runtime-models.txt'
if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
    throw "Missing compiled model manifest: $manifest. Rebuild the game."
}
$runtimePaths = @(Get-Content -LiteralPath $manifest)
if ($runtimePaths.Count -eq 0 -or @($runtimePaths | Sort-Object -Unique).Count -ne $runtimePaths.Count) {
    throw 'Invalid compiled model manifest. Rebuild the game.'
}
$importerHash = (Get-FileHash -LiteralPath (Join-Path $BuildRoot 'relaycore_model_import.exe')).Hash
$compilerHash = (Get-FileHash -LiteralPath (Join-Path $ProjectRoot 'cmake/compile_model_asset.cmake')).Hash
$declarationHash = (Get-FileHash -LiteralPath (Join-Path $ProjectRoot 'cmake/RelayCoreModelAssets.cmake')).Hash
foreach ($relative in $runtimePaths) {
    if ($relative -notmatch '^models/[a-zA-Z0-9_-]+\.rcmodel$') {
        throw "Invalid compiled model path: $relative"
    }
    $receiptPath = Join-Path $BuildRoot "generated/model-receipts/$relative.json"
    $receipt = Get-Content -LiteralPath $receiptPath -Raw | ConvertFrom-Json
    if ($receipt.version -ne 1 -or $receipt.source -notmatch '^content/models/[a-zA-Z0-9_-]+\.glb$') {
        throw "Invalid model receipt: $receiptPath"
    }
    $sourceHash = (Get-FileHash -LiteralPath (Join-Path $ProjectRoot $receipt.source)).Hash
    $productHash = (Get-FileHash -LiteralPath (Join-Path $BuildRoot "generated/model-assets/$relative")).Hash
    $stagedHash = (Get-FileHash -LiteralPath (Join-Path $RuntimeAssets $relative)).Hash
    if ($sourceHash -ne $receipt.source_sha256 -or $importerHash -ne $receipt.importer_sha256 -or
        $compilerHash -ne $receipt.compiler_sha256 -or $declarationHash -ne $receipt.declaration_sha256 -or
        $productHash -ne $receipt.product_sha256 -or $stagedHash -ne $productHash) {
        throw "Compiled model is stale or differs: $relative. Rebuild the game."
    }
}
$runtimePaths
