#include <ultra64.h>

#include "audio/sound.h"
#include "camera/view_projection.h"
#include "characters/character_model.h"
#include "course/course.h"
#include "course/hole_loader.h"
#include "game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "round/bets.h"
#include "round/fireworks.h"
#include "round/game_state.h"
#include "round/player.h"
#include "round/round_hud.h"
#include "round/round_stats.h"
#include "round/scorecard.h"
#include "round/scoring.h"
#include "round/tournament.h"
#include "save.h"
#include "scene.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/frame_time.h"
#include "system/memory_address.h"
#include "text/text.h"

#define PLACE_SUFFIX_X 24
#define PLACE_SUFFIX_PARTS_OFFSET \
    (LARGE_PALETTED_TEXELS_OFFSET + (PLACE_NUMBER_PART_COUNT * PLACE_PART_IMAGE_SIZE))

#define LABEL_WIDTH 80
#define LABEL_HEIGHT 18
#define LABEL_SIZE (LABEL_WIDTH * LABEL_HEIGHT / 2)
#define LABEL_SHOWN_WIDTH 54

#define BET_TALLY_LEFT 64
#define BET_TALLY_TOP 96
#define BET_TALLY_COLUMN_SPACING 34
#define BET_TALLY_ROW_SPACING 36
#define BET_TOTAL_LEFT 302

#define LEADERBOARD_SCORE_COLUMN 12
#define LEADERBOARD_HOLES_COLUMN 18
#define MIRRORED_FACE_MASK 5

#define HOLE_NUMBER_ENV_COLOR 0xFFFF0000
#define HOLE_NUMBER_PRIM_COLOR 0x30FF0000
#define HEADING_ENV_COLOR 0xFF730000
#define HEADING_PRIM_COLOR 0xFFFF1C00
#define STANDINGS_HOLE_LEFT 40
#define STANDINGS_HOLE_TOP 48
#define SCORE_LABEL 2
#define BETS_PAGE_TALLY_LEFT 53
#define BETS_PAGE_TALLY_TOP 112

enum BetSubtitle {
    NO_BET_SUBTITLE = -1,
    BET_SUBTITLE_SCORE,
    BET_SUBTITLE_SKINS,
    BET_SUBTITLE_NASSAU
};

typedef s32 BetSubtitle;
typedef s8 BetSubtitle8;

#define FLAP_TOP_QUAD 0
#define FLAP_BOTTOM_QUAD 1
#define FLAP_NAME_QUAD 2
#define FLAP_QUAD_COUNT 3

enum ScoresStyle {
    SCORES_STYLE_YELLOW,
    SCORES_STYLE_FADED,
    SCORES_STYLE_OPAQUE,
    SCORES_STYLE_TINTED
};

typedef s32 ScoresStyle;

static const char f_BlankBetName[];

static s32 f_MatchResultFrameCount = 0;
static u8 f_WinMarkFrames[] = {3, 4, 5, 0, 1, 2, 3, 4, 5, 0};
static char f_CarryOverText[] = "Carry over";
static Radians f_WinColorAngle = 0.0f;
static const char *f_BetNames[BET_COUNT] = {
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
};
static char f_UnusedCarryText[] UNUSED = "Carry";
static s32 f_BetBlinkFrame = 0;
static s32 f_BetBlinkTime = 0;
static BetSubtitle8 f_BetSubtitleRows[BET_COUNT] = {
    NO_BET_SUBTITLE,
    NO_BET_SUBTITLE,
    BET_SUBTITLE_SCORE,
    BET_SUBTITLE_NASSAU,
    BET_SUBTITLE_SKINS,
    NO_BET_SUBTITLE,
    NO_BET_SUBTITLE,
};
static Vtx f_FlapVertices[FLAP_QUAD_COUNT][QUAD_VERTEX_COUNT] = {
    {
        {{{-128, 5, 0}, 0, {0, 0}, {0, 0, 127, 0xFF}}},
        {{{-128, -1, 0}, 0, {0, 0x180}, {0, 0, 127, 0xFF}}},
        {{{128, -1, 0}, 0, {0x3FC0, 0x180}, {0, 0, 127, 0xFF}}},
        {{{128, 5, 0}, 0, {0x3FC0, 0}, {0, 0, 127, 0xFF}}},
    },
    {
        {{{-128, -7, 0}, 0, {0, 0x340}, {0, 0, 0x81, 0xFF}}},
        {{{-128, -2, 0}, 0, {0, 0x200}, {0, 0, 0x81, 0xFF}}},
        {{{128, -2, 0}, 0, {0x3FC0, 0x200}, {0, 0, 0x81, 0xFF}}},
        {{{128, -7, 0}, 0, {0x3FC0, 0x340}, {0, 0, 0x81, 0xFF}}},
    },
    {
        {{{-64, 5, 0}, 0, {0, 0}, {0, 0, 127, 0xFF}}},
        {{{-64, -1, 0}, 0, {0, 0x180}, {0, 0, 127, 0xFF}}},
        {{{64, -1, 0}, 0, {0x1FC0, 0x180}, {0, 0, 127, 0xFF}}},
        {{{64, 5, 0}, 0, {0x1FC0, 0}, {0, 0, 127, 0xFF}}},
    },
};
static Lights1 f_FlapLights = gdSPDefLights1(0xFF, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0, 0, -127);
static Radians f_ArrowPulseAngle = 0.0f;
static Radians f_ScorePulseAngle = 0.0f;
static ScoresStyle f_ScoresStyle = SCORES_STYLE_OPAQUE;
static Radians f_BadgeGlowAngle = 0.0f;

#define MATCH_TITLE_X 32
#define MATCH_TITLE_Y 28
#define MATCH_HOLE_X 136
#define MATCH_HOLE_Y 48
#define MATCH_RESULT_LEFT 64
#define MATCH_RESULT_SPACING 128
#define MATCH_RESULT_Y 64
#define RESULT_CHARACTER_LEFT (-56.0f)
#define RESULT_CHARACTER_SPACING 112.0f
#define RESULT_CHARACTER_Y (-80.0f)
#define RESULT_CHARACTER_Z (-500.0f)
#define RESULT_CHARACTER_SCALE 5.0f

void DrawScorecardFace(Gfx **displayListHead, s32 left, s32 top, s32 playerIndex) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, g_Scorecard.facePalettes[playerIndex]);
    LoadTextureBlock(
        &gfx,
        g_Scorecard.faceImages[playerIndex],
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        FACE_FRAME_SIZE,
        FACE_FRAME_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + FACE_FRAME_SIZE),
        QuarterPixels(top + FACE_FRAME_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

void DrawSmallFace(Gfx **displayListHead, s32 left, s32 top, s32 playerIndex, Boolean isMirrored) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, g_PlayerFacePalettes[playerIndex]);
    LoadTextureBlock(
        &gfx,
        g_PlayerFaceImages[playerIndex],
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        FACE_WIDTH,
        FACE_HEIGHT,
        0,
        G_TX_MIRROR,
        G_TX_MIRROR,
        isMirrored * MIRRORED_FACE_MASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + FACE_WIDTH),
        QuarterPixels(top + FACE_HEIGHT),
        G_TX_RENDERTILE,
        isMirrored * TexelCoordinate(FACE_WIDTH),
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

static Mtx f_ResultCharacterMatrices[MATCH_PLAYER_COUNT];
static Mtx f_FlapMatrices[LEADERBOARD_ROW_COUNT][MATRIX_BUFFER_COUNT];

void DrawPlace(Gfx **displayListHead, s32 left, s32 top, s32 place) {
    Gfx *gfx = *displayListHead;

    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.placesImage));
    LoadTextureBlock(
        &gfx,
        g_Scorecard.placesImage + ((place * PLACE_PART_IMAGE_SIZE) + PLACE_SUFFIX_PARTS_OFFSET),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PLACE_PART_SIZE,
        PLACE_PART_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(left + PLACE_SUFFIX_X),
        QuarterPixels(top),
        QuarterPixels(left + PLACE_SUFFIX_X + PLACE_PART_SIZE),
        QuarterPixels(top + PLACE_PART_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    LoadTextureBlock(
        &gfx,
        LargeImageTexelsAt(g_Scorecard.placesImage, place * PLACE_PART_IMAGE_SIZE),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PLACE_PART_SIZE,
        PLACE_PART_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + PLACE_PART_SIZE),
        QuarterPixels(top + PLACE_PART_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

void DrawScorecardLabel(Gfx **displayListHead, s32 left, s32 top, s32 labelIndex) {
    Gfx *gfx = *displayListHead;

    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.labelsImage));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(g_Scorecard.labelsImage) + labelIndex * LABEL_SIZE),
        G_IM_FMT_CI,
        LABEL_WIDTH,
        LABEL_HEIGHT,
        0,
        0,
        LABEL_WIDTH - 1,
        LABEL_HEIGHT - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + LABEL_SHOWN_WIDTH),
        QuarterPixels(top + LABEL_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

void DrawMatchResult(Gfx **displayListHead) {
    s32 holeNumber = g_Scorecard.content.holeIndex;
    s32 playerIndex;
    s32 x;
    s32 holeIndex;
    s32 ownHole;
    Gfx *gfx = *displayListHead;

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    DrawLargeText(&gfx, MATCH_TITLE_X, MATCH_TITLE_Y, "Match Game");
    sprintf(g_PrintBuffer, "ﾎｰﾙ%d", holeNumber);
    DrawLargeText(&gfx, MATCH_HOLE_X, MATCH_HOLE_Y, g_PrintBuffer);
    holeIndex = holeNumber - 1;
    ownHole = holeIndex;
    x = MATCH_RESULT_LEFT;
    for (playerIndex = 0; playerIndex != MATCH_PLAYER_COUNT; playerIndex++) {
        if (g_Players[playerIndex].holes[ownHole].strokeCount ==
            g_Players[playerIndex ^ 1].holes[holeIndex].strokeCount) {
            SetTextColor(&gfx, 0xFF, 0xFF, 0);
            DrawLargeText(&gfx, x, MATCH_RESULT_Y, "DRAW");
        }
        else if (g_Players[playerIndex].holes[ownHole].strokeCount <
            g_Players[playerIndex ^ 1].holes[holeIndex].strokeCount) {
            SetTextColor(&gfx, 0xFF, 0x7F, 0x7F);
            DrawLargeText(&gfx, x, MATCH_RESULT_Y, "WIN");
        }
        else {
            SetTextColor(&gfx, 0x7F, 0x7F, 0xFF);
            DrawLargeText(&gfx, x, MATCH_RESULT_Y, "LOSE");
        }

        x += MATCH_RESULT_SPACING;
    }

    UpdateAndDrawFireworks(&gfx);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    if (g_ResultCharactersLoadStep == RESULT_CHARACTERS_LOADING && LoadResultCharacters() == TRUE) {
        g_ResultCharactersLoadStep = RESULT_CHARACTERS_LOADED;
    }

    if (g_ResultCharactersLoadStep < RESULT_CHARACTERS_LOADING) {
        g_ResultCharactersLoadStep++;
    }

    if (g_ResultCharactersLoadStep == RESULT_CHARACTERS_LOADED) {
        Vec3 translation;
        Vec3 rotation;
        Vec3 scale;
        Mat4 matrix;

        f_MatchResultFrameCount++;
        gSPMatrix(
            gfx++,
            PhysicalAddress(&g_ScreenOrthoMatrix),
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
        );
        for (playerIndex = 0; playerIndex != MATCH_PLAYER_COUNT; playerIndex++) {
            translation[AXIS_X] = (playerIndex * RESULT_CHARACTER_SPACING) + RESULT_CHARACTER_LEFT;
            rotation[AXIS_X] = 0.0f;
            rotation[AXIS_Y] = 0;
            rotation[AXIS_Z] = 0;
            translation[AXIS_Y] = RESULT_CHARACTER_Y;
            translation[AXIS_Z] = RESULT_CHARACTER_Z;
            scale[AXIS_X] = RESULT_CHARACTER_SCALE;
            scale[AXIS_Y] = RESULT_CHARACTER_SCALE;
            scale[AXIS_Z] = RESULT_CHARACTER_SCALE;
            BuildTransformMatrix(matrix, rotation, translation, scale);
            ConvertMatrixToFixed(matrix, &f_ResultCharacterMatrices[playerIndex]);
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_ResultCharacterMatrices[playerIndex]),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
            );
            g_DisplayListHead = gfx;
            DrawCharacter(playerIndex);
            *displayListHead = g_DisplayListHead;
            gfx = *displayListHead;
        }

        AnimateAllCharacters();
    }

    *displayListHead = gfx;
}

#define NO_BETS_SHIFT 32
#define STANDINGS_PLACE_LEFT 32
#define STANDINGS_FACE_LEFT 108
#define STANDINGS_SCORE_LEFT 152
#define STANDINGS_BET_TOTAL_LEFT 224
#define STANDINGS_ROW_TOP 72
#define STANDINGS_TEXT_TOP 82
#define STANDINGS_ROW_SPACING 36
#define STANDINGS_LABEL_TOP 52
#define STANDINGS_SCORE_LABEL_LEFT 160
#define STANDINGS_BET_TOTAL_LABEL_LEFT 248
#define BET_TOTAL_LABEL 4
#define WIDE_BET_TOTAL 1000

void DrawStandingsPage(Gfx **displayListHead) {
    s32 places[PLAYER_COUNT_MAX];
    s32 scoresToPar[PLAYER_COUNT_MAX];
    Gfx *gfx;
    s32 holeIndex;
    s32 shift;
    s32 playerIndex;
    s32 j;
    s32 betTotal;

    gfx = *displayListHead;
    holeIndex = g_Scorecard.content.holeIndex;
    shift = g_Scorecard.hasBets ? 0 : NO_BETS_SHIFT;
    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, "Hole %d", (int)(holeIndex + g_Scorecard.isTallied));
    DrawLargeTextWithShadow(
        &gfx,
        g_Scorecard.x + STANDINGS_HOLE_LEFT,
        g_Scorecard.y + STANDINGS_HOLE_TOP,
        g_PrintBuffer,
        HOLE_NUMBER_ENV_COLOR,
        HOLE_NUMBER_PRIM_COLOR,
        0
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);

    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        scoresToPar[playerIndex] = GetNetScoreToPar(g_Scorecard.content.holeIndex, playerIndex);
    }

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        places[playerIndex] = 0;
        for (j = 0; j != g_GameState.playerCount; j++) {
            if (playerIndex != j && scoresToPar[playerIndex] > scoresToPar[j]) {
                places[playerIndex]++;
            }
        }
    }

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        DrawScorecardFace(
            &gfx,
            shift + STANDINGS_FACE_LEFT + g_Scorecard.x,
            STANDINGS_ROW_TOP + (playerIndex * STANDINGS_ROW_SPACING),
            playerIndex
        );
    }

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        DrawPlace(
            &gfx,
            shift + STANDINGS_PLACE_LEFT + g_Scorecard.x,
            STANDINGS_ROW_TOP + (playerIndex * STANDINGS_ROW_SPACING),
            places[playerIndex] & PLACE_MASK
        );
    }

    DrawScorecardLabel(
        &gfx,
        shift + STANDINGS_SCORE_LABEL_LEFT + g_Scorecard.x,
        STANDINGS_LABEL_TOP,
        SCORE_LABEL
    );
    if (g_Scorecard.hasBets) {
        DrawScorecardLabel(
            &gfx,
            g_Scorecard.x + STANDINGS_BET_TOTAL_LABEL_LEFT,
            STANDINGS_LABEL_TOP,
            BET_TOTAL_LABEL
        );
    }

    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.digitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.digitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        betTotal = 0;
        if (scoresToPar[playerIndex] == 0) {
            sprintf(g_PrintBuffer, "  #0");
        }
        else {
            sprintf(g_PrintBuffer, "%+4d", scoresToPar[playerIndex]);
        }

        DrawDigitText(
            &gfx,
            shift + STANDINGS_SCORE_LEFT + g_Scorecard.x,
            STANDINGS_TEXT_TOP + (playerIndex * STANDINGS_ROW_SPACING),
            g_PrintBuffer
        );
        if (g_Scorecard.hasBets) {
            for (j = 0; j != BET_COUNT; j++) {
                if (g_GameState.betRates[j] > 0) {
                    betTotal += g_BetTallies[playerIndex][j];
                }
            }

            if (betTotal >= WIDE_BET_TOTAL) {
                sprintf(g_PrintBuffer, "%5d", betTotal);
            }
            else {
                sprintf(g_PrintBuffer, "%4d", betTotal);
            }

            DrawDigitText(
                &gfx,
                g_Scorecard.x + STANDINGS_BET_TOTAL_LEFT,
                STANDINGS_TEXT_TOP + (playerIndex * STANDINGS_ROW_SPACING),
                g_PrintBuffer
            );
        }
    }

    *displayListHead = gfx;
}

#define MESSAGE_WINS_NEEDED 1
#define MESSAGE_WIN_ON_DRAW 2
#define SUDDEN_DEATH_ENV_COLOR 0x00FFFF00
#define SUDDEN_DEATH_PRIM_COLOR 0xCF48FF00
#define MATCH_TITLE_LEFT 128
#define MATCH_TITLE_TOP 48
#define MATCH_ROW_SPACING 80
#define WINS_NEEDED_LEFT 88
#define WINS_NEEDED_TOP 72
#define PANEL_LEFT 32
#define PANEL_TOP 64
#define PANEL_WIDTH 128
#define PANEL_HEIGHT 32
#define PANEL_SHOWN_HEIGHT 62
#define PANEL_SECOND_TOP 144
#define TEXEL_STEP_HALF (TEXEL_STEP_ONE / 2)
#define TILE_IMAGE_WIDTH 32
#define TILE_IMAGE_HEIGHT 96
#define TILE_LEFT 36
#define TILE_TOP 98
#define TILE_SPACING 25
#define TILE_PIECE_WIDTH 8
#define TILE_HEIGHT 24
#define TILES_PER_IMAGE_ROW 4
#define TILE_EDGE_IMAGE_ROW 3
#define WIDE_TILE_INDEX 9
#define WIDE_TILE_LEFT 261
#define WIDE_TILE_IMAGE_COLUMN 1
#define WIDE_TILE_IMAGE_ROW 2
#define MATCH_FACE_LEFT 48
#define MATCH_FACE_TOP 64
#define WIN_MARK_WIDTH 48
#define WIN_MARK_HEIGHT 24
#define WIN_MARK_SHOWN_WIDTH 24
#define WIN_MARK_TOP 97
#define WIN_MARK_SIZE (WIN_MARK_WIDTH * WIN_MARK_HEIGHT)
#define WIN_MARK_START_TIME 2
#define WIN_MARK_ANIMATION_TIME 18
#define WIN_MARK_FRAME_DURATION 2
#define WIN_MARK_BOUNCE_HEIGHT 24.0f
#define WIN_MARK_BOUNCE_STEP (M_PI / 12)
#define ARROW_WIDTH 16
#define ARROW_HEIGHT 8
#define ARROW_LEFT 40
#define ARROW_TOP 81
#define MATCH_COUNT_TIME 20
#define MATCH_ANIMATION_TIME 30

void DrawMatchPage(Gfx **displayListHead) {
    s32 wins[MATCH_PLAYER_COUNT];
    Gfx *gfx;
    s32 winsNeeded;
    Boolean canDraw = FALSE;
    s32 halvedCount;
    s32 countEnd;
    s32 holeIndex;
    s32 i;
    s32 shareCount;
    s32 j;
    s32 imageLeft;
    s32 imageTop;
    s32 frame;
    s32 markImage;
    s32 bounce;
    s32 arrowRise;

    gfx = *displayListHead;
    if (g_Scorecard.content.isSuddenDeath == TRUE) {
        winsNeeded = 2;
    }
    else {
        winsNeeded = GetRoundHoleCount() + 2;
    }

    if (g_Scorecard.skinsCarryover.onLastHole > 0) {
        if (++g_Scorecard.skinsCarryover.onLastHole == MATCH_COUNT_TIME) {
            g_Scorecard.skinsCarryover.intoLastHole++;
        }

        if (g_Scorecard.skinsCarryover.onLastHole == MATCH_ANIMATION_TIME) {
            g_Scorecard.skinsCarryover.onLastHole = 0;
        }
    }

    countEnd = g_Scorecard.skinsCarryover.intoLastHole;
    halvedCount = 0;
    wins[1] = 0;
    wins[0] = 0;
    for (i = GetFirstHoleIndex(); i != countEnd; i++) {
        if (g_Players[0].holes[i].strokeCount < g_Players[1].holes[i].strokeCount) {
            wins[0]++;
        }
        else if (g_Players[1].holes[i].strokeCount < g_Players[0].holes[i].strokeCount) {
            wins[1]++;
        }
        else if (g_Players[0].holes[i].strokeCount == g_Players[1].holes[i].strokeCount) {
            winsNeeded--;
            halvedCount++;
        }
    }

    if (g_Scorecard.skinsCarryover.onLastHole == MATCH_COUNT_TIME &&
        g_Scorecard.content.isSuddenDeath == FALSE &&
        g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED &&
        (halvedCount & 1) == ((GetRoundHoleCount() - 1) & 1)) {
        PlaySoundWithEffects(
            SOUND_LOOK_BACK,
            TALLY_SOUND_CHANNEL,
            SCORECARD_SOUND_VOLUME,
            NO_REVERB,
            SOUND_FULL_SCALE
        );
    }

    shareCount = winsNeeded;
    winsNeeded = shareCount / 2;
    if ((shareCount - 1) / 2 != winsNeeded) {
        canDraw = TRUE;
    }

    if (winsNeeded <= 0) {
        winsNeeded = 1;
    }

    SetUpGradientTextDrawing(&gfx, 0);
    gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0, OPAQUE_ALPHA);
    gDPSetPrimColor(gfx++, 0, 0, 0x30, 0xFF, 0, OPAQUE_ALPHA);
    sprintf(g_PrintBuffer, "Hole%d", g_Scorecard.content.holeIndex + g_Scorecard.isTallied);
    DrawLargeTextWithShadow(
        &gfx,
        g_Scorecard.x + STANDINGS_HOLE_LEFT,
        g_Scorecard.y + STANDINGS_HOLE_TOP,
        g_PrintBuffer,
        HOLE_NUMBER_ENV_COLOR,
        HOLE_NUMBER_PRIM_COLOR,
        0
    );
    if (g_Scorecard.content.isSuddenDeath) {
        sprintf(g_PrintBuffer, "SUDDEN DEATH");
        DrawLargeTextWithShadow(
            &gfx,
            g_Scorecard.x + MATCH_TITLE_LEFT,
            g_Scorecard.y + MATCH_TITLE_TOP,
            g_PrintBuffer,
            SUDDEN_DEATH_ENV_COLOR,
            SUDDEN_DEATH_PRIM_COLOR,
            0
        );
    }
    else {
        s32 points[MATCH_PLAYER_COUNT];

        points[0] = 0;
        points[1] = 0;
        for (i = GetFirstHoleIndex(); i != g_Scorecard.skinsCarryover.fromLastHole; i++) {
            if (g_Players[0].holes[i].strokeCount < g_Players[1].holes[i].strokeCount) {
                points[0] += 2;
            }
            else if (g_Players[1].holes[i].strokeCount < g_Players[0].holes[i].strokeCount) {
                points[1] += 2;
            }
            else if (g_Players[0].holes[i].strokeCount == g_Players[1].holes[i].strokeCount) {
                points[0]++;
                points[1]++;
            }
        }

        if (GetRoundHoleCount() - 1 < points[0] || GetRoundHoleCount() - 1 < points[1]) {
            sprintf(g_PrintBuffer, "DORMIE");
            DrawLargeTextWithShadow(
                &gfx,
                g_Scorecard.x + MATCH_TITLE_LEFT,
                g_Scorecard.y + MATCH_TITLE_TOP,
                g_PrintBuffer,
                HEADING_ENV_COLOR,
                HEADING_PRIM_COLOR,
                0
            );
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
        if (canDraw == TRUE && winsNeeded - wins[i] == 1 && winsNeeded - wins[i ^ 1] >= 2) {
            SetTextSlot(
                g_Scorecard.x + WINS_NEEDED_LEFT,
                (i * MATCH_ROW_SPACING) + WINS_NEEDED_TOP + g_Scorecard.y,
                MESSAGE_WIN_ON_DRAW,
                TEXT_SLOT_LARGE_FONT,
                i,
                0,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
        }
        else if (winsNeeded != wins[i] && winsNeeded != wins[i ^ 1]) {
            SetTextSlotValue(winsNeeded - wins[i], i, 0);
            SetTextSlot(
                g_Scorecard.x + WINS_NEEDED_LEFT,
                (i * MATCH_ROW_SPACING) + WINS_NEEDED_TOP + g_Scorecard.y,
                MESSAGE_WINS_NEEDED,
                TEXT_SLOT_LARGE_FONT,
                i,
                0,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
        }
        else {
            SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, i, TEXT_FLAG_TYPED, 0, 0, 0, 0);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.matchPanelImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.matchPanelImage),
        G_IM_FMT_CI,
        PANEL_WIDTH,
        PANEL_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(g_Scorecard.x + PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + PANEL_TOP),
        QuarterPixels(g_Scorecard.x + PANEL_LEFT + (PANEL_WIDTH * 2)),
        QuarterPixels(g_Scorecard.y + PANEL_TOP + PANEL_SHOWN_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_HALF,
        TEXEL_STEP_HALF
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(g_Scorecard.x + PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + PANEL_SECOND_TOP),
        QuarterPixels(g_Scorecard.x + PANEL_LEFT + (PANEL_WIDTH * 2)),
        QuarterPixels(g_Scorecard.y + PANEL_SECOND_TOP + PANEL_SHOWN_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_HALF,
        TEXEL_STEP_HALF
    );

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.winsNeededImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.winsNeededImage),
        G_IM_FMT_CI,
        TILE_IMAGE_WIDTH,
        TILE_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
        for (j = 0; j != winsNeeded; j++) {
            imageLeft = (j % TILES_PER_IMAGE_ROW) * TILE_PIECE_WIDTH;
            imageTop = (j / TILES_PER_IMAGE_ROW) * TILE_HEIGHT;
            if (j == WIDE_TILE_INDEX) {
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(WIDE_TILE_LEFT + g_Scorecard.x),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + g_Scorecard.y),
                    QuarterPixels(WIDE_TILE_LEFT + (TILE_PIECE_WIDTH * 3) + g_Scorecard.x),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + TILE_HEIGHT + g_Scorecard.y),
                    G_TX_RENDERTILE,
                    TexelCoordinate(WIDE_TILE_IMAGE_COLUMN * TILE_PIECE_WIDTH),
                    TexelCoordinate(WIDE_TILE_IMAGE_ROW * TILE_HEIGHT),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
            else {
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels((j * TILE_SPACING) + TILE_LEFT + g_Scorecard.x),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + g_Scorecard.y),
                    QuarterPixels(
                        (j * TILE_SPACING) + TILE_LEFT + TILE_PIECE_WIDTH + g_Scorecard.x
                    ),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + TILE_HEIGHT + g_Scorecard.y),
                    G_TX_RENDERTILE,
                    0,
                    TexelCoordinate(TILE_EDGE_IMAGE_ROW * TILE_HEIGHT),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(
                        (j * TILE_SPACING) + TILE_LEFT + TILE_PIECE_WIDTH + g_Scorecard.x
                    ),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + g_Scorecard.y),
                    QuarterPixels(
                        (j * TILE_SPACING) + TILE_LEFT + (TILE_PIECE_WIDTH * 2) + g_Scorecard.x
                    ),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + TILE_HEIGHT + g_Scorecard.y),
                    G_TX_RENDERTILE,
                    TexelCoordinate(imageLeft),
                    TexelCoordinate(imageTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(
                        (j * TILE_SPACING) + TILE_LEFT + (TILE_PIECE_WIDTH * 2) + g_Scorecard.x
                    ),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + g_Scorecard.y),
                    QuarterPixels(
                        (j * TILE_SPACING) + TILE_LEFT + (TILE_PIECE_WIDTH * 3) + g_Scorecard.x
                    ),
                    QuarterPixels((i * MATCH_ROW_SPACING) + TILE_TOP + TILE_HEIGHT + g_Scorecard.y),
                    G_TX_RENDERTILE,
                    TexelCoordinate(TILE_PIECE_WIDTH),
                    TexelCoordinate(TILE_EDGE_IMAGE_ROW * TILE_HEIGHT),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawScorecardFace(
            &gfx,
            g_Scorecard.x + MATCH_FACE_LEFT,
            (i * MATCH_ROW_SPACING) + MATCH_FACE_TOP,
            i
        );
    }

    gDPPipeSync(gfx++);
    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.winMarkImage));
    LoadTextureBlock(
        &gfx,
        LargeImageTexels(g_Scorecard.winMarkImage),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        WIN_MARK_WIDTH,
        WIN_MARK_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
        for (j = 0; j != wins[i]; j++) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((j * TILE_SPACING) + TILE_LEFT + g_Scorecard.x),
                QuarterPixels((i * MATCH_ROW_SPACING) + WIN_MARK_TOP + g_Scorecard.y),
                QuarterPixels(
                    (j * TILE_SPACING) + TILE_LEFT + WIN_MARK_SHOWN_WIDTH + g_Scorecard.x
                ),
                QuarterPixels(
                    (i * MATCH_ROW_SPACING) + WIN_MARK_TOP + WIN_MARK_HEIGHT + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                (j < wins[i ^ 1]) ? TexelCoordinate(WIN_MARK_SHOWN_WIDTH) : 0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    if ((u8)(g_Scorecard.skinsCarryover.onLastHole - WIN_MARK_START_TIME) <
        WIN_MARK_ANIMATION_TIME) {
        frame = g_Scorecard.skinsCarryover.onLastHole - WIN_MARK_START_TIME;
        markImage = f_WinMarkFrames[frame / WIN_MARK_FRAME_DURATION];
        bounce = -(s32)(sinf((f32)frame * WIN_MARK_BOUNCE_STEP) * WIN_MARK_BOUNCE_HEIGHT);
        bounce = (bounce <= 0) ? bounce : 0;
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(g_Scorecard.winMarkImage) + (markImage * WIN_MARK_SIZE),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            WIN_MARK_WIDTH,
            WIN_MARK_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        holeIndex = g_Scorecard.skinsCarryover.intoLastHole;
        if (g_Players[0].holes[holeIndex].strokeCount < g_Players[1].holes[holeIndex].strokeCount) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((wins[0] * TILE_SPACING) + TILE_LEFT + g_Scorecard.x),
                QuarterPixels(WIN_MARK_TOP + bounce + g_Scorecard.y),
                QuarterPixels(
                    (wins[0] * TILE_SPACING) + TILE_LEFT + WIN_MARK_SHOWN_WIDTH + g_Scorecard.x
                ),
                QuarterPixels(WIN_MARK_TOP + WIN_MARK_HEIGHT + bounce + g_Scorecard.y),
                G_TX_RENDERTILE,
                (wins[1] < wins[0]) ? TexelCoordinate(WIN_MARK_SHOWN_WIDTH) : 0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else if (g_Players[1].holes[holeIndex].strokeCount <
            g_Players[0].holes[holeIndex].strokeCount) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((wins[1] * TILE_SPACING) + TILE_LEFT + g_Scorecard.x),
                QuarterPixels(MATCH_ROW_SPACING + WIN_MARK_TOP + bounce + g_Scorecard.y),
                QuarterPixels(
                    (wins[1] * TILE_SPACING) + TILE_LEFT + WIN_MARK_SHOWN_WIDTH + g_Scorecard.x
                ),
                QuarterPixels(
                    MATCH_ROW_SPACING + WIN_MARK_TOP + WIN_MARK_HEIGHT + bounce + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                (wins[0] < wins[1]) ? TexelCoordinate(WIN_MARK_SHOWN_WIDTH) : 0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.winnerArrowImage));
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexels(g_Scorecard.winnerArrowImage),
            G_IM_FMT_CI,
            ARROW_WIDTH,
            ARROW_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        arrowRise = g_Scorecard.skinsCarryover.onLastHole;
        holeIndex = g_Scorecard.skinsCarryover.intoLastHole;
        if (g_Players[0].holes[holeIndex].strokeCount < g_Players[1].holes[holeIndex].strokeCount) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((wins[0] * TILE_SPACING) + ARROW_LEFT + g_Scorecard.x),
                QuarterPixels(-arrowRise + ARROW_TOP + g_Scorecard.y),
                QuarterPixels((wins[0] * TILE_SPACING) + ARROW_LEFT + ARROW_WIDTH + g_Scorecard.x),
                QuarterPixels(-arrowRise + ARROW_TOP + ARROW_HEIGHT + g_Scorecard.y),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else if (g_Players[1].holes[holeIndex].strokeCount <
            g_Players[0].holes[holeIndex].strokeCount) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((wins[1] * TILE_SPACING) + ARROW_LEFT + g_Scorecard.x),
                QuarterPixels(-arrowRise + MATCH_ROW_SPACING + ARROW_TOP + g_Scorecard.y),
                QuarterPixels((wins[1] * TILE_SPACING) + ARROW_LEFT + ARROW_WIDTH + g_Scorecard.x),
                QuarterPixels(
                    -arrowRise + MATCH_ROW_SPACING + ARROW_TOP + ARROW_HEIGHT + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

#define SKINS_TITLE_LEFT 122
#define SKINS_TITLE_TOP 52
#define SKINS_ROW_TOP 72
#define SKINS_ROW_SPACING 36
#define SKINS_WIN_LEFT 32
#define SKINS_WIN_TOP 82
#define SKINS_PLACE_LEFT 56
#define SKINS_FACE_LEFT 120
#define SKIN_ICON_LEFT 168
#define SKIN_ICON_TOP 80
#define SKIN_ICON_SIZE 24
#define SKINS_COUNT_LEFT 208
#define SKINS_COUNT_TOP 83
#define CARRYOVER_ICON_LEFT 204
#define CARRYOVER_ICON_TOP 46
#define CARRYOVER_COUNT_LEFT 236
#define CARRYOVER_COUNT_TOP 49
#define SKIN_ICON_TEXELS_OFFSET LARGE_PALETTED_TEXELS_OFFSET
#define SKINS_DIGIT_FONT_HEIGHT (DIGIT_GLYPH_COLON * DIGIT_GLYPH_SIZE)
#define WIN_COLOR_STEP (M_PI / 9)
#define WIN_COLOR_MIDPOINT 0x7F
#define WIN_COLOR_SWING 127.0f

void DrawSkinsPage(Gfx **displayListHead) {
    s32 places[PLAYER_COUNT_MAX];
    s32 skinCounts[PLAYER_COUNT_MAX];
    Gfx *gfx;
    s32 holeIndex;
    s32 playerIndex;
    s32 otherIndex;
    s32 red;
    s32 green;

    gfx = *displayListHead;
    holeIndex = g_Scorecard.content.holeIndex;
    SetUpGradientTextDrawing(&gfx, 0);
    sprintf(g_PrintBuffer, "Hole%d", holeIndex + g_Scorecard.isTallied);
    DrawLargeTextWithShadow(
        &gfx,
        g_Scorecard.x + STANDINGS_HOLE_LEFT,
        g_Scorecard.y + STANDINGS_HOLE_TOP,
        g_PrintBuffer,
        HOLE_NUMBER_ENV_COLOR,
        HOLE_NUMBER_PRIM_COLOR,
        0
    );
    sprintf(g_PrintBuffer, "%s", f_CarryOverText);
    DrawLargeTextWithShadow(
        &gfx,
        g_Scorecard.x + SKINS_TITLE_LEFT,
        g_Scorecard.y + SKINS_TITLE_TOP,
        g_PrintBuffer,
        HEADING_ENV_COLOR,
        HEADING_PRIM_COLOR,
        0
    );

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (g_Scorecard.holeWinnerIndex == playerIndex) {
            skinCounts[playerIndex] =
                g_RoundHud.skinsTotals[playerIndex] - g_Scorecard.skinsCarryover.onLastHole;
        }
        else {
            skinCounts[playerIndex] = g_RoundHud.skinsTotals[playerIndex];
        }
    }

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        places[playerIndex] = 0;
        for (otherIndex = 0; otherIndex != g_GameState.playerCount; otherIndex++) {
            if (playerIndex != otherIndex) {
                if (skinCounts[playerIndex] < skinCounts[otherIndex]) {
                    places[playerIndex]++;
                }

                if (skinCounts[playerIndex] == skinCounts[otherIndex]) {
                    places[playerIndex] |= PLACE_TIED;
                }
            }
        }
    }

    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (g_Scorecard.holeWinnerIndex >= 0 &&
            g_Players[playerIndex].holes[holeIndex].strokeCount != 0) {
            f_WinColorAngle += WIN_COLOR_STEP;
            red = (u8)((s32)(sinf(f_WinColorAngle) * WIN_COLOR_SWING) + WIN_COLOR_MIDPOINT);
            green =
                (u8)((s32)(sinf(f_WinColorAngle + M_PI) * WIN_COLOR_SWING) + WIN_COLOR_MIDPOINT);
            SetTextColor(
                &gfx,
                red,
                green,
                (u8)((s32)(sinf(f_WinColorAngle + (M_PI / 2)) * WIN_COLOR_SWING) +
                    WIN_COLOR_MIDPOINT)
            );
            DrawSmallText(
                &gfx,
                g_Scorecard.x + SKINS_WIN_LEFT,
                SKINS_WIN_TOP + (playerIndex * SKINS_ROW_SPACING),
                "WIN"
            );
        }
    }

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        DrawScorecardFace(
            &gfx,
            g_Scorecard.x + SKINS_FACE_LEFT,
            SKINS_ROW_TOP + (playerIndex * SKINS_ROW_SPACING),
            playerIndex
        );
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        DrawPlace(
            &gfx,
            g_Scorecard.x + SKINS_PLACE_LEFT,
            SKINS_ROW_TOP + (playerIndex * SKINS_ROW_SPACING),
            places[playerIndex] & PLACE_MASK
        );
    }

    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.winMarkImage));
    LoadTextureBlock(
        &gfx,
        g_Scorecard.winMarkImage + SKIN_ICON_TEXELS_OFFSET,
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        SKIN_ICON_SIZE,
        SKIN_ICON_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + SKIN_ICON_LEFT),
            QuarterPixels(SKIN_ICON_TOP + (playerIndex * SKINS_ROW_SPACING)),
            QuarterPixels(g_Scorecard.x + SKIN_ICON_LEFT + SKIN_ICON_SIZE),
            QuarterPixels(SKIN_ICON_TOP + SKIN_ICON_SIZE + (playerIndex * SKINS_ROW_SPACING)),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    DrawTextureRectangle(
        &gfx,
        QuarterPixels(g_Scorecard.x + CARRYOVER_ICON_LEFT),
        QuarterPixels(CARRYOVER_ICON_TOP),
        QuarterPixels(g_Scorecard.x + CARRYOVER_ICON_LEFT + SKIN_ICON_SIZE),
        QuarterPixels(CARRYOVER_ICON_TOP + SKIN_ICON_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );

    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.digitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.digitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        SKINS_DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        sprintf(g_PrintBuffer, "* %d", (int)skinCounts[playerIndex]);
        DrawDigitText(
            &gfx,
            g_Scorecard.x + SKINS_COUNT_LEFT,
            SKINS_COUNT_TOP + (playerIndex * SKINS_ROW_SPACING),
            g_PrintBuffer
        );
    }

    sprintf(
        g_PrintBuffer,
        "* %d",
        (g_Scorecard.isTallied == TRUE) ? g_Scorecard.skinsCarryover.fromLastHole :
                                          g_Scorecard.skinsCarryover.intoLastHole
    );
    DrawDigitText(&gfx, g_Scorecard.x + CARRYOVER_COUNT_LEFT, CARRYOVER_COUNT_TOP, g_PrintBuffer);
    *displayListHead = gfx;
}

static const char f_BlankBetName[] = " ";

#define BET_COLUMN_SPACING 35
#define BET_NAME_SPACING 36
#define BET_BLINK_TIME 7
#define UNPLAYED_BET_GRAY 0xBF
#define GRID_IMAGE_SIZE 16
#define GRID_LINE_WIDTH 3
#define GRID_LEFT 32
#define GRID_RIGHT 292
#define GRID_TOP 64
#define GRID_BOTTOM 220
#define GRID_HEADER_LINE_TOP 74
#define GRID_ROW_LINE_TOP 96
#define GRID_ROW_LINE_LEFT 28
#define GRID_ROW_COUNT 4
#define GRID_ROW_HEIGHT 32
#define GRID_COLUMN_LINE_LEFT 54
#define GRID_COLUMN_COUNT 7
#define GRID_STRETCHED_STEP 11
#define GRID_COLUMN_STEP 18
#define GRID_TEXEL_LINE 3
#define GRID_TEXEL_TOP_END 6
#define GRID_TEXEL_BOTTOM_END 9
#define GRID_TEXEL_CROSSING 12
#define BET_IMAGE_SIZE 24
#define BET_IMAGE_LEFT 60
#define BET_IMAGE_TOP 46
#define BET_SUBTITLE_WIDTH 32
#define BET_SUBTITLE_HEIGHT 16
#define BET_SUBTITLE_IMAGE_HEIGHT 48
#define BET_SUBTITLE_LEFT 56
#define BET_SUBTITLE_TOP 58
#define BET_FACE_LEFT 18
#define BET_FACE_TOP 100
#define BET_NAME_LEFT 71
#define BET_NAME_TOP 46
#define BET_RATE_LEFT 74
#define BET_RATE_TOP 48
#define BET_RATE_RED 0xFF
#define BET_RATE_GREEN 0xA0
#define CARRY_OVER_TOP 78
#define CARRY_OVER_SECOND_TOP 86
#define BET_POT_LEFT 69
#define BET_POT_TOP 86
#define NO_TALLY_LEFT 69

void DrawBetsPage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 i;
    s32 j;
    s32 frame;
    s32 column;
    s32 total;

    if (++f_BetBlinkTime >= BET_BLINK_TIME) {
        f_BetBlinkTime = 0;
        f_BetBlinkFrame ^= 1;
    }

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.gridImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.gridImage),
        G_IM_FMT_CI,
        GRID_IMAGE_SIZE,
        GRID_IMAGE_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(g_Scorecard.x + GRID_LEFT),
        QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP),
        QuarterPixels(g_Scorecard.x + GRID_RIGHT),
        QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP + GRID_LINE_WIDTH),
        G_TX_RENDERTILE,
        TexelCoordinate(GRID_TEXEL_LINE),
        0,
        GRID_STRETCHED_STEP,
        TEXEL_STEP_ONE
    );
    for (i = 0; i != GRID_ROW_COUNT; i++) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + GRID_ROW_LINE_LEFT),
            QuarterPixels((i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
            QuarterPixels(g_Scorecard.x + GRID_RIGHT),
            QuarterPixels(
                (i * GRID_ROW_HEIGHT) + (GRID_LINE_WIDTH + GRID_ROW_LINE_TOP) + g_Scorecard.y
            ),
            G_TX_RENDERTILE,
            TexelCoordinate(GRID_TEXEL_LINE),
            0,
            GRID_STRETCHED_STEP,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + GRID_ROW_LINE_LEFT),
            QuarterPixels((i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
            QuarterPixels(g_Scorecard.x + GRID_ROW_LINE_LEFT + GRID_LINE_WIDTH),
            QuarterPixels(
                (i * GRID_ROW_HEIGHT) + (GRID_LINE_WIDTH + GRID_ROW_LINE_TOP) + g_Scorecard.y
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + GRID_RIGHT - GRID_LINE_WIDTH),
            QuarterPixels((i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
            QuarterPixels(g_Scorecard.x + GRID_RIGHT),
            QuarterPixels(
                (i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + GRID_LINE_WIDTH + g_Scorecard.y
            ),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(GRID_TEXEL_LINE),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    for (i = 0; i != GRID_COLUMN_COUNT; i++) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels((i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + g_Scorecard.x),
            QuarterPixels(g_Scorecard.y + GRID_TOP),
            QuarterPixels(
                (i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH + g_Scorecard.x
            ),
            QuarterPixels(g_Scorecard.y + GRID_BOTTOM),
            G_TX_RENDERTILE,
            TexelCoordinate(GRID_TEXEL_LINE),
            TexelCoordinate(GRID_TEXEL_LINE),
            TEXEL_STEP_ONE,
            GRID_COLUMN_STEP
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels((i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + g_Scorecard.x),
            QuarterPixels(g_Scorecard.y + GRID_TOP),
            QuarterPixels(
                (i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH + g_Scorecard.x
            ),
            QuarterPixels(g_Scorecard.y + GRID_TOP + GRID_LINE_WIDTH),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(GRID_TEXEL_TOP_END),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels((i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + g_Scorecard.x),
            QuarterPixels(g_Scorecard.y + GRID_BOTTOM - GRID_LINE_WIDTH),
            QuarterPixels(
                (i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH + g_Scorecard.x
            ),
            QuarterPixels(g_Scorecard.y + GRID_BOTTOM),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(GRID_TEXEL_BOTTOM_END),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    for (i = 0; i != GRID_COLUMN_COUNT; i++) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels((i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + g_Scorecard.x),
            QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP),
            QuarterPixels(
                (i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH + g_Scorecard.x
            ),
            QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP + GRID_LINE_WIDTH),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(GRID_TEXEL_CROSSING),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        for (j = 0; j != GRID_ROW_COUNT; j++) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + g_Scorecard.x),
                QuarterPixels((j * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
                QuarterPixels(
                    (i * BET_COLUMN_SPACING) + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH +
                    g_Scorecard.x
                ),
                QuarterPixels(
                    (j * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + GRID_LINE_WIDTH + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GRID_TEXEL_CROSSING),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
    j = 0;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            frame = 0;
            if (g_GameState.betRates[i] == 0) {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    UNPLAYED_BET_GRAY,
                    UNPLAYED_BET_GRAY,
                    UNPLAYED_BET_GRAY,
                    OPAQUE_ALPHA
                );
            }
            else {
                frame = f_BetBlinkFrame;
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            }

            LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.betImages[j][frame]));
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(ImageTexels(g_Scorecard.betImages[j][frame])),
                G_IM_FMT_CI,
                BET_IMAGE_SIZE,
                BET_IMAGE_SIZE,
                0,
                0,
                BET_IMAGE_SIZE - 1,
                BET_IMAGE_SIZE - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels((j * BET_COLUMN_SPACING) + BET_IMAGE_LEFT + g_Scorecard.x),
                QuarterPixels(BET_IMAGE_TOP),
                QuarterPixels(
                    (j * BET_COLUMN_SPACING) + (BET_IMAGE_LEFT + BET_IMAGE_SIZE) + g_Scorecard.x
                ),
                QuarterPixels(BET_IMAGE_TOP + BET_IMAGE_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            j++;
        }
    }

    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.totalHeadingImage));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(g_Scorecard.totalHeadingImage)),
        G_IM_FMT_CI,
        BET_IMAGE_SIZE,
        BET_IMAGE_SIZE,
        0,
        0,
        BET_IMAGE_SIZE - 1,
        BET_IMAGE_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    DrawTextureRectangle(
        &gfx,
        QuarterPixels((j * BET_COLUMN_SPACING) + BET_IMAGE_LEFT + g_Scorecard.x),
        QuarterPixels(BET_IMAGE_TOP),
        QuarterPixels((j * BET_COLUMN_SPACING) + (BET_IMAGE_LEFT + BET_IMAGE_SIZE) + g_Scorecard.x),
        QuarterPixels(BET_IMAGE_TOP + BET_IMAGE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );

    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.betSubtitlesImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.betSubtitlesImage),
        G_IM_FMT_CI,
        BET_SUBTITLE_WIDTH,
        BET_SUBTITLE_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    column = 0;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            if (f_BetSubtitleRows[i] != NO_BET_SUBTITLE) {
                if (g_GameState.betRates[i] == 0) {
                    SetTextColor(&gfx, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY);
                }
                else {
                    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
                }

                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(
                        (column * BET_COLUMN_SPACING) + BET_SUBTITLE_LEFT + g_Scorecard.x
                    ),
                    QuarterPixels(BET_SUBTITLE_TOP),
                    QuarterPixels(
                        (column * BET_COLUMN_SPACING) + (BET_SUBTITLE_LEFT + BET_SUBTITLE_WIDTH) +
                        g_Scorecard.x
                    ),
                    QuarterPixels(BET_SUBTITLE_TOP + BET_SUBTITLE_HEIGHT),
                    G_TX_RENDERTILE,
                    0,
                    TexelCoordinate(f_BetSubtitleRows[i] * BET_SUBTITLE_HEIGHT),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }

            column++;
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    for (i = 0; i != g_GameState.playerCount; i++) {
        DrawSmallFace(
            &gfx,
            g_Scorecard.x + BET_FACE_LEFT,
            BET_FACE_TOP + (i * GRID_ROW_HEIGHT),
            i,
            TRUE
        );
    }

    SetUpShadowedTextDrawing(&gfx);
    column = 0;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            if (g_GameState.betRates[i] == 0) {
                SetTextColor(&gfx, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY);
            }
            else {
                SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
            }

            DrawCenteredSmallText(
                &gfx,
                (column * BET_NAME_SPACING) + BET_NAME_LEFT + g_Scorecard.x,
                g_Scorecard.y + BET_NAME_TOP,
                (READ_ONLY char *)f_BetNames[i]
            );
            column++;
        }
    }

    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);

    DrawSmallText(&gfx, g_Scorecard.x + 21, g_Scorecard.y + CARRY_OVER_TOP, "C");
    DrawSmallText(&gfx, g_Scorecard.x + 29, g_Scorecard.y + CARRY_OVER_TOP, "a");
    DrawSmallText(&gfx, g_Scorecard.x + 35, g_Scorecard.y + CARRY_OVER_TOP, "r");
    DrawSmallText(&gfx, g_Scorecard.x + 40, g_Scorecard.y + CARRY_OVER_TOP, "r");
    DrawSmallText(&gfx, g_Scorecard.x + 46, g_Scorecard.y + CARRY_OVER_TOP, "y");
    DrawSmallText(&gfx, g_Scorecard.x + 24, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "o");
    DrawSmallText(&gfx, g_Scorecard.x + 31, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "v");
    DrawSmallText(&gfx, g_Scorecard.x + 39, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "e");
    DrawSmallText(&gfx, g_Scorecard.x + 45, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "r");
    j = 0;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] > 0) {
            if (i == BET_LONGEST_DRIVE) {
                sprintf(
                    g_PrintBuffer,
                    "%4d",
                    g_LongestDrivePot - g_GameState.betRates[BET_LONGEST_DRIVE]
                );
                DrawCenteredSmallText(
                    &gfx,
                    (j * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
                    g_Scorecard.y + BET_POT_TOP,
                    g_PrintBuffer
                );
            }

            if (i == BET_NEAREST_PIN) {
                sprintf(
                    g_PrintBuffer,
                    "%4d",
                    g_NearestPinPot - g_GameState.betRates[BET_NEAREST_PIN]
                );
                DrawCenteredSmallText(
                    &gfx,
                    (j * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
                    g_Scorecard.y + BET_POT_TOP,
                    g_PrintBuffer
                );
            }

            if (i == BET_SKINS) {
                sprintf(g_PrintBuffer, "%4d", g_HolePot - g_GameState.betRates[BET_SKINS]);
                DrawCenteredSmallText(
                    &gfx,
                    (j * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
                    g_Scorecard.y + BET_POT_TOP,
                    g_PrintBuffer
                );
            }

            if (i == BET_MATCH_POT) {
                sprintf(g_PrintBuffer, "%4d", g_HolePot - g_GameState.betRates[BET_MATCH_POT]);
                DrawCenteredSmallText(
                    &gfx,
                    (j * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
                    g_Scorecard.y + BET_POT_TOP,
                    g_PrintBuffer
                );
            }
        }

        j += g_GameState.betRates[i] >= 0;
    }

    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, BET_RATE_RED, BET_RATE_GREEN, 0);
    column = 0;
    for (i = 0; i != BET_COUNT; i++) {
        if (g_GameState.betRates[i] != UNSET_BET_RATE) {
            if (g_GameState.betRates[i] != 0) {
                sprintf(g_PrintBuffer, "%2d", g_GameState.betRates[i]);
                DrawCondensedShadowedText(
                    &gfx,
                    (column * BET_COLUMN_SPACING) + BET_RATE_LEFT + g_Scorecard.x,
                    g_Scorecard.y + BET_RATE_TOP,
                    g_PrintBuffer
                );
            }

            column++;
        }
    }

    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != g_GameState.playerCount; i++) {
        total = 0;
        column = 0;
        for (j = 0; j != BET_COUNT; j++) {
            if (g_GameState.betRates[j] == 0) {
                DrawShadowedText(
                    &gfx,
                    (column * BET_COLUMN_SPACING) + NO_TALLY_LEFT + g_Scorecard.x,
                    (i * GRID_ROW_HEIGHT) + BETS_PAGE_TALLY_TOP + g_Scorecard.y,
                    "-"
                );
                column++;
            }
            else if (g_GameState.betRates[j] != UNSET_BET_RATE) {
                total += g_BetTallies[i][j];
                sprintf(g_PrintBuffer, "%5d", g_BetTallies[i][j]);
                DrawCondensedShadowedText(
                    &gfx,
                    (column * BET_COLUMN_SPACING) + BETS_PAGE_TALLY_LEFT + g_Scorecard.x,
                    (i * GRID_ROW_HEIGHT) + BETS_PAGE_TALLY_TOP + g_Scorecard.y,
                    g_PrintBuffer
                );
                column++;
            }
        }

        sprintf(g_PrintBuffer, "%5d", total);
        DrawCondensedShadowedText(
            &gfx,
            (column * BET_COLUMN_SPACING) + BETS_PAGE_TALLY_LEFT + g_Scorecard.x,
            (i * GRID_ROW_HEIGHT) + BETS_PAGE_TALLY_TOP + g_Scorecard.y,
            g_PrintBuffer
        );
    }

    *displayListHead = gfx;
}

#define HOLE_NUMBER_LEFT 32
#define HOLE_TIME_LEFT 64
#define BACK_NINE_SPACING 128
#define HOLE_NUMBER_TOP 50
#define HOLE_TIME_TOP 51
#define HOLE_ROW_SPACING 16
#define NINE_HOLE_COUNT 9
#define ROUND_TOTAL_LABEL_LEFT 96
#define ROUND_TOTAL_LEFT 160
#define ROUND_TIME_TOP 194
#define ROUND_SCORE_TOP 208

void DrawSpeedGolfPage(Gfx **displayListHead) {
    s32 shownHoleCount;
    Gfx *gfx = *displayListHead;
    s32 holeIndex;
    s32 minutes;
    s32 seconds;
    s32 hundredths;
    s32 scoreToPar;

    shownHoleCount = g_Scorecard.content.holeIndex;
    if (g_Players[0].holeStatus != HOLE_STATUS_PLAYING) {
        shownHoleCount++;
    }

    SetUpGradientTextDrawing(&gfx, 0);
    for (holeIndex = 0; holeIndex != NINE_HOLE_COUNT; holeIndex++) {
        sprintf(g_PrintBuffer, "%2d", holeIndex + 1);
        DrawLargeTextWithShadow(
            &gfx,
            g_Scorecard.x + HOLE_NUMBER_LEFT,
            (holeIndex * HOLE_ROW_SPACING) + HOLE_NUMBER_TOP + g_Scorecard.y,
            g_PrintBuffer,
            HOLE_NUMBER_ENV_COLOR,
            HOLE_NUMBER_PRIM_COLOR,
            0
        );
        sprintf(g_PrintBuffer, "%2d", holeIndex + NINE_HOLE_COUNT + 1);
        DrawLargeTextWithShadow(
            &gfx,
            g_Scorecard.x + HOLE_NUMBER_LEFT + BACK_NINE_SPACING,
            (holeIndex * HOLE_ROW_SPACING) + HOLE_NUMBER_TOP + g_Scorecard.y,
            g_PrintBuffer,
            HOLE_NUMBER_ENV_COLOR,
            HOLE_NUMBER_PRIM_COLOR,
            0
        );
    }

    sprintf(g_PrintBuffer, "Time");
    DrawLargeTextWithShadow(
        &gfx,
        g_Scorecard.x + ROUND_TOTAL_LABEL_LEFT,
        g_Scorecard.y + ROUND_TIME_TOP,
        g_PrintBuffer,
        HEADING_ENV_COLOR,
        HEADING_PRIM_COLOR,
        0
    );
    sprintf(g_PrintBuffer, "Score");
    DrawLargeTextWithShadow(
        &gfx,
        g_Scorecard.x + ROUND_TOTAL_LABEL_LEFT,
        g_Scorecard.y + ROUND_SCORE_TOP,
        g_PrintBuffer,
        HEADING_ENV_COLOR,
        HEADING_PRIM_COLOR,
        0
    );

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (holeIndex = 0; holeIndex != NINE_HOLE_COUNT; holeIndex++) {
        if (holeIndex < shownHoleCount) {
            minutes = g_HoleTimeMinutes[holeIndex];
            if (minutes < 0) {
                sprintf(g_PrintBuffer, "%s", "--------");
                DrawMediumText(
                    &gfx,
                    g_Scorecard.x + HOLE_TIME_LEFT,
                    (holeIndex * HOLE_ROW_SPACING) + HOLE_TIME_TOP + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
            else {
                hundredths = g_HoleTimeHundredths[holeIndex];
                seconds = g_HoleTimeSeconds[holeIndex];
                sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, seconds, hundredths);
                DrawMediumText(
                    &gfx,
                    g_Scorecard.x + HOLE_TIME_LEFT,
                    (holeIndex * HOLE_ROW_SPACING) + HOLE_TIME_TOP + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
        }

        if (holeIndex + NINE_HOLE_COUNT < shownHoleCount) {
            minutes = g_HoleTimeMinutes[holeIndex + NINE_HOLE_COUNT];
            if (minutes < 0) {
                sprintf(g_PrintBuffer, "%s", "--------");
                DrawMediumText(
                    &gfx,
                    g_Scorecard.x + HOLE_TIME_LEFT + BACK_NINE_SPACING,
                    (holeIndex * HOLE_ROW_SPACING) + HOLE_TIME_TOP + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
            else {
                hundredths = g_HoleTimeHundredths[holeIndex + NINE_HOLE_COUNT];
                seconds = g_HoleTimeSeconds[holeIndex + NINE_HOLE_COUNT];
                sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, seconds, hundredths);
                DrawMediumText(
                    &gfx,
                    g_Scorecard.x + HOLE_TIME_LEFT + BACK_NINE_SPACING,
                    (holeIndex * HOLE_ROW_SPACING) + HOLE_TIME_TOP + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
        }
    }

    hundredths =
        (s32)(g_GameState.stopwatchFrameCount * ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
        HUNDREDTHS_PER_SECOND;
    seconds = g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND % SECONDS_PER_MINUTE;
    sprintf(
        g_PrintBuffer,
        "%02d:%02d:%02d",
        g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND / SECONDS_PER_MINUTE,
        seconds,
        hundredths
    );
    DrawFixedWidthLargeText(
        &gfx,
        g_Scorecard.x + ROUND_TOTAL_LEFT,
        g_Scorecard.y + ROUND_TIME_TOP,
        g_PrintBuffer
    );
    scoreToPar = GetNetScoreToPar(shownHoleCount, 0);
    if (scoreToPar == 0) {
        sprintf(g_PrintBuffer, "#0");
    }
    else {
        sprintf(g_PrintBuffer, "%+2d", scoreToPar);
    }

    DrawFixedWidthLargeText(
        &gfx,
        g_Scorecard.x + ROUND_TOTAL_LEFT,
        g_Scorecard.y + ROUND_SCORE_TOP,
        g_PrintBuffer
    );
    *displayListHead = gfx;
}

void FormatLeaderboardRow(char *row, TournamentStanding *standing) {
    u8 place = standing->place;
    s16 score;
    s8 holeCount;

    if (place & PLACE_TIED) {
        sprintf(row, "%2dT          ", place & PLACE_MASK);
    }
    else {
        sprintf(row, "%2d           ", place);
    }

    score = standing->score;
    if (score == 0) {
        sprintf(row + LEADERBOARD_SCORE_COLUMN, "   #0   ", 0);
    }
    else {
        sprintf(row + LEADERBOARD_SCORE_COLUMN, "  %+3d   ", score);
    }

    holeCount = standing->holeCount;
    if (holeCount == 0) {
        sprintf(row + LEADERBOARD_HOLES_COLUMN, "  -");
    }
    else {
        sprintf(row + LEADERBOARD_HOLES_COLUMN, " %2d", holeCount);
    }
}

#define PLACE_LABEL 0
#define NAME_LABEL 1
#define HOLES_LABEL 3
#define LEADERBOARD_LABEL_TOP 46
#define PLACE_LABEL_LEFT 32
#define NAME_LABEL_LEFT 100
#define SCORE_LABEL_LEFT 184
#define HOLES_LABEL_LEFT 248
#define LEADERBOARD_ROW_SPACING 16
#define LAST_ROW_FLIP_TIME ((LEADERBOARD_ROW_COUNT - 1) * ROW_FLIP_STAGGER)
#define ROW_IMAGE_WIDTH 256
#define ROW_IMAGE_HEIGHT 14
#define ROW_LEFT 32
#define ROW_TOP 66
#define ROW_HINGE_TOP 72
#define ROW_BOTTOM_HALF_ROW 7
#define FLAP_NAME_IMAGE_WIDTH 128
#define FLAP_NAME_LEFT 96
#define ROW_TEXT_LEFT 34
#define ROW_TEXT_TOP 67
#define RowTextTop(row) (ROW_TEXT_TOP + ((row) * LEADERBOARD_ROW_SPACING) + g_Scorecard.y)
#define ROW_NAME_LEFT 98
#define FLAP_EDGE_ON_ANGLE (M_PI / 2)
#define FLAP_SHADE_RANGE 63.0f
#define FLAP_LIT_SHADE 255.0f
#define FLAP_FACING_SHADE (FLAP_LIT_SHADE - FLAP_SHADE_RANGE - 1.0f)
#define FLAP_DISTANCE (-290.0f)
#define FLAP_NAME_X (-62)
#define FLAP_GLYPH_GROUP_COUNT 10
#define LEADERBOARD_FACE_LEFT 66
#define LEADERBOARD_FACE_TOP 60
#define ARROW_IMAGE_SIZE 32
#define ARROW_CENTER_X 32
#define ARROW_MIRROR_MASK 5
#define ARROW_PULSE_SIZE 2.0f
#define FELL_ARROW_HALF_SIZE 8.0f
#define ROSE_ARROW_HALF_SIZE 12.0f
#define FELL_ARROW_PULSE_STEP 0.4f
#define ROSE_ARROW_PULSE_STEP 0.3f

void DrawLeaderboardPage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    TournamentStanding *standing;
    s32 row;
    s32 shade;
    s32 bottomFlapShade;
    s32 group;
    s32 contestantId;
    f32 halfSize;
    s32 roundedHalfSize;
    s32 texelStart;
    s32 texelStep;

    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    DrawScorecardLabel(&gfx, g_Scorecard.x + PLACE_LABEL_LEFT, LEADERBOARD_LABEL_TOP, PLACE_LABEL);
    DrawScorecardLabel(&gfx, g_Scorecard.x + NAME_LABEL_LEFT, LEADERBOARD_LABEL_TOP, NAME_LABEL);
    DrawScorecardLabel(&gfx, g_Scorecard.x + SCORE_LABEL_LEFT, LEADERBOARD_LABEL_TOP, SCORE_LABEL);
    DrawScorecardLabel(&gfx, g_Scorecard.x + HOLES_LABEL_LEFT, LEADERBOARD_LABEL_TOP, HOLES_LABEL);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.leaderboardRowImage));
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.leaderboardRowImage),
        G_IM_FMT_CI,
        ROW_IMAGE_WIDTH,
        ROW_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);

    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime) {
            shade = COLOR_CHANNEL_MAX;
            if (g_Scorecard.content.isRowFlipping[row] == TRUE &&
                !(g_Scorecard.content.rowFlipAngles[row] > FLAP_EDGE_ON_ANGLE)) {
                shade = (sinf(g_Scorecard.content.rowFlipAngles[row]) * FLAP_SHADE_RANGE) +
                    FLAP_FACING_SHADE;
            }
        }
        else {
            shade = COLOR_CHANNEL_MAX;
        }

        gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, OPAQUE_ALPHA);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + ROW_LEFT),
            QuarterPixels(((row * LEADERBOARD_ROW_SPACING) + ROW_TOP) + g_Scorecard.y),
            QuarterPixels(g_Scorecard.x + ROW_LEFT + ROW_IMAGE_WIDTH),
            QuarterPixels(
                (row * LEADERBOARD_ROW_SPACING) + ROW_TOP + ROW_IMAGE_HEIGHT + g_Scorecard.y
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.flapNameImages));
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (g_Scorecard.content.isRowFlipping[row] == TRUE &&
            (s32)g_Scorecard.content.rowFlipAngles[row] == 0) {
            LoadTextureBlock(
                &gfx,
                g_Scorecard.flapNameImages +
                    ((row * FLAP_NAME_IMAGE_WIDTH * ROW_IMAGE_HEIGHT) +
                        LARGE_PALETTED_TEXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FLAP_NAME_IMAGE_WIDTH,
                ROW_IMAGE_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT),
                QuarterPixels(((row * LEADERBOARD_ROW_SPACING) + ROW_TOP) + g_Scorecard.y),
                QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT + FLAP_NAME_IMAGE_WIDTH),
                QuarterPixels(
                    (row * LEADERBOARD_ROW_SPACING) + (ROW_IMAGE_HEIGHT + ROW_TOP) + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime) {
            FormatLeaderboardRow(g_PrintBuffer, &g_TournamentStandings[g_Scorecard.topRow + row]);
            DrawFixedWidthLargeText(
                &gfx,
                g_Scorecard.x + ROW_TEXT_LEFT,
                RowTextTop(row),
                g_PrintBuffer
            );
            contestantId = g_TournamentStandings[g_Scorecard.topRow + row].contestantId;
            if (contestantId == PLAYER_CONTESTANT) {
                sprintf(g_PrintBuffer, "%-8s  ", g_Players[0].name);
            }
            else {
                sprintf(
                    g_PrintBuffer,
                    "%s",
                    g_ContestantNames[g_ContestantNameIndices[contestantId]]
                );
            }

            DrawLargeText(&gfx, g_Scorecard.x + ROW_NAME_LEFT, RowTextTop(row), g_PrintBuffer);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPSetLights1(gfx++, f_FlapLights);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_LIGHTING | G_SHADING_SMOOTH);
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
            g_Scorecard.content.isRowFlipping[row] == TRUE) {
            Mat4 matrix;
            Vec3 translation;
            Vec3 rotation;

            if (g_Scorecard.content.rowFlipAngles[row] > FLAP_EDGE_ON_ANGLE) {
                rotation[AXIS_X] = g_Scorecard.content.rowFlipAngles[row] + M_PI;
                rotation[AXIS_Y] = 0;
                rotation[AXIS_Z] = 0;
            }
            else {
                rotation[AXIS_X] = g_Scorecard.content.rowFlipAngles[row];
                rotation[AXIS_Y] = 0;
                rotation[AXIS_Z] = 0;
            }

            translation[AXIS_X] = g_Scorecard.x;
            translation[AXIS_Y] = (SCREEN_HEIGHT / 2.0f) -
                ((row * LEADERBOARD_ROW_SPACING) + ROW_HINGE_TOP + g_Scorecard.y);
            translation[AXIS_Z] = FLAP_DISTANCE;
            BuildRotationMatrix(matrix, rotation, translation);
            ConvertMatrixToFixed(matrix, &f_FlapMatrices[row][g_MatrixBufferIndex]);
        }
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenEffectProjection),
        G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
    );
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.leaderboardRowImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.leaderboardRowImage),
        G_IM_FMT_CI,
        ROW_IMAGE_WIDTH,
        ROW_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
            g_Scorecard.content.isRowFlipping[row] == TRUE) {
            if (g_Scorecard.content.rowFlipAngles[row] < FLAP_EDGE_ON_ANGLE) {
                bottomFlapShade = COLOR_CHANNEL_MAX;
            }
            else {
                bottomFlapShade = FLAP_LIT_SHADE -
                    (sinf(g_Scorecard.content.rowFlipAngles[row] - FLAP_EDGE_ON_ANGLE) *
                        FLAP_SHADE_RANGE);
            }

            gDPSetPrimColor(
                gfx++,
                0,
                0,
                bottomFlapShade,
                bottomFlapShade,
                bottomFlapShade,
                OPAQUE_ALPHA
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + ROW_LEFT),
                QuarterPixels(
                    (row * LEADERBOARD_ROW_SPACING) + ROW_TOP + ROW_BOTTOM_HALF_ROW + g_Scorecard.y
                ),
                QuarterPixels(g_Scorecard.x + ROW_LEFT + ROW_IMAGE_WIDTH),
                QuarterPixels(
                    (row * LEADERBOARD_ROW_SPACING) + (ROW_IMAGE_HEIGHT + ROW_TOP) + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(ROW_BOTTOM_HALF_ROW),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.flapNameImages));
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (g_Scorecard.content.isRowFlipping[row] == TRUE) {
            if (g_Scorecard.content.rowFlipAngles[row] < FLAP_EDGE_ON_ANGLE) {
                shade = COLOR_CHANNEL_MAX;
            }
            else {
                shade = FLAP_LIT_SHADE -
                    (sinf(g_Scorecard.content.rowFlipAngles[row] - FLAP_EDGE_ON_ANGLE) *
                        FLAP_SHADE_RANGE);
            }

            gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, OPAQUE_ALPHA);
            LoadTextureBlock(
                &gfx,
                g_Scorecard.flapNameImages +
                    ((row * FLAP_NAME_IMAGE_WIDTH * ROW_IMAGE_HEIGHT) +
                        LARGE_PALETTED_TEXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FLAP_NAME_IMAGE_WIDTH,
                ROW_IMAGE_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT),
                QuarterPixels(
                    (row * LEADERBOARD_ROW_SPACING) + ROW_TOP + ROW_BOTTOM_HALF_ROW + g_Scorecard.y
                ),
                QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT + FLAP_NAME_IMAGE_WIDTH),
                QuarterPixels(
                    (row * LEADERBOARD_ROW_SPACING) + ROW_TOP + ROW_IMAGE_HEIGHT + g_Scorecard.y
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(ROW_BOTTOM_HALF_ROW),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPPipeSync(gfx++);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.leaderboardRowImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.leaderboardRowImage),
        G_IM_FMT_CI,
        ROW_IMAGE_WIDTH,
        ROW_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetCombineMode(gfx++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
            g_Scorecard.content.isRowFlipping[row] == TRUE) {
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_FlapMatrices[row][g_MatrixBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
            );
            if (g_Scorecard.content.rowFlipAngles[row] > FLAP_EDGE_ON_ANGLE) {
                gSPVertex(gfx++, f_FlapVertices[FLAP_BOTTOM_QUAD], QUAD_VERTEX_COUNT, 0);
            }
            else {
                gSPVertex(gfx++, f_FlapVertices[FLAP_TOP_QUAD], QUAD_VERTEX_COUNT, 0);
            }

            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
        }
    }

    LoadLargePalette(&gfx, ImagePalette(g_Scorecard.flapNameImages));
    for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
            g_Scorecard.content.isRowFlipping[row] == TRUE &&
            g_Scorecard.content.rowFlipAngles[row] < FLAP_EDGE_ON_ANGLE) {
            LoadTextureBlock(
                &gfx,
                g_Scorecard.flapNameImages +
                    ((row * FLAP_NAME_IMAGE_WIDTH * ROW_IMAGE_HEIGHT) +
                        LARGE_PALETTED_TEXELS_OFFSET),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FLAP_NAME_IMAGE_WIDTH,
                ROW_IMAGE_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPMatrix(
                gfx++,
                PhysicalAddress(&f_FlapMatrices[row][g_MatrixBufferIndex]),
                G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
            );
            gDPPipeSync(gfx++);
            gSPVertex(gfx++, f_FlapVertices[FLAP_NAME_QUAD], QUAD_VERTEX_COUNT, 0);
            gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
            gDPPipeSync(gfx++);
        }
    }

    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    SetUpPerspectiveTextDrawing(&gfx);
    for (row = 0, group = 0; row != LEADERBOARD_ROW_COUNT; row++) {
        if (row * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime) {
            if (g_Scorecard.content.isRowFlipping[row] == TRUE &&
                g_Scorecard.content.rowFlipAngles[row] > FLAP_EDGE_ON_ANGLE) {
                standing = &g_TournamentStandings[g_Scorecard.topRow + row];
                FormatLeaderboardRow(g_PrintBuffer, standing);
                PlaceGlyphGroup(
                    group,
                    FONT_SIZE_LARGE,
                    TEXT_FLAG_FIXED_WIDTH | GLYPH_FLAG_BOTTOM_PART,
                    0,
                    0,
                    g_PrintBuffer
                );
                if (standing->contestantId == PLAYER_CONTESTANT) {
                    sprintf(g_PrintBuffer, "%-8s", g_Players[0].name);
                }
                else {
                    sprintf(
                        g_PrintBuffer,
                        "%s",
                        g_ContestantNames[g_ContestantNameIndices[standing->contestantId]]
                    );
                }

                PlaceGlyphGroup(
                    group + 1,
                    FONT_SIZE_LARGE,
                    GLYPH_FLAG_LEFT_ALIGNED | GLYPH_FLAG_BOTTOM_PART,
                    FLAP_NAME_X,
                    0,
                    g_PrintBuffer
                );
                gSPMatrix(
                    gfx++,
                    PhysicalAddress(&f_FlapMatrices[row][g_MatrixBufferIndex]),
                    G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
                );
                DrawGlyphGroup(&gfx, group);
                DrawGlyphGroup(&gfx, group + 1);
            }

            group = (group + 2) % FLAP_GLYPH_GROUP_COUNT;
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);

    if (g_Scorecard.content.flipTime > LAST_ROW_FLIP_TIME) {
        for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
            TournamentStanding *rowStanding = &g_TournamentStandings[g_Scorecard.topRow + row];

            if (rowStanding->contestantId == PLAYER_CONTESTANT) {
                gSPDisplayList(gfx++, g_DecalTextureSetup);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                DrawSmallFace(
                    &gfx,
                    g_Scorecard.x + LEADERBOARD_FACE_LEFT,
                    LEADERBOARD_FACE_TOP + (row * LEADERBOARD_ROW_SPACING),
                    0,
                    TRUE
                );
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    LoadNibbleTextureBlock(
        &gfx,
        ImageBody(g_Scorecard.placeArrowImage),
        G_IM_FMT_I,
        ARROW_IMAGE_SIZE,
        ARROW_IMAGE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_MIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        ARROW_MIRROR_MASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetCombineLERP(
        gfx++,
        ENVIRONMENT,
        PRIMITIVE,
        TEXEL0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        ENVIRONMENT,
        PRIMITIVE,
        TEXEL0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (g_Scorecard.content.flipTime > LAST_ROW_FLIP_TIME) {
        for (row = 0; row != LEADERBOARD_ROW_COUNT; row++) {
            if (g_TournamentStandings[g_Scorecard.topRow + row].contestantId == PLAYER_CONTESTANT &&
                g_Scorecard.placeChange != PLACE_HELD) {
                halfSize = FELL_ARROW_HALF_SIZE;
                if (g_Scorecard.placeChange & PLACE_FELL) {
                    gDPSetEnvColor(gfx++, 0xA0, 0xA0, 0xFF, OPAQUE_ALPHA);
                    gDPSetPrimColor(gfx++, 0, 0, 0x00, 0x40, 0xFF, OPAQUE_ALPHA);
                    gDPPipeSync(gfx++);
                    f_ArrowPulseAngle += FELL_ARROW_PULSE_STEP;
                    halfSize += ARROW_PULSE_SIZE * sinf(f_ArrowPulseAngle);
                }
                else {
                    gDPSetEnvColor(gfx++, 0xFF, 0x7F, 0x90, OPAQUE_ALPHA);
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x40, 0x40, 0xF0);
                    gDPPipeSync(gfx++);
                    f_ArrowPulseAngle += ROSE_ARROW_PULSE_STEP;
                    halfSize = (ARROW_PULSE_SIZE * sinf(f_ArrowPulseAngle)) + ROSE_ARROW_HALF_SIZE;
                }

                texelStep = (TEXEL_STEP_ONE * ARROW_IMAGE_SIZE / 2) / halfSize;
                roundedHalfSize = halfSize + 0.5f;
                texelStart = (ARROW_IMAGE_SIZE / 2) -
                    (((f32)roundedHalfSize - halfSize) * (ARROW_IMAGE_SIZE / 2));
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(g_Scorecard.x + ARROW_CENTER_X - roundedHalfSize),
                    QuarterPixels(
                        ((row * LEADERBOARD_ROW_SPACING) + ROW_HINGE_TOP + g_Scorecard.y) -
                        roundedHalfSize
                    ),
                    QuarterPixels(g_Scorecard.x + ARROW_CENTER_X + roundedHalfSize),
                    QuarterPixels(
                        ((row * LEADERBOARD_ROW_SPACING) + ROW_HINGE_TOP + g_Scorecard.y) +
                        roundedHalfSize
                    ),
                    G_TX_RENDERTILE,
                    texelStart,
                    texelStart + TexelCoordinate(g_Scorecard.placeChange * ARROW_IMAGE_SIZE),
                    texelStep,
                    texelStep
                );
            }
        }
    }

    *displayListHead = gfx;
}

#define IsMatchPlayMode() \
    (g_GameState.gameMode == GAME_MODE_MATCH_GAME || \
        g_GameState.gameMode == GAME_MODE_GET_CHARACTER || \
        g_GameState.gameMode == GAME_MODE_SKINS_MATCH || \
        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)
#define FRONT_NINE_TOP 57
#define BACK_NINE_TOP 137
#define SCORES_PANEL_IMAGE_WIDTH 32
#define SCORES_PANEL_IMAGE_HEIGHT 78
#define PANEL_TEXEL_RIGHT 7
#define PANEL_EDGE_TEXEL_LEFT 10
#define PANEL_EDGE_TEXEL_RIGHT 17
#define PANEL_EDGE_WIDTH 8
#define LABEL_PANEL_LEFT 24
#define LABEL_PANEL_RIGHT 76
#define LABEL_PANEL_TOP 54
#define LABEL_PANEL_HEIGHT 80
#define LABEL_PANEL_STEP (SCORES_PANEL_IMAGE_HEIGHT * TEXEL_STEP_ONE / LABEL_PANEL_HEIGHT)
#define TOTAL_PANEL_LEFT 244
#define TOTAL_PANEL_TOP 56
#define TOTAL_PANEL_BOTTOM 134
#define NINE_SPACING 80
#define MATCH_TOTAL_PANEL_RIGHT 288
#define FRONT_TOTAL_PANEL_RIGHT 262
#define BACK_TOTAL_PANEL_RIGHT 256
#define NET_PANEL_LEFT 264
#define NET_PANEL_RIGHT 290
#define CELL_LEFT 82
#define CELL_TOP 56
#define CELL_WIDTH 18
#define CELL_HEIGHT 13
#define CELL_ROW_COUNT 6
#define SCORE_ROW_COUNT 4
#define TOTAL_LABEL_WIDTH 32
#define TOTAL_LABEL_HEIGHT 12
#define TOTAL_LABELS_IMAGE_HEIGHT 36
#define FRONT_NINE_LABEL 0
#define BACK_NINE_LABEL 1
#define ROUND_LABEL 2
#define TOTAL_LABEL_TOP 58
#define FRONT_NINE_LABEL_LEFT 246
#define BACK_NINE_LABEL_LEFT 244
#define NET_ROUND_LABEL_LEFT 268
#define ROUND_LABEL_LEFT 256
#define CELL_SHADE 0xDC
#define SCORES_FACE_LEFT 18
#define SCORES_FACE_STAGGER 24
#define SCORES_FACE_RISE 14
#define HEADING_LEFT 48
#define PAR_ROW_TOP 12
#define PLAYER_ROW_TOP 27
#define PLAYER_ROW_SPACING 13
#define WIDE_PLAYER_ROW_SPACING 26
#define EXTRA_ROW_TOP 41
#define PUTTS_LABEL_LEFT 42
#define OVER_LABEL_LEFT 50
#define BADGES_LABEL_LEFT 34
#define HOLE_NUMBERS_LEFT 82
#define BACK_HOLE_NUMBERS_LEFT 84
#define HOLE_NUMBERS_TOP 3
#define PAR_LEFT 89
#define PAR_TOP 15
#define FRONT_PAR_TOTAL_LEFT 242
#define BACK_PAR_TOTAL_LEFT 240
#define MATCH_MARK_LEFT 88
#define MATCH_STANDING_LEFT 248
#define SKINS_MARK_LEFT 90
#define SKINS_WON_LEFT 82
#define SKINS_TOTAL_LEFT 272
#define STROKES_LEFT 84
#define PUTTS_LEFT 92
#define PUTTS_TOP 40
#define OVER_LEFT 90
#define FRONT_TOTAL_LEFT 245
#define BACK_TOTAL_LEFT 243
#define NET_SCORE_LEFT 264
#define CONDENSED_NET_SCORE_LEFT 266
#define HANDICAP_STROKE_TOP 32
#define HANDICAP_STROKE_LEFT 92
#define SECOND_HANDICAP_STROKE_LEFT 84
#define BADGE_LEFT 87
#define BADGE_TOP 66
#define NO_SCORE_CODE 0x83
#define HOLE_WON_CODE 0x82
#define HOLE_LOST_CODE 0x85
#define HANDICAP_STROKE_CODE 0x91
#define BIRDIE_BADGE_CODE 0x81
#define PAR_COLOR 0
#define BIRDIE_COLOR 1
#define DOUBLE_BOGEY_COLOR 2
#define EAGLE_COLOR 3
#define BOGEY_COLOR 4
#define PULSING_COLOR 5
#define PULSE_STEP DegreesToRadians(20)
#define PULSE_RANGE 127
#define BADGE_GLOW_STEP DegreesToRadians(10)
#define BADGE_GLOW_RANGE 30

void DrawScoresPage(Gfx **displayListHead) {
    s32 frontPar = 0;
    s32 backPar = 0;
    Gfx *gfx = *displayListHead;
    s8 handicapStrokes[SCORE_ROW_COUNT][COURSE_HOLE_COUNT];
    s32 frontTop = FRONT_NINE_TOP;
    s32 backTop = BACK_NINE_TOP;
    CourseRecord *course;
    s32 pulseRed;
    s32 pulseGreen;
    s32 pulseBlue;
    s32 cellAlpha;
    s32 firstHole = GetFirstHoleIndex();
    s32 rowSpacing;
    s32 standing;
    s32 colorIndex;
    s32 extraTop;
    s32 overTop;
    s32 secondRow;
    s32 i;
    s32 j;
    Awards *awards;
    s32 glow;
    Boolean isPuttsShown;
    Boolean isOverShown;
    Boolean isBadgesShown;

    f_ScorePulseAngle += PULSE_STEP;
    pulseRed = (s32)(sinf(f_ScorePulseAngle) * (f32)PULSE_RANGE) + PULSE_RANGE;
    pulseGreen = (s32)(sinf(f_ScorePulseAngle + M_PI) * (f32)PULSE_RANGE) + PULSE_RANGE;
    pulseBlue = (s32)(sinf(f_ScorePulseAngle + (M_PI / 2)) * (f32)PULSE_RANGE) + PULSE_RANGE;
    course = &g_CourseRecords[GetCourseRecordIndex()];

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    IN_OWN_BLOCK(;);
    switch (f_ScoresStyle) {
        case SCORES_STYLE_YELLOW:
            gDPSetEnvColor(gfx++, 0xFF, 0xF0, 0x7F, OPAQUE_ALPHA);
            gDPSetBlendColor(gfx++, 0, 0, 0, 0x80);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetCombineLERP(
                gfx++,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            cellAlpha = OPAQUE_ALPHA;
            break;
        case SCORES_STYLE_FADED:
            gDPSetEnvColor(gfx++, 0, 0x3F, 0, OPAQUE_ALPHA);
            gDPSetBlendColor(gfx++, 0, 0, 0, 0x40);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineLERP(
                gfx++,
                PRIMITIVE,
                ENVIRONMENT,
                TEXEL0,
                ENVIRONMENT,
                0,
                0,
                0,
                PRIMITIVE,
                PRIMITIVE,
                ENVIRONMENT,
                TEXEL0,
                ENVIRONMENT,
                0,
                0,
                0,
                PRIMITIVE
            );
            cellAlpha = 0x9F;
            break;
        case SCORES_STYLE_OPAQUE:
#ifdef AVOID_UB
            cellAlpha = OPAQUE_ALPHA;
#endif
            gDPSetEnvColor(gfx++, 0, 0x1F, 0, OPAQUE_ALPHA);
            gDPSetBlendColor(gfx++, 0, 0, 0, 0x40);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetCombineLERP(
                gfx++,
                PRIMITIVE,
                ENVIRONMENT,
                TEXEL0,
                ENVIRONMENT,
                0,
                0,
                0,
                TEXEL0,
                PRIMITIVE,
                ENVIRONMENT,
                TEXEL0,
                ENVIRONMENT,
                0,
                0,
                0,
                TEXEL0
            );
            break;
#ifdef AVOID_UB
        default:
#endif
        case SCORES_STYLE_TINTED:
            gDPSetEnvColor(gfx++, 0, 0x40, 0, OPAQUE_ALPHA);
            gDPSetBlendColor(gfx++, 0, 0, 0, 0x1F);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineLERP(
                gfx++,
                PRIMITIVE,
                ENVIRONMENT,
                TEXEL0,
                ENVIRONMENT,
                PRIMITIVE,
                0,
                TEXEL0,
                0,
                PRIMITIVE,
                ENVIRONMENT,
                TEXEL0,
                ENVIRONMENT,
                PRIMITIVE,
                0,
                TEXEL0,
                0
            );
            cellAlpha = OPAQUE_ALPHA;
            break;
    }

    gDPSetPrimColor(gfx++, 0, 0, 0, CELL_SHADE, 0, cellAlpha);
    LoadNibbleTextureBlock(
        &gfx,
        ImageBody(g_Scorecard.scoresPanelImage),
        G_IM_FMT_I,
        SCORES_PANEL_IMAGE_WIDTH,
        SCORES_PANEL_IMAGE_HEIGHT,
        0,
        G_TX_CLAMP,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        0,
        PANEL_TEXEL_RIGHT << G_TEXTURE_IMAGE_FRAC,
        (SCORES_PANEL_IMAGE_HEIGHT - 1) << G_TEXTURE_IMAGE_FRAC
    );
    gDPPipeSync(gfx++);
    if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP),
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + LABEL_PANEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            LABEL_PANEL_STEP
        );
    }

    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING + LABEL_PANEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            LABEL_PANEL_STEP
        );
    }

    if (IsMatchPlayMode()) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
                QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
                QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }
    else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
            QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
            QuarterPixels(g_Scorecard.x + FRONT_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + BACK_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        0,
        PANEL_EDGE_TEXEL_RIGHT << G_TEXTURE_IMAGE_FRAC,
        (SCORES_PANEL_IMAGE_HEIGHT - 1) << G_TEXTURE_IMAGE_FRAC
    );
    if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP),
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + LABEL_PANEL_HEIGHT),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            LABEL_PANEL_STEP
        );
    }

    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING + LABEL_PANEL_HEIGHT),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            LABEL_PANEL_STEP
        );
    }

    if (IsMatchPlayMode()) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
                QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
                G_TX_RENDERTILE,
                TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
                QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
                QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM),
                G_TX_RENDERTILE,
                TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }
    else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
            QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + FRONT_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
            QuarterPixels(g_Scorecard.x + FRONT_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + BACK_TOTAL_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + BACK_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    for (j = 0; j != SCORE_ROW_COUNT; j++) {
        for (i = 0; i != COURSE_HOLE_COUNT; i++) {
            handicapStrokes[j][i] = 0;
        }
    }

    if (g_GameState.playerCount == 2 && !IsMatchPlayMode()) {
        for (j = 0; j != g_GameState.playerCount; j++) {
            for (i = 0; i != COURSE_HOLE_COUNT; i++) {
                handicapStrokes[j * 2][i] =
                    GetHandicapOffset(GetCourseRecordIndex(), i, g_Players[j].handicap);
            }
        }
    }
    else {
        for (j = 0; j != g_GameState.playerCount; j++) {
            for (i = 0; i != COURSE_HOLE_COUNT; i++) {
                handicapStrokes[j][i] =
                    GetHandicapOffset(GetCourseRecordIndex(), i, g_Players[j].handicap);
            }
        }
    }

    for (j = 0; j != CELL_ROW_COUNT; j++) {
        for (i = 0; i != HOLES_PER_NINE; i++) {
            gDPPipeSync(gfx++);
            if (j & 1) {
                gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xBF, 0, cellAlpha);
            }
            else {
                gDPSetPrimColor(gfx++, 0, 0, 0, CELL_SHADE, 0, cellAlpha);
            }

            gDPPipeSync(gfx++);
            if (i & 1) {
                if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + g_Scorecard.x),
                        QuarterPixels((j * CELL_HEIGHT) + CELL_TOP + g_Scorecard.y),
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + CELL_WIDTH + g_Scorecard.x),
                        QuarterPixels((j * CELL_HEIGHT) + CELL_TOP + CELL_HEIGHT + g_Scorecard.y),
                        G_TX_RENDERTILE,
                        0,
                        TexelCoordinate(j * CELL_HEIGHT),
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                }

                if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + g_Scorecard.x),
                        QuarterPixels((j * CELL_HEIGHT) + CELL_TOP + NINE_SPACING + g_Scorecard.y),
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + CELL_WIDTH + g_Scorecard.x),
                        QuarterPixels(
                            (j * CELL_HEIGHT) + CELL_TOP + NINE_SPACING + CELL_HEIGHT +
                            g_Scorecard.y
                        ),
                        G_TX_RENDERTILE,
                        0,
                        TexelCoordinate(j * CELL_HEIGHT),
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                }
            }
        }
    }

    gDPPipeSync(gfx++);
    LoadNibbleTextureBlock(
        &gfx,
        ImageBody(g_Scorecard.scoresPanelImage),
        G_IM_FMT_I,
        SCORES_PANEL_IMAGE_WIDTH,
        SCORES_PANEL_IMAGE_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x1F, cellAlpha);
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        0,
        PANEL_TEXEL_RIGHT << G_TEXTURE_IMAGE_FRAC,
        (SCORES_PANEL_IMAGE_HEIGHT - 1) << G_TEXTURE_IMAGE_FRAC
    );
    gDPPipeSync(gfx++);
    if (IsMatchPlayMode() == FALSE &&
        !(g_GameState.holeSelection & (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + NET_PANEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + NET_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        0,
        PANEL_EDGE_TEXEL_RIGHT << G_TEXTURE_IMAGE_FRAC,
        (SCORES_PANEL_IMAGE_HEIGHT - 1) << G_TEXTURE_IMAGE_FRAC
    );
    gDPPipeSync(gfx++);
    if (IsMatchPlayMode() == FALSE &&
        !(g_GameState.holeSelection & (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + NET_PANEL_RIGHT),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + NET_PANEL_RIGHT + PANEL_EDGE_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
            G_TX_RENDERTILE,
            TexelCoordinate(PANEL_EDGE_TEXEL_LEFT),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    for (j = 0; j != CELL_ROW_COUNT; j++) {
        if (j & 1) {
            gDPSetPrimColor(gfx++, 0, 0, 0xBF, 0xFF, 0, cellAlpha);
        }
        else {
            gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0, cellAlpha);
        }

        for (i = 0; i != HOLES_PER_NINE; i++) {
            if (!(i & 1)) {
                if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + g_Scorecard.x),
                        QuarterPixels((j * CELL_HEIGHT) + CELL_TOP + g_Scorecard.y),
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + CELL_WIDTH + g_Scorecard.x),
                        QuarterPixels((j * CELL_HEIGHT) + CELL_TOP + CELL_HEIGHT + g_Scorecard.y),
                        G_TX_RENDERTILE,
                        0,
                        TexelCoordinate(j * CELL_HEIGHT),
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                }

                if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + g_Scorecard.x),
                        QuarterPixels((j * CELL_HEIGHT) + CELL_TOP + NINE_SPACING + g_Scorecard.y),
                        QuarterPixels((i * CELL_WIDTH) + CELL_LEFT + CELL_WIDTH + g_Scorecard.x),
                        QuarterPixels(
                            (j * CELL_HEIGHT) + CELL_HEIGHT + CELL_TOP + NINE_SPACING +
                            g_Scorecard.y
                        ),
                        G_TX_RENDERTILE,
                        0,
                        TexelCoordinate(j * CELL_HEIGHT),
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                }
            }
        }
    }

    gDPPipeSync(gfx++);
    gSPDisplayList(gfx++, g_DecalTextureSetup);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.totalLabelsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.totalLabelsImage),
        G_IM_FMT_CI,
        TOTAL_LABEL_WIDTH,
        TOTAL_LABELS_IMAGE_HEIGHT,
        0,
        G_TX_MIRROR,
        0,
        TextureMask(TOTAL_LABEL_WIDTH),
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (IsMatchPlayMode()) {
        if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
                QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP),
                QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
                QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + TOTAL_LABEL_HEIGHT),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
        else {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
                QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
                QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
                QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING + TOTAL_LABEL_HEIGHT),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }
    else if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP),
            QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + TOTAL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING + TOTAL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }
    else {
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + FRONT_NINE_LABEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP),
            QuarterPixels(g_Scorecard.x + FRONT_NINE_LABEL_LEFT + TOTAL_LABEL_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + TOTAL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(FRONT_NINE_LABEL * TOTAL_LABEL_HEIGHT),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + BACK_NINE_LABEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + BACK_NINE_LABEL_LEFT + TOTAL_LABEL_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING + TOTAL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(BACK_NINE_LABEL * TOTAL_LABEL_HEIGHT),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(g_Scorecard.x + NET_ROUND_LABEL_LEFT),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
            QuarterPixels(g_Scorecard.x + NET_ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
            QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING + TOTAL_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    switch (g_GameState.playerCount) {
        case 2:
            if (IsMatchPlayMode()) {
                rowSpacing = PLAYER_ROW_SPACING;
                secondRow = 1;
            }
            else {
                secondRow = 2;
                rowSpacing = WIDE_PLAYER_ROW_SPACING;
            }

            if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
                DrawSmallFace(
                    &gfx,
                    g_Scorecard.x + SCORES_FACE_LEFT,
                    frontTop + PLAYER_ROW_TOP + g_Scorecard.y - SCORES_FACE_RISE,
                    0,
                    TRUE
                );
                DrawSmallFace(
                    &gfx,
                    ((secondRow & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT + g_Scorecard.x,
                    frontTop + PLAYER_ROW_TOP + (secondRow * PLAYER_ROW_SPACING) + g_Scorecard.y -
                        SCORES_FACE_RISE,
                    1,
                    TRUE
                );
            }

            if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
                DrawSmallFace(
                    &gfx,
                    g_Scorecard.x + SCORES_FACE_LEFT,
                    backTop + PLAYER_ROW_TOP + g_Scorecard.y - SCORES_FACE_RISE,
                    0,
                    TRUE
                );
                DrawSmallFace(
                    &gfx,
                    ((secondRow & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT + g_Scorecard.x,
                    backTop + PLAYER_ROW_TOP + (secondRow * PLAYER_ROW_SPACING) + g_Scorecard.y -
                        SCORES_FACE_RISE,
                    1,
                    TRUE
                );
            }

            break;
#ifdef AVOID_UB
        default:
#endif
        case 1:
        case 3:
        case 4:
            rowSpacing = PLAYER_ROW_SPACING;
            for (i = 0; i != g_GameState.playerCount; i++) {
                LoadLargePalette(&gfx, g_PlayerFacePalettes[i]);
                LoadTextureBlock(
                    &gfx,
                    g_PlayerFaceImages[i],
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    FACE_WIDTH,
                    FACE_HEIGHT,
                    0,
                    G_TX_MIRROR,
                    0,
                    MIRRORED_FACE_MASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
                    DrawSmallFace(
                        &gfx,
                        ((i & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT + g_Scorecard.x,
                        frontTop + PLAYER_ROW_TOP + (i * PLAYER_ROW_SPACING) + g_Scorecard.y -
                            SCORES_FACE_RISE,
                        i,
                        TRUE
                    );
                }

                if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
                    DrawSmallFace(
                        &gfx,
                        ((i & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT + g_Scorecard.x,
                        backTop + PLAYER_ROW_TOP + (i * PLAYER_ROW_SPACING) + g_Scorecard.y -
                            SCORES_FACE_RISE,
                        i,
                        TRUE
                    );
                }
            }

            break;
    }

    SetTextColor(&gfx, 0xBF, 0xFF, 0x7F);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xBF, 0);
    if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
        DrawLargeText(&gfx, g_Scorecard.x + HEADING_LEFT, frontTop + g_Scorecard.y, "NO.");
        DrawLargeText(
            &gfx,
            g_Scorecard.x + HEADING_LEFT,
            frontTop + PAR_ROW_TOP + g_Scorecard.y,
            "PAR"
        );
    }

    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
        DrawLargeText(&gfx, g_Scorecard.x + HEADING_LEFT, backTop + g_Scorecard.y, "NO.");
        DrawLargeText(
            &gfx,
            g_Scorecard.x + HEADING_LEFT,
            backTop + PAR_ROW_TOP + g_Scorecard.y,
            "PAR"
        );
    }

    isBadgesShown = FALSE;
    isOverShown = FALSE;
    isPuttsShown = FALSE;
    if (g_GameState.playerCount == 1) {
        isOverShown = TRUE;
        isBadgesShown = g_GameState.gameMode == GAME_MODE_TOURNAMENT;
        isPuttsShown = !g_GameState.isMiniGolf;
    }

    if (g_GameState.playerCount == 2 && IsMatchPlayMode() == FALSE) {
        if (g_GameState.isMiniGolf) {
            isOverShown = TRUE;
        }
        else {
            isPuttsShown = TRUE;
        }
    }

    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != g_GameState.playerCount; i++) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
            extraTop = EXTRA_ROW_TOP;
            if (isPuttsShown) {
                extraTop += PLAYER_ROW_SPACING;
                DrawSmallText(
                    &gfx,
                    g_Scorecard.x + PUTTS_LABEL_LEFT,
                    frontTop + (EXTRA_ROW_TOP + (i * rowSpacing)) + g_Scorecard.y,
                    "Putts"
                );
            }

            if (isOverShown) {
                s32 labelTop = frontTop + extraTop;

                extraTop += PLAYER_ROW_SPACING;
                DrawSmallText(
                    &gfx,
                    g_Scorecard.x + OVER_LABEL_LEFT,
                    labelTop + (i * rowSpacing) + g_Scorecard.y,
                    "Over"
                );
            }

            if (isBadgesShown) {
                DrawSmallText(
                    &gfx,
                    g_Scorecard.x + BADGES_LABEL_LEFT,
                    frontTop + extraTop + (i * rowSpacing) + g_Scorecard.y,
                    "Badges"
                );
            }
        }

        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
            extraTop = EXTRA_ROW_TOP;
            if (isPuttsShown) {
                extraTop += PLAYER_ROW_SPACING;
                DrawSmallText(
                    &gfx,
                    g_Scorecard.x + PUTTS_LABEL_LEFT,
                    backTop + (EXTRA_ROW_TOP + (i * rowSpacing)) + g_Scorecard.y,
                    "Putts"
                );
            }

            if (isOverShown) {
                s32 labelTop = backTop + extraTop;

                extraTop += PLAYER_ROW_SPACING;
                DrawSmallText(
                    &gfx,
                    g_Scorecard.x + OVER_LABEL_LEFT,
                    labelTop + (i * rowSpacing) + g_Scorecard.y,
                    "Over"
                );
            }

            if (isBadgesShown) {
                DrawSmallText(
                    &gfx,
                    g_Scorecard.x + BADGES_LABEL_LEFT,
                    backTop + extraTop + (i * rowSpacing) + g_Scorecard.y,
                    "Badges"
                );
            }
        }
    }

    SetUpShadowedTextDrawing(&gfx);
    SetTextColor(&gfx, 0xFF, 0xFF, 0);
    for (i = 0; i != HOLES_PER_NINE; i++) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
            sprintf(g_PrintBuffer, "%2d", i + 1);
            DrawCondensedShadowedText(
                &gfx,
                (i * CELL_WIDTH) + HOLE_NUMBERS_LEFT + g_Scorecard.x,
                frontTop + HOLE_NUMBERS_TOP + g_Scorecard.y,
                g_PrintBuffer
            );
        }

        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
            sprintf(g_PrintBuffer, "%2d", i + HOLES_PER_NINE + 1);
            DrawCondensedShadowedText(
                &gfx,
                (i * CELL_WIDTH) + BACK_HOLE_NUMBERS_LEFT + g_Scorecard.x,
                backTop + HOLE_NUMBERS_TOP + g_Scorecard.y,
                g_PrintBuffer
            );
        }
    }

    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != HOLES_PER_NINE; i++) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
            frontPar += course->holes[i].par;
            sprintf(g_PrintBuffer, "%d", course->holes[i].par);
            DrawCondensedShadowedText(
                &gfx,
                (i * CELL_WIDTH) + PAR_LEFT + g_Scorecard.x,
                frontTop + PAR_TOP + g_Scorecard.y,
                g_PrintBuffer
            );
        }

        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
            backPar += course->holes[i + HOLES_PER_NINE].par;
            sprintf(g_PrintBuffer, "%d", course->holes[i + HOLES_PER_NINE].par);
            DrawCondensedShadowedText(
                &gfx,
                (i * CELL_WIDTH) + PAR_LEFT + g_Scorecard.x,
                backTop + PAR_TOP + g_Scorecard.y,
                g_PrintBuffer
            );
        }
    }

    if (IsMatchPlayMode() == FALSE &&
        !(g_GameState.holeSelection & (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
        sprintf(g_PrintBuffer, "%3d", frontPar);
        DrawShadowedText(
            &gfx,
            g_Scorecard.x + FRONT_PAR_TOTAL_LEFT,
            frontTop + PAR_TOP + g_Scorecard.y,
            g_PrintBuffer
        );
        sprintf(g_PrintBuffer, "%3d", backPar);
        DrawShadowedText(
            &gfx,
            g_Scorecard.x + BACK_PAR_TOTAL_LEFT,
            backTop + PAR_TOP + g_Scorecard.y,
            g_PrintBuffer
        );
    }

    SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
    for (i = 0; i != g_GameState.playerCount; i++) {
        s32 shownHoleCount;
        s32 backTotal;
        s32 frontTotal;

        shownHoleCount = g_Scorecard.content.holeIndex;
        standing = 0;
        colorIndex = PAR_COLOR;
        backTotal = 0;
        frontTotal = 0;
        if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
            if (g_Players[i].holeStatus != HOLE_STATUS_PLAYING) {
                shownHoleCount = g_Scorecard.content.holeIndex +
                    (g_Players[i ^ 1].holeStatus != HOLE_STATUS_PLAYING);
            }

            SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
            for (j = firstHole; j != shownHoleCount; j++) {
                if (g_Players[i].holes[j].strokeCount == g_Players[i ^ 1].holes[j].strokeCount) {
                    g_PrintBuffer[0] = NO_SCORE_CODE;
                }
                else if (g_Players[i].holes[j].strokeCount <
                    g_Players[i ^ 1].holes[j].strokeCount) {
                    g_PrintBuffer[0] = HOLE_WON_CODE;
                    standing++;
                }
                else {
                    g_PrintBuffer[0] = HOLE_LOST_CODE;
                    standing--;
                }

                sprintf(g_PrintBuffer + 1, "");
                if (j < HOLES_PER_NINE) {
                    DrawShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + MATCH_MARK_LEFT + g_Scorecard.x,
                        frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
                else {
                    DrawShadowedText(
                        &gfx,
                        ((j - HOLES_PER_NINE) * CELL_WIDTH) + MATCH_MARK_LEFT + g_Scorecard.x,
                        backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
            }

            if (standing == 0) {
                sprintf(g_PrintBuffer, " EVEN");
            }
            else if (standing > 0) {
                sprintf(g_PrintBuffer, "%2dUP", standing);
            }
            else {
                sprintf(g_PrintBuffer, "%2dDOWN", -standing);
            }

            if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
                DrawShadowedText(
                    &gfx,
                    g_Scorecard.x + MATCH_STANDING_LEFT,
                    frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
            else {
                DrawShadowedText(
                    &gfx,
                    g_Scorecard.x + MATCH_STANDING_LEFT,
                    backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
        }
        else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
            g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
            SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
            for (j = firstHole; j != shownHoleCount; j++) {
                if (g_Players[i].holes[j].strokeCount == 0) {
                    sprintf(g_PrintBuffer, "%c", NO_SCORE_CODE);
                    if (j < HOLES_PER_NINE) {
                        DrawShadowedText(
                            &gfx,
                            (j * CELL_WIDTH) + SKINS_MARK_LEFT + g_Scorecard.x,
                            frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                            g_PrintBuffer
                        );
                    }
                    else {
                        DrawShadowedText(
                            &gfx,
                            ((j - HOLES_PER_NINE) * CELL_WIDTH) + SKINS_MARK_LEFT + g_Scorecard.x,
                            backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                            g_PrintBuffer
                        );
                    }
                }
                else {
                    sprintf(g_PrintBuffer, "%2d", g_Players[i].holes[j].strokeCount);
                    if (j < HOLES_PER_NINE) {
                        DrawShadowedText(
                            &gfx,
                            (j * CELL_WIDTH) + SKINS_WON_LEFT + g_Scorecard.x,
                            frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                            g_PrintBuffer
                        );
                    }
                    else {
                        DrawShadowedText(
                            &gfx,
                            ((j - HOLES_PER_NINE) * CELL_WIDTH) + SKINS_WON_LEFT + g_Scorecard.x,
                            backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                            g_PrintBuffer
                        );
                    }

                    standing += g_Players[i].holes[j].strokeCount;
                }
            }

            sprintf(g_PrintBuffer, "%2dP", standing);
            if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
                DrawShadowedText(
                    &gfx,
                    g_Scorecard.x + SKINS_TOTAL_LEFT,
                    frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
            else {
                DrawShadowedText(
                    &gfx,
                    g_Scorecard.x + SKINS_TOTAL_LEFT,
                    backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
        }
        else {
            shownHoleCount += g_Players[i].holeStatus != HOLE_STATUS_PLAYING;
            for (j = firstHole; j != shownHoleCount; j++) {
                standing = g_Players[i].holes[j].strokeCount - course->holes[j].par;
                if (j < HOLES_PER_NINE) {
                    frontTotal += g_Players[i].holes[j].strokeCount;
                }
                else {
                    backTotal += g_Players[i].holes[j].strokeCount;
                }

                if ((standing < -2 && course->holes[j].par == PAR_FIVE) ||
                    (standing < -1 && course->holes[j].par != PAR_FIVE)) {
                    if (colorIndex != PULSING_COLOR) {
                        colorIndex = PULSING_COLOR;
                        SetTextColor(&gfx, pulseRed, pulseGreen, pulseBlue);
                    }
                }
                else if (standing < -1) {
                    if (colorIndex != EAGLE_COLOR) {
                        colorIndex = EAGLE_COLOR;
                        SetTextColor(&gfx, 0xFF, 0, 0);
                    }
                }
                else if (standing < 0) {
                    if (colorIndex != BIRDIE_COLOR) {
                        colorIndex = BIRDIE_COLOR;
                        SetTextColor(&gfx, 0xFF, 0x7F, 0x1F);
                    }
                }
                else if (standing == 0) {
                    if (colorIndex != PAR_COLOR) {
                        colorIndex = PAR_COLOR;
                        SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
                    }
                }
                else if (standing >= 2) {
                    if (colorIndex != DOUBLE_BOGEY_COLOR) {
                        colorIndex = DOUBLE_BOGEY_COLOR;
                        SetTextColor(&gfx, 0x9F, 0xBF, 0xFF);
                    }
                }
                else if (standing > 0) {
                    if (colorIndex != BOGEY_COLOR) {
                        colorIndex = BOGEY_COLOR;
                        SetTextColor(&gfx, 0x7F, 0xFF, 0x7F);
                    }
                }

                sprintf(g_PrintBuffer, "%2d", g_Players[i].holes[j].strokeCount);
                if (j < HOLES_PER_NINE) {
                    DrawCondensedShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + STROKES_LEFT + g_Scorecard.x,
                        frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
                else {
                    DrawCondensedShadowedText(
                        &gfx,
                        ((j - HOLES_PER_NINE) * CELL_WIDTH) + STROKES_LEFT + g_Scorecard.x,
                        backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }

                overTop = PUTTS_TOP;
                if (isPuttsShown) {
                    if (g_Players[i].holes[j].puttCount == UNCOUNTED_PUTTS) {
                        sprintf(g_PrintBuffer, "%c", NO_SCORE_CODE);
                        overTop += PLAYER_ROW_SPACING;
                        if (j < HOLES_PER_NINE) {
                            DrawShadowedText(
                                &gfx,
                                (j * CELL_WIDTH) + PUTTS_LEFT + g_Scorecard.x,
                                frontTop + (PUTTS_TOP + (i * rowSpacing)) + g_Scorecard.y,
                                g_PrintBuffer
                            );
                        }
                        else {
                            DrawShadowedText(
                                &gfx,
                                ((j - HOLES_PER_NINE) * CELL_WIDTH) + PUTTS_LEFT + g_Scorecard.x,
                                backTop + (PUTTS_TOP + (i * rowSpacing)) + g_Scorecard.y,
                                g_PrintBuffer
                            );
                        }
                    }
                    else {
                        sprintf(g_PrintBuffer, "%2d", g_Players[i].holes[j].puttCount);
                        if (j < HOLES_PER_NINE) {
                            overTop += PLAYER_ROW_SPACING;
                            DrawCenteredNarrowText(
                                &gfx,
                                (j * CELL_WIDTH) + PUTTS_LEFT + g_Scorecard.x,
                                frontTop + (PUTTS_TOP + (i * rowSpacing)) + g_Scorecard.y,
                                g_PrintBuffer
                            );
                        }
                        else {
                            overTop += PLAYER_ROW_SPACING;
                            DrawCenteredNarrowText(
                                &gfx,
                                ((j - HOLES_PER_NINE) * CELL_WIDTH) + PUTTS_LEFT + g_Scorecard.x,
                                backTop + (PUTTS_TOP + (i * rowSpacing)) + g_Scorecard.y,
                                g_PrintBuffer
                            );
                        }
                    }
                }

                if (isOverShown) {
                    if (standing == 0) {
                        sprintf(g_PrintBuffer, "<0");
                    }
                    else if (standing > 0) {
                        sprintf(g_PrintBuffer, ":%d", standing);
                    }
                    else {
                        sprintf(g_PrintBuffer, ";%d", -standing);
                    }

                    if (j < HOLES_PER_NINE) {
                        DrawCenteredNarrowText(
                            &gfx,
                            (j * CELL_WIDTH) + OVER_LEFT + g_Scorecard.x,
                            frontTop + overTop + (i * rowSpacing) + g_Scorecard.y,
                            g_PrintBuffer
                        );
                    }
                    else {
                        DrawCenteredNarrowText(
                            &gfx,
                            ((j - HOLES_PER_NINE) * CELL_WIDTH) + OVER_LEFT + g_Scorecard.x,
                            backTop + overTop + (i * rowSpacing) + g_Scorecard.y,
                            g_PrintBuffer
                        );
                    }
                }
            }

            if (colorIndex != PAR_COLOR) {
                SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
            }

            if (!(g_GameState.holeSelection &
                    (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
                if (shownHoleCount > 0) {
                    sprintf(g_PrintBuffer, "%3d", frontTotal);
                    DrawCondensedShadowedText(
                        &gfx,
                        g_Scorecard.x + FRONT_TOTAL_LEFT,
                        frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }

                if (shownHoleCount > HOLES_PER_NINE) {
                    sprintf(g_PrintBuffer, "%3d", backTotal);
                    DrawCondensedShadowedText(
                        &gfx,
                        g_Scorecard.x + BACK_TOTAL_LEFT,
                        backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
            }

            standing = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
            if (standing == 0) {
                sprintf(g_PrintBuffer, "  #0");
            }
            else {
                sprintf(g_PrintBuffer, "%+4d", standing);
            }

            if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
                DrawShadowedText(
                    &gfx,
                    g_Scorecard.x + NET_SCORE_LEFT,
                    frontTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
            else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
                DrawShadowedText(
                    &gfx,
                    g_Scorecard.x + NET_SCORE_LEFT,
                    backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
            else {
                DrawCondensedShadowedText(
                    &gfx,
                    g_Scorecard.x + CONDENSED_NET_SCORE_LEFT,
                    backTop + PLAYER_ROW_TOP + (i * rowSpacing) + g_Scorecard.y,
                    g_PrintBuffer
                );
            }
        }
    }

    sprintf(g_PrintBuffer, "%c", HANDICAP_STROKE_CODE);
    for (i = 0; i != SCORE_ROW_COUNT; i++) {
        for (j = 0; j != HOLES_PER_NINE; j++) {
            if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
                if (handicapStrokes[i][j] < 0) {
                    DrawShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + HANDICAP_STROKE_LEFT + g_Scorecard.x,
                        frontTop + HANDICAP_STROKE_TOP + (i * PLAYER_ROW_SPACING) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }

                if (handicapStrokes[i][j] < -1) {
                    DrawShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + SECOND_HANDICAP_STROKE_LEFT + g_Scorecard.x,
                        frontTop + HANDICAP_STROKE_TOP + (i * PLAYER_ROW_SPACING) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
            }

            if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
                if (handicapStrokes[i][j + HOLES_PER_NINE] < 0) {
                    DrawShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + HANDICAP_STROKE_LEFT + g_Scorecard.x,
                        backTop + HANDICAP_STROKE_TOP + (i * PLAYER_ROW_SPACING) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }

                if (handicapStrokes[i][j + HOLES_PER_NINE] < -1) {
                    DrawShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + SECOND_HANDICAP_STROKE_LEFT + g_Scorecard.x,
                        backTop + HANDICAP_STROKE_TOP + (i * PLAYER_ROW_SPACING) + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
            }
        }
    }

    if (isBadgesShown) {
        awards = &GetSaveData()->records.awards;
        f_BadgeGlowAngle += BADGE_GLOW_STEP;
        glow = sinf(f_BadgeGlowAngle) * (f32)BADGE_GLOW_RANGE;
        gDPSetPrimColor(gfx++, 0, 0, glow + 0xE0, glow + 0xC4, 0x1E, OPAQUE_ALPHA);
        for (j = 0; j != COURSE_HOLE_COUNT; j++) {
            if (awards->birdieBadgeCounts[g_Scorecard.content.courseIndex][j] > 0) {
                sprintf(g_PrintBuffer, "%c", BIRDIE_BADGE_CODE);
                if (j < HOLES_PER_NINE) {
                    DrawShadowedText(
                        &gfx,
                        (j * CELL_WIDTH) + BADGE_LEFT + g_Scorecard.x,
                        frontTop + BADGE_TOP + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
                else {
                    DrawShadowedText(
                        &gfx,
                        ((j - HOLES_PER_NINE) * CELL_WIDTH) + BADGE_LEFT + g_Scorecard.x,
                        backTop + BADGE_TOP + g_Scorecard.y,
                        g_PrintBuffer
                    );
                }
            }
        }
    }

    *displayListHead = gfx;
}

void DrawBetTallies(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 playerIndex;
    s32 betIndex;
    s32 total;
    s32 column;
    s32 digit;

    LoadSmallPalette(&gfx, 0, ImagePalette(g_Scorecard.digitsImage));
    LoadNibbleTextureBlock(
        &gfx,
        ImageTexels(g_Scorecard.digitsImage),
        G_IM_FMT_CI,
        DIGIT_GLYPH_SIZE,
        DIGIT_FONT_HEIGHT,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        total = 0;
        for (betIndex = 0, column = BET_TALLY_LEFT; betIndex != BET_COUNT; betIndex++) {
            total += g_BetTallies[playerIndex][betIndex];
            if (g_GameState.betRates[playerIndex] != 0) {
                if (g_GameState.betRates[playerIndex] != UNSET_BET_RATE) {
                    for (digit = 0; g_PrintBuffer[digit] != '\0'; digit++) {
                        s32 y = playerIndex * BET_TALLY_ROW_SPACING;
                        s32 x = column + (digit * DIGIT_GLYPH_SIZE);

                        y += BET_TALLY_TOP;
                        DrawTextureRectangle(
                            &gfx,
                            QuarterPixels(x + g_Scorecard.x),
                            QuarterPixels(y),
                            QuarterPixels(x + g_Scorecard.x + DIGIT_GLYPH_SIZE),
                            QuarterPixels(y + DIGIT_GLYPH_SIZE),
                            G_TX_RENDERTILE,
                            0,
                            TexelCoordinate(GetDigitGlyphTop(g_PrintBuffer[digit])),
                            TEXEL_STEP_ONE,
                            TEXEL_STEP_ONE
                        );
                    }
                }
            }

            column += BET_TALLY_COLUMN_SPACING;
        }

        sprintf(g_PrintBuffer, "%d", total);
        for (digit = 0; g_PrintBuffer[digit] != '\0'; digit++) {
            s32 y = playerIndex * BET_TALLY_ROW_SPACING;
            s32 x = BET_TOTAL_LEFT + (digit * DIGIT_GLYPH_SIZE);

            y += BET_TALLY_TOP;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(x + g_Scorecard.x),
                QuarterPixels(y),
                QuarterPixels(x + g_Scorecard.x + DIGIT_GLYPH_SIZE),
                QuarterPixels(y + DIGIT_GLYPH_SIZE),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GetDigitGlyphTop(g_PrintBuffer[digit])),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    *displayListHead = gfx;
}
