#include "common.h"

/* Placeholder types; real types unknown. Rename once identified. */

typedef struct Unk80054354Entry {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ u8 unk4;
    /* 0x5 */ u8 pad5[0x3];
} Unk80054354Entry; /* size = 0x8 */

/* Object returned by func_80054354(id). */
/* Tables reached through Unk80054354.unk0 (func_80054FF4 / func_80055FFC):
 * an array of 0xC-byte entries, each pointing at a list of 4-byte records
 * terminated by unk0 == -1. Placeholders; probably animation/model data. */
typedef struct Unk80054354Rec {
    /* 0x0 */ s16 unk0;
    /* 0x2 */ u8 unk2;
    /* 0x3 */ u8 unk3;
} Unk80054354Rec; /* size = 0x4 */

typedef struct Unk80054354Table {
    /* 0x0 */ Unk80054354Rec *unk0;
    /* 0x4 */ s16 unk4;
    /* 0x6 */ u8 pad6[0x2];
    /* 0x8 */ u8 unk8;
    /* 0x9 */ u8 pad9[0x3];
} Unk80054354Table; /* size = 0xC */

typedef struct Unk80054354Def {
    /* 0x00 */ u8 pad0[0x14];
    /* 0x14 */ Unk80054354Table *unk14;
} Unk80054354Def;

typedef struct Unk80054354 {
    /* 0x000 */ Unk80054354Def *unk0;
    /* 0x004 */ u8 pad4[0xC];
    /* 0x010 */ f32 unk10;
    /* 0x014 */ u8 pad14[0x4];
    /* 0x018 */ s32 unk18;
    /* 0x01C */ s32 unk1C;
    /* 0x020 */ u8 pad20[0x18];
    /* 0x038 */ f32 unk38;
    /* 0x03C */ f32 unk3C;
    /* 0x040 */ f32 unk40;
    /* 0x044 */ f32 unk44; /* 0x38..0x58: position, rotation (radians), scale */
    /* 0x048 */ f32 unk48;
    /* 0x04C */ f32 unk4C;
    /* 0x050 */ f32 unk50;
    /* 0x054 */ f32 unk54;
    /* 0x058 */ f32 unk58;
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
/* Assert / error report: (expression, source file, line). */
void func_800298E0(const char *expr, const char *file, s32 line);
s32 func_80043A04(s32 arg0, s32 arg1);
void func_800453E0(f32 *pos, f32 *rot, f32 *scale);

/* A 16-bit game angle (0x10000 = full turn). */
extern s16 D_800FBE48;

/* "EX" and "c:/usgolf64/polychara/pcsub.c": this segment is probably the
 * original polychara/pcsub.c. */
extern char D_800D0620[];
extern char D_800D0624[];

/* Argument of func_80058C58: three s32s, probably an integer x/y/z. */
typedef struct Unk80058C58 {
    /* 0x0 */ s32 unk0;
    /* 0x4 */ s32 unk4;
    /* 0x8 */ s32 unk8;
} Unk80058C58;
void func_80054BC0(void);
s32 func_80054FF4(s32 arg0, s32 arg1, s32 arg2);
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

/* Returns s32 but has no return statement: the original was presumably
 * declared with an implicit-int (or int) return type. With void, GCC fills
 * the delay slot after the null check, which the original leaves empty. */
s32 func_80055738(s32 arg0, s32 arg1, s32 arg2) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk88[arg1].unk0 = arg2;
        p->unk88[arg1].unk4 = 3;
    }
}

void func_80055788(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);
    s32 i;

    for (i = 31; i >= 0; i--) {
        p->unk88[i].unk4 = 0;
    }
}

/* s32 with no return: implicit int in the original (see func_80055738). */
s32 func_800557BC(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);

    if (arg1 >= 0x20) {
        func_800298E0(D_800D0620, D_800D0624, 1956);
    }
    if (p != NULL) {
        p->unk88[arg1].unk4 = 4;
        p->unk88[arg1].unk0 = 0;
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055828);

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80055AF4);

s32 func_80055FFC(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);
    Unk80054354Table *entry;
    s32 i;
    s32 ret = 0;

    if (p != NULL) {
        entry = p->unk0->unk14;
        for (i = 0; entry->unk0 != NULL; i++, entry++) {
            if (entry->unk8 == arg1) {
                ret = i;
                break;
            }
        }
    }
    return ret;
}

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

void func_80056294(s32 arg0, s32 arg1) {
    Unk80054354 *p = func_80054354(arg0);
    f32 value;
    f32 limit;

    if (p != NULL) {
        value = p->unk10 + (f32)arg1;
        limit = (f32)p->unk18;
        p->unk10 = value;
        if (limit <= value) {
            p->unk10 = limit;
        }
    }
}

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

/* Same implicit-int return type as func_80055738 (see there). */
s32 func_800564F0(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        p->unk189 = 1;
    }
}

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

void func_80058ACC(s32 arg0) {
    Unk80054354 *p = func_80054354(arg0);

    if (p != NULL) {
        func_800453E0(&p->unk38, &p->unk44, &p->unk50);
        /* 0.0000958738f = 2 * pi / 0x10000: game angle -> radians. */
        p->unk48 = (f32)(-D_800FBE48 & 0xFFFF) * 0.0000958738f;
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80058B34);

void func_80058C58(s32 arg0, Unk80058C58 *arg1, f32 arg2) {
    Unk80054354 *p = func_80054354(arg0);
    f32 z;

    if (p != NULL) {
        p->unk38 = arg1->unk0;
        z = arg1->unk8;
        p->unk40 = z;
        p->unk3C = func_80043A04(p->unk38, z);
        p->unk44 = 0;
        p->unk4C = 0;
        p->unk48 = -arg2;
        p->unk50 = 1.0f;
        p->unk54 = 1.0f;
        p->unk58 = 1.0f;
    }
}

INCLUDE_ASM("asm/nonmatchings/2FD00", func_80058D04);
