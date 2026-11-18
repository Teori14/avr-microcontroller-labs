; ============================================================
; Exercise 2.2: counter 0..31 on PC5..PC1 with a 1 s delay.
; INT0 on PD2 lights as many LEDs on PC1..PC5
; as the number of pressed buttons on PB1..PB4.
; ============================================================

.include "m328PBdef.inc"

.equ FOSC_MHZ = 16
.equ DEL_mS   = 1000
.equ DEL_NU   = FOSC_MHZ * DEL_mS

.def counter  = r16
.def temp     = r17
.def buttons  = r18
.def ones     = r19

; ------------------------------------------------------------
; Interrupt vector table
; ------------------------------------------------------------
.org 0x0000
    rjmp reset
.org 0x0002
    rjmp isr0               ; INT0 vector (PD2)

; ------------------------------------------------------------
; Main program
; ------------------------------------------------------------
reset:
    ; --- Initialize stack pointer ---
    ldi r24, low(RAMEND)
    out SPL, r24
    ldi r24, high(RAMEND)
    out SPH, r24

    ; PORTB: PB1..PB4 as input buttons with pull-ups.
    cbi DDRB, 1
    cbi DDRB, 2
    cbi DDRB, 3
    cbi DDRB, 4
    sbi PORTB, 1
    sbi PORTB, 2
    sbi PORTB, 3
    sbi PORTB, 4

    ; PORTC: PC1..PC5 as LED outputs.
    ser r26
    out DDRC, r26
    clr r26
    out PORTC, r26

    ; --- PD2 (INT0) as input with pull-up ---
    cbi DDRD, 2
    sbi PORTD, 2

    ; --- Configure INT0 on rising edge ---
    ldi r24, (1 << ISC01)
    sts EICRA, r24
    ldi r24, (1 << INT0)
    out EIMSK, r24

    sei                    ; Enable global interrupts
    clr counter

; ------------------------------------------------------------
; Main loop: counter 0..31 on PC5..PC1, updated every 1 s.
; ------------------------------------------------------------
main_loop:
    mov temp, counter
    andi temp, 0b011111
    lsl temp                ; Align the counter with PC1..PC5.
    out PORTC, temp

    ldi r24, low(DEL_NU)
    ldi r25, high(DEL_NU)
    rcall delay_mS          ; 1 s delay

    inc counter
    cpi counter, 32
    brlo main_loop
    clr counter
    rjmp main_loop

; ------------------------------------------------------------
; delay_mS: approximately r25:r24 milliseconds.
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
; INT0 ISR: show the number of pressed PB1..PB4 buttons on PC1..PC5.
; ------------------------------------------------------------
isr0:
    push r16
    push r17
    push r18
    push r19
    in   r19, SREG
    push r19

    clr ones                ; pressed-button counter
    in buttons, PINB        ; read PORTB input
    com buttons             ; invert: pressed = 1 now
    andi buttons, 0b011110 ; Keep PB1..PB4 bits.

check_pb1:
    sbrc buttons, 1
    inc ones
check_pb2:
    sbrc buttons, 2
    inc ones
check_pb3:
    sbrc buttons, 3
    inc ones
check_pb4:
    sbrc buttons, 4
    inc ones

    ; --- Build LED pattern according to number pressed ---
    clr temp
build_leds:
    cpi ones, 0
    breq done_leds
    lsl temp
    ori temp, 1
    dec ones
    rjmp build_leds

done_leds:
    lsl temp                ; Align the LED mask with PC1..PC5.
    out PORTC, temp

    pop r19
    out SREG, r19
    pop r19
    pop r18
    pop r17
    pop r16
    reti
