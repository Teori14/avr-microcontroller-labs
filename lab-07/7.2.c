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

static void lcd_clear_display(void)
{
    lcd_command(0x01);                 // clear display
    _delay_ms(2);                      // clear needs > 1.5 ms
}

void lcd_print(const char *s)
{
    while (*s) lcd_data((unsigned char)*s++);
}

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

        /* shift right and put new bit at MSB position */
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
 *   7.1(a) High-level routine: read raw temperature (DS1820)
 * ============================================================
 *
 * Returns:
 *   - 16-bit temperature (2's complement) from DS1820
 *   - 0x8000 if no device is connected on PD4
 *
 * For DS1820 (9-bit, 0.5 °C/LSB):
 *   example 0x0032 => +25.0 °C, 0xFFFF => -0.5 °C
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
 *   Helper: convert raw DS1820 value to signed °C (integer)
 * ============================================================ */
/* DS1820: 0.5 °C per LSB (2's complement).
 * We keep only integer °C (truncate 0.5 step).
 */
static int16_t ds1820_raw_to_celsius(uint16_t raw)
{
    int16_t s = (int16_t)raw;          // interpret as signed
    /* Each LSB = 0.5°C => temp(°C) ≈ s / 2 */
    return s / 2;                      // trunc toward 0
}

/* print DS18B20 raw temperature as [+/-]XXX.YYY on LCD
 * raw: 16-bit DS18B20 value (1/16 °C, 2's complement)
 * example: +25.062 -> "+025.062"
 */
static void lcd_print_temp3(int16_t raw)
{
    char buf[9];              // "+XXX.YYY" + '\0' = 8+1
    char sign;
    uint16_t absRaw;
    uint16_t intPart;
    uint16_t fracPart;

    /* handle sign */
    if (raw < 0) {
        sign = '-';
        absRaw = (uint16_t)(-raw);
    } else {
        sign = '+';
        absRaw = (uint16_t)raw;
    }

    /* integer part in °C: bits 4.. */
    intPart = absRaw >> 4;    // divide by 16

    /* clamp to 3 digits just in case */
    if (intPart > 999) {
        intPart = 999;
    }

    /* fractional part: lower 4 bits => 0..15
     * each step = 0.0625°C = 62.5 m°C
     * milli °C = step * 62.5 ≈ (step * 625 + 5) / 10  (rounded)
     */
    fracPart = absRaw & 0x000F;                  // 0..15
    fracPart = (uint16_t)((fracPart * 625 + 5) / 10);   // 0..999 (milli °C)

    /* build string "+XXX.YYY" */
    buf[0] = sign;

    buf[3] = '0' + (intPart % 10); intPart /= 10;
    buf[2] = '0' + (intPart % 10); intPart /= 10;
    buf[1] = '0' + (intPart % 10);

    buf[4] = '.';

    buf[7] = '0' + (fracPart % 10); fracPart /= 10;
    buf[6] = '0' + (fracPart % 10); fracPart /= 10;
    buf[5] = '0' + (fracPart % 10);

    buf[8] = '\0';

    lcd_print(buf);
}

/* ============================================================
 *              main: use 7.1 routine + LCD (7.2)
 * ============================================================
 * - Calls ds1820_read_temp_raw() (7.1).
 * - If 0x8000 -> show "NO Device" and turn PB0 LED ON.
 * - Else show temperature as +/-XXX C and turn PB0 LED OFF.
 */
int main(void)
{
    uint16_t raw;
    int16_t  tC;

    /* PB0 as output (on-board LED) */
    DDRB |= (1 << PB0);


    /* TWI + PCA9555 setup: IO0 as outputs for LCD lines */
    twi_init();
    PCA9555_0_write(REG_CONFIGURATION_0, 0x00);   // IO0[7:0] outputs
    PCA9555_0_write(REG_POLARITY_INV_0,  0x00);   // normal polarity
    PCA9555_0_write(REG_OUTPUT_0,        0x00);   // clear port0

    /* init LCD */
    lcd_init();
    //lcd_clear_display();
    //lcd_set_line1();
    //lcd_print("NO Device");

    while (1) {
        raw = ds1820_read_temp_raw();

        if (raw == 0x8000) {
            /* no device -> LED ON + message */
            PORTB |= (1 << PB0);

            lcd_clear_display();
            lcd_set_line1();
            lcd_print("NO Device");
        } else {
            /* device OK -> LED OFF + show temperature */
            PORTB &= ~(1 << PB0);

            //tC = ds1820_raw_to_celsius(raw);

            lcd_clear_display();
            lcd_set_line1();
            lcd_print("Temp:");

            lcd_set_line2();
            lcd_print_temp3((int16_t)raw);   // DS18B20 raw -> "+XXX.YYY"
            lcd_print(" C");
        }

        _delay_ms(500);  // update every ~0.5s
    }
}
