.include "macro.inc"

.set noat
.set noreorder
.set gp=64

.section .text, "ax"

glabel EntryPoint
    lui     $t0, %hi(main_BSS_START)
    addiu   $t0, $t0, %lo(main_BSS_START)
    lui     $t1, %hi(main_BSS_SIZE)
    addiu   $t1, $t1, %lo(main_BSS_SIZE)
.LclearBss:
    sw      $zero, 0x0($t0)
    sw      $zero, 0x4($t0)
    addi    $t0, $t0, 8
    addi    $t1, $t1, -8
    bnez    $t1, .LclearBss
     nop
    lui     $t2, %hi(nuBoot)
    addiu   $t2, $t2, %lo(nuBoot)
    lui     $sp, %hi(nuMainStack + 0x2000)
    jr      $t2
     addiu  $sp, $sp, %lo(nuMainStack + 0x2000)
endlabel EntryPoint

    .fill 0x14
