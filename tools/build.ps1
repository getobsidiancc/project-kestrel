# Project Kestrel - build both firmwares
# Usage:  powershell -ExecutionPolicy Bypass -File tools\build.ps1
#
# release\kestrel_<SIDE>_*.bin        merged image (writes the whole chip, ERASES saved settings)
# release\<side>\bootloader.bin etc.  separate parts (flash.ps1 uses these, keeps saved settings)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$idf = "C:\esp\esp-idf-v5.5.6\export.ps1"
if (-not (Test-Path $idf)) { throw "ESP-IDF not found at $idf" }
. $idf | Out-Null

New-Item -ItemType Directory -Force "$root\release" | Out-Null

foreach ($p in @(
    @{ dir = "kestrel_input";  side = "right"; out = "kestrel_RIGHT_input.bin"  },
    @{ dir = "kestrel_output"; side = "left";  out = "kestrel_LEFT_output.bin" })) {
    Write-Host "`n=== Building $($p.dir) ===" -ForegroundColor Cyan
    Push-Location "$root\firmware\$($p.dir)"
    idf.py build
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "$($p.dir) build failed" }
    idf.py merge-bin
    if ($LASTEXITCODE -ne 0) { Pop-Location; throw "$($p.dir) merge-bin failed" }
    Copy-Item "build\merged-binary.bin" "$root\release\$($p.out)" -Force
    $parts = "$root\release\$($p.side)"
    New-Item -ItemType Directory -Force $parts | Out-Null
    Copy-Item "build\bootloader\bootloader.bin" "$parts\bootloader.bin" -Force
    Copy-Item "build\partition_table\partition-table.bin" "$parts\partition-table.bin" -Force
    Copy-Item "build\$($p.dir).bin" "$parts\app.bin" -Force
    Pop-Location
}

Write-Host "`nDone. Flash images are in $root\release" -ForegroundColor Green
