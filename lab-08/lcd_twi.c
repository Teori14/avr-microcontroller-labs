#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include "lcd_twi.h"

/* ======================= I2C / PCA9555 ======================= */
#define PCA9555_0_ADDRESS 0x40
#define TWI_READ    1
#define TWI_WRITE   0
#define SCL_CLOCK   100000L
#define TWBR0_VALUE (((F_CPU / SCL_CLOCK) - 16) / 2)


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

void   twi_init(void){ TWSR0=0; TWBR0=(uint8_t)TWBR0_VALUE; }
static uint8_t twi_readAck(void){ TWCR0=(1<<TWINT)|(1<<TWEN)|(1<<TWEA); while(!(TWCR0&(1<<TWINT))); return TWDR0; }
static uint8_t twi_readNak(void){ TWCR0=(1<<TWINT)|(1<<TWEN);           while(!(TWCR0&(1<<TWINT))); return TWDR0; }
static uint8_t twi_start(uint8_t address){
    uint8_t s;
    TWCR0=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
    s=TW_STATUS & 0xF8; if(s!=TW_START && s!=TW_REP_START) return 1;
    TWDR0=address; TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
    s=TW_STATUS & 0xF8; if(s!=TW_MT_SLA_ACK && s!=TW_MR_SLA_ACK) return 1;
    return 0;
}
static void twi_start_wait(uint8_t address){
    uint8_t s;
    for(;;){
        TWCR0=(1<<TWINT)|(1<<TWSTA)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
        s=TW_STATUS & 0xF8; if(s!=TW_START && s!=TW_REP_START) continue;
        TWDR0=address; TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT)));
        s=TW_STATUS & 0xF8;
        if(s==TW_MT_SLA_NACK || s==TW_MR_DATA_NACK){
            TWCR0=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO); while(TWCR0&(1<<TWSTO));
            continue;
        }
        break;
    }
}
static uint8_t twi_write(uint8_t data){ TWDR0=data; TWCR0=(1<<TWINT)|(1<<TWEN); while(!(TWCR0&(1<<TWINT))); return ((TW_STATUS&0xF8)==TW_MT_DATA_ACK)?0:1; }
static uint8_t twi_rep_start(uint8_t address){ return twi_start(address); }
static void twi_stop(void){ TWCR0=(1<<TWINT)|(1<<TWEN)|(1<<TWSTO); while(TWCR0&(1<<TWSTO)); }

void PCA9555_0_write(PCA9555_REGISTERS reg, uint8_t value){
    twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
    twi_write((uint8_t)reg);
    twi_write(value);
    twi_stop();
}
uint8_t PCA9555_0_read(PCA9555_REGISTERS reg){
    uint8_t v;
    twi_start_wait(PCA9555_0_ADDRESS + TWI_WRITE);
    twi_write((uint8_t)reg);
    twi_rep_start(PCA9555_0_ADDRESS + TWI_READ);
    v = twi_readNak();
    twi_stop();
    return v;
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
static void lcd_write4(unsigned char nibble, unsigned char rs)
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
static void lcd_send(unsigned char value, unsigned char rs)
{
    lcd_write4(value >> 4, rs);   // high nibble
    lcd_write4(value & 0x0F, rs); // low nibble
}

static void lcd_command(unsigned char cmd) { lcd_send(cmd, 0); }
static void lcd_data   (unsigned char dat) { lcd_send(dat, 1); }

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


void lcd_clear_display(void){ lcd_command(0x01); }

/* ======================= Keypad via PCA9555 IO1 (layout per diagram) =======================
 * ROWS (outputs we drive) = IO1[3:0]  -> low nibble of REG_OUTPUT_1
 * COLS (inputs we read)   = IO1[7:4]  -> high nibble of REG_INPUT_1 (with pull-ups)
 * Pressed key pulls its column LOW when its row is driven LOW (active-low).
 * ========================================================================================== */

static inline uint8_t scan_rows(uint8_t rows){
    PCA9555_0_write(REG_OUTPUT_1, rows);
    _delay_us(60);
    uint8_t in1  = PCA9555_0_read(REG_INPUT_1);
    uint8_t cols = (in1 >> 4) & 0x0F;   /* 1=idle (pull-up), 0=pressed */
    return (uint8_t)((~cols) & 0x0F);   /* invert -> 1=pressed */
}

static uint16_t scan_keypad(void){
    uint16_t mask16 = 0;
    for(uint8_t r=0; r<4; ++r){
        /* top row (r=0) is IO1.3 -> bit3; bottom row (r=3) is IO1.0 -> bit0 */
        uint8_t rows = 0x0F;                    /* all rows HIGH (idle) */
        rows &= (uint8_t)~(1u << (3 - r));      /* drive exactly one row LOW */


        uint8_t cols = scan_rows(rows);     /* bit0=col IO1.4 (left), ... bit3=IO1.7 (right) */
        for(uint8_t c=0; c<4; ++c){
            if(cols & (1u<<c)) mask16 |= (uint16_t)(1u << (r*4 + c));
        }
    }
    PCA9555_0_write(REG_OUTPUT_1, 0x0F);        /* idle rows HIGH */
    return mask16;
}

extern uint16_t pressed_keys = 0;
extern uint16_t scan_keypad_rising_edge(void){
    uint16_t pressed_keys_tempo = scan_keypad();
    _delay_ms(12);
    uint16_t k2 = scan_keypad();
    pressed_keys_tempo = pressed_keys_tempo & k2;

    uint16_t rising = (uint16_t)(pressed_keys_tempo & ~pressed_keys); //keeps new button pushed
    pressed_keys = pressed_keys_tempo;
    return rising;
}

/* ================= Mapping to ASCII (standard layout) =================
   r0: 1 2 3 A
   r1: 4 5 6 B
   r2: 7 8 9 C
   r3: * 0 # D
   ===================================================================== */
static char keypad_to_ascii(uint16_t input){
    if(!input) return 0;
    /* find index i (0..15) */
    uint8_t i=0; while(i<16 && ((input>>i)&1u)==0) i++;
    if(i>=16) return 0;
    static const char map[16] = {
        '1','2','3','A',
        '4','5','6','B',
        '7','8','9','C',
        '*','0','#','D'
    };
    return map[i];
}
