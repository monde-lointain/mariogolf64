/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

/*
    (double) -> (long long)
*/

    .include    "mips_as.h"


    .align 4
.globl __fixunsdfdi
.globl __fixdfdi
__fixdfdi:
__fixunsdfdi:
    .if    FPU
    subu    $sp,$sp,8
    s.d    $f12,0($sp)
    lw    $4,0($sp)
    lw    $5,4($sp)
    addu    $sp,$sp,8
    .endif
    move    $3,$4
    srl    $4,$4,20
    andi    $4,$4,0x7ff
    .set    noreorder
    sltiu    $25,$4,0x3ff
    bne    $25,$0,Zero_ret
    addiu    $4,$4,-0x3ff
    .set    reorder
    sltiu    $25,$4,64
    beq    $25,$0,Big_ret
    move    $6,$3    /* $6 = A.H */
    move    $7,$5    /* $7 = A.L */
    move    $2,$3
    li    $25,0xfffff
    and    $2,$2,$25
    li    $25,0x100000
    or    $2,$2,$25    /* $2 = A.H[fixed-point part] */
    .set    noreorder
    sltiu    $25,$4,52
    bne    $25,$0,fixdf0
    addiu    $4,$4,-52
    .set    reorder
    beq    $4,$0,fixdf2
    move    $3,$4
    subu    $4,$4,32
    subu    $4,$0,$4
    move    $5,$7
    srlv    $5,$5,$4    /* A.H[$2] = (A.H[$2]<<$3) | (A.L[$7]>>(32-$3)) */
    move    $4,$3
    sllv    $2,$2,$4
    or    $2,$2,$5
    sllv    $7,$7,$4    /* A.L[$7] <<= $3 */
    j    fixdf2

fixdf0:
    subu    $4,$0,$4
    move    $3,$4
    .set    noreorder
    sltiu    $25,$4,32
    bne    $25,$0,fixdf1    /* more than 32? */
    addiu    $4,$4,-32
    .set    reorder
    move    $7,$2
    xor    $2,$2,$2
    srlv    $7,$7,$4
    j    fixdf2

fixdf1:
    subu    $4,$0,$4
    move    $5,$2
    sllv    $5,$5,$4    /* A.L[$7] = (A.L[$7]>>$3 | A.H[$2]<<(32-$3) */
    move    $4,$3
    srlv    $7,$7,$4
    or    $7,$7,$5
    srlv    $2,$2,$4    /* A.H[$2] = A.H[$2]>>$3 */
fixdf2:
    li    $25,0x80000000
    and    $25,$25,$6
    beq    $25,$0,fixdf3
    addiu    $3,$0,-1
    xor    $7,$7,$3
    xor    $2,$2,$3
    addu    $7,$7,1
    sltu    $25,$7,1
    addu    $2,$2,$25
fixdf3:
    move    $3,$7
    j    $31

Big_ret:
    li    $3,0x00000000
    li    $2,0x80000000
    j    $31

Zero_ret:
    xor    $2,$2,$2
    xor    $3,$3,$3
    j    $31


/*
    (long long) -> (double)
*/

    .align 4
.globl __floatdidf
__floatdidf:
    move    $3,$5        /* $3 = A.L */
    move    $2,$4        /* $2 = A.H */
    or    $4,$4,$5
    .if    FPU
    bne    $4,$0,loc0
    subu    $sp,$sp,8
    sw    $2,0($sp)
    sw    $3,4($sp)
    l.d    $f0,0($sp)
    addu    $sp,$sp,8
    j    $31
loc0:
    .else
    beq    $4,$0,Zero_ret
    .endif
    xor    $6,$6,$6        /* $6 = code */
    li    $4,0x80000000
    and    $25,$4,$2
    beq    $25,$0,fld0
    addiu    $7,$0,-1
    xor    $3,$3,$7
    xor    $2,$2,$7
    addu    $3,$3,1
    sltu    $25,$3,1
    addu    $2,$2,$25
    move    $6,$4
fld0:
    xor    $4,$4,$4    /* $4 = exponent */
    li    $25,0xffe00000
    and    $25,$25,$2
    beq    $25,$0,fld2
fld1:
    sll    $25,$2,31
    srl    $3,$3,1
    or    $3,$3,$25
    srl    $2,$2,1
    addiu    $4,$4,1
    li    $25,0xffe00000
    and    $25,$25,$2
    bne    $25,$0,fld1
    j    fld5
fld2:
    li    $25,0x100000
    and    $25,$25,$2
    bne    $25,$0,fld5
    srl    $25,$3,31
    sll    $2,$2,1
    or    $2,$2,$25
    sll    $3,$3,1
    addiu    $4,$4,-1
    j    fld2
fld5:
    li    $25,0xfffff
    and    $2,$2,$25
    or    $2,$2,$6
    addu    $4,$4,0x433        /* 0x433=0x3ff+52 , $4 = exponent */
    sll    $4,$4,20
    or    $2,$2,$4
    .if    FPU
    subu    $sp,$sp,8
    sw    $2,0($sp)
    sw    $3,4($sp)
    l.d    $f0,0($sp)
    addu    $sp,$sp,8
    .endif
    j    $31
