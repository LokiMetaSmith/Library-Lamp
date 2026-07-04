Write-Host "==============================================="
Write-Host " E-Book Librarian - Firmware Installer"
Write-Host "==============================================="

# Check if idf.py is available
$idfCmd = Get-Command idf.py -ErrorAction SilentlyContinue

if (-not $idfCmd) {
    Write-Error "Error: idf.py could not be found."
    Write-Host "Please ensure you have set up the ESP-IDF environment."
    Write-Host "Run '%userprofile%\esp\esp-idf\export.ps1' or use the VS Code extension terminal."
    Exit 1
}

Write-Host "Building firmware..."
idf.py build

Write-Host "Flashing SPIFFS partition (web assets)..."
idf.py storage-flash

Write-Host "Flashing main firmware and opening monitor..."
idf.py flash monitor
