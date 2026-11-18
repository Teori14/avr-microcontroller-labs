#define F_CPU 16000000UL

#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>

/* ------------ 1-Wire line on PD4 (ntuAboard_G1) ------------ */
#define OW_DDR   DDRD
#define OW_PORT  PORTD
#define OW_PIN   PIND
#define OW_BIT   PD4

/* ------------ Helper delay in microseconds ------------------ */
/* Variable delay: call _delay_us(1) inside a loop */
static void wait_usec(uint16_t us)
{
    while (us--) {
        _delay_us(1);
    }
}

/* ============================================================
 *      Low-level 1-Wire routines (same logic as assembly)
 * ============================================================ */

/* Reset pulse + presence detect.
 * Return 1 if a device responds, 0 otherwise.
 */
uint8_t one_wire_reset(void)
{
    uint8_t presence;

    /* drive PD4 low for 480 us (reset pulse) */
    OW_DDR  |= (1 << OW_BIT);      // PD4 as output
    OW_PORT &= ~(1 << OW_BIT);     // drive low
    wait_usec(480);

    /* release line: input, no pull-up (external pull-up on board) */
    OW_DDR  &= ~(1 << OW_BIT);     // PD4 as input
    OW_PORT &= ~(1 << OW_BIT);     // disable internal pull-up
    wait_usec(100);                // wait for presence pulse start

    /* read presence: device pulls line low => presence = 1 */
    if ( (OW_PIN & (1 << OW_BIT)) == 0 )
        presence = 1;
    else
        presence = 0;

    /* wait to complete reset time slot */
    wait_usec(380);

    return presence;
}

/* Receive ONE bit from DS1820.
 * Return 0 or 1.
 */
uint8_t one_wire_receive_bit(void)
{
    uint8_t bit;

    /* master pulls line low for 2 us to start read slot */
    OW_DDR  |= (1 << OW_BIT);
    OW_PORT &= ~(1 << OW_BIT);
    wait_usec(2);

    /* release line and sample after ~10 us */
    OW_DDR  &= ~(1 << OW_BIT);
    OW_PORT &= ~(1 << OW_BIT);
    wait_usec(10);

    bit = (OW_PIN & (1 << OW_BIT)) ? 1 : 0;

    /* wait rest of time slot (49 us) */
    wait_usec(49);

    return bit;
}

/* Transmit ONE bit to DS1820 (LSB in bit0).
 * bit==1 -> release line after 2 us (high),
 * bit==0 -> keep line low for the whole slot.
 */
void one_wire_transmit_bit(uint8_t bit)
{
    /* start time slot: pull low for 2 us */
    OW_DDR  |= (1 << OW_BIT);
    OW_PORT &= ~(1 << OW_BIT);
    wait_usec(2);

    /* write bit value on the bus */
    if (bit & 0x01) {
        OW_PORT |=  (1 << OW_BIT);   // logic '1' => release high
    } else {
        OW_PORT &= ~(1 << OW_BIT);   // logic '0' => keep low
    }

    /* wait for device to sample (58 us) */
    wait_usec(58);

    /* release line and small recovery time */
    OW_DDR  &= ~(1 << OW_BIT);
    OW_PORT &= ~(1 << OW_BIT);
    wait_usec(1);
}

/* Receive ONE BYTE (LSB first) from DS1820.
 * Return in r24 (C ABI -> function return).
 */
uint8_t one_wire_receive_byte(void)
{
    uint8_t i;
    uint8_t value = 0;

    for (i = 0; i < 8; i++) {
        uint8_t b = one_wire_receive_bit();  // b in {0,1}

        /* same algorithm as assembly:
         * shift right and put new bit at MSB position
         */
        value >>= 1;
        if (b) {
            value |= 0x80;
        }
    }

    return value;
}

/* Transmit ONE BYTE (LSB first) to DS1820. */
void one_wire_transmit_byte(uint8_t data)
{
    uint8_t i;

    for (i = 0; i < 8; i++) {
        uint8_t b = data & 0x01;      // take LSB
        one_wire_transmit_bit(b);
        data >>= 1;                   // next bit
    }
}

/* ============================================================
 *        High-level routine: read raw temperature
 * ============================================================
 *
 * Returns:
 *   - 16-bit temperature (2's complement) from DS1820/DS18B20
 *   - 0x8000 if no device is connected on PD4
 *
 * For DS1820 (9-bit, 0.5 °C/LSB):
 *   example 0x0032 => +25.0 °C, 0xFFFF => -0.5 °C
 * For DS18B20 (0.0625 °C/LSB):
 *   example 0x0190 => +25.0 °C, 0xFFF8 => -0.5 °C
 */
uint16_t ds1820_read_temp_raw(void)
{
    uint8_t presence;
    uint8_t temp_lsb, temp_msb;
    uint16_t raw;

    /* 1) Reset + presence detect */
    presence = one_wire_reset();
    if (!presence) {
        /* no device on PD4 -> return 0x8000 */
        return 0x8000;
    }

    /* 2) Skip ROM (0xCC) because we know there is only one device */
    one_wire_transmit_byte(0xCC);

    /* 3) Convert T command (0x44) starts temperature conversion */
    one_wire_transmit_byte(0x44);

    /* 4) Wait until conversion is finished.
     * Device sends '1' when conversion is done.
     */
    while (1) {
        uint8_t done = one_wire_receive_bit();
        if (done) {
            break;   // conversion ready
        }
        /* each read slot already has ~60 us delay */
    }

    /* 5) Reset again and check presence */
    presence = one_wire_reset();
    if (!presence) {
        return 0x8000;
    }

    /* 6) Skip ROM again */
    one_wire_transmit_byte(0xCC);

    /* 7) Read Scratchpad (0xBE) -> first two bytes = temperature */
    one_wire_transmit_byte(0xBE);

    temp_lsb = one_wire_receive_byte();  // low 8 bits
    temp_msb = one_wire_receive_byte();  // high 8 bits (sign extension)

    raw = (uint16_t)temp_lsb | ((uint16_t)temp_msb << 8);

    return raw;  // ABI: r24 = low, r25 = high
}

/* ============================================================
 *              Simple main() test on ntuAboard_G1
 * ============================================================
 * PB0 LED ON  : no device (0x8000)
 * PB0 LED OFF : device present (valid temperature received)
 */
int main(void)
{
    uint16_t t;

    /* PB0 as output (on-board LED) */
    DDRB |= (1 << PB0);

    while (1) {
        t = ds1820_read_temp_raw();

        if (t == 0x8000) {
            /* no device -> turn LED ON */
            PORTB |= (1 << PB0);
        } else {
            /* device OK -> turn LED OFF */
            PORTB &= ~(1 << PB0);
        }

        _delay_ms(500);
    }
}
