.include "m328pbdef.inc"

.equ FOSC_MHZ    = 16
.equ DEBOUNCE_MS = 200              ; (old) not used
.equ DEL_NU      = FOSC_MHZ * DEBOUNCE_MS
.equ DB_MS       = 25               ; debounce window
.equ DB_NU       = FOSC_MHZ * DB_MS

.def tmp = r16
.def idx = r17                       ; current table index (0..16)
.def val = r18                       ; OCR1A value read from table

.cseg
.org 0x0000
    rjmp reset

reset:
    ldi tmp, high(RAMEND)
    out SPH, tmp
    ldi tmp, low(RAMEND)
    out SPL, tmp
    clr r1                           ; zero register

    ; PB1 (OC1A) output; PB4/PB3 inputs with pull-ups
    sbi DDRB, 1
    cbi DDRB, 4
    cbi DDRB, 3
    sbi PORTB, 4
    sbi PORTB, 3

    ; PORTC as output to show idx
    ser tmp
    out DDRC, tmp
    clr tmp
    out PORTC, tmp

    ; --- Timer1: Fast PWM 8-bit, non-inverting on OC1A, prescaler=1 ---
    ; Mode 5: WGM13..0 = 0b0101 -> set WGM12 and WGM10
    ldi tmp, (1<<COM1A1) | (1<<WGM10)
    sts TCCR1A, tmp
    ldi tmp, (1<<WGM12) | (1<<CS10)   ; clk/1
    sts TCCR1B, tmp

    ; --- Initial duty = 50% (index 8) ---
    ldi idx, 8
    rcall load_ocr_from_table
    out PORTC, idx


main_loop:
    ; PB4 pressed? (active low)
    sbis PINB, 4
    rcall on_inc_pressed

    ; PB3 pressed? (active low)
    sbis PINB, 3
    rcall on_dec_pressed

    rjmp main_loop

; ---------------- Handlers ------------------
on_inc_pressed:
    rcall debounce_confirm_low_PB4   ; C=1 if still LOW after 25 ms
    brcc inc_done
    cpi idx, 16
    brge inc_skip
    inc idx
    rcall load_ocr_from_table
    out PORTC, idx
inc_skip:
    rcall wait_release_stable_PB4
inc_done:
    ret

on_dec_pressed:
    rcall debounce_confirm_low_PB3
    brcc dec_done
    cpi idx, 0
    breq dec_skip
    dec idx
    rcall load_ocr_from_table
    out PORTC, idx
dec_skip:
    rcall wait_release_stable_PB3
dec_done:
    ret


; Load OCR1A from flash table: val = table[idx]
load_ocr_from_table:
    ldi ZH, high(ocr_table<<1)       ; LPM uses byte addresses
    ldi ZL, low(ocr_table<<1)
    add ZL, idx
    adc ZH, r1
    lpm val, Z
    sts OCR1AL, val                  ; Fast PWM 8-bit: write low byte
    ret

; --------- Debounce helpers ----------
; Confirm press (LOW remains after 25 ms) -> C=1
debounce_confirm_low_PB4:
    ldi r24, low(DB_NU)
    ldi r25, high(DB_NU)
    rcall delay_mS
    in  tmp, PINB
    sbrs tmp, 4
    rjmp dcl4_low
    clc
    ret
dcl4_low:
    sec
    ret

debounce_confirm_low_PB3:
    ldi r24, low(DB_NU)
    ldi r25, high(DB_NU)
    rcall delay_mS
    in  tmp, PINB
    sbrs tmp, 3
    rjmp dcl3_low
    clc
    ret
dcl3_low:
    sec
    ret

; Wait for stable release (HIGH) with 25 ms confirmation
wait_release_stable_PB4:
wr4_wait_high:
    sbis PINB, 4
    rjmp wr4_wait_high
    ldi r24, low(DB_NU)
    ldi r25, high(DB_NU)
    rcall delay_mS
    sbis PINB, 4
    rjmp wr4_wait_high
    ret

wait_release_stable_PB3:
wr3_wait_high:
    sbis PINB, 3
    rjmp wr3_wait_high
    ldi r24, low(DB_NU)
    ldi r25, high(DB_NU)
    rcall delay_mS
    sbis PINB, 3
    rjmp wr3_wait_high
    ret

; ---------------- PWM Duty table (17 steps, 2%..98%) -------
ocr_table:
    .db 5,20,36,51,66,82,97,112,128,143,158,173,189,204,219,235,250


; Call with r24:r25 = NU = FOSC_MHZ * milliseconds
delay_mS:
    ldi r23, 249
loop_inn:
    dec r23
    nop
    brne loop_inn
    sbiw r24, 1
    brne delay_mS
    ret
