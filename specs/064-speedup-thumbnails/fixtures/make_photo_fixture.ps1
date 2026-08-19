# Builds the thumbnail-performance fixture folders (quickstart.md Prerequisites).
#
#   powershell -ExecutionPolicy Bypass -File make_photo_fixture.ps1 `
#       [-Root <folder>] [-PlainCount 2000] [-SmallCount 100] [-Width 6000] [-Height 4000]
#
# Generates:
#   <Root>\big-plain\  - synthetic JPEGs WITHOUT embedded EXIF thumbnails
#                        (exercises the reduced-resolution decode path).
#                        Noise-filled so files are multi-MB like real photos.
#   <Root>\small\      - 100 synthetic JPEGs (control folder, SC-004)
#   <Root>\extras\     - one corrupt .jpg, one oversized (>90 MPix guard) note,
#                        and non-image files
#
# NOT generated: big-exif\ - synthetic images cannot carry representative EXIF
# preview thumbnails (cameras embed them, GDI+ does not). Stage it by COPYING
# a real photo archive (>=5,000 camera JPEGs) into <Root>\big-exif\ - the
# embedded-preview fast path and EXIF rotation must be validated on real files.
#
# Generation of 2,000 x ~4-8 MB files takes tens of minutes and ~10+ GB of disk;
# lower -PlainCount for a quick run (the scheduling behavior shows from ~500).
param(
    [string]$Root = "$env:USERPROFILE\Desktop\thumb-fixture",
    [int]$PlainCount = 2000,
    [int]$SmallCount = 100,
    [int]$Width = 6000,
    [int]$Height = 4000
)

Add-Type -AssemblyName System.Drawing

function New-NoiseJpeg([string]$path, [int]$w, [int]$h, [int]$seed) {
    $bmp = New-Object System.Drawing.Bitmap($w, $h)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $rnd = New-Object System.Random($seed)
    # coarse random rectangles: compresses poorly enough to give multi-MB files
    for ($i = 0; $i -lt 400; $i++) {
        $c = [System.Drawing.Color]::FromArgb($rnd.Next(256), $rnd.Next(256), $rnd.Next(256))
        $b = New-Object System.Drawing.SolidBrush($c)
        $g.FillRectangle($b, $rnd.Next($w), $rnd.Next($h), $rnd.Next(64, 600), $rnd.Next(64, 600))
        $b.Dispose()
    }
    # fine noise band for entropy
    for ($i = 0; $i -lt 30000; $i++) {
        $c = [System.Drawing.Color]::FromArgb($rnd.Next(256), $rnd.Next(256), $rnd.Next(256))
        $b = New-Object System.Drawing.SolidBrush($c)
        $g.FillRectangle($b, $rnd.Next($w), $rnd.Next($h), $rnd.Next(1, 8), $rnd.Next(1, 8))
        $b.Dispose()
    }
    $g.Dispose()
    $enc = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
    $p = New-Object System.Drawing.Imaging.EncoderParameters(1)
    $p.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, 95L)
    $bmp.Save($path, $enc, $p)
    $bmp.Dispose()
}

foreach ($dir in @("$Root\big-plain", "$Root\small", "$Root\extras")) {
    New-Item -ItemType Directory -Force $dir | Out-Null
}

Write-Host "small/ ($SmallCount files)..."
for ($i = 1; $i -le $SmallCount; $i++) {
    $f = "$Root\small\photo-{0:d4}.jpg" -f $i
    if (-not (Test-Path $f)) { New-NoiseJpeg $f 3000 2000 $i }
}

Write-Host "big-plain/ ($PlainCount files, this takes a while)..."
for ($i = 1; $i -le $PlainCount; $i++) {
    $f = "$Root\big-plain\photo-{0:d5}.jpg" -f $i
    if (-not (Test-Path $f)) { New-NoiseJpeg $f $Width $Height $i }
    if ($i % 100 -eq 0) { Write-Host "  $i / $PlainCount" }
}

# extras: corrupt file + non-image files
Set-Content -LiteralPath "$Root\extras\corrupt.jpg" -Value "this is not a jpeg" -Encoding ascii
Set-Content -LiteralPath "$Root\extras\readme.txt" -Value "non-image file" -Encoding ascii
Set-Content -LiteralPath "$Root\extras\data.bin" -Value ([byte[]](1..64)) -Encoding Byte
Write-Host @"
Done. Remaining manual steps:
 - stage big-exif\ by copying >=5,000 REAL camera JPEGs (EXIF previews + portrait shots)
 - optionally add one >90 MPix image to extras\ (e.g. a stitched panorama)
"@
