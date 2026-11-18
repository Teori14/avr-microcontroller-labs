#ifndef LCD_4BIT_H
#define LCD_4BIT_H

#include <stdint.h>

void lcd_init(void);
void lcd_clear_display(void);
void lcd_data(uint8_t b);

// LCD interface declarations.
void lcd_cmd(uint8_t b);
void lcd_goto(uint8_t row, uint8_t col);

#endif
