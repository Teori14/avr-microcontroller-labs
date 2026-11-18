/*
 * Exercise 2.3: restartable timer triggered by INT1 on PD3
 * MCU: ATmega328PB
 * Clock: 16 MHz as configured by F_CPU below.
 *
 * PD3: INT1 button input, pull-up, falling-edge interrupt.
 * PB3: lamp indicator for the final 3 s.
 * PB0..PB5: indicators for 1 s after a new event.
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <stdint.h>

#define INDICATORS_MASK  ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5))
#define LAMP_MASK        (1<<PB3)

static volatile uint8_t timerflag = 0;

/* Millisecond delay implemented as repeated one-millisecond waits. */
static inline void delay_ms_block(uint16_t ms)
{
    while (ms--) {
        _delay_ms(1);
    }
}

/* INT1 ISR: request a restart. */
ISR(INT1_vect)
{
    timerflag = 1;
}

int main(void)
{
    /* Configure PB0..PB5 as outputs, initially low. */
    DDRB  |= INDICATORS_MASK;   // Outputs.
    PORTB &= ~INDICATORS_MASK;  // Initially off.

    /* Configure PD3 as an input with pull-up. */
    DDRD  &= ~(1<<PD3);
    PORTD |=  (1<<PD3);

    /* INT1 falling edge: ISC11 = 1, ISC10 = 0. */
    EICRA = (1<<ISC11);     // INT1 falling edge.
    EIMSK = (1<<INT1);      // enable INT1

    sei();

    for (;;)
    {
        /* Wait for an event. */
        while (timerflag == 0) {
            // idle
        }

        /* New event: turn on PB0..PB5 for 1 s. */
        PORTB = (PORTB & ~INDICATORS_MASK) | INDICATORS_MASK;
        delay_ms_block(1000);

        /* Leave only PB3 on. */
        PORTB = (PORTB & ~INDICATORS_MASK) | LAMP_MASK;

        /* Clear the event flag before the final three-second interval. */
        timerflag = 0;

        /* Wait for 3 s, checking for a restart every second. */
        for (uint8_t sec = 0; sec < 3; ++sec) {
            delay_ms_block(1000);
            if (timerflag) {
                /* New INT1 event: leave the wait loop. */
                break;
            }
        }

        if (timerflag) {
            /* Restart from the beginning and turn on PB0..PB5 for 1 s. */
            continue;
        }

        /* After 1 s on all indicators and 3 s on PB3, turn off the outputs. */
        PORTB &= ~INDICATORS_MASK;
    }
}
