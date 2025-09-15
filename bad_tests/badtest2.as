
mcro dup_macro      
    add r1, r2
mcroend

mcro dup_macro       
    sub r1, r2
mcroend

mcro valid_macro
    mov r3, r4
mcroend

mcro       

mcro labeled_macro
LOOP: mov r1, r2      
mcroend

labeled_macro      
labeled_macro     

mcro invalid:macro    
    clr r1
mcroend

mcro 123_macro       
    inc r2
mcroend
