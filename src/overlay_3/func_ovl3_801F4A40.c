#include "common.h"

extern Gfx D_ovl3_80232300[];
extern void play_sound_effect(s32 sfx, s32 channel, s32 volume);
extern s32 D_800C2004;
extern s32 D_800C2000;
extern s32 current_game_mode;
extern s32 game_mode;
extern s32 func_800959F8(void);
extern s32 transfer_mode_continue_slot(s32 restore);
extern void func_800543A4(void);
extern s32 func_ovl3_801F7660(s32, s32);
extern u32 player_context_offset;
extern void func_ovl3_801FB54C(Gfx** dl, s32 flags);
extern void func_ovl3_801FC754(Gfx** gfxp);
extern void func_ovl3_801FCB74(Gfx** gfxp);
extern void func_ovl3_801FD4F0(Gfx** gfxp);
extern void func_ovl3_801FD7B0(Gfx** gfxp);
extern void func_80074D0C(Gfx** dl, s16 x, s16 y, u8* str);
extern u8 D_ovl3_80231198[];
extern u8 D_ovl3_80231284[];
extern u8 D_801052E8;
extern u8 D_801B9410;
extern s8 D_801323B8[];
extern s32 func_8006AD94(void);
extern s32 func_8006ADA0(void);
extern void func_8006AD1C(s32 table_index);
extern void func_8006ADF8(s32 arg);
extern void func_8009226C(s32 arg0);
extern void play_bgm_by_id(s32 id, s32 flags);
extern u8 D_801323E5[];
extern s16 D_ovl3_8022BF1C[];
extern s32 flag_is_set(s32 flag);
extern void func_ovl3_80217B88(s32 arg0);
extern s32 active_player_count;
extern u8 D_800FF4C0;
extern u8 D_80105268;
extern u8 D_801B5634;

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

void func_ovl3_801F7CD0(void) {
  s32 ready;

  ready = func_800959F8();
  if ((ready == 1) && (func_ovl3_801F7660(1, 2) == -1)) {
    player_context_offset = ready;
    current_game_mode = 0;
    func_800543A4();
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F7D28);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F8BAC);

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801F93D8);

void func_ovl3_801F9BCC(void) {
    if (func_800959F8() == 1) {
        if (D_800C2004 & 0x8000) {
            play_sound_effect(0x5D, 0xF, 0x50);
            current_game_mode = 0x13;
            game_mode = 0;
            transfer_mode_continue_slot(1);
            return;
        }
        if (D_800C2004 & 0x4000) {
            play_sound_effect(0x5F, 0xF, 0x7F);
            current_game_mode = 0;
        }
    }
}

void func_ovl3_801F9C58(void) {
    s32 ret;
    s32 next_mode;

    ret = func_800959F8();
    if (ret != 1) {
        return;
    }
    if (D_800C2004 & 0x8000) {
        switch ((s8)D_801323B8[3]) {
        case 0:
            next_mode = 9;
            break;
        case 2:
            func_8009226C(8);
            func_ovl3_80228B60();
            next_mode = 0x1B;
            break;
        case 1:
            func_8009226C(8);
            current_game_mode = 0x11;
            if (flag_is_set(0x56) != 0) {
                game_mode = 4;
                active_player_count = 2;
                D_800FF4C0 = 1;
                D_80105268 = 2;
                D_801B5634 = 0;
                func_ovl3_80217B88(0xA);
            }
            goto sound;
        case 3:
            func_8009226C(8);
            next_mode = 4;
            break;
        default:
            goto sound;
        }
        current_game_mode = next_mode;
sound:
        play_sound_effect(0x5D, 0xF, 0x50);
    } else {
        if (D_800C2004 & 0x4000) {
            play_sound_effect(0x5F, 0xF, 0x7F);
            current_game_mode = 0;
            play_bgm_by_id(0x27, 0);
            return;
        }
        if (D_800C2000 & 0x800) {
            play_sound_effect(0x5C, 0xF, 0x64);
            D_801323B8[3] = (u8)D_801323B8[3] - 1;
        } else if (D_800C2000 & 0x400) {
            play_sound_effect(0x5C, 0xF, 0x64);
            D_801323B8[3] = (u8)D_801323B8[3] + 1;
        }
        if (D_801323B8[3] >= 4) {
            D_801323B8[3] = 0;
        }
        if (D_801323B8[3] < 0) {
            D_801323B8[3] = 3;
        }
    }
}

void func_ovl3_801F9E38(void) {
    s32 ret;
    s32 mode;
    s8 *cursor;
    s8 new_value;

    ret = func_800959F8();
    if (ret == 1) {
        if (D_800C2004 & 0x8000) {
            if (D_801323B8[4] != ret) {
                if (D_801323B8[4] < 2) {
                    mode = 9;
                    if (D_801323B8[4] == 0) {
                        goto store_mode;
                    }
                } else if (D_801323B8[4] == 2) {
                    goto cursor_two;
                }
                goto play_sound;
            } else {
                func_8009226C(8);
                current_game_mode = 0x11;
                if (flag_is_set(0x56) != 0) {
                    game_mode = 4;
                    active_player_count = 2;
                    D_800FF4C0 = 1;
                    D_80105268 = 2;
                    D_801B5634 = 0;
                    func_ovl3_80217B88(0xA);
                }
                goto play_sound;
            }
cursor_two:
            func_8009226C(8);
            mode = 4;
store_mode:
            current_game_mode = mode;
play_sound:
            play_sound_effect(0x5D, 0xF, 0x50);
        } else {
            if (D_800C2004 & 0x4000) {
                play_sound_effect(0x5F, 0xF, 0x7F);
                current_game_mode = 0;
                play_bgm_by_id(0x27, 0);
                return;
            }
            if (D_800C2000 & 0x800) {
                play_sound_effect(0x5C, 0xF, 0x64);
                cursor = &D_801323B8[4];
                new_value = (u8)*cursor - 1;
                goto store_cursor;
            }
            if (D_800C2000 & 0x400) {
                play_sound_effect(0x5C, 0xF, 0x64);
                cursor = &D_801323B8[4];
                new_value = (u8)*cursor + 1;
store_cursor:
                *cursor = new_value;
            }
            cursor = &D_801323B8[4];
            if (*cursor >= 3) {
                *cursor = 0;
            }
            if (*cursor < 0) {
                *cursor = 2;
            }
        }
    }
}

void func_ovl3_801F9FF8(void) {
    s32 ret;

    ret = func_800959F8();
    if (ret != 1) {
        return;
    }
    if (D_800C2004 & 0x8000) {
        switch (D_ovl3_8022BF1C[D_801323B8[5]]) {
        case 0:
            func_8009226C(8);
            current_game_mode = 0xB;
            break;
        case 1:
            func_8009226C(8);
            current_game_mode = 0xC;
            break;
        case 2:
            func_8009226C(8);
            current_game_mode = 0xD;
            break;
        case 3:
            func_8009226C(8);
            current_game_mode = 0xE;
            break;
        case 4:
            func_8009226C(8);
            current_game_mode = 0xF;
            break;
        case 5:
            func_8009226C(8);
            current_game_mode = 7;
            break;
        }
        play_sound_effect(0x5D, 0xF, 0x50);
        return;
    }
    if (D_800C2004 & 0x4000) {
        play_sound_effect(0x5F, 0xF, 0x7F);
        if (D_801323E5[1] == ret) {
            current_game_mode = 2;
        } else {
            current_game_mode = 0x1C;
        }
        play_bgm_by_id(0x27, 0);
        return;
    }
    if (D_800C2000 & 0x800) {
        play_sound_effect(0x5C, 0xF, 0x64);
        D_801323B8[5] = (u8)D_801323B8[5] - 1;
    } else if (D_800C2000 & 0x400) {
        play_sound_effect(0x5C, 0xF, 0x64);
        D_801323B8[5] = (u8)D_801323B8[5] + 1;
    }
    if (D_801323B8[5] >= 5) {
        D_801323B8[5] = 0;
    }
    if (D_801323B8[5] < 0) {
        D_801323B8[5] = 4;
    }
}

void func_ovl3_801FA1B8(void) {
  if (func_800959F8() == 1 && (D_800C2004 & 0x4000)) {
    play_sound_effect(0x5F, 0xF, 0x50);
    current_game_mode = 2;
  }
}

INCLUDE_ASM("asm/nonmatchings/overlay_3/func_ovl3_801F4A40", func_ovl3_801FA20C);

void func_ovl3_801FADBC(void) {
  s8 *cursor;
  s8 new_value;

  if (func_800959F8() == 1) {
    if (func_8006AD94() != 0) {
      if (func_8006ADA0() == 0) {
        current_game_mode = 0x13;
      }
    } else {
      if (D_800C2004 & 0x8000) {
        play_sound_effect(0x50, 0xF, 0x50);
        func_8006ADF8(1);
        return;
      }
      if (D_800C2004 & 0x4000) {
        play_sound_effect(0x5F, 0xF, 0x7F);
        if ((s8)D_801323A0[0xB] == 5) {
          current_game_mode = (s8)D_801323A0[0xB];
          return;
        }
        current_game_mode = 6;
        return;
      }
      if (D_800C2000 & 0x800) {
        cursor = &D_801323B8[1];
        if (D_801323B8[1] > 0) {
          play_sound_effect(0x5C, 0xF, 0x64);
          new_value = (u8)D_801323B8[1] - 1;
          goto store_cursor;
        }
      }
      if (D_800C2000 & 0x400) {
        cursor = &D_801323B8[1];
        if (D_801323B8[1] <= 0) {
          play_sound_effect(0x5C, 0xF, 0x64);
          new_value = (u8)D_801323B8[1] + 1;
store_cursor:
          *cursor = new_value;
        }
      }
    }
  }
}

void func_ovl3_801FAF08(void) {
  if (func_8006AD94() != 0 && D_801B9410 == 0) {
    if (func_8006ADA0() == 0) {
      current_game_mode = 0x13;
    }
  } else {
    func_8006AD1C(0);
    func_8006ADF8(1);
    D_801052E8 = 0;
    D_801B9410 = 0;
  }
}

void func_ovl3_801FAF78(void) {
  if (func_8006AD94() != 0 && D_801B9410 == 0) {
    if (func_8006ADA0() == 0) {
      current_game_mode = 0x13;
    }
  } else {
    func_8006AD1C(D_801323B8[0x17]);
    func_8006ADF8(1);
    D_801052E8 = 0;
    D_801B9410 = 0;
  }
}

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

void func_ovl3_801FC60C(Gfx** gfxp, s32 flags) {
  Gfx* gfx;

  gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  if (flags & 1) {
    func_ovl3_801FC754(&gfx);
  }
  if (flags & 4) {
    func_ovl3_801FCB74(&gfx);
  }
  if (flags & 0x10) {
    func_ovl3_801FD4F0(&gfx);
  }
  if (flags & 8) {
    func_ovl3_801FD7B0(&gfx);
  }
  gDPPipeSync(gfx++);
  *gfxp = gfx;
}

extern void* D_801F4A20;

void func_ovl3_801FC754(Gfx** gfxp) {
  Gfx* gfx;
  s32 i;
  s32 n;

  gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, (u8*)((((u32)D_801F4A20) + 8) & ~7));
  for (i = 0; i != 0xB; i++) {
    n = (i == 0xA) ? 1 : 11;
    if (i == 0) {
      gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                         (u8*)((((u32)D_801F4A20) + 0x208) & ~7));
      gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
      gDPLoadSync(gfx++);
      gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, 0x3DB, 0x6C);
      gDPPipeSync(gfx++);
      gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 19, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 151 << G_TEXTURE_IMAGE_FRAC,
                     12 << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, 8 << G_TEXTURE_IMAGE_FRAC,
                              8 << G_TEXTURE_IMAGE_FRAC,
                              312 << G_TEXTURE_IMAGE_FRAC,
                              32 << G_TEXTURE_IMAGE_FRAC,
                              G_TX_RENDERTILE, 0, 0, 0x200, 0x200);
    } else {
      gDPPipeSync(gfx++);
      gDPSetTextureImage(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                         (u8*)((((u32)D_801F4A20) + 0x208 + i * 0x688) & ~7));
      gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0, 0, 0, 0);
      gDPLoadSync(gfx++);
      gDPLoadBlock(gfx++, G_TX_LOADTILE, 0, 0, (n + 2) * 0x4C - 1, 0x6C);
      gDPPipeSync(gfx++);
      gDPSetTile(gfx++, G_IM_FMT_CI, G_IM_SIZ_8b, 19, 0, G_TX_RENDERTILE, 0, 0, 0, 0, 0, 0, 0);
      gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, 151 << G_TEXTURE_IMAGE_FRAC,
                     (n + 1) << G_TEXTURE_IMAGE_FRAC);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(gfx++, 8 << G_TEXTURE_IMAGE_FRAC,
                              (10 + i * 22) << G_TEXTURE_IMAGE_FRAC,
                              312 << G_TEXTURE_IMAGE_FRAC,
                              (i * 22 + 10 + 2 * n) << G_TEXTURE_IMAGE_FRAC,
                              G_TX_RENDERTILE, 0, 0x20, 0x200, 0x200);
    }
  }
  gDPPipeSync(gfx++);

  *gfxp = gfx;
}

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
