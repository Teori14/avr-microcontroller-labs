# Verification and known limitations

## Checks completed

The source files were mapped to labs and given language-appropriate extensions. Greek comments were translated; comments already damaged into question marks were rewritten from the surrounding implementation. Lab 4.3 filenames were aligned with its include directives. Functional code was retained during this editorial pass.

No new compiler, simulator or board result is claimed. Contributors still need to confirm tested versions and individual roles before a public portfolio release.

## Review items

| Item | Observation | Next check |
| :--- | :--- | :--- |
| Lab 3.3 | `sum >> 4;` does not update the sum | Correct separately and test against known ADC inputs |
| Lab 3.3 | Index display overwrites PORTD | Review mode-button pull-ups and intended output mask |
| Lab 3.2 | F_CPU is defined after the delay header | Define it in build settings and inspect diagnostics |
| Lab 5.2 | Comment assumes expander pull-ups | Check device documentation and board resistors |
| I²C routines | Several waits have no timeout | Test missing-device and bus-fault behavior |
| Lab 7 | No complete scratchpad CRC check | Add integrity handling if extending beyond the lab |
| Alternative versions | Multiple 2.3 and 6.1 programs | Identify submitted and hardware-tested versions |

## Evidence to collect

| Lab | Useful evidence |
| :--- | :--- |
| 01 | Simulator trace and timed LED movement |
| 02 | Normal sequence and interrupt/restart trace |
| 03 | PWM trace and controlled ADC input checks |
| 04 | LCD output and threshold transitions |
| 05 | I²C transaction capture and keypad/LCD demo |
| 06 | Key press/release behavior and both combination outcomes |
| 07 | Presence/disconnection and raw-temperature formatting checks |

Real captures should identify firmware version, board, toolchain, clock and test conditions. Expected examples should be labeled separately from observed output.
