#include "shot_labels.h"

#include <ultra64.h>

// The original calls DrawCenteredShadowedText here unprototyped; see text.h.
#define CALLS_CENTERED_SHADOWED_TEXT_UNPROTOTYPED

#include "address.h"
#include "asset.h"
#include "camera.h"
#include "club.h"
#include "compiler.h"
#include "course_space.h"
#include "cpu_projection.h"
#include "current_shot.h"
#include "game.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "math_util.h"
#include "overlay_manager.h"
#include "play.h"
#include "player.h"
#include "round_hud.h"
#include "screen.h"
#include "shot_camera.h"
#include "surface.h"
#include "text.h"
#include "turn.h"

enum ShotLabelMode {
    SHOT_LABELS_NONE = -1,
    SHOT_LABELS_OVERHEAD,
    SHOT_LABELS_OTHER_BALLS
};

typedef s32 ShotLabelMode;

#define NO_CORNER (-1)
#define CORNER_LOWER_RIGHT 0
#define CORNER_LOWER_LEFT 1
#define CORNER_UPPER_LEFT 2
#define CORNER_UPPER_RIGHT 3
#define CORNER_COUNT 4
#define UPPER_CORNER_COUNT 2

#define TAG_WIDTH 16
#define TAG_HEIGHT 8
#define IsTagOverlapping(spot, tagX, tagY) \
    ((tagX) > (spot).x - TAG_WIDTH && \
        (tagX) < (spot).x + TAG_WIDTH && \
        (tagY) > (spot).y - TAG_HEIGHT && \
        (tagY) < (spot).y + TAG_HEIGHT)
#define TAG_IMAGE_SIZE (TAG_WIDTH * TAG_HEIGHT / 2)
#define TAG_SCREEN_MARGIN 64
#define COMPUTER_TAG_OFFSET 4
#define TAG_ALPHA 255
#define TAG_SHADOW_ALPHA 79
#define LEADER_LINE_NEAR_X 6
#define LEADER_LINE_FAR_X 22
#define LEADER_LINE_NEAR_Y 4
#define LEADER_LINE_FAR_Y 12

typedef struct ShotLabel {
    s32 x;
    s32 y;
    s32 depth;
    s32 corner;
} ShotLabel;

#define SIGN_WIDTH 40
#define SIGN_HEIGHT 24
#define SIGN_BOARD_HEIGHT 18
#define SIGN_DISTANCE_TEXT_Y 15
#define DISTANCE_TEXT_OFFSET 10
#define SIGN_HEIGHT_TEXT_Y 23
#define OUT_OF_BOUNDS_TEXT_Y 31
#define SIGN_FADE_START_DEPTH (-2000)
#define SIGN_FADE_END_DEPTH (-149)
#define SIGN_ALPHA_MIN 64
#define SIGN_FADE_ALPHA_RANGE (255 - SIGN_ALPHA_MIN)
#define SIGN_FADE_DEPTH_RANGE 1850
#define HEIGHT_YARDS_MAX 99
#define TINT_YARDS_MAX 25
#define TINT_PER_YARD 10
#define TEXT_SHADOW_ALPHA 127
#define OUT_OF_BOUNDS_GREEN 95
#define OUT_OF_BOUNDS_GREEN_SWING 45
#define OUT_OF_BOUNDS_PULSE_STEP (M_PI / 6)

#define PIN_FLAG_IMAGE_WIDTH 32
#define PIN_FLAG_IMAGE_HEIGHT 16
#define PIN_FLAG_IMAGE_SIZE (PIN_FLAG_IMAGE_WIDTH * PIN_FLAG_IMAGE_HEIGHT / 2)
#define PIN_FLAG_FRAME_WIDTH 8
#define PIN_FLAG_LEFT 3
#define PIN_FLAG_RIGHT 5
#define PIN_FLAG_WAVE_MASK 15
#define PIN_FLAG_WAVE_TICKS 4
#define PIN_FLAG_WAVE_HALF 8
#define PIN_FLAG_TOP_Y 40
#define PIN_FLAG_DROP 0xB400
#define PIN_FLAG_GREEN_IMAGE 0
#define PIN_FLAG_IMAGE 1
#define PIN_FLAG_HIDDEN 2
#define PIN_FLAG_FADE_FRAME 114
#define PIN_FLAG_HIDE_FRAME 120
#define PIN_FLAG_FADE_STEP 32

enum SpotState {
    SPOT_HIDDEN = -1,
    SPOT_SHOWN
};

typedef s32 SpotState;

typedef struct ScreenSpot {
    s32 x;
    s32 y;
    SpotState state;
} ScreenSpot;

static u8 f_PlayerTagColors[PLAYER_COUNT_MAX][3] =
    {{0xDC, 0x00, 0x00}, {0x00, 0x00, 0xDC}, {0xF0, 0x7F, 0x00}, {0x00, 0x7F, 0x00}};
static ShotLabel f_ShotLabels[PLAYER_COUNT_MAX] =
    {{0, 0, 0, NO_CORNER}, {0, 0, 0, NO_CORNER}, {0, 0, 0, NO_CORNER}, {0, 0, 0, NO_CORNER}};
static ShotLabelMode f_ShotLabelMode = SHOT_LABELS_NONE;
static s8 f_TagOffsetsX[CORNER_COUNT] = {22, -22, -22, 22};
static s8 f_TagOffsetsY[CORNER_COUNT] = {12, 12, -12, -12};
static Radians f_OutOfBoundsPulseAngle = 0.0f;
static s32 f_PinFlagWaveFrame = 0;

u8 g_UnusedBytesAfterFlagWaveFrame[4];

void DrawShotLabels(Gfx **displayListHead) {
    ScreenPoint point;
    ScreenSpot cup;
    Gfx *gfx = *displayListHead;
    ShotLabelMode labelMode;
    s32 corner;
    s32 cornerOffset;

    s32 playerIndex;
    s32 earlierIndex;

    void DrawLeaderLine(s32 x, s32 y, s32 corner) {
        gDPPipeSync(gfx++);
        switch (corner) {
            case CORNER_LOWER_RIGHT:
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(x - LEADER_LINE_FAR_X),
                    QuarterPixels(y - LEADER_LINE_FAR_Y),
                    QuarterPixels(x - LEADER_LINE_NEAR_X),
                    QuarterPixels(y - LEADER_LINE_NEAR_Y),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                break;
            case CORNER_LOWER_LEFT:
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(x + LEADER_LINE_NEAR_X),
                    QuarterPixels(y - LEADER_LINE_FAR_Y),
                    QuarterPixels(x + LEADER_LINE_FAR_X),
                    QuarterPixels(y - LEADER_LINE_NEAR_Y),
                    G_TX_RENDERTILE,
                    TexelCoordinate(TAG_WIDTH),
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                break;
            case CORNER_UPPER_LEFT:
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(x + LEADER_LINE_NEAR_X),
                    QuarterPixels(y + LEADER_LINE_NEAR_Y),
                    QuarterPixels(x + LEADER_LINE_FAR_X),
                    QuarterPixels(y + LEADER_LINE_FAR_Y),
                    G_TX_RENDERTILE,
                    TexelCoordinate(TAG_WIDTH),
                    TexelCoordinate(TAG_HEIGHT),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                break;
            case CORNER_UPPER_RIGHT:
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(x - LEADER_LINE_FAR_X),
                    QuarterPixels(y + LEADER_LINE_NEAR_Y),
                    QuarterPixels(x - LEADER_LINE_NEAR_X),
                    QuarterPixels(y + LEADER_LINE_FAR_Y),
                    G_TX_RENDERTILE,
                    0,
                    TexelCoordinate(TAG_HEIGHT),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                break;
        }

        gDPPipeSync(gfx++);
    }

    void DrawPlayerTag(s32 x, s32 y, s32 corner UNUSED, s32 tagIndex) {
        gDPPipeSync(gfx++);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageTexels(g_PlayerTagImages) + tagIndex * TAG_IMAGE_SIZE),
            G_IM_FMT_CI,
            TAG_WIDTH,
            TAG_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(x - TAG_WIDTH / 2),
            QuarterPixels(y - TAG_HEIGHT / 2),
            QuarterPixels(x + TAG_WIDTH / 2),
            QuarterPixels(y + TAG_HEIGHT / 2),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    labelMode = SHOT_LABELS_NONE;
    if (g_Cameras[MAIN_CAMERA].mode == CAMERA_MODE_FREE_LOOK) {
        labelMode = SHOT_LABELS_OVERHEAD;
    }
    else if (g_GameState.step == GAME_STATE_TAKING_TURNS &&
        g_CurrentShot.playerIndex >= 0 &&
        g_CurrentShot.playerIndex < PLAYER_COUNT_MAX) {
        if (g_Players[g_CurrentShot.playerIndex].state == PLAYER_STATE_AIMING &&
            IsLetterboxState(g_LetterboxState)) {
            labelMode = SHOT_LABELS_OTHER_BALLS;
        }
    }

    if (f_ShotLabelMode != labelMode) {
        for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
            f_ShotLabels[playerIndex].corner = NO_CORNER;
        }
    }

    f_ShotLabelMode = labelMode;
    if (labelMode != SHOT_LABELS_NONE) {
        if (ProjectPoint(
                g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                &point
            ) < 0) {
            cup.state = SPOT_SHOWN;
            cup.x = point.x;
            cup.y = point.y;
        }
        else {
            cup.state = SPOT_HIDDEN;
        }

        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (labelMode == SHOT_LABELS_OTHER_BALLS &&
                (g_CurrentShot.playerIndex == playerIndex ||
                    g_Players[playerIndex].strokeCount == 0)) {
                f_ShotLabels[playerIndex].corner = NO_CORNER;
                continue;
            }

            if (g_Players[playerIndex].holeStatus != HOLE_STATUS_PLAYING) {
                f_ShotLabels[playerIndex].corner = NO_CORNER;
                continue;
            }

            if (ProjectPoint(
                    g_Players[playerIndex].ballPosition[AXIS_X] * COURSE_TO_WORLD_SCALE,
                    g_Players[playerIndex].ballPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE,
                    g_Players[playerIndex].ballPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE,
                    &point
                ) >= 0) {
                f_ShotLabels[playerIndex].corner = NO_CORNER;
                continue;
            }

            if (point.x < -TAG_SCREEN_MARGIN ||
                point.x > SCREEN_WIDTH + TAG_SCREEN_MARGIN ||
                point.y < -TAG_SCREEN_MARGIN ||
                point.y > SCREEN_HEIGHT + TAG_SCREEN_MARGIN) {
                f_ShotLabels[playerIndex].x = point.x;
                f_ShotLabels[playerIndex].y = point.y;
                f_ShotLabels[playerIndex].depth = point.depth;
                f_ShotLabels[playerIndex].corner = NO_CORNER;
                continue;
            }

            if (f_ShotLabels[playerIndex].corner == NO_CORNER) {
                if (labelMode == SHOT_LABELS_OTHER_BALLS) {
                    f_ShotLabels[playerIndex].corner = (playerIndex & 1) + CORNER_UPPER_LEFT;
                }
                else {
                    f_ShotLabels[playerIndex].corner = playerIndex;
                }
            }

            f_ShotLabels[playerIndex].x = point.x + f_TagOffsetsX[f_ShotLabels[playerIndex].corner];
            f_ShotLabels[playerIndex].y = point.y + f_TagOffsetsY[f_ShotLabels[playerIndex].corner];
            f_ShotLabels[playerIndex].depth = point.depth;
            if (labelMode == SHOT_LABELS_OVERHEAD) {
                for (cornerOffset = 0; cornerOffset != CORNER_COUNT; cornerOffset++) {
                    s32 overlaps = FALSE;

                    corner = (f_ShotLabels[playerIndex].corner + cornerOffset) % CORNER_COUNT;
                    for (earlierIndex = 0; earlierIndex != playerIndex; earlierIndex++) {
                        if (f_ShotLabels[earlierIndex].corner != NO_CORNER &&
                            IsTagOverlapping(
                                f_ShotLabels[earlierIndex],
                                point.x + f_TagOffsetsX[corner],
                                point.y + f_TagOffsetsY[corner]
                            )) {
                            overlaps = TRUE;
                        }
                    }

                    if (!overlaps) {
                        f_ShotLabels[playerIndex].x = point.x + f_TagOffsetsX[corner];
                        f_ShotLabels[playerIndex].y = point.y + f_TagOffsetsY[corner];
                        goto PLACED;
                    }
                }
                continue;
            }
            else {
                for (cornerOffset = 0; cornerOffset != UPPER_CORNER_COUNT; cornerOffset++) {
                    s32 overlaps = FALSE;

                    corner =
                        ((f_ShotLabels[playerIndex].corner + cornerOffset) % UPPER_CORNER_COUNT) +
                        CORNER_UPPER_LEFT;
                    if (cup.state == SPOT_SHOWN &&
                        IsTagOverlapping(
                            cup,
                            point.x + f_TagOffsetsX[corner],
                            point.y + f_TagOffsetsY[corner]
                        )) {
                        if (corner == CORNER_UPPER_LEFT) {
                            f_ShotLabels[playerIndex].corner = CORNER_UPPER_RIGHT;
                        }
                        else {
                            f_ShotLabels[playerIndex].corner = CORNER_UPPER_LEFT;
                        }

                        f_ShotLabels[playerIndex].x =
                            point.x + f_TagOffsetsX[f_ShotLabels[playerIndex].corner];
                        f_ShotLabels[playerIndex].y =
                            point.y + f_TagOffsetsY[f_ShotLabels[playerIndex].corner];
                        goto NEXT;
                    }

                    for (earlierIndex = 0; earlierIndex != playerIndex; earlierIndex++) {
                        if (f_ShotLabels[earlierIndex].corner != NO_CORNER &&
                            IsTagOverlapping(
                                f_ShotLabels[earlierIndex],
                                point.x + f_TagOffsetsX[corner],
                                point.y + f_TagOffsetsY[corner]
                            )) {
                            overlaps = TRUE;
                        }
                    }

                    if (!overlaps) {
                        f_ShotLabels[playerIndex].x = point.x + f_TagOffsetsX[corner];
                        f_ShotLabels[playerIndex].y = point.y + f_TagOffsetsY[corner];
                        goto PLACED;
                    }
                }
                continue;
            }

PLACED:
            f_ShotLabels[playerIndex].corner = corner;

NEXT:;
        }

        gDPPipeSync(gfx++);
        if (labelMode == SHOT_LABELS_OTHER_BALLS) {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(
                gfx++,
                AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA |
                    ALPHA_CVG_SEL | GBL_c1(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
                AA_EN | Z_CMP | Z_UPD | IM_RD | CVG_DST_CLAMP | ZMODE_OPA | CVG_X_ALPHA |
                    ALPHA_CVG_SEL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM)
            );
        }
        else {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        }

        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPSetBlendColor(gfx++, 255, 255, 255, 1);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIDECALA_PRIM, G_CC_MODULATEIDECALA_PRIM);
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageBody(g_LeaderLineImage)),
            G_IM_FMT_I,
            TAG_WIDTH,
            TAG_HEIGHT,
            0,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_MIRROR | G_TX_WRAP,
            TextureMask(TAG_WIDTH),
            TextureMask(TAG_HEIGHT),
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gDPSetDepthSource(gfx++, G_ZS_PRIM);
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (f_ShotLabels[playerIndex].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, f_ShotLabels[playerIndex].depth, 0);
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    f_PlayerTagColors[playerIndex][0],
                    f_PlayerTagColors[playerIndex][1],
                    f_PlayerTagColors[playerIndex][2],
                    TAG_ALPHA
                );
                DrawLeaderLine(
                    f_ShotLabels[playerIndex].x,
                    f_ShotLabels[playerIndex].y,
                    f_ShotLabels[playerIndex].corner
                );
            }
        }

        gDPPipeSync(gfx++);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(g_PlayerTagImages)));
        gDPPipeSync(gfx++);
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (f_ShotLabels[playerIndex].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, f_ShotLabels[playerIndex].depth, 0);
                DrawPlayerTag(
                    f_ShotLabels[playerIndex].x,
                    f_ShotLabels[playerIndex].y,
                    f_ShotLabels[playerIndex].corner,
                    g_Players[playerIndex].isComputer ? playerIndex + COMPUTER_TAG_OFFSET :
                                                        playerIndex
                );
            }
        }

        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        gDPSetCombineLERP(
            gfx++,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            TEXEL0,
            0,
            PRIMITIVE,
            0,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(ImageBody(g_LeaderLineImage)),
            G_IM_FMT_I,
            TAG_WIDTH,
            TAG_HEIGHT,
            0,
            G_TX_MIRROR | G_TX_WRAP,
            G_TX_MIRROR | G_TX_WRAP,
            TextureMask(TAG_WIDTH),
            TextureMask(TAG_HEIGHT),
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (f_ShotLabels[playerIndex].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, f_ShotLabels[playerIndex].depth, 0);
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    f_PlayerTagColors[playerIndex][0],
                    f_PlayerTagColors[playerIndex][1],
                    f_PlayerTagColors[playerIndex][2],
                    TAG_SHADOW_ALPHA
                );
                DrawLeaderLine(
                    f_ShotLabels[playerIndex].x,
                    f_ShotLabels[playerIndex].y,
                    f_ShotLabels[playerIndex].corner
                );
            }
        }

        gDPPipeSync(gfx++);
        gDPSetCombineLERP(
            gfx++,
            0,
            0,
            0,
            TEXEL0,
            PRIMITIVE,
            0,
            TEXEL0,
            0,
            0,
            0,
            0,
            TEXEL0,
            PRIMITIVE,
            0,
            TEXEL0,
            0
        );
        gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, TAG_SHADOW_ALPHA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(g_PlayerTagImages)));
        gDPPipeSync(gfx++);
        for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
            if (f_ShotLabels[playerIndex].corner != NO_CORNER) {
                gDPSetPrimDepth(gfx++, f_ShotLabels[playerIndex].depth, 0);
                DrawPlayerTag(
                    f_ShotLabels[playerIndex].x,
                    f_ShotLabels[playerIndex].y,
                    f_ShotLabels[playerIndex].corner,
                    g_Players[playerIndex].isComputer ? playerIndex + COMPUTER_TAG_OFFSET :
                                                        playerIndex
                );
            }
        }

        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawAimGuides(CameraMatrices *matrices) {
    ScreenPoint point;
    Vec3 position;
    s32 alpha;
    s32 depth;
    s32 heightYards;
    s32 tintYards;
    s32 distance;
    s32 flagImage;
    SurfaceKind surfaceKind;
    Club *club;
    s32 outOfBoundsGreen;

    club = GetSelectedClub();
    outOfBoundsGreen = sinf(f_OutOfBoundsPulseAngle) * OUT_OF_BOUNDS_GREEN_SWING;
    surfaceKind =
        GetSurfaceTraitsAt(g_CurrentShot.targetPoint[AXIS_X], g_CurrentShot.targetPoint[AXIS_Z])
            ->kind;
    if (g_IsCupViewShown) {
        return;
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_NONE);
    DrawShotLabels(&g_DisplayListHead);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPPipeSync(g_DisplayListHead++);
    if (!(club->surfaceMask & CLUB_SURFACE_GREEN) && g_RoundHud.isDistanceSignShown != 0) {
        position[AXIS_X] = g_CurrentShot.targetPoint[AXIS_X] * COURSE_TO_WORLD_SCALE;
        position[AXIS_Y] = g_CurrentShot.targetPoint[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        position[AXIS_Z] = g_CurrentShot.targetPoint[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        depth = ProjectPoint(position[AXIS_X], position[AXIS_Y], position[AXIS_Z], &point);
        if (depth < 0) {
            heightYards = (g_CurrentShot.ballPosition[AXIS_Y] - g_CurrentShot.targetPoint[AXIS_Y]) *
                YARDS_PER_UNIT;
            if (heightYards > HEIGHT_YARDS_MAX) {
                heightYards = HEIGHT_YARDS_MAX;
            }

            if (heightYards < -HEIGHT_YARDS_MAX) {
                heightYards = -HEIGHT_YARDS_MAX;
            }

            tintYards = heightYards;
            if (tintYards > TINT_YARDS_MAX) {
                tintYards = TINT_YARDS_MAX;
            }

            if (tintYards < -TINT_YARDS_MAX) {
                tintYards = -TINT_YARDS_MAX;
            }

            if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
                alpha = OPAQUE_ALPHA;
            }
            else {
                alpha = SIGN_ALPHA_MIN;
                if (depth >= SIGN_FADE_START_DEPTH) {
                    if (depth >= SIGN_FADE_END_DEPTH) {
                        alpha = OPAQUE_ALPHA;
                    }
                    else {
                        alpha = ((depth - SIGN_FADE_START_DEPTH) * SIGN_FADE_ALPHA_RANGE /
                                    SIGN_FADE_DEPTH_RANGE) +
                            SIGN_ALPHA_MIN;
                    }
                }
            }

            distance = GetSelectedShotDistance();
            gDPPipeSync(g_DisplayListHead++);
            gDPSetPrimDepth(g_DisplayListHead++, point.depth, 0);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetDepthSource(g_DisplayListHead++, G_ZS_PRIM);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, alpha);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
            gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
            gDPLoadTLUT_pal16(
                g_DisplayListHead++,
                0,
                TextureAddress(ImageBody(g_DistanceSignImage))
            );
            gDPLoadTextureTile_4b(
                g_DisplayListHead++,
                TextureAddress(ImageTexels(g_DistanceSignImage)),
                G_IM_FMT_CI,
                SIGN_WIDTH,
                SIGN_HEIGHT,
                0,
                0,
                SIGN_WIDTH - 1,
                SIGN_HEIGHT - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, alpha);
            gDPPipeSync(g_DisplayListHead++);
            gSPScisTextureRectangle(
                g_DisplayListHead++,
                QuarterPixels(point.x - (SIGN_WIDTH / 2)),
                QuarterPixels(point.y - SIGN_HEIGHT),
                QuarterPixels(point.x + (SIGN_WIDTH / 2)),
                QuarterPixels(point.y - SIGN_HEIGHT + SIGN_BOARD_HEIGHT),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
            gDPPipeSync(g_DisplayListHead++);
            gSPScisTextureRectangle(
                g_DisplayListHead++,
                QuarterPixels(point.x - (SIGN_WIDTH / 2)),
                QuarterPixels(point.y - SIGN_HEIGHT + SIGN_BOARD_HEIGHT),
                QuarterPixels(point.x + (SIGN_WIDTH / 2)),
                QuarterPixels(point.y),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(SIGN_BOARD_HEIGHT),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(g_DisplayListHead++);
            gDPPipeSync(g_DisplayListHead++);
            gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            gDPLoadTextureBlock_4b(
                g_DisplayListHead++,
                TextureAddress(ImageBody(g_ShadowedFontImage)),
                G_IM_FMT_IA,
                SHADOWED_FONT_IMAGE_WIDTH,
                SHADOWED_FONT_IMAGE_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            if (club->surfaceMask & CLUB_SURFACE_GREEN) {
                sprintf(g_PrintBuffer + DISTANCE_TEXT_OFFSET, "%2dm", (int)distance);
            }
            else {
                sprintf(g_PrintBuffer + DISTANCE_TEXT_OFFSET, "%2dy", (int)distance);
            }

            if (g_LetterboxState != LETTERBOX_STATE_STARTING_LOOK_BACK &&
                g_LetterboxState != LETTERBOX_STATE_LOOKING_BACK) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
                gDPPipeSync(g_DisplayListHead++);
                DrawCenteredShadowedText(
                    &g_DisplayListHead,
                    point.x,
                    point.y - SIGN_DISTANCE_TEXT_Y,
                    g_PrintBuffer + DISTANCE_TEXT_OFFSET
                );
            }

            gDPPipeSync(g_DisplayListHead++);
            if (tintYards > 0) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(
                    g_DisplayListHead++,
                    0,
                    0,
                    255,
                    255 - (tintYards * TINT_PER_YARD),
                    255 - (tintYards * TINT_PER_YARD),
                    OPAQUE_ALPHA
                );
                gDPPipeSync(g_DisplayListHead++);
                sprintf(g_PrintBuffer, "%dyUP", (int)heightYards);
            }
            else if (tintYards < 0) {
                sprintf(g_PrintBuffer, "%dyDW", (int)-heightYards);
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(
                    g_DisplayListHead++,
                    0,
                    0,
                    255 + (tintYards * TINT_PER_YARD),
                    255 + (tintYards * TINT_PER_YARD),
                    255,
                    OPAQUE_ALPHA
                );
                gDPPipeSync(g_DisplayListHead++);
            }
            else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
                gDPPipeSync(g_DisplayListHead++);
                sprintf(g_PrintBuffer, "FLAT");
            }

            if (g_LetterboxState != LETTERBOX_STATE_STARTING_LOOK_BACK &&
                g_LetterboxState != LETTERBOX_STATE_LOOKING_BACK) {
                DrawCenteredShadowedText(
                    &g_DisplayListHead,
                    point.x,
                    point.y - SIGN_HEIGHT_TEXT_Y,
                    g_PrintBuffer
                );
            }

            if (IsOutOfBoundsKind(surfaceKind)) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(
                    g_DisplayListHead++,
                    0,
                    0,
                    255,
                    outOfBoundsGreen + OUT_OF_BOUNDS_GREEN,
                    0,
                    OPAQUE_ALPHA
                );
                gDPPipeSync(g_DisplayListHead++);
                f_OutOfBoundsPulseAngle += OUT_OF_BOUNDS_PULSE_STEP;
                DrawCenteredShadowedText(
                    &g_DisplayListHead,
                    point.x,
                    point.y - OUT_OF_BOUNDS_TEXT_Y,
                    "OB!"
                );
            }

            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
            if (tintYards > 0) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(
                    g_DisplayListHead++,
                    0,
                    0,
                    255,
                    255 - (tintYards * TINT_PER_YARD),
                    255 - (tintYards * TINT_PER_YARD),
                    TEXT_SHADOW_ALPHA
                );
                gDPPipeSync(g_DisplayListHead++);
            }
            else if (tintYards < 0) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(
                    g_DisplayListHead++,
                    0,
                    0,
                    255 + (tintYards * TINT_PER_YARD),
                    255 + (tintYards * TINT_PER_YARD),
                    255,
                    TEXT_SHADOW_ALPHA
                );
                gDPPipeSync(g_DisplayListHead++);
            }
            else {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, TEXT_SHADOW_ALPHA);
                gDPPipeSync(g_DisplayListHead++);
            }

            DrawCenteredShadowedText(
                &g_DisplayListHead,
                point.x,
                point.y - SIGN_HEIGHT_TEXT_Y,
                g_PrintBuffer
            );
            if (IsOutOfBoundsKind(surfaceKind)) {
                gDPPipeSync(g_DisplayListHead++);
                gDPSetPrimColor(
                    g_DisplayListHead++,
                    0,
                    0,
                    255,
                    outOfBoundsGreen + OUT_OF_BOUNDS_GREEN,
                    0,
                    TEXT_SHADOW_ALPHA
                );
                gDPPipeSync(g_DisplayListHead++);
                f_OutOfBoundsPulseAngle += OUT_OF_BOUNDS_PULSE_STEP;
                DrawCenteredShadowedText(
                    &g_DisplayListHead,
                    point.x,
                    point.y - OUT_OF_BOUNDS_TEXT_Y,
                    "OB!"
                );
            }

            gDPPipeSync(g_DisplayListHead++);
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 255, 255, 255, TEXT_SHADOW_ALPHA);
            gDPPipeSync(g_DisplayListHead++);
            DrawCenteredShadowedText(
                &g_DisplayListHead,
                point.x,
                point.y - SIGN_DISTANCE_TEXT_Y,
                g_PrintBuffer + DISTANCE_TEXT_OFFSET
            );
        }
    }

    if (g_ShotSetupFrameCount < PIN_FLAG_HIDE_FRAME) {
        g_ShotSetupFrameCount++;
    }

    if (g_RoundHud.isPinFlagShown == 1) {
        position[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
        position[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
        if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
            position[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
        }
        else {
            position[AXIS_Y] =
                (g_HoleSetup.cupPosition[AXIS_Y] - PIN_FLAG_DROP) * COURSE_TO_WORLD_SCALE;
        }

        if (ProjectPoint(position[AXIS_X], position[AXIS_Y], position[AXIS_Z], &point) < 0) {
            flagImage = PIN_FLAG_IMAGE;
            if (point.y < SCREEN_HEIGHT) {
                if (g_PauseScreen == PAUSE_SCREEN_FREE_LOOK) {
                    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    gDPPipeSync(g_DisplayListHead++);
                    gDPSetRenderMode(
                        g_DisplayListHead++,
                        OPAQUE_ALPHA_COVERAGE_RENDER_MODE,
                        OPAQUE_ALPHA_COVERAGE_RENDER_MODE2
                    );
                }
                else if (g_ShotFlags & SHOT_FLAG_ON_GREEN) {
                    flagImage = PIN_FLAG_GREEN_IMAGE;
                    if (g_ShotSetupFrameCount == PIN_FLAG_HIDE_FRAME) {
                        flagImage = PIN_FLAG_HIDDEN;
                    }
                    else if (g_ShotSetupFrameCount >= PIN_FLAG_FADE_FRAME) {
                        gDPSetPrimColor(
                            g_DisplayListHead++,
                            0,
                            0,
                            255,
                            255,
                            255,
                            255 -
                                ((g_ShotSetupFrameCount - (PIN_FLAG_FADE_FRAME - 1)) *
                                    PIN_FLAG_FADE_STEP)
                        );
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetRenderMode(g_DisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
                        gDPSetCombineMode(
                            g_DisplayListHead++,
                            G_CC_MODULATEIA_PRIM,
                            G_CC_MODULATEIA_PRIM
                        );
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetAlphaCompare(g_DisplayListHead++, G_AC_THRESHOLD);
                        gDPSetBlendColor(g_DisplayListHead++, 255, 255, 255, 1);
                    }
                    else {
                        gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetRenderMode(
                            g_DisplayListHead++,
                            OPAQUE_ALPHA_COVERAGE_RENDER_MODE,
                            OPAQUE_ALPHA_COVERAGE_RENDER_MODE2
                        );
                    }

                    if (point.y < PIN_FLAG_TOP_Y) {
                        point.y = PIN_FLAG_TOP_Y;
                    }
                }
                else {
                    gDPSetCombineMode(g_DisplayListHead++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                    gDPPipeSync(g_DisplayListHead++);
                    gDPSetRenderMode(
                        g_DisplayListHead++,
                        OPAQUE_ALPHA_COVERAGE_RENDER_MODE,
                        OPAQUE_ALPHA_COVERAGE_RENDER_MODE2
                    );
                    point.y = PIN_FLAG_TOP_Y;
                }

                if (flagImage != PIN_FLAG_HIDDEN) {
                    gDPPipeSync(g_DisplayListHead++);
                    gDPLoadTLUT_pal16(
                        g_DisplayListHead++,
                        0,
                        TextureAddress(ImageBody(g_PinFlagImages))
                    );
                    gDPPipeSync(g_DisplayListHead++);
                    gDPSetTextureLUT(g_DisplayListHead++, G_TT_RGBA16);
                    gDPLoadTextureBlock_4b(
                        g_DisplayListHead++,
                        TextureAddress(
                            ImageTexels(g_PinFlagImages) + (flagImage * PIN_FLAG_IMAGE_SIZE)
                        ),
                        G_IM_FMT_CI,
                        PIN_FLAG_IMAGE_WIDTH,
                        PIN_FLAG_IMAGE_HEIGHT,
                        0,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    gDPPipeSync(g_DisplayListHead++);

                    f_PinFlagWaveFrame = (f_PinFlagWaveFrame + 1) & PIN_FLAG_WAVE_MASK;
                    if (f_PinFlagWaveFrame > PIN_FLAG_WAVE_HALF) {
                        point.y -= (f_PinFlagWaveFrame - PIN_FLAG_WAVE_HALF) / PIN_FLAG_WAVE_TICKS;
                    }
                    else {
                        point.y += f_PinFlagWaveFrame / PIN_FLAG_WAVE_TICKS;
                    }

                    gSPScisTextureRectangle(
                        g_DisplayListHead++,
                        QuarterPixels(point.x - PIN_FLAG_LEFT),
                        QuarterPixels(point.y - PIN_FLAG_IMAGE_HEIGHT),
                        QuarterPixels(point.x + PIN_FLAG_RIGHT),
                        QuarterPixels(point.y),
                        G_TX_RENDERTILE,
                        TexelCoordinate(
                            f_PinFlagWaveFrame / PIN_FLAG_WAVE_TICKS * PIN_FLAG_FRAME_WIDTH
                        ),
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                    gDPPipeSync(g_DisplayListHead++);
                }
            }
        }
    }

    gDPPipeSync(g_DisplayListHead++);
    gDPSetDepthSource(g_DisplayListHead++, G_ZS_PIXEL);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTextureLUT(g_DisplayListHead++, G_TT_NONE);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetTexturePersp(g_DisplayListHead++, G_TP_PERSP);
    gDPPipeSync(g_DisplayListHead++);
    if (g_IsModuleAlive[MODULE_PLAY] || ReportModuleNotAlive(MODULE_PLAY)) {
        DrawShotPreview(matrices);
    }
}
