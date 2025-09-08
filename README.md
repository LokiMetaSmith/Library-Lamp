# Library-Lamp
This project creates a digital library that lives in a book. It sits on your shelf and when you interact with it, the world of literature opens up for you. 


# 📖 E-Book Librarian

The E-Book Librarian is a standalone hardware device designed to create a physical, shareable library of public domain e-books. It allows users to easily connect their e-readers (like Kindle, Kobo, or BOOX devices) via USB and transfer books to and from a local library stored on an SD card.

The device hosts its own Wi-Fi network and provides a simple web interface, allowing anyone with a smartphone or computer to manage the library without needing any special software.

## ✨ Core Features

- **USB Host for E-Readers:** Automatically detects and mounts the storage of any USB Mass Storage compatible e-reader.
- **Local Library Storage:** Uses a MicroSD card to hold a large collection of e-books.
- **Simple Web Interface:** Provides an intuitive, browser-based UI for transferring files between the SD card and the connected e-reader.
- **Wi-Fi Access Point:** Creates its own Wi-Fi network, making it fully portable and operational without an internet connection.
- **Visual Status Indicator:** An onboard RGB LED strip shows the system's current state (idle, connected, transferring).

## 🛠️ Hardware & Wiring

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
    Connect your ESP32-S3 board and run the following command to build the project, upload it to the device, and view the serial output:
    ```bash
    idf.py build flash monitor
    ```

## 💻 Usage

### Normal Mode (E-Reader Library)

1.  **Power On:** Power the ESP32-S3 board using a reliable 5V power supply. The LED strip will light up with a pulsing blue light, indicating it's ready.
2.  **Connect to Wi-Fi:** On your phone or computer, connect to the Wi-Fi network with the SSID `Ebook-Library-Box-Setup`. There is no password.
3.  **Open the Web Interface:** Open a web browser and navigate to `http://192.168.4.1`. This will either show you a Wi-Fi setup page (on first boot) or the main library interface.
4.  **Connect Your E-Reader:** Plug your e-reader into the ESP32-S3's USB OTG port. The LED strip will turn solid green, and the web interface will update to show the files on your device.
5.  **Transfer Books:** Select a book from either the library or your e-reader and use the buttons to copy it to the other device. The LED will pulse white during the transfer.

### PC Connection Mode (USB Drive)

In addition to the normal e-reader library mode, the device can be started in a special "PC Connection Mode". In this mode, the device acts as a standard USB Mass Storage Device (like a flash drive), allowing you to easily manage the SD card's contents directly from your computer.

**To start in PC Connection Mode:**
1.  **Unplug the device.**
2.  **Press and hold the `MENU` button** on the ESP32-S3-USB-OTG board.
3.  **While holding the button, plug the device into your PC** using the `USB_DEV` port (the male USB-A connector).
4.  The LED strip will turn solid green, and your computer should recognize the device as a new USB drive.
5.  You can now drag and drop files to and from the SD card.
6.  To return to the normal e-reader library mode, simply unplug the device and plug it back in without holding the `MENU` button.


Here's the breakdown:

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

If you want to create your own custom book cover, you can use the following workflow:

1.  **Generate Lithophanes:** Use the open-source [LithoMaker](https://github.com/muldjord/lithomaker) tool to convert your images into 3D-printable lithophane STL files. Follow the instructions on the GitHub page for preparing your images and generating the models.

2.  **Assemble the Enclosure:** Use a 3D modeling tool like [OpenSCAD](https://openscad.org/) to "stitch" the generated lithophane panels together into a book-shaped enclosure. This gives you full control over the final design.

3.  **Print and Assemble:** Print the parts and assemble your custom E-Book Librarian!
