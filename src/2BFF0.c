#include "common.h"

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80050BF0);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80050DA0);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80050F68);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80050FC0);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051024);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051088);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_800510EC);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051100);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051164);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_800511D8);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051210);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051778);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_800517E0);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051A08);

extern s32 D_800C119C;
extern s32 D_800C11A8;
extern s32 D_800C11AC;

void func_80051A60(void) {
    D_800C119C = 2;
    D_800C11A8 = 1;
    D_800C11AC = -1;
}

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051A88);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051AE4);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051B40);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051B9C);

s32 func_80051BFC(void) {
    return D_800C11AC;
}

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051C08);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051D8C);

extern char D_800CCC14[]; /* "alSynNew\n" */

/* Stubbed in this build: the game drives the n_ synthesizer (libnaudio), so the
 * libultra entry point only logs that it was called. */
void alSynNew(ALSynth *s, ALSynConfig *config) {
    osSyncPrintf(D_800CCC14);
}

extern char D_800CCC20[]; /* "alSynDelete\n" */

/* Stubbed like alSynNew: only logs that it was called. */
void alSynDelete(ALSynth *s) {
    osSyncPrintf(D_800CCC20);
}

/* Empty stub: ALParam (struct ALParam_s) lives in libultra's internal synthInternals.h. */
void __freeParam(struct ALParam_s *param) {
}

/* Empty stub: PVoice (struct PVoice_s) lives in libultra's internal synthInternals.h. */
void _freePVoice(ALSynth *drvr, struct PVoice_s *pvoice) {
}
