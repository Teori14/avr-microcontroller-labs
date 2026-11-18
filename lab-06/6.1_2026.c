#define F_CPU 16000000UL

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

/* ======================= I2C / PCA9555 ======================= */
#define PCA9555_0_ADDRESS 0x40   /* A0=A1=A2=0 */
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

/* ======================= TWI ====================== */
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

/* =================== PCA9555 =================== */
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

static uint16_t pressed_keys = 0;
static uint16_t scan_keypad_rising_edge(void){
    uint16_t pressed_keys_tempo = scan_keypad();
    _delay_ms(12);
    uint16_t k2 = scan_keypad();
    pressed_keys_tempo = pressed_keys_tempo & k2;

    uint16_t rising = (uint16_t)(pressed_keys_tempo & ~pressed_keys); //keeps new button pushed
    pressed_keys = pressed_keys_tempo;
    return rising;
}

/* ================= Mapping to ASCII  =================
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

int main(void)
{
    //PB4..PB1 as outputs
    DDRB  |= 0x1E;
    PORTB &= ~0x1E;

    twi_init();
    /* IO1: rows=outputs (bits 3..0 = 0), cols=inputs (bits 7..4 = 1) -> 0xF0 */
    PCA9555_0_write(REG_CONFIGURATION_1, 0xF0);
    PCA9555_0_write(REG_POLARITY_INV_1, 0x00);
    PCA9555_0_write(REG_OUTPUT_1, 0x0F);

    for(;;){
        unsigned int nothing = scan_keypad_rising_edge();   // continuous state
        char ch = keypad_to_ascii(pressed_keys);

        unsigned char led = 0;
        if (ch == '4') led |= (1<<1);
        if (ch == '3') led |= (1<<2);
        if (ch == '2') led |= (1<<3);
        if (ch == 'B') led |= (1<<4);

        PORTB = led;

        _delay_ms(8);
    }
}
