#include "common.h"
#include <nusys.h>

extern s32 D_800FE4A8;
extern s32 D_800C42D4;
extern u8 D_800FF1E8[];
extern u8 D_800FF1E9[];
extern s32 D_800FF210[];
extern s32 D_800FF21C[];
extern s32 D_800FF220[];
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
  s32 off = arg0 * 140;
  *(s32*)((u8*)D_800FF210 + off) += arg1;
  if (*(s32*)((u8*)D_800FF210 + off) >= 10000) {
    *(s32*)((u8*)D_800FF210 + off) = 9999;
  }
}

s32 func_8006F1F0(void) { return D_800FE4A8; }

s32 func_8006F1FC(s32 arg0) { return D_800FF1E8[arg0 * 140] == 1; }

u8* func_8006F228(s32 arg0) { return &D_800FF1E9[arg0 * 140]; }

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
      s32 off = i * 140;
      *(u8*)((u8*)D_800FF1E8 + off) = 0;
      *(s32*)((u8*)D_800FF21C + off) = val;
      *(s32*)((u8*)D_800FF220 + off) = val;
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

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F5E0);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006F734);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_8006FE88);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_800708B4);

INCLUDE_ASM("asm/nonmatchings/main/func_8006F1A0", func_80070BCC);
