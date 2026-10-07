param(
    [Parameter(Mandatory)][string]$Compiler,
    [Parameter(Mandatory)][string]$Importer,
    [Parameter(Mandatory)][string]$CMake,
    [Parameter(Mandatory)][string]$Ninja
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$fixture = Join-Path ([IO.Path]::GetTempPath()) ('RelayCore collision assets ' + [guid]::NewGuid().ToString('N'))
$build = Join-Path $fixture 'build/msvc-relwithdebinfo'
$runtime = Join-Path $build 'src/relaygame/assets'
function Assert([bool]$Condition, [string]$Message) {
    if (-not $Condition) { throw $Message }
}
function Build-Collision {
    $output = (& $CMake --build $build --target staged_assets 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0) "Collision build failed: $output"
    return $output
}
function Reject-Build([string]$Expected) {
    $output = (& $CMake --build $build --target staged_assets 2>&1) -join "`n"
    Assert ($LASTEXITCODE -ne 0 -and $output.Contains($Expected)) "Expected '$Expected', got: $output"
}
function Check-Collision([bool]$MustFail = $false) {
    $output = (& $CMake "-DCOLLISION_PROJECT_ROOT=$fixture" "-DCOLLISION_BUILD_ROOT=$build" `
        "-DCOLLISION_COMPILER=$build/relaycore_collision_compile.exe" "-DCOLLISION_RUNTIME_ASSETS=$runtime" `
        -P "$fixture/cmake/check_collision_assets.cmake" 2>&1) -join "`n"
    if ($MustFail) {
        Assert ($LASTEXITCODE -ne 0) 'Stale collision was accepted.'
    } else {
        Assert ($LASTEXITCODE -eq 0) "Collision check failed: $output"
    }
}
try {
    foreach ($directory in @('content/collision', 'content/models', 'assets/config', 'assets/levels', 'cmake', 'scripts', 'build')) {
        [IO.Directory]::CreateDirectory((Join-Path $fixture $directory)) | Out-Null
    }
    [IO.Directory]::CreateDirectory($build) | Out-Null
    [IO.File]::WriteAllText("$fixture/assets/config/engine.toml", '# fixture')
    [IO.File]::WriteAllText("$fixture/assets/levels/power_restoration.toml", '# fixture')
    foreach ($script in @('RelayCoreModelAssets.cmake', 'compile_model_asset.cmake', 'RelayCoreCollisionAssets.cmake',
        'compile_collision_asset.cmake', 'check_collision_assets.cmake', 'sync_runtime_assets.cmake')) {
        Copy-Item -LiteralPath "$projectRoot/cmake/$script" -Destination "$fixture/cmake/$script"
    }
    Copy-Item -LiteralPath $Compiler -Destination "$build/relaycore_collision_compile.exe"
    Copy-Item -LiteralPath $Importer -Destination "$build/relaycore_model_import.exe"
    foreach ($tool in @($Compiler, $Importer)) {
        Get-ChildItem -LiteralPath (Split-Path -Parent $tool) -Filter '*.dll' -File | ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $build -Force
        }
    }
    Get-ChildItem -LiteralPath "$projectRoot/content/models" -Filter '*.glb' -File | ForEach-Object {
        Copy-Item -LiteralPath $_.FullName -Destination "$fixture/content/models"
    }
    $source = "$fixture/content/collision/lab_bench.toml"
    Copy-Item -LiteralPath "$projectRoot/content/collision/lab_bench.toml" -Destination $source
    $originalSource = [IO.File]::ReadAllText($source)
    [IO.File]::WriteAllText("$fixture/CMakeLists.txt", @'
cmake_minimum_required(VERSION 4.0)
project(CollisionAssets NONE)
foreach(tool IN ITEMS model_import collision_compile)
    add_executable(relaycore_${tool} IMPORTED GLOBAL)
    set_target_properties(relaycore_${tool} PROPERTIES IMPORTED_LOCATION "${PROJECT_BINARY_DIR}/relaycore_${tool}.exe")
endforeach()
include(cmake/RelayCoreModelAssets.cmake)
include(cmake/RelayCoreCollisionAssets.cmake)
file(CONFIGURE OUTPUT "${PROJECT_BINARY_DIR}/generated/runtime-assets.txt"
    CONTENT "${model_manifest}\n${collision_manifest}\n" @ONLY)
add_custom_target(staged_assets
    COMMAND ${CMAKE_COMMAND}
        "-DRELAYCORE_ASSET_SOURCE=${PROJECT_SOURCE_DIR}/assets"
        "-DRELAYCORE_GENERATED_ASSETS=${RELAYCORE_MODEL_ASSET_ROOT}"
        "-DRELAYCORE_GENERATED_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/generated/runtime-assets.txt"
        "-DRELAYCORE_ASSET_OUTPUT=${PROJECT_BINARY_DIR}/src/relaygame/assets"
        "-DRELAYCORE_ASSET_MANIFEST=${PROJECT_BINARY_DIR}/staged.manifest"
        "-DCOLLISION_PROJECT_ROOT=${PROJECT_SOURCE_DIR}"
        "-DCOLLISION_BUILD_ROOT=${PROJECT_BINARY_DIR}"
        "-DCOLLISION_COMPILER=$<TARGET_FILE:relaycore_collision_compile>"
        -P "${PROJECT_SOURCE_DIR}/cmake/sync_runtime_assets.cmake" VERBATIM)
add_dependencies(staged_assets relaycore_model_assets relaycore_collision_assets)
'@)
    $configured = (& $CMake -S $fixture -B $build -G Ninja "-DCMAKE_MAKE_PROGRAM=$Ninja" 2>&1) -join "`n"
    Assert ($LASTEXITCODE -eq 0) "Fixture configure failed: $configured"
    Assert ((Build-Collision).Contains('Compiling collision')) 'Fresh build did not compile collision.'
    Check-Collision
    $product = "$build/generated/model-assets/collision/lab_bench.rccollision"
    $receipt = "$build/generated/collision-receipts/lab_bench.json"
    $staged = "$runtime/collision/lab_bench.rccollision"
    $originalHash = (Get-FileHash -LiteralPath $product).Hash
    $originalTime = (Get-Item -LiteralPath $product).LastWriteTimeUtc
    Assert (-not (Build-Collision).Contains('Compiling collision')) 'No-op build recompiled collision.'
    Assert ((Get-Item -LiteralPath $product).LastWriteTimeUtc -eq $originalTime) 'No-op build rewrote collision.'
    Remove-Item -LiteralPath $product
    Build-Collision | Out-Null
    Assert ((Get-FileHash -LiteralPath $product).Hash -eq $originalHash) 'Repeated compilation is not deterministic.'

    # Content hashing detects stale inputs even when timestamp-based Ninja invalidation cannot.
    $sourceTime = (Get-Item -LiteralPath $source).LastWriteTimeUtc
    [IO.File]::WriteAllText($source, $originalSource.Replace('surface = "tile"', 'surface = "metal"'))
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = $sourceTime
    Check-Collision $true
    Reject-Build 'stale or differs'
    (Get-Item -LiteralPath $source).LastWriteTimeUtc = [DateTime]::UtcNow
    Assert ((Build-Collision).Contains('Compiling collision')) 'Source change did not invalidate collision.'
    Assert ((Get-FileHash -LiteralPath $product).Hash -ne $originalHash) 'Surface change did not reach product.'
    [IO.File]::WriteAllText($source, $originalSource)
    Build-Collision | Out-Null

    $declaration = "$fixture/cmake/RelayCoreCollisionAssets.cmake"
    $declarationText = [IO.File]::ReadAllText($declaration)
    [IO.File]::AppendAllText($declaration, "`n")
    Check-Collision $true
    Assert ((Build-Collision).Contains('Compiling collision')) 'Declaration change did not invalidate collision.'
    [IO.File]::AppendAllText("$fixture/cmake/compile_collision_asset.cmake", "`n")
    Check-Collision $true
    Assert ((Build-Collision).Contains('Compiling collision')) 'Compiler rule change did not invalidate collision.'
    $stream = [IO.File]::Open("$build/relaycore_collision_compile.exe", [IO.FileMode]::Append)
    try { $stream.WriteByte(0) } finally { $stream.Dispose() }
    Check-Collision $true
    Assert ((Build-Collision).Contains('Compiling collision')) 'Compiler change did not invalidate collision.'

    $pairedModel = "$build/generated/model-assets/models/lab_bench.rcmodel"
    [IO.File]::AppendAllText($pairedModel, 'changed')
    Check-Collision $true
    Assert ((Build-Collision).Contains('Compiling collision')) 'Model product change did not invalidate collision.'
    Remove-Item -LiteralPath $pairedModel
    Build-Collision | Out-Null
    Check-Collision

    foreach ($badSource in @(
        $originalSource.Replace('surface = "tile"', 'surface = "unknown"'),
        $originalSource.Replace('models/lab_bench.rcmodel', 'models/missing.rcmodel'),
        $originalSource.Replace('[-1.2, 0.84, -0.45]', '[1.2, 0.84, -0.45]'),
        'malformed TOML ['
    )) {
        $goodProduct = (Get-FileHash -LiteralPath $product).Hash
        $goodReceipt = (Get-FileHash -LiteralPath $receipt).Hash
        [IO.File]::WriteAllText($source, $badSource)
        Reject-Build 'lab_bench.toml'
        Assert ((Get-FileHash -LiteralPath $product).Hash -eq $goodProduct) 'Failed compile replaced product.'
        Assert ((Get-FileHash -LiteralPath $receipt).Hash -eq $goodReceipt) 'Failed compile replaced receipt.'
        Assert ((Get-FileHash -LiteralPath $staged).Hash -eq $goodProduct) 'Failed compile replaced staged product.'
        [IO.File]::WriteAllText($source, $originalSource)
        Build-Collision | Out-Null
    }
    Remove-Item -LiteralPath $source
    Reject-Build 'lab_bench.toml'
    [IO.File]::WriteAllText($source, $originalSource)
    Build-Collision | Out-Null

    $triangleSource = @'
version = 1
units = "metres"
axes = "right-handed-y-up"
winding = "ccw"
model = "models/lab_bench.rcmodel"
transform = [-2,0,0,0, 0,3,0,0, 0,0,0.5,0, 5,2,-1,1]
[[triangles]]
a = [0,0,0]
b = [0,1,0]
c = [1,0,0]
surface = "metal"
'@
    [IO.File]::WriteAllText($source, $triangleSource)
    Build-Collision | Out-Null
    Check-Collision
    [IO.File]::WriteAllText($source, $triangleSource.Replace('b = [0,1,0]', 'b = [0,0,0]'))
    Reject-Build 'degenerate'
    [IO.File]::WriteAllText($source, $originalSource)
    Build-Collision | Out-Null

    [IO.File]::AppendAllText($staged, 'damaged')
    Check-Collision $true
    Build-Collision | Out-Null
    Check-Collision
    [IO.File]::AppendAllText($product, 'damaged')
    Check-Collision $true
    Reject-Build 'stale or differs'
    Remove-Item -LiteralPath $product
    Build-Collision | Out-Null

    Copy-Item -LiteralPath $source -Destination "$fixture/content/collision/extra.toml"
    $extraDeclaration = $declarationText.Replace('set(RELAYCORE_COLLISION_SOURCES lab_bench)',
        'set(RELAYCORE_COLLISION_SOURCES lab_bench extra)')
    $extraDeclaration = $extraDeclaration.Replace('set(collision_runtime_paths)',
        "set(RELAYCORE_COLLISION_MODEL_extra lab_bench)`nset(collision_runtime_paths)")
    [IO.File]::WriteAllText($declaration, $extraDeclaration)
    Build-Collision | Out-Null
    Assert (Test-Path -LiteralPath "$runtime/collision/extra.rccollision") 'New collision was not staged.'
    [IO.File]::WriteAllText($declaration, $declarationText)
    Build-Collision | Out-Null
    Assert (-not (Test-Path -LiteralPath "$runtime/collision/extra.rccollision")) 'Removed collision remained staged.'
    Check-Collision

    # Run the actual package preflight, stopping before any directory/archive creation.
    Copy-Item -LiteralPath "$projectRoot/scripts/check-model-assets.ps1" -Destination "$fixture/scripts"
    $packageScript = [IO.File]::ReadAllText("$projectRoot/scripts/package-playtest.ps1")
    $validationEnd = $packageScript.IndexOf('New-Item -ItemType Directory -Path $package')
    Assert ($validationEnd -gt 0) 'Package validation boundary missing.'
    [IO.File]::WriteAllText("$fixture/scripts/package-validation.ps1", $packageScript.Substring(0, $validationEnd))
    [IO.File]::WriteAllText("$fixture/VERSION", '1.0.0')
    [IO.File]::WriteAllText("$build/src/relaygame/updates.json", '{"version":"1.0.0"}')
    foreach ($executable in @('RelayCore.exe', 'RelayCoreUpdater.exe')) {
        [IO.File]::WriteAllText("$build/src/relaygame/$executable", 'fixture')
    }
    [IO.Directory]::CreateDirectory("$build/generated/shaders") | Out-Null
    [IO.Directory]::CreateDirectory("$runtime/shaders/vulkan") | Out-Null
    [IO.File]::WriteAllText("$build/generated/shaders/runtime-shaders.txt", 'fixture.spv')
    [IO.File]::WriteAllText("$build/generated/shaders/fixture.spv", 'fixture')
    [IO.File]::WriteAllText("$runtime/shaders/vulkan/fixture.spv", 'fixture')
    & "$fixture/scripts/package-validation.ps1"
    Assert (-not (Test-Path -LiteralPath "$fixture/build/playtest")) 'Asset validation created a package.'
    [IO.File]::AppendAllText($staged, 'damaged')
    $failure = ''
    try { & "$fixture/scripts/package-validation.ps1" | Out-Null } catch { $failure = $_.Exception.Message }
    Assert ($failure.Contains('Collision assets are stale')) 'Package preflight accepted corrupt collision.'
    Build-Collision | Out-Null
    Write-Output 'Collision determinism, no-op, invalidation, validation, staging and rollback passed.'
} finally {
    $resolved = [IO.Path]::GetFullPath($fixture)
    $temporaryRoot = [IO.Path]::GetFullPath([IO.Path]::GetTempPath())
    if ($resolved.StartsWith($temporaryRoot, [StringComparison]::OrdinalIgnoreCase) -and
        (Split-Path -Leaf $resolved).StartsWith('RelayCore collision assets ') -and
        (Test-Path -LiteralPath $resolved)) {
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
