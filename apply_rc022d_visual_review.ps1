param()

$ErrorActionPreference = "Stop"

function Decode-Utf8([string]$Base64) {
    return [System.Text.Encoding]::UTF8.GetString([System.Convert]::FromBase64String($Base64))
}

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

$RepoRoot = (Get-Location).Path
$StatusPath = Join-Path $RepoRoot "STATUS.md"
$ReferencePath = Join-Path $RepoRoot "docs/color_pipeline_visual_reference.md"

$Status = Read-Utf8 $StatusPath
$Reference = Read-Utf8 $ReferencePath

$NextHeading = Decode-Utf8 "IyMg0KHQu9C10LTRg9GO0YnQsNGPINGA0LDQsdC+0YLQsA=="
$ClosedC = Decode-Utf8 "LSBSQy0wMjJjIOKAlCDQt9Cw0LrRgNGL0YI6INC00L7QsdCw0LLQu9C10L0gb3B0LWluIGhhcmR3YXJlLXNSR0IgcHJvZHVjdGlvbiBwaXBlbGluZSDRgSBiZWhhdmlvcmFsIGRlZmF1bHQtZnJhbWVidWZmZXIgdmVyaWZpY2F0aW9uLCDQv9C+0LvQvdGL0LwgbGluZWFyIHNjZW5lIFJHQiBoYW5kbGluZyDQuCDRj9Cy0L3QvtC5IEltR3VpIHN0YXRlIGJvdW5kYXJ5OyBsZWdhY3lfZ2FtbWEg0YHQvtGF0YDQsNC90ZHQvSDQutCw0LogZGVmYXVsdC4="
$ClosedD = Decode-Utf8 "LSBSQy0wMjJkIOKAlCDQt9Cw0LrRgNGL0YI6IGxvY2FsIGxlZ2FjeV9nYW1tYS9oYXJkd2FyZV9zcmdiIEEvQiByZXZpZXcg0L/QvtC00YLQstC10YDQtNC40Lsg0L7RgtGB0YPRgtGB0YLQstC40LUg0L7Rh9C10LLQuNC00L3QvtCz0L4gZG91YmxlIGVuY29kaW5nINC4INGB0L7RhdGA0LDQvdC90L7RgdGC0YwgSW1HdWkvSFVEIGJvdW5kYXJ5LCDQvdC+INGB0YPRidC10YHRgtCy0YPRjtGJ0LjQtSBsaWdodGluZy9tYXRlcmlhbCB2YWx1ZXMg0L7RgdGC0LDRjtGC0YHRjyBsZWdhY3ktdHVuZWQ7IGxlZ2FjeV9nYW1tYSDQvtGB0YLQsNGR0YLRgdGPIHByb2R1Y3Rpb24gZGVmYXVsdCwgaGFyZHdhcmVfc3JnYiDigJQgZXhwbGljaXQgb3B0LWluLg=="
$NextWork = Decode-Utf8 "UkMtMDIyZSDigJQg0YPQt9C60LjQuSBoYXJkd2FyZS1zUkdCIHZpc3VhbCByZXR1bmluZyBhdXRob3JlZCBsaWdodGluZy9tYXRlcmlhbCB2YWx1ZXMg0LHQtdC3INC40LfQvNC10L3QtdC90LjRjyBwaXBlbGluZSBtYXRoINC4IHByb2R1Y3Rpb24gZGVmYXVsdCDQtNC+INC/0L7QstGC0L7RgNC90L7Qs9C+IGRldGVybWluaXN0aWMgQS9CIHJldmlldy4="
$DocBlock = Decode-Utf8 "IyMgUkMtMDIyZCBsb2NhbCBBL0IgcmV2aWV3CgpSZXZpZXcgZGF0ZTogMjAyNi0wNy0yNy4KClRoZSByZXZpZXcgdXNlZCBmcmVzaCBgbGVnYWN5X2dhbW1hYCBhbmQgYGhhcmR3YXJlX3NyZ2JgIHByb2Nlc3NlcyB3aXRoIHRoZSBzYW1lCmBhc3NldHMvY29uZmlnL3JjMDIyX2NhcHR1cmUudG9tbGAgcmVmZXJlbmNlIHNjZW5lLCBmaXhlZCBjYW1lcmEsIDI1NjB4MTQ0MCBkcmF3YWJsZSBleHRlbnQsIEltR3VpIGRpYWdub3N0aWNzCnZpc2libGUsIGFuZCB0aGUgSFVEIGluIHRoZSBzYW1lIGluYWN0aXZlIHN0YXRlLiBUaGUgY2FwdHVyZXMgcmVtYWluIGxvY2FsIHJldmlldyBhcnRpZmFjdHMgcmF0aGVyIHRoYW4KY3Jvc3MtR1BVIGdvbGRlbiBpbWFnZXMuCgojIyMgUnVudGltZSBldmlkZW5jZQoKVGhlIGhhcmR3YXJlIHJ1biBjb21wbGV0ZWQgc3RhcnR1cCBhbmQgc2h1dGRvd24gd2l0aG91dCBHTCBkZWJ1ZyBlcnJvcnMuIFRoZSBXaW5kb3dzIFNETC9XR0wgcGF0aCByZXBvcnRlZDoKCmBgYHRleHQKU0RMIHJlcG9ydGVkIGNhcGFiaWxpdHk9MQphdHRhY2htZW50IGVuY29kaW5nIHF1ZXJ5PUdMX0xJTkVBUgpiZWhhdmlvcmFsIGRpc2FibGVkIFJHQkE9MTI3LDEyNywxMjcsMjU1CmJlaGF2aW9yYWwgZW5hYmxlZCBSR0JBPTE4OCwxODgsMTg4LDI1NQp2ZXJpZmljYXRpb249cGFzc2VkCmBgYAoKVGhpcyBjb25maXJtcyBPdXRjb21lIEI6IHRoZSBhdHRhY2htZW50IGVuY29kaW5nIHF1ZXJ5IGlzIHVucmVsaWFibGUgb24gdGhpcyBwYXRoLCB3aGlsZSBjb250cm9sbGVkCmRyYXcvcmVhZGJhY2sgcHJvdmVzIHRoYXQgYEdMX0ZSQU1FQlVGRkVSX1NSR0JgIHBlcmZvcm1zIHRoZSBleHBlY3RlZCBoYXJkd2FyZSBsaW5lYXItdG8tc1JHQiBjb252ZXJzaW9uLgoKIyMjIFZpc3VhbCByZXN1bHQKCi0gTm8gb2J2aW91cyBkb3VibGUgZW5jb2Rpbmcgd2FzIG9ic2VydmVkLgotIEltR3VpIGRpYWdub3N0aWNzIGFuZCB0aGUgSFVEIHJldGFpbmVkIHRoZSBleHBlY3RlZCBlbmNvZGVkLXNwYWNlIGFwcGVhcmFuY2UgYWNyb3NzIGJvdGggbW9kZXMuCi0gQ29tcGFyZWQgd2l0aCBgbGVnYWN5X2dhbW1hYCwgYGhhcmR3YXJlX3NyZ2JgIGxpZnRzIGRhcmsgY2VpbGluZywgZmxvb3IsIGFuZCB3YWxsIHZhbHVlcy4KLSBCbHVlIGFuZCBvcmFuZ2UgbGlnaHRpbmcgZ3JhZGllbnRzIGNvdmVyIGEgYnJvYWRlciBhcmVhIGFuZCBhcHBlYXIgc21vb3RoZXIuCi0gTmV1dHJhbCBtYXRlcmlhbCBzZXBhcmF0aW9uIGFuZCBsb2NhbCBzaGFkb3cgY29udHJhc3QgYXJlIHJlZHVjZWQuCi0gVGhlIGJyaWdodCByaWdodC1zaWRlIGNvbHVtbiBhbmQgYmx1ZSB0cmFuc3BhcmVudCBzdXJmYWNlIGJlY29tZSBtb3JlIHZpc3VhbGx5IGRvbWluYW50LgotIFRoZXNlIGRpZmZlcmVuY2VzIGFyZSBjb25zaXN0ZW50IHdpdGggZXhpc3RpbmcgbGlnaHRpbmcgYW5kIG1hdGVyaWFsIHZhbHVlcyBoYXZpbmcgYmVlbiB0dW5lZCBmb3IgdGhlCiAgbGVnYWN5IGdhbW1hLXNwYWNlIHBpcGVsaW5lOyB0aGV5IGFyZSBub3QgZXZpZGVuY2UgdGhhdCB0aGUgaGFyZHdhcmUtc1JHQiBjb252ZXJzaW9uIGlzIG1hbGZ1bmN0aW9uaW5nLgoKIyMjIERlY2lzaW9uCgpgbGVnYWN5X2dhbW1hYCByZW1haW5zIHRoZSBwcm9kdWN0aW9uIGRlZmF1bHQuIGBoYXJkd2FyZV9zcmdiYCByZW1haW5zIGFuIGV4cGxpY2l0IHN0YXJ0dXAgb3B0LWluIGZvcgp2YWxpZGF0aW9uIGFuZCBmdXR1cmUgcmV0dW5pbmcuIFRoZSBwaXBlbGluZSBtYXRoZW1hdGljcywgYmVoYXZpb3JhbCBmcmFtZWJ1ZmZlciB2ZXJpZmljYXRpb24sIHRleHR1cmUKZW5jb2RpbmcgY29udHJhY3QsIGFuZCBJbUd1aSBzdGF0ZSBib3VuZGFyeSByZW1haW4gdW5jaGFuZ2VkLgoKQW55IHZpc3VhbCBtYXRjaGluZyB3b3JrIGJlbG9uZ3MgdG8gUkMtMDIyZSBhbmQgbXVzdCByZXR1bmUgYXV0aG9yZWQgbGlnaHRpbmcvbWF0ZXJpYWwgdmFsdWVzIHJhdGhlciB0aGFuCnJlaW50cm9kdWNpbmcgZ2FtbWEtc3BhY2Ugb3BlcmF0aW9ucyBpbnRvIHRoZSBoYXJkd2FyZSBwYXRoLgo="

$Newline = "`n"
if ($Status.Contains("`r`n")) {
    $Newline = "`r`n"
}

$HeadingPattern = [regex]::new(
    "(?m)^" + [regex]::Escape($NextHeading) + "[ `t]*$"
)
$HeadingMatches = $HeadingPattern.Matches($Status)
if ($HeadingMatches.Count -ne 1) {
    throw "Expected exactly one next-work heading in STATUS.md, found $($HeadingMatches.Count)"
}

$OpenRc022cPattern = [regex]::new("(?m)^RC-022c[^
]*$")
$OpenRc022cMatches = $OpenRc022cPattern.Matches($Status)
if ($OpenRc022cMatches.Count -ne 1) {
    throw "Expected exactly one open RC-022c line in STATUS.md, found $($OpenRc022cMatches.Count)"
}

if ($Status.Contains("- RC-022d")) {
    throw "STATUS.md already contains an RC-022d completion entry"
}

$CompletionLines = @()
if (-not $Status.Contains("- RC-022c")) {
    $CompletionLines += $ClosedC
}
$CompletionLines += $ClosedD

$CompletionBlock = [string]::Join($Newline, $CompletionLines) + $Newline + $Newline
$StatusUpdated = $HeadingPattern.Replace(
    $Status,
    [System.Text.RegularExpressions.MatchEvaluator]{
        param($Match)
        return $CompletionBlock + $Match.Value
    },
    1
)
$StatusUpdated = $OpenRc022cPattern.Replace($StatusUpdated, $NextWork, 1)

if ($Reference.Contains("## RC-022d local A/B review")) {
    throw "Visual reference already contains the RC-022d review"
}

$ReferenceNewline = "`n"
if ($Reference.Contains("`r`n")) {
    $ReferenceNewline = "`r`n"
}
$NormalizedDocBlock = $DocBlock.Replace("`n", $ReferenceNewline)
$ReferenceUpdated = $Reference.TrimEnd("`r", "`n") + $ReferenceNewline + $ReferenceNewline + $NormalizedDocBlock.TrimEnd("`r", "`n") + $ReferenceNewline

Write-Utf8 $StatusPath $StatusUpdated
Write-Utf8 $ReferencePath $ReferenceUpdated

Write-Host "Applied RC-022d visual review patch."
Write-Host "Updated:"
Write-Host "  STATUS.md"
Write-Host "  docs/color_pipeline_visual_reference.md"
Write-Host ""
Write-Host "Next:"
Write-Host "  git diff --check"
Write-Host "  git diff -- STATUS.md docs/color_pipeline_visual_reference.md"
