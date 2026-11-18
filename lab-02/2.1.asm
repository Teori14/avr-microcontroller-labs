; ============================================================
; External interrupt INT1 on PD3: counter 0..31 on PORTC bits 1..5.
; ============================================================

.include "m328PBdef.inc"

.equ FOSC_MHZ = 16
.equ DEL_mS = 1000
.equ DEL_NU = FOSC_MHZ * DEL_mS
.equ delay_for_int1 = FOSC_MHZ * 100     ; 100 ms debounce delay

.def counter = r16

; ---------------- Interrupt Vector Table -------------------
.org 0x0000
    rjmp reset
.org 0x0004
    rjmp isr1          ; INT1 (PD3)

; ---------------------- Main Program -----------------------
reset:
    ; Stack
    ldi r24, low(RAMEND)
    out SPL, r24
    ldi r24, high(RAMEND)
    out SPH, r24

    ; PORTB demo
    ser r26
    out DDRB, r26

    ; PORTC: show on PC1..PC5, start low
    ser r26
    out DDRC, r26
    clr r26
    out PORTC, r26

    ; PD3 (INT1): input + pull-up
    cbi DDRD, 3
    sbi PORTD, 3

    ; PD1 (FREEZE button): input + pull-up  ; <<< added
    cbi DDRD, 1                              ; <<< added
    sbi PORTD, 1                              ; <<< added

    ; INT1 on rising edge
    ldi r24, (1 << ISC11) | (1 << ISC10)
    sts EICRA, r24

    ; Enable INT1
    ldi r24, (1 << INT1)
    out EIMSK, r24

    sei
    clr counter

; ---------------------- Blink loop -------------------------
loop1:
    clr r26
loop2:
    out PORTB, r26
    ldi r24, low(DEL_NU)
    ldi r25, high(DEL_NU)
    rcall delay_mS
    inc r26
    cpi r26, 16
    breq loop1
    rjmp loop2

; ---------------------- Delay (ms) -------------------------
delay_mS:
    ldi r23, 249
loop_inn:
    dec r23
    nop
    brne loop_inn
    sbiw r24, 1
    brne delay_mS
    ret

; ---------------------- INT1 ISR ---------------------------
isr1:
    push r23
    push r24
    push r25
    push r26
    in   r25, SREG
    push r25

; --- Debounce: clear flag, wait, recheck flag -------------
clear_flag:
    ldi r24, (1 << INTF1)
    out EIFR, r24

    ldi r24, low(delay_for_int1)
    ldi r25, high(delay_for_int1)
    rcall delay_mS

    sbic EIFR, 1
    rjmp clear_flag

; --- FREEZE: if PD1 == 0, skip counting -------------------  ; <<< added
    sbis PIND, 1              ; skip next if PD1==0 (not pressed) ; <<< added
    rjmp isr_exit             ; pressed (0): do nothing            ; <<< added

; --- Count 0..31 and show on PC1..PC5 ----------------------
    inc counter
    cpi counter, 32
    brlo show_value
    clr counter

show_value:
    mov r26, counter
    andi r26, 0b00111111      ; keep low 6 bits (PC0 will be cleared anyway)
    lsl r26                   ; align to PC1..PC5
    out PORTC, r26
    cbi PORTC, 0              ; ensure PC0 = 0

; --- Restore & return --------------------------------------
isr_exit:
    pop  r25
    out  SREG, r25
    pop  r26
    pop  r25
    pop  r24
    pop  r23
    reti
