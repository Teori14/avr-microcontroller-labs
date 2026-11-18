# Lab 06 · Matrix keypad applications

A 4×4 keypad is scanned through the PCA9555. Each row is activated in turn, columns are read into a key mask, and a lookup table converts positions to characters.

| Source | Application | Behavior |
| :--- | :--- | :--- |
| [6.1_2025.c](6.1_2025.c) | Key-to-LED mapping | Continuous scanning; A, 8, 6 and * mapping |
| [6.1_2026.c](6.1_2026.c) | Alternative key-to-LED mapping | Two-sample filtering; 4, 3, 2 and B mapping |
| [6.2.c](6.2.c) | Key display | New-key detection updates the LCD |
| [6.3.c](6.3.c) | Two-digit combination exercise | State machine collects and evaluates the pair |

## From scan to application

Later versions intersect two scans separated by a delay to filter input. Comparison with the previous key state detects new presses. The combination exercise waits for a first digit, then a second, and evaluates them. A match produces a timed LED indication; a mismatch produces a blink sequence.

## Version notes

Build the 6.1 variants separately: their mappings differ. In the 2026 variant, the main loop calculates the new-press mask but decodes the maintained pressed-key state. The intended final version still needs confirmation.

The combination is a laboratory constant. This is an input/state-machine exercise with no claim of security properties.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
