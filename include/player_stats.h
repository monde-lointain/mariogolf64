#ifndef PLAYER_STATS_H
#define PLAYER_STATS_H

#include <ultra64.h>

#include "compiler.h"

/* The bet settings, the handicaps and the best shots, in ovl_01/player_stats.c
 */

/* The buffers that LoadPlayerStatsBuffer fills, and the assets it reads into
 * them */
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

/* The buffers that LoadScoreLabelBuffer keeps, by the kind its caller gives */
enum ScoreLabelBuffer {
    /* The menus' name plate image, kept without reading an image into it */
    SCORE_LABEL_BUFFER_NAME_PLATE = 1,
    SCORE_LABEL_BUFFER_CURSOR_BOX,
    /* The four Japanese score labels, one after another */
    SCORE_LABEL_BUFFER_LABELS
};

/* The ways SlideBrickWall slides the stats panel's rows */
#define STATS_SLIDE_CURRENT 0
#define STATS_SLIDE_OUT_RIGHT 1
#define STATS_SLIDE_OUT_LEFT 2
#define STATS_SLIDE_IN_FROM_RIGHT 3
#define STATS_SLIDE_IN_FROM_LEFT 4
#define STATS_SLIDE_COUNT 5

void LoadPlayerStatsBuffer(void *buffer, s16 bufferKind);
void ApplyBetPreset(s32 requestedPreset);
void UpdateBetSettings(void);
void DrawBetSettings(Gfx **displayListHead, s32 slide, u8 isSlideReversed, u8 isSettled);
NO_RESULT UpdateHandicaps(s32 firstPlayerIndex, s32 playerCount);
void DrawHandicaps(Gfx **displayListHead, s32 slide, u8 reverseVerticalSlide, u8 canShowPrompt);
s32 SlideBrickWall(Gfx **displayListHead, u8 slide);
void LoadBestShotsCourseIcons(void **buffers);
void LoadScoreLabelBuffer(void *buffer, s16 bufferKind);
void UpdateBestShots(void);
void DrawBestShots(Gfx **displayListHead);
void ReadBestShots(void);
void LoadBestShotFaces(void);

#endif
