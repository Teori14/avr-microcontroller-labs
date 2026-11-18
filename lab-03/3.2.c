
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

#define F_CPU 16000000UL

// ---------- PWM setup (Timer1 -> OC1A on PB1) ----------
static void pwm_init(void) {
    // PB1 (OC1A) output
    DDRB |= (1 << DDB1);

    // Fast PWM 8-bit: WGM13:0 = 0b0101 (WGM12=1, WGM10=1)
    TCCR1A = (1 << COM1A1) | (1 << WGM10);    // non-inverting on OC1A
    TCCR1B = (1 << WGM12)  | (1 << CS10);     // prescaler = 1 -> 62.5 kHz

    // Optional: set initial duty 50%
    OCR1A = 128;  // 0..255
}

// ---------- ADC setup/read (ADC1 on PC1) ----------
static void adc_init(void) {
    // Select Vref = AVCC (REFS1:0 = 01), right-adjust result (ADLAR=0),
    // and input channel ADC1 (MUX[3:0] = 0001).
    ADMUX  = (1 << REFS0) | 0x01;

    // ADC enable (ADEN=1), prescaler=128 -> 16MHz/128 = 125 kHz (good for 10-bit)
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);

    // (Optional) disable digital input buffer on PC1 to save power/noise
    DIDR0 |= (1 << ADC1D);
}

static uint16_t adc_read_once(void) {
    // Start single conversion
    ADCSRA |= (1 << ADSC);
    // Wait until conversion complete (ADSC clears to 0)
    while (ADCSRA & (1 << ADSC)) { }
    // Read 10-bit result: read ADCL first, then ADCH
    uint8_t low  = ADCL;
    uint8_t high = ADCH;
    return (uint16_t)((high << 8) | low); // 0..1023
}

// Return average of 16 consecutive samples (sum >> 4)
static uint16_t adc_read_avg16(void) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 16; i++) {
        sum += adc_read_once();
    }
    return (uint16_t)(sum >> 4);
}

// ---------- LED range decode on PORTD ----------
static void leds_init(void) {
    DDRD |= 0b00011111;  // PD0..PD4 as outputs
    PORTD &= ~0b00011111;
}

// Light exactly one PDx based on value
static void leds_show_by_range(uint16_t v) {
    uint8_t out = 0;
    if (v <= 200)           out = (1 << PD0);
    else if (v <= 400)      out = (1 << PD1);
    else if (v <= 600)      out = (1 << PD2);
    else if (v <= 800)      out = (1 << PD3);
    else                    out = (1 << PD4);
    // Clear PD0..PD4 then set selected bit
    PORTD = (PORTD & ~0b00011111) | out;
}

int main(void) {
    pwm_init();
    adc_init();
    leds_init();

    // Main loop: every ~100 ms, take 16 samples and update LEDs
    for (;;) {
        _delay_ms(100);                // small deviation acceptable
        uint16_t avg = adc_read_avg16();
        leds_show_by_range(avg);

        // (Optional) also map ADC avg to PWM duty to "preview" signal on PB1:
        // OCR1A = (uint8_t)(avg >> 2); // 10-bit -> 8-bit (uncomment if desired)
    }
}
