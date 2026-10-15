#include "common.h"
#include <nusys.h>
#include "rom_asset.h"

extern s32 D_800FE4A8;
extern s32 D_800C42D4;
/* D_800FF1E9/210/21C/220 are fields within these four records. */
typedef struct {
  /* 0x00 */ u8 is_active;
  /* 0x01 */ u8 unknown_01[0x27];
  /* 0x28 */ s32 score_total;
  /* 0x2C */ u8 unknown_2C[8];
  /* 0x34 */ s32 unknown_34;
  /* 0x38 */ s32 unknown_38;
  /* 0x3C */ u8 unknown_3C[0x50];
} PlayerResultRecord; /* 0x8C */

extern PlayerResultRecord D_800FF1E8[4];
extern s32 D_800FE450[];
extern s32 D_800FE4AC;
extern s32 D_800FE4B0;
extern s32 D_800FE4B4;
extern s32 D_800FE4B8;
extern s32 D_800FEC00;
extern char D_800D1540[];
extern s32 D_80105218;
extern u32 controller_handle_array[];
extern char D_800D14C0[], D_800D14D0[], D_800D14E0[], D_800D14F4[];
extern s32 D_8012D3B0[];
extern char D_800D150C[], D_800D151C[], D_800D152C[];

void func_8006F1A0(s32 arg0, s32 arg1) {
  D_800FF1E8[arg0].score_total += arg1;
  if (D_800FF1E8[arg0].score_total >= 10000) {
    D_800FF1E8[arg0].score_total = 9999;
  }
}

s32 func_8006F1F0(void) { return D_800FE4A8; }

s32 func_8006F1FC(s32 arg0) { return D_800FF1E8[arg0].is_active == 1; }

u8* func_8006F228(s32 arg0) { return D_800FF1E8[arg0].unknown_01; }

void func_8006F24C(s32 arg0) {
  s32 i;
  s32 bound;
  s32* fe = D_800FE450;
  s32 val;
  s32* p;

  D_800FE4B0 = arg0;
  D_800FE4AC = 0;
  if (arg0 == 0) {
    i = 0;
    val = -1;
    bound = 4;
    p = fe + 471;
    do {
      D_800FF1E8[i].is_active = 0;
      D_800FF1E8[i].unknown_34 = val;
      D_800FF1E8[i].unknown_38 = val;
      *p++ = val;
      i++;
    } while (i != bound);
    D_800FE4B4 = 0;
    D_800FEC00 = 0;
    D_800FE4B8 = 0;
  } else {
    D_800FE4B4 = 0;
    D_800FEC00 = 0;
  }
}

void func_8006F2E8(void) { D_800C42D4 = 0; }

s32 func_8006F2F4(void) { return D_800C42D4; }

void func_8006F300(void) {
  NUContPakFile handle;
  s32 i;
  nuContGBPakMgrInit();
  D_800FE4A8 = 0;
  for (i = 0; i < D_80105218; i++) {
    switch (nuContGBPakOpen(&handle, controller_handle_array[i])) {
      case 0:
        osSyncPrintf(D_800D14C0, i);
        D_800FE4A8 = 1;
        break;
      case 1:
        osSyncPrintf(D_800D14D0, i);
        break;
      case 11:
        osSyncPrintf(D_800D14E0, i);
        break;
      default:
        osSyncPrintf(D_800D14F4, i);
        break;
    }
  }
  nuContGBPakMgrRemove();
}

void func_8006F404(void) {
  s32 i;
  s32 result;
  for (i = 0; i != D_80105218; i++) {
    nuContRmbModeSet(controller_handle_array[i], 2);
    result = nuContRmbCheck(controller_handle_array[i]);
    if (result == 1) {
      osSyncPrintf(D_800D150C);
      D_8012D3B0[i] = 0;
    } else if (result == 11) {
      osSyncPrintf(D_800D151C);
      D_8012D3B0[i] = 0;
    } else if (result == 4) {
      osSyncPrintf(D_800D152C);
      D_8012D3B0[i] = 0;
    } else {
      D_8012D3B0[i] = 1;
    }
  }
  func_8006F300();
}

void func_8006F4F0(void) { nuContRmbForceStop(); }

void func_8006F50C(void) {
  nuContRmbForceStop();
  osSyncPrintf(D_800D1540);
}

void func_8006F534(void) {
  nuContGBPakMgrInit();
  nuContGBPakGetStatus(NULL, NULL);
  nuContGBPakPower(NULL, 1);
  nuContGBPakReadWrite(NULL, 1, 0, NULL, 0x20);
  nuContGBPakCheckConnector(NULL, NULL);
  nuContGBPakReadID(NULL, NULL, NULL);
  nuContGBPakReadWrite(NULL, 0, 0, NULL, 0);
  nuContGBPakFread(NULL, 0, NULL, 0);
  nuContGBPakFwrite(NULL, 0, NULL, 0);
}

extern u8* func_8005AF74(void);
extern void func_800718C4(void);
extern void* D_800E1C20;
extern void* D_800E1C24;
extern void* D_800E1C2C;
extern void* D_800E1C28;
extern s8 D_800C42E0;
extern s8 D_800C42E1;
extern s32 D_800C42E4;
extern s32 D_800C42E8;
extern s32 D_800C42EC;
extern s32 D_800C42F0;
extern s32 D_800C42F4;
extern s32 D_800C42F8;
extern s32 D_800C42FC;

void func_8006F5E0(void* destination, s32 kind) {
  RomAssetLoad asset;
  u8* player = func_8005AF74();
  s8 player_kind;

  switch (kind) {
    case 1:
      D_800E1C20 = destination;
      get_rom_asset_allocation_size(0x70F, &asset);
      load_rom_asset_payload(D_800E1C20, &asset);
      break;
    case 2:
      D_800E1C24 = destination;
      get_rom_asset_allocation_size(0x720, &asset);
      load_rom_asset_payload(D_800E1C24, &asset);
      break;
    case 3:
      D_800E1C2C = destination;
      get_rom_asset_allocation_size(0x833, &asset);
      load_rom_asset_payload(D_800E1C2C, &asset);
      break;
  }
  D_800C42E0 = 0;
  D_800C42E1 = 0;
  D_800C42E4 = 0;
  D_800C42E8 = 0;
  D_800C42EC = 0;
  D_800C42F0 = 0;
  D_800C42F4 = 0;
  D_800C42F8 = 0;
  player_kind = (s8)player[0x20];
  if (player_kind < 5) {
    D_800C42FC = 0;
  } else if (player_kind + 4 < 16) {
    D_800C42FC = player_kind;
  } else {
    D_800C42FC = 12;
  }
  func_800718C4();
}

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F734);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006FE88);

void func_800708B4(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPSetPrimColor(gfx++, 0, 0, 0, 0xDC, 0xE6, 0xD2);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
  gDPSetCombineLERP(gfx++, TEXEL0, K5, PRIMITIVE, COMBINED_ALPHA,
                    PRIMITIVE, 0, TEXEL0, 0,
                    TEXEL0, K5, PRIMITIVE, COMBINED_ALPHA,
                    PRIMITIVE, 0, TEXEL0, 0);
  gDPLoadTextureBlock_4b(gfx++, (void*)(((u32)D_800E1C28 + 8) & ~7),
                         G_IM_FMT_I, 128, 64, 0,
                         G_TX_CLAMP, G_TX_CLAMP, 0, 0, 0, 0);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, 0xA0, 0xC8, 0x460, 0x398, 0,
                      0, 0, 0x222, 0x16C);
  gDPPipeSync(gfx++);
  *gfxp = gfx;
}

void func_80070BCC(Gfx** gfxp) {
  Gfx* gfx = *gfxp;
  s32 i;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPLoadTLUT_pal256(gfx++, (void*)(((u32)D_800E1C2C + 8) & ~7));
  for (i = 0; i != 20; i++) {
    gDPLoadTextureBlock(gfx++, (void*)(((u32)D_800E1C2C + 0x208 + i * 0x800) & ~7),
                        G_IM_FMT_CI, G_IM_SIZ_8b, 256, 8, 0,
                        G_TX_WRAP, G_TX_WRAP, 0, 0, 0, 0);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, 0x90, 0xA0 + i * 0x24,
                            0x490, 0xC4 + i * 0x24, 0,
                            0, 0, 0x400, 0x38E);
    gDPPipeSync(gfx++);
  }
  *gfxp = gfx;
}
