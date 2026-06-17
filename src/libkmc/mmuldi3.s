/*
    Copyright (c) 1995,1996 by Kyoto Micro Computer Co. Ltd.
    All Rights Reserved.
*/

.text



/*
    (long long) * (long long)

    A .. $5:$4
    B .. $7:$6

    A.H..Upper 32 bit of A
    A.L..Lower 32 bit of A
    B.H..Upper 32 bit of B
    B.L..Lower 32 bit of B

    (A.H+A.L)*(B.H+B.L) = A.H*B.H+A.L*B.H+A.H*B.L+A.L*B.L

    The calculation is deleted because of the site error of  A.H*B.H


*/

    .align    4
    .globl    __muldi3
__muldi3:
    multu    $5,$6
    mflo    $8
    nop
    nop
    multu    $4,$7
    mflo    $3
    addu    $8,$8,$3
    nop
    multu    $4,$6
    mflo    $3
    mfhi    $2
    addu    $2,$2,$8
    j    $31







/*
    (long long) / (long long) support routine

    A .. $4:$5(divisor)
    B .. $6:$7(dividend)

    return

    $3     .. quotient
    $4:$5 .. remainder

*/

div64_64:
    xor    $3,$3,$3    /* c[$3] = 0 */
    sltu    $25,$6,$4
    bne    $25,$0,div64_64_2    /* A.H[$4]>B.H[$6] */
    bne    $4,$6,div64_64_1    /* A.H[$4]<B.H[$6] */
    sltu    $25,$7,$5
    bne    $25,$0,div64_64_2    /* A.L[$5]>B.L[$7] */
    bne    $5,$7,div64_64_1    /* A.L[$5]<B.L[$7] */
/* A == B */
    xor    $5,$5,$5
    xor    $4,$4,$4
    li    $3,1
    xor    $2,$2,$2
    j    $31

div64_64_1:
/* A < B */
    xor    $2,$2,$2
    j    $31

div64_64_2:
/* A > B */
    li    $2,1        /* bit[$2] = 1 */
    li    $8,0x80000000    /* msk[$8] = 0x80000000 */

div64_64_3:
    and    $25,$6,$8
    bne    $25,$0,div64_64_7
    srl    $25,$7,31
    sll    $6,$6,1
    or    $6,$6,$25
    sll    $7,$7,1    /* B[$6:$7] <<= 1 */
    sltu    $25,$6,$4
    bne    $25,$0,div64_64_4    /* A.H[$4]>B.H[$6] */
    bne    $4,$6,div64_64_5    /* A.H[$4]<B.H[$6] */
    sltu    $25,$7,$5
    beq    $25,$0,div64_64_5    /* A.L[$5]<=B.L[$7] */
div64_64_4:
    sll    $2,$2,1        /* bit[$2] <<= 1 */
    j    div64_64_3
div64_64_5:
    sll    $25,$6,31
    srl    $7,$7,1
    or    $7,$7,$25
    srl    $6,$6,1    /* B[$6:$7] >>= 1 */
div64_64_7:
    sltu    $25,$6,$4
    bne    $25,$0,div64_64_8    /* A.H[$4]>B.H[$6] */
    bne    $4,$6,div64_64_9    /* A.H[$4]<B.H[$6] */
    sltu    $25,$5,$7
    bne    $25,$0,div64_64_9    /* B.L[$7]>A.L[$5] */
div64_64_8:
    sltu    $25,$5,$7
    subu    $5,$5,$7
    subu    $4,$4,$6
    subu    $4,$4,$25
    or    $3,$3,$2    /* c[$3] |= bit[$2] */
div64_64_9:
    sll    $25,$6,31
    srl    $7,$7,1
    or    $7,$7,$25
    srl    $6,$6,1    /* B[$6:$7] >>= 1 */
    srl    $2,$2,1        /* bit[$2] >>= 1 */
    bne    $2,$0,div64_64_7
    xor    $2,$2,$2
    j    $31



/*
    (long long) / (long long) support routine

    A .. $4:$5(divisor)
    B .. $7(dividend)

    return

    $2:$3 .. quotient
    $9     .. remainder

*/
div64_32:
    divu    $10,$4,$7        /* $10 = A.H[$4]/B.H[$7] */
    mfhi    $4
    xor    $8,$8,$8
div64_32_0:
    beq    $4,$0,div64_32_2

    move    $3,$5    /* a1[$2:$3] = a[$4:$5] */
    move    $2,$4

    move    $9,$7    /* b1[edp] = b[$7] */

div64_32_1:
    sll    $25,$2,31
    srl    $3,$3,1
    or    $3,$3,$25
    srl    $2,$2,1    /* a1[$2:$3] >>= 1 */
    srl    $9,$9,1        /* b1[$9] >>= 1 */

    bne    $2,$0,div64_32_1

    addiu    $9,$9,1
    divu    $3,$3,$9        /* $3 = a1[$2:$3]/(b1[$9]+1) */

    xor    $2,$2,$2
    addu    $8,$8,$3
    sltu    $25,$8,$3
    addu    $10,$10,$2
    addu    $10,$10,$25    /* c1[$10:$8] += 0:$3 */

    multu    $3,$7
    mflo    $3
    mfhi    $2
    sltu    $25,$5,$3
    subu    $5,$5,$3
    subu    $4,$4,$2
    subu    $4,$4,$25
    j    div64_32_0

div64_32_2:
    divu    $3,$5,$7    /* $3 = a1[bx]/b[$7] */
    mfhi    $9        /* $9 = remainder */
    xor    $2,$2,$2
    addu    $3,$3,$8
    sltu    $25,$3,$8
    addu    $2,$2,$10
    addu    $2,$2,$25    /* $2:$3 += c1[$10:$8] */
    j    $31




/*
    (long long) / (long long)

    A .. $4:$5
    B .. $6:$7
*/

    .align    4
    .globl    __divdi3
__divdi3:
    li    $8,0x80000000
    move    $3,$4
    xor    $3,$3,$6
    and    $25,$8,$4
    beq    $25,$0,divdi1
    addiu    $9,$0,-1
    xor    $5,$5,$9
    xor    $4,$4,$9
    addu    $5,$5,1
    sltu    $25,$5,1
    addu    $4,$4,$25
divdi1:
    and    $25,$8,$6
    beq    $25,$0,divdi2
    addiu    $9,$0,-1
    xor    $7,$7,$9
    xor    $6,$6,$9
    addu    $7,$7,1
    sltu    $25,$7,1
    addu    $6,$6,$25
divdi2:
    and    $3,$3,$8
    beq    $3,$0,div_com
    bne    $6,$0,divdi64
    move    $13,$31

    jal    div64_32
    move    $31,$13
    j    divdi_ret
divdi64:
    move    $13,$31
    jal    div64_64
    move    $31,$13
divdi_ret:
    addiu    $7,$0,-1
    xor    $3,$3,$7
    xor    $2,$2,$7
    addu    $3,$3,1
    sltu    $25,$3,1
    addu    $2,$2,$25
    j    $31



/*
    (long long) % (long long)

    A .. $4:$5
    B .. $6:$7
*/

    .align    4
    .globl    __moddi3
__moddi3:
    li    $8,0x80000000
    move    $3,$4
    and    $25,$8,$4
    beq    $25,$0,moddi1
    addiu    $9,$0,-1
    xor    $5,$5,$9
    xor    $4,$4,$9
    addu    $5,$5,1
    sltu    $25,$5,1
    addu    $4,$4,$25
moddi1:
    and    $25,$8,$6
    beq    $25,$0,moddi2
    addiu    $9,$0,-1
    xor    $7,$7,$9
    xor    $6,$6,$9
    addu    $7,$7,1
    sltu    $25,$7,1
    addu    $6,$6,$25
moddi2:
    and    $3,$3,$8
    beq    $3,$0,mod_com
    bne    $6,$0,moddi64
    move    $13,$31
    jal    div64_32
    move    $31,$13
    move    $3,$9
    xor    $2,$2,$2
    j    moddi_ret
moddi64:
    move    $13,$31
    jal    div64_64
    move    $31,$13
    move    $3,$5
    move    $2,$4
moddi_ret:
    addiu    $7,$0,-1
    xor    $3,$3,$7
    xor    $2,$2,$7
    addu    $3,$3,1
    sltu    $25,$3,1
    addu    $2,$2,$25
    j    $31



    .align    4
    .globl    __udivdi3
__udivdi3:
div_com:
    bne    $6,$0,udivdi64
    j    div64_32
udivdi64:
    j    div64_64

    .align    4
    .globl    __umoddi3
__umoddi3:
mod_com:
    bne    $6,$0,umoddi64
    move    $13,$31
    jal    div64_32
    move    $31,$13
    move    $3,$9
    xor    $2,$2,$2
    j    $31
umoddi64:
    move    $13,$31
    jal    div64_64
    move    $31,$13
    move    $3,$5
    move    $2,$4
    j    $31



