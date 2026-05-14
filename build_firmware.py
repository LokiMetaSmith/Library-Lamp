import os
import sys
import subprocess
import venv
from pathlib import Path

VENV_DIR = Path(".venv")

def get_venv_python():
    """Return the path to the python executable in the virtual environment."""
    if os.name == 'nt':
        return VENV_DIR / "Scripts" / "python.exe"
    else:
        return VENV_DIR / "bin" / "python"

def get_venv_pio():
    """Return the path to the platformio executable in the virtual environment."""
    if os.name == 'nt':
        return VENV_DIR / "Scripts" / "platformio.exe"
    else:
        return VENV_DIR / "bin" / "platformio"

def setup_environment():
    """Create venv and install platformio if needed."""
    if not VENV_DIR.exists():
        print(f"Creating virtual environment in {VENV_DIR}...")
        venv.create(VENV_DIR, with_pip=True)

    python_exe = get_venv_python()
    if not python_exe.exists():
        print("Virtual environment python not found!")
        sys.exit(1)

    print("Ensuring platformio is installed in the virtual environment...")
    subprocess.check_call([str(python_exe), "-m", "pip", "install", "platformio", "-q"])

def build_firmware():
    """Run platformio build."""
    # Set environment variables for the ESP32 dev environment / platformio
    env = os.environ.copy()

    # We add the venv bin/Scripts directory to PATH so pio commands find dependencies
    venv_bin = VENV_DIR / "Scripts" if os.name == 'nt' else VENV_DIR / "bin"
    env["PATH"] = f"{venv_bin.absolute()}{os.pathsep}{env.get('PATH', '')}"

    # PlatformIO manages the ESP-IDF toolchain automatically, but we can set specific variables
    # if required. By default, it uses ~/.platformio or %USERPROFILE%\.platformio.
    # We can enforce a local core dir to be completely self-contained, but it's optional:
    # env["PLATFORMIO_CORE_DIR"] = str((Path.cwd() / ".pio" / "core").absolute())

    pio_exe = get_venv_pio()
    print("Building firmware with PlatformIO...")
    # Run PlatformIO build
    result = subprocess.run([str(pio_exe), "run"], env=env)

    if result.returncode != 0:
        print("Firmware compilation failed!")
        sys.exit(result.returncode)
    else:
        print("Firmware compiled successfully!")

if __name__ == "__main__":
    print("Setting up ESP32 Dev Environment and PlatformIO...")
    setup_environment()
    build_firmware()
