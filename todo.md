# Project TODO & Future Enhancements

This file tracks planned features and improvements for the E-Book Librarian project.

### High Priority

* [x] **Web UI Enhancements:**
  * [x] Implement a real-time progress bar for file transfers (likely requires WebSockets).
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
* [ ] **Support for More File Types:**
  * [ ] Add file filters for comic book formats (`.cbr`, `.cbz`).
* [ ] **Alternative Interface:**
  * [x] Implement a Bluetooth Low Energy (BLE) service to allow device provisioning.
* [ ] **Calibre DB Auto-Import:**
  * [ ] Automatically import all books from a USB drive containing a Calibre `metadata.db` file.
  * [ ] **NOTE:** This is currently blocked by the inability to download the required SQLite library into the development environment.

### Low Priority

* [x] **Hardware & Enclosure:**
  * [x] Design a 3D-printable case for the project.
  * [ ] Provide an example OpenSCAD script for custom enclosures.
  * [ ] Add a physical button to safely eject the USB device or shut down the system.
* [ ] **Power Management:**
  * [ ] Implement light or deep sleep modes to save power when the device is idle.
* [ ] **Firmware Updates:**
  * [ ] Add support for Over-the-Air (OTA) firmware updates to easily deploy new versions.
