#include "common.h"

extern Gfx D_ovl3_80232300[];
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C2004;
extern s32 current_game_mode;
extern void func_ovl3_801FB54C(Gfx** dl, s32 flags);
extern void func_80074D0C(Gfx** dl, s16 x, s16 y, u8* str);
extern u8 D_ovl3_80231198[];
extern u8 D_ovl3_80231284[];

Gfx* func_ovl3_801F4A40(void) { return D_ovl3_80232300; }

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F4A4C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F4C34);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F4C78);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F6A38);

void func_ovl3_801F6B20(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F6B28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F70F0);

extern s32 D_800FBD40[];
extern u8 D_801323A0[];
extern s32 D_ovl3_8022BE0C;

void func_ovl3_801F75EC(void) {
    u8 temp_v0;

    D_801323A0[2] = 0U;
    if (D_800FBD40[2] == 1) {
        do {
            temp_v0 = D_801323A0[2] + 1;
            D_801323A0[2] = temp_v0;
            if ((D_ovl3_8022BE0C - 1) < (s8) temp_v0) {
                D_801323A0[2] = 0U;
            }
        } while (*(&D_800FBD40[2] + (s8) D_801323A0[2]) == 1);
    }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7660);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7CD0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7D28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F8BAC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F93D8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9BCC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9C58);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9E38);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F9FF8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FA1B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FA20C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FADBC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FAF08);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FAF78);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FAFEC);

void func_ovl3_801FB11C(void) {
  if (D_800C2004 & 0x4000) {
    play_sound_effect(0x5F, 0xF, 0x7F);
    current_game_mode = 2;
  }
}

void func_ovl3_801FB15C(void) {
  if (D_800C2004 & 0x8000) {
    play_sound_effect(0x5D, 0xF, 0x50);
    current_game_mode = 0;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FB198);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FB54C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FC60C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FC754);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FCB74);

extern void* D_801B60D0;

void func_ovl3_801FCEF8(Gfx** gfxp) {
  Gfx* gfx;
  s32 i;

  gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, (u8*)((((u32)D_801B60D0) + 8) & ~7));
  for (i = 0; i != 3; i++) {
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)((((u32)D_801B60D0) + 0x208 + i * 0x7D0) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3E7, 0x52);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 25, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 199 << G_TEXTURE_IMAGE_FRAC,
                   9 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 60 << G_TEXTURE_IMAGE_FRAC,
                            (16 + i * 0xA) << G_TEXTURE_IMAGE_FRAC,
                            260 << G_TEXTURE_IMAGE_FRAC,
                            (26 + i * 0xA) << G_TEXTURE_IMAGE_FRAC,
                            G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
  }

  *gfxp = gfx;
}

void func_ovl3_801FD1B4(Gfx** gfxp, s32 x, s32 y) {
  Gfx* gfx;
  s32 i;

  gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, (u8*)((((u32)D_801B60D0) + 8) & ~7));
  for (i = 0; i != 3; i++) {
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)((((u32)D_801B60D0) + 0x208 + i * 0x7D0) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3E7, 0x52);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 25, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 199 << G_TEXTURE_IMAGE_FRAC,
                   9 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, x << G_TEXTURE_IMAGE_FRAC,
                            (y + i * 0xA) << G_TEXTURE_IMAGE_FRAC,
                            (x + 0x60) << G_TEXTURE_IMAGE_FRAC,
                            (y + i * 0xA + 0xA) << G_TEXTURE_IMAGE_FRAC,
                            G_TX_RENDERTILE, 0, 0, 0x855, 0x400);
    gDPPipeSync(gfx++);
  }

  *gfxp = gfx;
}

void func_ovl3_801FD4F0(Gfx** gfxp) {
  Gfx* gfx;
  s32 i;

  gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, (u8*)((((u32)D_801B60D0) + 8) & ~7));
  for (i = 0; i != 3; i++) {
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)((((u32)D_801B60D0) + 0x208 + i * 0x7D0) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3E7, 0x52);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 25, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 199 << G_TEXTURE_IMAGE_FRAC,
                   9 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 0x68,
                            (0xAE + i * 0x10) << G_TEXTURE_IMAGE_FRAC,
                            0x4A0,
                            (0xC2 + i * 0x10) << G_TEXTURE_IMAGE_FRAC,
                            G_TX_RENDERTILE, 0, 0, 0x2F6, 0x200);
    gDPPipeSync(gfx++);
  }

  *gfxp = gfx;
}

extern void* D_801052BC[];
extern s16 D_ovl3_8022BE8C;

void func_ovl3_801FD7B0(Gfx** gfxp) {
  Gfx* gfx;
  u32 buf;
  s32 i;

  buf = (u32)D_801052BC[D_ovl3_8022BE8C];
  gfx = *gfxp;
  gDPLoadTLUT_pal256(gfx++, (u8*)((buf + 8) & ~7));
  for (i = 0; i != 3; i++) {
    gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)((buf + 0x208 + i * 0x7D0) & ~7));
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPLoadSync(gfx++);
    gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3E7, 0x52);
    gDPPipeSync(gfx++);
    gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 25, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
    gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 199 << G_TEXTURE_IMAGE_FRAC,
                   9 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 60 << G_TEXTURE_IMAGE_FRAC,
                            (18 + i * 0xA) << G_TEXTURE_IMAGE_FRAC,
                            260 << G_TEXTURE_IMAGE_FRAC,
                            (28 + i * 0xA) << G_TEXTURE_IMAGE_FRAC,
                            G_TX_RENDERTILE, 0, 0, 0x400, 0x400);
    gDPPipeSync(gfx++);
  }

  *gfxp = gfx;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FDA40);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FE4D0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802004A0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80200A40);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80200EF0);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80201574);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80202000);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802047F0);

void func_ovl3_80204FF0(Gfx** arg0) {
  Gfx* saved = *arg0;

  func_80074D0C(&saved, 0x60, 0x40, D_ovl3_80231198);
  *arg0 = saved;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80205038);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802058B8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80206180);

void func_ovl3_80206A2C(Gfx** arg0) {
  Gfx* saved = *arg0;

  func_ovl3_801FB54C(&saved, 0x3D);
  *arg0 = saved;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80206A68);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80207D08);

void func_ovl3_80207E18(void) {
}

void func_ovl3_80207E20(void) {
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80207E28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_802081E8);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_8020879C);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80208F8C);

void func_ovl3_80209430(Gfx** arg0) {
  Gfx* saved = *arg0;

  func_80074D0C(&saved, 0x40, 0x20, D_ovl3_80231284);
  *arg0 = saved;
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_80209478);
