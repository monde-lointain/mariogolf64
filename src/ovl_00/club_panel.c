#include "club_panel.h"

#include <ultra64.h>

#include "club.h"
#include "controllers/controller.h"
#include "current_shot.h"
#include "debug_text.h"
#include "game_flags.h"
#include "game_state.h"
#include "math/math_util.h"
#include "menu.h"
#include "play.h"
#include "player.h"
#include "round_hud.h"
#include "scene.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "text.h"

#define GAME_FLAG_CLUB_PANEL_DEBUG 0x6E
#define DEBUG_VALUE_COLUMN 20
#define DEBUG_VALUE_ROW 5

enum ClubPanelState {
    CLUB_PANEL_FREED = -1,
    CLUB_PANEL_LOADED = 1
};

typedef s32 ClubPanelState;

#define SHOT_MODE_CHANGE_FRAME_COUNT 4
#define SHOT_MODE_CHANGE_LAST_FRAME (SHOT_MODE_CHANGE_FRAME_COUNT - 1)

#define CLUB_PANEL_BUFFER_SLOT 12

#define ASSET_CLUB_PANEL_IMAGE 0x4FF
#define SHOT_MODE_IMAGE_SIZE 0x808

#define ASSET_PANEL_POWER 0x56C
#define ASSET_PANEL_POWER_TRAINING 0x56D
#define ASSET_PANEL_NORMAL 0x56E
#define ASSET_PANEL_APPROACH 0x56F
#define ASSET_PANEL_PUTT_LONG 0x570
#define ASSET_PANEL_PUTT_MIDDLE 0x571
#define ASSET_PANEL_PUTT_SHORT 0x572

#define PANEL_NOT_SHOWN 0xFF
#define INITIAL_CLUB_PANEL_DEBUG_VALUE (-2)

#define SPAN_BAR_X 38.0f
#define SPAN_BAR_Y (-103.0f)
#define SPAN_BAR_Z (-100.0f)
#define SPAN_BAR_SCALE_PER_SPAN 0.032857146f
#define SPAN_BAR_THICKNESS 0.1f
#define SPAN_BAR_VERTEX_COUNT 8

#define ASSET_SHOT_MODE_POWER 0x52F
#define ASSET_SHOT_MODE_NORMAL 0x52D
#define ASSET_SHOT_MODE_APPROACH 0x52E

static UShotMode8 f_ShownShotMode = PANEL_NOT_SHOWN;
static UShotMode8 f_PanelShotMode = PANEL_NOT_SHOWN;
static UBoolean8 f_PanelIsPutter = PANEL_NOT_SHOWN;
static u8 f_ShotModeChangeFrame = 0;
static s32 f_ChosenPowerFill = 0;
static s32 f_PanelBufferIndex = 0;
static ClubPanelState f_ClubPanelState = CLUB_PANEL_FREED;
static Vtx f_SpanBarVertices[SPAN_BAR_VERTEX_COUNT] = {
    {{{-70, 10, -2}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-70, -10, -2}, 0, {0, 0x100}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-10, 10, -2}, 0, {0x400, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-10, -10, -2}, 0, {0x400, 0x100}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{10, 10, -2}, 0, {0x400, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{10, -10, -2}, 0, {0x400, 0x100}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{70, 10, -2}, 0, {0x800, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{70, -10, -2}, 0, {0x800, 0x100}, {0xFF, 0x00, 0x00, 0xFF}}},
};
static Gfx f_SpanBarDisplayList[] = {
    gsSP2Triangles(0, 1, 3, 0, 0, 3, 2, 0),
    gsSP2Triangles(2, 3, 5, 0, 2, 5, 4, 0),
    gsSP2Triangles(4, 5, 7, 0, 4, 7, 6, 0),
    gsSPEndDisplayList(),
};
static s32 f_ClubPanelDebugValue = INITIAL_CLUB_PANEL_DEBUG_VALUE;
static u16 f_ApproachToNormalFrames[SHOT_MODE_CHANGE_FRAME_COUNT] =
    {0x578, 0x577, 0x576, ASSET_PANEL_NORMAL};
static u16 f_PowerToNormalFrames[SHOT_MODE_CHANGE_FRAME_COUNT] =
    {0x573, 0x574, 0x575, ASSET_PANEL_NORMAL};
static u16 f_NormalToApproachFrames[SHOT_MODE_CHANGE_FRAME_COUNT] =
    {0x576, 0x577, 0x578, ASSET_PANEL_APPROACH};
static u16 f_PowerToApproachFrames[SHOT_MODE_CHANGE_FRAME_COUNT] =
    {0x57B, 0x57A, 0x579, ASSET_PANEL_APPROACH};
static u16 f_ToPowerFrames[SHOT_MODE_CHANGE_FRAME_COUNT] = {0x575, 0x574, 0x573, ASSET_PANEL_POWER};
static u16 f_ApproachToPowerFrames[SHOT_MODE_CHANGE_FRAME_COUNT] UNUSED =
    {0x579, 0x57A, 0x57B, ASSET_PANEL_POWER};
static Radians f_MarkerPulseAngle = 0.0f;

static Mtx f_SpanBarTranslation;
static Mtx f_SpanBarScale;

ImageAsset *f_ClubPanelImage;
ImageAsset *f_ShotModeImage;

void UpdateClubPanelDebugValue(void) {
    if (IsGameFlagSet(GAME_FLAG_CLUB_PANEL_DEBUG)) {
        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & U_JPAD) {
            f_ClubPanelDebugValue--;
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & D_JPAD) {
            f_ClubPanelDebugValue++;
        }

        sprintf(g_PrintBuffer, "%3d", (int)f_ClubPanelDebugValue);
        PrintDebugText(g_PrintBuffer, DEBUG_VALUE_COLUMN, DEBUG_VALUE_ROW);
    }
}

void LoadClubPanelImages(void) {
    EncodedAsset asset;

    f_ClubPanelImage = AllocMemory(OpenEncodedAsset(ASSET_CLUB_PANEL_IMAGE, &asset));
    ReadEncodedAsset(f_ClubPanelImage, &asset);
    g_ImageSlots[CLUB_PANEL_BUFFER_SLOT] = AllocMemory(OpenEncodedAsset(ASSET_PANEL_POWER, &asset));
    ReadEncodedAsset(g_ImageSlots[CLUB_PANEL_BUFFER_SLOT], &asset);
    g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + 1] =
        AllocMemory(OpenEncodedAsset(ASSET_PANEL_POWER, &asset));
    ReadEncodedAsset(g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + 1], &asset);
    f_ShotModeImage = AllocMemory(SHOT_MODE_IMAGE_SIZE);
    f_ShownShotMode = PANEL_NOT_SHOWN;
    f_PanelShotMode = PANEL_NOT_SHOWN;
    f_PanelIsPutter = PANEL_NOT_SHOWN;
    f_ClubPanelState = CLUB_PANEL_LOADED;
}

void FreeClubPanelImages(void) {
    f_ClubPanelState = CLUB_PANEL_FREED;
    FreeMemory(&f_ClubPanelImage);
    FreeMemory(&f_ShotModeImage);
    FreeMemory(&g_ImageSlots[CLUB_PANEL_BUFFER_SLOT]);
    FreeMemory(&g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + 1]);
}

void ResetShotModeChange(void) {
    f_ChosenPowerFill = 0;
    f_ShotModeChangeFrame = 0;
}

void UpdateClubPanelImages(void) {
    EncodedAsset panelAsset;
    EncodedAsset nameAsset;
    EncodedAsset frameAsset;
    Boolean isPutter;
    AssetId assetId;

    isPutter = GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN;
    if (f_ClubPanelState == CLUB_PANEL_FREED) {
        return;
    }

    if (f_PanelIsPutter != isPutter) {
        f_PanelIsPutter = isPutter;
        f_PanelBufferIndex ^= 1;
        if (isPutter) {
            switch ((u32)g_CurrentShot.shotMode) {
                case PUTT_MODE_MIDDLE:
                    OpenEncodedAsset(ASSET_PANEL_PUTT_MIDDLE, &panelAsset);
                    break;
                case PUTT_MODE_LONG:
                    OpenEncodedAsset(ASSET_PANEL_PUTT_LONG, &panelAsset);
                    break;
                case PUTT_MODE_SHORT:
                    OpenEncodedAsset(ASSET_PANEL_PUTT_SHORT, &panelAsset);
                    break;
            }
        }
        else {
            switch ((u32)g_CurrentShot.shotMode) {
                case SHOT_MODE_NORMAL:
                    OpenEncodedAsset(ASSET_PANEL_NORMAL, &panelAsset);
                    break;
                case SHOT_MODE_POWER:
                    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                        OpenEncodedAsset(ASSET_PANEL_POWER_TRAINING, &panelAsset);
                    }
                    else {
                        OpenEncodedAsset(ASSET_PANEL_POWER, &panelAsset);
                    }

                    break;
                case SHOT_MODE_APPROACH:
                    OpenEncodedAsset(ASSET_PANEL_APPROACH, &panelAsset);
                    break;
            }
        }

        ReadEncodedAsset(g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex], &panelAsset);
        g_ClubPanelImage = g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex];
    }

    if (f_ShownShotMode != g_CurrentShot.shotMode) {
        f_ShownShotMode = g_CurrentShot.shotMode;
        if (g_CurrentShot.shotMode != SHOT_MODE_POWER) {
            if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                assetId = ASSET_SHOT_MODE_NORMAL;
            }
            else {
                assetId = ASSET_SHOT_MODE_APPROACH;
            }
        }
        else {
            assetId = ASSET_SHOT_MODE_POWER;
        }

        OpenEncodedAsset(assetId, &nameAsset);
        ReadEncodedAsset(f_ShotModeImage, &nameAsset);
    }

    if (f_PanelShotMode != g_CurrentShot.shotMode) {
        f_PanelBufferIndex ^= 1;
        if (f_ShotModeChangeFrame == SHOT_MODE_CHANGE_LAST_FRAME) {
            if (isPutter) {
                switch ((u32)g_CurrentShot.shotMode) {
                    case PUTT_MODE_MIDDLE:
                        OpenEncodedAsset(ASSET_PANEL_PUTT_MIDDLE, &frameAsset);
                        break;
                    case PUTT_MODE_LONG:
                        OpenEncodedAsset(ASSET_PANEL_PUTT_LONG, &frameAsset);
                        break;
                    case PUTT_MODE_SHORT:
                        OpenEncodedAsset(ASSET_PANEL_PUTT_SHORT, &frameAsset);
                        break;
                }
            }
            else {
                switch ((u32)g_CurrentShot.shotMode) {
                    case SHOT_MODE_NORMAL:
                        OpenEncodedAsset(ASSET_PANEL_NORMAL, &frameAsset);
                        break;
                    case SHOT_MODE_POWER:
                        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            OpenEncodedAsset(ASSET_PANEL_POWER_TRAINING, &frameAsset);
                        }
                        else {
                            OpenEncodedAsset(ASSET_PANEL_POWER, &frameAsset);
                        }

                        break;
                    case SHOT_MODE_APPROACH:
                        OpenEncodedAsset(ASSET_PANEL_APPROACH, &frameAsset);
                        break;
                }
            }
        }
        else {
            switch (g_CurrentShot.shotMode) {
                case SHOT_MODE_NORMAL:
                    if (f_PanelShotMode == SHOT_MODE_POWER) {
                        assetId = f_PowerToNormalFrames[f_ShotModeChangeFrame];
                    }
                    else {
                        assetId = f_ApproachToNormalFrames[f_ShotModeChangeFrame];
                    }

                    break;
                case SHOT_MODE_POWER:
                    assetId = f_ToPowerFrames[f_ShotModeChangeFrame];
                    break;
                default:
                    if (f_PanelShotMode == SHOT_MODE_POWER) {
                        assetId = f_PowerToApproachFrames[f_ShotModeChangeFrame];
                    }
                    else {
                        assetId = f_NormalToApproachFrames[f_ShotModeChangeFrame];
                    }

                    break;
            }

            OpenEncodedAsset(assetId, &frameAsset);
        }

        ReadEncodedAsset(g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex], &frameAsset);
        f_ShotModeChangeFrame++;
        g_ClubPanelImage = g_ImageSlots[CLUB_PANEL_BUFFER_SLOT + f_PanelBufferIndex];
        if (f_ShotModeChangeFrame == SHOT_MODE_CHANGE_FRAME_COUNT) {
            f_ShotModeChangeFrame = 0;
            f_PanelShotMode = g_CurrentShot.shotMode;
        }
    }
}

Boolean IsShotModeChangeDone(void) {
    return f_ShotModeChangeFrame == 0;
}

#define GAUGE_FULL_X 56
#define GAUGE_LENGTH 140
#define GAUGE_EMPTY_X (GAUGE_FULL_X + GAUGE_LENGTH)
#define GAUGE_VALUE_MAX 30
#define GAUGE_TOP 210
#define GAUGE_BOTTOM 218
#define GAUGE_TICK_SPACING 7
#define GAUGE_TICK_WIDTH 4
#define GAUGE_GROUP_COUNT 5
#define GAUGE_TICKS_PER_GROUP 5
#define GAUGE_EXTRA_TICK_COUNT 2
#define GAUGE_END_TICK_COUNT 2
#define GAUGE_MEDIUM_TICK_COUNT 3
#define GAUGE_MEDIUM_TICK_TOP 212
#define GAUGE_MEDIUM_TICK_BOTTOM 216
#define GAUGE_MARKER_TOP 208
#define GAUGE_MARKER_BOTTOM 220
#define GAUGE_MARKER_WIDTH 4
#define GAUGE_FILL_IMAGE_WIDTH 64
#define GAUGE_FILL_IMAGE_HEIGHT 16
#define GAUGE_FILL_SPAN_PER_VALUE 5
#define GAUGE_FILL_SPAN (GAUGE_VALUE_MAX * GAUGE_FILL_SPAN_PER_VALUE)
#define GAUGE_TICK_IMAGE_WIDTH 16
#define GAUGE_TICK_IMAGE_HEIGHT 12

#define MARKER_PULSE_STEP (M_PI / 6)
#define MARKER_PULSE_LEVEL 100.0

void DrawClubPanel(Gfx **displayListHead) {
    s32 lieSpan[2];
    Gfx *gfx;
    Boolean isPutter;
    s32 fill;
    PlayerState playerState;
    s32 tick;
    s32 group;
    s32 emptyLeft;

    s32 markerOffset;
    s32 pulse;
    s32 alpha;
    s32 fillMagnitude;

    gfx = *displayListHead;
    isPutter = GetSelectedClub()->surfaceMask & CLUB_SURFACE_GREEN;
    playerState = g_Players[g_CurrentShot.playerIndex].state;
    GetLieSpan(
        GetClub(g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]),
        g_CurrentShot.surface,
        lieSpan,
        g_CurrentShot.shotMode
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, 191);
    gDPPipeSync(gfx++);
    if (g_RoundHud.flags & HUD_FLAG_TRANSLUCENT_GAUGE) {
        alpha = 127;
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    }
    else {
        alpha = OPAQUE_ALPHA;
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    }

    switch (g_CurrentShot.shotMode) {
        case SHOT_MODE_NORMAL:
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 32, alpha);
            break;
        case SHOT_MODE_POWER:
            gDPSetPrimColor(gfx++, 0, 0, 255, 32, 32, alpha);
            break;
        default:
            gDPSetPrimColor(gfx++, 0, 0, 64, 127, 255, alpha);
            break;
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE
    );
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_ClubPanelImage)),
        G_IM_FMT_I,
        GAUGE_TICK_IMAGE_WIDTH,
        GAUGE_TICK_IMAGE_HEIGHT,
        0,
        G_TX_WRAP,
        G_TX_WRAP,
        2,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (group = 0; group != GAUGE_GROUP_COUNT; group++) {
        for (tick = 1; tick != GAUGE_TICKS_PER_GROUP; tick++) {
            s32 x = (tick + (group * GAUGE_TICKS_PER_GROUP)) * GAUGE_TICK_SPACING;

            gSPTextureRectangle(
                gfx++,
                QuarterPixels(GAUGE_FULL_X + x),
                QuarterPixels(GAUGE_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
                QuarterPixels(GAUGE_FULL_X + GAUGE_TICK_WIDTH + x),
                QuarterPixels(GAUGE_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TexelRunStep(GAUGE_TICK_IMAGE_HEIGHT, GAUGE_BOTTOM - GAUGE_TOP)
            );
        }
    }

    for (tick = 0; tick != GAUGE_EXTRA_TICK_COUNT; tick++) {
        s32 x = (tick + (group * GAUGE_TICKS_PER_GROUP)) * GAUGE_TICK_SPACING;

        gSPTextureRectangle(
            gfx++,
            QuarterPixels(GAUGE_FULL_X + x),
            QuarterPixels(GAUGE_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            QuarterPixels(GAUGE_FULL_X + GAUGE_TICK_WIDTH + x),
            QuarterPixels(GAUGE_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TexelRunStep(GAUGE_TICK_IMAGE_HEIGHT, GAUGE_BOTTOM - GAUGE_TOP)
        );
    }

    for (group = 0; group != GAUGE_END_TICK_COUNT; group++) {
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(GAUGE_FULL_X + group * GAUGE_LENGTH),
            QuarterPixels(GAUGE_MARKER_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            QuarterPixels(GAUGE_FULL_X + group * GAUGE_LENGTH + GAUGE_TICK_WIDTH),
            QuarterPixels(GAUGE_MARKER_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    tick = 1;
    for (group = 0; group != GAUGE_MEDIUM_TICK_COUNT; group++) {
        gSPTextureRectangle(
            gfx++,
            QuarterPixels(GAUGE_FULL_X + tick * GAUGE_TICKS_PER_GROUP * GAUGE_TICK_SPACING),
            QuarterPixels(GAUGE_MEDIUM_TICK_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            QuarterPixels(
                GAUGE_FULL_X + tick * GAUGE_TICKS_PER_GROUP * GAUGE_TICK_SPACING + GAUGE_TICK_WIDTH
            ),
            QuarterPixels(GAUGE_MEDIUM_TICK_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TexelRunStep(GAUGE_TICK_IMAGE_HEIGHT, GAUGE_MEDIUM_TICK_BOTTOM - GAUGE_MEDIUM_TICK_TOP)
        );

        tick++;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 0, OPAQUE_ALPHA);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    fill = GetGaugeFill();
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(ImageBody(f_ShotModeImage)),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        GAUGE_FILL_IMAGE_WIDTH,
        GAUGE_FILL_IMAGE_HEIGHT,
        0,
        G_TX_WRAP,
        G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (playerState != PLAYER_STATE_AIMING) {
        if (playerState == PLAYER_STATE_SETTING_POWER) {
            f_ChosenPowerFill = fill;
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (playerState != PLAYER_STATE_AIMING) {
        emptyLeft =
            QuarterPixels(GAUGE_EMPTY_X - f_ChosenPowerFill * GAUGE_LENGTH / GAUGE_VALUE_MAX);
        gSPTextureRectangle(
            gfx++,
            emptyLeft,
            QuarterPixels(GAUGE_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            QuarterPixels(GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH),
            QuarterPixels(GAUGE_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
            G_TX_RENDERTILE,
            TexelCoordinate(
                ((GAUGE_FILL_SPAN - (f_ChosenPowerFill * GAUGE_FILL_SPAN_PER_VALUE)) *
                    GAUGE_FILL_IMAGE_WIDTH) /
                GAUGE_FILL_SPAN
            ),
            0,
            TexelRunStep(GAUGE_FILL_IMAGE_WIDTH, GAUGE_FILL_SPAN),
            TexelRunStep(GAUGE_FILL_IMAGE_HEIGHT, GAUGE_BOTTOM - GAUGE_TOP)
        );
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_ClubPanelImage)),
        G_IM_FMT_I,
        GAUGE_TICK_IMAGE_WIDTH,
        GAUGE_TICK_IMAGE_HEIGHT,
        0,
        G_TX_WRAP,
        G_TX_WRAP,
        2,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE
    );
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(GAUGE_EMPTY_X),
        QuarterPixels(GAUGE_MARKER_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
        QuarterPixels(GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH),
        QuarterPixels(GAUGE_MARKER_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    if (isPutter) {
        markerOffset = f_ChosenPowerFill * GAUGE_LENGTH / GAUGE_VALUE_MAX;
    }
    else {
        markerOffset = fill * GAUGE_LENGTH / GAUGE_VALUE_MAX;
    }

    f_MarkerPulseAngle += MARKER_PULSE_STEP;
    pulse = (sinf(f_MarkerPulseAngle) * MARKER_PULSE_LEVEL) + MARKER_PULSE_LEVEL;
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, OPAQUE_ALPHA);
    if (playerState == PLAYER_STATE_AIMING ||
        playerState == PLAYER_STATE_SETTING_POWER ||
        playerState == PLAYER_STATE_SETTING_IMPACT ||
        isPutter) {
        gDPSetEnvColor(gfx++, 0, 0, 0, OPAQUE_ALPHA);
    }
    else if (fill == 0) {
        gDPSetEnvColor(gfx++, pulse, pulse, pulse, OPAQUE_ALPHA);
    }
    else {
        fillMagnitude = fill;
        if (fillMagnitude < 0) {
            fillMagnitude = -fillMagnitude;
        }

        if (fillMagnitude <= lieSpan[0] / 2) {
            gDPSetEnvColor(gfx++, 0, 0, 0, OPAQUE_ALPHA);
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, OPAQUE_ALPHA);
            gDPSetEnvColor(gfx++, 0, 0, 0, OPAQUE_ALPHA);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetCombineLERP(
        gfx++,
        PRIMITIVE,
        ENVIRONMENT,
        TEXEL0,
        ENVIRONMENT,
        TEXEL0,
        0,
        0,
        1,
        PRIMITIVE,
        ENVIRONMENT,
        TEXEL0,
        ENVIRONMENT,
        TEXEL0,
        0,
        0,
        1
    );
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(GAUGE_EMPTY_X - markerOffset),
        QuarterPixels(GAUGE_MARKER_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
        QuarterPixels(GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH - markerOffset),
        QuarterPixels(GAUGE_MARKER_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    switch (playerState) {
        case PLAYER_STATE_AIMING:
        case PLAYER_STATE_SETTING_POWER:
            break;
        default:
            if (!isPutter) {
                gDPSetPrimColor(gfx++, 0, 0, 191, 191, 191, OPAQUE_ALPHA);
                markerOffset = f_ChosenPowerFill * GAUGE_LENGTH / GAUGE_VALUE_MAX;
                if (f_ChosenPowerFill == GAUGE_VALUE_MAX) {
                    gDPSetEnvColor(gfx++, pulse, pulse, pulse, OPAQUE_ALPHA);
                }
                else {
                    gDPSetEnvColor(gfx++, 0, 0, 0, OPAQUE_ALPHA);
                }

                gDPPipeSync(gfx++);
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(GAUGE_EMPTY_X - markerOffset),
                    QuarterPixels(GAUGE_MARKER_TOP + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
                    QuarterPixels(GAUGE_EMPTY_X + GAUGE_MARKER_WIDTH - markerOffset),
                    QuarterPixels(GAUGE_MARKER_BOTTOM + g_ShotPanelOffsetX + f_ClubPanelDebugValue),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    if (lieSpan[0] != 0) {
        guTranslate(
            &f_SpanBarTranslation,
            SPAN_BAR_X,
            SPAN_BAR_Y - g_ShotPanelOffsetX - f_ClubPanelDebugValue,
            SPAN_BAR_Z
        );
        guScale(
            &f_SpanBarScale,
            lieSpan[0] * SPAN_BAR_SCALE_PER_SPAN,
            SPAN_BAR_THICKNESS,
            SPAN_BAR_THICKNESS
        );
        gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_SpanBarTranslation),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_SpanBarScale),
            G_MTX_MODELVIEW | G_MTX_MUL | G_MTX_NOPUSH
        );
        gSPVertex(gfx++, f_SpanBarVertices, SPAN_BAR_VERTEX_COUNT, 0);
        gSPDisplayList(gfx++, f_SpanBarDisplayList);
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawClubPanelIfShown(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    if (g_RoundHud.isClubPanelShown) {
        UpdateClubPanelDebugValue();
        DrawClubPanel(&gfx);
        *displayListHead = gfx;
    }
}
