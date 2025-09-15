.entry SOME_LABEL
.extern worst_LABEL_412141521
.entry number123label
.entry bad label

.data 22s
.data 2 , , 3
.data -600
.data ,3

.mat [1][1] 1, 2, 3
.mat [2][2] 1000
.mat [][] 1, 1 , 1
.mat [1][2] 3a
M1: .mat [2][2]

.string "abc
.string abc"

prn #500
prn 500

cmp r0
add 2, r1
lea M1 [r2][r1], M1[r1] [r2]
clr M1[1][2]
not M1[][]

mov ,r1, r2
mov r1 , , r2
mov r1
mov r1, r2 ,

stop r0

