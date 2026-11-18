#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* ======================= I2C / PCA9555 (as in ex.5) ======================= */
#define PCA9555_0_ADDRESS 0x40   /* A0=A1=A2=0 on ntuAboard_G1 */
#define TWI_READ    1
#define TWI_WRITE   0
#define SCL_CLOCK   100000L
#define TWBR0_VALUE ((F_CPU/SCL_CLOCK)-16)/2

typedef enum {
  REG_INPUT_0         = 0,
  REG_INPUT_1         = 1,
  REG_OUTPUT_0        = 2,
  REG_OUTPUT_1        = 3,
  REG_POLARITY_INV_0  = 4,
  REG_POLARITY_INV_1  = 5,
  REG_CONFIGURATION_0 = 6,
  REG_CONFIGURATION_1 = 7
} PCA9555_REGISTERS;

/* ----------- Master status codes ---------- */
#define TW_START         0x08
#define TW_REP_START     0x10
#define TW_MT_SLA_ACK    0x18
#define TW_MT_SLA_NACK   0x20
#define TW_MT_DATA_ACK   0x28
#define TW_MR_SLA_ACK    0x40
#define TW_MR_SLA_NACK   0x48
#define TW_MR_DATA_NACK  0x58
#define TW_STATUS_MASK   0b11111000
#define TW_STATUS        (TWSR0 & TW_STATUS_MASK)

/* ======================= TWI helpers (same spirit as 5.x) ====================== */
void twi_init(void){ TWSR0 = 0; TWBR0 = TWBR0_VALUE; }
unsigned char twi_readAck(void){ TWCR0=(1<<TWINT)|(1<<TWEN)|(1<<TWEA); while(!(TWCR0&(1<<TWINT))); return TWDR0; }
unsigned char twi_readNak(void){ TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT))); return TWDR0; }
unsigned char twi_start(unsigned char address){
    unsigned char s;
    TWCR0=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
    s = TW_STATUS & 0xF8; if(s!=TW_START && s!=TW_REP_START) return 1;
    TWDR0=address; TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
    s = TW_STATUS & 0xF8; if(s!=TW_MT_SLA_ACK && s!=TW_MR_SLA_ACK) return 1;
    return 0;
}
void twi_start_wait(unsigned char address){
    unsigned char s;
    for(;;){
        TWCR0=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
        s = TW_STATUS & 0xF8; if(s!=TW_START && s!=TW_REP_START) continue;
        TWDR0=address; TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
        s = TW_STATUS & 0xF8;
        if(s==TW_MT_SLA_NACK || s==TW_MR_DATA_NACK){ TWCR0=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO); while(TWCR0&(1<<TWSTO)); continue; }
        break;
    }
}
unsigned char twi_write(unsigned char data){ TWDR0=data; TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT))); return ((TW_STATUS&0xF8)==TW_MT_DATA_ACK)?0:1; }
unsigned char twi_rep_start(unsigned char address){ return twi_start(address); }
void twi_stop(void){ TWCR0=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO); while(TWCR0&(1<<TWSTO)); }

/* =================== PCA9555 wrappers =================== */
void PCA9555_0_write(PCA9555_REGISTERS reg, unsigned char value){
    twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
    twi_write((unsigned char)reg);
    twi_write(value);
    twi_stop();
}
unsigned char PCA9555_0_read(PCA9555_REGISTERS reg){
    unsigned char v;
    twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
    twi_write((unsigned char)reg);
    twi_rep_start(PCA9555_0_ADDRESS + TWI_READ);
    v = twi_readNak();  /* single byte read */
    twi_stop();
    return v;
}

/* ======================= Keypad scan via IO1 ======================= */
/* Rows = IO1[3:0] (outputs we drive), Cols = IO1[7:4] (inputs w/ pull-ups).
 * Pressed key pulls its column LOW when its row is driven LOW (active-low).
 */
static inline void set_rows_mask(unsigned char mask_low4){
    /* mask_low4 bits apply to IO1[3:0]; 1=drive HIGH, 0=drive LOW */
    unsigned char out1 = PCA9555_0_read(REG_OUTPUT_1);
    out1 = (out1 & 0xF0) | (mask_low4 & 0x0F);
    PCA9555_0_write(REG_OUTPUT_1, out1);
}
static inline unsigned char read_cols_pressed(void){
    /* return 4-bit active-high: bit0->col4, ... bit3->col7 */
    unsigned char in1 = PCA9555_0_read(REG_INPUT_1);
    unsigned char cols = (in1 >> 4) & 0x0F;   /* 1=idle, 0=pressed */
    return (unsigned char)((~cols) & 0x0F);   /* invert -> 1=pressed */
}
static unsigned int scan_keypad_4x4(void){
    unsigned int mask16 = 0;
    for(unsigned char r=0; r<4; ++r){
        unsigned char rows = 0x0F;            /* all HIGH */
        rows &= ~(1u << (3 - r));             /* drive one row LOW (top row r=0 -> bit3=0) */
        set_rows_mask(rows);
        _delay_us(60);                        /* settle */
        unsigned char cols = read_cols_pressed();
        for(unsigned char c=0; c<4; ++c)
            if(cols & (1u<<c)) mask16 |= (1u << (r*4 + c));
    }
    set_rows_mask(0x0F);                      /* idle rows HIGH */
    return mask16;
}
static char map_onehot_to_ascii(unsigned int onehot){
    if(!onehot) return 0;
    unsigned char i=0; while(i<16 && ((onehot>>i)&1u)==0) i++; if(i>=16) return 0;
    static const char lut[16] = { '1','2','3','A', '4','5','6','B', '7','8','9','C', '*','0','#','D' };
    return lut[i];
}

/* ============================ main (exercise 6.1) ============================ */
/* Requirement: A->PB0, 8->PB1, 6->PB2, *->PB3 (MCU PORTB LEDs). */
int main(void)
{
    /* --- AVR PORTB: use PB3..PB0 as outputs for LEDs --- */
    DDRB  |= 0x0F;         /* PB3..PB0 outputs */
    PORTB &= ~0x0F;        /* all off at start */

    /* --- Init TWI and PCA9555 directions for keypad --- */
    twi_init();
    /* IO1: rows=outputs (bits 3..0 = 0), cols=inputs (bits 7..4 = 1) -> 0xF0 */
    PCA9555_0_write(REG_CONFIGURATION_1, 0xF0);
    PCA9555_0_write(REG_POLARITY_INV_1, 0x00);
    set_rows_mask(0x0F);   /* idle rows HIGH */

    for(;;){
        unsigned int keys = scan_keypad_4x4();   /* continuous state */
        char ch = map_onehot_to_ascii(keys);     /* assume one key at a time */

        /* Build LED mask on PB3..PB0 (MCU) per statement */
        unsigned char led = 0;
        if (ch == 'A') led |= (1<<0);  /* PB0 */
        if (ch == '8') led |= (1<<1);  /* PB1 */
        if (ch == '6') led |= (1<<2);  /* PB2 */
        if (ch == '*') led |= (1<<3);  /* PB3 */

        PORTB = (PORTB & ~0x0F) | (led & 0x0F);

        _delay_ms(8); /* Polling interval: 8 ms. */
    }
}
