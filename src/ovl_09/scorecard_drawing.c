/*
 * Draws the scorecard's pages: the standings, the match, the skins, the bets,
 * Speed Golf, the leaderboard and the scores.
 */
#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "character_slot.h"
#include "course.h"
#include "cpu_projection.h"
#include "fireworks.h"
#include "game.h"
#include "hole_loader.h"
#include "math_util.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scorecard.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "tournament.h"

#define FACE_SIZE 32

/*
 * placesImage holds the left halves of the four places' images, then their
 * right halves, each half PLACE_HALF_WIDTH by PLACE_HEIGHT. The right half
 * overlaps the left one.
 */
#define PLACE_IMAGE_COUNT 4
#define PLACE_HALF_WIDTH 32
#define PLACE_HEIGHT 32
#define PLACE_HALF_SIZE (PLACE_HALF_WIDTH * PLACE_HEIGHT)
#define PLACE_RIGHT_HALF_X 24
#define PLACE_RIGHT_HALVES_OFFSET \
  (LARGE_PALETTED_TEXELS_OFFSET + (PLACE_IMAGE_COUNT * PLACE_HALF_SIZE))

/* labelsImage holds four-bit labels of this size, one after another */
#define LABEL_WIDTH 80
#define LABEL_HEIGHT 18
#define LABEL_SIZE (LABEL_WIDTH * LABEL_HEIGHT / 2)
#define LABEL_SHOWN_WIDTH 54

/* Where DrawBetTallies draws the players' rows, from the scorecard's left */
#define BET_TALLY_LEFT 64
#define BET_TALLY_TOP 96
#define BET_TALLY_COLUMN_SPACING 34
#define BET_TALLY_ROW_SPACING 36
#define BET_TOTAL_LEFT 302

/* Where a leaderboard row's columns start in its text */
#define LEADERBOARD_SCORE_COLUMN 12
#define LEADERBOARD_HOLES_COLUMN 18
#define SMALL_FACE_HEIGHT 24
/* With G_TX_MIRROR, texels from this column on mirror the face */
#define MIRRORED_FACE_MASK 5

#define HOLE_NUMBER_ENV_COLOR 0xFFFF0000
#define HOLE_NUMBER_PRIM_COLOR 0x30FF0000
/* The colors of the headings that some pages draw beside their columns */
#define HEADING_ENV_COLOR 0xFF730000
#define HEADING_PRIM_COLOR 0xFFFF1C00
/*
 * The standings page lists each player's place, face, score to par and,
 * when there are bets, bet total. Without bets its columns shift right.
 */
#define STANDINGS_HOLE_LEFT 40
#define STANDINGS_HOLE_TOP 48
/* Drawn over the score column */
#define SCORE_LABEL 2
/* Where the bets page draws each player's tallies, from the scorecard's left
 * and top */
#define BETS_PAGE_TALLY_LEFT 53
#define BETS_PAGE_TALLY_TOP 112

#define NO_BET_SUBTITLE (-1)
/* The rows of the subtitles image */
#define BET_SUBTITLE_SCORE 0
#define BET_SUBTITLE_SKINS 1
#define BET_SUBTITLE_NASSAU 2

/* The quads of f_FlapVertices */
#define FLAP_TOP_QUAD 0
/* The top flap's far side, once it has turned down */
#define FLAP_BOTTOM_QUAD 1
#define FLAP_NAME_QUAD 2
#define FLAP_QUAD_COUNT 3
#define QUAD_VERTEX_COUNT 4

/* The page's four looks. No code changes f_ScoresStyle, so the page keeps
 * SCORES_STYLE_OPAQUE. */
enum ScoresStyle {
  SCORES_STYLE_YELLOW,
  SCORES_STYLE_FADED,
  SCORES_STYLE_OPAQUE,
  SCORES_STYLE_TINTED
};

/* "Hole %d" in half-width katakana */
extern const char f_HoleNumberFormat[];
/* Formats that more than one page shares */
extern const char f_StringFormat[];
extern const char f_CompactHoleFormat[];
extern const char f_TwoDigitFormat[];
extern const char f_ZeroToParFormat[];
extern const char f_ScoreToParFormat[];
extern const char f_WideTotalFormat[];
extern const char f_TotalFormat[];
/* Each player's result in a match. func_802460F8 shows f_WinText too. */
extern const char f_DrawText[];
extern const char f_WinText[];
extern const char f_LoseText[];
/* " ", still in the assembly's rodata */
extern char f_BlankBetName[];

static s32 f_MatchResultFrameCount = 0;
static u8 f_WinMarkFrames[] = {3, 4, 5, 0, 1, 2, 3, 4, 5, 0};
static char f_CarryOverText[] = "Carry over";
/* The angle the colors of "Win" are at */
static Radians f_WinColorAngle = 0.0f;
static char* f_BetNames[BET_COUNT] = {
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
    f_BlankBetName,
};
/* Read by no routine; the start of f_CarryOverText */
static char f_UnusedCarryText[] UNUSED = "Carry";
/* The headings blink between their two images, a frame each this many draws */
static s32 f_BetBlinkFrame = 0;
static s32 f_BetBlinkTime = 0;
static s8 f_BetSubtitleRows[BET_COUNT] = {
    NO_BET_SUBTITLE,
    NO_BET_SUBTITLE,
    BET_SUBTITLE_SCORE,
    BET_SUBTITLE_NASSAU,
    BET_SUBTITLE_SKINS,
    NO_BET_SUBTITLE,
    NO_BET_SUBTITLE,
};
/* The bottom quad faces away, its normal's Z of 0x81 being -127 */
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
static Lights1 f_FlapLights =
    gdSPDefLights1(0xFF, 0xFF, 0xFF, 0x1F, 0x1F, 0x1F, 0, 0, -127);
static Radians f_ArrowPulseAngle = 0.0f;
static Radians f_ScorePulseAngle = 0.0f;
static s32 f_ScoresStyle = SCORES_STYLE_OPAQUE;
static Radians f_BadgeGlowAngle = 0.0f;

/* Where DrawMatchResult draws its text */
#define MATCH_TITLE_X 32
#define MATCH_TITLE_Y 28
#define MATCH_HOLE_X 136
#define MATCH_HOLE_Y 48
#define MATCH_RESULT_LEFT 64
#define MATCH_RESULT_SPACING 128
#define MATCH_RESULT_Y 64
/* And where it draws the characters */
#define RESULT_CHARACTER_LEFT (-56.0f)
#define RESULT_CHARACTER_SPACING 112.0f
#define RESULT_CHARACTER_Y (-80.0f)
#define RESULT_CHARACTER_Z (-500.0f)
#define RESULT_CHARACTER_SCALE 5.0f
extern const char f_DecimalFormat[];

void DrawScorecardFace(
    Gfx** displayListHead, s32 left, s32 top, s32 playerIndex) {
  Gfx* gfx = *displayListHead;

  LoadLargePalette(&gfx, g_Scorecard.facePalettes[playerIndex]);
  LoadTextureBlock(&gfx, g_Scorecard.faceImages[playerIndex], G_IM_FMT_CI,
      G_IM_SIZ_8b, FACE_SIZE, FACE_SIZE, 0, 0, 0, 0, 0, 0, 0);
  DrawTextureRect(&gfx, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + FACE_SIZE), QuarterPixels(top + FACE_SIZE),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

void DrawSmallFace(Gfx** displayListHead, s32 left, s32 top, s32 playerIndex,
    Boolean isMirrored) {
  Gfx* gfx = *displayListHead;

  LoadLargePalette(&gfx, g_PlayerFacePalettes[playerIndex]);
  LoadTextureBlock(&gfx, g_PlayerFaceImages[playerIndex], G_IM_FMT_CI,
      G_IM_SIZ_8b, FACE_SIZE, SMALL_FACE_HEIGHT, 0, G_TX_MIRROR, G_TX_MIRROR,
      isMirrored * MIRRORED_FACE_MASK, 0, 0, 0);
  /* Mirrored faces start one face width in, where the mirrored tile reads the
   * face backwards */
  DrawTextureRect(&gfx, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + FACE_SIZE), QuarterPixels(top + SMALL_FACE_HEIGHT),
      G_TX_RENDERTILE, isMirrored * TexelCoordinate(FACE_SIZE), 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

static Mtx f_ResultCharacterMatrices[MATCH_PLAYER_COUNT];
/* A turning flap's matrix, one for each draw buffer */
static Mtx f_FlapMatrices[LEADERBOARD_ROW_COUNT][MATRIX_BUFFER_COUNT];

/* Draws a place, from 0 for first, as the left and right halves of its image */
void DrawPlace(Gfx** displayListHead, s32 left, s32 top, s32 place) {
  Gfx* gfx = *displayListHead;

  LoadLargePalette(&gfx, ImageBody(g_Scorecard.placesImage));
  LoadTextureBlock(&gfx,
      g_Scorecard.placesImage +
          ((place * PLACE_HALF_SIZE) + PLACE_RIGHT_HALVES_OFFSET),
      G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_HALF_WIDTH, PLACE_HEIGHT, 0, 0, 0, 0, 0,
      0, 0);
  DrawTextureRect(&gfx, QuarterPixels(left + PLACE_RIGHT_HALF_X),
      QuarterPixels(top),
      QuarterPixels(left + PLACE_RIGHT_HALF_X + PLACE_HALF_WIDTH),
      QuarterPixels(top + PLACE_HEIGHT), G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  LoadTextureBlock(&gfx,
      LargeImageTexelsAt(g_Scorecard.placesImage, place * PLACE_HALF_SIZE),
      G_IM_FMT_CI, G_IM_SIZ_8b, PLACE_HALF_WIDTH, PLACE_HEIGHT, 0, 0, 0, 0, 0,
      0, 0);
  DrawTextureRect(&gfx, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + PLACE_HALF_WIDTH), QuarterPixels(top + PLACE_HEIGHT),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

/* Draws one of the labels in labelsImage, which shows only its left
 * LABEL_SHOWN_WIDTH texels */
void DrawScorecardLabel(
    Gfx** displayListHead, s32 left, s32 top, s32 labelIndex) {
  Gfx* gfx = *displayListHead;

  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.labelsImage));
  gDPLoadTextureTile_4b(gfx++,
      TextureAddress(
          ImageTexels(g_Scorecard.labelsImage) + labelIndex * LABEL_SIZE),
      G_IM_FMT_CI, LABEL_WIDTH, LABEL_HEIGHT, 0, 0, LABEL_WIDTH - 1,
      LABEL_HEIGHT - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  DrawTextureRect(&gfx, QuarterPixels(left), QuarterPixels(top),
      QuarterPixels(left + LABEL_SHOWN_WIDTH),
      QuarterPixels(top + LABEL_HEIGHT), G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE,
      TEXEL_STEP_ONE);
  *displayListHead = gfx;
}

/*
 * Shows who won the last hole of a match game, above the two players'
 * characters, once LoadResultCharacters has loaded them.
 */
void DrawMatchResult(Gfx** displayListHead) {
  s32 holeNumber = g_Scorecard.content.holeIndex;
  s32 i;
  s32 x;
  s32 holeIndex;
  s32 ownHole; /* holeIndex again, for the player's own record, to match */
  Gfx* gfx = *displayListHead;

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  DrawLargeText(&gfx, MATCH_TITLE_X, MATCH_TITLE_Y, "Match Game");
  sprintf(g_PrintBuffer, f_HoleNumberFormat, holeNumber);
  DrawLargeText(&gfx, MATCH_HOLE_X, MATCH_HOLE_Y, g_PrintBuffer);
  holeIndex = holeNumber - 1;
  ownHole = holeIndex;
  for (i = 0, x = MATCH_RESULT_LEFT; i != MATCH_PLAYER_COUNT; i++) {
    if (g_Players[i].holes[ownHole].strokes ==
        g_Players[i ^ 1].holes[holeIndex].strokes) {
      SetTextColor(&gfx, 0xFF, 0xFF, 0);
      DrawLargeText(&gfx, x, MATCH_RESULT_Y, (char*)f_DrawText);
    } else if (g_Players[i].holes[ownHole].strokes <
        g_Players[i ^ 1].holes[holeIndex].strokes) {
      SetTextColor(&gfx, 0xFF, 0x7F, 0x7F);
      DrawLargeText(&gfx, x, MATCH_RESULT_Y, (char*)f_WinText);
    } else {
      SetTextColor(&gfx, 0x7F, 0x7F, 0xFF);
      DrawLargeText(&gfx, x, MATCH_RESULT_Y, (char*)f_LoseText);
    }

    x += MATCH_RESULT_SPACING;
  }

  UpdateAndDrawFireworks(&gfx);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  if (f_ResultCharactersLoadStep == RESULT_CHARACTERS_LOADING &&
      LoadResultCharacters() == TRUE) {
    f_ResultCharactersLoadStep = RESULT_CHARACTERS_LOADED;
  }

  if (f_ResultCharactersLoadStep < RESULT_CHARACTERS_LOADING) {
    f_ResultCharactersLoadStep++;
  }

  if (f_ResultCharactersLoadStep == RESULT_CHARACTERS_LOADED) {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;

    f_MatchResultFrameCount++;
    gSPMatrix(gfx++, PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
      translation[AXIS_X] =
          (i * RESULT_CHARACTER_SPACING) + RESULT_CHARACTER_LEFT;
      rotation[AXIS_X] = 0.0f;
      rotation[AXIS_Y] = 0;
      rotation[AXIS_Z] = 0;
      translation[AXIS_Y] = RESULT_CHARACTER_Y;
      translation[AXIS_Z] = RESULT_CHARACTER_Z;
      scale[AXIS_X] = RESULT_CHARACTER_SCALE;
      scale[AXIS_Y] = RESULT_CHARACTER_SCALE;
      scale[AXIS_Z] = RESULT_CHARACTER_SCALE;
      BuildTransformMatrix(matrix, rotation, translation, scale);
      ConvertMatrixToFixed(matrix, &f_ResultCharacterMatrices[i]);
      gSPMatrix(gfx++, PhysicalAddress(&f_ResultCharacterMatrices[i]),
          G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
      g_DisplayListHead = gfx;
      DrawCharacter(i);
      *displayListHead = g_DisplayListHead;
      gfx = *displayListHead;
    }

    AnimateAllCharacters();
  }

  *displayListHead = gfx;
}

const char f_HoleNumberFormat[] = "ﾎｰﾙ%d";
const char f_DrawText[] = "DRAW";
const char f_WinText[] = "WIN";
const char f_LoseText[] = "LOSE";

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
/* Drawn over the bet total column */
#define BET_TOTAL_LABEL 4
#define WIDE_BET_TOTAL 1000

/*
 * Draws the standings page of a round with more than one player: the hole,
 * then each player's place, face and score to par, and their bet total from
 * the bets that are worth something.
 */
void DrawStandingsPage(Gfx** displayListHead) {
  s32 places[MAX_PLAYER_COUNT];
  s32 scoresToPar[MAX_PLAYER_COUNT];
  Gfx* gfx;
  s32 holeIndex;
  s32 shift;
  s32 i;
  s32 j;
  s32 betTotal;

  gfx = *displayListHead;
  holeIndex = g_Scorecard.content.holeIndex;
  shift = g_Scorecard.hasBets ? 0 : NO_BETS_SHIFT;
  SetUpGradientTextDrawing(&gfx, 0);
  sprintf(g_PrintBuffer, "Hole %d", (int)(holeIndex + g_Scorecard.isTallied));
  DrawLargeTextWithShadow(&gfx, g_Scorecard.x + STANDINGS_HOLE_LEFT,
      g_Scorecard.y + STANDINGS_HOLE_TOP, g_PrintBuffer, HOLE_NUMBER_ENV_COLOR,
      HOLE_NUMBER_PRIM_COLOR, 0);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);

  for (i = 0; i < g_GameState.playerCount; i++) {
    scoresToPar[i] = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
  }

  /* Each player's place counts the players ahead of them */
  for (i = 0; i != g_GameState.playerCount; i++) {
    places[i] = 0;
    for (j = 0; j != g_GameState.playerCount; j++) {
      if (i != j && scoresToPar[i] > scoresToPar[j]) {
        places[i]++;
      }
    }
  }

  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawScorecardFace(&gfx, shift + STANDINGS_FACE_LEFT + g_Scorecard.x,
        STANDINGS_ROW_TOP + (i * STANDINGS_ROW_SPACING), i);
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawPlace(&gfx, shift + STANDINGS_PLACE_LEFT + g_Scorecard.x,
        STANDINGS_ROW_TOP + (i * STANDINGS_ROW_SPACING),
        places[i] & PLACE_MASK);
  }

  DrawScorecardLabel(&gfx, shift + STANDINGS_SCORE_LABEL_LEFT + g_Scorecard.x,
      STANDINGS_LABEL_TOP, SCORE_LABEL);
  if (g_Scorecard.hasBets) {
    DrawScorecardLabel(&gfx, g_Scorecard.x + STANDINGS_BET_TOTAL_LABEL_LEFT,
        STANDINGS_LABEL_TOP, BET_TOTAL_LABEL);
  }

  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.digitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.digitsImage),
      G_IM_FMT_CI, DIGIT_GLYPH_SIZE, DIGIT_FONT_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    betTotal = 0;
    if (scoresToPar[i] == 0) {
      sprintf(g_PrintBuffer, f_ZeroToParFormat);
    } else {
      sprintf(g_PrintBuffer, f_ScoreToParFormat, scoresToPar[i]);
    }

    DrawDigitText(&gfx, shift + STANDINGS_SCORE_LEFT + g_Scorecard.x,
        STANDINGS_TEXT_TOP + (i * STANDINGS_ROW_SPACING), g_PrintBuffer);
    if (g_Scorecard.hasBets) {
      for (j = 0; j != BET_COUNT; j++) {
        if (g_GameState.betRates[j] > 0) {
          betTotal += g_BetTallies[i][j];
        }
      }

      if (betTotal >= WIDE_BET_TOTAL) {
        sprintf(g_PrintBuffer, f_WideTotalFormat, betTotal);
      } else {
        sprintf(g_PrintBuffer, f_TotalFormat, betTotal);
      }

      DrawDigitText(&gfx, g_Scorecard.x + STANDINGS_BET_TOTAL_LEFT,
          STANDINGS_TEXT_TOP + (i * STANDINGS_ROW_SPACING), g_PrintBuffer);
    }
  }

  *displayListHead = gfx;
}

const char f_ZeroToParFormat[] = "  #0";
const char f_ScoreToParFormat[] = "%+4d";
const char f_WideTotalFormat[] = "%5d";
const char f_TotalFormat[] = "%4d";
const char f_CompactHoleFormat[] = "Hole%d";

/* SetTextSlot messages */
/* Shows SetTextSlotValue's count */
#define WINS_NEEDED_MESSAGE 1
/* Shown instead of a count of one when the match can be drawn */
#define LAST_WIN_NEEDED_MESSAGE 2
#define SUDDEN_DEATH_ENV_COLOR 0x00FFFF00
#define SUDDEN_DEATH_PRIM_COLOR 0xCF48FF00
#define MATCH_TITLE_LEFT 128
#define MATCH_TITLE_TOP 48
#define MATCH_ROW_SPACING 80
#define WINS_NEEDED_LEFT 88
#define WINS_NEEDED_TOP 72
/* The panel behind each player's row, drawn at twice its image's size */
#define PANEL_LEFT 32
#define PANEL_TOP 64
#define PANEL_WIDTH 128
#define PANEL_HEIGHT 32
#define PANEL_SHOWN_HEIGHT 62
#define PANEL_SECOND_TOP 144
#define TEXEL_STEP_HALF (TEXEL_STEP_ONE / 2)
/* The tiles that count off the wins needed, each from three pieces */
#define TILE_IMAGE_WIDTH 32
#define TILE_IMAGE_HEIGHT 96
#define TILE_LEFT 36
#define TILE_TOP 98
#define TILE_SPACING 25
#define TILE_PIECE_WIDTH 8
#define TILE_HEIGHT 24
#define TILES_PER_IMAGE_ROW 4
#define TILE_EDGE_IMAGE_ROW 3
/* The tenth tile has an image of its own, three pieces wide */
#define WIDE_TILE_INDEX 9
#define WIDE_TILE_LEFT 261
#define WIDE_TILE_IMAGE_COLUMN 1
#define WIDE_TILE_IMAGE_ROW 2
#define MATCH_FACE_LEFT 48
#define MATCH_FACE_TOP 64
/* The marks of the holes won, from a strip of images */
#define WIN_MARK_WIDTH 48
#define WIN_MARK_HEIGHT 24
#define WIN_MARK_SHOWN_WIDTH 24
#define WIN_MARK_TOP 97
#define WIN_MARK_SIZE (WIN_MARK_WIDTH * WIN_MARK_HEIGHT)
#define WIN_MARK_START_TIME 2
#define WIN_MARK_ANIMATION_TIME 18
#define WIN_MARK_FRAME_TIME 2
#define WIN_MARK_BOUNCE_HEIGHT 24.0f
#define WIN_MARK_BOUNCE_STEP (M_PI / 12)
/* The arrow that rises over the hole's winner */
#define ARROW_WIDTH 16
#define ARROW_HEIGHT 8
#define ARROW_LEFT 40
#define ARROW_TOP 81
/* When the hole's result joins the counts */
#define MATCH_COUNT_TIME 20
#define MATCH_ANIMATION_TIME 30

/*
 * Draws a match's first page: the hole, any sudden death or dormie, and, for
 * each player, their face, the tiles of the holes they still need and a mark
 * for each hole won. Once the hole's result is tallied, it animates the
 * winner's new mark.
 *
 * Doesn't match: sched places $fp's setup from the matchPanelImage address
 * later, after the panel's calls, and the tile loops take &gfx from $s1 where
 * the original copies it to $s7, spilling zero rather than $s1 to 0x6C($sp).
 * The allocation oracle finds no closer forcing. 110 words differ, 104 in
 * shape.
 */
#ifdef NON_MATCHING
void DrawMatchPage(Gfx** displayListHead) {
  s32 wins[MATCH_PLAYER_COUNT];
  Gfx* gfx;
  Gfx** head;
  s32 winsNeeded;
  s32 rowOffset;
  Boolean canDraw = FALSE;
  s32 halvedCount;
  s32 countEnd;
  s32 holeIndex;
  s32 i;
  s32 shareCount;
  s32 j;
  s32 column;
  s32 imageRow;
  s32 frame;
  s32 bounce;

  head = displayListHead;
  gfx = *displayListHead;
  if (g_Scorecard.content.isSuddenDeath == TRUE) {
    winsNeeded = 2;
  } else {
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

  /* Each halved hole leaves one fewer to win */
  countEnd = g_Scorecard.skinsCarryover.intoLastHole;
  halvedCount = 0;
  wins[1] = 0;
  wins[0] = 0;
  for (i = GetFirstHoleIndex(); i != countEnd; i++) {
    if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
      wins[0]++;
    } else if (g_Players[1].holes[i].strokes < g_Players[0].holes[i].strokes) {
      wins[1]++;
    } else if (g_Players[0].holes[i].strokes == g_Players[1].holes[i].strokes) {
      winsNeeded--;
      halvedCount++;
    }
  }

  if (g_Scorecard.skinsCarryover.onLastHole == MATCH_COUNT_TIME &&
      g_Scorecard.content.isSuddenDeath == FALSE &&
      g_Scorecard.holeWinnerIndex == HOLE_WINNER_HALVED &&
      (halvedCount & 1) == ((GetRoundHoleCount() - 1) & 1)) {
    PlaySoundWithEffects(SOUND_LOOK_BACK, TALLY_SOUND_CHANNEL,
        SCORECARD_SOUND_VOLUME, 0, SOUND_FULL_SCALE);
  }

  /* An even count of holes to share out can end level */
  shareCount = winsNeeded;
  winsNeeded = shareCount / 2;
  if ((shareCount - 1) / 2 != winsNeeded) {
    canDraw = TRUE;
  }

  if (winsNeeded <= 0) {
    winsNeeded = 1;
  }

  SetUpGradientTextDrawing(&gfx, 0);
  gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0, 0xFF);
  gDPSetPrimColor(gfx++, 0, 0, 0x30, 0xFF, 0, 0xFF);
  sprintf(g_PrintBuffer, f_CompactHoleFormat,
      g_Scorecard.content.holeIndex + g_Scorecard.isTallied);
  DrawLargeTextWithShadow(&gfx, g_Scorecard.x + STANDINGS_HOLE_LEFT,
      g_Scorecard.y + STANDINGS_HOLE_TOP, g_PrintBuffer, HOLE_NUMBER_ENV_COLOR,
      HOLE_NUMBER_PRIM_COLOR, 0);
  if (g_Scorecard.content.isSuddenDeath) {
    sprintf(g_PrintBuffer, "SUDDEN DEATH");
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + MATCH_TITLE_LEFT,
        g_Scorecard.y + MATCH_TITLE_TOP, g_PrintBuffer, SUDDEN_DEATH_ENV_COLOR,
        SUDDEN_DEATH_PRIM_COLOR, 0);
  } else {
    s32 points[MATCH_PLAYER_COUNT];

    /* A win counts two points and a halved hole one */
    points[0] = 0;
    points[1] = 0;
    for (i = GetFirstHoleIndex(); i != g_Scorecard.skinsCarryover.fromLastHole;
        i++) {
      if (g_Players[0].holes[i].strokes < g_Players[1].holes[i].strokes) {
        points[0] += 2;
      } else if (g_Players[1].holes[i].strokes <
          g_Players[0].holes[i].strokes) {
        points[1] += 2;
      } else if (g_Players[0].holes[i].strokes ==
          g_Players[1].holes[i].strokes) {
        points[0]++;
        points[1]++;
      }
    }

    if (GetRoundHoleCount() - 1 < points[0] ||
        GetRoundHoleCount() - 1 < points[1]) {
      sprintf(g_PrintBuffer, "DORMIE");
      DrawLargeTextWithShadow(&gfx, g_Scorecard.x + MATCH_TITLE_LEFT,
          g_Scorecard.y + MATCH_TITLE_TOP, g_PrintBuffer, HEADING_ENV_COLOR,
          HEADING_PRIM_COLOR, 0);
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gSPDisplayList(gfx++, g_DecalTextureSetup);
  for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
    if (canDraw == TRUE && winsNeeded - wins[i] == 1 &&
        winsNeeded - wins[i ^ 1] >= 2) {
      SetTextSlot(g_Scorecard.x + WINS_NEEDED_LEFT,
          (i * MATCH_ROW_SPACING) + WINS_NEEDED_TOP + g_Scorecard.y,
          LAST_WIN_NEEDED_MESSAGE, TEXT_SLOT_LARGE_FONT, i, 0, 0xFF, 0xFF, 0xFF,
          0xFF);
    } else if (winsNeeded != wins[i] && winsNeeded != wins[i ^ 1]) {
      SetTextSlotValue(winsNeeded - wins[i], i, 0);
      SetTextSlot(g_Scorecard.x + WINS_NEEDED_LEFT,
          (i * MATCH_ROW_SPACING) + WINS_NEEDED_TOP + g_Scorecard.y,
          WINS_NEEDED_MESSAGE, TEXT_SLOT_LARGE_FONT, i, 0, 0xFF, 0xFF, 0xFF,
          0xFF);
    } else {
      SetTextSlot(0, 0, NO_MESSAGE, TEXT_SLOT_LARGE_FONT, i, TEXT_FLAG_TYPED, 0,
          0, 0, 0);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.matchPanelImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.matchPanelImage),
      G_IM_FMT_CI, PANEL_WIDTH, PANEL_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + PANEL_LEFT),
      QuarterPixels(g_Scorecard.y + PANEL_TOP),
      QuarterPixels(g_Scorecard.x + PANEL_LEFT + (PANEL_WIDTH * 2)),
      QuarterPixels(g_Scorecard.y + PANEL_TOP + PANEL_SHOWN_HEIGHT),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_HALF, TEXEL_STEP_HALF);
  DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + PANEL_LEFT),
      QuarterPixels(g_Scorecard.y + PANEL_SECOND_TOP),
      QuarterPixels(g_Scorecard.x + PANEL_LEFT + (PANEL_WIDTH * 2)),
      QuarterPixels(g_Scorecard.y + PANEL_SECOND_TOP + PANEL_SHOWN_HEIGHT),
      G_TX_RENDERTILE, 0, 0, TEXEL_STEP_HALF, TEXEL_STEP_HALF);

  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.winsNeededImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.winsNeededImage),
      G_IM_FMT_CI, TILE_IMAGE_WIDTH, TILE_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  gDPPipeSync(gfx++);
  for (i = 0, rowOffset = 0; i != MATCH_PLAYER_COUNT;
      i++, rowOffset += MATCH_ROW_SPACING) {
    for (j = 0; j != winsNeeded; j++) {
      column = j % TILES_PER_IMAGE_ROW;
      imageRow = j / TILES_PER_IMAGE_ROW;
      if (j == WIDE_TILE_INDEX) {
        DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + WIDE_TILE_LEFT),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP),
            QuarterPixels(
                g_Scorecard.x + WIDE_TILE_LEFT + (TILE_PIECE_WIDTH * 3)),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP + TILE_HEIGHT),
            G_TX_RENDERTILE,
            TexelCoordinate(WIDE_TILE_IMAGE_COLUMN * TILE_PIECE_WIDTH),
            TexelCoordinate(WIDE_TILE_IMAGE_ROW * TILE_HEIGHT), TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
      } else {
        DrawTextureRect(&gfx,
            QuarterPixels(g_Scorecard.x + (j * TILE_SPACING) + TILE_LEFT),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP),
            QuarterPixels(g_Scorecard.x + (j * TILE_SPACING) + TILE_LEFT +
                TILE_PIECE_WIDTH),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP + TILE_HEIGHT),
            G_TX_RENDERTILE, 0,
            TexelCoordinate(TILE_EDGE_IMAGE_ROW * TILE_HEIGHT), TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
        DrawTextureRect(&gfx,
            QuarterPixels(g_Scorecard.x +
                ((j * TILE_SPACING) + TILE_LEFT + TILE_PIECE_WIDTH)),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP),
            QuarterPixels((j * TILE_SPACING) +
                (g_Scorecard.x + TILE_LEFT + (TILE_PIECE_WIDTH * 2))),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP + TILE_HEIGHT),
            G_TX_RENDERTILE, TexelCoordinate(column * TILE_PIECE_WIDTH),
            TexelCoordinate(imageRow * TILE_HEIGHT), TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
        DrawTextureRect(&gfx,
            QuarterPixels((j * TILE_SPACING) +
                (g_Scorecard.x + TILE_LEFT + (TILE_PIECE_WIDTH * 2))),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP),
            QuarterPixels((j * TILE_SPACING) + g_Scorecard.x + TILE_LEFT +
                (TILE_PIECE_WIDTH * 3)),
            QuarterPixels(g_Scorecard.y + rowOffset + TILE_TOP + TILE_HEIGHT),
            G_TX_RENDERTILE, TexelCoordinate(TILE_PIECE_WIDTH),
            TexelCoordinate(TILE_EDGE_IMAGE_ROW * TILE_HEIGHT), TEXEL_STEP_ONE,
            TEXEL_STEP_ONE);
      }
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawScorecardFace(&gfx, g_Scorecard.x + MATCH_FACE_LEFT,
        MATCH_FACE_TOP + (i * MATCH_ROW_SPACING), i);
  }

  /* A mark that the other player has matched shows its second image */
  gDPPipeSync(gfx++);
  LoadLargePalette(&gfx, ImageBody(g_Scorecard.winMarkImage));
  LoadTextureBlock(&gfx, LargeImageTexels(g_Scorecard.winMarkImage),
      G_IM_FMT_CI, G_IM_SIZ_8b, WIN_MARK_WIDTH, WIN_MARK_HEIGHT, 0, 0, 0, 0, 0,
      0, 0);
  gDPPipeSync(gfx++);
  for (i = 0; i != MATCH_PLAYER_COUNT; i++) {
    for (j = 0; j != wins[i]; j++) {
      DrawTextureRect(&gfx,
          QuarterPixels(((j * TILE_SPACING) + TILE_LEFT) + g_Scorecard.x),
          QuarterPixels(
              ((i * MATCH_ROW_SPACING) + WIN_MARK_TOP) + g_Scorecard.y),
          QuarterPixels((j * TILE_SPACING) + TILE_LEFT + WIN_MARK_SHOWN_WIDTH +
              g_Scorecard.x),
          QuarterPixels((i * MATCH_ROW_SPACING) + WIN_MARK_TOP +
              WIN_MARK_HEIGHT + g_Scorecard.y),
          G_TX_RENDERTILE, 0,
          (j < wins[i ^ 1]) ? TexelCoordinate(WIN_MARK_HEIGHT) : 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  if ((u8)(g_Scorecard.skinsCarryover.onLastHole - WIN_MARK_START_TIME) <
      WIN_MARK_ANIMATION_TIME) {
    frame = g_Scorecard.skinsCarryover.onLastHole - WIN_MARK_START_TIME;
    j = f_WinMarkFrames[frame / WIN_MARK_FRAME_TIME];
    bounce = -(
        s32)(sinf((f32)frame * WIN_MARK_BOUNCE_STEP) * WIN_MARK_BOUNCE_HEIGHT);
    /* The mark only rises */
    bounce = (bounce <= 0) ? bounce : 0;
    LoadTextureBlock(&gfx,
        LargeImageTexels(g_Scorecard.winMarkImage) + (j * WIN_MARK_SIZE),
        G_IM_FMT_CI, G_IM_SIZ_8b, WIN_MARK_WIDTH, WIN_MARK_HEIGHT, 0, 0, 0, 0,
        0, 0, 0);
    holeIndex = g_Scorecard.skinsCarryover.intoLastHole;
    if (g_Players[0].holes[holeIndex].strokes <
        g_Players[1].holes[holeIndex].strokes) {
      DrawTextureRect(&gfx,
          QuarterPixels((wins[0] * TILE_SPACING) + TILE_LEFT + g_Scorecard.x),
          QuarterPixels(WIN_MARK_TOP + bounce + g_Scorecard.y),
          QuarterPixels((wins[0] * TILE_SPACING) +
              (TILE_LEFT + WIN_MARK_SHOWN_WIDTH) + g_Scorecard.x),
          QuarterPixels(
              WIN_MARK_HEIGHT + (WIN_MARK_TOP + bounce) + g_Scorecard.y),
          G_TX_RENDERTILE, 0,
          (wins[1] < wins[0]) ? TexelCoordinate(WIN_MARK_HEIGHT) : 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else if (g_Players[1].holes[holeIndex].strokes <
        g_Players[0].holes[holeIndex].strokes) {
      DrawTextureRect(&gfx,
          QuarterPixels((wins[1] * TILE_SPACING) + TILE_LEFT + g_Scorecard.x),
          QuarterPixels(
              MATCH_ROW_SPACING + (WIN_MARK_TOP + bounce) + g_Scorecard.y),
          QuarterPixels((wins[1] * TILE_SPACING) +
              (TILE_LEFT + WIN_MARK_SHOWN_WIDTH) + g_Scorecard.x),
          QuarterPixels(MATCH_ROW_SPACING + WIN_MARK_HEIGHT + WIN_MARK_TOP +
              bounce + g_Scorecard.y),
          G_TX_RENDERTILE, 0,
          (wins[0] < wins[1]) ? TexelCoordinate(WIN_MARK_HEIGHT) : 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }

    LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.winnerArrowImage));
    LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.winnerArrowImage),
        G_IM_FMT_CI, ARROW_WIDTH, ARROW_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
    holeIndex = g_Scorecard.skinsCarryover.intoLastHole;
    if (g_Players[0].holes[holeIndex].strokes <
        g_Players[1].holes[holeIndex].strokes) {
      /* In a local, to match */
      s32 arrowRight = g_Scorecard.x + ARROW_LEFT + ARROW_WIDTH;

      DrawTextureRect(&gfx,
          QuarterPixels((wins[0] * TILE_SPACING) + ARROW_LEFT + g_Scorecard.x),
          QuarterPixels(-g_Scorecard.skinsCarryover.onLastHole + ARROW_TOP +
              g_Scorecard.y),
          QuarterPixels((wins[0] * TILE_SPACING) + arrowRight),
          QuarterPixels(-g_Scorecard.skinsCarryover.onLastHole +
              (ARROW_HEIGHT + ARROW_TOP) + g_Scorecard.y),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else if (g_Players[1].holes[holeIndex].strokes <
        g_Players[0].holes[holeIndex].strokes) {
      /* In a local, to match */
      s32 arrowRight = g_Scorecard.x + ARROW_LEFT + ARROW_WIDTH;

      DrawTextureRect(&gfx,
          QuarterPixels((wins[1] * TILE_SPACING) + ARROW_LEFT + g_Scorecard.x),
          QuarterPixels(-g_Scorecard.skinsCarryover.onLastHole +
              (ARROW_TOP + MATCH_ROW_SPACING) + g_Scorecard.y),
          QuarterPixels((wins[1] * TILE_SPACING) + arrowRight),
          QuarterPixels(-g_Scorecard.skinsCarryover.onLastHole + ARROW_HEIGHT +
              ARROW_TOP + MATCH_ROW_SPACING + g_Scorecard.y),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  gDPPipeSync(gfx++);
  *head = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard_drawing", DrawMatchPage);
#endif

RESUME_RODATA_SECTION();
const char f_StringFormat[] = "%s";

/*
 * The skins page lists each player's place, face and skins, and the skins
 * carried over
 */
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
/* Only the star and the digits */
#define SKINS_DIGIT_FONT_HEIGHT (DIGIT_GLYPH_COLON * DIGIT_GLYPH_SIZE)
/* "Win" cycles through the colors, each channel a sine wave about mid-gray */
#define WIN_COLOR_STEP (M_PI / 9)
#define WIN_COLOR_MIDPOINT 0x7F
#define WIN_COLOR_SWING 127.0f

/*
 * Draws the skins page of a skins match or club slots round: the hole, each
 * player's place, face and skins, and the skins carried over. The last hole's
 * winner is shown without the skins still on it, which TallyScorecardSkins
 * counts down. While a hole has a winner, "Win" flashes beside each player
 * with strokes at holes[holeIndex].
 */
void DrawSkinsPage(Gfx** displayListHead) {
  s32 places[MAX_PLAYER_COUNT];
  s32 skinCounts[MAX_PLAYER_COUNT];
  Gfx* gfx;
  s32 holeIndex;
  s32 i;
  s32 j;
  s32 red;
  s32 green;

  gfx = *displayListHead;
  holeIndex = g_Scorecard.content.holeIndex;
  SetUpGradientTextDrawing(&gfx, 0);
  sprintf(
      g_PrintBuffer, f_CompactHoleFormat, holeIndex + g_Scorecard.isTallied);
  DrawLargeTextWithShadow(&gfx, g_Scorecard.x + STANDINGS_HOLE_LEFT,
      g_Scorecard.y + STANDINGS_HOLE_TOP, g_PrintBuffer, HOLE_NUMBER_ENV_COLOR,
      HOLE_NUMBER_PRIM_COLOR, 0);
  sprintf(g_PrintBuffer, f_StringFormat, f_CarryOverText);
  DrawLargeTextWithShadow(&gfx, g_Scorecard.x + SKINS_TITLE_LEFT,
      g_Scorecard.y + SKINS_TITLE_TOP, g_PrintBuffer, HEADING_ENV_COLOR,
      HEADING_PRIM_COLOR, 0);

  for (i = 0; i != g_GameState.playerCount; i++) {
    if (g_Scorecard.holeWinnerIndex == i) {
      skinCounts[i] = (s16)g_RoundHud.skinsTotals[i] -
          g_Scorecard.skinsCarryover.onLastHole;
    } else {
      skinCounts[i] = (s16)g_RoundHud.skinsTotals[i];
    }
  }

  /* Each player's place counts the players with more skins */
  for (i = 0; i != g_GameState.playerCount; i++) {
    places[i] = 0;
    for (j = 0; j != g_GameState.playerCount; j++) {
      if (i != j) {
        if (skinCounts[i] < skinCounts[j]) {
          places[i]++;
        }

        if (skinCounts[i] == skinCounts[j]) {
          places[i] |= PLACE_TIED;
        }
      }
    }
  }

  for (i = 0; i != g_GameState.playerCount; i++) {
    if (g_Scorecard.holeWinnerIndex >= 0 &&
        g_Players[i].holes[holeIndex].strokes != 0) {
      f_WinColorAngle += WIN_COLOR_STEP;
      red = (u8)((s32)(sinf(f_WinColorAngle) * WIN_COLOR_SWING) +
          WIN_COLOR_MIDPOINT);
      green = (u8)((s32)(sinf(f_WinColorAngle + M_PI) * WIN_COLOR_SWING) +
          WIN_COLOR_MIDPOINT);
      SetTextColor(&gfx, red, green,
          (u8)((s32)(sinf(f_WinColorAngle + (M_PI / 2)) * WIN_COLOR_SWING) +
              WIN_COLOR_MIDPOINT));
      DrawSmallText(&gfx, g_Scorecard.x + SKINS_WIN_LEFT,
          SKINS_WIN_TOP + (i * SKINS_ROW_SPACING), (char*)f_WinText);
    }
  }

  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawScorecardFace(&gfx, g_Scorecard.x + SKINS_FACE_LEFT,
        SKINS_ROW_TOP + (i * SKINS_ROW_SPACING), i);
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawPlace(&gfx, g_Scorecard.x + SKINS_PLACE_LEFT,
        SKINS_ROW_TOP + (i * SKINS_ROW_SPACING), places[i] & PLACE_MASK);
  }

  LoadLargePalette(&gfx, ImageBody(g_Scorecard.winMarkImage));
  LoadTextureBlock(&gfx, g_Scorecard.winMarkImage + SKIN_ICON_TEXELS_OFFSET,
      G_IM_FMT_CI, G_IM_SIZ_8b, SKIN_ICON_SIZE, SKIN_ICON_SIZE, 0, 0, 0, 0, 0,
      0, 0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + SKIN_ICON_LEFT),
        QuarterPixels(SKIN_ICON_TOP + (i * SKINS_ROW_SPACING)),
        QuarterPixels(g_Scorecard.x + SKIN_ICON_LEFT + SKIN_ICON_SIZE),
        QuarterPixels(SKIN_ICON_TOP + SKIN_ICON_SIZE + (i * SKINS_ROW_SPACING)),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + CARRYOVER_ICON_LEFT),
      QuarterPixels(CARRYOVER_ICON_TOP),
      QuarterPixels(g_Scorecard.x + CARRYOVER_ICON_LEFT + SKIN_ICON_SIZE),
      QuarterPixels(CARRYOVER_ICON_TOP + SKIN_ICON_SIZE), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);

  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.digitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.digitsImage),
      G_IM_FMT_CI, DIGIT_GLYPH_SIZE, SKINS_DIGIT_FONT_HEIGHT, 0, 0, 0, 0, 0, 0,
      0);
  for (i = 0; i != g_GameState.playerCount; i++) {
    sprintf(g_PrintBuffer, "* %d", (int)skinCounts[i]);
    DrawDigitText(&gfx, g_Scorecard.x + SKINS_COUNT_LEFT,
        SKINS_COUNT_TOP + (i * SKINS_ROW_SPACING), g_PrintBuffer);
  }

  sprintf(g_PrintBuffer, "* %d",
      (g_Scorecard.isTallied == TRUE)
          ? g_Scorecard.skinsCarryover.fromLastHole
          : g_Scorecard.skinsCarryover.intoLastHole);
  DrawDigitText(&gfx, g_Scorecard.x + CARRYOVER_COUNT_LEFT, CARRYOVER_COUNT_TOP,
      g_PrintBuffer);
  *displayListHead = gfx;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_BlankBetName);

/* The bets page: a grid with a column for each bet being played and one for the
 * totals */
#define BET_COLUMN_SPACING 35
#define BET_NAME_SPACING 36
#define BET_BLINK_TIME 7
#define UNPLAYED_BET_GRAY 0xBF
/* The grid's lines are cut from a small image of 3 texel pieces */
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
/* Stretches one texel along a header line */
#define GRID_STRETCHED_STEP 11
/* Stretches the column line's texels down the grid */
#define GRID_COLUMN_STEP 18
#define GRID_TEXEL_LINE 3
#define GRID_TEXEL_TOP_END 6
#define GRID_TEXEL_BOTTOM_END 9
#define GRID_TEXEL_CROSSING 12
/* The bets' heading images, and the totals' heading after them */
#define BET_IMAGE_SIZE 24
#define BET_IMAGE_LEFT 60
#define BET_IMAGE_TOP 46
/* The subtitles under some bets' headings, from rows of one image */
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

/*
 * Draws the bets page: each bet being played, its rate, any pot carried over,
 * and what each player has won on it and in total. A bet with a rate of zero is
 * grayed out.
 *
 * Doesn't match: GCC's loop pass hoists a table address into $s2 before
 * the bet rates' loop, which the original doesn't, and the loop's
 * registers differ from there. 210 words differ, 42 in shape.
 */
#ifdef NON_MATCHING
void DrawBetsPage(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 i;
  s32 j;
  s32 left;
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
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.gridImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.gridImage), G_IM_FMT_CI,
      GRID_IMAGE_SIZE, GRID_IMAGE_SIZE, 0, 0, 0, 0, 0, 0, 0);
  DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + GRID_LEFT),
      QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP),
      QuarterPixels(g_Scorecard.x + GRID_RIGHT),
      QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP + GRID_LINE_WIDTH),
      G_TX_RENDERTILE, TexelCoordinate(GRID_TEXEL_LINE), 0, GRID_STRETCHED_STEP,
      TEXEL_STEP_ONE);
  for (i = 0; i != GRID_ROW_COUNT; i++) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + GRID_ROW_LINE_LEFT),
        QuarterPixels(
            (i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
        QuarterPixels(g_Scorecard.x + GRID_RIGHT),
        QuarterPixels((i * GRID_ROW_HEIGHT) +
            (GRID_LINE_WIDTH + GRID_ROW_LINE_TOP) + g_Scorecard.y),
        G_TX_RENDERTILE, TexelCoordinate(GRID_TEXEL_LINE), 0,
        GRID_STRETCHED_STEP, TEXEL_STEP_ONE);
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + GRID_ROW_LINE_LEFT),
        QuarterPixels(
            (i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
        QuarterPixels(g_Scorecard.x + GRID_ROW_LINE_LEFT + GRID_LINE_WIDTH),
        QuarterPixels((i * GRID_ROW_HEIGHT) +
            (GRID_LINE_WIDTH + GRID_ROW_LINE_TOP) + g_Scorecard.y),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + GRID_RIGHT - GRID_LINE_WIDTH),
        QuarterPixels(
            (i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP + g_Scorecard.y),
        QuarterPixels(g_Scorecard.x + GRID_RIGHT),
        QuarterPixels((i * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP +
            GRID_LINE_WIDTH + g_Scorecard.y),
        G_TX_RENDERTILE, 0, TexelCoordinate(GRID_TEXEL_LINE), TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
  }

  for (i = 0, left = 0; i != GRID_COLUMN_COUNT;
      i++, left += BET_COLUMN_SPACING) {
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT),
        QuarterPixels(g_Scorecard.y + GRID_TOP),
        QuarterPixels(
            g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH),
        QuarterPixels(g_Scorecard.y + GRID_BOTTOM), G_TX_RENDERTILE,
        TexelCoordinate(GRID_TEXEL_LINE), TexelCoordinate(GRID_TEXEL_LINE),
        TEXEL_STEP_ONE, GRID_COLUMN_STEP);
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT),
        QuarterPixels(g_Scorecard.y + GRID_TOP),
        QuarterPixels(
            g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH),
        QuarterPixels(g_Scorecard.y + GRID_TOP + GRID_LINE_WIDTH),
        G_TX_RENDERTILE, 0, TexelCoordinate(GRID_TEXEL_TOP_END), TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT),
        QuarterPixels(g_Scorecard.y + GRID_BOTTOM - GRID_LINE_WIDTH),
        QuarterPixels(
            g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH),
        QuarterPixels(g_Scorecard.y + GRID_BOTTOM), G_TX_RENDERTILE, 0,
        TexelCoordinate(GRID_TEXEL_BOTTOM_END), TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  /* Where the lines cross */
  for (i = 0, left = 0; i != GRID_COLUMN_COUNT;
      i++, left += BET_COLUMN_SPACING) {
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT),
        QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP),
        QuarterPixels(
            g_Scorecard.x + left + GRID_COLUMN_LINE_LEFT + GRID_LINE_WIDTH),
        QuarterPixels(g_Scorecard.y + GRID_HEADER_LINE_TOP + GRID_LINE_WIDTH),
        G_TX_RENDERTILE, 0, TexelCoordinate(GRID_TEXEL_CROSSING),
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    for (j = 0; j != GRID_ROW_COUNT; j++) {
      DrawTextureRect(&gfx,
          QuarterPixels(GRID_COLUMN_LINE_LEFT + left + g_Scorecard.x),
          QuarterPixels(
              g_Scorecard.y + GRID_ROW_LINE_TOP + (j * GRID_ROW_HEIGHT)),
          QuarterPixels(
              GRID_COLUMN_LINE_LEFT + (GRID_LINE_WIDTH + left) + g_Scorecard.x),
          QuarterPixels((j * GRID_ROW_HEIGHT) + GRID_ROW_LINE_TOP +
              GRID_LINE_WIDTH + g_Scorecard.y),
          G_TX_RENDERTILE, 0, TexelCoordinate(GRID_TEXEL_CROSSING),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
  for (i = 0, column = 0, left = 0; i != BET_COUNT; i++) {
    if (g_GameState.betRates[i] != UNSET_BET_RATE) {
      frame = 0;
      if (g_GameState.betRates[i] == 0) {
        gDPSetPrimColor(gfx++, 0, 0, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY,
            UNPLAYED_BET_GRAY, 0xFF);
      } else {
        frame = f_BetBlinkFrame;
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
      }

      LoadSmallPalette(
          &gfx, 0, ImageBody(g_Scorecard.betImages[column][frame]));
      gDPLoadTextureTile_4b(gfx++,
          ImageTexels(g_Scorecard.betImages[column][frame]), G_IM_FMT_CI,
          BET_IMAGE_SIZE, BET_IMAGE_SIZE, 0, 0, BET_IMAGE_SIZE - 1,
          BET_IMAGE_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
          G_TX_NOLOD);
      DrawTextureRect(&gfx,
          QuarterPixels(BET_IMAGE_LEFT + left + g_Scorecard.x),
          QuarterPixels(BET_IMAGE_TOP),
          QuarterPixels(
              BET_IMAGE_LEFT + (BET_IMAGE_SIZE + left) + g_Scorecard.x),
          QuarterPixels(BET_IMAGE_TOP + BET_IMAGE_SIZE), G_TX_RENDERTILE, 0, 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      column++;
      left += BET_COLUMN_SPACING;
    }
  }

  gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.totalHeadingImage));
  gDPLoadTextureTile_4b(gfx++, ImageTexels(g_Scorecard.totalHeadingImage),
      G_IM_FMT_CI, BET_IMAGE_SIZE, BET_IMAGE_SIZE, 0, 0, BET_IMAGE_SIZE - 1,
      BET_IMAGE_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  DrawTextureRect(&gfx,
      QuarterPixels(
          (column * BET_COLUMN_SPACING) + BET_IMAGE_LEFT + g_Scorecard.x),
      QuarterPixels(BET_IMAGE_TOP),
      QuarterPixels((column * BET_COLUMN_SPACING) +
          (BET_IMAGE_LEFT + BET_IMAGE_SIZE) + g_Scorecard.x),
      QuarterPixels(BET_IMAGE_TOP + BET_IMAGE_SIZE), G_TX_RENDERTILE, 0, 0,
      TEXEL_STEP_ONE, TEXEL_STEP_ONE);

  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.betSubtitlesImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.betSubtitlesImage),
      G_IM_FMT_CI, BET_SUBTITLE_WIDTH, BET_SUBTITLE_IMAGE_HEIGHT, 0, 0, 0, 0, 0,
      0, 0);
  for (i = 0, left = 0; i < BET_COUNT; i++) {
    if (g_GameState.betRates[i] != UNSET_BET_RATE) {
      if (f_BetSubtitleRows[i] != NO_BET_SUBTITLE) {
        if (g_GameState.betRates[i] == 0) {
          SetTextColor(
              &gfx, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY);
        } else {
          SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
        }

        DrawTextureRect(&gfx,
            QuarterPixels(BET_SUBTITLE_LEFT + left + g_Scorecard.x),
            QuarterPixels(BET_SUBTITLE_TOP),
            QuarterPixels(BET_SUBTITLE_LEFT + (BET_SUBTITLE_WIDTH + left) +
                g_Scorecard.x),
            QuarterPixels(BET_SUBTITLE_TOP + BET_SUBTITLE_HEIGHT),
            G_TX_RENDERTILE, 0,
            TexelCoordinate(f_BetSubtitleRows[i] * BET_SUBTITLE_HEIGHT),
            TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      }

      left += BET_COLUMN_SPACING;
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  for (i = 0; i != g_GameState.playerCount; i++) {
    DrawSmallFace(&gfx, g_Scorecard.x + BET_FACE_LEFT,
        BET_FACE_TOP + (i * GRID_ROW_HEIGHT), i, TRUE);
  }

  BeginShadowedText(&gfx);
  for (i = 0, left = 0; i != BET_COUNT; i++) {
    if (g_GameState.betRates[i] != UNSET_BET_RATE) {
      if (g_GameState.betRates[i] == 0) {
        SetTextColor(
            &gfx, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY, UNPLAYED_BET_GRAY);
      } else {
        SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
      }

      DrawCenteredSmallText(&gfx, BET_NAME_LEFT + left + g_Scorecard.x,
          g_Scorecard.y + BET_NAME_TOP, f_BetNames[i]);
      left += BET_NAME_SPACING;
    }
  }

  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);

  /* Drawn a letter at a time, each at its own position */
  DrawSmallText(&gfx, g_Scorecard.x + 21, g_Scorecard.y + CARRY_OVER_TOP, "C");
  DrawSmallText(&gfx, g_Scorecard.x + 29, g_Scorecard.y + CARRY_OVER_TOP, "a");
  DrawSmallText(&gfx, g_Scorecard.x + 35, g_Scorecard.y + CARRY_OVER_TOP, "r");
  DrawSmallText(&gfx, g_Scorecard.x + 40, g_Scorecard.y + CARRY_OVER_TOP, "r");
  DrawSmallText(&gfx, g_Scorecard.x + 46, g_Scorecard.y + CARRY_OVER_TOP, "y");
  DrawSmallText(
      &gfx, g_Scorecard.x + 24, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "o");
  DrawSmallText(
      &gfx, g_Scorecard.x + 31, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "v");
  DrawSmallText(
      &gfx, g_Scorecard.x + 39, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "e");
  DrawSmallText(
      &gfx, g_Scorecard.x + 45, g_Scorecard.y + CARRY_OVER_SECOND_TOP, "r");
  /* A pot holds its bet's rate plus whatever ties carried over */
  for (i = 0, column = 0; i != BET_COUNT; i++) {
    if (g_GameState.betRates[i] > 0) {
      if (i == BET_LONGEST_DRIVE) {
        sprintf(g_PrintBuffer, f_TotalFormat,
            g_LongestDrivePot - g_GameState.betRates[BET_LONGEST_DRIVE]);
        DrawCenteredSmallText(&gfx,
            (column * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
            g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
      }

      if (i == BET_NEAREST_PIN) {
        sprintf(g_PrintBuffer, f_TotalFormat,
            g_NearestPinPot - g_GameState.betRates[BET_NEAREST_PIN]);
        DrawCenteredSmallText(&gfx,
            (column * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
            g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
      }

      if (i == BET_UNK4) {
        sprintf(g_PrintBuffer, f_TotalFormat,
            g_HolePot - g_GameState.betRates[BET_UNK4]);
        DrawCenteredSmallText(&gfx,
            (column * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
            g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
      }

      if (i == BET_UNK5) {
        sprintf(g_PrintBuffer, f_TotalFormat,
            g_HolePot - g_GameState.betRates[BET_UNK5]);
        DrawCenteredSmallText(&gfx,
            (column * BET_COLUMN_SPACING) + BET_POT_LEFT + g_Scorecard.x,
            g_Scorecard.y + BET_POT_TOP, g_PrintBuffer);
      }
    }

    column += g_GameState.betRates[i] >= 0;
  }

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, BET_RATE_RED, BET_RATE_GREEN, 0);
  for (i = 0, left = 0; i != BET_COUNT; i++) {
    if (g_GameState.betRates[i] != UNSET_BET_RATE) {
      if (g_GameState.betRates[i] != 0) {
        sprintf(g_PrintBuffer, f_TwoDigitFormat, g_GameState.betRates[i]);
        DrawCondensedShadowedText(&gfx, BET_RATE_LEFT + left + g_Scorecard.x,
            g_Scorecard.y + BET_RATE_TOP, g_PrintBuffer);
      }

      left += BET_COLUMN_SPACING;
    }
  }

  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  for (i = 0; i != g_GameState.playerCount; i++) {
    total = 0;
    column = 0;
    for (j = 0, left = 0; j != BET_COUNT; j++) {
      if (g_GameState.betRates[j] == 0) {
        DrawShadowedText(&gfx, NO_TALLY_LEFT + left + g_Scorecard.x,
            (i * GRID_ROW_HEIGHT) + BETS_PAGE_TALLY_TOP + g_Scorecard.y, "-");
        left += BET_COLUMN_SPACING;
        column++;
      } else if (g_GameState.betRates[j] != UNSET_BET_RATE) {
        total += g_BetTallies[i][j];
        sprintf(g_PrintBuffer, f_WideTotalFormat, g_BetTallies[i][j]);
        column++;
        DrawCondensedShadowedText(&gfx,
            BETS_PAGE_TALLY_LEFT + left + g_Scorecard.x,
            (i * GRID_ROW_HEIGHT) + BETS_PAGE_TALLY_TOP + g_Scorecard.y,
            g_PrintBuffer);
        left += BET_COLUMN_SPACING;
      }
    }

    sprintf(g_PrintBuffer, f_WideTotalFormat, total);
    DrawCondensedShadowedText(&gfx,
        (column * BET_COLUMN_SPACING) + BETS_PAGE_TALLY_LEFT + g_Scorecard.x,
        (i * GRID_ROW_HEIGHT) + BETS_PAGE_TALLY_TOP + g_Scorecard.y,
        g_PrintBuffer);
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard_drawing", DrawBetsPage);
#endif

RESUME_RODATA_SECTION();
const char f_TwoDigitFormat[] = "%2d";
const char f_BetDashText[] = "-";

/* The Speed Golf page: the front nine's times on the left, the back nine's on
 * the right */
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

/*
 * Draws the Speed Golf page: each hole's time, with dashes for a hole without
 * one, and the round's time and score so far.
 *
 * Doesn't match: the target indexes the minute arrays from the offset it
 * shares with the other time arrays, where this steps pointers through them,
 * and it adds the rows' offsets to the scorecard's y in a different order.
 * Keeping the round's seconds in scoreToPar, which the original may share
 * with them, leaves 95. 104 words differ.
 */
#ifdef NON_MATCHING
void DrawSpeedGolfPage(Gfx** displayListHead) {
  s32 shownHoleCount = g_Scorecard.content.holeIndex +
      (g_Players[0].holeStatus != HOLE_STATUS_PLAYING);
  Gfx* gfx = *displayListHead;
  s32 i;
  s32 minutes;
  s32 seconds;
  s32 scoreToPar;

  SetUpGradientTextDrawing(&gfx, 0);
  for (i = 0; i != NINE_HOLE_COUNT; i++) {
    sprintf(g_PrintBuffer, f_TwoDigitFormat, i + 1);
    DrawLargeTextWithShadow(&gfx, g_Scorecard.x + HOLE_NUMBER_LEFT,
        (i * HOLE_ROW_SPACING) + (g_Scorecard.y + HOLE_NUMBER_TOP),
        g_PrintBuffer, HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
    sprintf(g_PrintBuffer, f_TwoDigitFormat, i + NINE_HOLE_COUNT + 1);
    DrawLargeTextWithShadow(&gfx,
        g_Scorecard.x + HOLE_NUMBER_LEFT + BACK_NINE_SPACING,
        (i * HOLE_ROW_SPACING) + (g_Scorecard.y + HOLE_NUMBER_TOP),
        g_PrintBuffer, HOLE_NUMBER_ENV_COLOR, HOLE_NUMBER_PRIM_COLOR, 0);
  }

  sprintf(g_PrintBuffer, "Time");
  DrawLargeTextWithShadow(&gfx, g_Scorecard.x + ROUND_TOTAL_LABEL_LEFT,
      g_Scorecard.y + ROUND_TIME_TOP, g_PrintBuffer, HEADING_ENV_COLOR,
      HEADING_PRIM_COLOR, 0);
  sprintf(g_PrintBuffer, "Score");
  DrawLargeTextWithShadow(&gfx, g_Scorecard.x + ROUND_TOTAL_LABEL_LEFT,
      g_Scorecard.y + ROUND_SCORE_TOP, g_PrintBuffer, HEADING_ENV_COLOR,
      HEADING_PRIM_COLOR, 0);

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  for (i = 0; i != NINE_HOLE_COUNT; i++) {
    if (i < shownHoleCount) {
      minutes = g_HoleTimeMinutes[i];
      if (minutes < 0) {
        sprintf(g_PrintBuffer, f_StringFormat, "--------");
        DrawMediumText(&gfx, g_Scorecard.x + HOLE_TIME_LEFT,
            (i * HOLE_ROW_SPACING) + (g_Scorecard.y + HOLE_TIME_TOP),
            g_PrintBuffer);
      } else {
        sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes, g_HoleTimeSeconds[i],
            g_HoleTimeHundredths[i]);
        DrawMediumText(&gfx, g_Scorecard.x + HOLE_TIME_LEFT,
            (i * HOLE_ROW_SPACING) + (g_Scorecard.y + HOLE_TIME_TOP),
            g_PrintBuffer);
      }
    }

    if (i + NINE_HOLE_COUNT < shownHoleCount) {
      minutes = g_HoleTimeMinutes[i + NINE_HOLE_COUNT];
      if (minutes < 0) {
        sprintf(g_PrintBuffer, f_StringFormat, "--------");
        DrawMediumText(&gfx, g_Scorecard.x + HOLE_TIME_LEFT + BACK_NINE_SPACING,
            (i * HOLE_ROW_SPACING) + (g_Scorecard.y + HOLE_TIME_TOP),
            g_PrintBuffer);
      } else {
        sprintf(g_PrintBuffer, "%02d:%02d:%02d", minutes,
            g_HoleTimeSeconds[i + NINE_HOLE_COUNT],
            g_HoleTimeHundredths[i + NINE_HOLE_COUNT]);
        DrawMediumText(&gfx, g_Scorecard.x + HOLE_TIME_LEFT + BACK_NINE_SPACING,
            (i * HOLE_ROW_SPACING) + (g_Scorecard.y + HOLE_TIME_TOP),
            g_PrintBuffer);
      }
    }
  }

  seconds =
      g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND % SECONDS_PER_MINUTE;
  sprintf(g_PrintBuffer, "%02d:%02d:%02d",
      g_GameState.stopwatchFrameCount / FRAMES_PER_SECOND / SECONDS_PER_MINUTE,
      seconds,
      (s32)(g_GameState.stopwatchFrameCount *
          ((f32)HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND)) %
          HUNDREDTHS_PER_SECOND);
  DrawFixedWidthLargeText(&gfx, g_Scorecard.x + ROUND_TOTAL_LEFT,
      g_Scorecard.y + ROUND_TIME_TOP, g_PrintBuffer);
  scoreToPar = GetNetScoreToPar(shownHoleCount, 0);
  if (scoreToPar == 0) {
    sprintf(g_PrintBuffer, "#0");
  } else {
    sprintf(g_PrintBuffer, "%+2d", scoreToPar);
  }

  DrawFixedWidthLargeText(&gfx, g_Scorecard.x + ROUND_TOTAL_LEFT,
      g_Scorecard.y + ROUND_SCORE_TOP, g_PrintBuffer);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard_drawing", DrawSpeedGolfPage);
RESUME_RODATA_SECTION();
#endif

/* Writes a standing's place, score to par and holes played into a leaderboard
 * row */
void FormatLeaderboardRow(char* row, TournamentStanding* standing) {
  u8 place = standing->place;
  s16 score;
  s8 holeCount;

  if (place & PLACE_TIED) {
    sprintf(row, "%2dT          ", place & PLACE_MASK);
  } else {
    sprintf(row, "%2d           ", place);
  }

  score = standing->score;
  if (score == 0) {
    sprintf(row + LEADERBOARD_SCORE_COLUMN, "   #0   ", 0);
  } else {
    sprintf(row + LEADERBOARD_SCORE_COLUMN, "  %+3d   ", score);
  }

  holeCount = standing->holeCount;
  if (holeCount == 0) {
    sprintf(row + LEADERBOARD_HOLES_COLUMN, "  -");
  } else {
    sprintf(row + LEADERBOARD_HOLES_COLUMN, " %2d", holeCount);
  }
}

/*
 * The tournament leaderboard is a board of split flaps. A row that changes
 * turns its top flap down over a hinge across the middle of the row.
 */
/* Each label sits over the column it names */
#define PLACE_LABEL 0
#define NAME_LABEL 1
#define HOLES_LABEL 3
#define LEADERBOARD_LABEL_TOP 46
#define PLACE_LABEL_LEFT 32
#define NAME_LABEL_LEFT 100
#define SCORE_LABEL_LEFT 184
#define HOLES_LABEL_LEFT 248
#define LEADERBOARD_ROW_SPACING 16
#define ROW_IMAGE_WIDTH 256
#define ROW_IMAGE_HEIGHT 14
#define ROW_LEFT 32
#define ROW_TOP 66
#define ROW_HINGE_TOP 72
/* The texel row where the bottom flap starts */
#define ROW_BOTTOM_HALF_ROW 7
#define FLAP_NAME_IMAGE_WIDTH 128
#define FLAP_NAME_LEFT 96
#define ROW_TEXT_LEFT 34
#define ROW_TEXT_TOP 67
#define RowTextTop(row) \
  (ROW_TEXT_TOP + ((row) * LEADERBOARD_ROW_SPACING) + g_Scorecard.y)
#define ROW_NAME_LEFT 98
/* A flap turns its far side to the screen past a quarter turn */
#define FLAP_EDGE_ON_ANGLE (M_PI / 2)
/* The board shades a flap by up to this much as it turns away from the light */
#define FLAP_SHADE_RANGE 63.0f
#define FLAP_LIT_SHADE 255.0f
#define FLAP_FACING_SHADE (FLAP_LIT_SHADE - FLAP_SHADE_RANGE - 1.0f)
#define FLAP_DISTANCE (-290.0f)
#define FLAP_NAME_X (-62)
#define FLAP_GLYPH_GROUP_COUNT 10
#define LEADERBOARD_FACE_LEFT 66
#define LEADERBOARD_FACE_TOP 60
/* An arrow pulses beside the player's row when their place has changed */
#define ARROW_IMAGE_SIZE 32
#define ARROW_CENTER_X 32
/* With G_TX_MIRROR, the texels below the image are the image upside down, which
 * a fall draws */
#define ARROW_MIRROR_MASK 5
#define ARROW_PULSE_SIZE 2.0f
#define FELL_ARROW_HALF_SIZE 8.0f
#define ROSE_ARROW_HALF_SIZE 12.0f
#define FELL_ARROW_PULSE_STEP 0.4f
#define ROSE_ARROW_PULSE_STEP 0.3f

/*
 * Draws the tournament leaderboard's page. Rows flip over in turn to show the
 * new standings, and the player's row shows their face and, if their place
 * changed, an arrow.
 *
 * Doesn't match: the target keeps displayListHead in a saved register, where
 * this spills it, so its frame is 0xE0 bytes, not 0xD8. The bottom flaps'
 * loop also reads the flip angles through the stagger's index instead of a
 * pointer of their own. Stepping the rows' tops beside the index gives 140
 * words differing, 54 in shape. 124 words differ, 48 in shape.
 */
#ifdef NON_MATCHING
void DrawLeaderboardPage(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  TournamentStanding* standing;
  s32 i;
  s32 shade;
  s32 group;
  s32 contestantId;
  f32 halfSize;
  s32 roundedHalfSize;
  s32 top;
  s32 texelStart;
  s32 texelStep;

  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  DrawScorecardLabel(&gfx, g_Scorecard.x + PLACE_LABEL_LEFT,
      LEADERBOARD_LABEL_TOP, PLACE_LABEL);
  DrawScorecardLabel(
      &gfx, g_Scorecard.x + NAME_LABEL_LEFT, LEADERBOARD_LABEL_TOP, NAME_LABEL);
  DrawScorecardLabel(&gfx, g_Scorecard.x + SCORE_LABEL_LEFT,
      LEADERBOARD_LABEL_TOP, SCORE_LABEL);
  DrawScorecardLabel(&gfx, g_Scorecard.x + HOLES_LABEL_LEFT,
      LEADERBOARD_LABEL_TOP, HOLES_LABEL);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.leaderboardRowImage));
  gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.leaderboardRowImage),
      G_IM_FMT_CI, ROW_IMAGE_WIDTH, ROW_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  gDPPipeSync(gfx++);

  /* The rows behind the flaps */
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime) {
      shade = 0xFF;
      if (g_Scorecard.content.isRowFlipping[i] == TRUE &&
          !(g_Scorecard.content.rowFlipAngles[i] > FLAP_EDGE_ON_ANGLE)) {
        shade =
            (sinf(g_Scorecard.content.rowFlipAngles[i]) * FLAP_SHADE_RANGE) +
            FLAP_FACING_SHADE;
      }
    } else {
      shade = 0xFF;
    }

    gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, 0xFF);
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + ROW_LEFT),
        QuarterPixels(
            ((i * LEADERBOARD_ROW_SPACING) + ROW_TOP) + g_Scorecard.y),
        QuarterPixels(g_Scorecard.x + ROW_LEFT + ROW_IMAGE_WIDTH),
        QuarterPixels((i * LEADERBOARD_ROW_SPACING) + ROW_TOP +
            ROW_IMAGE_HEIGHT + g_Scorecard.y),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  LoadLargePalette(&gfx, ImageBody(g_Scorecard.flapNameImages));
  gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    /* Until the flap has turned a radian */
    if (g_Scorecard.content.isRowFlipping[i] == TRUE &&
        (s32)g_Scorecard.content.rowFlipAngles[i] == 0) {
      LoadTextureBlock(&gfx,
          g_Scorecard.flapNameImages +
              ((i * FLAP_NAME_IMAGE_WIDTH * ROW_IMAGE_HEIGHT) +
                  LARGE_PALETTED_TEXELS_OFFSET),
          G_IM_FMT_CI, G_IM_SIZ_8b, FLAP_NAME_IMAGE_WIDTH, ROW_IMAGE_HEIGHT, 0,
          0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT),
          QuarterPixels(
              ((i * LEADERBOARD_ROW_SPACING) + ROW_TOP) + g_Scorecard.y),
          QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT + FLAP_NAME_IMAGE_WIDTH),
          QuarterPixels((i * LEADERBOARD_ROW_SPACING) +
              (ROW_IMAGE_HEIGHT + ROW_TOP) + g_Scorecard.y),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime) {
      FormatLeaderboardRow(
          g_PrintBuffer, &g_TournamentStandings[g_Scorecard.topRow + i]);
      DrawFixedWidthLargeText(
          &gfx, g_Scorecard.x + ROW_TEXT_LEFT, RowTextTop(i), g_PrintBuffer);
      contestantId = g_TournamentStandings[g_Scorecard.topRow + i].contestantId;
      if (contestantId == PLAYER_CONTESTANT) {
        sprintf(g_PrintBuffer, "%-8s  ", g_Players[0].name);
      } else {
        sprintf(g_PrintBuffer, f_StringFormat,
            g_ContestantNames[g_ContestantNameIndices[contestantId]]);
      }

      DrawLargeText(
          &gfx, g_Scorecard.x + ROW_NAME_LEFT, RowTextTop(i), g_PrintBuffer);
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gSPSetLights1(gfx++, f_FlapLights);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_LIGHTING | G_SHADING_SMOOTH);
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
        g_Scorecard.content.isRowFlipping[i] == TRUE) {
      Mat4 matrix;
      Vec3 translation;
      Vec3 rotation;

      /* Past a quarter turn, the flap turns a half turn more to show its far
       * side the right way up */
      if (g_Scorecard.content.rowFlipAngles[i] > FLAP_EDGE_ON_ANGLE) {
        rotation[AXIS_X] = g_Scorecard.content.rowFlipAngles[i] + M_PI;
        rotation[AXIS_Y] = 0;
        rotation[AXIS_Z] = 0;
      } else {
        rotation[AXIS_X] = g_Scorecard.content.rowFlipAngles[i];
        rotation[AXIS_Y] = 0;
        rotation[AXIS_Z] = 0;
      }

      translation[AXIS_X] = g_Scorecard.x;
      translation[AXIS_Y] = (SCREEN_HEIGHT / 2.0f) -
          ((i * LEADERBOARD_ROW_SPACING) + ROW_HINGE_TOP + g_Scorecard.y);
      translation[AXIS_Z] = FLAP_DISTANCE;
      BuildRotationMatrix(matrix, rotation, translation);
      ConvertMatrixToFixed(matrix, &f_FlapMatrices[i][g_MatrixBufferIndex]);
    }
  }

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gSPMatrix(gfx++, PhysicalAddress(&g_ScreenEffectProjection),
      G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.leaderboardRowImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.leaderboardRowImage),
      G_IM_FMT_CI, ROW_IMAGE_WIDTH, ROW_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  gDPPipeSync(gfx++);
  gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

  /* The bottom flaps, which a top flap shades as it falls over them */
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
        g_Scorecard.content.isRowFlipping[i] == TRUE) {
      if (g_Scorecard.content.rowFlipAngles[i] < FLAP_EDGE_ON_ANGLE) {
        shade = 0xFF;
      } else {
        shade = FLAP_LIT_SHADE -
            (sinf(g_Scorecard.content.rowFlipAngles[i] - FLAP_EDGE_ON_ANGLE) *
                FLAP_SHADE_RANGE);
      }

      gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, 0xFF);
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + ROW_LEFT),
          QuarterPixels((i * LEADERBOARD_ROW_SPACING) + ROW_TOP +
              ROW_BOTTOM_HALF_ROW + g_Scorecard.y),
          QuarterPixels(g_Scorecard.x + ROW_LEFT + ROW_IMAGE_WIDTH),
          QuarterPixels((i * LEADERBOARD_ROW_SPACING) +
              (ROW_IMAGE_HEIGHT + ROW_TOP) + g_Scorecard.y),
          G_TX_RENDERTILE, 0, TexelCoordinate(ROW_BOTTOM_HALF_ROW),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  LoadLargePalette(&gfx, ImageBody(g_Scorecard.flapNameImages));
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (g_Scorecard.content.isRowFlipping[i] == TRUE) {
      if (g_Scorecard.content.rowFlipAngles[i] < FLAP_EDGE_ON_ANGLE) {
        shade = 0xFF;
      } else {
        shade = FLAP_LIT_SHADE -
            (sinf(g_Scorecard.content.rowFlipAngles[i] - FLAP_EDGE_ON_ANGLE) *
                FLAP_SHADE_RANGE);
      }

      gDPSetPrimColor(gfx++, 0, 0, shade, shade, shade, 0xFF);
      LoadTextureBlock(&gfx,
          g_Scorecard.flapNameImages +
              ((i * FLAP_NAME_IMAGE_WIDTH * ROW_IMAGE_HEIGHT) +
                  LARGE_PALETTED_TEXELS_OFFSET),
          G_IM_FMT_CI, G_IM_SIZ_8b, FLAP_NAME_IMAGE_WIDTH, ROW_IMAGE_HEIGHT, 0,
          0, 0, 0, 0, 0, 0);
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT),
          QuarterPixels((i * LEADERBOARD_ROW_SPACING) + ROW_TOP +
              ROW_BOTTOM_HALF_ROW + g_Scorecard.y),
          QuarterPixels(g_Scorecard.x + FLAP_NAME_LEFT + FLAP_NAME_IMAGE_WIDTH),
          QuarterPixels((i * LEADERBOARD_ROW_SPACING) + ROW_TOP +
              ROW_IMAGE_HEIGHT + g_Scorecard.y),
          G_TX_RENDERTILE, 0, TexelCoordinate(ROW_BOTTOM_HALF_ROW),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  /* The top flaps, turning */
  gDPPipeSync(gfx++);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.leaderboardRowImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.leaderboardRowImage),
      G_IM_FMT_CI, ROW_IMAGE_WIDTH, ROW_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  gDPSetCombineMode(gfx++, G_CC_MODULATERGBA, G_CC_MODULATERGBA);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
        g_Scorecard.content.isRowFlipping[i] == TRUE) {
      gSPMatrix(gfx++, PhysicalAddress(&f_FlapMatrices[i][g_MatrixBufferIndex]),
          G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
      if (g_Scorecard.content.rowFlipAngles[i] > FLAP_EDGE_ON_ANGLE) {
        gSPVertex(
            gfx++, f_FlapVertices[FLAP_BOTTOM_QUAD], QUAD_VERTEX_COUNT, 0);
      } else {
        gSPVertex(gfx++, f_FlapVertices[FLAP_TOP_QUAD], QUAD_VERTEX_COUNT, 0);
      }

      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    }
  }

  LoadLargePalette(&gfx, ImageBody(g_Scorecard.flapNameImages));
  for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime &&
        g_Scorecard.content.isRowFlipping[i] == TRUE &&
        g_Scorecard.content.rowFlipAngles[i] < FLAP_EDGE_ON_ANGLE) {
      LoadTextureBlock(&gfx,
          g_Scorecard.flapNameImages +
              ((i * FLAP_NAME_IMAGE_WIDTH * ROW_IMAGE_HEIGHT) +
                  LARGE_PALETTED_TEXELS_OFFSET),
          G_IM_FMT_CI, G_IM_SIZ_8b, FLAP_NAME_IMAGE_WIDTH, ROW_IMAGE_HEIGHT, 0,
          0, 0, 0, 0, 0, 0);
      gSPMatrix(gfx++, PhysicalAddress(&f_FlapMatrices[i][g_MatrixBufferIndex]),
          G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
      gDPPipeSync(gfx++);
      gSPVertex(gfx++, f_FlapVertices[FLAP_NAME_QUAD], QUAD_VERTEX_COUNT, 0);
      gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
      gDPPipeSync(gfx++);
    }
  }

  /* The new standings, on the far sides of the flaps that are past a quarter
   * turn */
  gDPPipeSync(gfx++);
  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  SetUpPerspectiveTextDrawing(&gfx);
  for (i = 0, group = 0; i != LEADERBOARD_ROW_COUNT; i++) {
    if (i * ROW_FLIP_STAGGER < g_Scorecard.content.flipTime) {
      if (g_Scorecard.content.isRowFlipping[i] == TRUE &&
          g_Scorecard.content.rowFlipAngles[i] > FLAP_EDGE_ON_ANGLE) {
        standing = &g_TournamentStandings[g_Scorecard.topRow + i];
        FormatLeaderboardRow(g_PrintBuffer, standing);
        PlaceGlyphGroup(group, FONT_SIZE_LARGE,
            TEXT_FLAG_FIXED_WIDTH | GLYPH_FLAG_BOTTOM_PART, 0, 0,
            g_PrintBuffer);
        if (standing->contestantId == PLAYER_CONTESTANT) {
          sprintf(g_PrintBuffer, "%-8s", g_Players[0].name);
        } else {
          sprintf(g_PrintBuffer, f_StringFormat,
              g_ContestantNames
                  [g_ContestantNameIndices[standing->contestantId]]);
        }

        PlaceGlyphGroup(group + 1, FONT_SIZE_LARGE,
            GLYPH_FLAG_LEFT_ALIGNED | GLYPH_FLAG_BOTTOM_PART, FLAP_NAME_X, 0,
            g_PrintBuffer);
        gSPMatrix(gfx++,
            PhysicalAddress(&f_FlapMatrices[i][g_MatrixBufferIndex]),
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
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

  if (g_Scorecard.content.flipTime > ROW_FLIP_TIME) {
    for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
      if (g_TournamentStandings[g_Scorecard.topRow + i].contestantId ==
          PLAYER_CONTESTANT) {
        gSPDisplayList(gfx++, g_DecalTextureSetup);
        IN_OWN_BLOCK(gDPPipeSync(gfx++));
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        DrawSmallFace(&gfx, g_Scorecard.x + LEADERBOARD_FACE_LEFT,
            LEADERBOARD_FACE_TOP + (i * LEADERBOARD_ROW_SPACING), 0, TRUE);
      }
    }
  }

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  LoadNibbleTextureBlock(&gfx, ImageBody(g_Scorecard.placeArrowImage),
      G_IM_FMT_I, ARROW_IMAGE_SIZE, ARROW_IMAGE_SIZE, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_MIRROR | G_TX_WRAP, G_TX_NOMASK,
      ARROW_MIRROR_MASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPSetCombineLERP(gfx++, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, PRIMITIVE,
      0, TEXEL0, 0, ENVIRONMENT, PRIMITIVE, TEXEL0, PRIMITIVE, PRIMITIVE, 0,
      TEXEL0, 0);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  if (g_Scorecard.content.flipTime > ROW_FLIP_TIME) {
    for (i = 0; i != LEADERBOARD_ROW_COUNT; i++) {
      if (g_TournamentStandings[g_Scorecard.topRow + i].contestantId ==
              PLAYER_CONTESTANT &&
          g_Scorecard.placeChange != PLACE_HELD) {
        if (g_Scorecard.placeChange & PLACE_FELL) {
          gDPSetEnvColor(gfx++, 0xA0, 0xA0, 0xFF, 0xFF);
          gDPSetPrimColor(gfx++, 0, 0, 0x00, 0x40, 0xFF, 0xFF);
          gDPPipeSync(gfx++);
          f_ArrowPulseAngle += FELL_ARROW_PULSE_STEP;
          halfSize = (ARROW_PULSE_SIZE * sinf(f_ArrowPulseAngle)) +
              FELL_ARROW_HALF_SIZE;
        } else {
          gDPSetEnvColor(gfx++, 0xFF, 0x7F, 0x90, 0xFF);
          gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0x40, 0x40, 0xF0);
          gDPPipeSync(gfx++);
          f_ArrowPulseAngle += ROSE_ARROW_PULSE_STEP;
          halfSize = (ARROW_PULSE_SIZE * sinf(f_ArrowPulseAngle)) +
              ROSE_ARROW_HALF_SIZE;
        }

        roundedHalfSize = halfSize + 0.5f;
        top = (i * LEADERBOARD_ROW_SPACING) + ROW_HINGE_TOP + g_Scorecard.y;

        /* Starts the texels part of a texel in, as far as the rect's rounding
         * moved its edge */
        texelStart = (ARROW_IMAGE_SIZE / 2) -
            (((f32)roundedHalfSize - halfSize) * (ARROW_IMAGE_SIZE / 2));
        texelStep = (TEXEL_STEP_ONE * ARROW_IMAGE_SIZE / 2) / halfSize;
        DrawTextureRect(&gfx,
            QuarterPixels(g_Scorecard.x - (roundedHalfSize - ARROW_CENTER_X)),
            QuarterPixels(top - roundedHalfSize),
            QuarterPixels(g_Scorecard.x + ARROW_CENTER_X + roundedHalfSize),
            QuarterPixels(top + roundedHalfSize), G_TX_RENDERTILE, texelStart,
            texelStart +
                TexelCoordinate(g_Scorecard.placeChange * ARROW_IMAGE_SIZE),
            texelStep, texelStep);
      }
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard_drawing", DrawLeaderboardPage);
#endif

/*
 * The scores page: the front nine above the back nine, each with a row of
 * hole numbers, a row of pars and a row for each player
 */
/* Some tests compare it with FALSE rather than negate it, to match */
#define IsMatchPlayMode() \
  (g_GameState.gameMode == GAME_MODE_MATCH_GAME || \
      g_GameState.gameMode == GAME_MODE_GET_CHARACTER || \
      g_GameState.gameMode == GAME_MODE_SKINS_MATCH || \
      g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)
#define FRONT_NINE_TOP 57
#define BACK_NINE_TOP 137
/* Columns 0 to 7 of scoresPanelImage fill a panel, and columns 10 to 17 edge it
 */
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
#define LABEL_PANEL_STEP \
  (SCORES_PANEL_IMAGE_HEIGHT * TEXEL_STEP_ONE / LABEL_PANEL_HEIGHT)
#define TOTAL_PANEL_LEFT 244
#define TOTAL_PANEL_TOP 56
#define TOTAL_PANEL_BOTTOM 134
#define NINE_SPACING 80
#define MATCH_TOTAL_PANEL_RIGHT 288
#define FRONT_TOTAL_PANEL_RIGHT 262
#define BACK_TOTAL_PANEL_RIGHT 256
#define NET_PANEL_LEFT 264
#define NET_PANEL_RIGHT 290
/* The cells of a nine: a column for each hole, and a row each for its numbers,
 * its pars and four players */
#define CELL_LEFT 82
#define CELL_TOP 56
#define CELL_WIDTH 18
#define CELL_HEIGHT 13
#define CELL_ROW_COUNT 6
#define SCORE_ROW_COUNT 4
/* totalLabelsImage holds three labels, one above another: over a nine's total,
 * or the round's */
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
/* Faces of odd rows sit this far right, so that neighbors don't cover each
 * other */
#define SCORES_FACE_STAGGER 24
#define SCORES_FACE_RISE 14
#define HEADING_LEFT 48
#define PAR_ROW_TOP 12
#define PLAYER_ROW_TOP 27
#define PLAYER_ROW_SPACING 13
/* Leaves room for the putts and the strokes over par */
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
/* Codes of the shadowed font */
#define NO_SCORE_CODE 0x83
#define HOLE_WON_CODE 0x82
#define HOLE_LOST_CODE 0x85
#define HANDICAP_STROKE_CODE 0x91
#define BIRDIE_BADGE_CODE 0x81
/* The colors a score is drawn in, by how it stands to par */
#define PAR_COLOR 0
#define BIRDIE_COLOR 1
/* For a double bogey or worse */
#define DOUBLE_BOGEY_COLOR 2
/* For an eagle on a par five */
#define EAGLE_COLOR 3
#define BOGEY_COLOR 4
/* An eagle on a shorter hole, or better */
#define PULSING_COLOR 5
#define PULSE_STEP DegreesToRadians(20)
#define PULSE_RANGE 127
#define BADGE_GLOW_STEP DegreesToRadians(10)
#define BADGE_GLOW_RANGE 30

/*
 * Draws the scores page: the par and each player's score of every hole, in
 * the color of how it stands to par. In the match modes it shows who won each
 * hole instead, and in the skins modes the skins won. A single player also
 * sees their putts and strokes over par, and in a tournament the holes whose
 * birdie badge they hold.
 *
 * Doesn't match: the registers and stack slots differ throughout. The cell
 * loops add the column's offset to x after adding the cells' left edge, where
 * this adds it first, and the target keeps the address of
 * g_GameState.holeSelection in a saved register while it draws the faces.
 * 800 words differ, 244 in shape.
 */
#ifdef NON_MATCHING
void DrawScoresPage(Gfx** displayListHead) {
  s32 frontPar = 0;
  Gfx* gfx = *displayListHead;
  s8 handicapStrokes[SCORE_ROW_COUNT][COURSE_HOLE_COUNT];
  s32 firstHole = GetFirstHoleIndex();
  s32 frontTop = FRONT_NINE_TOP;
  s32 backTop = BACK_NINE_TOP;
  CourseRecord* course;
  s32 pulseRed;
  s32 pulseGreen;
  s32 pulseBlue;
  s32 cellAlpha;
  s32 rowSpacing;
  s32 backPar = 0;
  s32 standing;
  s32 colorIndex;
  s32 overPar;
  s32 extraTop;
  s32 overTop;
  s32 secondRow;
  s32 i;
  s32 cellRowIndex;
  s32 facePlayerIndex;
  s32 j;
  s32 cellColumn;
  s32 parHoleIndex;
  s32 hole;
  SaveData* saveData;
  s32 glow;
  Boolean isPuttsShown;
  Boolean isOverShown;
  Boolean isBadgesShown;

  f_ScorePulseAngle += PULSE_STEP;
  pulseRed = (s32)(sinf(f_ScorePulseAngle) * (f32)PULSE_RANGE) + PULSE_RANGE;
  pulseGreen =
      (s32)(sinf(f_ScorePulseAngle + M_PI) * (f32)PULSE_RANGE) + PULSE_RANGE;
  pulseBlue = (s32)(sinf(f_ScorePulseAngle + (M_PI / 2)) * (f32)PULSE_RANGE) +
      PULSE_RANGE;
  course = &g_CourseRecords[GetCourseRecordIndex()];

  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
  switch (f_ScoresStyle) {
    case SCORES_STYLE_YELLOW:
      gDPSetEnvColor(gfx++, 0xFF, 0xF0, 0x7F, 0xFF);
      gDPSetBlendColor(gfx++, 0, 0, 0, 0x80);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
      gDPSetCombineLERP(gfx++, PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0,
          PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, 0, TEXEL0, 0);
      cellAlpha = 0xFF;
      break;
    case SCORES_STYLE_FADED:
      cellAlpha = 0x9F;
      gDPSetEnvColor(gfx++, 0, 0x3F, 0, 0xFF);
      gDPSetBlendColor(gfx++, 0, 0, 0, 0x40);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0,
          0, 0, PRIMITIVE, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0,
          PRIMITIVE);
      break;
    case SCORES_STYLE_OPAQUE:
      /* The cells are opaque, so the original leaves their alpha unset */
#ifdef AVOID_UB
      cellAlpha = 0xFF;
#endif
      gDPSetEnvColor(gfx++, 0, 0x1F, 0, 0xFF);
      gDPSetBlendColor(gfx++, 0, 0, 0, 0x40);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
      gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0,
          0, 0, TEXEL0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT, 0, 0, 0,
          TEXEL0);
      break;
#ifdef AVOID_UB
    default:
#endif
    case SCORES_STYLE_TINTED:
      gDPSetEnvColor(gfx++, 0, 0x40, 0, 0xFF);
      gDPSetBlendColor(gfx++, 0, 0, 0, 0x1F);
      gDPPipeSync(gfx++);
      gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
      gDPSetCombineLERP(gfx++, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT,
          PRIMITIVE, 0, TEXEL0, 0, PRIMITIVE, ENVIRONMENT, TEXEL0, ENVIRONMENT,
          PRIMITIVE, 0, TEXEL0, 0);
      cellAlpha = 0xFF;
      break;
  }

  /* The panels behind the headings and the totals */
  gDPSetPrimColor(gfx++, 0, 0, 0, CELL_SHADE, 0, cellAlpha);
  LoadNibbleTextureBlock(&gfx, ImageBody(g_Scorecard.scoresPanelImage),
      G_IM_FMT_I, SCORES_PANEL_IMAGE_WIDTH, SCORES_PANEL_IMAGE_HEIGHT, 0,
      G_TX_CLAMP, 0, 0, 0, 0, 0);
  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, PANEL_TEXEL_RIGHT << 2,
      (SCORES_PANEL_IMAGE_HEIGHT - 1) << 2);
  gDPPipeSync(gfx++);
  if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + LABEL_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP),
        QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + LABEL_PANEL_HEIGHT),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, LABEL_PANEL_STEP);
  }

  if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + LABEL_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING +
            LABEL_PANEL_HEIGHT),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, LABEL_PANEL_STEP);
  }

  if (IsMatchPlayMode()) {
    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
          QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else {
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
          QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM), G_TX_RENDERTILE, 0,
          0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  } else if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
        QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM), G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  } else {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
        QuarterPixels(g_Scorecard.x + FRONT_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM), G_TX_RENDERTILE, 0,
        0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + TOTAL_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + BACK_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, PANEL_EDGE_TEXEL_RIGHT << 2,
      (SCORES_PANEL_IMAGE_HEIGHT - 1) << 2);
  if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP),
        QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + LABEL_PANEL_HEIGHT),
        G_TX_RENDERTILE, TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0,
        TEXEL_STEP_ONE, LABEL_PANEL_STEP);
  }

  if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + LABEL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + LABEL_PANEL_TOP + NINE_SPACING +
            LABEL_PANEL_HEIGHT),
        G_TX_RENDERTILE, TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0,
        TEXEL_STEP_ONE, LABEL_PANEL_STEP);
  }

  if (IsMatchPlayMode()) {
    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
      DrawTextureRect(&gfx,
          QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
          QuarterPixels(
              g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
          G_TX_RENDERTILE, TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0,
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else {
      DrawTextureRect(&gfx,
          QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
          QuarterPixels(
              g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
          QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM), G_TX_RENDERTILE,
          TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0, TEXEL_STEP_ONE,
          TEXEL_STEP_ONE);
    }
  } else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(
            g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
        G_TX_RENDERTILE, TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  } else if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
        QuarterPixels(
            g_Scorecard.x + MATCH_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM), G_TX_RENDERTILE,
        TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0, TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
  } else {
    DrawTextureRect(&gfx,
        QuarterPixels(g_Scorecard.x + FRONT_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP),
        QuarterPixels(
            g_Scorecard.x + FRONT_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM), G_TX_RENDERTILE,
        TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0, TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + BACK_TOTAL_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(
            g_Scorecard.x + BACK_TOTAL_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
        G_TX_RENDERTILE, TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  /* Two players in stroke play take every other row, so their handicap strokes
   * do too */
  for (i = 0; i != SCORE_ROW_COUNT; i++) {
    for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
      handicapStrokes[i][hole] = 0;
    }
  }

  if (g_GameState.playerCount == 2 && !IsMatchPlayMode()) {
    for (i = 0; i != g_GameState.playerCount; i++) {
      for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
        handicapStrokes[i * 2][hole] = GetHandicapOffset(
            GetCourseRecordIndex(), hole, g_Players[i].handicap);
      }
    }
  } else {
    for (i = 0; i != g_GameState.playerCount; i++) {
      for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
        handicapStrokes[i][hole] = GetHandicapOffset(
            GetCourseRecordIndex(), hole, g_Players[i].handicap);
      }
    }
  }

  /* The cells, in a checkerboard of two shades of each row's color */
  for (i = 0; i != CELL_ROW_COUNT; i++) {
    for (j = 0; j != HOLES_PER_NINE; j++) {
      gDPPipeSync(gfx++);
      if (i & 1) {
        gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0xBF, 0, cellAlpha);
      } else {
        gDPSetPrimColor(gfx++, 0, 0, 0, CELL_SHADE, 0, cellAlpha);
      }

      gDPPipeSync(gfx++);
      if (j & 1) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
          DrawTextureRect(&gfx,
              QuarterPixels(((j * CELL_WIDTH) + CELL_LEFT) + g_Scorecard.x),
              QuarterPixels(g_Scorecard.y + (CELL_TOP + (i * CELL_HEIGHT))),
              QuarterPixels(
                  (j * CELL_WIDTH) + (CELL_LEFT + CELL_WIDTH) + g_Scorecard.x),
              QuarterPixels(
                  g_Scorecard.y + (i * CELL_HEIGHT) + CELL_TOP + CELL_HEIGHT),
              G_TX_RENDERTILE, 0, TexelCoordinate(i * CELL_HEIGHT),
              TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }

        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
          DrawTextureRect(&gfx,
              QuarterPixels((j * CELL_WIDTH) + CELL_LEFT + g_Scorecard.x),
              QuarterPixels(
                  g_Scorecard.y + (i * CELL_HEIGHT) + CELL_TOP + NINE_SPACING),
              QuarterPixels(
                  g_Scorecard.x + (j * CELL_WIDTH) + CELL_LEFT + CELL_WIDTH),
              QuarterPixels(g_Scorecard.y + (i * CELL_HEIGHT) + CELL_TOP +
                  NINE_SPACING + CELL_HEIGHT),
              G_TX_RENDERTILE, 0, TexelCoordinate(i * CELL_HEIGHT),
              TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
      }
    }
  }

  gDPPipeSync(gfx++);
  LoadNibbleTextureBlock(&gfx, ImageBody(g_Scorecard.scoresPanelImage),
      G_IM_FMT_I, SCORES_PANEL_IMAGE_WIDTH, SCORES_PANEL_IMAGE_HEIGHT, 0, 0, 0,
      0, 0, 0, 0);
  gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0x1F, cellAlpha);
  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, PANEL_TEXEL_RIGHT << 2,
      (SCORES_PANEL_IMAGE_HEIGHT - 1) << 2);
  gDPPipeSync(gfx++);
  if (IsMatchPlayMode() == FALSE &&
      !(g_GameState.holeSelection &
          (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + NET_PANEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + NET_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
        G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  gDPPipeSync(gfx++);
  gDPSetTileSize(gfx++, G_TX_RENDERTILE, 0, 0, PANEL_EDGE_TEXEL_RIGHT << 2,
      (SCORES_PANEL_IMAGE_HEIGHT - 1) << 2);
  gDPPipeSync(gfx++);
  if (IsMatchPlayMode() == FALSE &&
      !(g_GameState.holeSelection &
          (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + NET_PANEL_RIGHT),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + NET_PANEL_RIGHT + PANEL_EDGE_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_PANEL_BOTTOM + NINE_SPACING),
        G_TX_RENDERTILE, TexelCoordinate(PANEL_EDGE_TEXEL_LEFT), 0,
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  }

  for (cellRowIndex = 0; cellRowIndex != CELL_ROW_COUNT; cellRowIndex++) {
    if (cellRowIndex & 1) {
      gDPSetPrimColor(gfx++, 0, 0, 0xBF, 0xFF, 0, cellAlpha);
    } else {
      gDPSetPrimColor(gfx++, 0, 0, 0x7F, 0xFF, 0, cellAlpha);
    }

    for (cellColumn = 0; cellColumn != HOLES_PER_NINE; cellColumn++) {
      if (!(cellColumn & 1)) {
        if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
          DrawTextureRect(&gfx,
              QuarterPixels(
                  g_Scorecard.x + (cellColumn * CELL_WIDTH) + CELL_LEFT),
              QuarterPixels(
                  g_Scorecard.y + (cellRowIndex * CELL_HEIGHT) + CELL_TOP),
              QuarterPixels(g_Scorecard.x + (cellColumn * CELL_WIDTH) +
                  CELL_LEFT + CELL_WIDTH),
              QuarterPixels(g_Scorecard.y + (cellRowIndex * CELL_HEIGHT) +
                  CELL_TOP + CELL_HEIGHT),
              G_TX_RENDERTILE, 0, TexelCoordinate(cellRowIndex * CELL_HEIGHT),
              TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }

        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
          DrawTextureRect(&gfx,
              QuarterPixels(
                  ((cellColumn * CELL_WIDTH) + CELL_LEFT) + g_Scorecard.x),
              QuarterPixels((cellRowIndex * CELL_HEIGHT) +
                  (CELL_TOP + NINE_SPACING) + g_Scorecard.y),
              QuarterPixels((cellColumn * CELL_WIDTH) +
                  (CELL_LEFT + CELL_WIDTH) + g_Scorecard.x),
              QuarterPixels((cellRowIndex * CELL_HEIGHT) + CELL_HEIGHT +
                  CELL_TOP + NINE_SPACING + g_Scorecard.y),
              G_TX_RENDERTILE, 0, TexelCoordinate(cellRowIndex * CELL_HEIGHT),
              TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
      }
    }
  }

  /* The labels over the totals */
  gDPPipeSync(gfx++);
  gSPDisplayList(gfx++, g_DecalTextureSetup);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.totalLabelsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.totalLabelsImage),
      G_IM_FMT_CI, TOTAL_LABEL_WIDTH, TOTAL_LABELS_IMAGE_HEIGHT, 0, G_TX_MIRROR,
      0, 5, 0, 0, 0);
  if (IsMatchPlayMode()) {
    if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
          QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP),
          QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
          QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + TOTAL_LABEL_HEIGHT),
          G_TX_RENDERTILE, 0, TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    } else {
      DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
          QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
          QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
          QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING +
              TOTAL_LABEL_HEIGHT),
          G_TX_RENDERTILE, 0, TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  } else if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP),
        QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + TOTAL_LABEL_HEIGHT),
        G_TX_RENDERTILE, 0, TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  } else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING +
            TOTAL_LABEL_HEIGHT),
        G_TX_RENDERTILE, 0, TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  } else {
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + FRONT_NINE_LABEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP),
        QuarterPixels(
            g_Scorecard.x + FRONT_NINE_LABEL_LEFT + TOTAL_LABEL_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + TOTAL_LABEL_HEIGHT),
        G_TX_RENDERTILE, 0,
        TexelCoordinate(FRONT_NINE_LABEL * TOTAL_LABEL_HEIGHT), TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + BACK_NINE_LABEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + BACK_NINE_LABEL_LEFT + TOTAL_LABEL_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING +
            TOTAL_LABEL_HEIGHT),
        G_TX_RENDERTILE, 0,
        TexelCoordinate(BACK_NINE_LABEL * TOTAL_LABEL_HEIGHT), TEXEL_STEP_ONE,
        TEXEL_STEP_ONE);
    DrawTextureRect(&gfx, QuarterPixels(g_Scorecard.x + NET_ROUND_LABEL_LEFT),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING),
        QuarterPixels(g_Scorecard.x + NET_ROUND_LABEL_LEFT + TOTAL_LABEL_WIDTH),
        QuarterPixels(g_Scorecard.y + TOTAL_LABEL_TOP + NINE_SPACING +
            TOTAL_LABEL_HEIGHT),
        G_TX_RENDERTILE, 0, TexelCoordinate(ROUND_LABEL * TOTAL_LABEL_HEIGHT),
        TEXEL_STEP_ONE, TEXEL_STEP_ONE);
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
      } else {
        secondRow = 2;
        rowSpacing = WIDE_PLAYER_ROW_SPACING;
      }

      if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
        DrawSmallFace(&gfx, g_Scorecard.x + SCORES_FACE_LEFT,
            PLAYER_ROW_TOP - SCORES_FACE_RISE + (frontTop + g_Scorecard.y), 0,
            TRUE);
        DrawSmallFace(&gfx,
            ((secondRow & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT +
                g_Scorecard.x,
            PLAYER_ROW_TOP - SCORES_FACE_RISE +
                (frontTop + (secondRow * PLAYER_ROW_SPACING) + g_Scorecard.y),
            1, TRUE);
      }

      if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
        DrawSmallFace(&gfx, g_Scorecard.x + SCORES_FACE_LEFT,
            PLAYER_ROW_TOP - SCORES_FACE_RISE + (backTop + g_Scorecard.y), 0,
            TRUE);
        DrawSmallFace(&gfx,
            ((secondRow & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT +
                g_Scorecard.x,
            PLAYER_ROW_TOP - SCORES_FACE_RISE +
                (backTop + (secondRow * PLAYER_ROW_SPACING) + g_Scorecard.y),
            1, TRUE);
      }

      break;
#ifdef AVOID_UB
    default:
#endif
    case 1:
    case 3:
    case 4:
      rowSpacing = PLAYER_ROW_SPACING;
      for (facePlayerIndex = 0; facePlayerIndex != g_GameState.playerCount;
          facePlayerIndex++) {
        LoadLargePalette(&gfx, g_PlayerFacePalettes[facePlayerIndex]);
        LoadTextureBlock(&gfx, g_PlayerFaceImages[facePlayerIndex], G_IM_FMT_CI,
            G_IM_SIZ_8b, FACE_SIZE, SMALL_FACE_HEIGHT, 0, G_TX_MIRROR, 0,
            MIRRORED_FACE_MASK, 0, 0, 0);
        if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
          DrawSmallFace(&gfx,
              ((facePlayerIndex & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT +
                  g_Scorecard.x,
              PLAYER_ROW_TOP - SCORES_FACE_RISE + g_Scorecard.y +
                  ((facePlayerIndex * PLAYER_ROW_SPACING) + frontTop),
              facePlayerIndex, TRUE);
        }

        if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
          DrawSmallFace(&gfx,
              ((facePlayerIndex & 1) * SCORES_FACE_STAGGER) + SCORES_FACE_LEFT +
                  g_Scorecard.x,
              PLAYER_ROW_TOP - SCORES_FACE_RISE + g_Scorecard.y +
                  ((facePlayerIndex * PLAYER_ROW_SPACING) + backTop),
              facePlayerIndex, TRUE);
        }
      }

      break;
  }

  SetTextColor(&gfx, 0xBF, 0xFF, 0x7F);
  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 0xFF, 0xBF, 0);
  if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
    DrawLargeText(
        &gfx, g_Scorecard.x + HEADING_LEFT, frontTop + g_Scorecard.y, "NO.");
    DrawLargeText(&gfx, g_Scorecard.x + HEADING_LEFT,
        frontTop + PAR_ROW_TOP + g_Scorecard.y, "PAR");
  }

  if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
    DrawLargeText(
        &gfx, g_Scorecard.x + HEADING_LEFT, backTop + g_Scorecard.y, "NO.");
    DrawLargeText(&gfx, g_Scorecard.x + HEADING_LEFT,
        backTop + PAR_ROW_TOP + g_Scorecard.y, "PAR");
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
    } else {
      isPuttsShown = TRUE;
    }
  }

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  for (i = 0; i != g_GameState.playerCount; i++) {
    if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
      extraTop = EXTRA_ROW_TOP;
      if (isPuttsShown) {
        extraTop += PLAYER_ROW_SPACING;
        DrawSmallText(&gfx, g_Scorecard.x + PUTTS_LABEL_LEFT,
            frontTop + (EXTRA_ROW_TOP + (i * rowSpacing)) + g_Scorecard.y,
            "Putts");
      }

      if (isOverShown) {
        DrawSmallText(&gfx, g_Scorecard.x + OVER_LABEL_LEFT,
            frontTop + extraTop + (i * rowSpacing) + g_Scorecard.y, "Over");
        extraTop += PLAYER_ROW_SPACING;
      }

      if (isBadgesShown) {
        DrawSmallText(&gfx, g_Scorecard.x + BADGES_LABEL_LEFT,
            frontTop + extraTop + (i * rowSpacing) + g_Scorecard.y, "Badges");
      }
    }

    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
      extraTop = EXTRA_ROW_TOP;
      if (isPuttsShown) {
        extraTop += PLAYER_ROW_SPACING;
        DrawSmallText(&gfx, g_Scorecard.x + PUTTS_LABEL_LEFT,
            backTop + (EXTRA_ROW_TOP + (i * rowSpacing)) + g_Scorecard.y,
            "Putts");
      }

      if (isOverShown) {
        DrawSmallText(&gfx, g_Scorecard.x + OVER_LABEL_LEFT,
            backTop + extraTop + (i * rowSpacing) + g_Scorecard.y, "Over");
        extraTop += PLAYER_ROW_SPACING;
      }

      if (isBadgesShown) {
        DrawSmallText(&gfx, g_Scorecard.x + BADGES_LABEL_LEFT,
            backTop + extraTop + (i * rowSpacing) + g_Scorecard.y, "Badges");
      }
    }
  }

  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 0xFF, 0xFF, 0);
  for (j = 0; j != HOLES_PER_NINE; j++) {
    if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
      sprintf(g_PrintBuffer, f_TwoDigitFormat, j + 1);
      DrawCondensedShadowedText(&gfx,
          (j * CELL_WIDTH) + HOLE_NUMBERS_LEFT + g_Scorecard.x,
          frontTop + HOLE_NUMBERS_TOP + g_Scorecard.y, g_PrintBuffer);
    }

    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
      sprintf(g_PrintBuffer, f_TwoDigitFormat, j + HOLES_PER_NINE + 1);
      DrawCondensedShadowedText(&gfx,
          (j * CELL_WIDTH) + BACK_HOLE_NUMBERS_LEFT + g_Scorecard.x,
          backTop + HOLE_NUMBERS_TOP + g_Scorecard.y, g_PrintBuffer);
    }
  }

  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  for (parHoleIndex = 0; parHoleIndex != HOLES_PER_NINE; parHoleIndex++) {
    if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
      frontPar += course->holes[parHoleIndex].par;
      sprintf(g_PrintBuffer, f_DecimalFormat, course->holes[parHoleIndex].par);
      DrawCondensedShadowedText(&gfx,
          (parHoleIndex * CELL_WIDTH) + PAR_LEFT + g_Scorecard.x,
          frontTop + PAR_TOP + g_Scorecard.y, g_PrintBuffer);
    }

    if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
      backPar += course->holes[parHoleIndex + HOLES_PER_NINE].par;
      sprintf(g_PrintBuffer, f_DecimalFormat,
          course->holes[parHoleIndex + HOLES_PER_NINE].par);
      DrawCondensedShadowedText(&gfx,
          (parHoleIndex * CELL_WIDTH) + PAR_LEFT + g_Scorecard.x,
          backTop + PAR_TOP + g_Scorecard.y, g_PrintBuffer);
    }
  }

  if (IsMatchPlayMode() == FALSE &&
      !(g_GameState.holeSelection &
          (HOLE_SELECTION_BACK_NINE | HOLE_SELECTION_FRONT_NINE))) {
    sprintf(g_PrintBuffer, "%3d", frontPar);
    DrawShadowedText(&gfx, g_Scorecard.x + FRONT_PAR_TOTAL_LEFT,
        frontTop + PAR_TOP + g_Scorecard.y, g_PrintBuffer);
    sprintf(g_PrintBuffer, "%3d", backPar);
    DrawShadowedText(&gfx, g_Scorecard.x + BACK_PAR_TOTAL_LEFT,
        backTop + PAR_TOP + g_Scorecard.y, g_PrintBuffer);
  }

  SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
  for (i = 0; i != g_GameState.playerCount; i++) {
    s32 shownHoleCount;
    s32 backTotal;
    s32 frontTotal;

    standing = 0;
    colorIndex = PAR_COLOR;
    backTotal = 0;
    frontTotal = 0;
    shownHoleCount = g_Scorecard.content.holeIndex;
    if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
      /* A hole shows once both players have finished it */
      if (g_Players[i].holeStatus != HOLE_STATUS_PLAYING) {
        shownHoleCount = g_Scorecard.content.holeIndex +
            (g_Players[i ^ 1].holeStatus != HOLE_STATUS_PLAYING);
      }

      SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
      for (hole = firstHole; hole != shownHoleCount; hole++) {
        if (g_Players[i].holes[hole].strokes ==
            g_Players[i ^ 1].holes[hole].strokes) {
          g_PrintBuffer[0] = NO_SCORE_CODE;
        } else if (g_Players[i].holes[hole].strokes <
            g_Players[i ^ 1].holes[hole].strokes) {
          g_PrintBuffer[0] = HOLE_WON_CODE;
          standing++;
        } else {
          g_PrintBuffer[0] = HOLE_LOST_CODE;
          standing--;
        }

        sprintf(g_PrintBuffer + 1, "");
        if (hole < HOLES_PER_NINE) {
          DrawShadowedText(&gfx,
              (hole * CELL_WIDTH) + MATCH_MARK_LEFT + g_Scorecard.x,
              frontTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
              g_PrintBuffer);
        } else {
          DrawShadowedText(&gfx,
              g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                  MATCH_MARK_LEFT,
              backTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
              g_PrintBuffer);
        }
      }

      if (standing == 0) {
        sprintf(g_PrintBuffer, " EVEN");
      } else if (standing > 0) {
        sprintf(g_PrintBuffer, "%2dUP", standing);
      } else {
        sprintf(g_PrintBuffer, "%2dDOWN", -standing);
      }

      if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
        DrawShadowedText(&gfx, g_Scorecard.x + MATCH_STANDING_LEFT,
            frontTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
            g_PrintBuffer);
      } else {
        DrawShadowedText(&gfx, g_Scorecard.x + MATCH_STANDING_LEFT,
            (i * rowSpacing) + backTop + (PLAYER_ROW_TOP + g_Scorecard.y),
            g_PrintBuffer);
      }
    } else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
        g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
      /* Strokes here count the skins won on the hole */
      SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
      for (hole = firstHole; hole != shownHoleCount; hole++) {
        if (g_Players[i].holes[hole].strokes == 0) {
          sprintf(g_PrintBuffer, "%c", NO_SCORE_CODE);
          if (hole < HOLES_PER_NINE) {
            DrawShadowedText(&gfx,
                g_Scorecard.x + (hole * CELL_WIDTH) + SKINS_MARK_LEFT,
                frontTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
                g_PrintBuffer);
          } else {
            DrawShadowedText(&gfx,
                g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                    SKINS_MARK_LEFT,
                backTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
                g_PrintBuffer);
          }
        } else {
          sprintf(g_PrintBuffer, f_TwoDigitFormat,
              g_Players[i].holes[hole].strokes);
          if (hole < HOLES_PER_NINE) {
            DrawShadowedText(&gfx,
                g_Scorecard.x + (hole * CELL_WIDTH) + SKINS_WON_LEFT,
                frontTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
                g_PrintBuffer);
          } else {
            DrawShadowedText(&gfx,
                g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                    SKINS_WON_LEFT,
                backTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
                g_PrintBuffer);
          }

          standing += g_Players[i].holes[hole].strokes;
        }
      }

      sprintf(g_PrintBuffer, "%2dP", standing);
      if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
        DrawShadowedText(&gfx, g_Scorecard.x + SKINS_TOTAL_LEFT,
            (i * rowSpacing) + frontTop + (PLAYER_ROW_TOP + g_Scorecard.y),
            g_PrintBuffer);
      } else {
        DrawShadowedText(&gfx, g_Scorecard.x + SKINS_TOTAL_LEFT,
            backTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
            g_PrintBuffer);
      }
    } else {
      shownHoleCount += g_Players[i].holeStatus != HOLE_STATUS_PLAYING;
      for (hole = firstHole; hole != shownHoleCount; hole++) {
        overPar = g_Players[i].holes[hole].strokes - course->holes[hole].par;
        if (hole < HOLES_PER_NINE) {
          frontTotal += g_Players[i].holes[hole].strokes;
        } else {
          backTotal += g_Players[i].holes[hole].strokes;
        }

        if ((overPar < -2 && course->holes[hole].par == PAR_FIVE) ||
            (overPar < -1 && course->holes[hole].par != PAR_FIVE)) {
          if (colorIndex != PULSING_COLOR) {
            colorIndex = PULSING_COLOR;
            SetTextColor(&gfx, pulseRed, pulseGreen, pulseBlue);
          }
        } else if (overPar < -1) {
          if (colorIndex != EAGLE_COLOR) {
            colorIndex = EAGLE_COLOR;
            SetTextColor(&gfx, 0xFF, 0, 0);
          }
        } else if (overPar < 0) {
          if (colorIndex != BIRDIE_COLOR) {
            colorIndex = BIRDIE_COLOR;
            SetTextColor(&gfx, 0xFF, 0x7F, 0x1F);
          }
        } else if (overPar == 0) {
          if (colorIndex != PAR_COLOR) {
            colorIndex = PAR_COLOR;
            SetTextColor(&gfx, 0xFF, 0xFF, 0xFF);
          }
        } else if (overPar == 1) {
          if (colorIndex != BOGEY_COLOR) {
            colorIndex = BOGEY_COLOR;
            SetTextColor(&gfx, 0x7F, 0xFF, 0x7F);
          }
        } else if (colorIndex != DOUBLE_BOGEY_COLOR) {
          colorIndex = DOUBLE_BOGEY_COLOR;
          SetTextColor(&gfx, 0x9F, 0xBF, 0xFF);
        }

        sprintf(
            g_PrintBuffer, f_TwoDigitFormat, g_Players[i].holes[hole].strokes);
        if (hole < HOLES_PER_NINE) {
          DrawCondensedShadowedText(&gfx,
              g_Scorecard.x + (hole * CELL_WIDTH) + STROKES_LEFT,
              frontTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
              g_PrintBuffer);
        } else {
          DrawCondensedShadowedText(&gfx,
              g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                  STROKES_LEFT,
              backTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
              g_PrintBuffer);
        }

        overTop = PUTTS_TOP;
        if (isPuttsShown) {
          if (g_Players[i].holes[hole].putts == UNCOUNTED_PUTTS) {
            sprintf(g_PrintBuffer, "%c", NO_SCORE_CODE);
            overTop += PLAYER_ROW_SPACING;
            if (hole < HOLES_PER_NINE) {
              DrawShadowedText(&gfx,
                  g_Scorecard.x + (hole * CELL_WIDTH) + PUTTS_LEFT,
                  frontTop + (i * rowSpacing) + PUTTS_TOP + g_Scorecard.y,
                  g_PrintBuffer);
            } else {
              DrawShadowedText(&gfx,
                  g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                      PUTTS_LEFT,
                  backTop + (i * rowSpacing) + PUTTS_TOP + g_Scorecard.y,
                  g_PrintBuffer);
            }
          } else {
            sprintf(g_PrintBuffer, f_TwoDigitFormat,
                g_Players[i].holes[hole].putts);
            if (hole < HOLES_PER_NINE) {
              overTop += PLAYER_ROW_SPACING;
              DrawCenteredNarrowText(&gfx,
                  g_Scorecard.x + (hole * CELL_WIDTH) + PUTTS_LEFT,
                  frontTop + (i * rowSpacing) + PUTTS_TOP + g_Scorecard.y,
                  g_PrintBuffer);
            } else {
              overTop += PLAYER_ROW_SPACING;
              DrawCenteredNarrowText(&gfx,
                  g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                      PUTTS_LEFT,
                  backTop + (i * rowSpacing) + PUTTS_TOP + g_Scorecard.y,
                  g_PrintBuffer);
            }
          }
        }

        if (isOverShown) {
          if (overPar == 0) {
            sprintf(g_PrintBuffer, "<0");
          } else if (overPar > 0) {
            sprintf(g_PrintBuffer, ":%d", overPar);
          } else {
            sprintf(g_PrintBuffer, ";%d", -overPar);
          }

          if (hole < HOLES_PER_NINE) {
            DrawCenteredNarrowText(&gfx,
                g_Scorecard.x + (hole * CELL_WIDTH) + OVER_LEFT,
                frontTop + overTop + (i * rowSpacing) + g_Scorecard.y,
                g_PrintBuffer);
          } else {
            DrawCenteredNarrowText(&gfx,
                g_Scorecard.x + ((hole - HOLES_PER_NINE) * CELL_WIDTH) +
                    OVER_LEFT,
                backTop + (i * rowSpacing) + overTop + g_Scorecard.y,
                g_PrintBuffer);
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
          DrawCondensedShadowedText(&gfx, g_Scorecard.x + FRONT_TOTAL_LEFT,
              frontTop + (i * rowSpacing) + (PLAYER_ROW_TOP + g_Scorecard.y),
              g_PrintBuffer);
        }

        if (shownHoleCount > HOLES_PER_NINE) {
          sprintf(g_PrintBuffer, "%3d", backTotal);
          DrawCondensedShadowedText(&gfx, g_Scorecard.x + BACK_TOTAL_LEFT,
              backTop + (i * rowSpacing) + (PLAYER_ROW_TOP + g_Scorecard.y),
              g_PrintBuffer);
        }
      }

      overPar = GetNetScoreToPar(g_Scorecard.content.holeIndex, i);
      if (overPar == 0) {
        sprintf(g_PrintBuffer, f_ZeroToParFormat);
      } else {
        sprintf(g_PrintBuffer, f_ScoreToParFormat, overPar);
      }

      if (g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE) {
        DrawShadowedText(&gfx, g_Scorecard.x + NET_SCORE_LEFT,
            frontTop + (i * rowSpacing) + (PLAYER_ROW_TOP + g_Scorecard.y),
            g_PrintBuffer);
      } else if (g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE) {
        DrawShadowedText(&gfx, g_Scorecard.x + NET_SCORE_LEFT,
            backTop + (i * rowSpacing) + PLAYER_ROW_TOP + g_Scorecard.y,
            g_PrintBuffer);
      } else {
        DrawCondensedShadowedText(&gfx,
            g_Scorecard.x + CONDENSED_NET_SCORE_LEFT,
            backTop + (i * rowSpacing) + (PLAYER_ROW_TOP + g_Scorecard.y),
            g_PrintBuffer);
      }
    }
  }

  /* A mark for each stroke a player's handicap gives them on a hole */
  sprintf(g_PrintBuffer, "%c", HANDICAP_STROKE_CODE);
  for (i = 0; i != SCORE_ROW_COUNT; i++) {
    for (j = 0; j != HOLES_PER_NINE; j++) {
      if (!(g_GameState.holeSelection & HOLE_SELECTION_BACK_NINE)) {
        if (handicapStrokes[i][j] < 0) {
          DrawShadowedText(&gfx,
              g_Scorecard.x + (j * CELL_WIDTH) + HANDICAP_STROKE_LEFT,
              frontTop + (i * PLAYER_ROW_SPACING) + HANDICAP_STROKE_TOP +
                  g_Scorecard.y,
              g_PrintBuffer);
        }

        if (handicapStrokes[i][j] < -1) {
          DrawShadowedText(&gfx,
              g_Scorecard.x + (j * CELL_WIDTH) + SECOND_HANDICAP_STROKE_LEFT,
              frontTop + (i * PLAYER_ROW_SPACING) + HANDICAP_STROKE_TOP +
                  g_Scorecard.y,
              g_PrintBuffer);
        }
      }

      if (!(g_GameState.holeSelection & HOLE_SELECTION_FRONT_NINE)) {
        if (handicapStrokes[i][j + HOLES_PER_NINE] < 0) {
          DrawShadowedText(&gfx,
              g_Scorecard.x + (j * CELL_WIDTH) + HANDICAP_STROKE_LEFT,
              backTop + (i * PLAYER_ROW_SPACING) + HANDICAP_STROKE_TOP +
                  g_Scorecard.y,
              g_PrintBuffer);
        }

        if (handicapStrokes[i][j + HOLES_PER_NINE] < -1) {
          DrawShadowedText(&gfx,
              g_Scorecard.x + (j * CELL_WIDTH) + SECOND_HANDICAP_STROKE_LEFT,
              backTop + (i * PLAYER_ROW_SPACING) + HANDICAP_STROKE_TOP +
                  g_Scorecard.y,
              g_PrintBuffer);
        }
      }
    }
  }

  if (isBadgesShown) {
    saveData = GetSaveData();
    f_BadgeGlowAngle += BADGE_GLOW_STEP;
    glow = sinf(f_BadgeGlowAngle) * (f32)BADGE_GLOW_RANGE;
    gDPSetPrimColor(gfx++, 0, 0, glow + 0xE0, glow + 0xC4, 0x1E, 0xFF);
    for (hole = 0; hole != COURSE_HOLE_COUNT; hole++) {
      if (saveData->awards
              .birdieBadgeCounts[g_Scorecard.content.courseIndex][hole] > 0) {
        sprintf(g_PrintBuffer, "%c", BIRDIE_BADGE_CODE);
        if (hole < HOLES_PER_NINE) {
          DrawShadowedText(&gfx,
              (hole * CELL_WIDTH) + BADGE_LEFT + g_Scorecard.x,
              frontTop + BADGE_TOP + g_Scorecard.y, g_PrintBuffer);
        } else {
          DrawShadowedText(&gfx,
              ((hole - HOLES_PER_NINE) * CELL_WIDTH) + BADGE_LEFT +
                  g_Scorecard.x,
              backTop + BADGE_TOP + g_Scorecard.y, g_PrintBuffer);
        }
      }
    }
  }

  *displayListHead = gfx;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard_drawing", DrawScoresPage);
#endif

RESUME_RODATA_SECTION();
const char f_DecimalFormat[] = "%d";

#ifndef NON_MATCHING
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_ScoreTotalFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_EmptyFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_EvenStandingText);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_UpStandingFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_DownStandingFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_ScoreGlyphFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_PointsStandingFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_EvenParText);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_OverParFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_UnderParFormat);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_ScoresTwentyDegrees);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_ScoresPi);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_ScoresHalfPi);
INCLUDE_RODATA("asm/nonmatchings/ovl_09/scorecard_drawing", f_ScoresTenDegrees);
#endif

/*
 * Draws each player's row of bet tallies in the digit font, with their total
 * at the end. The bet columns show g_PrintBuffer as it already stands, and
 * are skipped when the bet rate at the player's index is zero or unset.
 *
 * Doesn't match: the original counts the bet column from 0 and adds
 * BET_TALLY_LEFT where it sets each digit's left edge, and tests
 * g_PrintBuffer's first character at its own address before the digits' loop.
 * The nested ifs make GCC compare the bet rate with zero and UNSET_BET_RATE in
 * turn, as the original does, rather than as a range. 35 words differ.
 */
#ifdef NON_MATCHING
void DrawBetTallies(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 playerIndex;
  s32 betIndex;
  s32 total;
  s32 column;
  s32 left;
  char* text;
  s32 rowOffset;

  LoadSmallPalette(&gfx, 0, ImageBody(g_Scorecard.digitsImage));
  LoadNibbleTextureBlock(&gfx, ImageTexels(g_Scorecard.digitsImage),
      G_IM_FMT_CI, DIGIT_GLYPH_SIZE, DIGIT_FONT_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
  for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
    rowOffset = playerIndex * BET_TALLY_ROW_SPACING;
    total = 0;
    for (betIndex = 0, column = BET_TALLY_LEFT; betIndex != BET_COUNT;
        betIndex++) {
      total += g_BetTallies[playerIndex][betIndex];
      if (g_GameState.betRates[playerIndex] != 0) {
        if (g_GameState.betRates[playerIndex] != UNSET_BET_RATE) {
          for (text = g_PrintBuffer, left = column; *text != '\0';
              left += DIGIT_GLYPH_SIZE) {
            DrawTextureRect(&gfx, QuarterPixels(left + g_Scorecard.x),
                QuarterPixels(BET_TALLY_TOP + rowOffset),
                QuarterPixels(left + g_Scorecard.x + DIGIT_GLYPH_SIZE),
                QuarterPixels(BET_TALLY_TOP + DIGIT_GLYPH_SIZE + rowOffset),
                G_TX_RENDERTILE, 0, TexelCoordinate(GetDigitGlyphTop(*text++)),
                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
          }
        }
      }

      column += BET_TALLY_COLUMN_SPACING;
    }

    sprintf(g_PrintBuffer, f_DecimalFormat, total);
    for (text = g_PrintBuffer, left = BET_TOTAL_LEFT; *text != '\0';
        left += DIGIT_GLYPH_SIZE) {
      DrawTextureRect(&gfx, QuarterPixels(left + g_Scorecard.x),
          QuarterPixels(BET_TALLY_TOP + rowOffset),
          QuarterPixels(left + g_Scorecard.x + DIGIT_GLYPH_SIZE),
          QuarterPixels(BET_TALLY_TOP + DIGIT_GLYPH_SIZE + rowOffset),
          G_TX_RENDERTILE, 0, TexelCoordinate(GetDigitGlyphTop(*text++)),
          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    }
  }

  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_09/scorecard_drawing", DrawBetTallies);
#endif
