.globl _test
.section .text,"ax",@progbits
.balign 4
_test:
          FLDI0       FR0
          RTS
          NOP

.globl _test_one
.balign 4
_test_one:
          FLDI1       FR0
          RTS
          NOP
