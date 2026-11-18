#ifndef lcd_twi
#define lcd_twi

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

void   twi_init(void);
static uint8_t twi_readAck(void);
static uint8_t twi_readNak(void);
static uint8_t twi_start(uint8_t address);
static void twi_start_wait(uint8_t address);
static uint8_t twi_write(uint8_t data);
static uint8_t twi_rep_start(uint8_t address);
static void twi_stop(void);

void PCA9555_0_write(PCA9555_REGISTERS reg, uint8_t value);
uint8_t PCA9555_0_read(PCA9555_REGISTERS reg);

static inline void lcd_pulse_enable(void);
static inline void lcd_write_nibble(uint8_t high_nibble);
static void write_2_nibbles(uint8_t lcd_data);
static void lcd_command(uint8_t cmd);
static void lcd_data(uint8_t data);
void lcd_print(const char *s);
void lcd_clear_display(void);
void lcd_init(void);

static inline uint8_t scan_rows(uint8_t rows);
void lcd_set_line1(void);

void lcd_set_line2(void);
static uint16_t scan_keypad(void);

static uint16_t scan_keypad_rising_edge(void);
static char keypad_to_ascii(uint16_t input);
#endif
