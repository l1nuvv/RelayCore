param(
    [Parameter(Mandatory)][string]$Importer,
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string]$Ninja
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('RelayCore model assets ' + [guid]::NewGuid().ToString('N'))
$build = Join-Path $fixture 'build'
$runtime = Join-Path $build 'runtime/assets'
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Reject([scriptblock]$Action, [string]$Expected) {
    $failure = ''
    try { & $Action | Out-Null } catch { $failure = $_.Exception.Message }
    Assert ($failure.Contains($Expected)) "Expected '$Expected', got '$failure'."
}
function Build-Models {
    $output = (& $CMake --build $build --target staged_assets 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0) "Model build failed: $output"
    return $output
}
function Check-Models {
    & (Join-Path $projectRoot 'scripts/check-model-assets.ps1') -ProjectRoot $fixture -BuildRoot $build -RuntimeAssets $runtime
}
try {
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'content/models')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'assets')) | Out-Null
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'cmake')) | Out-Null
    [IO.Directory]::CreateDirectory($build) | Out-Null
    foreach ($script in @('RelayCoreModelAssets.cmake', 'compile_model_asset.cmake', 'sync_runtime_assets.cmake')) {
        Copy-Item -LiteralPath (Join-Path $projectRoot "cmake/$script") -Destination (Join-Path $fixture "cmake/$script")
    }
    Copy-Item -LiteralPath $Importer -Destination (Join-Path $build 'relaycore_model_import.exe')
    Get-ChildItem -LiteralPath (Split-Path -Parent $Importer) -Filter '*.dll' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination $build
    }
    $source = Join-Path $fixture 'content/models/lab_bench.glb'
    Get-ChildItem -LiteralPath (Join-Path $projectRoot 'content/models') -Filter '*.glb' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination (Join-Path $fixture 'content/models')
    }
    $originalSource = [IO.File]::ReadAllBytes($source)
    [IO.File]::WriteAllText((Join-Path $fixture 'assets/readme.txt'), 'authored fixture')
    [IO.File]::WriteAllText((Join-Path $fixture 'CMakeLists.txt'), @'
cmake_minimum_required(VERSION 4.0)
project(ModelAssets NONE)
add_executable(relaycore_model_import IMPORTED GLOBAL)
set_target_properties(relaycore_model_import PROPERTIES IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/relaycore_model_import.exe")
include(cmake/RelayCoreModelAssets.cmake)
add_custom_target(staged_assets
    COMMAND ${CMAKE_COMMAND}
        "-DRELAYCORE_ASSET_SOURCE=${PROJECT_SOURCE_DIR}/assets"
        "-DRELAYCORE_GENERATED_ASSETS=${RELAYCORE_MODEL_ASSET_ROOT}"
        "-DRELAYCORE_GENERATED_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/generated/runtime-models.txt"
        "-DRELAYCORE_ASSET_OUTPUT=${PROJECT_BINARY_DIR}/runtime/assets"
        "-DRELAYCORE_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/staged.manifest"
        -P "${PROJECT_SOURCE_DIR}/cmake/sync_runtime_assets.cmake"
    VERBATIM)
add_dependencies(staged_assets relaycore_model_assets)
'@)
    $configured = (& $CMake -S $fixture -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0) "Fixture configure failed: $configured"
    $modelCount = @(Get-Content -LiteralPath (Join-Path $build 'generated/runtime-models.txt')).Count
    Assert ((Build-Models).Contains('Compiling model')) 'Fresh build did not import the GLB.'
    $relative = 'models/lab_bench.rcmodel'
    $product = Join-Path $build "generated/model-assets/$relative"
    $staged = Join-Path $runtime $relative
    $receipt = Join-Path $build "generated/model-receipts/$relative.json"
    Assert (@(Check-Models).Count -eq $modelCount) 'Compiled models did not pass packaging validation.'
    $timestamp = (Get-Item -LiteralPath $product).LastWriteTimeUtc
    $stagedTimestamp = (Get-Item -LiteralPath $staged).LastWriteTimeUtc
    Assert (-not (Build-Models).Contains('Compiling model')) 'Unchanged model was recompiled.'
    Assert ((Get-Item -LiteralPath $product).LastWriteTimeUtc -eq $timestamp) 'No-op build rewrote the model.'
    Assert ((Get-Item -LiteralPath $staged).LastWriteTimeUtc -eq $stagedTimestamp) 'No-op sync rewrote the runtime model.'

    $materialDeclaration = Join-Path $fixture 'cmake/RelayCoreModelAssets.cmake'
    $materialDeclarationBytes = [IO.File]::ReadAllBytes($materialDeclaration)
    $cabinetProduct = Join-Path $build 'generated/model-assets/models/power_cabinet.rcmodel'
    $materialProductHash = (Get-FileHash -LiteralPath $cabinetProduct).Hash
    $declarationText = [IO.File]::ReadAllText($materialDeclaration)
    $compileLoop = 'foreach(source IN LISTS RELAYCORE_MODEL_SOURCES)'
    Assert ($declarationText.Contains($compileLoop)) 'Missing model compilation loop in fixture declaration.'
    [IO.File]::WriteAllText($materialDeclaration, $declarationText.Replace($compileLoop,
        "set(RELAYCORE_MODEL_METALLIC_ROUGHNESS_power_cabinet OFF)`n$compileLoop"))
    Reject { Check-Models } 'stale or differs'
    Assert ((Build-Models).Contains('Compiling model')) 'Material policy change did not invalidate compilation.'
    Assert ((Get-FileHash -LiteralPath $cabinetProduct).Hash -ne $materialProductHash) 'Material policy did not reach the product.'
    Check-Models | Out-Null
    [IO.File]::WriteAllBytes($materialDeclaration, $materialDeclarationBytes)
    Build-Models | Out-Null
    Assert ((Get-FileHash -LiteralPath $cabinetProduct).Hash -eq $materialProductHash) 'Material policy restoration was not deterministic.'

    $oldSourceTime = (Get-Item -LiteralPath $source).LastWriteTimeUtc
    $jsonLength = [BitConverter]::ToUInt32($originalSource, 12)
    $json = [Text.Encoding]::UTF8.GetString($originalSource, 20, $jsonLength) | ConvertFrom-Json
    $json.materials[0].pbrMetallicRoughness | Add-Member -NotePropertyName baseColorFactor -NotePropertyValue @(0.2, 0.4, 0.8, 1) -Force
    $jsonBytes = [Text.Encoding]::UTF8.GetBytes(($json | ConvertTo-Json -Depth 100 -Compress))
    $padding = (4 - $jsonBytes.Length % 4) % 4
    $binaryTail = $originalSource[(20 + $jsonLength)..($originalSource.Length - 1)]
    $writer = [IO.BinaryWriter]::new([IO.File]::Create($source))
    try {
        $writer.Write([uint32]0x46546c67); $writer.Write([uint32]2)
        $writer.Write([uint32](20 + $jsonBytes.Length + $padding + $binaryTail.Length))
        $writer.Write([uint32]($jsonBytes.Length + $padding)); $writer.Write([uint32]0x4e4f534a)
        $writer.Write($jsonBytes)
        for ($i = 0; $i -lt $padding; $i++) { $writer.Write([byte]32) }
        $writer.Write([byte[]]$binaryTail)
    } finally { $writer.Dispose() }
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = $oldSourceTime
    Reject { Check-Models } 'stale or differs'
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = [DateTime]::UtcNow
    $oldHash = (Get-FileHash -LiteralPath $product).Hash
    Assert ((Build-Models).Contains('Compiling model')) 'Changed GLB was not compiled.'
    Assert ((Get-FileHash -LiteralPath $product).Hash -ne $oldHash) 'Changed material did not reach the product.'
    Check-Models | Out-Null

    [IO.File]::AppendAllText((Join-Path $fixture 'cmake/compile_model_asset.cmake'), "`n")
    Reject { Check-Models } 'stale or differs'
    Assert ((Build-Models).Contains('Compiling model')) 'Compiler rule change did not invalidate the product.'
    Check-Models | Out-Null
    $tool = Join-Path $build 'relaycore_model_import.exe'
    $stream = [IO.File]::Open($tool, [IO.FileMode]::Append)
    try { $stream.WriteByte(0) } finally { $stream.Dispose() }
    Reject { Check-Models } 'stale or differs'
    Assert ((Build-Models).Contains('Compiling model')) 'Importer change did not invalidate the product.'
    Check-Models | Out-Null

    $goodProduct = (Get-FileHash -LiteralPath $product).Hash
    $goodReceipt = (Get-FileHash -LiteralPath $receipt).Hash
    [IO.File]::WriteAllText($source, 'broken GLB')
    $failed = (& $CMake --build $build --target staged_assets 2>&1) -join "`n"
    Assert ($LASTEXITCODE -ne 0 -and $failed.Contains('lab_bench.glb')) 'Bad GLB did not fail the build with its source path.'
    Assert ((Get-FileHash -LiteralPath $product).Hash -eq $goodProduct) 'Failed import replaced the product.'
    Assert ((Get-FileHash -LiteralPath $receipt).Hash -eq $goodReceipt) 'Failed import certified bad data.'
    Assert ((Get-FileHash -LiteralPath $staged).Hash -eq $goodProduct) 'Failed build replaced runtime assets.'
    Reject { Check-Models } 'stale or differs'
    [IO.File]::WriteAllBytes($source, $originalSource)
    Build-Models | Out-Null
    Check-Models | Out-Null

    [IO.File]::AppendAllText($staged, 'damaged')
    Reject { Check-Models } 'stale or differs'
    Build-Models | Out-Null
    Check-Models | Out-Null
    [IO.File]::AppendAllText($product, 'damaged')
    Reject { Check-Models } 'stale or differs'
    Remove-Item -LiteralPath $product
    Build-Models | Out-Null
    Check-Models | Out-Null

    $declaration = Join-Path $fixture 'cmake/RelayCoreModelAssets.cmake'
    $originalDeclaration = [IO.File]::ReadAllText($declaration)
    Copy-Item -LiteralPath $source -Destination (Join-Path $fixture 'content/models/extra.glb')
    [IO.File]::WriteAllText($declaration, $originalDeclaration.Replace('    lab_bench.glb', "    lab_bench.glb`n    extra.glb"))
    Build-Models | Out-Null
    Assert (@(Check-Models).Count -eq ($modelCount + 1)) 'New declared model was not compiled and staged.'
    $obsolete = Join-Path $runtime 'models/extra.rcmodel'
    Assert (Test-Path -LiteralPath $obsolete) 'New model was not added to runtime assets.'
    [IO.File]::WriteAllText($declaration, $originalDeclaration)
    [IO.File]::WriteAllText((Join-Path $build 'generated/model-assets/models/unused.rcmodel'), 'undeclared generated file')
    Build-Models | Out-Null
    Assert (-not (Test-Path -LiteralPath $obsolete)) 'Removed declaration left a stale runtime model.'
    Assert (@(Check-Models).Count -eq $modelCount) 'Model declaration update did not refresh receipts.'
    Assert (-not (Test-Path -LiteralPath (Join-Path $runtime 'models/unused.rcmodel'))) 'Undeclared generated model was staged.'
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'assets/models')) | Out-Null
    Copy-Item -LiteralPath $product -Destination (Join-Path $fixture "assets/$relative")
    $collision = (& $CMake --build $build --target staged_assets 2>&1) -join "`n"
    Assert ($LASTEXITCODE -ne 0 -and $collision.Contains('both authored and generated owners')) 'Ambiguous model ownership was accepted.'
    'model_assets_tests: passed'
} finally {
    $resolvedFixture = [IO.Path]::GetFullPath($fixture)
    $tempRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath()).TrimEnd('\') + '\'
    if (-not $resolvedFixture.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unsafe fixture cleanup path.' }
    Remove-Item -LiteralPath $resolvedFixture -Recurse -Force
}
