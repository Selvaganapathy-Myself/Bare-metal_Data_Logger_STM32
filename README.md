# Bare-Metal Data-Logger

This repository contains a bare-metal motion-logging system built on an STM32F446RE ARM Cortex-M4 microcontroller. It captures raw 16-bit gravitational data from an MPU6050 sensor via I2C and permanently logs it into a W25Q128 flash memory chip via SPI.

## 1. The Core Architecture
This system utilizes a strict Master-Slave architecture operating over two separate communication buses:
* **The Master (STM32 MCU):** Controls the system clock and initiates all communication.
* **The Observer (MPU6050 IMU):** An I2C slave that measures physical acceleration and translates it into a digital 16-bit signed integer.
* **The Vault (W25Q128 Flash):** An SPI slave providing non-volatile silicon storage to retain data after power loss.

---

## 2.I2C and Open-Drain Logic (The Sensor)
The MPU6050 communicates via I2C, a two-wire (SCL, SDA), half-duplex protocol. 

### Open-Drain vs. Bus Contention
To prevent two chips from simultaneously pushing high (3.3V) and low (0V) voltages onto the shared data line—which would cause a dead short circuit (Bus Contention)—I2C uses **Open-Drain logic**. Chips can only pull the line down to 0V. To transmit a `1`, the chip simply lets go of the wire, allowing external pull-up resistors to float the voltage back to 3.3V.

### The Bare-Metal Transaction Flow
To read the sensor, the MCU manually manipulates the I2C registers (`CR1`, `DR`, `SR1`) to generate this exact waveform:
1. **START:** MCU pulls SDA low while SCL is high.
2. **Address Phase:** MCU transmits the 7-bit sensor address (`0x68`) shifted left by 1 bit, appending a Write bit (`0`). The final byte sent is `0xD0`.
3. **ACK:** The sensor acknowledges by pulling SDA low.
4. **Register Select:** MCU sends the internal sensor register address (e.g., `0x3B` for X-Axis High Byte).
5. **REPEATED START:** MCU reverses the bus direction without releasing it, resending the address with a Read bit (`1`).
6. **Data Read:** MCU clocks in the sensor's data byte and generates a **STOP** condition.

---

## 3. SPI and Push-Pull Logic (The Memory)
Flash memory requires high-speed data transfer, making I2C too slow. The W25Q128 uses SPI, a 4-wire, full-duplex protocol utilizing **Push-Pull logic** (actively forcing lines to 3.3V and 0V).

### The "Revolving Door" Shift Register
SPI does not have independent read and write commands; it relies on a simultaneous exchange. Every time the MCU pushes 8 bits out on the MOSI line, the slave chip pushes 8 bits back on the MISO line. 
* **The Dummy Byte** If the MCU only wants to read data (like pulling a logged gravity number), it still must generate the clock signal. It does this by transmitting a "Dummy Byte" (`0x00`) on MOSI, which forces the clock to tick 8 times, allowing the Flash chip to push the requested data back on MISO.

---

## 4. Silicon-Level Memory Management
Writing to raw flash memory requires managing physical silicon states. A blank flash memory bit defaults to a state of `1` (`0xFF`).

### The Golden Rule: Erase Before Write
When writing to flash memory, you can only change `1`s into `0`s. To turn a `0` back into a `1`, you must dump a massive electrical charge to wipe the entire sector clean.
1. **Write Enable:** Send command `0x06` to unlock the silicon.
2. **Sector Erase:** Send command `0x20` followed by the 24-bit memory address.
3. **Wait State:** The MCU must poll the flash chip's internal Status Register in a `while` loop until the "Busy" flag drops to `0`.

### Data Serialization (16-bit to 8-bit)
The MPU6050 generates gravity data as **16-bit integers**, but the W25Q128 memory only accepts data in **8-bit chunks**. 
* The STM32 splits the 16-bit data into a High Byte and a Low Byte.
* The High Byte is written to address `0x000000`, and the Low Byte to `0x000001`.
* Upon reading the data back, the MCU reconstructs the 16-bit integer using bitwise operations: `(High_Byte << 8) | Low_Byte`.
