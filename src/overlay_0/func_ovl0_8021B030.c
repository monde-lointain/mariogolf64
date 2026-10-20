#include "common.h"

/* Frame of func_ovl0_8021B79C as seen through the static chain ($v0 = parent sp+0x10). */
typedef struct {
    u8 pad[0x28];
    Gfx* gfx;
} Ovl0Func8021B79CFrame;

/* Nested function of func_ovl0_8021B79C (see its frame typedef above). Emits one scissored
 * 16x8 texture rectangle around (x, y); `mode` selects the corner offsets, wrapped in pipe syncs. */
void func_ovl0_8021B030(s32 x, s32 y, s32 mode) {
    Ovl0Func8021B79CFrame* r;
    struct { Ovl0Func8021B79CFrame* p; } volatile unused;
    Ovl0Func8021B79CFrame* chain;

    unused.p = r;
    chain = r;

    gDPPipeSync(chain->gfx++);
    switch (mode) {
    case 0:
        gSPScisTextureRectangle(chain->gfx++, (x - 22) << 2, (y - 12) << 2, (x - 6) << 2,
                                (y - 4) << 2, 0, 0, 0, 0x400, 0x400);
        break;
    case 1:
        gSPScisTextureRectangle(chain->gfx++, (x + 6) << 2, (y - 12) << 2, (x + 22) << 2,
                                (y - 4) << 2, 0, 0x200, 0, 0x400, 0x400);
        break;
    case 2:
        gSPScisTextureRectangle(chain->gfx++, (x + 6) << 2, (y + 4) << 2, (x + 22) << 2,
                                (y + 12) << 2, 0, 0x200, 0x100, 0x400, 0x400);
        break;
    case 3:
        gSPScisTextureRectangle(chain->gfx++, (x - 22) << 2, (y + 4) << 2, (x - 6) << 2,
                                (y + 12) << 2, 0, 0, 0x100, 0x400, 0x400);
        break;
    }
    gDPPipeSync(chain->gfx++);
}

extern void* D_80132360;

/* Nested-function original (parent func_ovl0_8021B79C not recovered): the
 * uninitialized `r` stands in for the static chain in $v0, saved by the dead
 * `unused` store and copied to `chain` for the parent's display-list head.
 * The struct wrapper keeps the store ahead of the gfx accesses.
 * Draws 16x8 CI4 icon `idx` from D_80132360 centred on (x, y). */
void func_ovl0_8021B52C(s32 x, s32 y, s32 arg2, s32 idx) {
    Ovl0Func8021B79CFrame* r;
    struct { Ovl0Func8021B79CFrame* p; } volatile unused;
    Ovl0Func8021B79CFrame* chain;
    s32 texOff = idx * 64 + 0x28;

    unused.p = r;
    chain = r;

    gDPPipeSync(chain->gfx++);
    gDPSetTextureImage(chain->gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 1,
                       (u8*)(((u32)D_80132360 + texOff) & ~7));
    gDPSetTile(chain->gfx++, G_IM_FMT_CI, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPLoadSync(chain->gfx++);
    gDPLoadBlock(chain->gfx++, G_TX_LOADTILE, 0, 0, 31, 2048);
    gDPPipeSync(chain->gfx++);
    gDPSetTile(chain->gfx++, G_IM_FMT_CI, G_IM_SIZ_4b, 1, 0, G_TX_RENDERTILE, 0, 0, 0, 0,
               0, 0, 0);
    gDPSetTileSize(chain->gfx++, G_TX_RENDERTILE, 0, 0, 15 << G_TEXTURE_IMAGE_FRAC,
                   7 << G_TEXTURE_IMAGE_FRAC);
    gDPPipeSync(chain->gfx++);
    gSPScisTextureRectangle(chain->gfx++, (x - 8) << G_TEXTURE_IMAGE_FRAC,
                            (y - 4) << G_TEXTURE_IMAGE_FRAC,
                            (x + 8) << G_TEXTURE_IMAGE_FRAC,
                            (y + 4) << G_TEXTURE_IMAGE_FRAC, G_TX_RENDERTILE,
                            0, 0, 0x400, 0x400);
    gDPPipeSync(chain->gfx++);
}

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021B030", func_ovl0_8021B79C);

INCLUDE_ASM("asm/nonmatchings/overlay_0/func_ovl0_8021B030", func_ovl0_8021C6B4);
