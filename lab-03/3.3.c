#define F_CPU 16000000UL
#include <avr/io.h>
#include <stdint.h>
#include <stdbool.h>

// ---------------- PWM init (Timer1 -> OC1A on PB1) ----------------
static void pwm_init(void) {
    DDRB  |= (1 << DDB1);                    // Set PB1 as output for Timer 1 OC1A.
    TCCR1A = (1 << COM1A1) | (1 << WGM10);   // 8-bit fast PWM, non-inverting output; duty is controlled by OCR1A.
    TCCR1B = (1 << WGM12)  | (1 << CS10);    // 8-bit fast PWM, prescaler 1: nominal frequency 16 MHz / 256 = 62.5 kHz.
    OCR1A  = 0;                              // Initial duty setting: zero.
}

// ---------------- ADC init/read (generic channel) -----------------
static void adc_init(void) {
    ADMUX  = (1 << REFS0) | 0x00;            // AVCC reference, right-adjusted result, ADC0 selected.
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0); // Enable ADC, prescaler 128: nominal ADC clock 125 kHz.
}

static uint16_t adc_read_once(void) {
    ADCSRA |= (1 << ADSC);                   // Start conversion.
    while (ADCSRA & (1 << ADSC)) { }         // Wait for conversion to finish.
    uint8_t lo = ADCL, hi = ADCH;            // Read ADCL before ADCH to latch the 10-bit result.
    return (uint16_t)((hi << 8) | lo);       // Combine the result bytes into a 16-bit value.
}

// ---------------- GPIO init ------------------
static void io_init(void) {
    // PD0 and PD1: inputs with pull-ups.
    DDRD  &= ~((1 << DDD0) | (1 << DDD1));
    PORTD |=  ((1 << PORTD0) | (1 << PORTD1));

    // Manual-mode buttons: PB4 = UP and PB3 = DOWN, with pull-ups.
    DDRB  &= ~((1 << DDB4) | (1 << DDB3));
    PORTB |=  ((1 << PORTB4) | (1 << PORTB3));

    // PD2..PD6: outputs; PD0..PD1 remain inputs.
    DDRD  |=  (1 << DDD2) | (1 << DDD3) | (1 << DDD4) | (1 << DDD5) | (1 << DDD6);
    // Clear PD2..PD6.
    PORTD &= ~((1 << PD2) | (1 << PD3) | (1 << PD4) | (1 << PD5) | (1 << PD6));
}

static inline bool fell(uint8_t prev, uint8_t now, uint8_t mask) {
    // Active-low button: released = 1, pressed = 0; detect a falling edge.
    return ( (prev & mask) && !(now & mask) ); // True when the masked input changes from 1 to 0.
}

// Display idx on PD2..PD6, with LSB on PD2.
static void show_idx_on_PORTD(uint8_t idx) {
    uint8_t bits = (uint8_t)(idx & 0x1F) << 2;
    PORTD = bits;
}

// OCR1A lookup table: approximately 2%..98% in 6% steps.
static const uint8_t duty_table[] = {
    //   2%,   8%,  14%,  20%,  26%,  32%,  38%,  44%,  50%,  56%,  62%,  68%,  74%,  80%,  86%,  92%,  98%
         5,   20,   36,   51,   66,   82,   97,  112,  128,  143,  158,  173,  189,  204,  219,  235,  250
};
// OCR1A settings for 8-bit fast PWM with TOP = 255.

#define DUTY_MIN_IDX   0
#define DUTY_MAX_IDX   16

int main(void) {
    pwm_init();   // Initialize Timer 1 / OC1A PWM.
    adc_init();   // Initialize ADC.
    io_init();    // Initialize GPIO.

    uint8_t mode = 2;   // Start in ADC-controlled mode.
    int8_t  idx  = 8;   // Initial index: nominal duty approximately 50%.
    OCR1A = duty_table[idx];              // Apply the initial duty setting.
    show_idx_on_PORTD((uint8_t)idx);      // Display the current index.

    // Previous input samples for edge detection on ports D and B.
    uint8_t prevPIND = PIND;
    uint8_t prevPINB = PINB;

    // Button latches prevent repeated changes while a button remains held.
    bool pb4_down = false;  // UP (PB4)
    bool pb3_down = false;  // DOWN (PB3)

    for (;;) {
        uint8_t nowPIND = PIND;           // Sample port D.
        uint8_t nowPINB = PINB;           // Sample port B.

        // Mode selection
        if (fell(prevPIND, nowPIND, (1 << PIND0))) {
            mode = 1;
            // Synchronize the port B button latches when entering manual mode.
            prevPINB = nowPINB;
            pb4_down = !(nowPINB & (1 << PINB4)); // Latch UP if it is already pressed.
            pb3_down = !(nowPINB & (1 << PINB3)); // Latch DOWN if it is already pressed.
            // Refresh the displayed index.
            show_idx_on_PORTD((uint8_t)idx);
        }
        if (fell(prevPIND, nowPIND, (1 << PIND1))) {
            mode = 2;
        }

        // Mode 1: manual UP/DOWN control
        if (mode == 1) {
            // PB4 UP: increment once per new press, within the allowed range.
            if (!pb4_down && fell(prevPINB, nowPINB, (1 << PINB4)) && idx < DUTY_MAX_IDX) {
                idx++;
                OCR1A = duty_table[idx];              // Apply the updated duty setting.
                show_idx_on_PORTD((uint8_t)idx);
                pb4_down = true;                      // Latch until button release.
            }
            // PB3 DOWN: decrement once per new press.
            if (!pb3_down && fell(prevPINB, nowPINB, (1 << PINB3)) && idx > DUTY_MIN_IDX) {
                idx--;
                OCR1A = duty_table[idx];
                show_idx_on_PORTD((uint8_t)idx);
                pb3_down = true;
            }
            // Release the latches when the buttons return high.
            if (pb4_down && (nowPINB & (1 << PINB4))) pb4_down = false;
            if (pb3_down && (nowPINB & (1 << PINB3))) pb3_down = false;
        }
        // Mode 2: ADC0-controlled duty
        else {
            ADMUX = (ADMUX & 0xF0);
            uint32_t sum = 0;
            for (uint8_t i = 0; i < 16; i++) sum += adc_read_once(); // Accumulate 16 readings for intended averaging.
            sum >> 4;             // This expression does not update sum; see the review notes in the README.

            uint16_t avg = sum;
            OCR1A = (uint8_t)(avg >> 2);                 // Convert to an 8-bit PWM setting; review the averaging issue noted above.
        }

        prevPIND = nowPIND;
        prevPINB = nowPINB;
    }
}
