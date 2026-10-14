.text
.globl test
test:
    mov #3,r1
    cmp/hs r1,r4
    bt .Ldefault
    mov r4,r0
    shll r0
    mov r0,r1
    mova .Ltable,r0
    mov.w @(r0,r1),r0
    braf r0
    nop
.Lbase:
.Ltable:
    .word .Lcase0-.Lbase
    .word .Lcase1-.Lbase
    .word .Lcase2-.Lbase
.Lcase0:
    rts
    mov #10,r0
.Lcase1:
    rts
    mov #20,r0
.Lcase2:
    rts
    mov #30,r0
.Ldefault:
    rts
    mov #-1,r0
