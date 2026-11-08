#include "common.h"

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80050BF0);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80050DA0);

/* Interrupt-safe wrappers: look up a handle by index in D_800C0E6C and,
 * if it is valid (not -1), pass it to a 748B0 (probably libmus) function. */
extern s32 D_800C0E6C[];

void func_80050F68(s32 index) {
    OSIntMask mask = osSetIntMask(1);
    s32 handle = D_800C0E6C[index];

    if (handle != -1) {
        func_80099F54(handle, 0);
    }
    osSetIntMask(mask);
}

s32 func_8009A0C0(s32, s32);

void func_80050FC0(s32 index, s32 arg1) {
    OSIntMask mask = osSetIntMask(1);
    s32 handle = D_800C0E6C[index];

    if (handle != -1) {
        func_8009A0C0(handle, arg1);
    }
    osSetIntMask(mask);
}


void func_80051024(s32 index, s32 arg1) {
    OSIntMask mask = osSetIntMask(1);
    s32 handle = D_800C0E6C[index];

    if (handle != -1) {
        func_8009A060(handle, arg1);
    }
    osSetIntMask(mask);
}


void func_80051088(s32 index, s32 arg1) {
    OSIntMask mask = osSetIntMask(1);
    s32 handle = D_800C0E6C[index];

    if (handle != -1) {
        func_8009A22C(handle, arg1);
    }
    osSetIntMask(mask);
}

extern s32 D_800C0EAC[];

void func_800510EC(s32 arg0, s32 arg1) {
    D_800C0EAC[arg0] = arg1;
}


void func_80051100(s32 index, s32 arg1) {
    OSIntMask mask = osSetIntMask(1);
    s32 handle = D_800C0E6C[index];

    if (handle != -1) {
        func_8009A1A0(handle, arg1);
    }
    osSetIntMask(mask);
}

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051164);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_800511D8);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051210);


INCLUDE_ASM("asm/nonmatchings/2BFF0", func_80051778);

INCLUDE_ASM("asm/nonmatchings/2BFF0", func_800517E0);

s32 func_80099F54(s32, s32);
extern s32 D_800C0E64;

void func_80051A08(s32 arg0) {
    u32 temp_s0;

    temp_s0 = osSetIntMask(1U);
    if (D_800C0E64 != -1) {
        func_80099F54(D_800C0E64, arg0);
    }
    osSetIntMask(temp_s0);
}

extern s32 D_800C119C;
extern s32 D_800C11A8;
extern s32 D_800C11AC;

void func_80051A60(void) {
    D_800C119C = 2;
    D_800C11A8 = 1;
    D_800C11AC = -1;
}

s32 func_8009A060(s32, s32);

void func_80051A88(s32 arg0) {
    u32 temp_s0;

    if (D_800C0E64 != -1) {
        temp_s0 = osSetIntMask(1U);
        func_8009A060(D_800C0E64, arg0);
        osSetIntMask(temp_s0);
    }
}

s32 func_8009A22C(s32, s32);

void func_80051AE4(s32 arg0) {
    u32 temp_s0;

    if (D_800C0E64 != -1) {
        temp_s0 = osSetIntMask(1U);
        func_8009A22C(D_800C0E64, arg0);
        osSetIntMask(temp_s0);
    }
}

s32 func_8009A1A0(s32, s32);

void func_80051B40(s32 arg0) {
    u32 temp_s0;

    if (D_800C0E64 != -1) {
        temp_s0 = osSetIntMask(1U);
        func_8009A1A0(D_800C0E64, arg0);
        osSetIntMask(temp_s0);
    }
}

s32 func_8009A130(s32, f32);

void func_80051B9C(f32 arg0) {
    u32 temp_s0;

    if (D_800C0E64 != -1) {
        temp_s0 = osSetIntMask(1U);
        func_8009A130(D_800C0E64, arg0);
        osSetIntMask(temp_s0);
    }
}

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
