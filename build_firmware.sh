#!/usr/bin/env bash
set -e

VENV_DIR=".venv"

echo "Setting up ESP32 Dev Environment and PlatformIO..."

# Create virtual environment if it doesn't exist
if [ ! -d "$VENV_DIR" ]; then
    echo "Creating virtual environment in $VENV_DIR..."
    python3 -m venv "$VENV_DIR"
fi

# Ensure virtual environment python exists
if [ ! -f "$VENV_DIR/bin/python" ] && [ ! -f "$VENV_DIR/Scripts/python" ]; then
    echo "Virtual environment python not found!"
    exit 1
fi

echo "Ensuring platformio is installed in the virtual environment..."
if [ -f "$VENV_DIR/bin/python" ]; then
    "$VENV_DIR/bin/python" -m pip install platformio -q
else
    "$VENV_DIR/Scripts/python" -m pip install platformio -q
fi

echo "Building firmware with PlatformIO..."

# Set path to include venv binaries
if [ -d "$VENV_DIR/bin" ]; then
    export PATH="$PWD/$VENV_DIR/bin:$PATH"
else
    export PATH="$PWD/$VENV_DIR/Scripts:$PATH"
fi

platformio run

if [ $? -ne 0 ]; then
    echo "Firmware compilation failed!"
    exit 1
else
    echo "Firmware compiled successfully!"
fi
