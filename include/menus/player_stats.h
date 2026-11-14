/*
 * The bet settings, the handicaps and the best shots, in ovl_01/player_stats.c
 */

#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#include <ultra64.h>

#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"

// The buffers that LoadPlayerStatsBuffer fills, and the assets it reads into
// them
enum PlayerStatsBuffer {
    PLAYER_STATS_BUFFER_BET_ICONS = 1,
    PLAYER_STATS_BUFFER_STAKE_MARK,
    PLAYER_STATS_BUFFER_NAME_PLATE,
    PLAYER_STATS_BUFFER_BET_ICON_FIRST_FRAME,
    PLAYER_STATS_BUFFER_BET_ICON_SECOND_FRAME,
    PLAYER_STATS_BUFFER_UNUSED,
    PLAYER_STATS_BUFFER_GREEN_ARROWS,
    PLAYER_STATS_BUFFER_HANDICAP_PLAQUE,
    PLAYER_STATS_BUFFER_BET_LABELS,
    PLAYER_STATS_BUFFER_PLAYER_LABELS,
    PLAYER_STATS_BUFFER_BRICK_WALL,
    PLAYER_STATS_BUFFER_TRIPLE_PAGE_ARROWS,
    PLAYER_STATS_BUFFER_DOUBLE_PAGE_ARROWS
};

typedef s16 PlayerStatsBuffer;

// The buffers that LoadScoreLabelBuffer keeps, by the kind its caller gives
enum ScoreLabelBuffer {
    // The menus' name plate image, kept without reading an image into it
    SCORE_LABEL_BUFFER_NAME_PLATE = 1,
    SCORE_LABEL_BUFFER_CURSOR_BOX,
    // The four Japanese score labels, one after another
    SCORE_LABEL_BUFFER_LABELS
};

typedef s16 ScoreLabelBuffer;

// The ways SlideBrickWall slides the stats panel's rows
#define STATS_SLIDE_CURRENT 0
#define STATS_SLIDE_OUT_RIGHT 1
#define STATS_SLIDE_OUT_LEFT 2
#define STATS_SLIDE_IN_FROM_RIGHT 3
#define STATS_SLIDE_IN_FROM_LEFT 4
#define STATS_SLIDE_COUNT 5

void LoadPlayerStatsBuffer(void *buffer, PlayerStatsBuffer bufferKind);
void ApplyBetPreset(s32 requestedPreset);
void UpdateBetSettings(void);
void DrawBetSettings(
    Gfx **displayListHead,
    s32 slide,
    UBoolean8 isSlideReversed,
    UBoolean8 isSettled
);
NO_RESULT UpdateHandicaps(s32 firstPlayerIndex, s32 playerCount);
// Draws the handicap screen: the players' characters, a column for each player with the player's
// label and handicap, the banner below them, and the arrows around the column of the player that
// memory->handicapPlayer picks
void DrawHandicaps(
    Gfx **displayListHead,
    s32 slide,
    UBoolean8 isVerticalSlideReversed,
    UBoolean8 canShowPrompt
);
s32 SlideBrickWall(Gfx **displayListHead, u8 slide);
void LoadBestShotsCourseIcons(ImageAsset **buffers);
void LoadScoreLabelBuffer(void *buffer, ScoreLabelBuffer bufferKind);
void UpdateBestShots(void);
void DrawBestShots(Gfx **displayListHead);
void ReadBestShots(void);
void LoadBestShotFaces(void);
// DrawCharacterFace takes an s16 character and a u8 draw mode, but the
// original's character_table.c passes the character, and save_screens.c the
// draw mode, without narrowing it. Those files define WIDENS_FACE_CHARACTER_INDEX
// and WIDENS_FACE_DRAW_MODE to see the wide parameter, unless AVOID_UB is set.
#if !defined(AVOID_UB) && defined(WIDENS_FACE_CHARACTER_INDEX)
typedef s32 FaceCharacterIndexArgument;
#else
typedef s16 FaceCharacterIndexArgument;
#endif
#if !defined(AVOID_UB) && defined(WIDENS_FACE_DRAW_MODE)
typedef s32 FaceDrawModeArgument;
#else
typedef u8 FaceDrawModeArgument;
#endif
void DrawCharacterFace(
    Gfx **displayListHead,
    ImageAsset *texels,
    PaletteColor *palette,
    s32 left,
    s32 top,
    FaceCharacterIndexArgument characterIndex,
    FaceDrawModeArgument drawMode
);
// GetChosenBestShot returns an s16, but the original's main_menu.c uses the
// whole return register; it defines WIDENS_CHOSEN_BEST_SHOT to see a word,
// unless AVOID_UB is set.
#if !defined(AVOID_UB) && defined(WIDENS_CHOSEN_BEST_SHOT)
s32 GetChosenBestShot(void);
#else
s16 GetChosenBestShot(void);
#endif
#endif
