param(
    [string]$SourcePath = "$PSScriptRoot/../assets/textures/facility_source.png",
    [string]$OutputPath = "$PSScriptRoot/../assets/textures/blockout_atlas.bmp",
    [string]$ScientistPath = "$PSScriptRoot/../assets/textures/scientist_materials.png",
    [string]$WingPath = "$PSScriptRoot/../assets/textures/wing_materials.png"
)
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$source = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $SourcePath))
$scientist = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $ScientistPath))
$wing = [System.Drawing.Bitmap]::FromFile((Resolve-Path -LiteralPath $WingPath))
$atlas = [System.Drawing.Bitmap]::new(2048, 1024)
# SurfaceMaterial order; each source square is packed with a 16-pixel repeating gutter.
$tiles = @(0, 5, 2, 5, 7, 2, 4, 0, 1, 3, 1, 3, 3, 5, 1, 1, 5, 5, 1, 5, 6, 5)
try {
    if ($source.Width -ne $source.Height * 2) { throw 'Expected a 4 by 2 source texture sheet.' }
    if ($wing.Width -ne $wing.Height) { throw 'Expected a 2 by 2 wing texture sheet.' }
    $cell = $source.Width / 4
    for ($index = 0; $index -lt 32; ++$index) {
        $sourceIndex = if ($index -lt $tiles.Count) { $tiles[$index] } else { 5 }
        $tile = [System.Drawing.Bitmap]::new(224, 224)
        $graphics = [System.Drawing.Graphics]::FromImage($tile)
        try {
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $rect = [System.Drawing.RectangleF]::new(($sourceIndex % 4) * $cell, [math]::Floor($sourceIndex / 4) * $cell, $cell, $cell)
            $image = $source
            if ($index -ge 23 -and $index -le 26) {
                $clothCell = $scientist.Width / 2
                $clothIndex = $index - 23
                $rect = [System.Drawing.RectangleF]::new(($clothIndex % 2) * $clothCell, [math]::Floor($clothIndex / 2) * $clothCell, $clothCell, $clothCell)
                $image = $scientist
            } elseif ($index -ge 27 -and $index -le 30) {
                $wingCell = $wing.Width / 2
                $wingIndex = $index - 27
                $rect = [System.Drawing.RectangleF]::new(($wingIndex % 2) * $wingCell, [math]::Floor($wingIndex / 2) * $wingCell, $wingCell, $wingCell)
                $image = $wing
            }
            $graphics.DrawImage($image, [System.Drawing.RectangleF]::new(0, 0, 224, 224), $rect, [System.Drawing.GraphicsUnit]::Pixel)
            for ($y = 0; $y -lt 256; ++$y) {
                for ($x = 0; $x -lt 256; ++$x) {
                    $sx = ($x - 16 + 224) % 224
                    $sy = 223 - (($y - 16 + 224) % 224)
                    $atlas.SetPixel(($index % 8) * 256 + $x, [math]::Floor($index / 8) * 256 + $y, $tile.GetPixel($sx, $sy))
                }
            }
        } finally { $graphics.Dispose(); $tile.Dispose() }
    }
    $atlas.Save([System.IO.Path]::GetFullPath($OutputPath), [System.Drawing.Imaging.ImageFormat]::Bmp)
} finally { $atlas.Dispose(); $source.Dispose(); $scientist.Dispose(); $wing.Dispose() }
