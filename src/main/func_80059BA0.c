#include "common.h"

typedef struct {
  s32 unk_00[26];
} Struct80131510;

extern Struct80131510 D_80131510[];
extern u8 D_801323A0[];
extern s8 D_80105DC1;
extern s8 D_80105DC2;
extern s32 D_80105DC4;
extern s32 D_800C2B28[];
extern s32 D_800FE334;
extern u8 D_801052E8;
extern u8 D_801B9410;
extern u8 D_800C2BD4;
extern s32 current_game_mode;
extern u8 D_8012F720[];
extern s32 D_801B6098;
extern s32 scenario_mode_id;
extern u8 D_8012D400[];
extern u32 D_8012F724;
extern s32 D_80105DCC;
extern s32 D_80105DC8;

extern u8 rumble_disable_flag;
extern u8 D_800FE3EC;
extern u8 D_800FE402;
extern u8 D_800FE418;
extern u8 D_800FE42E;
extern s8 D_800C1FF4;
extern s8 D_800C1FF5;
extern s8 D_800C1FF6;
extern s8 D_800C1FF7;
extern s32 D_800C2B3C;

extern s32 D_800C2BD0;
extern s32 D_80105B68;
extern u32 player_context_offset;
extern s32 D_8012F480;
extern s32 D_8012F484;
extern s32 D_8012F488;
extern s32 D_8012F48C;
extern u8 D_801B70F0;
extern u8 D_801B70F1;
extern u8 D_801B70F2;
extern u8 D_801B70F3;
extern s16 D_800FBD40;
extern s16 D_801B71CC;
extern s16 D_800FBE2C;
extern u8 D_800FBDB9;
extern s16 D_800FE33C;
extern u8 D_800FF4C0;
extern u8 D_80105268;
extern u8 D_800FBE08[];
extern u8 D_801323B8;
extern s32 D_801B60AC;
extern u8 D_801323D0;
extern s32 D_801B608C;
extern u8 D_801B5634;
extern u8 D_800C2BD5;

u8* func_8005AF74(void);
void func_8005DF54(u8*, s32);
s32 func_80099490(void);
void flag_clear(s32);
void func_8005D9A0(void);

/* func_80059BA0: fabsf(arg0) via union bit-clear of the sign bit (0x7FFFFFFF).
 * NEAR-MATCH, CARRIED (terminal sched2 coin). The faithful body
 *   union{f32 f;s32 i;}x; x.f=arg0; x.i&=0x7FFFFFFF; arg0=x.f; return arg0;
 * yields the exact 8 insns + identical regalloc, but gcc's post-reload
 * scheduler (schedule_select, sched.c:2650) front-loads the independent `li`
 * ahead of `mfc1` to fill the mov.s->mfc1 FP-load-coproc hazard slot:
 *   mine: mov.s $f0,$f12; li; mfc1; ori; and; mtc1
 *   ROM : mov.s $f0,$f12; mfc1; li; ori; and; mtc1
 * See docs/wip/func_80059BA0.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BA0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059BC0);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_80059FAC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005A2AC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005A580);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005ACF8);

u8* func_8005AF50(void) {
  func_8005AF74();
  return D_8012F720;
}

u8* func_8005AF74(void) { return D_801323A0; }

void* memset(void*, s32, u32);

void func_8005AF80(void) {
  u8* base = func_8005AF50();
  s8 i;
  s8 j;
  memset(base, 0, 0x2A78);
  *(s32*)base = 0x12345678;
  i = 0;
  do {
    s32* row;
    j = 0;
    /* (s32)base drops REG_POINTER so the addu keeps i*12 as rs (addu v0,v0,s0),
     * matching the ROM's commutative operand order. */
    row = (s32*)(i * 12 + (s32)base + 0xDC0);
    do {
      row[j] = -1;
      j++;
    } while (j != 3);
    i++;
  } while (i != 6);
  func_8005DF54(base, 1);
}

void func_8005B03C(void) {
  u8* temp = func_8005AF50();
  func_80099490();
  func_8005DF54(temp, 0);
}

s32 func_8005B070(s32 arg0) {
  s32 i = 5;
  s32* p = &D_800C2B3C;
  while (i != 0) {
    if (arg0 >= *p) {
      break;
    }
    i--;
    p--;
  }
  return i + 1;
}

s32 func_8005B0A0(s32 arg0) { return D_800C2B28[arg0]; }

/* func_8005B0B4: selector (flags&0xF) dispatch -> strength value. NEAR-MATCH
 * 37/39 (faithful body in nonmatchings/func_8005B0B4/base.c). Residual = S263
 * value-select-branch-likely: ROM emits the const early-returns as `beql
 * sel,K,end` with the value in the annulled delay slot; gcc-from-C branches
 * away + `j;li`. Terminal / permuter-denied. See
 * docs/wip/func_8005B0B4.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B0B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B150);

void func_8005B28C(s32 idx, Struct80131510* src) {
  D_80131510[idx] = *src;
  func_8005DF54(func_8005AF50(), 1);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B314);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005B7BC);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005BC10);

void func_8005C018(u32 arg0) {
  u32* p = &D_8012F724;
  if (*p < arg0) {
    *p = arg0;
  }
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C038);

s32 func_8005C458(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0x1320;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 6;
    i++;
  } while (i != n);
  return count == 36;
}

s32 func_8005C4B4(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0x1320;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 6;
    i++;
  } while (i != n);
  return count >= 30;
}

s32 func_8005C510(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0x1320;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 6;
    i++;
  } while (i != n);
  if (count == 4) {
    return 1;
  }
  if (count == 8) {
    return 2;
  }
  if (count == 12) {
    return 3;
  }
  if (count == 16) {
    return 4;
  }
  return (count == 20) ? 5 : 0;
}

s32 func_8005C5B4(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 18, lim = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0xA84;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 18;
    i++;
  } while (i != lim);
  return count >= 50;
}

s32 func_8005C614(void) {
  u8* ret = func_8005AF50();
  s32 count = 0, i = 0, n = 18, lim = 6;
  u8* base = ret;
  do {
    u32 p = (u32)base + 0xA84;
    u32 end = n + p;
    do {
      count += (*(s8*)p != 0);
      p++;
    } while (p != end);
    base += 18;
    i++;
  } while (i != lim);
  return count == 108;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005C674);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CA48);

/* func_8005CEE0: bounds-check(0..14) + triple-table sign predicate.
 * NEAR-MATCH 38/38 (permuter dry, 400s). Faithful body in
 * nonmatchings/func_8005CEE0/base.c: exact instr count, sole residual =
 * field-load order + a0/v1 regalloc role swap + the a*14 vs b*2 multiply
 * emission order (base-vs-disp fixed via explicit `u8* base`).
 * See docs/wip/func_8005CEE0.near-match.md. */
INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005CEE0);

typedef struct {
  /* 0x00 */ s32 active;
  /* 0x04 */ s8 pad04[0x3C - 0x04];
  /* 0x3C */ s32 unk_3C;
  /* 0x40 */ s32 sel;
  /* 0x44 */ s32 count;
  /* 0x48 */ s32 unk_48;
  /* 0x4C */ s32 unk_4C;
  /* 0x50 */ s8 pad50[0x76 - 0x50];
  /* 0x76 */ s8 f76;
  /* 0x77 */ s8 pad77[0xA0 - 0x77];
} Cf78Section;

typedef struct {
  /* 0x00 */ s8 pad00[0x1D];
  /* 0x1D */ u8 f1D;
  /* 0x1E */ s8 pad1E[0x7B - 0x1E];
  /* 0x7B */ u8 f7B;
  /* 0x7C */ s8 pad7C[0xB8 - 0x7C];
} Cf78Row;

void func_8005CF78(void) {
  u8* base = func_8005AF50();
  s32 flags[3];
  s32 k;

  k = 0;
  do {
    flags[k] = 0;
    k++;
  } while (k != 3);

  k = 0;
  do {
    if (*(s32*)(base + k * 0xA0 + 0x1FF8) != 0) {
      Cf78Section* sect = (Cf78Section*)(base + 0x1FF8 + k * 0xA0);
      s32* a3 = &sect->unk_3C;

      if (sect->f76 == 0) {
        switch (sect->sel) {
          case 0:
          case 2:
          case 4:
          case 5:
          case 10:
          case 11:
            break;
          default:
            flags[k] = 1;
            break;
        }
      }
      if ((u32)(a3[2] - 1) >= 4U) {
        flags[k] = 1;
      }
      if ((u32)a3[3] >= 0xCU) {
        flags[k] = 1;
      }
      if ((u32)a3[4] >= 0x12U) {
        flags[k] = 1;
      }
      {
        s32 i = 0;
        Cf78Row* rows;
        while (i != a3[2]) {
          rows = (Cf78Row*)(base + 0x21D8 + k * 0x2E0);
          if (rows[i].f1D >= 0x12) {
            flags[k] = 1;
          }
          if (rows[i].f7B >= 0x4) {
            flags[k] = 1;
          }
          i++;
        }
      }
    }
    k++;
  } while (k != 3);

  k = 0;
  do {
    if (flags[k] == 1) {
      *(s32*)(base + k * 0xA0 + 0x1FF8) = 0;
    }
    k++;
  } while (k != 3);
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D0D8);

void func_8005D1E4(void) {
  u8* p = func_8005AF74();
  p[0x2E] = D_801B6098;
  p[0x2F] = scenario_mode_id;
}

s32 func_8005D218(void) {
  switch (D_80105DC1) {
    case 0:
    default:
      return 0;
    case 1:
      return 7;
  }
}

s32 func_8005D23C(void) { return D_80105DC2; }

s32 func_8005D248(void) { return D_80105DC4 != 0; }

s32 func_8005D258(void) {
  s32 x = D_80105DC8;
  return (x & (~x >> 31)) + 0x16F;
}

s32 func_8005D274(void) {
  s32 v = func_8005D23C();
  return (v & (~v >> 31)) + 0x1EB;
}

s32 func_8005D2A0(void) { return (D_80105DCC > 0) ? D_80105DCC : 1; }

void func_8005D2B8(u8* arg0) {
  s32 i = 0;
  do {
    *arg0++ = D_8012D400[i];
  } while (++i != 3);
}

s32 func_8005D2E4(void) {
  switch (D_80105DC1) {
    case 0:
    default:
      return 158;
    case 1:
      return 157;
  }
}

s32 func_8005D308(void) {
  s32 x = D_80105DC2;
  s32 r;

  if (x >= 6) {
    if (x >= 9) {
      r = 0xC8;
    } else {
      r = 0xC7;
    }
  } else {
    r = x + 0x1EB;
  }
  return r;
}

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D334);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005D3B8);

void func_8005D9A0(void) {
  s8 i;

  D_800C2BD0 = 6;
  D_800FE334 = 0;
  current_game_mode = 0;
  D_80105B68 = 0;
  player_context_offset = 0;
  D_8012F480 = -1;
  D_8012F484 = -1;
  D_8012F488 = -1;
  D_8012F48C = -1;
  D_801B70F0 = 0;
  D_801B70F1 = 0;
  D_801B70F2 = 0;
  D_801B70F3 = 0;
  memset(D_801323A0, 0, 0x49);
  D_800FBD40 = 0;
  D_801B71CC = 0;
  D_800FBE2C = 0;
  D_800FBDB9 = 0;
  D_800FE33C = 0;
  D_800FF4C0 = 0;
  D_80105268 = 0;
  i = 0;
  do {
    D_800FBE08[i] = 0;
  } while (++i != 4);
  D_801323B8 = 3;
  D_801B60AC = 0;
  D_801323D0 = 6;
  D_801052E8 = 0;
  D_800C2BD4 = 0;
}

void func_8005DAD4(void) {
  D_800FE334 = 0;
  D_801052E8 = 1;
  D_801B9410 = 0;
  D_800C2BD4 = 0;
}

void func_8005DAFC(void) {
  switch (D_801B608C) {
    case 5:
      D_800FF4C0 = 2;
      D_80105268 = 0;
      D_801B5634 = 0;
      break;
    case 0:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      if (D_80105B68 == 4) {
        D_801B5634 = 2;
      } else {
        D_801B5634 = 1;
      }
      D_800C2BD5 = 1;
      break;
    case 2:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      D_801B5634 = 3;
      break;
    case 10:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      D_801B5634 = 4;
      break;
    case 3:
    case 4:
    case 6:
    case 7:
    case 9:
      D_800FF4C0 = 1;
      D_80105268 = 2;
      D_801B5634 = 0;
      break;
    case 11:
      D_800FF4C0 = 1;
      D_80105268 = 1;
      D_801B5634 = 5;
      break;
    default:
      break;
  }
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 6;
}

void func_8005DC50(void) {
  func_8005D9A0();
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  D_800FE3EC = 0;
  D_800FE402 = 0;
  D_800FE418 = 0;
  D_800FE42E = 0;
  D_800C1FF4 = -1;
  D_800C1FF5 = -1;
  D_800C1FF6 = -1;
  D_800C1FF7 = -1;
  current_game_mode = 0;
}

void func_8005DCDC(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0x17;
}

void func_8005DD10(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 5;
}

void func_8005DD44(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0x12;
}

void func_8005DD78(void) {
  D_801052E8 = 1;
  D_801B9410 = 1;
  D_800FE334 = 0;
  D_800C2BD4 = 0;
  current_game_mode = 0xD;
}

void func_8005DDAC(void) {
  D_800FE334 = 0;
  D_801052E8 = 1;
  D_800C2BD4 = 0;
  D_801B9410 = 1;
  rumble_disable_flag = 0;
  flag_clear(0x1E);
  current_game_mode = 0xC;
}

s8 func_8005DE00(void) { return ((s8*)func_8005AF50())[0x28]; }

s8 func_8005DE20(void) { return ((s8*)func_8005AF50())[0x29]; }

s8 func_8005DE40(void) { return ((s8*)func_8005AF50())[0x2B]; }

void func_8005DE60(void) { func_8005DF54(func_8005AF50(), 1); }

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DE88);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DF54);

INCLUDE_ASM("asm/nonmatchings/main/func_80059BA0", func_8005DFE8);

extern u8 D_801B5588[];
extern u32 D_800C2BE0;
extern u32 D_800C2BE4;
extern u32 D_800C2BE8;
extern u32 D_800C2BEC;
extern s32 func_80028110(u32 addr, s32 len);

s32 func_8005E180(u8* buf, s32 save_flag) {
  u8 raw[0xC0];
  u16* stat = (u16*)(((u32)raw + 0xF) & ~0xF);
  s32 i;
  s32 j;
  s32 flag;
  u16 crc;
  s32 off;

  for (j = 0; j < 0x20; j++) {
    u8* dst = buf + j;
    dst[8] = ~D_801B5588[j];
  }
  crc = crc16_ccitt(buf, 0x2A78);
  i = 0;
  flag = save_flag & 0xFF;
  off = 0;
  do {
    s32 r = rand();
    stat[0] = r;
    stat[1] = r;
    stat[1] = stat[1] ^ crc;
    if (flag == 0) {
      if (func_80028110(*(u32*)((u8*)&D_800C2BE0 + off), 0x2A78) == 0) {
        func_80028110(*(u32*)((u8*)&D_800C2BE4 + off), 4);
      }
    } else {
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BE8 + off), buf, 0x2A78, 1);
      nuPiReadWriteSram(*(u32*)((u8*)&D_800C2BEC + off), stat, 4, 1);
    }
    i++;
    off += 0x10;
  } while (i < 2);
  return 1;
}
