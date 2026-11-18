#define F_CPU 16000000UL
#include "lcd_4bit.h"


#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdio.h>
#include <string.h>

static void lcd_print(const char *s) {
    while (*s) lcd_data((uint8_t)*s++);
}

// --- App constants (from spec & assignment) ---
#define VREF_AVCC_V     5.0f            // AVCC=5V
#define VZERO_V         0.10f           // Vgas0
#define TIA_GAIN_V_PER_A 100000.0f      // 100 kV/A
#define SENS_NA_PER_PPM 129.0f          // 129 nA/ppm (given)
#define M_V_PER_PPM     ((SENS_NA_PER_PPM * 1e-9f) * TIA_GAIN_V_PER_A) // Nominal voltage slope: 0.0129 V/ppm.
#define ALARM_PPM       75.0f

// Bargraph scaling: 0..300 ppm mapped to 0..6 LEDs.
#define MAX_PPM_FOR_BAR 300.0f
#define STEP_PPM        50.0f           // Nominal step: 50 ppm per LED.

volatile uint32_t ms = 0;
volatile uint8_t  tick100 = 0;
volatile uint8_t  blink250 = 0;

ISR(TIMER0_COMPA_vect) {
    ms++;
    if (ms % 100 == 0) tick100 = 1;     // ~100 ms
    if (ms % 250 == 0) blink250 ^= 1;   // toggle every 250 ms (4 Hz blink)
}

static void timer0_init_1ms(void) {
    // CTC mode, prescaler 64, OCR0A for ~1ms @16MHz:
    // 16 MHz / 64 = 250 kHz; 250 ticks per ms gives OCR0A = 249.
    TCCR0A = (1<<WGM01);
    TCCR0B = (1<<CS01) | (1<<CS00);     // /64
    OCR0A  = 249;
    TIMSK0 = (1<<OCIE0A);
}


static uint16_t adc_read_blocking(void) {
    ADCSRA |= (1<<ADSC);
    while (ADCSRA & (1<<ADSC)) { /* wait */ }
    uint16_t lo = ADCL;
    uint16_t hi = ADCH;
    return (hi<<8) | lo;
}

static float adc_to_voltage(uint16_t adc10) {
    return (adc10 * VREF_AVCC_V) / 1024.0f;
}

static float voltage_to_ppm(float vgas) {
    float ppm = (vgas - VZERO_V) / M_V_PER_PPM;
    return (ppm < 0.0f) ? 0.0f : ppm;
}

static void show_msg_and_value(float ppm, uint8_t alarm) {
    int p = (int)(ppm + 0.5f);
    if (p < 0)   p = 0;
    if (p > 999) p = 999;

    char line1[17];
    if (alarm) {
        // 16-char exact fit: "GAS DETECTED 123"
        // Display the alarm message followed by the rounded concentration.
        snprintf(line1, sizeof(line1), "GAS DETECTED %3d", p);
    } else {
        // Normal state: display the concentration.
        snprintf(line1, sizeof(line1), "CLEAR %3dppm", p);
    }

    lcd_clear_display();
    lcd_goto(0, 0);
    lcd_print(line1);
}



static void drive_bargraph(float ppm, uint8_t alarm, uint8_t blink_state) {
    if (ppm > MAX_PPM_FOR_BAR) ppm = MAX_PPM_FOR_BAR;
    int bars = (int)(ppm / STEP_PPM + 0.999f); // ceil
    if (bars < 0) bars = 0;
    if (bars > 6) bars = 6;

    uint8_t mask = (bars == 0) ? 0 : ((1u << bars) - 1u);


    uint8_t out = alarm ? (blink_state ? 0 : mask) : mask;

    PORTB = (PORTB & ~0x3F) | (out & 0x3F);
}


int main(void) {
    // PB0..PB5 as outputs (bargraph)
    DDRB |= 0x3F;
    PORTB &= ~0x3F;

    // LCD on PORTD per lab wiring (link the provided asm driver)
    DDRD = 0xFF;

    lcd_init();
    lcd_clear_display();
    //lcd_print("CO Monitor");

    // ADMUX: REFS=01 (AVCC=5V), MUX=ADC3 (PC3), ADLAR=0
    ADMUX  = (1<<REFS0) | (3);          // MUX[3:0]=0011
    // Enable ADC with prescaler 128: nominal clock 125 kHz.
    ADCSRA = (1<<ADEN) | (1<<ADPS2)|(1<<ADPS1)|(1<<ADPS0);

    timer0_init_1ms();
    sei();

    // simple moving average over 4 samples
    float ppm_ma = 0.0f;
    //const float alpha = 0.5f; // EMA

    uint8_t last_alarm = 0xFF; // force first LCD refresh

    for (;;) {
        if (tick100) {
            tick100 = 0;

            uint16_t adc = adc_read_blocking();
            float vgas = adc_to_voltage(adc);
            float ppm  = voltage_to_ppm(vgas);

            // smooth
            ppm_ma = (0.5f)*ppm + (0.5f)*ppm_ma;

            uint8_t alarm = (ppm_ma >= ALARM_PPM);

            // update LEDs (blink if alarm)
            drive_bargraph(ppm_ma, alarm, blink250);

            // update LCD only when alarm state changes or every ~1s (optional)
            static uint32_t last_lcd = 0;
            if (alarm != last_alarm || (ms - last_lcd) >= 1000) {
                show_msg_and_value(ppm_ma, alarm);
                last_lcd = ms;
                last_alarm = alarm;
            }
        }
    }
}
