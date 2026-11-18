/*
 * Exercise 2.3 – External interrupt on INT1 (PD3)
 * MCU: ATmega328PB
 * Clock: 16 MHz (F_CPU)
 *
 * PD3 -> INT1 (button input, pull-up, falling-edge triggered)
 * PB3 -> Main lamp LED (stays ON for the remaining time)
 * PB0–PB5 -> Indicator LEDs (all ON for the first 1 s)
 */

#define F_CPU 16000000UL
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include <util/atomic.h>
#include <stdint.h>

#define INDICATORS_MASK  ((1<<PB0)|(1<<PB1)|(1<<PB2)|(1<<PB3)|(1<<PB4)|(1<<PB5))
#define LAMP_MASK        (1<<PB3)

static volatile uint8_t restartflag = 0;

/* Interruptible delay: waits up to 'ms' milliseconds.
 * Returns 1 if a new press was detected (restart requested), 0 otherwise. */
static inline uint8_t delay_ms_interruptible(uint16_t ms)
{
    while (ms--) {
        _delay_ms(1);
        if (restartflag) {
            return 1;   // interrupted by button press
        }
    }
    return 0;           // completed normally
}

/* INT1 ISR: mark that a restart was requested */
ISR(INT1_vect)
{
    restartflag = 1;
}

int main(void)
{
    /* --- PORTB: PB0–PB5 outputs, start LOW (LEDs OFF) --- */
    DDRB  |= INDICATORS_MASK;      // outputs
    PORTB &= ~INDICATORS_MASK;     // all OFF

    /* --- PD3 as input with internal pull-up --- */
    DDRD  &= ~(1<<PD3);
    PORTD |=  (1<<PD3);

    /* --- Configure INT1 for falling edge (ISC11=1, ISC10=0) --- */
    EICRA = (1<<ISC11);            // falling edge on INT1
    EIMSK = (1<<INT1);             // enable INT1

    sei();

    for (;;)
    {
        /* Idle until we see a press (set by ISR) */
        while (restartflag == 0) {
            /* idle */
        }

start_cycle:
        /* Consume the press that brought us here (atomically) */
        ATOMIC_BLOCK(ATOMIC_FORCEON) {
            restartflag = 0;
        }

        /* Step 1: Turn ON PB0–PB5 for 1 s */
        PORTB = (PORTB & ~INDICATORS_MASK) | INDICATORS_MASK;

        /* If pressed during the 1 s window, restart immediately */
        if (delay_ms_interruptible(1000) || restartflag) {
            goto start_cycle;
        }

        /* Step 2: Leave only PB3 ON for the remaining 3 s */
        PORTB = (PORTB & ~INDICATORS_MASK) | LAMP_MASK;

        for (uint8_t sec = 0; sec < 3; ++sec) {
            /* Each second is interruptible; any press restarts the cycle */
            if (delay_ms_interruptible(1000) || restartflag) {
                goto start_cycle;
            }
        }

        /* No new press within 4 s total → turn everything OFF */
        PORTB &= ~INDICATORS_MASK;
        /* Then wait again for the next button press */
    }
}
