#include "common.h"

typedef struct {
  /* 0x0 */ u32 start;
  /* 0x4 */ u32 size;
  /* 0x8 */ u32 pos;
  /* 0xC */ u32 end;
} RomLoadSlot; /* 0x10 */

extern u32 func_8005062C(u16 index, RomLoadSlot* slot);
extern void func_800506D4(void* data, RomLoadSlot* slot);

/* Destination the caller preallocated for asset 0x6EF; its first six bytes are
 * the image header, big-endian width at +2 and height at +4. */
extern u8* D_ovl15_801FBBE0;

/* The textured quad the image is drawn on, centered on the origin. Its initial
 * .data contents describe a 48x48 sprite (+/-24 position, 0x0C00 = 48 << 6
 * texcoord), which is what this function rewrites to the loaded image's size.
 */
extern Vtx D_ovl15_801FBBE8[4];

/* Loads the image asset into the caller's buffer and fits the quad to it. The
 * corners run v0 top-left, v1 bottom-left, v2 bottom-right, v3 top-right; the
 * assignments below are grouped by vertex attribute rather than by vertex,
 * which is the order the ROM emits. */
void func_ovl15_801FB400(u8* image) {
  RomLoadSlot slot[2];
  u32 width;
  u32 height;

  D_ovl15_801FBBE0 = image;
  func_8005062C(0x6EF, slot);
  func_800506D4(D_ovl15_801FBBE0, slot);

  width = (D_ovl15_801FBBE0[2] << 8) | D_ovl15_801FBBE0[3];
  height = (D_ovl15_801FBBE0[4] << 8) | D_ovl15_801FBBE0[5];

  D_ovl15_801FBBE8[0].v.ob[0] = -(s32)width / 2;
  D_ovl15_801FBBE8[1].v.ob[0] = -(s32)width / 2;
  D_ovl15_801FBBE8[2].v.ob[0] = width / 2;
  D_ovl15_801FBBE8[3].v.ob[0] = width / 2;

  D_ovl15_801FBBE8[0].v.ob[1] = height / 2;
  D_ovl15_801FBBE8[1].v.ob[1] = -(s32)height / 2;
  D_ovl15_801FBBE8[2].v.ob[1] = -(s32)height / 2;
  D_ovl15_801FBBE8[3].v.ob[1] = height / 2;

  D_ovl15_801FBBE8[0].v.tc[0] = 0;
  D_ovl15_801FBBE8[1].v.tc[0] = 0;
  D_ovl15_801FBBE8[2].v.tc[0] = width << 6;
  D_ovl15_801FBBE8[3].v.tc[0] = width << 6;

  D_ovl15_801FBBE8[0].v.tc[1] = 0;
  D_ovl15_801FBBE8[1].v.tc[1] = height << 6;
  D_ovl15_801FBBE8[2].v.tc[1] = height << 6;
  D_ovl15_801FBBE8[3].v.tc[1] = 0;
}
