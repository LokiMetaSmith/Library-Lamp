# Project TODO & Future Enhancements

This file tracks planned features and improvements for the E-Book Librarian project.

### High Priority

* [x] **Web UI Enhancements:**
  * [x] Implement a real-time progress bar for file transfers using WebSockets.
  * [x] Add a "cancel transfer" button.
  * [x] Improve the mobile/responsive layout of the web interface.
* [x] **Robust Error Handling:**
  * [x] Display more specific error messages on the web UI (e.g., "Not enough space on device," "File already exists," "Transfer failed").
  * [x] Make the LED strip turn red on a critical error (e.g., SD card failed to mount).
* [x] **Refactor Web Assets:**
  * [x] Move the embedded HTML, CSS, and JavaScript from `main.c` into separate files.
  * [x] Serve these static assets from a SPIFFS or FAT partition on the ESP32's flash.

### Medium Priority

* [x] **E-Book Metadata Parsing:**
  * [x] Integrate a lightweight library (like `libzip`) to read metadata from `.epub` files.
  * [x] Display the book's Title and Author in the file lists instead of just the filename.
* [x] **Calibre DB Auto-Import:**
  * [x] Automatically import all books from a USB drive containing a Calibre `metadata.db` file.
* [x] **Support for More File Types:**
  * [x] Add file filters for comic book formats (`.cbr`, `.cbz`).
* [x] **USB Device Mode:**
  * [x] Implement a USB Mass Storage device mode, allowing the device to act as a flash drive when plugged into a computer.
* [x] **Alternative Interface:**
  * [x] Implement a Bluetooth Low Energy (BLE) service to allow device provisioning.
* [ ] **Unified Enclosure Build Script:**
  * [ ] Create a master script to orchestrate the lithophane generation, OpenSCAD modeling, and Blender mesh bending.

### Low Priority

* [x] **Hardware & Enclosure:**
  * [x] Design a 3D-printable case for the project.
  * [x] Provide an example OpenSCAD script for custom enclosures.
  * [x] Add a physical button to safely eject the USB device or shut down the system.
* [x] **Power Management:**
  * [x] Implement manual deep sleep mode ("shipping mode") to save power.
* [x] **Firmware Updates:**
  * [x] Add support for Over-the-Air (OTA) firmware updates to easily deploy new versions.
