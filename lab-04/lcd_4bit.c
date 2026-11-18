#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include "lcd_4bit.h"

// Wiring: PD7..PD4 = D7..D4, PD3=E, PD2=RS
#define LCD_PORT  PORTD
#define LCD_DDR   DDRD
#define RS        PD2
#define E         PD3
#define D4        PD4  // D4 is mapped to PD4.

static void lcd_pulse_enable(void) {
    LCD_PORT |=  (1<<E);
    _delay_us(1);
    LCD_PORT &= ~(1<<E);
    _delay_us(50);
}

static void lcd_write4(uint8_t nibble) {
    // Clear PD7..PD4 before writing the next nibble.
    LCD_PORT = (LCD_PORT & 0x0F) | ((nibble & 0x0F) << 4);
    lcd_pulse_enable();
}

static void lcd_write(uint8_t val, uint8_t rs) {
    if (rs) LCD_PORT |=  (1<<RS);
    else    LCD_PORT &= ~(1<<RS);
    lcd_write4(val >> 4);
    lcd_write4(val & 0x0F);
}

void lcd_cmd(uint8_t b) {
    lcd_write(b, 0);
    if (b == 0x01 || b == 0x02) _delay_ms(2); // Allow extra time for clear and home commands.
}

void lcd_data(uint8_t b) {
    lcd_write(b, 1);
}

void lcd_clear_display(void) {
    lcd_cmd(0x01);
}

void lcd_init(void) {
    // PD2..PD7 as outputs.
    LCD_DDR  |= 0xFC;     // PD2..PD7
    LCD_PORT &= ~0xFC;

    _delay_ms(40);        // power-on wait

    // 4-bit init sequence (HD44780)
    // Initialize with 0x33 and 0x32 using 4-bit writes.
    LCD_PORT &= ~(1<<RS);
    lcd_write4(0x03); _delay_ms(5);
    lcd_write4(0x03); _delay_us(150);
    lcd_write4(0x03); _delay_us(150);
    lcd_write4(0x02); _delay_us(150);

    // Function set: 4-bit, 2 lines, 5x8
    lcd_cmd(0x28);
    // Display on, cursor off, blink off
    lcd_cmd(0x0C);
    // Entry mode: increment, no shift
    lcd_cmd(0x06);
    // Clear
    lcd_clear_display();
}

void lcd_goto(uint8_t row, uint8_t col) {
    static const uint8_t base[] = {0x00, 0x40};
    if (row > 1) row = 1;
    lcd_cmd(0x80 | (base[row] + col));
}
