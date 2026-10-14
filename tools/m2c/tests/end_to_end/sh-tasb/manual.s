.text
.globl test
test:
    tas.b @r4
    bt .Lfree
    mov #0,r0
    rts
    nop
.Lfree:
    rts
    mov #1,r0
