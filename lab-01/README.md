# Lab 01 · AVR Assembly fundamentals

Three standalone programs connect instruction execution to register values, port outputs and elapsed time.

| Source | Application | Concepts |
| :--- | :--- | :--- |
| [1.1_working.asm](1.1_working.asm) | Repeated calls to a millisecond delay routine, with duration in r25:r24 | Stack, subroutines, nested loops and cycle counting |
| [1.2_working.asm](1.2_working.asm) | Register-based evaluation of Boolean expressions | Bitwise logic, complements and output inspection |
| [1.3_working.asm](1.3_working.asm) | A single active bit moving between PORTD endpoints | Shifts, branches and the T flag |

## Moving LED control flow

The program writes its position to PORTD, checks for an endpoint, selects direction with the T flag, waits, and shifts once. Configured delays are 2000 ms per step plus 1000 ms at endpoints. These are intended settings, not measured timings.

## Evidence to add

A simulator trace can show register and port changes. A short LED video with a timing trace would demonstrate the moving-bit sequence. The work was tested and assessed during the course. The original `working` filenames are retained; they do not distinguish the exact assessed version from other archived copies.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
