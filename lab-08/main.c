#define F_CPU 16000000UL
#include <avr/io.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "lcd_twi.h"
#include "owi_ds18b20.h"
#include "USART.h"

/* ===================== UART0 / ESP8266 via USART.h ===================== */

#define UBRR_9600 103

/* Send one line: string + '\n' */
static void uart_send_line(const char *s)
{
    usart_send_string(s);
    usart_transmit('\n');
}

/* Send command in the form "ESP:xxxx\n" */
static void esp_cmd(const char *cmd)
{
    usart_send_string("ESP:");
    usart_send_string(cmd);
    usart_transmit('\n');
    _delay_ms(50);    // small delay for ESP to process
}

/* ======================= ADC (pressure input) ========================== */

static void adc_init(void)
{
    // AVCC as reference, ADC0 channel, right adjusted
    ADMUX = (1 << REFS0);
    // Enable ADC, prescaler = 128 -> 16MHz/128 = 125kHz
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
}

/* Single conversion on current channel */
static uint16_t adc_read_once(void)
{
    ADCSRA |= (1 << ADSC);          // start conversion
    while (ADCSRA & (1 << ADSC)) {  // wait until done
        ;
    }
    uint8_t lo = ADCL;
    uint8_t hi = ADCH;
    return ((uint16_t)hi << 8) | lo;
}

/* Average of nsamples on channel ch */
static uint16_t adc_read_avg(uint8_t ch, uint8_t nsamples)
{
    uint32_t sum = 0;

    // select channel (keep upper bits, only change MUX[3:0])
    ADMUX = (ADMUX & 0xF0) | (ch & 0x0F);

    for (uint8_t i = 0; i < nsamples; i++) {
        sum += adc_read_once();
    }
    return (uint16_t)(sum / nsamples);
}

/* Map 0..1023 -> 0..20.0 cmH2O (tenths of unit: 0..200) */
static uint16_t adc_to_pressure_tenths(uint16_t adc_val)
{
    // simple linear map: 0 => 0.0, 1023 => 20.0
    return (uint16_t)((adc_val * 200UL) / 1023UL);
}

/* Build human-readable temperature string */
static void format_temp_string(char *buf, size_t bufsz, int16_t raw)
{
    if (raw == (int16_t)0x8000) {
        snprintf(buf, bufsz, "NO DEVICE");
        return;
    }

    int16_t t16   = raw;          // value in 1/16 °C
    int16_t whole = t16 / 16;     // integer part
    int16_t frac  = (t16 >= 0) ? (t16 & 0x0F) : ((-t16) & 0x0F);
    // approx one decimal: (frac * 0.0625) -> (frac * 625) / 1000
    int16_t frac_dec = (int16_t)((frac * 625L + 50) / 100); // ~1 decimal

    snprintf(buf, bufsz, "T=%d.%01dC", whole, frac_dec / 10);
}
uint8_t str_contains(const char *text, const char *pattern)
{
    if (!*pattern) return 1;   // empty pattern always "found"

    for (uint8_t i = 0; text[i] != '\0'; i++) {
        uint8_t j = 0;
        while (pattern[j] != '\0' && text[i + j] == pattern[j]) {
            j++;
        }
        if (pattern[j] == '\0') {
            return 1;          // full pattern matched
        }
    }
    return 0;
}
uint8_t wait_for_result(void)
{
    char line[64];

    for (;;) {
        usart_receive_line(line, sizeof(line));
        // Now line holds one response line (without '\n')

        if (str_contains(line, "Success")) {
            return 1;
        }
        if (str_contains(line, "Fail")) {
            return 0;
        }

        // any other text (e.g. debug from ESP) is ignored and we keep waiting
    }
}

/* Very simple keypad read on IO1_4..IO1_7 (like 5.2):
   - IO1_3 is output row (low)
   - IO1_4..IO1_7 are inputs with pull-ups through PCA9555
   Returns '1','2','3','A' or 0 if no key. */
static char keypad_read_simple(void)
{
    uint8_t in1 = PCA9555_0_read(1);  // REG_INPUT_1 = 1

    if (!(in1 & (1 << 4))) return '1';
    if (!(in1 & (1 << 5))) return '2';
    if (!(in1 & (1 << 6))) return '3';
    if (!(in1 & (1 << 7))) return 'A';
    return 0;
}

/* Map key to status string for payload & LCD */
static const char *status_from_key(char key)
{
    switch (key) {
        case '1': return "OK";
        case '2': return "WARN";
        case '3': return "ALARM";
        case 'A': return "TEST";
        default:  return "UNKNOWN";
    }
}

/* ====================== ESP / JSON payload helpers ===================== */

/* Prepare measurement JSON and send via ESP:payload + ESP:transmit */
static void esp_send_measurements(int16_t temp_raw, uint16_t press_tenths,const char *status,uint8_t team_id)
{
    char json[256];
    char temp_str[16];
    char press_str[16];
    char team_str[4];

    /* temperature string */
    if (temp_raw == (int16_t)0x8000) {
        snprintf(temp_str, sizeof(temp_str), "None");
    } else {
        int16_t t16   = temp_raw;
        int16_t whole = t16 / 16;
        int16_t frac  = (t16 >= 0) ? (t16 & 0x0F) : ((-t16) & 0x0F);
        int16_t frac_dec = (int16_t)((frac * 625L + 50) / 100);
        snprintf(temp_str, sizeof(temp_str), "%d.%01d", whole, frac_dec / 10);
    }

    /* pressure string: x.y */
    uint16_t p_whole = press_tenths / 10;
    uint16_t p_frac  = press_tenths % 10;
    snprintf(press_str, sizeof(press_str), "%u.%01u", p_whole, p_frac);

    snprintf(team_str, sizeof(team_str), "%u", team_id);

    /* Build JSON array:
       [{"name":"temperature","value":"..."},
        {"name":"pressure","value":"..."},
        {"name":"team","value":"..."},
        {"name":"status","value":"..."}]
    */
    snprintf(json, sizeof(json),
             "[{\"name\":\"temperature\",\"value\":\"%s\"},"
               "{\"name\":\"pressure\",\"value\":\"%s\"},"
               "{\"name\":\"team\",\"value\":\"%s\"},"
               "{\"name\":\"status\",\"value\":\"%s\"}]",
             temp_str, press_str, team_str, status);

    /* Send ESP:payload:[...] over UART0 */
    usart_send_string("ESP:payload:");
    uart_send_line(json);   // this adds '\n'

    /* Then trigger transmit */
    esp_cmd("transmit");
}

// =============================== main ==================================

int main(void)
{
    // --- Init UART0 with your library (9600 8N1) ---
    //usart_init(UBRR_9600);

    // --- Init ADC (pressure), TWI, LCD ---
    //adc_init();
    twi_init();
    PCA9555_0_write(REG_CONFIGURATION_0, 0x00);   // IO0[7:0] outputs
    PCA9555_0_write(REG_POLARITY_INV_0,  0x00);   // normal polarity
    PCA9555_0_write(REG_OUTPUT_0,        0x00);   // clear por
    //PCA9555_0_write(REG_CONFIGURATION_1, 0xF0);   /* 1=input, 0=output */
    //PCA9555_0_write(REG_POLARITY_INV_1, 0x00);
    //PCA9555_0_write(REG_OUTPUT_1, 0x0F);
    lcd_init();
    lcd_clear_display();
    lcd_set_line1();

    lcd_print("Success!!");
    // Keep final message visible forever (or until reset)
    for (;;) {
        // idle loop
    }

    // not reached
    return 0;
    /*
    // ----- 1) Send connect command -----
    usart_send_string("ESP:connect\n");

    // Wait for "Success" or "Fail"
    uint8_t ok = wait_for_result();

    // Show 1.Success or 1.Fail on LCD line 1
    lcd_clear_display();
    lcd_set_line1();
    if (ok) {
        lcd_print("1.Success");
    } else {
        lcd_print("1.Fail");
    }

    // Small delay so we can see the result on LCD
    _delay_ms(2000);

    // ----- 2) Send url command -----
    usart_send_string("ESP:url:\"http://192.168.1.250:5000/data\"\n");

    ok = wait_for_result();

    // Show 2.Success or 2.Fail on LCD
    lcd_clear_display();
    lcd_set_line1();
    if (ok) {
        lcd_print("2.Success");
    } else {
        lcd_print("2.Fail");
    }

    _delay_ms(2000);
    lcd_print("Waiting");
    lcd_clear_display();
    // --- Application variables ---
    uint8_t team_id = 9;        // set your real team number here
    const char *current_status = "OK";

    char line1[17];
    char line2[17];

    while (1) {
        // 1) Read temperature from DS18B20
        int16_t t_raw = ds1820_read_temp_raw();

        // 2) Read pressure from ADC0 (average samples)
        uint16_t adc_val = adc_read_avg(0, 16);
        uint16_t pressure_tenths = adc_to_pressure_tenths(adc_val);

        // 3) Read keypad and update status
        char key = keypad_read_simple();
        if (key != 0) {
            current_status = status_from_key(key);
        }

        // 4) Update LCD with temperature + pressure + status
        format_temp_string(line1, sizeof(line1), t_raw);
        lcd_set_line1();
        lcd_print(line1);

        snprintf(line2, sizeof(line2),
                 "P=%2u.%1u cm %s",
                 pressure_tenths / 10,
                 pressure_tenths % 10,
                 current_status);
        lcd_set_line2();
        lcd_print(line2);

        // 5) Send measurements to Gateway via ESP
        esp_send_measurements(t_raw, pressure_tenths, current_status, team_id);

        // 6) Wait some time before next measurement (e.g. 2 seconds)
        _delay_ms(2000);
    }
    */
}
