# Lab 04 · Analog acquisition and LCD output

The laboratory progresses from ADC acquisition and character display to a concentration-monitor exercise with threshold indication.

| Exercise | Sources | Scope |
| :--- | :--- | :--- |
| 4.1 | [4_1.asm](4_1.asm) | Assembly ADC, timer interrupt and LCD routines |
| 4.2 | [4_2.c](4_2.c) | C timer-triggered acquisition and LCD output |
| 4.3 | [4_3.c](4_3.c), [driver](lcd_4bit.c), [header](lcd_4bit.h) | Concentration estimate, bargraph and alarm display |

## Monitor flow

Timer 0 provides a nominal one-millisecond tick. Approximately every 100 ms, 4.3 reads ADC3, converts the result to voltage and concentration using the exercise constants, applies exponential smoothing, and updates the LED bargraph. Above 75 ppm, it displays `GAS DETECTED` and alternates the LED mask. The LCD refreshes on a state change or roughly once per second.

These are laboratory conversion constants; no calibration or measured detection accuracy is claimed.

## LCD wiring

The included 4-bit driver uses PD7..PD4 for data, PD3 for enable and PD2 for register select. Compile 4.3 with `lcd_4bit.c`. PB0..PB5 drive its bargraph.

## Review notes

An original comment calls the filter a four-sample average, but the expression implements exponential smoothing. The blink state toggles every 250 ms, giving a full on/off period of 500 ms. Hardware results remain to be recorded.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
