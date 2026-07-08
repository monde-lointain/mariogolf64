#include "common.h"

typedef struct FaultFlagLabel {
  u32 mask;
  u32 value;
  const char* label;
} FaultFlagLabel;

extern s32 D_800C2C14;
extern char D_800FF428[];
extern const char D_800D11D8[];
extern const char D_800D11E4[];
extern const char D_800D11E8[];
extern const char D_800D11EC[];
extern const char D_800D11F0[];

extern void func_8004D580(char* str, s32 x, s32 y);
extern void func_8005E360(void);

INCLUDE_ASM("asm/nonmatchings/main/func_8005E380", func_8005E380);

void func_8005EAD4(u32 flags, const char* name, FaultFlagLabel* labels) {
  u32 mask;
  s32 first;

  sprintf(D_800FF428, D_800D11D8, name, flags);
  func_8005E360();
  first = TRUE;
  sprintf(D_800FF428, D_800D11E4);
  func_8005E360();

  mask = labels->mask;
  if (mask != 0) {
    do {
      if ((flags & mask) == labels->value) {
        if (first) {
          first = FALSE;
        } else {
          sprintf(D_800FF428, D_800D11E8);
          func_8005E360();
        }

        sprintf(D_800FF428, D_800D11EC, labels->label);
        func_8005E360();
        func_8004D580(D_800FF428, 3, D_800C2C14++);
      }

      labels++;
      mask = labels->mask;
    } while (mask != 0);
  }

  sprintf(D_800FF428, D_800D11F0);
  func_8005E360();
}
