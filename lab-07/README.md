# Lab 07 · 1-Wire temperature sensing

These C programs implement GPIO timing and commands for a DS18B20 temperature sensor. The second adds a character display through the PCA9555 expander.

| Source | Application | Output |
| :--- | :--- | :--- |
| [7.1.c](7.1.c) | Presence detection and raw-temperature acquisition | PB0 indicates the missing-device sentinel |
| [7.2.c](7.2.c) | Acquisition and signed formatting | LCD temperature and missing-device messages |

## Acquisition sequence

1. Generate a reset pulse and sample the presence response.
2. Send Skip ROM for the single-device path.
3. Request temperature conversion and wait for completion.
4. Reset, send Read Scratchpad and receive the temperature bytes.
5. Combine the bytes and format the signed value for display.

The source assigns 1-Wire to PD4. GPIO direction changes and microsecond delays implement the bus slots. The display version combines this with I²C communication to the PCA9555.

## Verification scope

Presence/disconnection checks and known positive/negative raw-value cases would demonstrate operation and formatting. The code does not validate a complete scratchpad CRC, and no measured accuracy or end-to-end integrity result is claimed.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
