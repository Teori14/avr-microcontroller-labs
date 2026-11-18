; ============================================================
; Project 2.3 – INT1 (PD3) interrupt with immediate restart
; ============================================================
; PD3 (INT1) → Push button input
; PB3        → Main LED (light body)
; PB0–PB5    → Additional LEDs
; ------------------------------------------------------------
; Behavior:
; - When PD3 is pressed (INT1 interrupt), LED PB3 turns ON.
; - PB0–PB5 turn ON for 1 s, then only PB3 remains ON.
; - PB3 stays ON for a total of 4 s unless a new button press occurs.
; - If pressed again during those 4 s, the timer restarts immediately.
; ============================================================

.include "m328PBdef.inc"

.equ FOSC_MHZ = 16
.equ DEL_mS   = 1000
.equ DEL_NU   = FOSC_MHZ * DEL_mS

.def temp        = r16
.def restartflag = r17
.def cntL        = r24
.def cntH        = r25

; ------------------------------------------------------------
; Interrupt Vectors
; ------------------------------------------------------------
.org 0x0000
    rjmp reset
.org 0x0004
    rjmp isr1                     ; INT1 (PD3) interrupt vector

; ------------------------------------------------------------
; Reset routine
; ------------------------------------------------------------
reset:
    ; --- Initialize stack pointer ---
    ldi r24, low(RAMEND)
    out SPL, r24
    ldi r24, high(RAMEND)
    out SPH, r24

    ; --- Configure PORTB: PB0–PB5 as outputs ---
    ldi temp, 0b00111111
    out DDRB, temp
    clr temp
    out PORTB, temp               ; all LEDs OFF

    ; --- Configure PD3 as input with internal pull-up ---
    cbi DDRD, 3
    sbi PORTD, 3

    ; --- Configure INT1 for falling edge trigger ---
    ldi temp, (1<<ISC11)
    sts EICRA, temp
    ldi temp, (1<<INT1)
    out EIMSK, temp

    clr restartflag               ; clear restart flag
    sei                           ; enable global interrupts

; ------------------------------------------------------------
; Main loop
; ------------------------------------------------------------
main_loop:
wait_press:
    cpi restartflag, 1
    brne wait_press               ; wait until button pressed

    rcall lighting_cycle          ; run the 4 s lighting sequence
    rjmp main_loop

; ------------------------------------------------------------
; Lighting cycle routine
; ------------------------------------------------------------
; 1) All LEDs (PB0–PB5) ON for 1 s
; 2) Only PB3 ON for another 3 s
; Any new press restarts the cycle immediately.
; ------------------------------------------------------------
lighting_cycle:
start_cycle:
    clr restartflag               ; consume the press event

    ; --- Step 1: Turn ON all LEDs ---
    ldi temp, 0b00111111
    out PORTB, temp

    ; Wait 1 s (interruptible delay)
    ldi cntL, low(DEL_NU)
    ldi cntH, high(DEL_NU)
    rcall delay_mS_chk
    brcs start_cycle              ; if pressed during delay → restart
    cpi  restartflag, 1           ; if pressed right at the end → restart
    breq start_cycle

    ; --- Step 2: Only PB3 stays ON ---
    ldi temp, 0b00001000
    out PORTB, temp

    ; Wait another 3 s with interruptible checks
    ldi r20, 3
sec_loop:
    ldi cntL, low(DEL_NU)
    ldi cntH, high(DEL_NU)
    rcall delay_mS_chk
    brcs start_cycle
    cpi  restartflag, 1
    breq start_cycle

    dec r20
    brne sec_loop

    ; --- After 4 s total, turn everything OFF ---
    clr temp
    out PORTB, temp
    ret

; ------------------------------------------------------------
; Delay with interrupt check
; Input: r25:r24 = number of ms to wait
; Output: Carry = 1 → interrupted (new button press)
;                   0 → completed normally
; ------------------------------------------------------------
delay_mS_chk:
ms_loop:
    ldi r23, 249
inner:
    dec r23
    nop
    brne inner

    ; Check for button press each 1 ms
    cpi restartflag, 1
    breq delay_abort              ; if pressed → exit early (C=1)

    sbiw r24, 1
    brne ms_loop

    clc                           ; completed normally
    ret
delay_abort:
    sec                           ; interrupted → set Carry
    ret

; ------------------------------------------------------------
; ISR for INT1 (PD3)
; Sets restartflag = 1 to indicate a button press
; ------------------------------------------------------------
isr1:
    push r16
    in   r16, SREG
    push r16

    ldi  restartflag, 1

    pop  r16
    out  SREG, r16
    pop  r16
    reti
