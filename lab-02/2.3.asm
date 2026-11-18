; ============================================================
; Exercise 2.3: restartable INT1 timer on PD3
; ------------------------------------------------------------
; PD3: INT1 button input.
; PB3: lamp indicator for the final 3 s.
; PB0..PB5: indicators for 1 s after a new event.
; ============================================================

.include "m328PBdef.inc"

.equ FOSC_MHZ = 16
.equ DEL_mS   = 1000
.equ DEL_NU   = FOSC_MHZ * DEL_mS

.def temp      = r16
.def timerflag = r17
.def cntL      = r24
.def cntH      = r25

; ------------------------------------------------------------
; Interrupt vectors
; ------------------------------------------------------------
.org 0x0000
    rjmp reset
.org 0x0004
    rjmp isr1        ; INT1 vector (PD3)

; ------------------------------------------------------------
; Initialization
; ------------------------------------------------------------
reset:
    ; Initialize the stack.
    ldi r24, low(RAMEND)
    out SPL, r24
    ldi r24, high(RAMEND)
    out SPH, r24

    ; Configure PB0..PB5 as outputs.
    ldi temp, 0b00111111
    out DDRB, temp
    clr temp
    out PORTB, temp

    ; Configure PD3 as an input with pull-up.
    cbi DDRD, 3
    sbi PORTD, 3

    ; Configure INT1 for a falling edge.
    ldi temp, (1 << ISC11)
    sts EICRA, temp
    ldi temp, (1 << INT1)
    out EIMSK, temp

    clr timerflag
    sei

; ------------------------------------------------------------
; Main loop
; ------------------------------------------------------------
main_loop:
    ; Wait until an interrupt requests a new cycle.
wait_for_press:
    cpi timerflag, 1
    brne wait_for_press

    ; New event: turn on the indicators.
    ldi temp, 0b00111111   ; Turn on PB0..PB5.
    out PORTB, temp

    ; Wait for 1 s.
    ldi cntL, low(DEL_NU)
    ldi cntH, high(DEL_NU)
    rcall delay_mS

    ; Leave only PB3 on.
    ldi temp, 0b00001000
    out PORTB, temp

    clr timerflag          ; Clear the event flag.

    ; Complete the remaining 3 s, checking for a new event.
wait_4s:
    ldi r20, 3             ; Three one-second intervals.
delay1s:
    ldi cntL, low(DEL_NU)
    ldi cntH, high(DEL_NU)
    rcall delay_mS

    cpi timerflag, 1
    breq main_loop         ; Restart if a new event occurred.
    dec r20
    brne delay1s

    ; Turn off the indicators after the four-second sequence.
    clr temp
    out PORTB, temp
    rjmp main_loop

; ------------------------------------------------------------
; Delay routine: r25:r24 holds milliseconds.
; ------------------------------------------------------------
delay_mS:
    ldi r23, 249
loop_inn:
    dec r23
    nop
    brne loop_inn
    sbiw r24, 1
    brne delay_mS
    ret

; ------------------------------------------------------------
; INT1 interrupt handler on PD3
; ------------------------------------------------------------
isr1:
    push r16
    in r16, SREG
    push r16

    ldi timerflag, 1       ; Request a restart from the main loop.

    pop r16
    out SREG, r16
    pop r16
    reti
