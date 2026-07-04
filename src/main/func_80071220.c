#include "common.h"

extern u8* func_8005AF50(void);
extern s32 func_80052220(void);
extern s32 func_8006CD50(s32, s32);
extern void func_800680FC(s32, s32, s32, s32);
extern void func_8005DF54(u8*, s32);

extern s32 scenario_mode_id;
extern s32 D_801B608C;
extern s32 D_801B60B4;

/* stride-6 sentinel table walked from D_801B7118: a signed tag at +0 (the
   -1 terminator) and an unsigned flag byte at +5 (asm loads lb @0, lbu @5). */
typedef struct {
  /* 0x0 */ s8 tag;
  /* 0x1 */ u8 pad[4];
  /* 0x5 */ u8 flags;
} Entry; /* 0x6 */
extern Entry D_801B7118[];

/* array of 0x74-byte per-scenario records at the game-state base + 0xE98 */
typedef struct {
  /* 0x00 */ u8 pad[0x30];
  /* 0x30 */ s32 unk30;
  /* 0x34 */ s32 unk34;
  /* 0x38 */ s32 unk38;
  /* 0x3C */ u8 rest[0x74 - 0x3C];
} ScenarioRec; /* 0x74 */

void func_80071220(void) {
  ScenarioRec* rec = (ScenarioRec*)(func_8005AF50() + 0xE98) + scenario_mode_id;
  s32 set = 0;

  if (D_801B608C == 5) {
    s32 first = func_80052220() + 1;
    s32 second = func_80052220();
    s32 ret = func_8006CD50(second + 1, 0);

    func_800680FC(first, 0, ret, D_801B60B4);

    {
      Entry* table = D_801B7118;
      s32 i;
      s32 sel = 4;
      s32* dst;

      for (i = 0; i != 0x1e; i++) {
        if (table[i].tag == -1) {
          sel = table[i].flags & 0x7f;
          break;
        }
      }

      switch (sel) {
        case 1:
          dst = &rec->unk38;
          set = 1;
          break;
        case 2:
          dst = &rec->unk34;
          set = 1;
          break;
        case 3:
          dst = &rec->unk30;
          set = 1;
          break;
        default:
          dst = &rec->unk30;
          break;
      }
      if (set) {
        *dst = 1;
      }
    }

    func_8005DF54(func_8005AF50(), 1);
  }
}
