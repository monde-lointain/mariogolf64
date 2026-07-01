#include "common.h"

// Game-customized nusys color-framebuffer (CFB) setup. These are the game's
// relocated nusys CFB globals plus a game-custom advance table; the stock
// nuGfxSetCfb calls nuGfxRetraceWait/nuScSetFrameBufferNum, but this game
// variant drives its own D_800B67A4[] rotation state instead.
extern u16** nuGfxCfb;       // frame buffer pointer array
extern u16* nuGfxCfb_ptr;    // current frame buffer pointer
extern u32 nuGfxCfbNum;      // number of frame buffers
extern u32 nuGfxCfbCounter;  // current CFB index

extern u16* D_800B67A0;  // held/extra frame buffer pointer
extern u8 D_800B67A4[];  // CFB index advance table
extern u8 D_800B67A7;    // CFB advance cursor

void cfb_setup(u16** framebuf, u32 num) {
  nuGfxCfb = framebuf;
  nuGfxCfbNum = num;
  nuGfxCfbCounter = 0;
  nuGfxCfb_ptr = framebuf[0];

  if (num == 3) {
    D_800B67A0 = (u16*)0x802EA000;
    D_800B67A4[0] = 1;
    D_800B67A4[1] = 2;
    D_800B67A4[2] = 0;
    D_800B67A7 = 0xFF;
  } else {
    u16* held = framebuf[2];
    D_800B67A4[0] = 1;
    D_800B67A4[2] = 0xFF;
    D_800B67A4[1] = 0;
    D_800B67A7 = 2;
    D_800B67A0 = held;
  }
}

void cfb_set_num(u32 num) {
  if (nuGfxCfbNum == num) {
    return;
  }

  if (num == 3) {
    nuGfxCfbNum = num;
    D_800B67A4[D_800B67A7] = nuGfxCfbCounter;
    D_800B67A4[D_800B67A4[nuGfxCfbCounter]] = D_800B67A7;
    nuGfxCfb_ptr = nuGfxCfb[D_800B67A7];
    D_800B67A0 = (u16*)0x802EA000;
    nuGfxCfbCounter = D_800B67A7;
    D_800B67A7 = 0xFF;
  } else {
    nuGfxCfbNum = num;
    D_800B67A7 = nuGfxCfbCounter;
    nuGfxCfbCounter = D_800B67A4[nuGfxCfbCounter];
    D_800B67A4[D_800B67A4[nuGfxCfbCounter]] = nuGfxCfbCounter;
    nuGfxCfb_ptr = nuGfxCfb[nuGfxCfbCounter];
    D_800B67A0 = nuGfxCfb[D_800B67A7];
  }
}
