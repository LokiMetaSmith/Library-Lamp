#!/usr/bin/env bash

# We need to determine if this script is being sourced or executed.
# This script must be sourced to properly set environment variables for the user.
# In bash, BASH_SOURCE[0] is the script name. If it equals $0, it's executed directly.
if [ "${BASH_SOURCE[0]}" = "${0}" ]; then
    echo "Warning: This script should be sourced, not executed directly."
    echo "Please run: source ${BASH_SOURCE[0]} or . ${BASH_SOURCE[0]}"
    IS_SOURCED=0
else
    IS_SOURCED=1
fi

echo "Setting up ESP32 Dev Environment for E-Book Librarian..."

# Automatically set the required environment variable for this project
export SDKCONFIG="sdkconfig.esp32-s3-ebook-librarian"
echo "Set SDKCONFIG=$SDKCONFIG"

# Check if idf.py is already available in the current PATH
if command -v idf.py &> /dev/null; then
    echo "ESP-IDF is already in PATH. Environment is ready."
    if [ "$IS_SOURCED" = "1" ]; then return 0; else exit 0; fi
fi

# Define the expected ESP-IDF version
IDF_VERSION="v5.4.1"
DEFAULT_IDF_DIR="$HOME/esp/esp-idf"
IDF_EXPORT_SCRIPT=""

# Function to search for export.sh in a given directory
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

# If we still haven't found it, we need to download and install it
if [ -z "$IDF_EXPORT_SCRIPT" ]; then
    echo "ESP-IDF not found. Downloading ESP-IDF $IDF_VERSION..."

    # Create the esp directory if it doesn't exist
    mkdir -p "$HOME/esp"

    # Clone ESP-IDF
    git clone -b $IDF_VERSION --recursive https://github.com/espressif/esp-idf.git "$DEFAULT_IDF_DIR"

    if [ $? -ne 0 ]; then
        echo "Error: Failed to clone ESP-IDF repository."
        if [ "$IS_SOURCED" = "1" ]; then return 1; else exit 1; fi
    fi

    echo "Installing ESP-IDF tools..."
    # Support for Windows (Git Bash) and Linux
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
fi

# Finally, source the export script
if [ -f "$IDF_EXPORT_SCRIPT" ]; then
    echo "Sourcing ESP-IDF environment from: $IDF_EXPORT_SCRIPT"
    if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
        # In Windows Git Bash, we might need to source export.bat or just run the sh version.
        # Usually export.sh works in Git Bash, but let's be explicit
        . "$IDF_EXPORT_SCRIPT"
    else
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
        echo "Environment setup complete! You can now use 'idf.py build' and other commands."
    fi
else
    echo "Error: Could not find or access $IDF_EXPORT_SCRIPT."
    if [ "$IS_SOURCED" = "1" ]; then return 1; else exit 1; fi
fi
