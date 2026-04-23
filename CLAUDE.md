# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Arduino firmware for a 4-channel load cell data acquisition system. It reads four HX711 24-bit ADC modules connected to load cells and streams weight data over serial at 115200 baud.

## Build & Upload

This project uses the Arduino toolchain — there is no Makefile or package.json.

- **Arduino IDE**: Open `LBV1.ino`, select "Arduino Nano" as board, compile and upload.
- **Arduino CLI**:
  ```
  arduino-cli compile --fqbn arduino:avr:nano LBV1
  arduino-cli upload --fqbn arduino:avr:nano -p <PORT> LBV1
  ```

The only non-built-in dependency is the **HX711_ADC** library (v0.1+), which must be installed via Arduino Library Manager before compiling.

## Hardware Pinout

| Signal     | Pin  |
|------------|------|
| LC1 DOUT   | A2   |
| LC1 SCK    | A3   |
| LC2 DOUT   | 4    |
| LC2 SCK    | 7    |
| LC3 DOUT   | A5   |
| LC3 SCK    | A4   |
| LC4 DOUT   | 10   |
| LC4 SCK    | 9    |
| RATE_pin   | 2    |
| LED0       | 3    |
| LED1       | 5    |
| LED2       | 6    |

RATE_pin is driven HIGH on startup to set 80 SPS sampling rate on all HX711 modules.

## Serial Protocol

**Baud rate**: 115200

**Output** (continuous): `value1|value2|value3|value4$`  
Values are pipe-delimited floats, terminated by `$`. Readings are negated (`* -1`) to account for inverted load cell mounting orientation.

**Input commands** (sent via Serial Monitor):

| Key | Action |
|-----|--------|
| `t` | Tare all load cells |
| `q` / `a` | Increase / decrease LC1 calibration factor by 10 |
| `w` / `s` | Increase / decrease LC2 calibration factor by 10 |
| `e` / `d` | Increase / decrease LC3 calibration factor by 10 |
| `r` / `f` | Increase / decrease LC4 calibration factor by 10 |

## Calibration & EEPROM

Calibration factors are stored as 4-byte floats at EEPROM offsets 0, 4, 8, 12 (one per load cell). Default value is `14000.0`, typical for YZC-133 3 kg load cells.

Calibration factors can be tuned live via the serial commands above. To persist changes, EEPROM writes must be triggered manually — the current code loads from EEPROM on boot but does not auto-save adjustments.

## Architecture

The entire firmware is a single file (`LBV1.ino`):

- **`setup()`**: Configures pins, reads calibration from EEPROM, initializes all four HX711 objects via `startMultiple()`, waits for stabilization, then runs tare.
- **`loop()`**: Polls all four HX711s, prints the pipe-delimited output, and checks for incoming serial calibration commands.

Initialization uses a byte-flag state machine (`LC1_rdy` … `LC4_rdy`) to detect when all modules have finished their startup sequence before proceeding.
