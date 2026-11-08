#include "common.h"

/* Placeholder for the object passed to the 0x8009A5xx accessors; real type
 * unknown (their callers are not direct jal sites). Rename once identified. */
typedef struct Unk8009A500 {
    /* 0x00 */ u8 pad0[0x4];
    /* 0x04 */ s32 unk4;
    /* 0x08 */ u8 pad8[0x8];
    /* 0x10 */ s32 unk10;
} Unk8009A500;

/* Per-channel state of the sequence/music player that the 0x8009Bxxx-
 * 0x8009Cxxx command handlers operate on: each handler takes the channel
 * and a pointer into the command stream and returns the advanced pointer.
 * This segment sits just before libmus (0x78D10) and calls n_alSyn*Voice,
 * so this is probably libmus's player and its channel_t. Placeholder:
 * only the fields used so far are named. */
typedef struct UnkMusChannel {
    /* 0x00 */ u8 pad0[0x8];
    /* 0x08 */ s32 unk8;
    /* 0x0C */ u8 padC[0x18];
    /* 0x24 */ f32 unk24;
    /* 0x28 */ u8 pad28[0xC];
    /* 0x34 */ s32 unk34;
    /* 0x38 */ s32 unk38;
    /* 0x3C */ u8 pad3C[0x4];
    /* 0x40 */ s32 unk40;
    /* 0x44 */ s32 unk44;
    /* 0x48 */ u8 pad48[0x20];
    /* 0x68 */ s32 unk68;
    /* 0x6C */ f32 unk6C;
    /* 0x70 */ f32 unk70;
    /* 0x74 */ s32 unk74;
    /* 0x78 */ s32 unk78;
    /* 0x7C */ u8 pad7C[0x8];
    /* 0x84 */ s32 unk84;
    /* 0x88 */ u8 pad88[0xC];
    /* 0x94 */ s32 unk94;
    /* 0x98 */ u8 pad98[0x14];
    /* 0xAC */ s16 unkAC;
    /* 0xAE */ u8 padAE[0x4];
    /* 0xB2 */ s16 unkB2;
    /* 0xB4 */ s16 unkB4;
    /* 0xB6 */ u8 padB6[0x1];
    /* 0xB7 */ u8 unkB7;
    /* 0xB8 */ u8 unkB8;
    /* 0xB9 */ u8 unkB9;
    /* 0xBA */ u8 unkBA;
    /* 0xBB */ u8 padBB[0x1];
    /* 0xBC */ u8 unkBC;
    /* 0xBD */ u8 unkBD;
    /* 0xBE */ u8 padBE[0xB];
    /* 0xC9 */ u8 unkC9;
    /* 0xCA */ u8 unkCA;
    /* 0xCB */ u8 padCB[0x3];
    /* 0xCE */ u8 unkCE;
    /* 0xCF */ u8 unkCF;
    /* 0xD0 */ u8 unkD0;
    /* 0xD1 */ s8 unkD1;
    /* 0xD2 */ u8 unkD2;
    /* 0xD3 */ u8 unkD3;
    /* 0xD4 */ u8 unkD4;
    /* 0xD5 */ u8 unkD5;
    /* 0xD6 */ u8 unkD6;
    /* 0xD7 */ u8 unkD7;
    /* 0xD8 */ u8 unkD8;
    /* 0xD9 */ u8 unkD9;
    /* 0xDA */ u8 unkDA;
} UnkMusChannel;

/* Voice array element (0x1C bytes) passed to n_alSyn*Voice. */
typedef struct UnkMusVoice {
    /* 0x00 */ u8 pad0[0x1C];
} UnkMusVoice;

/* Message passed to func_8009A6B0: a command byte and an argument. */
typedef struct UnkMusMessage {
    /* 0x0 */ u8 unk0;
    /* 0x4 */ s32 unk4;
} UnkMusMessage;

s32 func_8009C280(void);
void func_8009A3C8(s32 arg0);
s32 func_8009C0D8(Unk8009A500 *arg0, s32 arg1, s32 arg2, s32 arg3, s32 arg4);
void func_8009BAC0(void);
void func_8009DBA0(void);
void func_8009C4EC(s32 arg0, s32 arg1, s32 arg2);
void func_8009D8A8(s32 arg0);
void n_alSynStartVoice(UnkMusVoice *voice, s32 wave);
void n_alSynStopVoice(UnkMusVoice *voice);

extern s32 D_800E7068;
extern s32 D_800E706C;
extern s32 D_800E7078;
extern s32 D_800E707C;
extern UnkMusVoice *D_800E7048;
extern f64 D_800D2080;

INCLUDE_ASM("asm/nonmatchings/748B0", main);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009951C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_800995F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009973C);

extern s16 D_800E705C;
extern s16 D_800E705E;

void func_800999C4(s32 arg0, s16 arg1) {
    if (arg0 & 1) {
        D_800E705C = arg1;
    }
    if (arg0 & 2) {
        D_800E705E = arg1;
    }
}

s32 func_800999F0(void) {
    s32 ret = func_8009C280();

    func_8009A3C8(ret);
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099A20);

s32 func_80099C70(s32 arg0) {
    Unk8009A500 *song;
    s32 ret;

    song = (Unk8009A500 *)D_800E707C;
    if (song == NULL) {
        song = (Unk8009A500 *)D_800E7078;
        if (song == NULL) {
            D_800E7068 = 0;
            return 0;
        }
    } else {
        D_800E707C = 0;
    }
    if (D_800E7068 == 0) {
        D_800E7068 = song->unk10;
    }
    ret = func_8009C0D8(song, arg0, 0x80, 0x80, -1);
    D_800E7068 = 0;
    return ret;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099CF0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099E08);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099ED4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_80099F54);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A008);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A060);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A0C0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A130);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A1A0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A22C);

void func_8009A2B4(s32 arg0) {
    func_8009BAC0();
    if (D_800E706C == 0) {
        D_800E706C = arg0;
    }
}

Unk8009A500 *func_8009A2F0(Unk8009A500 *arg0) {
    if (arg0 != NULL && arg0->unk10 < 0) {
        D_800E7068 = (s32)arg0;
    }
    return (Unk8009A500 *)D_800E7068;
}

void func_8009A318(Unk8009A500 *arg0) {
    if (arg0 != NULL && arg0->unk10 < 0) {
        D_800E706C = (s32)arg0;
    }
}

extern s32 D_800E706C;

s32 func_8009A33C(void) {
    return D_800E706C;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A348);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3A4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3C8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A3F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A420);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A47C);

s32 func_8009A500(Unk8009A500 *arg0) {
    return arg0->unk4;
}

extern s32 D_800E7078;

void func_8009A508(s32 arg0) {
    D_800E7078 = arg0;
}

extern s32 D_800E707C;

void func_8009A514(s32 arg0) {
    D_800E707C = arg0;
}

s32 func_8009A520(void) {
    return D_800E7078;
}

void func_8009A52C(Unk8009A500 *arg0, s32 arg1) {
    arg0->unk10 = arg1;
}

s32 func_8009A534(Unk8009A500 *arg0) {
    return arg0->unk10;
}

extern s32 __libmus_current_sched;

void func_8009A53C(s32 arg0) {
    __libmus_current_sched = arg0;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A548);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A5AC);

extern s32 D_800E7080;

void func_8009A624(s32 arg0) {
    D_800E7080 = arg0;
}

void func_8009A630(void) {
    func_8009DBA0();
}

s32 __MusIntMemMalloc(s32);
extern s32 D_800E7084;
extern s32 D_800E7088;
extern s32 D_800E708C;
extern s32 D_800E7090;

void func_8009A64C(s32 arg0) {
    s32 var_s0;

    var_s0 = arg0;
    if (var_s0 < 0x40) {
        var_s0 = 0x40;
    } else if (var_s0 >= 0x401) {
        var_s0 = 0x400;
    }
    D_800E7090 = __MusIntMemMalloc(var_s0 * 8);
    D_800E708C = var_s0;
    D_800E7088 = 0;
    D_800E7084 = 0;
}

void func_8009A6B0(UnkMusMessage *msg) {
    switch (msg->unk0) {
        case 0:
            func_8009C4EC(msg->unk4, -2, 1);
            return;
        case 1:
            func_8009C4EC(msg->unk4, -2, 0);
            return;
        case 2:
            func_8009D8A8(msg->unk4);
            return;
    }
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A730);

extern s32 D_800C7770;

s32 func_8009A7C8(void) {
    return D_800C7770;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009A7D4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009AB18);

void func_8009B060(UnkMusChannel *ch, s32 voice) {
    if (ch->unkC9 != 0) {
        n_alSynStopVoice(&D_800E7048[voice]);
    }
    ch->unkC9 = 1;
    n_alSynStartVoice(&D_800E7048[voice], ch->unk8);
    ch->unk8 = 0;
}

extern s32 D_800C76B8;

void func_8009B0DC(s32 arg0) {
    D_800C76B8 = arg0 == 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B0F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B254);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B360);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B3D0);

void func_8009B5C4(UnkMusChannel *ch) {
    ch->unkD9 = 0;
    ch->unk94 = ch->unk40;
    ch->unkDA = ch->unkBD & 0x40;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B5E0);

f32 func_8009B698(UnkMusChannel *ch) {
    if (--ch->unkD0 == 0) {
        if (ch->unkD1 == 0) {
            ch->unkD1 = ch->unkD8;
            ch->unkD0 = ch->unkCE;
        } else {
            ch->unkD1 = 0;
            ch->unkD0 = ch->unkCF;
        }
    }
    return ch->unkD1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B6F0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B754);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B818);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009B92C);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BAC0);

extern s32 D_800E7064;

s32 func_8009BC58(s32 arg0) {
    s32 temp_a1;
    s32 temp_v0;
    s32 temp_v1;
    s32 var_a2;

    var_a2 = 0;
    do {
        temp_a1 = D_800E7064 * 2;
        temp_v0 = D_800E7064 & 0x48000000;
        temp_v1 = (temp_v0 == 0x48000000) | (temp_v0 == 0x08000000);
        D_800E7064 = temp_a1;
        var_a2 += 1;
        if (temp_v1 != 0) {
            D_800E7064 = temp_a1 | 1;
        }
    } while (var_a2 < 8);
    return (s32) ((f32) arg0 * (((f32) D_800E7064 / 65536.0f) / 65536.0f));
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BCEC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BE28);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009BFF0);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C028);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C0D8);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C280);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C4EC);

s32 func_8009C540(UnkMusChannel *ch) {
    ch->unk38 = 0;
    ch->unk34 = 0;
    ch->unk74 = 0;
    ch->unk78 = 0;
    ch->unk44 = 0;
    ch->unk8 = 0;
    return 0;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C560);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C590);

u8 *func_8009C5B0(UnkMusChannel *ch, u8 *p) {
    ch->unkB8 = 0;
    return p;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C5BC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C6E4);

u8 *func_8009C7C8(UnkMusChannel *ch, u8 *p) {
    u8 v = *p;

    ch->unkB2 = 0;
    ch->unkB4 = v;
    return p + 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C7DC);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C800);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C870);

u8 *func_8009C8E4(UnkMusChannel *ch, u8 *p) {
    ch->unkD5 = 0;
    ch->unk68 = 0;
    return p;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C8F4);

u8 *func_8009C924(UnkMusChannel *ch, u8 *p) {
    ch->unkB7 = 1;
    return p;
}

u8 *func_8009C934(UnkMusChannel *ch, u8 *p) {
    ch->unkB9 = *p;
    return p + 1;
}

u8 *func_8009C944(UnkMusChannel *ch, u8 *p) {
    ch->unkBA = 1;
    return p;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C954);

u8 *func_8009C9AC(UnkMusChannel *ch, u8 *p) {
    ch->unkD6 = 1;
    return p;
}

u8 *func_8009C9BC(UnkMusChannel *ch, u8 *p) {
    ch->unkD6 = 0;
    return p;
}

u8 *func_8009C9C8(UnkMusChannel *ch, u8 *p) {
    ch->unkD7 = 1;
    return p;
}

u8 *func_8009C9D8(UnkMusChannel *ch, u8 *p) {
    ch->unkD7 = 0;
    return p;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009C9E4);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CA50);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CAEC);

u8 *func_8009CB14(UnkMusChannel *ch, u8 *p) {
    ch->unkCE = 0;
    return p;
}

u8 *func_8009CB20(UnkMusChannel *ch, u8 *p) {
    ch->unkD2 = 1;
    return p;
}

u8 *func_8009CB30(UnkMusChannel *ch, u8 *p) {
    ch->unkD2 = 0;
    return p;
}

u8 *func_8009CB3C(UnkMusChannel *ch, u8 *p) {
    u8 v = *p;

    ch->unkD2 = 0;
    ch->unkD3 = v;
    return p + 1;
}

u8 *func_8009CB50(UnkMusChannel *ch, u8 *p) {
    ch->unkBD = *p >> 1;
    return p + 1;
}

s32 func_8009CB64(s32 arg0, s32 arg1) {
    return arg1 + 2;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CB6C);

u8 *func_8009CBB4(UnkMusChannel *ch, u8 *p) {
    ch->unk84 = 0;
    return p;
}

s32 func_8009CBC0(s32 arg0, s32 arg1) {
    return arg1 + 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CBC8);

u8 *func_8009CC38(UnkMusChannel *ch, u8 *p) {
    ch->unkCA = *p;
    return p + 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CC48);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CC98);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CCE8);

u8 *func_8009CD38(UnkMusChannel *ch, u8 *p) {
    ch->unkBC = *p;
    return p + 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CD48);

u8 *func_8009CE18(UnkMusChannel *ch, u8 *p) {
    f32 value = (f32)*p * D_800D2080;

    ch->unk6C = value;
    ch->unk24 = ch->unk70 * value;
    return p + 1;
}

u8 *func_8009CE54(UnkMusChannel *ch, u8 *p) {
    ch->unkD4 = *p;
    return p + 1;
}

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CE64);

INCLUDE_ASM("asm/nonmatchings/748B0", func_8009CEA8);

u8 *func_8009CF18(UnkMusChannel *ch, u8 *p) {
    ch->unkAC = 0;
    return p;
}
