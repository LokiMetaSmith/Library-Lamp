#!/usr/bin/env bash
set -e

echo "Setting up ESP32 Dev Environment and building firmware..."

# Check if idf.py is in the PATH
if ! command -v idf.py &> /dev/null; then
    echo "Error: idf.py could not be found."
    echo "Please ensure you have set up the ESP-IDF environment."
    echo "Run '. <path-to-esp-idf>/export.sh' or use the VS Code extension terminal."
    exit 1
fi

echo "Building firmware with ESP-IDF..."

# Set the target to esp32s3 to ensure dependencies and proper compilation for the USB OTG board
idf.py set-target esp32s3

# Ensure we use the correct custom sdkconfig for the ESP32-S3 custom board
export SDKCONFIG="sdkconfig.esp32-s3-ebook-librarian"

idf.py build

if [ $? -ne 0 ]; then
    echo "Firmware compilation failed!"
    exit 1
else
    echo "Firmware compiled successfully!"
fi
