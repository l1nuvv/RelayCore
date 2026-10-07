$projectRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $projectRoot 'build/msvc-relwithdebinfo/src/relaygame/RelayCore.exe') `
    --config (Join-Path $projectRoot 'assets/config/occult_lab_b17.toml') `
    --color-pipeline hardware_srgb
exit $LASTEXITCODE
