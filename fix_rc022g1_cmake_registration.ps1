param()

$ErrorActionPreference = "Stop"

function Read-Utf8([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Missing required file: $Path"
    }
    return [System.IO.File]::ReadAllText($Path, [System.Text.Encoding]::UTF8)
}

function Write-Utf8([string]$Path, [string]$Content) {
    $Utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText($Path, $Content, $Utf8NoBom)
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
        "(?m)^(?<indent>[ `t]*)" + [regex]::Escape($Anchor) + "[ `t]*$"
    )
    $Matches = $Pattern.Matches($Content)
    if ($Matches.Count -ne 1) {
        throw "Expected exactly one ${Label} anchor, found $($Matches.Count)"
    }

    $LineEnding = "`n"
    if ($Content.Contains("`r`n")) {
        $LineEnding = "`r`n"
    }

    return $Pattern.Replace(
        $Content,
        [System.Text.RegularExpressions.MatchEvaluator]{
            param($Match)
            return $Match.Value + $LineEnding + $Match.Groups["indent"].Value + $InsertedLine
        },
        1
    )
}

function Replace-Unique(
    [string]$Content,
    [string]$OldValue,
    [string]$NewValue,
    [string]$Label
) {
    $First = $Content.IndexOf($OldValue, [System.StringComparison]::Ordinal)
    if ($First -lt 0) {
        throw "${Label} not found"
    }

    $Second = $Content.IndexOf(
        $OldValue,
        $First + $OldValue.Length,
        [System.StringComparison]::Ordinal
    )
    if ($Second -ge 0) {
        throw "${Label} is not unique"
    }

    return $Content.Substring(0, $First) + $NewValue + $Content.Substring($First + $OldValue.Length)
}

function Strip-Trailing-Whitespace([string]$Content) {
    return [regex]::Replace($Content, "(?m)[ `t]+$", "")
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

$SourceCMakeUpdated = Insert-After-UniqueAnchor `
    $SourceCMake `
    "render/render_authoring_config.cpp" `
    "render/pipeline_authoring_overlay.cpp" `
    "engine source"

$SourceCMakeUpdated = Insert-After-UniqueAnchor `
    $SourceCMakeUpdated `
    "render/render_authoring_config.h" `
    "render/pipeline_authoring_overlay.h" `
    "engine header"

$TestsCMakeUpdated = Insert-After-UniqueAnchor `
    $TestsCMake `
    "render_authoring_config_tests.cpp" `
    "pipeline_authoring_overlay_tests.cpp" `
    "unit test source"

if (-not $Source.Contains("#include <utility>")) {
    $Source = Insert-After-UniqueAnchor `
        $Source `
        "#include <unordered_set>" `
        "#include <utility>" `
        "utility include"
}

$OptionalChecksOld = @'
    CHECK(overlay.renderer.indirect_bounce_strength == doctest::Approx(0.12F));
    CHECK(overlay.renderer.fog_start == doctest::Approx(16.0F));
    CHECK(overlay.renderer.fog_end == doctest::Approx(42.0F));
'@

$OptionalChecksNew = @'
    REQUIRE(overlay.renderer.indirect_bounce_strength.has_value());
    CHECK(*overlay.renderer.indirect_bounce_strength == doctest::Approx(0.12F));
    REQUIRE(overlay.renderer.fog_start.has_value());
    CHECK(*overlay.renderer.fog_start == doctest::Approx(16.0F));
    REQUIRE(overlay.renderer.fog_end.has_value());
    CHECK(*overlay.renderer.fog_end == doctest::Approx(42.0F));
'@

$Tests = Replace-Unique `
    $Tests `
    $OptionalChecksOld `
    $OptionalChecksNew `
    "optional renderer checks"

$SourceCMakeUpdated = Strip-Trailing-Whitespace $SourceCMakeUpdated
$TestsCMakeUpdated = Strip-Trailing-Whitespace $TestsCMakeUpdated
$Source = Strip-Trailing-Whitespace $Source
$Tests = Strip-Trailing-Whitespace $Tests

Write-Utf8 $SourceCMakePath $SourceCMakeUpdated
Write-Utf8 $TestsCMakePath $TestsCMakeUpdated
Write-Utf8 $SourcePath $Source
Write-Utf8 $TestPath $Tests

Write-Host "Applied RC-022g1 corrective patch."
Write-Host "Registered:"
Write-Host "  render/pipeline_authoring_overlay.cpp"
Write-Host "  render/pipeline_authoring_overlay.h"
Write-Host "  pipeline_authoring_overlay_tests.cpp"
Write-Host "Also fixed:"
Write-Host "  trailing whitespace"
Write-Host "  missing <utility>"
Write-Host "  invalid optional-vs-Approx test comparisons"
Write-Host ""
Write-Host "Next:"
Write-Host "  git diff --check"
Write-Host "  cmake --fresh --preset msvc-debug"
Write-Host "  cmake --build --preset tests-debug --parallel 1"
