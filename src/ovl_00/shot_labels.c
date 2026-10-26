#include "common.h"
#include "asset.h"
#include "camera.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "round_hud.h"
#include "save.h"
#include "screen.h"
#include "variables.h"

/*
 * Each player's tag, "1P" or "1C" and so on, is drawn beside their ball with a
 * line joining the two. It sits off one of the ball's corners, and moves to
 * another corner when it would cover a tag drawn before it.
 */

#define GAME_STATE_UNK6 6
#define PLAYER_STATE_UNK2 2
#define LETTERBOX_STATE_NONE 0
#define LETTERBOX_STATE_UNK5 5

/* The players whose balls are tagged */
#define SHOT_LABELS_NONE -1
/* From the overhead camera, every player's ball */
#define SHOT_LABELS_OVERHEAD 0
/* While a player takes their shot, the other balls in play */
#define SHOT_LABELS_OTHER_BALLS 1

/* The corners of the ball that a tag sits off, clockwise from the lower right */
#define NO_CORNER -1
#define CORNER_LOWER_RIGHT 0
#define CORNER_LOWER_LEFT 1
#define CORNER_UPPER_LEFT 2
#define CORNER_UPPER_RIGHT 3
#define CORNER_COUNT 4
/* Only the upper corners are used while a shot is taken, keeping the tags clear of the golfer */
#define UPPER_CORNER_COUNT 2

#define TAG_WIDTH 16
#define TAG_HEIGHT 8
/* In bytes, at four bits a texel */
#define TAG_IMAGE_SIZE (TAG_WIDTH * TAG_HEIGHT / 2)
/* Balls this far off the screen are not tagged */
#define TAG_SCREEN_MARGIN 64
/* The tags of the computer's players follow the four "1P" to "4P" */
#define COMPUTER_TAG_OFFSET 4
#define TAG_ALPHA 255
#define TAG_SHADOW_ALPHA 79
/* The line from a tag spans this far beside it */
#define LEADER_LINE_NEAR_X 6
#define LEADER_LINE_FAR_X 22
#define LEADER_LINE_NEAR_Y 4
#define LEADER_LINE_FAR_Y 12

typedef struct ShotLabel {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 depth;
    /* 0xC */ s32 corner; /* NO_CORNER when the tag is hidden */
} ShotLabel; /* size = 0x10 */

#define SPOT_HIDDEN -1
#define SPOT_SHOWN 0

/* Where a point is on the screen */
typedef struct ScreenSpot {
    /* 0x0 */ s32 x;
    /* 0x4 */ s32 y;
    /* 0x8 */ s32 isHidden; /* SPOT_HIDDEN behind the camera, else SPOT_SHOWN */
} ScreenSpot; /* size = 0xC */

extern s32 D_800C308C; /* The letterbox's state */

extern u8 sPlayerTagColors[MAX_PLAYER_COUNT][3];
extern ShotLabel sShotLabels[MAX_PLAYER_COUNT];
extern s32 sShotLabelPlayers;
/* Where a tag's centre sits from the ball at each corner */
extern s8 sTagOffsetsX[CORNER_COUNT];
extern s8 sTagOffsetsY[CORNER_COUNT];

/*
 * DrawLeaderLine and DrawPlayerTag are nested functions, a GNU C extension.
 * The tag loop keeps its corner pointer and label offsets in other registers.
 */
#ifdef NON_MATCHING
/* Draws the tags of the players' balls, moving each to a corner that is clear of the others */
void DrawShotLabels(Gfx **displayListHead) {
    ScreenPoint point;
    ScreenSpot cup;
    ShotLabel *label;
    Gfx *gfx = *displayListHead;
    s32 players;
    s32 corner;
    s32 tries;
    s32 overlaps;
    s32 i;
    s32 j;

    /* Draws the line from a tag at x and y to its ball, which lies off corner */
    void DrawLeaderLine(s32 x, s32 y, s32 corner) {
        gDPPipeSync(gfx++);
        switch (corner) {
            case CORNER_LOWER_RIGHT:
                gSPScisTextureRectangle(gfx++, (x - LEADER_LINE_FAR_X) << 2, (y - LEADER_LINE_FAR_Y) << 2,
                                        (x - LEADER_LINE_NEAR_X) << 2, (y - LEADER_LINE_NEAR_Y) << 2, G_TX_RENDERTILE,
                                        0, 0, 1 << 10, 1 << 10);
                break;
            case CORNER_LOWER_LEFT:
                gSPScisTextureRectangle(gfx++, (x + LEADER_LINE_NEAR_X) << 2, (y - LEADER_LINE_FAR_Y) << 2,
                                        (x + LEADER_LINE_FAR_X) << 2, (y - LEADER_LINE_NEAR_Y) << 2, G_TX_RENDERTILE,
                                        TAG_WIDTH << 5, 0, 1 << 10, 1 << 10);
                break;
            case CORNER_UPPER_LEFT:
                gSPScisTextureRectangle(gfx++, (x + LEADER_LINE_NEAR_X) << 2, (y + LEADER_LINE_NEAR_Y) << 2,
                                        (x + LEADER_LINE_FAR_X) << 2, (y + LEADER_LINE_FAR_Y) << 2, G_TX_RENDERTILE,
                                        TAG_WIDTH << 5, TAG_HEIGHT << 5, 1 << 10, 1 << 10);
                break;
            case CORNER_UPPER_RIGHT:
                gSPScisTextureRectangle(gfx++, (x - LEADER_LINE_FAR_X) << 2, (y + LEADER_LINE_NEAR_Y) << 2,
                                        (x - LEADER_LINE_NEAR_X) << 2, (y + LEADER_LINE_FAR_Y) << 2, G_TX_RENDERTILE,
                                        0, TAG_HEIGHT << 5, 1 << 10, 1 << 10);
                break;
        }
        gDPPipeSync(gfx++);
    }

    /* Draws tag number tag centred on x and y */
    void DrawPlayerTag(s32 x, s32 y, s32 corner, s32 tag) {
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock_4b(gfx++,
                               ((u32)g_PlayerTagImages + PALETTED_TEXELS_OFFSET + tag * TAG_IMAGE_SIZE) & ~7,
                               G_IM_FMT_CI, TAG_WIDTH, TAG_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, (x - TAG_WIDTH / 2) << 2, (y - TAG_HEIGHT / 2) << 2, (x + TAG_WIDTH / 2) << 2,
                                (y + TAG_HEIGHT / 2) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gDPPipeSync(gfx++);
    }

    players = SHOT_LABELS_NONE;
    if (g_Cameras[0].view.mode == CAMERA_MODE_UNK2) {
        players = SHOT_LABELS_OVERHEAD;
    } else if (g_GameState.unk0 == GAME_STATE_UNK6 && g_CurrentShot.playerIndex >= 0 &&
               g_CurrentShot.playerIndex < MAX_PLAYER_COUNT &&
               g_Players[g_CurrentShot.playerIndex].unkC == PLAYER_STATE_UNK2 &&
               (D_800C308C != LETTERBOX_STATE_NONE & D_800C308C != LETTERBOX_STATE_UNK5)) {
        players = SHOT_LABELS_OTHER_BALLS;
    }
    if (sShotLabelPlayers != players) {
        for (i = 0; i != MAX_PLAYER_COUNT; i++) {
            sShotLabels[i].corner = NO_CORNER;
        }
    }
    sShotLabelPlayers = players;
    if (players != SHOT_LABELS_NONE) {
        /* This projects the cup with its x for all three coordinates */
        if (ProjectPoint(D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                         D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                         D_800BA9D8.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE, &point) < 0) {
            cup.isHidden = SPOT_SHOWN;
            cup.x = point.x;
            cup.y = point.y;
        } else {
            cup.isHidden = SPOT_HIDDEN;
        }
        for (i = 0; i != g_PlayerCount; i++) {
            label = &sShotLabels[i];
            if (players == SHOT_LABELS_OTHER_BALLS &&
                (g_CurrentShot.playerIndex == i || g_Players[i].strokeCount == 0)) {
                label->corner = NO_CORNER;
                continue;
            }
            if (g_Players[i].unk26 != 0) {
                label->corner = NO_CORNER;
                continue;
            }
            if (ProjectPoint(g_Players[i].ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                             g_Players[i].ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                             g_Players[i].ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE, &point) >= 0) {
                label->corner = NO_CORNER;
                continue;
            }
            if (point.x < -TAG_SCREEN_MARGIN || point.x > SCREEN_WIDTH + TAG_SCREEN_MARGIN ||
                point.y < -TAG_SCREEN_MARGIN || point.y > SCREEN_HEIGHT + TAG_SCREEN_MARGIN) {
                label->x = point.x;
                label->y = point.y;
                label->depth = point.depth;
                label->corner = NO_CORNER;
                continue;
            }
            if (label->corner == NO_CORNER) {
                if (players == SHOT_LABELS_OTHER_BALLS) {
                    label->corner = (i & 1) + CORNER_UPPER_LEFT;
                } else {
                    label->corner = i;
                }
            }
            label->x = point.x + sTagOffsetsX[label->corner];
            label->y = point.y + sTagOffsetsY[label->corner];
            label->depth = point.depth;
            tries = 0;
            if (players == SHOT_LABELS_OVERHEAD) {
                overlaps = FALSE;
                while (TRUE) {
                    corner = (label->corner + tries) % CORNER_COUNT;
                    for (j = 0; j != i; j++) {
                        if (sShotLabels[j].corner != NO_CORNER &&
                            sShotLabels[j].x - TAG_WIDTH < point.x + sTagOffsetsX[corner] &&
                            point.x + sTagOffsetsX[corner] < sShotLabels[j].x + TAG_WIDTH &&
                            sShotLabels[j].y - TAG_HEIGHT < point.y + sTagOffsetsY[corner] &&
                            point.y + sTagOffsetsY[corner] < sShotLabels[j].y + TAG_HEIGHT) {
                            overlaps = TRUE;
                        }
                    }
                    tries++;
                    if (!overlaps) {
                        break;
                    }
                    overlaps = FALSE;
                    if (tries == CORNER_COUNT) {
                        goto next;
                    }
                }
            } else {
                while (TRUE) {
                    overlaps = FALSE;
                    corner = (sShotLabels[i].corner + tries) % UPPER_CORNER_COUNT + CORNER_UPPER_LEFT;
                    if (cup.isHidden == SPOT_SHOWN && cup.x - TAG_WIDTH < point.x + sTagOffsetsX[corner] &&
                        point.x + sTagOffsetsX[corner] < cup.x + TAG_WIDTH &&
                        cup.y - TAG_HEIGHT < point.y + sTagOffsetsY[corner] &&
                        point.y + sTagOffsetsY[corner] < cup.y + TAG_HEIGHT) {
                        /* The tag would cover the cup, so it goes to the other upper corner */
                        if (corner == CORNER_UPPER_LEFT) {
                            sShotLabels[i].corner = CORNER_UPPER_RIGHT;
                        } else {
                            sShotLabels[i].corner = CORNER_UPPER_LEFT;
                        }
                        sShotLabels[i].x = point.x + sTagOffsetsX[sShotLabels[i].corner];
                        sShotLabels[i].y = point.y + sTagOffsetsY[sShotLabels[i].corner];
                        goto next;
                    }
                    for (j = 0; j != i; j++) {
                        if (sShotLabels[j].corner != NO_CORNER &&
                            sShotLabels[j].x - TAG_WIDTH < point.x + sTagOffsetsX[corner] &&
                            point.x + sTagOffsetsX[corner] < sShotLabels[j].x + TAG_WIDTH &&
                            sShotLabels[j].y - TAG_HEIGHT < point.y + sTagOffsetsY[corner] &&
                            point.y + sTagOffsetsY[corner] < sShotLabels[j].y + TAG_HEIGHT) {
                            overlaps = TRUE;
                        }
                    }
                    tries++;
                    if (!overlaps) {
                        break;
                    }
                    if (tries == UPPER_CORNER_COUNT) {
                        goto next;
                    }
                }
            }
            sShotLabels[i].x = point.x + sTagOffsetsX[corner];
            sShotLabels[i].y = point.y + sTagOffsetsY[corner];
            sShotLabels[i].corner = corner;
        next:;
        }
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        if (players == SHOT_LABELS_OTHER_BALLS) {
            gDPSetRenderMode(gfx++, AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA |
                                        ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
                             AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA | ALPHA_CVG_SEL |
                                 GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM));
        } else {
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        }
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPSetBlendColor(gfx++, 255, 255, 255, 1);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
        gDPLoadTextureBlock_4b(gfx++, ((u32)g_LeaderLineImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I, TAG_WIDTH,
                               TAG_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD,
                               G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        for (i = 0; i != g_PlayerCount; i++) {
            if (sShotLabels[i].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, sShotLabels[i].depth, 0);
                gDPSetPrimColor(gfx++, 0, 0, sPlayerTagColors[i][0], sPlayerTagColors[i][1], sPlayerTagColors[i][2],
                                TAG_ALPHA);
                DrawLeaderLine(sShotLabels[i].x, sShotLabels[i].y, sShotLabels[i].corner);
            }
        }
        gDPPipeSync(gfx++);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, ((u32)g_PlayerTagImages + IMAGE_HEADER_SIZE) & ~7);
        gDPPipeSync(gfx++);
        for (i = 0; i != g_PlayerCount; i++) {
            if (sShotLabels[i].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, sShotLabels[i].depth, 0);
                DrawPlayerTag(sShotLabels[i].x, sShotLabels[i].y, sShotLabels[i].corner,
                              g_Players[i].unk1E[0] ? i + COMPUTER_TAG_OFFSET : i);
            }
        }
        /* Then translucent over what covers them, so that they show through */
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0,
                          TEXEL0, 0, PRIMITIVE, 0, PRIMITIVE, 0, TEXEL0, 0);
        gDPLoadTextureBlock_4b(gfx++, ((u32)g_LeaderLineImage + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_I, TAG_WIDTH,
                               TAG_HEIGHT, 0, G_TX_MIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, 4, 3, G_TX_NOLOD,
                               G_TX_NOLOD);
        for (i = 0; i != g_PlayerCount; i++) {
            if (sShotLabels[i].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, sShotLabels[i].depth, 0);
                gDPSetPrimColor(gfx++, 0, 0, sPlayerTagColors[i][0], sPlayerTagColors[i][1], sPlayerTagColors[i][2],
                                TAG_SHADOW_ALPHA);
                DrawLeaderLine(sShotLabels[i].x, sShotLabels[i].y, sShotLabels[i].corner);
            }
        }
        gDPPipeSync(gfx++);
        gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0, 0, 0, 0, TEXEL0, PRIMITIVE, 0, TEXEL0, 0);
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, TAG_SHADOW_ALPHA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, ((u32)g_PlayerTagImages + IMAGE_HEADER_SIZE) & ~7);
        gDPPipeSync(gfx++);
        for (i = 0; i != g_PlayerCount; i++) {
            if (sShotLabels[i].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, sShotLabels[i].depth, 0);
                DrawPlayerTag(sShotLabels[i].x, sShotLabels[i].y, sShotLabels[i].corner,
                              g_Players[i].unk1E[0] ? i + COMPUTER_TAG_OFFSET : i);
            }
        }
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", DrawLeaderLine);

INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", DrawPlayerTag);

INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", DrawShotLabels);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_00/shot_labels", func_8021C6B4_ovl_00);
