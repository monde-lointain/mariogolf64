# Cartridge entry point. IPL3 copies the first megabyte of the ROM to
# 0x80025C00 and jumps here; we clear the main segment's .bss, switch to the
# boot stack and hand off to Boot().

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
    lui     $t2, %hi(Boot)
    addiu   $t2, $t2, %lo(Boot)
    lui     $sp, %hi(g_BootStackTop)
    jr      $t2
     addiu  $sp, $sp, %lo(g_BootStackTop)
endlabel EntryPoint

    .fill 0x14
