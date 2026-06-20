#!/usr/bin/env bash
set -e

echo "==============================================="
echo " E-Book Librarian - Firmware Installer"
echo "==============================================="

# Check if idf.py is in the PATH
if ! command -v idf.py &> /dev/null; then
    echo "Error: idf.py could not be found."
    echo "Please ensure you have set up the ESP-IDF environment."
    echo "Run '. <path-to-esp-idf>/export.sh' or use the VS Code extension terminal."
    exit 1
fi

echo "Setting target to ESP32-S3..."
idf.py set-target esp32s3

export SDKCONFIG="sdkconfig.esp32-s3-ebook-librarian"
echo "Using custom configuration: $SDKCONFIG"

echo "Building firmware..."
idf.py build

echo "Flashing SPIFFS partition (web assets)..."
idf.py storage-flash

echo "Flashing main firmware and opening monitor..."
idf.py flash monitor
