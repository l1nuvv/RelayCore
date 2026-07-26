[CmdletBinding()]
param(
    [string]$BuildDirectory = "build/msvc-debug"
)

$ErrorActionPreference = "Stop"

$repositoryRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([System.IO.Path]::IsPathRooted($BuildDirectory)) {
    $resolvedBuildDirectory = $BuildDirectory
} else {
    $resolvedBuildDirectory = Join-Path $repositoryRoot $BuildDirectory
}

if (-not (Test-Path -LiteralPath $resolvedBuildDirectory -PathType Container)) {
    throw "Build directory was not found: $resolvedBuildDirectory"
}

$compileCommands = Join-Path $resolvedBuildDirectory "compile_commands.json"
if (-not (Test-Path -LiteralPath $compileCommands -PathType Leaf)) {
    throw "compile_commands.json was not found: $compileCommands"
}

$clangTidy = Get-Command clang-tidy -CommandType Application -ErrorAction SilentlyContinue
if ($null -eq $clangTidy) {
    throw "clang-tidy was not found in PATH"
}

$sourceRoots = @(
    (Join-Path $repositoryRoot "src/relaycore"),
    (Join-Path $repositoryRoot "src/relaygame")
)
$sources = @(
    $sourceRoots | ForEach-Object {
        $sourceRoot = $_
        Get-ChildItem -LiteralPath $sourceRoot -Recurse -File -Filter "*.cpp"
    } | Sort-Object FullName
)

if ($sources.Count -eq 0) {
    throw "No production .cpp files were found under src/relaycore or src/relaygame"
}

Write-Host "clang-tidy: $($clangTidy.Source)"
Write-Host "Compile commands: $compileCommands"
Write-Host "Analyzing $($sources.Count) production source files:"
foreach ($source in $sources) {
    $relativePath = [System.IO.Path]::GetRelativePath($repositoryRoot, $source.FullName)
    Write-Host "  $relativePath"
}

$failedSources = @()
foreach ($source in $sources) {
    & $clangTidy.Source "-p" $resolvedBuildDirectory $source.FullName
    if ($LASTEXITCODE -ne 0) {
        $failedSources += $source.FullName
    }
}

if ($failedSources.Count -ne 0) {
    Write-Error "clang-tidy reported diagnostics for $($failedSources.Count) source file(s)."
    exit 1
}

Write-Host "clang-tidy completed successfully for $($sources.Count) production source files."
