# JTAG Debugger and Hardware Validation

Building a JTAG debugger allows for real-time hardware validation of firmware logic (like MTU limits, memory safety, and thread synchronization) directly on the ESP32-S3 without relying on mocked headers or software emulation.

## Requirements
*   **Hardware:** An ESP-Prog board or a compatible FT2232H-based JTAG adapter.
*   **Software:** OpenOCD (Open On-Chip Debugger) and GDB (GNU Debugger) configured for the Xtensa architecture (provided by ESP-IDF).

## Wiring the JTAG Adapter to the ESP32-S3
Connect the ESP-Prog to the ESP32-S3 using the following pinout:

| JTAG Signal | ESP-Prog Pin | ESP32-S3 Pin |
| :--- | :--- | :--- |
| **TMS** | TMS | GPIO 42 |
| **TDI** | TDI | GPIO 41 |
| **TCK** | TCK | GPIO 39 |
| **TDO** | TDO | GPIO 40 |
| **GND** | GND | GND |

*Note: Ensure the target board is independently powered, or supply 3.3V from the ESP-Prog if the board does not draw excessive current.*

## Running the Debugger

1.  **Launch OpenOCD:**
    In a terminal, navigate to the ESP-IDF directory and start OpenOCD with the configuration for the ESP32-S3 and the ESP-Prog adapter.
    ```bash
    openocd -f board/esp32s3-builtin.cfg
    ```
    *If using an external ESP-Prog, you may need to specify the interface:*
    ```bash
    openocd -f interface/ftdi/esp32_devkitj_v1.cfg -f target/esp32s3.cfg
    ```

2.  **Connect GDB:**
    Open a new terminal, activate the ESP-IDF environment, and start GDB.
    ```bash
    xtensa-esp32s3-elf-gdb build/E_Book_Librarian.elf
    ```

3.  **Execute Validation Scripts:**
    From within the GDB prompt, connect to OpenOCD and load the firmware.
    ```gdb
    target extended-remote :3333
    monitor reset halt
    load
    ```
    You can now set breakpoints (e.g., `break rns_encode_packet`) and step through the code to validate logic directly on the silicon.
