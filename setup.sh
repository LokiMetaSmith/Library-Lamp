#!/usr/bin/env bash

# We need to determine if this script is being sourced or executed.
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    echo "Warning: This script should be sourced, not executed directly."
    echo "Please run: source ${BASH_SOURCE[0]} or . ${BASH_SOURCE[0]}"
    IS_SOURCED=0
else
    IS_SOURCED=1
fi

echo "Setting up ESP32 Dev Environment for E-Book Librarian..."

export SDKCONFIG="sdkconfig.esp32-s3-ebook-librarian"
echo "Set SDKCONFIG=$SDKCONFIG"

if command -v idf.py &> /dev/null; then
    echo "ESP-IDF is already in PATH. Environment is ready."
    if [ "$IS_SOURCED" = "1" ]; then return 0; else exit 0; fi
fi

IDF_VERSION="v5.4.1"
DEFAULT_IDF_DIR="$HOME/esp/esp-idf"
IDF_EXPORT_SCRIPT=""

find_export_script() {
    if [ -f "$1/export.sh" ]; then
        echo "$1/export.sh"
        return 0
    fi
    return 1
}

# 1. Check if IDF_PATH is already set and has export.sh
if [ -n "$IDF_PATH" ] && find_export_script "$IDF_PATH" > /dev/null; then
    IDF_EXPORT_SCRIPT=$(find_export_script "$IDF_PATH")
    echo "Found ESP-IDF based on IDF_PATH: $IDF_EXPORT_SCRIPT"
# 2. Check the default ESP-IDF installation directory
elif find_export_script "$DEFAULT_IDF_DIR" > /dev/null; then
    IDF_EXPORT_SCRIPT=$(find_export_script "$DEFAULT_IDF_DIR")
    echo "Found ESP-IDF in default location: $IDF_EXPORT_SCRIPT"
# 3. Check PlatformIO's framework-espidf directory (from agents.md)
elif find_export_script "$HOME/.platformio/packages/framework-espidf" > /dev/null; then
    IDF_EXPORT_SCRIPT=$(find_export_script "$HOME/.platformio/packages/framework-espidf")
    echo "Found ESP-IDF in PlatformIO location: $IDF_EXPORT_SCRIPT"
fi

# If we found an export script, try sourcing it
if [ -n "$IDF_EXPORT_SCRIPT" ]; then
    echo "Sourcing ESP-IDF environment from: $IDF_EXPORT_SCRIPT"
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        . "$IDF_EXPORT_SCRIPT"
    else
        . "$IDF_EXPORT_SCRIPT"
    fi

    # Check if sourcing it actually worked (idf.py is in PATH)
    if ! command -v idf.py &> /dev/null; then
        echo "WARNING: Sourced $IDF_EXPORT_SCRIPT but idf.py is still not in PATH."
        echo "The existing ESP-IDF installation might be broken or missing the Python environment."
        echo "Falling back to fresh installation..."
        IDF_EXPORT_SCRIPT=""
    fi
fi

# If we didn't find an export script, or if the one we found was broken, do a fresh install
if [ -z "$IDF_EXPORT_SCRIPT" ]; then
    echo "Downloading and installing a fresh ESP-IDF $IDF_VERSION..."

    mkdir -p "$HOME/esp"

    # If the default dir already exists but is broken, we should remove it first or install over it.
    # We will try to run install.sh on it first if it's already a git repo.
    if [ -d "$DEFAULT_IDF_DIR/.git" ]; then
        echo "Directory $DEFAULT_IDF_DIR exists. Attempting to run install script on existing repository..."
    else
        git clone -b $IDF_VERSION --recursive https://github.com/espressif/esp-idf.git "$DEFAULT_IDF_DIR"
        if [ $? -ne 0 ]; then
            echo "Error: Failed to clone ESP-IDF repository."
            if [ "$IS_SOURCED" = "1" ]; then return 1; else exit 1; fi
        fi
    fi

    echo "Installing ESP-IDF tools..."
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        "$DEFAULT_IDF_DIR/install.bat"
    else
        "$DEFAULT_IDF_DIR/install.sh"
    fi

    if [ $? -ne 0 ]; then
        echo "Error: Failed to install ESP-IDF tools."
        if [ "$IS_SOURCED" = "1" ]; then return 1; else exit 1; fi
    fi

    IDF_EXPORT_SCRIPT="$DEFAULT_IDF_DIR/export.sh"

    # Source the newly installed script
    echo "Sourcing new ESP-IDF environment from: $IDF_EXPORT_SCRIPT"
    . "$IDF_EXPORT_SCRIPT"
fi

if [ "$IS_SOURCED" = "0" ]; then
    echo "========================================================================="
    echo "WARNING: You executed this script directly instead of sourcing it."
    echo "The environment variables (including idf.py) will NOT be available in your current terminal."
    echo "Please run this command to finish setup in your current terminal:"
    echo ""
    echo "    source ./setup.sh"
    echo ""
    echo "========================================================================="
else
    if command -v idf.py &> /dev/null; then
        echo "Environment setup complete! You can now use 'idf.py build' and other commands."
    else
        echo "Error: Setup failed. idf.py is not available."
    fi
fi
