#include "common.h"

/* Frames since the reaction began */
s32 sReactionFrame = 0;
s32 sUnusedReactionWord = 0; /* Unused; it is there to match */

Lights3 sReactionLights = gdSPDefLights3(
    0x60, 0x60, 0x60,
    0xFF, 0xDC, 0xDC, 0x00, 0x88, 0x78,
    0xFF, 0xDC, 0xDC, 0x00, 0x88, 0x78,
    0xFF, 0xDC, 0xDC, 0x00, 0x88, 0x78);

/* Reaction sounds take turns on these channels */
#define FIRST_REACTION_CHANNEL 4
s32 sReactionChannel = FIRST_REACTION_CHANNEL;

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095A10);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095A68);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095C10);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095D10);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095D64);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095D70);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80095DE0);

INCLUDE_ASM("asm/nonmatchings/reaction", func_8009676C);

INCLUDE_ASM("asm/nonmatchings/reaction", func_800967F4);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80096C04);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80096F44);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80097218);

INCLUDE_ASM("asm/nonmatchings/reaction", func_800974D8);

INCLUDE_ASM("asm/nonmatchings/reaction", func_800977E0);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80097A08);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80097C18);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80097E30);

INCLUDE_ASM("asm/nonmatchings/reaction", func_8009806C);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80098310);

INCLUDE_ASM("asm/nonmatchings/reaction", func_800985B4);

INCLUDE_ASM("asm/nonmatchings/reaction", func_8009874C);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80098758);

INCLUDE_ASM("asm/nonmatchings/reaction", func_800989C4);

INCLUDE_ASM("asm/nonmatchings/reaction", func_800989EC);

INCLUDE_ASM("asm/nonmatchings/reaction", func_80098C6C);


__asm__(".section .rodata");
static const char sUnusedFloatFormat[] = "            %f";
static const char sUnusedEmptyText[] = "";
