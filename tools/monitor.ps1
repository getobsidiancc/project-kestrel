# Project Kestrel - watch the adapter's log
#
# Usage:  powershell -ExecutionPolicy Bypass -File tools\monitor.ps1 -Port COM7
#
# Use the MAKCU's middle port (the CH343 "COM" port). Press Ctrl+] to quit.

param([Parameter(Mandatory = $true)][string]$Port)

$idf = "C:\esp\esp-idf-v5.5.6\export.ps1"
. $idf | Out-Null
python -m serial.tools.miniterm --raw $Port 921600
