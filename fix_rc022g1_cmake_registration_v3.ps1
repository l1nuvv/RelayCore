param()

$ErrorActionPreference = "Stop"

function Read-Utf8([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing required file: $Path"
    }

    return [System.IO.File]::ReadAllText(
        $Path,
        [System.Text.Encoding]::UTF8
    )
}

function Write-Utf8([string]$Path, [string]$Content) {
    $Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $Utf8NoBom)
}

function Get-LineEnding([string]$Content) {
    if ($Content.Contains("`r`n")) {
        return "`r`n"
    }

    return "`n"
}

function Insert-After-UniqueAnchor(
    [string]$Content,
    [string]$Anchor,
    [string]$InsertedLine,
    [string]$Label
) {
    if ($Content.Contains($InsertedLine)) {
        return $Content
    }

    $Pattern = [regex]::new(
        "(?m)^(?<indent>[ `t]*)" +
        [regex]::Escape($Anchor) +
        "[ `t]*`r?$"
    )

    $Matches = $Pattern.Matches($Content)
    if ($Matches.Count -ne 1) {
        throw "Expected exactly one ${Label} anchor, found $($Matches.Count)"
    }

    $LineEnding = Get-LineEnding $Content

    return $Pattern.Replace(
        $Content,
        [System.Text.RegularExpressions.MatchEvaluator]{
            param($Match)

            $AnchorLine = $Match.Value.TrimEnd("`r")
            return (
                $AnchorLine +
                $LineEnding +
                $Match.Groups["indent"].Value +
                $InsertedLine
            )
        },
        1
    )
}

function Ensure-Optional-Float-Assertion(
    [string]$Content,
    [string]$Property,
    [string]$Approximation
) {
    $RequireText = "REQUIRE(overlay.renderer.${Property}.has_value());"
    $CheckText = "CHECK(*overlay.renderer.${Property} == doctest::Approx(${Approximation}));"

    if ($Content.Contains($RequireText) -and $Content.Contains($CheckText)) {
        return $Content
    }

    $PatternText = (
        "(?m)^(?<indent>[ `t]*)CHECK\(" +
        "[^`r`n]*overlay\.renderer\." +
        [regex]::Escape($Property) +
        "[^`r`n]*\);[ `t]*`r?$"
    )

    $Pattern = [regex]::new($PatternText)
    $Matches = $Pattern.Matches($Content)

    if ($Matches.Count -ne 1) {
        throw (
            "Expected exactly one CHECK line for overlay.renderer.${Property}, " +
            "found $($Matches.Count)"
        )
    }

    $LineEnding = Get-LineEnding $Content

    return $Pattern.Replace(
        $Content,
        [System.Text.RegularExpressions.MatchEvaluator]{
            param($Match)

            $Indent = $Match.Groups["indent"].Value
            return (
                $Indent +
                $RequireText +
                $LineEnding +
                $Indent +
                $CheckText
            )
        },
        1
    )
}

function Strip-Trailing-Whitespace([string]$Content) {
    return [regex]::Replace(
        $Content,
        "(?m)[ `t]+`r?$",
        [System.Text.RegularExpressions.MatchEvaluator]{
            param($Match)

            if ($Match.Value.EndsWith("`r")) {
                return "`r"
            }

            return ""
        }
    )
}

$RepoRoot = (Get-Location).Path

$SourceCMakePath = Join-Path $RepoRoot "src/relaycore/CMakeLists.txt"
$TestsCMakePath = Join-Path $RepoRoot "tests/CMakeLists.txt"
$SourcePath = Join-Path $RepoRoot "src/relaycore/render/pipeline_authoring_overlay.cpp"
$HeaderPath = Join-Path $RepoRoot "src/relaycore/render/pipeline_authoring_overlay.h"
$TestPath = Join-Path $RepoRoot "tests/pipeline_authoring_overlay_tests.cpp"

foreach ($RequiredPath in @(
    $SourceCMakePath,
    $TestsCMakePath,
    $SourcePath,
    $HeaderPath,
    $TestPath
)) {
    if (-not (Test-Path -LiteralPath $RequiredPath)) {
        throw "Missing required RC-022g1 file: $RequiredPath"
    }
}

$SourceCMake = Read-Utf8 $SourceCMakePath
$TestsCMake = Read-Utf8 $TestsCMakePath
$Source = Read-Utf8 $SourcePath
$Tests = Read-Utf8 $TestPath

# Build all changes in memory first. Nothing is written unless every
# registration and source edit succeeds.
$SourceCMake = Insert-After-UniqueAnchor `
    $SourceCMake `
    "render/render_authoring_config.cpp" `
    "render/pipeline_authoring_overlay.cpp" `
    "engine implementation"

$SourceCMake = Insert-After-UniqueAnchor `
    $SourceCMake `
    "render/render_authoring_config.h" `
    "render/pipeline_authoring_overlay.h" `
    "engine header"

$TestsCMake = Insert-After-UniqueAnchor `
    $TestsCMake `
    "render_authoring_config_tests.cpp" `
    "pipeline_authoring_overlay_tests.cpp" `
    "test implementation"

if (-not $Source.Contains("#include <utility>")) {
    $Source = Insert-After-UniqueAnchor `
        $Source `
        "#include <unordered_set>" `
        "#include <utility>" `
        "utility include"
}

$Tests = Ensure-Optional-Float-Assertion `
    $Tests `
    "indirect_bounce_strength" `
    "0.12F"

$Tests = Ensure-Optional-Float-Assertion `
    $Tests `
    "fog_start" `
    "16.0F"

$Tests = Ensure-Optional-Float-Assertion `
    $Tests `
    "fog_end" `
    "42.0F"

$SourceCMake = Strip-Trailing-Whitespace $SourceCMake
$TestsCMake = Strip-Trailing-Whitespace $TestsCMake
$Source = Strip-Trailing-Whitespace $Source
$Tests = Strip-Trailing-Whitespace $Tests

Write-Utf8 $SourceCMakePath $SourceCMake
Write-Utf8 $TestsCMakePath $TestsCMake
Write-Utf8 $SourcePath $Source
Write-Utf8 $TestPath $Tests

Write-Host "Applied RC-022g1 corrective patch v3."
Write-Host ""
Write-Host "Registered:"
Write-Host "  render/pipeline_authoring_overlay.cpp"
Write-Host "  render/pipeline_authoring_overlay.h"
Write-Host "  pipeline_authoring_overlay_tests.cpp"
Write-Host ""
Write-Host "Corrected:"
Write-Host "  CMake registration"
Write-Host "  trailing whitespace"
Write-Host "  missing <utility>"
Write-Host "  optional float assertions"
Write-Host ""
Write-Host "Next:"
Write-Host "  git diff --check"
Write-Host "  cmake --fresh --preset msvc-debug"
Write-Host "  cmake --build --preset tests-debug --parallel 1"
