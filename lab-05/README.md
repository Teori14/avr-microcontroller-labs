# Lab 05 · I²C peripheral integration

Standalone C programs use the AVR TWI peripheral to read and write PCA9555 registers, adding digital inputs and outputs through a shared bus.

| Source | Application | Data path |
| :--- | :--- | :--- |
| [5_1.c](5_1.c) | Evaluate Boolean functions from four buttons | PB0..PB3 → logic evaluation → expander outputs |
| [5_2.c](5_2.c) | Read one keypad row and select an LED | Expander column inputs → key decoding → LED mask |
| [5_3.c](5_3.c) | Display a two-line name | TWI → PCA9555 → 4-bit LCD |

## Bus transactions

The code includes start, repeated-start, write, read and stop routines. A register read first selects the register, then uses a repeated start to receive its contents. Direction and polarity registers configure the ports.

The `0x40` address constant uses the transmitted address-byte convention, with the read/write bit added separately. Tools expecting a seven-bit device address use a different representation.

## Hardware review

Verify I²C pull-ups and keypad-column biasing against the board schematic and device documentation. The source comment in 5.2 assumes expander pull-ups and needs confirmation. Polling waits have no timeout, so bus-fault behavior also needs review.

A transaction capture and keypad/LCD demonstration would provide useful evidence. No hardware results are attached yet.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
