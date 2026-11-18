#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* ======================= I2C / PCA9555 ======================= */
#define PCA9555_0_ADDRESS 0x40   /* A0=A1=A2=0 by hardware on ntuAboard_G1 */
#define TWI_READ    1            /* read from TWI device */
#define TWI_WRITE   0            /* write to TWI device */
#define SCL_CLOCK   100000L      /* 100 kHz */

/* Fscl = Fcpu / (16 + 2*TWBR0_VALUE*PRESCALER_VALUE) */
#define TWBR0_VALUE ((F_CPU/SCL_CLOCK)-16)/2

/* ---------------------- PCA9555 REGISTERS ---------------------- */
typedef enum {
  REG_INPUT_0          = 0,
  REG_INPUT_1          = 1,
  REG_OUTPUT_0         = 2,
  REG_OUTPUT_1         = 3,
  REG_POLARITY_INV_0   = 4,
  REG_POLARITY_INV_1   = 5,
  REG_CONFIGURATION_0  = 6,
  REG_CONFIGURATION_1  = 7
} PCA9555_REGISTERS;

/* ----------- Master Transmitter/Receiver status codes ---------- */
#define TW_START         0x08
#define TW_REP_START     0x10

/* ---------------- Master Transmitter --------------------------- */
#define TW_MT_SLA_ACK    0x18
#define TW_MT_SLA_NACK   0x20
#define TW_MT_DATA_ACK   0x28

/* ---------------- Master Receiver ------------------------------ */
#define TW_MR_SLA_ACK    0x40
#define TW_MR_SLA_NACK   0x48
#define TW_MR_DATA_NACK  0x58

#define TW_STATUS_MASK   0b11111000
#define TW_STATUS        (TWSR0 & TW_STATUS_MASK)

/* ======================= TWI helpers =========================== */

void twi_init(void)
{
    TWSR0 = 0;                  /* prescaler = 1 */
    TWBR0 = TWBR0_VALUE;        /* set SCL to ~100 kHz */
}

unsigned char twi_readAck(void)
{
    TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWEA);
    while(!(TWCR0 & (1<<TWINT)));
    return TWDR0;
}

unsigned char twi_readNak(void)
{
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while(!(TWCR0 & (1<<TWINT)));
    return TWDR0;
}

unsigned char twi_start(unsigned char address)
{
    unsigned char twi_status;

    TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);  /* START */
    while(!(TWCR0 & (1<<TWINT)));

    twi_status = TW_STATUS & 0xF8;
    if ( (twi_status != TW_START) && (twi_status != TW_REP_START) ) return 1;

    TWDR0 = address;                                /* SLA+R/W */
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while(!(TWCR0 & (1<<TWINT)));

    twi_status = TW_STATUS & 0xF8;
    if ( (twi_status != TW_MT_SLA_ACK) && (twi_status != TW_MR_SLA_ACK) ) return 1;

    return 0;
}

void twi_start_wait(unsigned char address)
{
    unsigned char twi_status;

    for(;;) {
        TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
        while(!(TWCR0 & (1<<TWINT)));

        twi_status = TW_STATUS & 0xF8;
        if ( (twi_status != TW_START) && (twi_status != TW_REP_START) ) continue;

        TWDR0 = address;
        TWCR0 = (1<<TWINT) | (1<<TWEN);
        while(!(TWCR0 & (1<<TWINT)));

        twi_status = TW_STATUS & 0xF8;
        if ( (twi_status == TW_MT_SLA_NACK) || (twi_status == TW_MR_DATA_NACK) ) {
            TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
            while(TWCR0 & (1<<TWSTO));
            continue;
        }
        break;
    }
}

unsigned char twi_write(unsigned char data)
{
    TWDR0 = data;
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while(!(TWCR0 & (1<<TWINT)));
    if ( (TW_STATUS & 0xF8) != TW_MT_DATA_ACK ) return 1;
    return 0;
}

unsigned char twi_rep_start(unsigned char address)
{
    return twi_start(address);
}

void twi_stop(void)
{
    TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    while(TWCR0 & (1<<TWSTO));
}

/* =================== PCA9555 simple wrappers =================== */

void PCA9555_0_write(PCA9555_REGISTERS reg, unsigned char value)
{
    twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
    twi_write((unsigned char)reg);
    twi_write(value);
    twi_stop();
}

unsigned char PCA9555_0_read(PCA9555_REGISTERS reg)
{
    unsigned char ret_val;

    twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
    twi_write((unsigned char)reg);
    twi_rep_start(PCA9555_0_ADDRESS + TWI_READ);
    ret_val = twi_readNak();            /* single byte read */
    twi_stop();

    return ret_val;
}

/* ============================ main ============================ */
/* 5.2 style: Scan keypad via PCA9555 (1 row, 4 columns) and
   light LEDs via PCA9555 Port0 lower nibble.
   Mapping:
     key '1' -> LED0 (IO0_0)
     key '2' -> LED1 (IO0_1)
     key '3' -> LED2 (IO0_2)
     key 'A' -> LED3 (IO0_3)
   Row drive: IO1_3 set LOW (output)
   Columns:   IO1_4..IO1_7 inputs with pull-ups inside PCA9555
              not pressed -> reads '1', pressed -> reads '0'
*/

int main(void)
{
    /* ----- MCU pins are not used for LEDs here; everything on PCA9555 ----- */
    /* (Keep default DDR for safety) */

    /* ----- Init I2C and configure PCA9555 directions/polarities ----- */
    twi_init();

    /* Port0: 0xF0 => IO0_0..IO0_3 outputs (0), IO0_4..IO0_7 inputs (1) */
    PCA9555_0_write(REG_CONFIGURATION_0, 0xF0);
    /* Port1: 0xF7 => IO1_3 output (0), others inputs (1) */
    PCA9555_0_write(REG_CONFIGURATION_1, 0xF7);

    /* No polarity inversion on either port */
    PCA9555_0_write(REG_POLARITY_INV_0, 0x00);
    PCA9555_0_write(REG_POLARITY_INV_1, 0x00);

    /* Ensure LEDs off at start (Port0 lower nibble) */
    PCA9555_0_write(REG_OUTPUT_0, 0x00);

    /* Drive row line low: set IO1_3=0 in OUTPUT_1 */
    {
        unsigned char out1 = PCA9555_0_read(REG_OUTPUT_1);
        out1 &= ~(1<<3);                     /* bit3 = 0 -> row active low */
        PCA9555_0_write(REG_OUTPUT_1, out1);
    }

    while (1) {
        unsigned char in1, led_mask;

        /* Read columns on Port1 bits 7..4 (idle=1, pressed=0) */
        in1 = PCA9555_0_read(REG_INPUT_1);

        /* Decode: one key at a time (priority 1>2>3>A) */
        led_mask = 0x00;
        if ( !(in1 & (1<<4)) )      led_mask = 0x01;   /* '1' -> LED0 */
        else if ( !(in1 & (1<<5)) ) led_mask = 0x02;   /* '2' -> LED1 */
        else if ( !(in1 & (1<<6)) ) led_mask = 0x04;   /* '3' -> LED2 */
        else if ( !(in1 & (1<<7)) ) led_mask = 0x08;   /* 'A' -> LED3 */
        else                         led_mask = 0x00;  /* no key -> all off */

        /* Write LEDs to Port0 lower nibble (IO0_3..IO0_0) */
        PCA9555_0_write(REG_OUTPUT_0, led_mask);

        _delay_ms(10); /* small poll interval and mild debounce */
    }
}
