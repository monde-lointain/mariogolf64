#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#include <ultra64.h>

#include "asset.h"
#include "compiler.h"
#include "language.h"

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

enum ScoreLabelBuffer {
    SCORE_LABEL_BUFFER_NAME_PLATE = 1,
    SCORE_LABEL_BUFFER_CURSOR_BOX,
    SCORE_LABEL_BUFFER_LABELS
};

typedef s16 ScoreLabelBuffer;

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

// The original passes some callers' arguments, or reads some callers'
// results, wider than these routines take or give them. A file that
// defines WIDENS_FACE_CHARACTER_INDEX, WIDENS_FACE_DRAW_MODE or
// WIDENS_CHOSEN_BEST_SHOT gets that wider form.
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
#if !defined(AVOID_UB) && defined(WIDENS_CHOSEN_BEST_SHOT)
s32 GetChosenBestShot(void);
#else
s16 GetChosenBestShot(void);
#endif

#endif
