param(
    [string]$OutputDirectory = $PSScriptRoot
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

Add-Type -AssemblyName System.Drawing

function New-RoundedRectanglePath
{
    param(
        [System.Drawing.RectangleF]$Bounds,
        [float]$Radius
    )

    $diameter = $Radius * 2.0
    $path = [System.Drawing.Drawing2D.GraphicsPath]::new()
    $path.AddArc($Bounds.Left, $Bounds.Top, $diameter, $diameter, 180.0, 90.0)
    $path.AddArc($Bounds.Right - $diameter, $Bounds.Top, $diameter, $diameter, 270.0, 90.0)
    $path.AddArc($Bounds.Right - $diameter, $Bounds.Bottom - $diameter, $diameter, $diameter, 0.0, 90.0)
    $path.AddArc($Bounds.Left, $Bounds.Bottom - $diameter, $diameter, $diameter, 90.0, 90.0)
    $path.CloseFigure()
    return $path
}

function New-MfdStudioMarkBitmap
{
    param(
        [int]$Size,
        [bool]$Inverse
    )

    $bitmap = [System.Drawing.Bitmap]::new(
        $Size,
        $Size,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)

    try
    {
        $graphics.Clear([System.Drawing.Color]::Transparent)
        $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
        $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

        $surfaceColor = if ($Inverse) {
            [System.Drawing.Color]::FromArgb(255, 247, 245, 240)
        } else {
            [System.Drawing.Color]::FromArgb(255, 32, 35, 39)
        }
        $markColor = if ($Inverse) {
            [System.Drawing.Color]::FromArgb(255, 32, 35, 39)
        } else {
            [System.Drawing.Color]::FromArgb(255, 247, 245, 240)
        }

        $padding = [single]($Size * 0.09375)
        $frameBounds = [System.Drawing.RectangleF]::new(
            $padding,
            $padding,
            [single]($Size - 2.0 * $padding),
            [single]($Size - 2.0 * $padding))
        $cornerRadius = [single]($Size * 0.1875)
        $framePath = New-RoundedRectanglePath -Bounds $frameBounds -Radius $cornerRadius
        $surfaceBrush = [System.Drawing.SolidBrush]::new($surfaceColor)

        try
        {
            $graphics.FillPath($surfaceBrush, $framePath)
        }
        finally
        {
            $surfaceBrush.Dispose()
            $framePath.Dispose()
        }

        $points = [System.Drawing.PointF[]]@(
            [System.Drawing.PointF]::new([single]($Size * 0.2930), [single]($Size * 0.7129)),
            [System.Drawing.PointF]::new([single]($Size * 0.2930), [single]($Size * 0.2930)),
            [System.Drawing.PointF]::new([single]($Size * 0.5000), [single]($Size * 0.4590)),
            [System.Drawing.PointF]::new([single]($Size * 0.7070), [single]($Size * 0.2930)),
            [System.Drawing.PointF]::new([single]($Size * 0.7070), [single]($Size * 0.7129)))

        $pen = [System.Drawing.Pen]::new($markColor, [single]($Size * 0.140625))
        try
        {
            $pen.StartCap = [System.Drawing.Drawing2D.LineCap]::Square
            $pen.EndCap = [System.Drawing.Drawing2D.LineCap]::Square
            $pen.LineJoin = [System.Drawing.Drawing2D.LineJoin]::Miter
            $graphics.DrawLines($pen, $points)
        }
        finally
        {
            $pen.Dispose()
        }
    }
    finally
    {
        $graphics.Dispose()
    }

    return $bitmap
}

function Convert-BitmapToPngBytes
{
    param([System.Drawing.Bitmap]$Bitmap)

    $stream = [System.IO.MemoryStream]::new()
    try
    {
        $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
        return $stream.ToArray()
    }
    finally
    {
        $stream.Dispose()
    }
}

function Write-MultiResolutionIcon
{
    param(
        [string]$Path,
        [int[]]$Sizes
    )

    $images = [System.Collections.Generic.List[byte[]]]::new()
    foreach ($size in $Sizes)
    {
        $bitmap = New-MfdStudioMarkBitmap -Size $size -Inverse $false
        try
        {
            $images.Add((Convert-BitmapToPngBytes -Bitmap $bitmap))
        }
        finally
        {
            $bitmap.Dispose()
        }
    }

    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Create)
    $writer = [System.IO.BinaryWriter]::new($stream)
    try
    {
        $writer.Write([uint16]0)
        $writer.Write([uint16]1)
        $writer.Write([uint16]$Sizes.Count)

        $offset = 6 + 16 * $Sizes.Count
        for ($index = 0; $index -lt $Sizes.Count; ++$index)
        {
            $size = $Sizes[$index]
            $encodedSize = if ($size -ge 256) { 0 } else { $size }
            $writer.Write([byte]$encodedSize)
            $writer.Write([byte]$encodedSize)
            $writer.Write([byte]0)
            $writer.Write([byte]0)
            $writer.Write([uint16]1)
            $writer.Write([uint16]32)
            $writer.Write([uint32]$images[$index].Length)
            $writer.Write([uint32]$offset)
            $offset += $images[$index].Length
        }

        foreach ($image in $images)
        {
            $writer.Write([byte[]]$image)
        }
    }
    finally
    {
        $writer.Dispose()
        $stream.Dispose()
    }
}

$resolvedOutputDirectory = [System.IO.Path]::GetFullPath($OutputDirectory)
[System.IO.Directory]::CreateDirectory($resolvedOutputDirectory) | Out-Null

$darkMark = New-MfdStudioMarkBitmap -Size 1024 -Inverse $false
$lightMark = New-MfdStudioMarkBitmap -Size 1024 -Inverse $true
try
{
    $darkMark.Save(
        (Join-Path $resolvedOutputDirectory "mfdstudio_app_icon.png"),
        [System.Drawing.Imaging.ImageFormat]::Png)
    $darkMark.Save(
        (Join-Path $resolvedOutputDirectory "mfdstudio_mark_dark.png"),
        [System.Drawing.Imaging.ImageFormat]::Png)
    $lightMark.Save(
        (Join-Path $resolvedOutputDirectory "mfdstudio_mark_light.png"),
        [System.Drawing.Imaging.ImageFormat]::Png)
}
finally
{
    $darkMark.Dispose()
    $lightMark.Dispose()
}

Write-MultiResolutionIcon -Path (Join-Path $resolvedOutputDirectory "mfdstudio_app_icon.ico") -Sizes @(16, 20, 24, 32, 40, 48, 64, 128, 256)

Write-Host "Generated MFDStudio branding assets in $resolvedOutputDirectory"
