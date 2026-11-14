#ifndef BILLBOARD_H
#define BILLBOARD_H

#include <ultra64.h>

#include "camera.h"
#include "language.h"

#define BILLBOARD_COUNT 128
#define BILLBOARD_TEXTURE_COUNT 46
#define BILLBOARD_MASK_SIZE 16

typedef struct Billboard {
    s16 x;
    s16 y;
    s16 z;
    s8 textureIndex;
    u8 shapeIndex;
    UBoolean8 isIgnoredEditorMark;
    UBoolean16 isNearCourseEdge;
    u16 width;
    u16 height;
} Billboard;

typedef struct BillboardTransform {
    s32 viewDepth;
    Mtx translation;
    Mtx scale;
} BillboardTransform;

void DestroyBillboardManager(void);
void InitBillboardManager(void *heapStart, s32 heapSize);
void RevertBillboardManager(void);
void PlaceBillboards(void);
void SortBillboardsByDepth(void);
s32 GetBillboardSolidity(s32 textureIndex, f32 across, f32 down);

extern Billboard g_Billboards[BILLBOARD_COUNT];

void DrawBillboards(CameraMatrices *matrices, s32 cameraIndex);
void DrawFadingBillboards(CameraMatrices *matrices, s32 cameraIndex);

#endif
