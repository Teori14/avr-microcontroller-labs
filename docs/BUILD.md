# Build and hardware setup

## Select one program

Each subexercise has its own entry point or reset vector. Build alternatives separately. Lab 4.3 requires both `4_3.c` and `lcd_4bit.c`.

Lab 8 is a recovered intermediate snapshot, not a complete build target. It still requires the `USART.h` and `owi_ds18b20.h` interfaces and implementations, a review of linkage between `lcd_twi.h` and `lcd_twi.c`, and restoration of the application flow. See its [snapshot notes](../lab-08/README.md).

## C toolchain

Use an AVR C toolchain and device pack supporting **ATmega328PB**, with avr-libc headers. Source generally assumes **16 MHz**. Define F_CPU in the project settings too: in 3.2 its source definition appears after the delay header.

These AVR GCC commands illustrate the build from the repository root. They have not been executed successfully in this publication workflow:

```powershell
New-Item -ItemType Directory -Force build
avr-gcc -mmcu=atmega328pb -DF_CPU=16000000UL -Os -std=gnu11 -Wall -Wextra lab-07/7.1.c -o build/lab-07-1.elf
avr-objcopy -O ihex -R .eeprom build/lab-07-1.elf build/lab-07-1.hex
```

For the LCD application:

```powershell
avr-gcc -mmcu=atmega328pb -DF_CPU=16000000UL -Os -std=gnu11 -Wall -Wextra lab-04/4_3.c lab-04/lcd_4bit.c -o build/lab-04-3.elf
```

Compiler version, original linker settings and programmer configuration still require confirmation. Review warnings rather than treating these examples as verification results.

## Assembly toolchain

The source uses AVRASM-style directives and `m328pbdef.inc`, with mixed filename capitalization. Configure the matching assembler and device-pack include path. GNU assembler uses a different dialect, so do not assume these sources can be passed to it unchanged.

## Board preparation

Confirm board model, clock, voltage and programmer. Pin assignments differ between exercises: follow the selected source for LCD signals, keypad rows/columns, ADC and LEDs. Check I²C address conventions and pull-ups, including the external 1-Wire pull-up.

Programming commands depend on the confirmed board/programmer setup. No fuse changes are specified.
