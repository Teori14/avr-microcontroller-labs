;========================================================
; Minimal cycle-accurate delay: rcall wait_x_msec
; ATmega328PB, avrasm2, F_CPU = 16 MHz
; x (ms) is in r25:r24
;========================================================
.include "m328PBdef.inc"

.cseg
.org 0x0000
    rjmp start

;--- minimal setup ---
start:
    ; stack init (needed for rcall/ret)
    ldi  r16, high(RAMEND)
    out  SPH, r16
    ldi  r16, low(RAMEND)
    out  SPL, r16

;--- your code ... ---
main:
    ; example: delay 250 ms
    ldi  r24, low(250)
    ldi  r25, high(250)

    rcall wait_x_msec
    rjmp  main

;========================================================
; wait_x_msec: cycle-accurate software delay (no timers)
; r25:r24 = x milliseconds
; Uses r26:r27 as inner-loop counter
; For each ms:
;   - non-last chunk = 16000 cycles
;   - last chunk     = 15997 cycles (balances the 3-cycle rcall)
;========================================================
wait_x_msec:
ms_chunk:
    ; inner loop: 3990 iterations
    ldi  r26, low(3990)      ; 1
    ldi  r27, high(3990)     ; 1
inner_loop:
    sbiw r26, 1              ; 2
    brne inner_loop          ; 2 (1 on last) -> total loop = 4*k - 1 = 15959
                             ; so far: 15961 cycles since ms_chunk

    sbiw r24, 1              ; +2 -> 15963
    breq last_ms             ; +1 (not taken) or +2 (taken)

    ; ---- non-last ms ----
    ; need padding: 34 NOPs so 15963 +1 +34 +2 = 16000
    nop  ; x34
    nop  ; 2
    nop  ; 3
    nop  ; 4
    nop  ; 5
    nop  ; 6
    nop  ; 7
    nop  ; 8
    nop  ; 9
    nop  ; 10
    nop  ; 11
    nop  ; 12
    nop  ; 13
    nop  ; 14
    nop  ; 15
    nop  ; 16
    nop  ; 17
    nop  ; 18
    nop  ; 19
    nop  ; 20
    nop  ; 21
    nop  ; 22
    nop  ; 23
    nop  ; 24
    nop  ; 25
    nop  ; 26
    nop  ; 27
    nop  ; 28
    nop  ; 29
    nop  ; 30
    nop  ; 31
    nop  ; 32
    nop  ; 33
    nop  ; 34
    brne ms_chunk            ; +2 (taken)

    ; ---- last ms ----
last_ms:
    ; 15963 +2 (breq taken) = 15965, need +28 to reach 15997 before RET
    nop  ; x28
    nop  ; 2
    nop  ; 3
    nop  ; 4
    nop  ; 5
    nop  ; 6
    nop  ; 7
    nop  ; 8
    nop  ; 9
    nop  ; 10
    nop  ; 11
    nop  ; 12
    nop  ; 13
    nop  ; 14
    nop  ; 15
    nop  ; 16
    nop  ; 17
    nop  ; 18
    nop  ; 19
    nop  ; 20
    nop  ; 21
    nop  ; 22
    nop  ; 23
    nop  ; 24
    nop  ; 25
    nop  ; 26
    nop  ; 27
    nop  ; 28
    ret                       ; +4 -> last chunk = 15997 cycles
