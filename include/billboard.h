#ifndef BILLBOARD_H
#define BILLBOARD_H

#include <ultra64.h>

/*
 * Billboards are flat sprites that always face the camera, such as the
 * trees around a course. Each one shows one of the billboard textures.
 */

#define BILLBOARD_COUNT 128
#define BILLBOARD_TEXTURE_COUNT 46
/* A billboard texture's hit mask has this many rows of this many cells */
#define BILLBOARD_MASK_SIZE 16

typedef struct Billboard {
  /* 0x0 */ s16 x;
  /* 0x2 */ s16 y;
  /* 0x4 */ s16 z;
  /* 0x6 */ s8 textureIndex; /* Negative when the billboard is unused */
  /* 0x7 */ u8 shapeIndex;
  /* 0x8 */ u8 unk8;
  /* 0x9 */ u8 unk9;
  /* 0xA */ u16 isNearCourseEdge;
  /* 0xC */ u16 width;
  /* 0xE */ u16 height;
} Billboard; /* size = 0x10 */

/* The matrices that place one billboard, rebuilt every frame */
typedef struct BillboardTransform {
  /* 0x00 */ s32 viewDepth;
  /* 0x08 */ Mtx translation;
  /* 0x48 */ Mtx scale;
} BillboardTransform; /* size = 0x88 */

void DestroyBillboardManager(void);
void InitBillboardManager(void* heapStart, s32 heapSize);
void RevertBillboardManager(void);
void PlaceBillboards(void);
void SortBillboardsByDepth(void);
s32 GetBillboardSolidity(s32 textureIndex, f32 across, f32 down);

extern Billboard g_Billboards[BILLBOARD_COUNT];
extern BillboardTransform g_BillboardTransforms[BILLBOARD_COUNT];

#endif
