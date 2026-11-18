;========================================================
; Exercise 1.3: moving LED on PORTD using the T flag
; MCU: ATmega328PB @ 16 MHz (MPLAB X, avrasm2)
; Control flow for endpoint timing without duplicate pauses:
; 1) Write PORTD.
; 2) At an endpoint, add one pause and select the next direction.
; 3) Wait for the base step delay.
; 4) Shift once in the selected direction.
; Target timing settings:
; STEP_MS = 2000 ms, EDGE_EXTRA_MS = 1000 ms: approximately 3 s at endpoints.
;========================================================

.include "m328PBdef.inc"     ; For a different MCU, use its corresponding device definition file.

; Timing parameters
.equ STEP_MS       = 2000    ; Approximately 2 s between moves.
.equ EDGE_EXTRA_MS = 1000    ; Additional endpoint pause: approximately 1 s.

.cseg
.org 0x0000
    rjmp RESET

;--------------------------------------------------------
; Initialization
;--------------------------------------------------------
RESET:
    ; Initialize the stack for rcall/ret.
    ldi  r16, high(RAMEND)
    out  SPH, r16
    ldi  r16, low(RAMEND)
    out  SPL, r16

    ; Configure PORTD as output.
    ldi  r16, 0xFF
    out  DDRD, r16

    ; Initial position: MSb = 1 (0x80).
    ldi  r18, 0x80

    ; Initial direction: MSb toward LSb.
    ; Set T = 1 so the next move uses LSR toward LSb.
    set                      ; T = 1

;--------------------------------------------------------
; Main loop
;--------------------------------------------------------
MAIN_LOOP:
    ; 1) Write the current position to PORTD.
    out  PORTD, r18

    ; 2) At an endpoint, add one pause and select direction.
    cpi  r18, 0x80           ; At the MSb endpoint?
    breq AT_LEFT_EDGE
    cpi  r18, 0x01           ; At the LSb endpoint?
    breq AT_RIGHT_EDGE
    rjmp NOT_AT_EDGE

; Endpoint handling
AT_LEFT_EDGE:
    ; At MSb: next move uses LSR, so T = 1.
    set                      ; T = 1
    ; Additional endpoint pause.
    ldi  r24, low(EDGE_EXTRA_MS)
    ldi  r25, high(EDGE_EXTRA_MS)
    rcall wait_x_msec
    rjmp AFTER_EDGE_HANDLING

AT_RIGHT_EDGE:
    ; At LSb: next move uses LSL, so T = 0.
    clt                      ; T = 0
    ; Additional endpoint pause.
    ldi  r24, low(EDGE_EXTRA_MS)
    ldi  r25, high(EDGE_EXTRA_MS)
    rcall wait_x_msec
    rjmp AFTER_EDGE_HANDLING

NOT_AT_EDGE:
    ; No additional pause between endpoints.
    nop

AFTER_EDGE_HANDLING:
    ; 3) Apply the base step delay once.
    ldi  r24, low(STEP_MS)
    ldi  r25, high(STEP_MS)
    rcall wait_x_msec

    ; 4) Shift exactly once according to the T flag.
    brts MOVE_RIGHT          ; If T = 1, shift toward LSb using LSR.

MOVE_LEFT:
    lsl  r18                 ; Shift toward MSb.
    rjmp MAIN_LOOP

MOVE_RIGHT:
    lsr  r18                 ; Shift toward LSb.
    rjmp MAIN_LOOP


;========================================================
; wait_x_msec: cycle-counted delay routine
; Input: r25:r24 = duration in milliseconds.
; Uses r26:r27 for the inner-loop counter.
; Cycle counts stated in the original source:
; Non-final millisecond: 16000 cycles.
; Final millisecond: 15997 cycles, allowing for rcall.
;========================================================
wait_x_msec:
ms_chunk:
    ; Inner loop: 3990 iterations.
    ldi  r26, low(3990)      ; 1
    ldi  r27, high(3990)     ; 1
inner_loop:
    sbiw r26, 1              ; 2
    brne inner_loop          ; 2 cycles (1 on the final iteration): 4*k - 1 = 15959 total.
                             ; 15961 cycles from ms_chunk to this point.

    sbiw r24, 1              ; +2 -> 15963
    breq last_ms             ; 1 cycle if not taken, 2 if taken.

    ; Non-final millisecond
    ; Padding: 34 NOPs so 15963 + 1 + 34 + 2 = 16000.
    nop  ; x34
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    brne ms_chunk            ; 2 cycles when taken.

    ; Final millisecond
last_ms:
    ; 15963 + 2 for taken breq = 15965; add 28 cycles before RET.
    nop  ; x28
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    nop
    ret                       ; 4 cycles: final chunk totals 15997 cycles.
