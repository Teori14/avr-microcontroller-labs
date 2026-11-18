#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define PCA9555_0_ADDRESS 0x40   // A0=A1=A2=0 by hardware
#define TWI_READ    1            // reading from TWI device
#define TWI_WRITE   0            // writing to TWI device
#define SCL_CLOCK   100000L      // TWI clock in Hz

// Fscl = Fcpu / (16 + 2*TWBR0_VALUE*PRESCALER_VALUE)
#define TWBR0_VALUE ((F_CPU/SCL_CLOCK)-16)/2

// ---------------------- PCA9555 REGISTERS ----------------------
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

// ----------- Master Transmitter/Receiver status codes ----------
#define TW_START         0x08
#define TW_REP_START     0x10

// ---------------- Master Transmitter ---------------------------
#define TW_MT_SLA_ACK    0x18
#define TW_MT_SLA_NACK   0x20
#define TW_MT_DATA_ACK   0x28

// ---------------- Master Receiver ------------------------------
#define TW_MR_SLA_ACK    0x40
#define TW_MR_SLA_NACK   0x48
#define TW_MR_DATA_NACK  0x58

#define TW_STATUS_MASK   0b11111000
#define TW_STATUS        (TWSR0 & TW_STATUS_MASK)

// ======================= TWI helpers ===========================

void twi_init(void)
{
    TWSR0 = 0;                  // PRESCALER_VALUE = 1
    TWBR0 = TWBR0_VALUE;        // SCL_CLOCK = 100 KHz
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

    TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
    while(!(TWCR0 & (1<<TWINT)));

    twi_status = TW_STATUS & 0xF8;
    if ( (twi_status != TW_START) && (twi_status != TW_REP_START) ) return 1;

    TWDR0 = address;
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

// =================== PCA9555 simple wrappers ===================

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
    ret_val = twi_readNak();
    twi_stop();

    return ret_val;
}

// ============================ main ============================

int main(void)
{
    /* -------- Local inputs A,B,C,D on PB0..PB3 --------
       Inverse logic: pressed = 1, not pressed = 0
       (Assumes active-low buttons with pull-ups enabled)
    */
    DDRB &= ~0x0F;             // PB3..PB0 as inputs
    PORTB |= 0x0F;             // enable internal pull-ups (idle = 1 on pins)

    /* -------- I2C init and PCA9555 port setup --------- */
    twi_init();                                   // Nominal SCL frequency: 100 kHz.
    PCA9555_0_write(REG_CONFIGURATION_0, 0x00);   // Port0: outputs
    PCA9555_0_write(REG_OUTPUT_0, 0x00);          // clear outputs

    while (1) {
        /* --- Read A,B,C,D with inverted logic (pressed=1) --- */
        unsigned char in = (~PINB) & 0x0F;  // invert PB3..PB0
        unsigned char A = (in >> 0) & 1;
        unsigned char B = (in >> 1) & 1;
        unsigned char C = (in >> 2) & 1;
        unsigned char D = (in >> 3) & 1;

        /* --- Compute logic functions ---
           F0 = (A·B' + C·B·D)'   (NAND of two terms)
           F1 = (A + C) · (B · D)
        */
        unsigned char term1 = (A & (!B));
        unsigned char term2 = (C & B & D);
        unsigned char F0    = !(term1 | term2);
        unsigned char F1    = ((A | C) & (B & D));

        /* --- Pack F0->bit0, F1->bit1 and write to PCA9555 Port0 --- */
        unsigned char out0 = (F0 ? 0x01 : 0x00) | (F1 ? 0x02 : 0x00);
        PCA9555_0_write(REG_OUTPUT_0, out0);

        _delay_ms(5);  // small poll delay for stability
    }
}
