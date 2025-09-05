# Bill of Materials (BOM)

This document lists the necessary hardware components to build the E-Book Librarian project.

### Core Components

* **ESP32-S3 Development Board with USB OTG**
    * **Description:** The main microcontroller for the project. It is essential to choose a model that has a USB-C or Micro-USB port specifically for OTG/Host functionality.
    * **Notes / Example Link:** The official [ESP32-S3-USB-OTG board](https://www.espressif.com/en/products/devkits/esp32-s3-usb-otg) is ideal, but other S3 boards with OTG support will work.

* **MicroSD Card Module**
    * **Description:** A standard SPI-based MicroSD card reader to store the main e-book library.
    * **Notes / Example Link:** [HiLetgo MicroSD Card Adapter](https://www.amazon.com/HiLetgo-Adater-Interface-Conversion-Arduino/dp/B07BJ2P6X6/). Most generic modules are compatible.

* **MicroSD Card**
    * **Description:** The storage for your book library. An SDXC card (64GB or larger, formatted as exFAT) is recommended for larger collections.
    * **Notes / Example Link:** Any reputable brand will work. A Class 10 card is sufficient.

* **WS2812B / NeoPixel RGB LED Strip**
    * **Description:** An addressable RGB LED strip used for the visual status indicator.
    * **Notes / Example Link:** A short strip with 8-16 LEDs is plenty. Can be found on [Amazon](https://www.amazon.com/s?k=ws2812b+strip) or Adafruit.

### Optional / Miscellaneous

* **Breadboard and Jumper Wires**
    * **Description:** For prototyping the connections before creating a permanent enclosure.
    * **Notes / Example Link:** A standard electronics prototyping kit.

* **5V Power Supply**
    * **Description:** A reliable power source that can provide at least 2A to power the ESP32-S3, SD card module, and the LED strip simultaneously.
    * **Notes / Example Link:** A good quality USB wall adapter is usually sufficient.

* **USB OTG Adapter/Cable**
    * **Description:** If your ESP32-S3 board has a USB-C port, you will need a USB-C to USB-A female adapter to plug in the e-reader's cable.
    * **Notes / Example Link:** Widely available on sites like Amazon.

* **3D Printed Enclosure**
    * **Description:** A custom-designed, book-shaped enclosure to house the electronics.
    * **Notes / Example Link:** The 3D model can be downloaded from [Printables](https://www.printables.com/model/914425-lithophane-books-harry-potter-book-3). You will need access to a 3D printer and filament.
