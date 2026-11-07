#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

typedef struct Unk80054354Entry {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 pad5[0x3];
} Unk80054354Entry; /* size = 0x8 */

/* Object returned by func_80054354(id). */
typedef struct Unk80054354 {
    /* 0x000 */ u8 pad0[0x10];
    /* 0x010 */ f32 unk10;
    /* 0x014 */ u8 pad14[0x4];
    /* 0x018 */ s32 unk18;
    /* 0x01C */ u8 pad1C[0x40];
    /* 0x05C */ f32 unk5C;
    /* 0x060 */ u8 unk60;
    /* 0x061 */ u8 unk61;
    /* 0x062 */ u8 pad62[0x20];
    /* 0x082 */ u8 unk82;
    /* 0x083 */ u8 unk83;
    /* 0x084 */ u8 unk84;
    /* 0x085 */ u8 unk85;
    /* 0x086 */ u8 pad86[0x2];
    /* 0x088 */ Unk80054354Entry unk88[32];
    /* 0x188 */ u8 unk188;
    /* 0x189 */ u8 unk189;
} Unk80054354;

/* Object returned by func_80056060(id). */
typedef struct Unk80056060 {
    /* 0x0 */ u8 pad0[0x4];
    /* 0x4 */ s16 unk4;
    /* 0x6 */ s16 unk6;
} Unk80056060;

s32 func_800299F4(s32 arg0);
Unk80054354 *func_80054354(s32 arg0);
void func_80054BC0(void);
void func_80054FF4(s32 arg0, s32 arg1, s32 arg2);
Unk80056060 *func_80056060(s32 arg0);
void func_80056558(s32 arg0);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054900);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054938);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054B7C);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054BC0);

void func_80054E20(void) {
    if (func_800299F4(0x36)) {
        func_80054BC0();
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054E4C);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054F1C);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80054FF4);

void func_800550B8(s32 arg0) {
    func_80054FF4(arg0, 2, 6);
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800550D8);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055368);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055738);

void func_80055788(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);
    s32 i;

    for (i = 31; i >= 0; i--) {
        p->unk88[i].unk4 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800557BC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055828);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055AF4);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055FFC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80056060);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800560FC);

void func_80056238(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk85 = arg1;
    }
}

void func_80056264(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk10 = p->unk18;
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80056294);

void func_800562F4(s32 arg0, f32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk5C = arg1;
    }
}

void func_80056324(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk83 = arg1;
    }
}

void func_80056350(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk84 = arg1;
    }
}

void func_8005637C(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk60 = arg1;
    }
}

void func_800563A8(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk82 = arg1;
    }
}

void func_800563D4(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk61 = arg1;
    }
}

s32 func_80056400(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);
    s32 ret = -1;

    if (p != NULL) {
        ret = p->unk18;
    }
    return ret;
}

s32 func_8005642C(s32 arg0) {
    Unk80056060 *p = func_80056060(arg0);

    if (p != NULL) {
        return p->unk4 + p->unk6;
    }
    return -1;
}

s32 func_80056464(s32 arg0) {
    Unk80056060 *p = func_80056060(arg0);

    if (p != NULL) {
        return p->unk4;
    }
    return -1;
}

s32 func_80056494(s32 arg0) {
    Unk80056060 *p = func_80056060(arg0);

    if (p != NULL) {
        return p->unk6;
    }
    return -1;
}

void func_800564C4(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk188 = arg1;
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800564F0);

s32 func_80056520(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);
    s32 ret = -1;

    if (p != NULL) {
        ret = p->unk10;
    }
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80056558);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800568CC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800574A8);

void func_80057798(void) {
    s32 i;

    for (i = 0; i < 4; i++) {
        func_80056558(i);
    }
}

s32 func_800577D0(void) {
    return 0x23680;
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800577DC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_800578AC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80057914);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80057FFC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80058ACC);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80058B34);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80058C58);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80058D04);
