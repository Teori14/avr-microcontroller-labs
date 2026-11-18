# Lab 03 · PWM and analog input

These programs directly configure timers, ADC and GPIO for PWM control and analog-input indication.

| Source | Application | Interfaces |
| :--- | :--- | :--- |
| [3.1.asm](3.1.asm) | Select PWM duty with UP/DOWN buttons and display its table index | Timer 1, OC1A/PB1, PB4/PB3 buttons, PORTC |
| [3.2.c](3.2.c) | Average 16 ADC samples and select an LED by input range | ADC, GPIO indicators; PWM initialization is also present |
| [3.3.c](3.3.c) | Switch between manual duty selection and ADC-controlled PWM | Mode buttons, ADC0, Timer 1 |

## Design approach

Manual control uses an OCR1A lookup table. ADC routines read the low result byte before the high byte. In 3.2, a 16-sample sum is shifted by four to obtain an integer average before range decoding.

## Known issues in 3.3

`sum >> 4;` does not assign its result, so the following code uses the sum rather than the intended average. The index-display routine also assigns the entire PORTD register, affecting the mode-button pull-ups. These behaviors are preserved and flagged for review; this version's ADC-controlled PWM is not presented as verified.

A PWM trace and controlled ADC-input checks would establish duty, frequency and range behavior after review.

[Setup](../docs/BUILD.md) · [Collection](../README.md)
