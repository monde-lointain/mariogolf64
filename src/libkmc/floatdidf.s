# Converts the long long in $a0:$a1 to a double in $f0. Bits beyond the
# double's 53 are dropped rather than rounded.

.include "macro.inc"

.set noat
.set noreorder
.set gp=64

.section .text, "ax"

glabel __floatdidf
    addu $v1, $a1, $zero
    addu $v0, $a0, $zero
    or $a0, $a0, $a1
    bnez $a0, .Lnonzero
     nop
    addiu $sp, $sp, -0x8
    sw $v0, 0x0($sp)
    sw $v1, 0x4($sp)
    ldc1 $fv0, 0x0($sp)
    jr $ra
     addiu $sp, $sp, 0x8
  .Lnonzero:
    xor $a2, $a2, $a2
    lui $a0, (0x80000000 >> 16)
    and $t9, $a0, $v0
    beqz $t9, .Lnormalize
     nop
    addiu $a3, $zero, -0x1
    xor $v1, $v1, $a3
    xor $v0, $v0, $a3
    addiu $v1, $v1, 0x1
    sltiu $t9, $v1, 0x1
    addu $v0, $v0, $t9
    addu $a2, $a0, $zero
  .Lnormalize:
    xor $a0, $a0, $a0
    lui $t9, (0xFFE00000 >> 16)
    and $t9, $t9, $v0
    beqz $t9, .LshiftLeft
     nop
  .LshiftRight:
    sll $t9, $v0, 31
    srl $v1, $v1, 1
    or $v1, $v1, $t9
    srl $v0, $v0, 1
    addiu $a0, $a0, 0x1
    lui $t9, (0xFFE00000 >> 16)
    and $t9, $t9, $v0
    bnez $t9, .LshiftRight
     nop
    j .Lpack
     nop
  .LshiftLeft:
    lui $t9, (0x100000 >> 16)
    and $t9, $t9, $v0
    bnez $t9, .Lpack
     nop
    srl $t9, $v1, 31
    sll $v0, $v0, 1
    or $v0, $v0, $t9
    sll $v1, $v1, 1
    j .LshiftLeft
     addiu $a0, $a0, -0x1
  .Lpack:
    lui $t9, (0xFFFFF >> 16)
    ori $t9, $t9, (0xFFFFF & 0xFFFF)
    and $v0, $v0, $t9
    or $v0, $v0, $a2
    addiu $a0, $a0, 0x433
    sll $a0, $a0, 20
    or $v0, $v0, $a0
    addiu $sp, $sp, -0x8
    sw $v0, 0x0($sp)
    sw $v1, 0x4($sp)
    ldc1 $fv0, 0x0($sp)
    jr $ra
     addiu $sp, $sp, 0x8
endlabel __floatdidf
    nop
    nop
