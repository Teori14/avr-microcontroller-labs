#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define PCA9555_0_ADDRESS 0x40   // A0=A1=A2=0 by hardware (SLA+W byte)
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

/* ======================= TWI helpers ======================= */

void twi_init(void)
{
    TWSR0 = 0;                  // prescaler = 1
    TWBR0 = TWBR0_VALUE;        // SCL ~= 100 kHz
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

    TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);   // START
    while(!(TWCR0 & (1<<TWINT)));

    twi_status = TW_STATUS & 0xF8;
    if ((twi_status != TW_START) && (twi_status != TW_REP_START)) return 1;

    TWDR0 = address;                                // SLA+R/W
    TWCR0 = (1<<TWINT) | (1<<TWEN);
    while(!(TWCR0 & (1<<TWINT)));

    twi_status = TW_STATUS & 0xF8;
    if ((twi_status != TW_MT_SLA_ACK) && (twi_status != TW_MR_SLA_ACK)) return 1;

    return 0;
}

void twi_start_wait(unsigned char address)
{
    unsigned char twi_status;

    for(;;) {
        TWCR0 = (1<<TWINT) | (1<<TWSTA) | (1<<TWEN);
        while(!(TWCR0 & (1<<TWINT)));

        twi_status = TW_STATUS & 0xF8;
        if ((twi_status != TW_START) && (twi_status != TW_REP_START)) continue;

        TWDR0 = address;
        TWCR0 = (1<<TWINT) | (1<<TWEN);
        while(!(TWCR0 & (1<<TWINT)));

        twi_status = TW_STATUS & 0xF8;
        if ((twi_status == TW_MT_SLA_NACK) || (twi_status == TW_MR_DATA_NACK)) {
            TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);   // STOP, retry
            while (TWCR0 & (1<<TWSTO));
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
    if ((TW_STATUS & 0xF8) != TW_MT_DATA_ACK) return 1;
    return 0;
}

unsigned char twi_rep_start(unsigned char address)
{
    return twi_start(address);
}

void twi_stop(void)
{
    TWCR0 = (1<<TWINT) | (1<<TWEN) | (1<<TWSTO);
    while (TWCR0 & (1<<TWSTO));
}

/* ============== PCA9555 simple wrappers ============== */

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

/* ================= LCD via PCA9555 (4-bit) ================ */
/* Mapping on IO0: RS->bit2, E->bit3, D4..D7->bits4..7 */
#define LCD_RS   (1<<2)
#define LCD_E    (1<<3)
#define LCD_D4   (1<<4)
#define LCD_D5   (1<<5)
#define LCD_D6   (1<<6)
#define LCD_D7   (1<<7)

/* write one 4-bit nibble on D4..D7 and pulse E; rs=0:cmd, rs=1:data */
void lcd_write4(unsigned char nibble, unsigned char rs)
{
    unsigned char v = 0;

    if (rs) v |= LCD_RS;                   // set RS if data

    if (nibble & 0x01) v |= LCD_D4;
    if (nibble & 0x02) v |= LCD_D5;
    if (nibble & 0x04) v |= LCD_D6;
    if (nibble & 0x08) v |= LCD_D7;

    PCA9555_0_write(REG_OUTPUT_0, v);             // E=0, data setup
    PCA9555_0_write(REG_OUTPUT_0, v | LCD_E);     // E rising edge
    _delay_us(1);
    PCA9555_0_write(REG_OUTPUT_0, v);             // E falling edge
    _delay_us(50);                                 // allow command to settle
}

/* send full 8-bit value in two nibbles */
void lcd_send(unsigned char value, unsigned char rs)
{
    lcd_write4(value >> 4, rs);   // high nibble
    lcd_write4(value & 0x0F, rs); // low nibble
}

void lcd_command(unsigned char cmd) { lcd_send(cmd, 0); }
void lcd_data   (unsigned char dat) { lcd_send(dat, 1); }

void lcd_init(void)
{
    _delay_ms(200);                       // power-up time

    // force 8-bit mode 3x, then switch to 4-bit (standard init)
    lcd_write4(0x03, 0); _delay_us(250);
    lcd_write4(0x03, 0); _delay_us(250);
    lcd_write4(0x03, 0); _delay_us(250);
    lcd_write4(0x02, 0); _delay_us(250); // 4-bit

    lcd_command(0x28);    // Function set: 4-bit, 2 lines, 5x8
    lcd_command(0x0C);    // Display ON, cursor OFF, blink OFF
    lcd_command(0x01);    // Clear display
    _delay_ms(5);
    lcd_command(0x06);    // Entry mode: increment, no shift
}

void lcd_set_line1(void) { lcd_command(0x80); }
void lcd_set_line2(void) { lcd_command(0xC0); }

void lcd_print(const char *s)
{
    while (*s) lcd_data((unsigned char)*s++);
}

/* ============================ main ============================ */

int main(void)
{
    /* TWI + PCA9555 setup: IO0 as outputs for LCD lines */
    twi_init();
    PCA9555_0_write(REG_CONFIGURATION_0, 0x00);   // IO0[7:0] outputs
    PCA9555_0_write(REG_POLARITY_INV_0,  0x00);   // normal polarity
    PCA9555_0_write(REG_OUTPUT_0,        0x00);   // clear port0

    /* LCD init and print */
    lcd_init();

    lcd_set_line1();
    lcd_print("THEODOROS");

    lcd_set_line2();
    lcd_print("RIMAGMOS");

    while (1) { /* idle */ }
}
