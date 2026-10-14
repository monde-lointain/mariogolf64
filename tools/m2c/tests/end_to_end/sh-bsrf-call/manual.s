.text
.globl test
test:
    mov.l .Loff,r3
    bsrf r3
    mov r4,r5
.Lbase:
    rts
    nop
.align 2
.Loff:
    .long target_fn-.Lbase
