# Library-Lamp
This project creates a digital library that lives in a book. It sits on your shelf and when you interact with it, the world of literature opens up for you. 


# 📖 E-Book Librarian

The E-Book Librarian is a standalone hardware device designed to create a physical, shareable library of public domain e-books. It allows users to easily connect their e-readers (like Kindle, Kobo, or BOOX devices) via USB and transfer books to and from a local library stored on an SD card.

The device hosts its own Wi-Fi network and provides a simple web interface, allowing anyone with a smartphone or computer to manage the library without needing any special software.

## ✨ Core Features

- **USB Host for E-Readers:** Automatically detects and mounts the storage of any USB Mass Storage compatible e-reader.
- **Local Library Storage:** Uses a MicroSD card to hold a large collection of e-books.
- **Simple Web Interface:** Provides an intuitive, browser-based UI for transferring files between the SD card and the connected e-reader.
- **Direct Wi-Fi Download:** E-readers connecting directly to the device's Wi-Fi network are automatically served a lightweight, e-reader-friendly interface that allows them to download files wirelessly, without needing a USB connection.
- **Two-Step Transfer Confirmation:** Pushing files via USB requires physical confirmation (pressing the device button) to prevent accidental or unauthorized transfers.
- **Wi-Fi Access Point:** Creates its own Wi-Fi network, making it fully portable and operational without an internet connection.
- **Visual Status Indicator:** An onboard RGB LED strip shows the system's current state (idle, connected, waiting for confirmation, transferring).
- **Manual Sleep Mode:** A "shipping mode" can be activated from the web interface to put the device into deep sleep, conserving battery for long periods. A manual reset is required to wake the device.
- **Physical Eject/Sleep Button:** A single button provides three functions: confirming a pending USB transfer, safely ejecting the connected USB device (short press), and putting the device into deep sleep (long press).

All the necessary components to build this project are listed in the [Bill of Materials (BOM.md)](BOM.md).

The wiring connections between the ESP32-S3, SD card module, and LED strip are detailed in the [WireViz Diagram Source (wiring.yml)](wiring.yml). You can render this file into a visual diagram using the [WireViz](https://github.com/wireviz/WireViz) tool.

## 🚀 Setup and Compilation

This project is built using the **Espressif IoT Development Framework (ESP-IDF)**. The recommended way to set up the build environment is with the [VS Code ESP-IDF Extension](https://github.com/espressif/vscode-esp-idf-extension), which handles the toolchain installation.

### Steps:

1.  **Clone the Repository:**
    ```bash
    git clone <your-repo-url>
    cd <your-repo-folder>
    ```

2.  **Set up the ESP-IDF Environment:**
    Follow the official [ESP-IDF Get Started Guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) or use the VS Code extension to set up your toolchain.

3.  **Configure the Project:**
    Open a terminal in the project root and run the configuration tool:
    ```bash
    idf.py menuconfig
    ```
    You must enable support for large SD cards (exFAT):
    - Navigate to `Component config` ---> `FAT Filesystem support`
    - Check the box for `[*] Enable exFAT`
    - Save and exit.

4.  **Build, Flash, and Monitor:**
    Connect your ESP32-S3 board. Because the web UI is stored on a separate filesystem (SPIFFS), you must flash both the application and the `storage` partition.

    You can use the provided script on Linux/macOS:
    ```bash
    ./install_firmware.sh
    ```

    Or run the commands manually (required on Windows):
    ```bash
    idf.py build storage-flash flash monitor
    ```

## 💻 Usage

1.  **Power On:** Power the ESP32-S3 board using a reliable 5V power supply. The LED strip will light up with a pulsing blue light, indicating it's ready.
2.  **Connect to Wi-Fi:** On your phone, computer, or e-reader, connect to the Wi-Fi network with the SSID `Ebook-Library-Box-Setup`. There is no password.
3.  **Open the Web Interface:** Open a web browser and navigate to `http://192.168.4.1`.
    - If you are on an e-reader, you will be automatically redirected to a simple list of available e-books that you can download directly over Wi-Fi.
    - If you are on a phone or computer, you will see the main library interface (or the Wi-Fi setup page on first boot).
4.  **Connect Your E-Reader (USB Method):** Plug your e-reader into the ESP32-S3's USB OTG port. The LED strip will turn solid green, and the web interface will update to show the files on your device.
5.  **Transfer Books (USB Method):** Select a book from either the library or your e-reader and use the buttons to copy it to the other device.
    - **Confirmation Required:** The UI will prompt you, and the LED strip will begin flashing yellow. You must physically press the `EJECT/SLEEP` button on the ESP32-S3 to authorize the transfer.
    - The LED will pulse white during the transfer.
6.  **Enter Sleep Mode (Optional):** From the web interface, you can click the "Enter Sleep Mode" button. This will put the device into a very low power deep sleep state. **To wake the device, you must press the physical `RESET` button on the board.**
7.  **Button Functions:** You can use the physical button for three actions:
    - **Confirm Transfer:** When a transfer is initiated via the web UI, a short press confirms and starts the transfer.
    - **Short Press (Idle):** Safely unmounts the connected e-reader. The LED will flash green, indicating it's safe to unplug the USB cable.
    - **Long Press (2-3 seconds):** Puts the device into deep sleep mode. You must press the `RESET` button to wake it.

1.  **The Technology (SDHC vs. SDXC):**
    * SD cards up to **32GB** are typically **SDHC** (Secure Digital High Capacity) and are formatted with the **FAT32** filesystem.
    * SD cards larger than 32GB (e.g., 64GB, 128GB) are **SDXC** (Secure Digital eXtended Capacity) and are formatted with the **exFAT** filesystem by default.

2.  **ESP-IDF Filesystem Support:**
    The `esp_vfs_fat` component used in our `main.c` code is a wrapper around the robust `FatFs` library. By default, the ESP-IDF project configuration only enables support for FAT16/FAT32.

3.  **The Solution (Enabling exFAT):**
    To make it work with larger cards, you simply need to enable exFAT support in your project's configuration. You can do this using the `menuconfig` tool:

    * Open a terminal in your project directory.
    * Run the command: `idf.py menuconfig`
    * Navigate to: `Component config` ---> `FAT Filesystem support`
    * Check the box for `Enable exFAT`



After you enable that option, save the configuration and rebuild your project (`idf.py build`). The same `main.c` code will now be able to mount and use SDXC cards formatted with exFAT without any changes.

## 🏠 Enclosure

A 3D-printable enclosure for this project can be found on Printables:
- **Model:** [Lithophane Books (Harry Potter Book 3)](https://www.printables.com/model/914425-lithophane-books-harry-potter-book-3)
- **Author:** [MeasureOnce](https://www.printables.com/@MeasureOnce)

### Printing Instructions:
- **Supports:** No
- **Wall line count:** 6
- **Nozzle:** 0.2-0.4mm
- **Speed:** Slow
- **Book Filament:** Filamentum Light Ivory (or similar)
- **Page Filament:** Any white filament
- **Light Source:** A small LED or flashlight that fits inside the case.

## 🎨 Creating a Custom Enclosure

To create a personalized 3D-printable enclosure with your own cover image, this repository includes a unified build script that automates most of the process.

### Automated Build Script

The `./build_enclosure.sh` script is the recommended way to generate all the necessary files for a custom enclosure. It uses OpenSCAD and Blender to construct the final models.

**Prerequisites:**
- **OpenSCAD:** Install from [openscad.org](https://openscad.org/).
- **Blender:** Install from [blender.org](https://www.blender.org/).
- **LithoMaker:** Download from [github.com/muldjord/lithomaker](https://github.com/muldjord/lithomaker).

Ensure that `openscad` and `blender` are available in your system's PATH.

**Workflow:**
The script combines automated and manual steps to generate the final 3D models.

1.  **Run the Script:**
    Open a terminal and run the script, providing the path to your desired cover image:
    ```bash
    ./build_enclosure.sh -i /path/to/your/image.png
    ```

2.  **Manual Lithophane Generation (User Step):**
    The script will pause and prompt you to create the initial flat lithophane. This is a required manual step because `lithomaker` is a GUI application.
    - The script will provide you with precise instructions.
    - You will use the `lithomaker` application to load your image and save the resulting STL file as `build_enclosure/flat_litho.stl`.

3.  **Automated Processing:**
    Once you press Enter to continue, the script will automatically:
    - Bend the flat lithophane to create the book's spine using Blender.
    - Generate the main enclosure components using OpenSCAD.
    - Produce two final files in the `build_enclosure/` directory:
        - `printable_parts.stl`: All components laid out flat for printing.
        - `final_assembly.stl`: A complete, assembled view of the final product.

This streamlined process simplifies the creation of custom enclosures while still giving you control over the initial lithophane design.
