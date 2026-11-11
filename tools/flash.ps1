# Project Kestrel - flash one side of the MAKCU
#
# Usage:
#   powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Left  -Port COM4
#   powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Right -Port COM4
#   powershell -ExecutionPolicy Bypass -File tools\flash.ps1 -Side Left  -Port COM4 -Restore
#
# Put the chip in flash mode first: unplug everything, hold that side's button,
# and plug the cable into that side's USB port. It shows up as "USB Serial Device (COMx)".
#
# The first time you flash a side, the MAKCU's original firmware is backed up to
# backups\makcu_<side>_original.bin (takes ~10 minutes, checked with SHA-256).
# -Restore writes that backup back.
#
# Everything goes through the chip's ROM loader (--no-stub): esptool's helper stub
# re-enumerates USB on the MAKCU and stalls.

param(
    [Parameter(Mandatory = $true)][ValidateSet("Left", "Right")][string]$Side,
    [Parameter(Mandatory = $true)][string]$Port,
    [switch]$Restore
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
$idf = "C:\esp\esp-idf-v5.5.6\export.ps1"
if (-not (Test-Path $idf)) { throw "ESP-IDF not found at $idf" }
. $idf | Out-Null

$sideLower = $Side.ToLower()
$image  = if ($Side -eq "Left") { "$root\release\kestrel_LEFT_output.bin" } else { "$root\release\kestrel_RIGHT_input.bin" }
$backup = "$root\backups\makcu_${sideLower}_original.bin"
New-Item -ItemType Directory -Force "$root\backups" | Out-Null

$common = @("--chip", "esp32s3", "-p", $Port, "--before", "no_reset", "--after", "no_reset", "--no-stub")

if ($Restore) {
    if (-not (Test-Path $backup)) { throw "No backup found at $backup" }
    Write-Host "Restoring original MAKCU firmware to the $Side chip..." -ForegroundColor Yellow
    esptool.py @common write_flash 0x0 $backup
    if ($LASTEXITCODE -ne 0) { throw "Restore failed" }
    Write-Host "Restored. Unplug and replug the MAKCU." -ForegroundColor Green
    exit 0
}

if (-not (Test-Path $image)) { throw "Missing $image - run tools\build.ps1 first" }

if (-not (Test-Path $backup)) {
    Write-Host "Backing up the original $Side firmware first..." -ForegroundColor Cyan
    python "$PSScriptRoot\backup_makcu.py" $Port $backup
    if ($LASTEXITCODE -ne 0) { throw "Backup failed - nothing was written. Replug in flash mode and try again." }
} else {
    Write-Host "Backup already exists: $backup" -ForegroundColor DarkGray
}

Write-Host "Flashing Project Kestrel to the $Side chip (saved settings are kept)..." -ForegroundColor Cyan
# Write the parts separately so the settings area (NVS, 0x9000-0xEFFF) is left alone.
$parts = "$root\release\$sideLower"
if (-not (Test-Path "$parts\app.bin")) { throw "Missing $parts\app.bin - run tools\build.ps1 first" }
esptool.py @common write_flash 0x0 "$parts\bootloader.bin" 0x8000 "$parts\partition-table.bin" 0x10000 "$parts\app.bin"
if ($LASTEXITCODE -ne 0) { throw "Flash failed. Replug in flash mode and try again." }
Write-Host "Done. Unplug the cable, then plug the MAKCU back in normally." -ForegroundColor Green
