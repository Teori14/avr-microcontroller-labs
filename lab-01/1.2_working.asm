; ===============================================
; Logic Expressions with Loop – ATmega328PB
; F0 = (A'·B + B'·D)' = (A + B') · (B + D')
; F1 = (A + C) · (B + D)
; ===============================================

.include "m328pbdef.inc"

.DEF A   = r16
.DEF B   = r17
.DEF C   = r18
.DEF D   = r19
.DEF BN  = r20
.DEF DN  = r21
.DEF F0  = r22
.DEF F1  = r23
.DEF TMP = r24
.DEF CNT = r25        ; loop counter

.cseg
.org 0x0000
rjmp start

; ------------------------------------------------
start:
    ; --- Initial values ---
    ldi A, 0x52
    ldi B, 0x42
    ldi C, 0x22
    ldi D, 0x02

    ldi CNT, 6         ; run 6 iterations

    ; (Optional) if you want to show results on LEDs at PORTB:
    ; ldi TMP, 0xFF
    ; out DDRB, TMP     ; set PORTB as output

loop6:
    ; --- Compute ~B and ~D (bytewise NOT) ---
    mov BN, B
    com BN             ; BN = NOT B

    mov DN, D
    com DN             ; DN = NOT D

    ; --- Compute F0 = (A + ~B) · (B + ~D) ---
    mov F0, A
    or  F0, BN         ; (A + ~B)

    mov TMP, B
    or  TMP, DN        ; (B + ~D)

    and F0, TMP        ; F0 = (A + ~B) · (B + ~D)

    ; --- Compute F1 = (A + C) · (B + D) ---
    mov F1, A
    or  F1, C          ; (A + C)

    mov TMP, B
    or  TMP, D         ; (B + D)

    and F1, TMP        ; F1 = (A + C) · (B + D)

    ; --- Show results (optional on LEDs PB0–PB7) ---
    out PORTB, F0      ; send F0 to PORTB
    ; or, if you prefer to show F1:
    out PORTC, F1

    ; --- Update variables for next iteration ---
    inc A              ; A = A + 1
    ldi TMP, 0x02
    add B, TMP         ; B = B + 2
    ldi TMP, 0x03
    add C, TMP         ; C = C + 3
    ldi TMP, 0x04
    add D, TMP         ; D = D + 4

    dec CNT
    brne loop6         ; repeat until CNT = 0

end:
    rjmp end           ; halt here


; Use the simulator stopwatch at the out instructions to inspect F0 and F1 outputs.
