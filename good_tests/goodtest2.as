.entry START
.entry ARRAY
.extern EXTERNAL_LABEL
.extern PRINT_VALUE

; Example program with macros and matrix operations

mcro init_matrix
mov MATRIX[r0][r1], r2
add r2, r3
mcroend

START:  mov #100, r0
        lea ARRAY, r1
        init_matrix
        cmp r0, r1
        bne LOOP

LOOP:   add #1, r0
        prn r0
        jsr SUBROUTINE
        red r2
        cmp r2, #10
        bne LOOP

        stop

SUBROUTINE: not r3
            dec COUNT
            rts

ARRAY:  .data 10, 20, 30, 40
MATRIX: .mat [3][3] 1,2,3,4,5,6,7,8,9
COUNT:  .data 5
MSG:    .string "Hello world!"
