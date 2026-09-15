# brand.ps1 -- builds the channel identity and converts it to uploadable PNGs.
#
#   .\brand.ps1                  # everything: avatar, banner, watermark, thumbnails
#   .\brand.ps1 -Skip Thumbs     # leave the thumbnails alone (they need demo stills)
#
# The renderer writes PPM because that is twelve lines of C; ffmpeg turns those
# into PNG here. Same division of labour as the film: nothing that makes an
# image happens outside our own code.
#
# Intermediates land in brand/work (throwaway). Finished assets land in
# brand/channel and brand/thumbnails, which are the two folders you actually
# upload from.

param(
    [string]$Skip = ""
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root

# ffmpeg must be on PATH, or FFMPEG_BIN must point at the directory holding it.
if ($env:FFMPEG_BIN -and (Test-Path $env:FFMPEG_BIN)) {
    $env:Path = "$env:Path;$env:FFMPEG_BIN"
}
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw "ffmpeg not found on PATH. Install it, or set FFMPEG_BIN to its bin directory."
}

$work = "$root\brand\work"
New-Item -ItemType Directory -Force -Path $work, "$root\brand\channel", "$root\brand\thumbnails" | Out-Null

& "$root\build.ps1" -Tool brand
& "$root\tools\out\brand.exe" all
if ($LASTEXITCODE -ne 0) { throw "brand render failed" }

# Thumbnails composite over stills the demo renderer already produced. The
# caption is what is being tested here, not the plate: if the video title
# already carries the claim, a thumbnail repeating it spends its two lines
# saying nothing new.
#
# ENDURANCE's plates come from its `plate` mode: the picture filling the whole
# 16:9 frame with no captions, where a film frame would bring its letterbox
# bars and its burned-in text along. They are three, for YouTube's Test &
# compare, and each caption is written against the title it is tested with.
$thumbs = @(
    @{ dir = "01-coldstart";  still = "still_083.00.ppm";  name = "coldstart-lines";  l1 = "5228 LINES";  l2 = "NO ASSETS" },
    @{ dir = "01-coldstart";  still = "still_083.00.ppm";  name = "coldstart-assets"; l1 = "NO ASSETS";   l2 = "JUST C" },
    @{ dir = "01-coldstart";  still = "still_019.00.ppm";  name = "coldstart-tunnel"; l1 = "5228 LINES";  l2 = "NO ASSETS" },
    # Rendered with `endurance plate 112 3840 4`, `plate 36.5 ...`, `plate 70.5 ...`.
    # The twilight plate has no kicker: its top left is bright sky.
    @{ dir = "05-endurance";  still = "plate_112.000.ppm"; name = "endurance-found";  l1 = "LOST 1915";   l2 = "FOUND 2022";  kick = "ENDURANCE / KORMOS"; crop = "364,345,3111,1750" },
    @{ dir = "05-endurance";  still = "plate_036.500.ppm"; name = "endurance-depth";  l1 = "3,008 M";     l2 = "106 YEARS";   kick = " " },
    @{ dir = "05-endurance";  still = "plate_070.500.ppm"; name = "endurance-code";   l1 = "EVERY FRAME"; l2 = "IS C CODE";   kick = "ENDURANCE / KORMOS" }
)

if ($Skip -ne "Thumbs") {
    foreach ($t in $thumbs) {
        $src = "$root\demos\$($t.dir)\out\$($t.still)"
        if (-not (Test-Path $src)) {
            Write-Host "[skip] $($t.name): no $($t.dir)\out\$($t.still) -- render that frame first"
            continue
        }
        if     ($t.crop) { & "$root\tools\out\brand.exe" thumb $src $t.name $t.l1 $t.l2 $t.kick $t.crop }
        elseif ($t.kick) { & "$root\tools\out\brand.exe" thumb $src $t.name $t.l1 $t.l2 $t.kick }
        else             { & "$root\tools\out\brand.exe" thumb $src $t.name $t.l1 $t.l2 }
        if ($LASTEXITCODE -ne 0) { throw "thumbnail $($t.name) failed" }
    }
}

# ---- PPM -> PNG -------------------------------------------------------------

function Convert-Png($srcName, $dstPath) {
    $src = "$work\$srcName.ppm"
    if (-not (Test-Path $src)) { return }
    & ffmpeg -y -loglevel error -i $src $dstPath
    Write-Host "[png] $($dstPath.Replace("$root\", ''))"
}

Convert-Png "avatar" "$root\brand\channel\avatar.png"
Convert-Png "banner" "$root\brand\channel\banner.png"
foreach ($t in $thumbs) {
    Convert-Png "thumb_$($t.name)" "$root\brand\thumbnails\$($t.name).png"
    # And a JPEG, which stays well under the upload form's 2 MB limit however
    # busy the plate is.
    if (Test-Path "$work\thumb_$($t.name).ppm") {
        & ffmpeg -y -loglevel error -i "$work\thumb_$($t.name).ppm" -q:v 2 "$root\brand\thumbnails\$($t.name).jpg"
    }
}

# The watermark is the one asset that needs an alpha channel: it is stamped
# straight onto the video, and anything opaque would arrive as a black square.
& ffmpeg -y -loglevel error -i "$work\watermark.ppm" -i "$work\watermark_a.pgm" `
    -filter_complex "[0][1]alphamerge,format=rgba" "$root\brand\channel\watermark.png"
Write-Host "[png] brand\channel\watermark.png"

Get-ChildItem "$root\brand\channel\*.png", "$root\brand\thumbnails\*.png" | ForEach-Object {
    "{0,-28} {1,9:N0} bytes" -f $_.Name, $_.Length
}
