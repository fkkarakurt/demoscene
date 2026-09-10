# publish.ps1 -- stamps a finished render with its own metadata and gives it the
# name it is uploaded under.
#
#   .\publish.ps1 03-liftoff
#   .\publish.ps1 03-liftoff -Replace   # drop the untagged render afterwards
#
# The renderer writes a fragmented MP4. That is the right shape for a file being
# appended to one frame at a time and the wrong shape for one being uploaded:
# the index sits at the end, so nothing can start playing until the whole file
# has arrived. Remuxing moves it to the front. There is no re-encode -- the
# frames are copied through untouched, and it takes a few seconds.
#
# The tags go on in the same pass. A file that leaves this repository should say
# what it is, who made it and where the source is, without anybody having to
# remember; the upload name matters for the same reason, because YouTube fills
# the title in from the filename.

param(
    [string]$Demo = "04-trace",
    [switch]$Replace
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root

if ($env:FFMPEG_BIN -and (Test-Path $env:FFMPEG_BIN)) {
    $env:Path = "$env:Path;$env:FFMPEG_BIN"
}
if (-not (Get-Command ffmpeg -ErrorAction SilentlyContinue)) {
    throw "ffmpeg not found on PATH. Install it, or set FFMPEG_BIN to its bin directory."
}

# One entry per demo, added when that demo is actually being published --
# COLD START went up under a title this script did not write, and RUNTIME has
# not gone up at all, so neither has an entry here yet.
#
# `Upload` is a filename rather than a title: it is what the upload form reads,
# so it opens with the words the title should open with.
$meta = @{
    "03-liftoff" = @{
        File    = "liftoff.mp4"
        Upload  = "LIFTOFF - A Rocket Launch Written in C - No Assets, No Engine, No GPU.mp4"
        Title   = "LIFTOFF - A Rocket Launch Written in C"
        Comment = "Thirty seconds of a launch to orbit, computed at runtime from nothing but C. No images, no audio files, no 3D models, no game engine, no GPU. The trajectory is integrated rather than keyframed, and every camera, the exhaust and the numbers on screen read out of that one integration. Source: https://github.com/fkkarakurt/demoscene"
    }
    "04-trace" = @{
        File    = "trace.mp4"
        Upload  = "TRACE - The Soundtrack Is the Picture - Written in C.mp4"
        Title   = "TRACE - The Soundtrack Is the Picture"
        Comment = "Thirty seconds in which nothing is drawn. Every frame is the soundtrack itself, plotted the way an oscilloscope in XY mode plots a stereo signal - left channel across, right channel up - with time laid out in depth behind it. Music and picture are computed at runtime from nothing but C: no images, no audio files, no engine, no GPU. Source: https://github.com/fkkarakurt/demoscene"
    }
}

if (-not $meta.ContainsKey($Demo)) { throw "no publish metadata for demo: $Demo" }
$m = $meta[$Demo]

$dir = "$root\demos\$Demo\out"
$src = "$dir\$($m.File)"
if (-not (Test-Path $src)) { throw "nothing to publish: $src -- render it first" }

$dst = "$dir\$($m.Upload)"
$year = (Get-Item $src).LastWriteTime.Year

# -c copy: the H.264 and AAC streams are carried across bit for bit. The only
# things this pass changes are where the index sits and what the tags say.
& ffmpeg -y -hide_banner -loglevel error -i $src -c copy -movflags +faststart `
    -metadata "title=$($m.Title)" `
    -metadata "artist=KORMOS" `
    -metadata "album=demoscene" `
    -metadata "date=$year" `
    -metadata "genre=Demoscene" `
    -metadata "comment=$($m.Comment)" `
    -metadata "copyright=MIT - https://github.com/fkkarakurt/demoscene" `
    $dst
if ($LASTEXITCODE -ne 0) { throw "remux failed" }

# A remux that copies streams cannot lose much without losing nearly all of it,
# so comparing sizes is enough to catch a truncated or empty result -- and it is
# worth catching before the only other copy is deleted.
$srcLen = (Get-Item $src).Length
$dstLen = (Get-Item $dst).Length
if ($dstLen -lt $srcLen * 0.9) {
    throw "remux looks wrong: $dstLen bytes out of $srcLen in. Keeping $($m.File)."
}

# The render is hours of work and this file is not in git, so it is kept unless
# dropping it is asked for.
if ($Replace) { Remove-Item $src }

$mb = "{0:N1}" -f ($dstLen / 1MB)
Write-Host "[publish] $($m.Upload)  $mb MB"
Write-Host "[publish] upload from demos\$Demo\out"
