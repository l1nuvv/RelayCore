param()

$ErrorActionPreference = 'Stop'

Set-Location -LiteralPath (git rev-parse --show-toplevel)

git config core.hooksPath .githooks
if ($LASTEXITCODE -ne 0) {
    throw 'Failed to set core.hooksPath'
}

Write-Host 'Installed Git hooks path: .githooks'
Write-Host 'Pre-commit hook is active.'
