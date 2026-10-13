<#
.SYNOPSIS
Export Android launcher resources from the approved transparent badge artwork.
#>
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$iconRepoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$iconArtDirectory = Join-Path $iconRepoRoot 'app/assets/branding'
$iconResourceDirectory = Join-Path $iconRepoRoot 'app/android/app/src/main/res'
Add-Type -AssemblyName System.Drawing
$iconSourceImage = [System.Drawing.Image]::FromFile(
    (Join-Path $iconArtDirectory 'badge-icon-source.png'))
function Export-BadgeIcon {
    param([string]$Path, [int]$Size, [bool]$Foreground)
    $iconBitmap = New-Object System.Drawing.Bitmap($Size, $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $iconGraphics = [System.Drawing.Graphics]::FromImage($iconBitmap)
    $iconAttributes = New-Object System.Drawing.Imaging.ImageAttributes
    try {
        $iconGraphics.Clear($(if ($Foreground) {
            [System.Drawing.Color]::Transparent
        } else { [System.Drawing.ColorTranslator]::FromHtml('#111A22') }))
        $iconGraphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $iconGraphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $iconGraphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $iconAttributes.SetWrapMode([System.Drawing.Drawing2D.WrapMode]::TileFlipXY)
        # The source already includes transparent space. Extra adaptive inset
        # protects the lanyard and LED glow across launcher masks.
        $iconExtent = if ($Foreground) { [int][Math]::Round($Size * 0.9) } else { $Size }
        $iconInset = [int][Math]::Floor(($Size - $iconExtent) / 2)
        $iconRectangle = New-Object System.Drawing.Rectangle($iconInset, $iconInset,
            $iconExtent, $iconExtent)
        $iconGraphics.DrawImage($iconSourceImage, $iconRectangle, 0, 0,
            $iconSourceImage.Width, $iconSourceImage.Height,
            [System.Drawing.GraphicsUnit]::Pixel, $iconAttributes)
        $iconParentDirectory = Split-Path -Parent $Path
        [void][System.IO.Directory]::CreateDirectory($iconParentDirectory)
        $iconBitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $iconAttributes.Dispose()
        $iconGraphics.Dispose()
        $iconBitmap.Dispose()
    }
}
try {
    $iconDensities = @(
        @{ Name = 'mdpi'; Legacy = 48; Adaptive = 108 },
        @{ Name = 'hdpi'; Legacy = 72; Adaptive = 162 },
        @{ Name = 'xhdpi'; Legacy = 96; Adaptive = 216 },
        @{ Name = 'xxhdpi'; Legacy = 144; Adaptive = 324 },
        @{ Name = 'xxxhdpi'; Legacy = 192; Adaptive = 432 }
    )
    foreach ($iconDensity in $iconDensities) {
        $iconDensityDirectory = Join-Path $iconResourceDirectory "mipmap-$($iconDensity.Name)"
        Export-BadgeIcon (Join-Path $iconDensityDirectory 'ic_badge.png') $iconDensity.Legacy $false
        Export-BadgeIcon (Join-Path $iconDensityDirectory 'ic_badge_foreground.png') $iconDensity.Adaptive $true
    }
    Export-BadgeIcon (Join-Path $iconArtDirectory 'badge-icon.png') 512 $false
    Write-Output 'Exported badge launcher icons for five Android densities.'
} finally { $iconSourceImage.Dispose() }
