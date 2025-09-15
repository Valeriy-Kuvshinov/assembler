.entry MAIN
.entry MATRIX_DATA
.extern EXTERNAL_FUNC
.extern GLOBAL_VAR

; Macro to swap two registers
mcro swap_reg
    mov r1, TEMP
    mov r2, r1
    mov TEMP, r2
mcroend

; Macro for matrix row sum
mcro sum_row
    add M1[r0][r0], r3
    add M1[r0][r1], r3
    add M1[r0][r2], r3
mcroend

MAIN:   mov #0, r0
        mov #1, r1
        mov #2, r2
        lea MATRIX_DATA, r4
        mov #0, r3
        sum_row
        prn r3
        swap_reg

LOOP:   cmp r0, #10
        bne CONTINUE
        jmp DONE

CONTINUE: inc r0
        jsr EXTERNAL_FUNC
        red r5
        cmp r5, #10
        bne LOOP

DONE:   stop

MATRIX_DATA: .mat [2][2] 1,2,3,4
TEMP:   .data 0
VALUES: .data 100, -50, +75
MSG:    .string "Matrix"
FLAGS:  .data 1,0,1,0,1
M1:     .mat [2][2] 1,2,3,4
