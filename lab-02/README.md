# Lab 02 · External interrupts

Button events drive counters and timed output sequences. Interrupt handlers signal work through flags or update output state.

| Exercise | Sources | Behavior in the code |
| :--- | :--- | :--- |
| 2.1 | [Assembly](2.1.asm) | INT1 on PD3 triggers a counter on PC1..PC5 |
| 2.2 | [Assembly](2.2.asm) | Periodic counter and INT0 handling to show the number of pressed buttons |
| 2.3 | [Assembly](2.3.asm), [revised assembly](2.3_better.asm), [C](2.3_c.c), [revised C](2.3_c_better.c) | Restartable lamp sequence |

## Restartable lamp sequence

The indicator group is enabled for one second, followed by the lamp-only interval. Another INT1 event requests a restart. The revised C version checks this request during repeated one-millisecond waits, making the delay interruptible.

The four 2.3 files are alternative programs. The original C version checks for events at coarser intervals. The work was tested and assessed during the course; the saved alternatives still need to be matched to the assessed version.

## Evidence to add

Record the normal sequence and a second press during the lamp interval, along with clock and button wiring. No measured timing is claimed here.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
