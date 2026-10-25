#include "common.h"
#include "asset.h"

#define CURSOR_BOX_ASSET 0x6EF

void *sCursorBoxImage = NULL;
Vtx sCursorBoxVertices[] = {
    { { { -24, 24, 0 }, 0, { 0, 0 }, { 0xFF, 0x00, 0x00, 0xFF } } },
    { { { -24, -24, 0 }, 0, { 0, 0xC00 }, { 0x00, 0xFF, 0x00, 0xFF } } },
    { { { 24, -24, 0 }, 0, { 0xC00, 0xC00 }, { 0x00, 0x00, 0xFF, 0xFF } } },
    { { { 24, 24, 0 }, 0, { 0xC00, 0 }, { 0xFF, 0xFF, 0x00, 0xFF } } },
};
f32 sCursorCornerPhases[] = { 0.0f, 1.5707964f, 3.1415927f, 4.712389f };

/* Reads the cursor box image into the given buffer and sizes the quad to it */
void LoadCursorBox(void *image) {
    EncodedAsset asset;
    /* s32 sizes are there to match */
    s32 width;
    s32 height;

    sCursorBoxImage = image;
    OpenEncodedAsset(CURSOR_BOX_ASSET, &asset);
    ReadEncodedAsset(sCursorBoxImage, &asset);
    width = (((u8 *)sCursorBoxImage)[2] << 8) | ((u8 *)sCursorBoxImage)[3];
    height = (((u8 *)sCursorBoxImage)[4] << 8) | ((u8 *)sCursorBoxImage)[5];
    sCursorBoxVertices[0].v.tc[0] = 0;
    sCursorBoxVertices[1].v.tc[0] = 0;
    sCursorBoxVertices[0].v.tc[1] = 0;
    sCursorBoxVertices[3].v.tc[1] = 0;
    sCursorBoxVertices[0].v.ob[0] = -width / 2;
    sCursorBoxVertices[1].v.ob[0] = -width / 2;
    sCursorBoxVertices[2].v.ob[0] = width / 2;
    sCursorBoxVertices[3].v.ob[0] = width / 2;
    sCursorBoxVertices[0].v.ob[1] = height / 2;
    sCursorBoxVertices[1].v.ob[1] = -height / 2;
    sCursorBoxVertices[2].v.ob[1] = -height / 2;
    sCursorBoxVertices[3].v.ob[1] = height / 2;
    sCursorBoxVertices[2].v.tc[0] = width << 6;
    sCursorBoxVertices[3].v.tc[0] = width << 6;
    sCursorBoxVertices[1].v.tc[1] = height << 6;
    sCursorBoxVertices[2].v.tc[1] = height << 6;
}

INCLUDE_ASM("asm/nonmatchings/ovl_13/cursor_box", DrawCursorBox);
