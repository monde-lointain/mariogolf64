#include "common.h"
#include "../main/terrain.h"
#include "../main/club.h"

typedef struct {
    u8 pad00[0x18];
    s32 unk18;
    u8 pad1C[0x4];
    s32 unk20;
} Ovl7ArgObject;

typedef struct {
    u8 pad00[0x18];
    Ovl7ArgObject **argSlot;
} Ovl7NestFrame;

extern TerrainAttrEntry D_ovl7_8020B564[27];
extern s32 rand(void);
extern s32 func_800542A0(s32 index);
extern s32 clear_animation_slot(s32 index);
extern void load_character_model(s32 id, s32 model, s32 arg2);
extern void animation_play(s32 id, s32 animation);
extern void func_800562F4(s32 id, f32 value);

s32 func_ovl7_80206680(s32 arg0, s32 arg1) {
  if (func_800542A0(arg0) != 0) {
    clear_animation_slot(arg0);
    return 0;
  }
  load_character_model(arg0, arg1, 4);
  animation_play(arg0, 0xD);
  func_800562F4(arg0, 1.0f);
  return 1;
}

TerrainAttrEntry* func_ovl7_802066F0(u32 arg0) {
  u32 i = (arg0 < 0x1BU) ? arg0 : 0;
  return &D_ovl7_8020B564[i];
}

void func_ovl7_80206718(void) {
  s32 i;

  for (i = 0; i <= 0; i++) {
    D_800B7868[i].unknown_00 = 0x800;
    D_800B7868[i].unknown_04 = -1000;
    rand();
    D_800B7868[i].unknown_08 = 0x1000;
    D_800B7868[i].unknown_0C = rand() & 0x3FF;
    D_800B7868[i].unknown_10 = -100;
    D_800B7868[i].unknown_14 = rand() & 0x3FF;
    D_800B7868[i].unknown_18 = 0;
    D_800B7868[i].unknown_74 = 1;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206800);

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original; same stand-in as lerp_s32 (func_8008D100.c). */
f32 func_ovl7_802068B4(f32 x, f32 y, f32 z) {
    s32 r;
    volatile s32 unused = r;
    return sqrtf(x * x + y * y + z * z);
}

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original, as in func_ovl7_802068B4. */
f32 func_ovl7_802068EC(f32 arg0, f32 arg1) {
    s32 r;
    volatile s32 unused = r;
    return sqrtf(arg0 * arg0 + arg1 * arg1);
}

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original, as in func_ovl7_802068B4. */
void func_ovl7_80206910(f32 arg0, f32 *arg1, f32 *arg2, f32 *arg3) {
    s32 r;
    volatile s32 unused = r;
    f32 scale;

    scale = arg0 / sqrtf(*arg1 * *arg1 + *arg2 * *arg2 + *arg3 * *arg3);
    *arg1 *= scale;
    *arg2 *= scale;
    *arg3 *= scale;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206968);

/* Static-chain home of the nested-function original: the chain word is the parent frame pointer and its 0x18 slot holds &arg0. The volatile frame access keeps `sw $v0,0(sp)` ahead of the chain loads, as in func_8004DE60. */
f32 func_ovl7_80206A50(void) {
    s32 r;
    volatile s32 unused = r;
    Ovl7ArgObject *obj;
    s32 sum;

    obj = *((volatile Ovl7NestFrame *) r)->argSlot;
    sum = (obj->unk18 + obj->unk20) & 0xFF;
    return ((f32) sum - 127.5f) * 0.007843138f;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_80206AA4);

/* Dead 8-byte frame and `sw $v0,0(sp)`: static-chain home of the nested-function original, as in func_ovl7_802068B4. */
f32 func_ovl7_8020A474(void) {
    s32 r;
    volatile s32 unused = r;
    return 0.0f;
}

INCLUDE_ASM("asm/nonmatchings/overlay_7/func_ovl7_80206680", func_ovl7_8020A488);
