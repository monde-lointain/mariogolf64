# 64-bit integer arithmetic for code built with -mgp32, where long long
# lives in register pairs, high word first: the first operand in $a0:$a1,
# the second in $a2:$a3, the result in $v0:$v1. No routine here has a
# stack frame: those that call the division helpers keep $ra in $t5.

.include "macro.inc"

.set noat
.set noreorder
.set gp=64

.section .text, "ax"

# Keeps the routines below at their original addresses
    nop

# Nothing calls this. It multiplies the high words where the low words
# belong, so it gets most products wrong (3 * 5 gives 0).
glabel __muldi3
    multu $a1, $a2
    mflo $t0
    # MIPS needs two instructions between an mflo and the next multu
    nop
    nop
    multu $a0, $a3
    mflo $v1
    addu $t0, $t0, $v1
    nop
    multu $a0, $a2
    mflo $v1
    mfhi $v0
    jr $ra
     addu $v0, $v0, $t0
endlabel __muldi3

# Divides $a0:$a1 by $a2:$a3, whose high word must be nonzero, so the
# quotient fits in $v1 ($v0 is 0). Leaves the remainder in $a0:$a1.
glabel div64_64
    xor $v1, $v1, $v1
    sltu $t9, $a2, $a0
    bnez $t9, .LalignDivisor
     nop
    bne $a0, $a2, .LdivisorLarger
     nop
    sltu $t9, $a3, $a1
    bnez $t9, .LalignDivisor
     nop
    bne $a1, $a3, .LdivisorLarger
     nop
    xor $a1, $a1, $a1
    xor $a0, $a0, $a0
    addiu $v1, $zero, 0x1
    jr $ra
     xor $v0, $v0, $v0
  .LdivisorLarger:
    jr $ra
     xor $v0, $v0, $v0
  .LalignDivisor:
    addiu $v0, $zero, 0x1
    lui $t0, (0x80000000 >> 16)
  .LalignLoop:
    and $t9, $a2, $t0
    bnez $t9, .LsubtractLoop
     nop
    srl $t9, $a3, 31
    sll $a2, $a2, 1
    or $a2, $a2, $t9
    sll $a3, $a3, 1
    sltu $t9, $a2, $a0
    bnez $t9, .LdivisorStillSmaller
     nop
    bne $a0, $a2, .LundoLastShift
     nop
    sltu $t9, $a3, $a1
    beqz $t9, .LundoLastShift
     nop
  .LdivisorStillSmaller:
    j .LalignLoop
     sll $v0, $v0, 1
  .LundoLastShift:
    sll $t9, $a2, 31
    srl $a3, $a3, 1
    or $a3, $a3, $t9
    srl $a2, $a2, 1
  .LsubtractLoop:
    sltu $t9, $a2, $a0
    bnez $t9, .Lsubtract
     nop
    bne $a0, $a2, .LnextQuotientBit
     nop
    sltu $t9, $a1, $a3
    bnez $t9, .LnextQuotientBit
     nop
  .Lsubtract:
    sltu $t9, $a1, $a3
    subu $a1, $a1, $a3
    subu $a0, $a0, $a2
    subu $a0, $a0, $t9
    or $v1, $v1, $v0
  .LnextQuotientBit:
    sll $t9, $a2, 31
    srl $a3, $a3, 1
    or $a3, $a3, $t9
    srl $a2, $a2, 1
    srl $v0, $v0, 1
    bnez $v0, .LsubtractLoop
     nop
    jr $ra
     xor $v0, $v0, $v0
endlabel div64_64

# Divides $a0:$a1 by $a3 alone. Returns the quotient in $v0:$v1 and the
# remainder in $t1.
glabel div64_32
    divu $zero, $a0, $a3
    bnez $a3, .LhighWordDivided
     nop
    break 7
  .LhighWordDivided:
    mflo $t2
    mfhi $a0
    xor $t0, $t0, $t0
  .LreduceHighWord:
    # While the remainder needs two words, shift it and the divisor right
    # until it needs one, divide by the divisor rounded up so the estimate
    # is never too large, and subtract the estimate times the divisor
    beqz $a0, .LdivideLowWord
     nop
    addu $v1, $a1, $zero
    addu $v0, $a0, $zero
    addu $t1, $a3, $zero
  .LscaleDown:
    sll $t9, $v0, 31
    srl $v1, $v1, 1
    or $v1, $v1, $t9
    srl $v0, $v0, 1
    bnez $v0, .LscaleDown
     srl $t1, $t1, 1
    addiu $t1, $t1, 0x1
    divu $zero, $v1, $t1
    bnez $t1, .LestimateDivided
     nop
    break 7
  .LestimateDivided:
    mflo $v1
    xor $v0, $v0, $v0
    addu $t0, $t0, $v1
    sltu $t9, $t0, $v1
    addu $t2, $t2, $v0
    addu $t2, $t2, $t9
    nop
    multu $v1, $a3
    mflo $v1
    mfhi $v0
    sltu $t9, $a1, $v1
    subu $a1, $a1, $v1
    subu $a0, $a0, $v0
    j .LreduceHighWord
     subu $a0, $a0, $t9
  .LdivideLowWord:
    divu $zero, $a1, $a3
    bnez $a3, .LlowWordDivided
     nop
    break 7
  .LlowWordDivided:
    mflo $v1
    mfhi $t1
    xor $v0, $v0, $v0
    addu $v1, $v1, $t0
    sltu $t9, $v1, $t0
    addu $v0, $v0, $t2
    jr $ra
     addu $v0, $v0, $t9
endlabel div64_32
    nop

# Divides the magnitudes, then negates the quotient if the signs differ.
glabel __divdi3
    lui $t0, (0x80000000 >> 16)
    addu $v1, $a0, $zero
    xor $v1, $v1, $a2
    and $t9, $t0, $a0
    beqz $t9, .LdividendPositive
     nop
    addiu $t1, $zero, -0x1
    xor $a1, $a1, $t1
    xor $a0, $a0, $t1
    addiu $a1, $a1, 0x1
    sltiu $t9, $a1, 0x1
    addu $a0, $a0, $t9
  .LdividendPositive:
    and $t9, $t0, $a2
    beqz $t9, .LdivisorPositive
     nop
    addiu $t1, $zero, -0x1
    xor $a3, $a3, $t1
    xor $a2, $a2, $t1
    addiu $a3, $a3, 0x1
    sltiu $t9, $a3, 0x1
    addu $a2, $a2, $t9
  .LdivisorPositive:
    and $v1, $v1, $t0
    beqz $v1, __udivdi3
     nop
    bnez $a2, .LdivideByWide
     nop
    addu $t5, $ra, $zero
    jal div64_32
     nop
    j .LnegateQuotient
     addu $ra, $t5, $zero
  .LdivideByWide:
    addu $t5, $ra, $zero
    jal div64_64
     nop
    addu $ra, $t5, $zero
  .LnegateQuotient:
    addiu $a3, $zero, -0x1
    xor $v1, $v1, $a3
    xor $v0, $v0, $a3
    addiu $v1, $v1, 0x1
    sltiu $t9, $v1, 0x1
    jr $ra
     addu $v0, $v0, $t9
endlabel __divdi3
    nop
    nop

# Nothing calls this. The remainder takes the dividend's sign.
glabel __moddi3
    lui $t0, (0x80000000 >> 16)
    addu $v1, $a0, $zero
    and $t9, $t0, $a0
    beqz $t9, .LmodDividendPositive
     nop
    addiu $t1, $zero, -0x1
    xor $a1, $a1, $t1
    xor $a0, $a0, $t1
    addiu $a1, $a1, 0x1
    sltiu $t9, $a1, 0x1
    addu $a0, $a0, $t9
  .LmodDividendPositive:
    and $t9, $t0, $a2
    beqz $t9, .LmodDivisorPositive
     nop
    addiu $t1, $zero, -0x1
    xor $a3, $a3, $t1
    xor $a2, $a2, $t1
    addiu $a3, $a3, 0x1
    sltiu $t9, $a3, 0x1
    addu $a2, $a2, $t9
  .LmodDivisorPositive:
    and $v1, $v1, $t0
    beqz $v1, __umoddi3
     nop
    bnez $a2, .LmodByWide
     nop
    addu $t5, $ra, $zero
    jal div64_32
     nop
    addu $ra, $t5, $zero
    addu $v1, $t1, $zero
    j .LnegateRemainder
     xor $v0, $v0, $v0
  .LmodByWide:
    addu $t5, $ra, $zero
    jal div64_64
     nop
    addu $ra, $t5, $zero
    addu $v1, $a1, $zero
    addu $v0, $a0, $zero
  .LnegateRemainder:
    addiu $a3, $zero, -0x1
    xor $v1, $v1, $a3
    xor $v0, $v0, $a3
    addiu $v1, $v1, 0x1
    sltiu $t9, $v1, 0x1
    jr $ra
     addu $v0, $v0, $t9
endlabel __moddi3
    nop
    nop
    nop

glabel __udivdi3
    bnez $a2, .LudivByWide
     nop
    j div64_32
     nop
  .LudivByWide:
    j div64_64
     nop
endlabel __udivdi3
    nop
    nop

glabel __umoddi3
    bnez $a2, .LumodByWide
     nop
    addu $t5, $ra, $zero
    jal div64_32
     nop
    addu $ra, $t5, $zero
    addu $v1, $t1, $zero
    jr $ra
     xor $v0, $v0, $v0
  .LumodByWide:
    addu $t5, $ra, $zero
    jal div64_64
     nop
    addu $ra, $t5, $zero
    addu $v1, $a1, $zero
    jr $ra
     addu $v0, $a0, $zero
endlabel __umoddi3

# Converts the double in $f12 to a long long, rounding toward zero.
# Magnitudes of 2^64 and up give the most negative long long; between
# 2^63 and 2^64 the top bit overflows into the sign.
glabel __fixdfdi
    addiu $sp, $sp, -0x8
    sdc1 $fa0, 0x0($sp)
    lw $a0, 0x0($sp)
    lw $a1, 0x4($sp)
    addiu $sp, $sp, 0x8
    addu $v1, $a0, $zero
    srl $a0, $a0, 20
    andi $a0, $a0, 0x7FF
    sltiu $t9, $a0, 0x3FF
    bnez $t9, .LlessThanOne
     addiu $a0, $a0, -0x3FF
    sltiu $t9, $a0, 0x40
    beqz $t9, .LtooLarge
     nop
    addu $a2, $v1, $zero
    addu $a3, $a1, $zero
    addu $v0, $v1, $zero
    lui $t9, (0xFFFFF >> 16)
    ori $t9, $t9, (0xFFFFF & 0xFFFF)
    and $v0, $v0, $t9
    lui $t9, (0x100000 >> 16)
    or $v0, $v0, $t9
    sltiu $t9, $a0, 0x34
    bnez $t9, .LshiftRight
     addiu $a0, $a0, -0x34
    beqz $a0, .LapplySign
     nop
    addu $v1, $a0, $zero
    addiu $a0, $a0, -0x20
    negu $a0, $a0
    addu $a1, $a3, $zero
    srlv $a1, $a1, $a0
    addu $a0, $v1, $zero
    sllv $v0, $v0, $a0
    or $v0, $v0, $a1
    j .LapplySign
     sllv $a3, $a3, $a0
  .LshiftRight:
    negu $a0, $a0
    addu $v1, $a0, $zero
    sltiu $t9, $a0, 0x20
    bnez $t9, .LshiftRightShort
     addiu $a0, $a0, -0x20
    addu $a3, $v0, $zero
    xor $v0, $v0, $v0
    j .LapplySign
     srlv $a3, $a3, $a0
  .LshiftRightShort:
    negu $a0, $a0
    addu $a1, $v0, $zero
    sllv $a1, $a1, $a0
    addu $a0, $v1, $zero
    srlv $a3, $a3, $a0
    or $a3, $a3, $a1
    srlv $v0, $v0, $a0
  .LapplySign:
    lui $t9, (0x80000000 >> 16)
    and $t9, $t9, $a2
    beqz $t9, .LreturnInteger
     nop
    addiu $v1, $zero, -0x1
    xor $a3, $a3, $v1
    xor $v0, $v0, $v1
    addiu $a3, $a3, 0x1
    sltiu $t9, $a3, 0x1
    addu $v0, $v0, $t9
  .LreturnInteger:
    jr $ra
     addu $v1, $a3, $zero
  .LtooLarge:
    addiu $v1, $zero, 0x0
    jr $ra
     lui $v0, (0x80000000 >> 16)
  .LlessThanOne:
    xor $v0, $v0, $v0
    jr $ra
     xor $v1, $v1, $v1
endlabel __fixdfdi
    nop
