# Library-Lamp
This project creates a digital library that lives in a book. It sits on your shelf and when you interact with it, the world of literature opens up for you. 


Notes:
That's a very important and practical question.

**Yes, this approach absolutely allows for SD cards larger than 32GB, but it requires one small configuration change in the project.**

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
