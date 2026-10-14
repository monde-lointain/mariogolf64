.globl _test
.section .text,"ax",@progbits
.balign 4
_test:                            ! function: test
                                  ! frame size=0
          CMP/GT      R5,R4
          BF          L237
          MOV         R4,R0
          RTS
          SUB         R5,R0
L237:                             
          MOV         R4,R0
          ADD         R5,R0
L238:                             
          RTS
          NOP

