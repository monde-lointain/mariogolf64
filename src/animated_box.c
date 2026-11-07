/*
 * Boxes of flat color that open and close over a few frames. A box is queued
 * for each frame it shows, and drawing empties the queue. The debug scene menu
 * draws its box with them.
 */
#include "animated_box.h"

#include <ultra64.h>

#include "compiler.h"
#include "game.h"
#include "screen.h"

/* Frames that opening and closing each take */
#define BOX_ANIMATION_FRAME_COUNT 4

static s32 f_QueuedBoxCount = 0;

FILE_STATIC AnimatedBox *f_QueuedBoxes[QUEUED_BOX_CAPACITY];

static void DrawAnimatedBox(Gfx **gfxHead, AnimatedBox *box) {
    Gfx *gfx;
    s32 halfWidth;
    s16 step;
    s16 left;
    s16 right;
    s16 middle;
    s16 boxLeft;
    s16 boxRight;

    gfx = *gfxHead;
    if (box->state == BOX_CLOSED) {
        return;
    }

    box->frameIndex++;
    if (box->frameIndex == BOX_ANIMATION_FRAME_COUNT) {
        box->frameIndex = 0;
        if (box->state == BOX_CLOSING) {
            box->state = BOX_CLOSED;
            return;
        }

        if (box->state == BOX_OPENING) {
            box->state = BOX_OPEN;
        }
    }

    boxRight = box->right;
    boxLeft = box->left;
    halfWidth = (boxRight - boxLeft) / 2;
    step = ((s16)halfWidth / BOX_ANIMATION_FRAME_COUNT) * box->frameIndex;
    if (box->state == BOX_CLOSING) {
        left = boxLeft + step;
        right = boxRight - step;
    }
    else if (box->state == BOX_OPENING) {
        middle = boxLeft + halfWidth;
        left = middle - step;
        right = step + middle;
    }
    else {
        left = boxLeft;
        right = boxRight;
    }

    gDPSetPrimColor(gfx++, 0, 0, box->red, box->green, box->blue, box->alpha);
    /* The boxes are flat color, so any texel step will do */
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(left),
        QuarterPixels(box->top),
        QuarterPixels(right),
        QuarterPixels(box->bottom),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *gfxHead = gfx;
}

/* DrawAnimatedBoxes empties the queue, so a box is queued again for each draw;
 * nothing checks the capacity */
void QueueAnimatedBox(AnimatedBox *box) {
    f_QueuedBoxes[f_QueuedBoxCount] = box;
    f_QueuedBoxCount++;
}

void DrawAnimatedBoxes(void) {
    s32 i;

    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetAlphaCompare(g_DisplayListHead++, G_AC_NONE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(g_DisplayListHead++);
    for (i = 0; i != f_QueuedBoxCount; i++) {
        DrawAnimatedBox(&g_DisplayListHead, f_QueuedBoxes[i]);
    }

    f_QueuedBoxCount = 0;
}
