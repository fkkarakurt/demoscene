# build.ps1 -- compiles a demo (or a dev tool) against the shared engine.
#
#   .\build.ps1                 # builds demos/01-coldstart
#   .\build.ps1 01-coldstart    # the same, named explicitly
#   .\build.ps1 -Tool smoke     # builds tools/smoke.c
#   .\build.ps1 -Debug          # -O0 -g, assertions, no fast math
#
# There is no makefile on purpose: the whole project is a couple of dozen
# translation units and gcc rebuilds all of them in under two seconds.

param(
    [string]$Demo  = "01-coldstart",
    [string]$Tool  = "",
    [switch]$Debug
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root

# ffmpeg is optional at build time and only used when rendering video. If it is
# not on PATH, set FFMPEG_BIN to the directory holding ffmpeg.exe.
if ($env:FFMPEG_BIN -and (Test-Path $env:FFMPEG_BIN)) {
    $env:Path = "$env:Path;$env:FFMPEG_BIN"
}

if ($Debug) {
    $flags = @("-std=c11", "-O0", "-g", "-Wall", "-Wextra")
} else {
    # -ffast-math but keeping NaN/Inf semantics: the framebuffer resolve relies
    # on being able to detect a NaN that leaked out of scene code.
    $flags = @("-std=c11", "-O3", "-march=native", "-ffast-math",
               "-fno-finite-math-only", "-Wall", "-Wextra")
}

$engine = Get-ChildItem "$root\engine\*.c" | ForEach-Object { $_.FullName }

if ($Tool -ne "") {
    New-Item -ItemType Directory -Force -Path "$root\tools\out" | Out-Null
    $out = "$root\tools\out\$Tool.exe"
    $src = @("$root\tools\$Tool.c") + $engine
} else {
    $demoDir = "$root\demos\$Demo"
    if (-not (Test-Path $demoDir)) { throw "no such demo: $demoDir" }
    New-Item -ItemType Directory -Force -Path "$demoDir\out" | Out-Null
    $out = "$demoDir\out\$($Demo -replace '^\d+-', '').exe"
    $src = @(Get-ChildItem "$demoDir\*.c", "$demoDir\scenes\*.c" |
             ForEach-Object { $_.FullName }) + $engine
}

Write-Host "[build] $($src.Count) files -> $out"
& gcc @flags -o $out @src -lm
if ($LASTEXITCODE -ne 0) { throw "compilation failed" }
Write-Host "[build] ok"
