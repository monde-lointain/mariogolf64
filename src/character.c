#include "common.h"
#include "main.h"

/* The models with a row of sClubHeadDistances */
#define CLUB_HEAD_DISTANCE_CHARACTER_COUNT 18
#define CLUB_HEAD_DISTANCE_COUNT 5

/*
 * How far a character's club head sits from them at the ball, by model (the
 * model index modulo 36, when below 18) and by club kind: wood, iron, wedge,
 * putter, then the wood again when func_80054938's last argument is 2
 */
static const s32 sClubHeadDistances[CLUB_HEAD_DISTANCE_CHARACTER_COUNT][CLUB_HEAD_DISTANCE_COUNT] = {
    { 0x2BDD, 0x2070, 0x1903, 0x114D, 0x28F4 },
    { 0x3502, 0x2910, 0x21DD, 0x1A86, 0x2DAA },
    { 0x2B74, 0x1EDA, 0x17DA, 0x10B1, 0x287A },
    { 0x2F33, 0x279F, 0x2578, 0x1F5D, 0x2F33 },
    { 0x3967, 0x258A, 0x1FDF, 0x15C7, 0x2B1C },
    { 0x3017, 0x24B5, 0x2229, 0x17C7, 0x249B },
    { 0x3624, 0x2B1D, 0x24D4, 0x1891, 0x33E9 },
    { 0x39E2, 0x3049, 0x2D05, 0x20E4, 0x3279 },
    { 0x4248, 0x2E12, 0x27B9, 0x1B35, 0x32A7 },
    { 0x34EC, 0x2A0B, 0x2311, 0x18AB, 0x341D },
    { 0x2BCD, 0x1EF7, 0x197B, 0x1636, 0x2807 },
    { 0x5D47, 0x5119, 0x4DB7, 0x37C5, 0x5D47 },
    { 0x5418, 0x4540, 0x3EC1, 0x3AF8, 0x5418 },
    { 0x34EC, 0x2A0B, 0x2311, 0x18AB, 0x341D },
    { 0x2CD0, 0x1DAC, 0x183A, 0x1321, 0x224E },
    { 0x271C, 0x198F, 0x161D, 0x1433, 0x271C },
    { 0x334B, 0x276E, 0x1F84, 0x17E4, 0x3017 },
    { 0x3295, 0x2776, 0x1FD8, 0x1931, 0x2BC9 },
};

INCLUDE_ASM("asm/nonmatchings/character", func_80054900);

INCLUDE_ASM("asm/nonmatchings/character", func_80054938);

INCLUDE_ASM("asm/nonmatchings/character", func_80054B7C);

/*
 * GCC last wrote sClubHeadDistances, so it thinks the assembler is still in
 * .rodata, but INCLUDE_ASM left it in .text. Agree with GCC so the data below
 * lands in .rodata.
 */
__asm__(".section .rodata");

/* Assertion strings that func_80054BC0 and func_800557BC share */
static const char sAssertionExpression[] = "EX";
static const char sFileName[] = "c:/usgolf64/polychara/pcsub.c";

INCLUDE_ASM("asm/nonmatchings/character", func_80054BC0);

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

/* Unreferenced; left by debug output that the routines below no longer have */
static const char sUnusedFrameFormat[] = "TA %d : FRAME %d    ";

INCLUDE_ASM("asm/nonmatchings/character", func_80054E20);

INCLUDE_ASM("asm/nonmatchings/character", func_80054E4C);

INCLUDE_ASM("asm/nonmatchings/character", func_80054F1C);

INCLUDE_ASM("asm/nonmatchings/character", func_80054FF4);

INCLUDE_ASM("asm/nonmatchings/character", func_800550B8);

INCLUDE_ASM("asm/nonmatchings/character", func_800550D8);

INCLUDE_ASM("asm/nonmatchings/character", func_80055368);

INCLUDE_ASM("asm/nonmatchings/character", func_80055738);

INCLUDE_ASM("asm/nonmatchings/character", func_80055788);

INCLUDE_ASM("asm/nonmatchings/character", func_800557BC);

INCLUDE_ASM("asm/nonmatchings/character", func_80055828);

INCLUDE_ASM("asm/nonmatchings/character", func_80055AF4);

INCLUDE_ASM("asm/nonmatchings/character", func_80055FFC);

INCLUDE_ASM("asm/nonmatchings/character", func_80056060);

INCLUDE_ASM("asm/nonmatchings/character", func_800560FC);

INCLUDE_ASM("asm/nonmatchings/character", func_80056238);

INCLUDE_ASM("asm/nonmatchings/character", func_80056264);

INCLUDE_ASM("asm/nonmatchings/character", func_80056294);

INCLUDE_ASM("asm/nonmatchings/character", func_800562F4);

INCLUDE_ASM("asm/nonmatchings/character", func_80056324);

INCLUDE_ASM("asm/nonmatchings/character", func_80056350);

INCLUDE_ASM("asm/nonmatchings/character", func_8005637C);

INCLUDE_ASM("asm/nonmatchings/character", func_800563A8);

INCLUDE_ASM("asm/nonmatchings/character", func_800563D4);

INCLUDE_ASM("asm/nonmatchings/character", func_80056400);

INCLUDE_ASM("asm/nonmatchings/character", func_8005642C);

INCLUDE_ASM("asm/nonmatchings/character", func_80056464);

INCLUDE_ASM("asm/nonmatchings/character", func_80056494);

INCLUDE_ASM("asm/nonmatchings/character", func_800564C4);

INCLUDE_ASM("asm/nonmatchings/character", func_800564F0);

INCLUDE_ASM("asm/nonmatchings/character", func_80056520);

INCLUDE_ASM("asm/nonmatchings/character", func_80056558);

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

/* The double 1.0, which func_800568CC loads as two words */
const u32 D_800D07E8 = 0x3FF00000;
const u32 D_800D07EC = 0;

INCLUDE_ASM("asm/nonmatchings/character", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/character", func_800574A8);

INCLUDE_ASM("asm/nonmatchings/character", func_80057798);

INCLUDE_ASM("asm/nonmatchings/character", GetCharacterBufferSize);

INCLUDE_ASM("asm/nonmatchings/character", SetCharacterBuffer);

INCLUDE_ASM("asm/nonmatchings/character", ResetCharacterSlots);

INCLUDE_ASM("asm/nonmatchings/character", func_80057914);

/* As above, so the data below lands in .rodata */
__asm__(".section .rodata");

const u8 D_800D0834[4] = { 0x9B, 0x78, 0x78, 0x00 };
const u8 D_800D0838[8] = { 0x96, 0x96, 0x96, 0x00, 0x00, 0x00, 0x00, 0x00 };

INCLUDE_ASM("asm/nonmatchings/character", func_80057FFC);

INCLUDE_ASM("asm/nonmatchings/character", func_80058ACC);

INCLUDE_ASM("asm/nonmatchings/character", func_80058B34);

INCLUDE_ASM("asm/nonmatchings/character", func_80058C58);

INCLUDE_ASM("asm/nonmatchings/character", func_80058D04);

