param()

$ErrorActionPreference = 'Stop'

Set-Location -LiteralPath (git rev-parse --show-toplevel)

function Get-StagedFiles
{
    param(
        [string[]]$Pathspec = @()
    )

    $args = @('diff', '--cached', '--name-only', '--diff-filter=ACMR')
    if ($Pathspec.Count -gt 0)
    {
        $args += '--'
        $args += $Pathspec
    }

    $result = & git @args
    if ($LASTEXITCODE -ne 0)
    {
        throw 'git diff --cached failed'
    }

    return @($result | Where-Object { $_ -and $_.Trim().Length -gt 0 })
}

function Get-ClangFormatPath
{
    $cmd = Get-Command clang-format -ErrorAction SilentlyContinue
    if ($cmd)
    {
        return $cmd.Source
    }

    $candidates = @(
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-format.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\VC\Tools\Llvm\x64\bin\clang-format.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\VC\Tools\Llvm\x64\bin\clang-format.exe",
        "$env:ProgramFiles\LLVM\bin\clang-format.exe"
    )

    foreach ($candidate in $candidates)
    {
        if ($candidate -and (Test-Path -LiteralPath $candidate))
        {
            return $candidate
        }
    }

    return $null
}

function Format-StagedCppFiles
{
    $cppFiles = Get-StagedFiles @('*.c', '*.cpp', '*.h', '*.hpp', ':(exclude)extern/**')
    if ($cppFiles.Count -eq 0)
    {
        return
    }

    $clangFormat = Get-ClangFormatPath
    if (-not $clangFormat)
    {
        throw 'clang-format.exe not found'
    }

    Write-Host "Formatting staged C/C++ files with clang-format..."
    & $clangFormat -i -- $cppFiles
    if ($LASTEXITCODE -ne 0)
    {
        throw 'clang-format failed'
    }

    & git add -- $cppFiles
    if ($LASTEXITCODE -ne 0)
    {
        throw 'git add after clang-format failed'
    }
}

function Assert-NoConflictMarkers
{
    $textFiles = Get-StagedFiles
    if ($textFiles.Count -eq 0)
    {
        return
    }

    $badFiles = New-Object System.Collections.Generic.List[string]
    $pattern = '^(<<<<<<<|=======|>>>>>>>)'

    foreach ($file in $textFiles)
    {
        if (-not (Test-Path -LiteralPath $file))
        {
            continue
        }

        try
        {
            $content = Get-Content -LiteralPath $file -Raw -ErrorAction Stop
        }
        catch
        {
            continue
        }

        if ($content -match $pattern)
        {
            $badFiles.Add($file)
        }
    }

    if ($badFiles.Count -gt 0)
    {
        Write-Error ("Conflict markers found in staged files:`n - " + ($badFiles -join "`n - "))
        exit 1
    }
}

function Assert-ValidJson
{
    $jsonFiles = Get-StagedFiles @('*.json')
    if ($jsonFiles.Count -eq 0)
    {
        return
    }

    $badFiles = New-Object System.Collections.Generic.List[string]

    foreach ($file in $jsonFiles)
    {
        if (-not (Test-Path -LiteralPath $file))
        {
            continue
        }

        try
        {
            Get-Content -LiteralPath $file -Raw -ErrorAction Stop | ConvertFrom-Json -ErrorAction Stop | Out-Null
        }
        catch
        {
            $badFiles.Add($file)
        }
    }

    if ($badFiles.Count -gt 0)
    {
        Write-Error ("Invalid JSON in staged files:`n - " + ($badFiles -join "`n - "))
        exit 1
    }
}

try
{
    Format-StagedCppFiles
    Assert-NoConflictMarkers
    Assert-ValidJson
    exit 0
}
catch
{
    Write-Error $_
    exit 1
}
