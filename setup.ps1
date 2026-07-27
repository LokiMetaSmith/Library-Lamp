# We need to determine if this script is being sourced (dot-sourced) or executed.
# In PowerShell, $MyInvocation.InvocationName is '.' when dot-sourced.
$IsSourced = ($MyInvocation.InvocationName -eq '.')

if (-not $IsSourced) {
    Write-Host "Warning: This script should be dot-sourced, not executed directly." -ForegroundColor Yellow
    Write-Host "Please run: . .\setup.ps1" -ForegroundColor Yellow
}

Write-Host "Setting up ESP32 Dev Environment for E-Book Librarian..."

# Automatically set the required environment variable for this project
$env:SDKCONFIG = "sdkconfig.esp32-s3-ebook-librarian"
Write-Host "Set SDKCONFIG=$env:SDKCONFIG"

# Check if idf.py is already available in the current PATH
if (Get-Command idf.py -ErrorAction SilentlyContinue) {
    Write-Host "ESP-IDF is already in PATH. Environment is ready." -ForegroundColor Green
    if ($IsSourced) { return } else { exit }
}

# Define the expected ESP-IDF version
$IDF_VERSION = "v5.4.1"
$DEFAULT_IDF_DIR = Join-Path $HOME "esp\esp-idf"
$IDF_EXPORT_SCRIPT = ""

# Function to search for export.ps1 in a given directory
function Find-ExportScript {
    param([string]$Dir)
    $ScriptPath = Join-Path $Dir "export.ps1"
    if (Test-Path $ScriptPath) {
        return $ScriptPath
    }
    return $null
}

# 1. Check if IDF_PATH is already set and has export.ps1
if ($env:IDF_PATH) {
    $IDF_EXPORT_SCRIPT = Find-ExportScript $env:IDF_PATH
    if ($IDF_EXPORT_SCRIPT) {
        Write-Host "Found ESP-IDF based on IDF_PATH: $IDF_EXPORT_SCRIPT"
    }
}

# 2. Check the default ESP-IDF installation directory
if (-not $IDF_EXPORT_SCRIPT) {
    $IDF_EXPORT_SCRIPT = Find-ExportScript $DEFAULT_IDF_DIR
    if ($IDF_EXPORT_SCRIPT) {
        Write-Host "Found ESP-IDF in default location: $IDF_EXPORT_SCRIPT"
    }
}

# 3. Check PlatformIO's framework-espidf directory
if (-not $IDF_EXPORT_SCRIPT) {
    $PioDir = Join-Path $HOME ".platformio\packages\framework-espidf"
    $IDF_EXPORT_SCRIPT = Find-ExportScript $PioDir
    if ($IDF_EXPORT_SCRIPT) {
        Write-Host "Found ESP-IDF in PlatformIO location: $IDF_EXPORT_SCRIPT"
    }
}

# If we found an export script, try sourcing it
if ($IDF_EXPORT_SCRIPT) {
    Write-Host "Sourcing ESP-IDF environment from: $IDF_EXPORT_SCRIPT"
    . $IDF_EXPORT_SCRIPT

    # Check if sourcing it actually worked
    if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
        Write-Host "WARNING: Sourced $IDF_EXPORT_SCRIPT but idf.py is still not in PATH." -ForegroundColor Yellow
        Write-Host "The existing ESP-IDF installation might be broken. Falling back to fresh installation..." -ForegroundColor Yellow
        $IDF_EXPORT_SCRIPT = ""
    }
}

# If we didn't find an export script, or if the one we found was broken, do a fresh install
if (-not $IDF_EXPORT_SCRIPT) {
    Write-Host "Downloading and installing a fresh ESP-IDF $IDF_VERSION..."

    $EspDir = Join-Path $HOME "esp"
    if (-not (Test-Path $EspDir)) {
        New-Item -ItemType Directory -Path $EspDir | Out-Null
    }

    $GitDir = Join-Path $DEFAULT_IDF_DIR ".git"
    if (Test-Path $GitDir) {
        Write-Host "Directory $DEFAULT_IDF_DIR exists. Attempting to run install script on existing repository..."
    } else {
        # Clone ESP-IDF
        git clone -b $IDF_VERSION --recursive https://github.com/espressif/esp-idf.git $DEFAULT_IDF_DIR
        if ($LASTEXITCODE -ne 0) {
            Write-Host "Error: Failed to clone ESP-IDF repository." -ForegroundColor Red
            if ($IsSourced) { return } else { exit 1 }
        }
    }

    Write-Host "Installing ESP-IDF tools..."
    $InstallScript = Join-Path $DEFAULT_IDF_DIR "install.ps1"
    & $InstallScript

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: Failed to install ESP-IDF tools." -ForegroundColor Red
        if ($IsSourced) { return } else { exit 1 }
    }

    $IDF_EXPORT_SCRIPT = Join-Path $DEFAULT_IDF_DIR "export.ps1"

    # Source the newly installed script
    Write-Host "Sourcing new ESP-IDF environment from: $IDF_EXPORT_SCRIPT"
    . $IDF_EXPORT_SCRIPT
}

if (-not $IsSourced) {
    Write-Host "=========================================================================" -ForegroundColor Yellow
    Write-Host "WARNING: You executed this script directly instead of dot-sourcing it." -ForegroundColor Yellow
    Write-Host "The environment variables (including idf.py) will NOT be available in your current terminal." -ForegroundColor Yellow
    Write-Host "Please run this command to finish setup in your current terminal:" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "    . .\setup.ps1" -ForegroundColor Yellow
    Write-Host ""
    Write-Host "=========================================================================" -ForegroundColor Yellow
} else {
    if (Get-Command idf.py -ErrorAction SilentlyContinue) {
        Write-Host "Environment setup complete! You can now use 'idf.py build' and other commands." -ForegroundColor Green
    } else {
        Write-Host "Error: Setup failed. idf.py is not available." -ForegroundColor Red
    }
}
