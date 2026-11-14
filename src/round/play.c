/*
 * Play: setting up the players, their tee order and the clubs usable on the
 * surface, replays of Best Shots and Ring Shot records, and UpdatePlay, which
 * runs each hole and what comes between holes. It also sets the scene's fog and
 * draws the ground.
 */

#include "round/play.h"

#include <framebuffer_count.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "camera/shot_camera.h"
#include "camera/view_projection.h"
#include "characters/character.h"
#include "characters/character_model.h"
#include "controllers/controller.h"
#include "controllers/transfer_pak.h"
#include "course/course.h"
#include "course/course_space.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/hole_triangles.h"
#include "course/surface.h"
#include "course/terrain.h"
#include "course/work_area.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "debug/hole_editor.h"
#include "game/game.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "menus/course_select.h"
#include "menus/menu.h"
#include "round/club_slots.h"
#include "round/game_state.h"
#include "round/movie.h"
#include "round/pause_menu.h"
#include "round/player.h"
#include "round/round_events.h"
#include "round/round_hud.h"
#include "round/round_results.h"
#include "round/round_stats.h"
#include "round/save_menu.h"
#include "round/scorecard.h"
#include "round/scoring.h"
#include "round/tournament.h"
#include "round/turn.h"
#include "save/awards.h"
#include "save/play_record.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "shot/ball_simulation.h"
#include "shot/club.h"
#include "shot/current_shot.h"
#include "shot/golf_balls.h"
#include "shot/shot_effects.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "text/text.h"

// f_GroundNoiseTexels is a square of random bytes this many on a side
#define GROUND_NOISE_SIZE 32

// The power shots that each Ring Shot hole gives
#define RING_SHOT_POWER_SHOT_COUNT 1

// f_HoleEndStep, the step of the sequence between holes
enum HoleEndStep {
    HOLE_END_STEP_START,
    // Frees the character slot, then loads the next hole
    HOLE_END_STEP_FREE_SLOT,
    HOLE_END_STEP_SCORECARD,
    HOLE_END_STEP_WIPE_OUT,
    HOLE_END_STEP_LOAD_NEXT_HOLE,
    // Never set: would wait for A, then show the scorecard
    HOLE_END_STEP_UNUSED_SCORECARD_WAIT,
    HOLE_END_STEP_RING_SHOT_FREE_SLOT,
    HOLE_END_STEP_RING_SHOT_RESULTS,
    HOLE_END_STEP_PROMPT,
    HOLE_END_STEP_QUIT,
    HOLE_END_STEP_FINAL_RESULTS,
    HOLE_END_STEP_LOAD_SCORECARD,
    HOLE_END_STEP_LOAD_RING_SHOT,
    HOLE_END_STEP_DONKEY_KONG_NOTICE,
    HOLE_END_STEP_UNLOCK_NOTICE,
    HOLE_END_STEP_STAFF_ROLL,
    HOLE_END_STEP_RING_SHOT_COURSE_NOTICE
};

typedef s32 HoleEndStep;

// f_PlayExit, the menu that play returns to once the hole ends
enum PlayExit {
    PLAY_EXIT_NONE, // Plays on to the next hole
    PLAY_EXIT_COURSE_SELECT,
    PLAY_EXIT_BLANK_SCREEN,
    PLAY_EXIT_TITLE,
    PLAY_EXIT_BEST_SHOTS,
    PLAY_EXIT_RING_SHOT,
    PLAY_EXIT_QUIT,
    PLAY_EXIT_ROUND_OVER
};

typedef s32 PlayExit;

// f_HoleResultsKind, the results that the scorecard shows between holes
enum HoleResultsKind {
    HOLE_RESULTS_STROKE,
    HOLE_RESULTS_MATCH,
    HOLE_RESULTS_SKINS,
    HOLE_RESULTS_TOURNAMENT,
    HOLE_RESULTS_SPEED_GOLF
};

typedef s32 HoleResultsKind;

CurrentShot g_ScriptedShot;
s8 g_ReplayStrokeCount;
ContestMovieState g_ContestMovieState;
u32 g_GroundDrawFrame;
CurrentShot g_SavedShots[PLAYER_COUNT_MAX];
Player g_SavedPlayers[PLAYER_COUNT_MAX];
ShotLaunch g_LastShotLaunch;
// DrawGround's included assembly names it, as splat calls g_WorkArea's
// address less a row of tiles after the nearest symbol
FILE_STATIC u8 g_UnreadPlayBuffer[0xDEC] UNUSED;

FILE_STATIC u8 g_IsGroundTileNearby[GROUND_TILE_COUNT];
FILE_STATIC PlayRecord f_BestShotReplayRecord;
FILE_STATIC Boolean8 g_WasDonkeyKongUnlocked;
FILE_STATIC Boolean8 g_WasMapleUnlocked;
FILE_STATIC Boolean8 g_WasMetalMarioUnlocked;
FILE_STATIC Boolean8 g_HadEveryRingShotStar;
FILE_STATIC s8 g_RingShotCourseOpenedAtStart;

static s32 f_UnusedWordsBeforeTrainingHeading[2] UNUSED = {0};
Boolean8 g_IsTrainingHeadingKept = FALSE;
PauseScreen g_PauseScreen = PAUSE_SCREEN_NONE;
Boolean g_IsAimCameraEasing = FALSE;
Boolean g_HasPressedShotButton = FALSE;
SignedBinaryAngle g_LookBackHeading = 0;
SignedBinaryAngle g_FreeLookHeading = 0;
s32 g_ShotSetupFrameCount = 0;
s32 g_ImpactPointLingerFrameCount = 0;
ShotFlags g_ShotFlags = 0;
// Cleared by ResetShotSetup, and read by no routine
static s32 f_UnusedClearedPlayWord = 0;
LetterboxState g_LetterboxState = LETTERBOX_STATE_NONE;
ImpactPointState g_ImpactPointState = IMPACT_POINT_OFF;
Boolean g_HasShotComeToRest = TRUE;
Boolean8 g_IsHudTurnAwayRequested = FALSE;
static s32 f_UnusedWordsAfterTurnAwayRequest[2] UNUSED = {0};
// Whether the Ring Shot hole was holed out through every ring, which has the
// hole's end show the Ring Shot results
static Boolean8 f_IsRingShotCleared = FALSE;
static PlayExit f_PlayExit = PLAY_EXIT_NONE;
static HoleEndStep f_HoleEndStep = HOLE_END_STEP_START;
static HoleResultsKind f_HoleResultsKind = HOLE_RESULTS_STROKE;
static s32 f_PlayStateFrameCount = 0;
static s32 f_PlayFrameCount = 0;
static Boolean8 f_IsPlayStateBackedUp = FALSE;
static s32 f_DebugCupIndex = 0;
static s32 f_UnusedWordsAfterDebugCup[3] UNUSED = {0};
static s8 f_UnusedByteBeforeGroundWaves UNUSED = 0;
Boolean8 g_ShowsGroundWaves = FALSE;
f32 g_GroundFarthestDepth = 0.0f;
f32 g_GroundNearDepthEighth = 0.0f;
// The clip ratio, which the second controller's A button cycles under
// GAME_FLAG_CYCLE_CLIP_RATIO
static s32 f_GroundClipRatio = 1;
// Read by no routine; holds each of 0 to 24 once
static u8 f_UnusedShuffledIndices[25] UNUSED =
    {0, 5, 7, 10, 20, 1, 12, 13, 14, 21, 2, 15, 8, 16, 22, 3, 17, 18, 19, 23, 4, 6, 9, 11, 24};
static s32 f_GroundTimingRow = 0;

static u8 f_GroundNoiseTexels[GROUND_NOISE_SIZE * GROUND_NOISE_SIZE];

// Whether play's hole intro, flyover and results move on without A, as under
// GAME_FLAG_FAST_AUTOPLAY or GAME_FLAG_AUTOPLAY
Boolean IsPlayAdvancingItself(void) {
    return IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY) || IsGameFlagSet(GAME_FLAG_AUTOPLAY);
}

// Clears what the last shot's setup left: the pause screen, the shot button,
// flags and headings, the letterbox, the impact point and their frame counts
void ResetShotSetup(void) {
    g_PauseScreen = PAUSE_SCREEN_NONE;
    g_IsAimCameraEasing = FALSE;
    g_HasPressedShotButton = FALSE;
    g_LookBackHeading = 0;
    g_ShotFlags = 0;
    f_UnusedClearedPlayWord = 0;
    g_LetterboxState = LETTERBOX_STATE_NONE;
    g_ImpactPointState = IMPACT_POINT_OFF;
    g_FreeLookHeading = 0;
    g_ShotSetupFrameCount = 0;
    g_ImpactPointLingerFrameCount = 0;
}

// How full the swing gauge is at the current gaugeFrame: it fills until
// GAUGE_FULL_FRAME, then empties until the impact mark
s32 GetGaugeFill(void) {
    s32 fill = g_CurrentShot.gaugeFrame;

    if (fill > GAUGE_FULL_FRAME) {
        fill = GAUGE_IMPACT_FRAME - fill;
    }

    return fill;
}

// The launch power, out of 256, of the gauge's fill at a gauge frame
s32 GetGaugeFramePower(s32 gaugeFrame) {
    if (gaugeFrame > GAUGE_FULL_FRAME) {
        gaugeFrame = GAUGE_IMPACT_FRAME - gaugeFrame;
    }

    if (gaugeFrame < 0) {
        gaugeFrame = 0;
    }

    return (gaugeFrame * LAUNCH_POWER_MAX) / GAUGE_FULL_FRAME;
}

// Sets each of a club set's flags in isUsable to whether the current
// surface allows the club, and returns the set's size. The first argument
// is not read.
s32 MarkUsableClubs(s32 unused UNUSED, s16 *clubs, Boolean8 *isUsable) {
    ClubSlot clubSlot;

    for (clubSlot = 0; clubSlot < CLUB_SET_SIZE; clubSlot++) {
        isUsable[clubSlot] =
            (GetClub(clubs[clubSlot])->surfaceMask & GetCurrentSurfaceClubMask(TRUE)) != 0;
    }

    return CLUB_SET_SIZE;
}

// The height that the aim camera keeps above at (x, z): the ground or
// water there, or outside Mini-Golf where that is above zero, the ball's
// height (at least zero)
s32 GetAimCameraGround(s32 x, s32 z) {
    s32 height = GetGroundOrWaterHeight(x, z);
    s32 ballHeight;

    if (!g_GameState.isMiniGolf & (height > 0)) {
        ballHeight = g_CurrentShot.ballPosition[AXIS_Y];
        if (ballHeight <= 0) {
            ballHeight = 0;
        }

        height = ballHeight;
    }

    return height;
}

enum FlaggedRoundKind {
    FLAGGED_ROUND_STROKE,
    FLAGGED_ROUND_TRAINING,
    FLAGGED_ROUND_MATCH_GAME,
    FLAGGED_ROUND_SKINS_MATCH
};

// Sets up PLAY_KIND_FLAGGED_ROUND's round on the first hole, with the mode and
// players that the GAME_FLAG_ROUND_KIND and GAME_FLAG_EXTRA_PLAYERS bits pick
void SetUpFlaggedRound(void) {
    switch ((IsGameFlagSet(GAME_FLAG_ROUND_KIND_HIGH_BIT) * 2) +
        IsGameFlagSet(GAME_FLAG_ROUND_KIND_LOW_BIT)) {
        case FLAGGED_ROUND_STROKE:
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unusedHoleValue = 0;
            g_GameState.playerCount = (IsGameFlagSet(GAME_FLAG_EXTRA_PLAYERS_HIGH_BIT) * 2) +
                IsGameFlagSet(GAME_FLAG_EXTRA_PLAYERS_LOW_BIT) + 1;
            g_GameState.step = GAME_STATE_START;
            g_GameState.gameMode = GAME_MODE_STROKE;
            break;
        case FLAGGED_ROUND_TRAINING:
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unusedHoleValue = 0;
            g_GameState.playerCount = 1;
            g_GameState.step = GAME_STATE_START;
            g_GameState.gameMode = GAME_MODE_TRAINING;
            break;
        case FLAGGED_ROUND_MATCH_GAME:
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unusedHoleValue = 0;
            g_GameState.playerCount = 2;
            g_GameState.step = GAME_STATE_START;
            g_GameState.gameMode = GAME_MODE_MATCH_GAME;

        // The original has no break here, so the default case overrides this one
        // Falls through
        default:
            g_GameState.playerCount = 3;
            g_GameState.courseIndex = 0;
            g_GameState.holeIndex = 0;
            g_GameState.unusedHoleValue = 0;
            g_GameState.step = GAME_STATE_START;
            g_GameState.gameMode = GAME_MODE_SKINS_MATCH;
            break;
    }
}

// Sets up PLAY_KIND_HOLE's one player on the first hole
void SetUpOneHole(void) {
    g_GameState.courseIndex = 0;
    g_GameState.playerCount = 1;
    g_GameState.holeIndex = 0;
    g_GameState.unusedHoleValue = 0;
    g_GameState.step = GAME_STATE_REPEATING_HOLE;
    if (IsGameFlagSet(GAME_FLAG_INIT_RING_SHOT)) {
        g_GameState.gameMode = GAME_MODE_RING_SHOT;
    }
    else {
        g_GameState.gameMode = GAME_MODE_STROKE;
    }
}

// Writes the name a player of the given index has by default
void FormatDefaultPlayerName(s32 index, char *name) {
    sprintf(name, "MARIO%d", (int)(index + 1));
}

// Sets up the players for a round. In SCENE_PLAY_BEST_SHOT_REPLAY, in
// GAME_MODE_RECORD_REPLAY or during a replay, only the first player
// is set up, from the Ring Shot record or the Best Shot replay record;
// otherwise all four get default names, their own character's clubs and
// cleared scores.
void SetUpPlayers(void) {
    s32 playerIndex;
    s32 holeIndex;
    NotableShot slot;

    if (g_Scene == SCENE_PLAY_BEST_SHOT_REPLAY ||
        g_GameState.gameMode == GAME_MODE_RECORD_REPLAY ||
        g_GameState.replayKind != REPLAY_KIND_NONE) {
        if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
            g_Players[0].isComputer = FALSE;
            g_Players[0].teeOrder = 0;
            g_Players[0].characterIndex = g_RingShotRecords->characterIndex;
            g_Players[0].isLefty = g_RingShotRecords->isLefty;
            g_Players[0].paletteIndex = g_RingShotRecords->paletteIndex;
            g_Players[0].noiseSeed = g_RingShotRecords->noiseSeed;
        }
        else {
            g_Players[0].isComputer = FALSE;
            g_Players[0].teeOrder = 0;
            g_Players[0].characterIndex = g_PlayRecords->characterIndex;
            g_Players[0].isLefty = g_PlayRecords->isLefty;
            g_Players[0].paletteIndex = g_PlayRecords->paletteIndex;
            g_Players[0].noiseSeed = g_PlayRecords->noiseSeed;
        }

        for (holeIndex = 0; holeIndex < COURSE_HOLE_COUNT; holeIndex++) {
            g_Players[0].holes[holeIndex].strokeCount = 0;
            g_Players[0].holes[holeIndex].puttCount = UNCOUNTED_PUTTS;
        }

        g_Players[0].powerShotCount = POWER_SHOTS_UNLIMITED;
        LoadPlayerFace(0, g_Players[0].characterIndex, g_Players[0].paletteIndex);
        for (slot = 0; slot < CLUB_SLOT_COUNT; slot++) {
            if (slot >= CLUB_SET_SIZE) {
                g_Players[0].clubs[slot] = NO_CLUB;
            }
            else {
                g_Players[0].clubs[slot] = slot + (g_Players[0].characterIndex * CLUB_SET_SIZE);
            }
        }

        // The index variable is there to match: indexing player 0 directly gives
        // 12 words differing, 6 in shape
        playerIndex = 0;
        for (slot = NOTABLE_SHOT_COUNT - 1; slot >= 0; slot--) {
            g_Players[playerIndex].notableShotCounts[slot] = 0;
        }
    }
    else {
        for (playerIndex = 0; playerIndex < PLAYER_COUNT_MAX; playerIndex++) {
            FormatDefaultPlayerName(playerIndex, g_Players[playerIndex].name);
            g_Players[playerIndex].characterIndex = playerIndex;
            g_Players[playerIndex].isLefty = FALSE;
            g_Players[playerIndex].isComputer = FALSE;
            g_Players[playerIndex].teeOrder = 0;
            g_Players[playerIndex].gridColor = GetSaveData()->options[SAVE_OPTION_GRID_COLOR];
            for (holeIndex = 0; holeIndex < COURSE_HOLE_COUNT; holeIndex++) {
                g_Players[playerIndex].holes[holeIndex].strokeCount = 0;
                g_Players[playerIndex].holes[holeIndex].puttCount = UNCOUNTED_PUTTS;
            }

            for (slot = NOTABLE_SHOT_COUNT - 1; slot >= 0; slot--) {
                g_Players[playerIndex].notableShotCounts[slot] = 0;
            }

            for (slot = 0; slot < CLUB_SLOT_COUNT; slot++) {
                if (slot >= CLUB_SET_SIZE) {
                    g_Players[playerIndex].clubs[slot] = NO_CLUB;
                }
                else {
                    g_Players[playerIndex].clubs[slot] =
                        slot + (g_Players[playerIndex].characterIndex * CLUB_SET_SIZE);
                }
            }

            g_Players[playerIndex].stopwatchFrameCount = 0;
            g_Players[playerIndex].noiseSeed = NextRandom();
        }
    }
}

void InitPlay(PlayKind kind) {
    if (kind == PLAY_KIND_FLAGGED_ROUND) {
        SetUpFlaggedRound();
    }
    else if (kind == PLAY_KIND_HOLE) {
        SetUpOneHole();
    }
    else if (kind == PLAY_KIND_BEST_SHOT_REPLAY) {
        LoadBestShotReplay(&f_BestShotReplayRecord);
        g_GameState.unusedHoleValue = 0;
        g_GameState.playerCount = 1;
        g_GameState.step = GAME_STATE_START;
        g_GameState.gameMode = GAME_MODE_STROKE;
        g_GameState.courseIndex = g_PlayRecords[0].courseIndex;
        g_GameState.holeIndex = g_PlayRecords[0].holeIndex;
    }

    RenumberHoleHandicaps();
    if (!g_GameState.isRoundContinued) {
        SetUpPlayers();
    }

    if ((IsGameFlagSet(GAME_FLAG_ROUND_KIND_HIGH_BIT) * 2) +
            IsGameFlagSet(GAME_FLAG_ROUND_KIND_LOW_BIT) ==
        2) {
        g_Players[1].isComputer = TRUE;
    }

    InitExpansionPakFeatures();
    if (kind != PLAY_KIND_WITHOUT_HOLE_DISPLAY) {
        PrepareHoleDisplay();
    }

    WriteBallDistancesIfEnabled();
    osSyncPrintf("PlayInitComplete\n");
}

// Unless g_GameState.isRoundContinued is set, starts each player's ball on the
// tee, with the hole's per-player state cleared
void PlacePlayersOnTee(void) {
    s32 playerIndex;
    s32 ringIndex;
    f32 teeHeight;

    g_HasShotComeToRest = FALSE;
    g_IsHudTurnAwayRequested = FALSE;
    if (g_GameState.isRoundContinued != ROUND_CONTINUED) {
        for (playerIndex = 0; playerIndex < PLAYER_COUNT_MAX; playerIndex++) {
            g_Players[playerIndex].state = PLAYER_STATE_WAITING;
            g_Players[playerIndex].ballPosition[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
            teeHeight = g_HoleSetup.teePosition[AXIS_Y];
            g_Players[playerIndex].ballPosition[AXIS_Y] =
                !g_GameState.isMiniGolf ? teeHeight - TEE_HEIGHT : teeHeight;
            g_Players[playerIndex].ballPosition[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
            g_Players[playerIndex].strokeCount = 0;
            g_Players[playerIndex].puttCount = 0;
            g_Players[playerIndex].holeStatus = HOLE_STATUS_PLAYING;
            g_Players[playerIndex].ballSurface = SURFACE_TEE_UP;
            if (g_GameState.isMiniGolf) {
                g_Players[playerIndex].ballSurface = GetSurface(
                    g_Players[playerIndex].ballPosition[AXIS_X],
                    g_Players[playerIndex].ballPosition[AXIS_Z]
                );
            }

            g_Players[playerIndex].driveDistance = UNMEASURED_DISTANCE;
            g_Players[playerIndex].pinDistance = UNMEASURED_DISTANCE;
            g_Players[playerIndex].hasBeenInBunker = FALSE;
            for (ringIndex = RING_COUNT - 1; ringIndex >= 0; ringIndex--) {
                g_Players[playerIndex].ringStates[ringIndex] = 0;
            }

            if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                g_Players[playerIndex].powerShotCount = RING_SHOT_POWER_SHOT_COUNT;
            }
        }
    }

    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
        InitRings();
    }
}

// Once every player has finished the hole, plays the movies for the
// contests won on it, then waits for them to finish
void PlayContestMovies(void) {
    s32 playerIndex;

    if (g_ContestMovieState == CONTEST_MOVIES_WAITING) {
        for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
            if (g_Players[playerIndex].strokeCount == 0) {
                break;
            }

            if (g_Players[playerIndex].state != PLAYER_STATE_WAITING &&
                g_Players[playerIndex].state != PLAYER_STATE_HOLE_DONE) {
                break;
            }
        }

        if (g_GameState.replayKind == REPLAY_KIND_NONE && playerIndex == g_GameState.playerCount) {
            if (IsMarkedParFive(g_GameState.courseIndex, g_GameState.holeIndex) &&
                GetContestWinner() != NO_CONTEST_WINNER) {
                StartMovie(MOVIE_LONGEST_DRIVE);
            }

            if (IsParThree(g_GameState.courseIndex, g_GameState.holeIndex) &&
                GetContestWinner() != NO_CONTEST_WINNER) {
                StartMovie(MOVIE_NEAREST_PIN);
            }

            if (g_GameState.gameMode == GAME_MODE_TOURNAMENT && RecordDrive()) {
                StartMovie(MOVIE_DRIVE_RECORD);
            }

            g_ContestMovieState = CONTEST_MOVIES_PLAYING;
        }
    }
    else if (IsMovieDone()) {
        g_ContestMovieState = CONTEST_MOVIES_DONE;
    }
}

// Whether play can leave the hole: never while the contest movies play; in
// Skins Match, Club Slots and the one-on-one matches, once the hole is decided
// and every player is done or waiting; otherwise, once every player is done.
Boolean IsHoleOver(void) {
    Boolean isDone = FALSE;
    s32 playerIndex;

    if (g_ContestMovieState == CONTEST_MOVIES_PLAYING) {
        return FALSE;
    }

    if (g_GameState.replayKind == REPLAY_KIND_NONE) {
        // In its own block to match: without it, or in plain braces, 6 words
        // differ, and moving the players' done-or-waiting loop into an inline
        // helper, in 3 forms, leaves 63-86 lines of the diff differing
        IN_OWN_BLOCK(
            if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                if (!GetGroupHoleResult(NULL)) {
                    return FALSE;
                }

                for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
                    if (g_Players[playerIndex].state != PLAYER_STATE_HOLE_DONE &&
                        g_Players[playerIndex].state != PLAYER_STATE_WAITING) {
                        break;
                    }
                }

                // Through isDone, returning in its own block, to match. The block's
                // loop labels part the store from the return, so jump's store-flag
                // conversion (jump.c, "Try to use a conditional move") can't make the
                // test a set-on-equal, which the original lacks. Returning TRUE gives
                // 5 words; returning isDone outside a block, or in plain braces, 16.
                if (playerIndex == g_GameState.playerCount) {
                    isDone = TRUE;
                    IN_OWN_BLOCK(return isDone);
                }

                return FALSE;
            }

            if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                if (!GetMatchHoleResult(0, 1)) {
                    return FALSE;
                }

                for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
                    if (g_Players[playerIndex].state != PLAYER_STATE_HOLE_DONE &&
                        g_Players[playerIndex].state != PLAYER_STATE_WAITING) {
                        break;
                    }
                }

                // Clearing isDone, though it is clear already, returning it, unlike
                // the FALSE above, and setting it in its own block, to match, as
                // above: setting it outside a block, or in plain braces, gives 12
                // words, and returning TRUE or FALSE directly 11
                isDone = FALSE;
                if (playerIndex == g_GameState.playerCount) {
                    IN_OWN_BLOCK(isDone = TRUE);
                    return isDone;
                }

                return isDone;
            }
        );
    }

    for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        if (g_Players[playerIndex].state != PLAYER_STATE_HOLE_DONE) {
            break;
        }
    }

    if (playerIndex == g_GameState.playerCount) {
        isDone = TRUE;
    }

    return isDone;
}

// Whether the first player tees off after the second on the next hole.
// It is a macro, as an inline function compiles differently from the
// original.
#define TeesAfter(player, other) \
    (g_Players[player].strokeCount > g_Players[other].strokeCount || \
        (g_Players[player].strokeCount == g_Players[other].strokeCount && \
            g_Players[player].teeOrder > g_Players[other].teeOrder))

// Sets each player's teeOrder for the next hole. In the group modes a
// hole's winner goes first and the others move down one; otherwise the
// players go in order of strokeCount, ties keeping their previous order.
void SetNextTeeOrder(void) {
    s32 orders[PLAYER_COUNT_MAX];
    GroupPlayerMark results[PLAYER_COUNT_MAX];
    s32 playerIndex;
    s32 otherIndex;

    if (g_GameState.replayKind == REPLAY_KIND_NONE &&
        (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
            g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)) {
        if (GetGroupHoleResult(results) != GROUP_HOLE_WON) {
            for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
                orders[playerIndex] = g_Players[playerIndex].teeOrder;
            }
        }
        else {
            for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
                if (results[playerIndex] != GROUP_PLAYER_WINNER) {
                    orders[playerIndex] = g_Players[playerIndex].teeOrder + 1;
                }
                else {
                    orders[playerIndex] = 0;
                }
            }
        }
    }
    else {
        for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
            orders[playerIndex] = 0;
            for (otherIndex = 0; otherIndex < g_GameState.playerCount; otherIndex++) {
                if (otherIndex != playerIndex && TeesAfter(playerIndex, otherIndex)) {
                    orders[playerIndex]++;
                }
            }
        }
    }

    // The copy loop in a block of its own is there to match; without it, or in
    // plain braces, 38 words differ, none in shape, counting it with j 46 and
    // with a counter of its own 60; of every choice of i or j for the five
    // loops, alternating them from i or from j is closest, at 34
    IN_OWN_BLOCK(for (playerIndex = 0; playerIndex < g_GameState.playerCount; playerIndex++) {
        g_Players[playerIndex].teeOrder = orders[playerIndex];
    });
}

// Copies what the player chose for a shot of the Best Shot replay record: its
// heading, gauge frames, shot mode, club and offsets, for the replay to repeat
void LoadBestShotReplayInputs(s32 index) {
    g_ScriptedShot.heading = g_PlayRecords->shots[index].heading;
    g_ScriptedShot.powerGaugeFrame = g_PlayRecords->shots[index].powerGaugeFrame;
    g_ScriptedShot.impactGaugeFrame = g_PlayRecords->shots[index].impactGaugeFrame;
    g_ScriptedShot.shotMode = g_PlayRecords->shots[index].shotMode;
    g_ScriptedShot.clubSlot = g_PlayRecords->shots[index].clubSlot;
    g_ScriptedShot.offsetX = g_PlayRecords->shots[index].offsetX;
    g_ScriptedShot.offsetY = g_PlayRecords->shots[index].offsetY;
}

// LoadBestShotReplayInputs for a shot of the Ring Shot record
void LoadRingShotReplayInputs(s32 index) {
    g_ScriptedShot.heading = g_RingShotRecords->shots[index].heading;
    g_ScriptedShot.powerGaugeFrame = g_RingShotRecords->shots[index].powerGaugeFrame;
    g_ScriptedShot.impactGaugeFrame = g_RingShotRecords->shots[index].impactGaugeFrame;
    g_ScriptedShot.shotMode = g_RingShotRecords->shots[index].shotMode;
    g_ScriptedShot.clubSlot = g_RingShotRecords->shots[index].clubSlot;
    g_ScriptedShot.offsetX = g_RingShotRecords->shots[index].offsetX;
    g_ScriptedShot.offsetY = g_RingShotRecords->shots[index].offsetY;
}

// Gives the score worth recording for a hole finished in this many strokes,
// as SCORE_KIND_RECORDED with its ScoreKind, or 0 for none
s32 GetRecordedScoreKind(s32 strokes) {
    ScoreToPar toPar = strokes - g_HoleSetup.par;

    if (strokes == 1) {
        return SCORE_KIND_RECORDED | SCORE_KIND_HOLE_IN_ONE;
    }

    switch (toPar) {
        case SCORE_TO_PAR_ALBATROSS:
            return SCORE_KIND_RECORDED | SCORE_KIND_ALBATROSS;
        case SCORE_TO_PAR_EAGLE:
            return SCORE_KIND_RECORDED | SCORE_KIND_EAGLE;
        case SCORE_TO_PAR_BIRDIE:
            return SCORE_KIND_RECORDED | SCORE_KIND_BIRDIE;
        case SCORE_TO_PAR_PAR:
            if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                return SCORE_KIND_RECORDED | SCORE_KIND_PAR;
            }

            break;
    }

    return 0;
}

// Sets play up to replay a Best Shot record
void LoadBestShotReplay(PlayRecord *record) {
    g_PlayRecords[0] = *record;
    g_GameState.gameMode = record->gameMode;
    g_GameState.courseIndex = record->courseIndex;
    g_GameState.holeIndex = record->holeIndex;
    g_ReplayStrokeCount = record->strokeCount;
    SetUpPlayers();
    osSyncPrintf("SetSuperShotReplay\n");
}

// Sets play up for the Ring Shot hole of a record
void LoadRingShotRecord(RingShotRecord *record) {
    g_RingShotRecords[0] = *record;
    g_GameState.gameMode = GAME_MODE_RING_SHOT;
    g_GameState.courseIndex = record->courseIndex;
    g_GameState.holeIndex = record->holeIndex;
    g_ReplayStrokeCount = record->strokeCount;
    SetUpPlayers();
}

// Sets f_IsRingShotCleared, for the turn in ovl_00
void SetRingShotCleared(Boolean8 isCleared) {
    f_IsRingShotCleared = isCleared;
}

// Ends play for the menu's title, as the pause menu's return to the menu
// does: the game is saved or kept by mode, and the hole-end sequence quits
void QuitPlayToTitle(void) {
    g_PauseScreen = PAUSE_SCREEN_NONE;
    SaveOrContinueGameByMode(FALSE);
    ClearShotEffects(CLEAR_KIND_EFFECTS);
    g_GameState.step = GAME_STATE_HOLE_END;
    f_PlayExit = PLAY_EXIT_TITLE;
    f_HoleEndStep = HOLE_END_STEP_QUIT;
}

// The number that the GAME_FLAG_FLAGGED_NUMBER_* flags spell, one for each of
// its five bits. Nothing calls it.
s32 GetFlaggedFiveBitNumber(void) {
    return (IsGameFlagSet(GAME_FLAG_FLAGGED_NUMBER_SIXTEENS) << 4) |
        (IsGameFlagSet(GAME_FLAG_FLAGGED_NUMBER_EIGHTS) << 3) |
        (IsGameFlagSet(GAME_FLAG_FLAGGED_NUMBER_FOURS) << 2) |
        (IsGameFlagSet(GAME_FLAG_FLAGGED_NUMBER_TWOS) << 1) |
        IsGameFlagSet(GAME_FLAG_FLAGGED_NUMBER_ONES);
}

// Copies the game state and players to g_BackedUpGameState and
// g_BackedUpPlayers
void BackUpPlayState(void) {
    s32 playerIndex;

    g_BackedUpGameState = g_GameState;
    for (playerIndex = 0; playerIndex < PLAYER_COUNT_MAX; playerIndex++) {
        g_BackedUpPlayers[playerIndex] = g_Players[playerIndex];
    }
}

// Whether the player has beaten Bowser in a Get Character match, or has
// won first place in a Boo Valley or Mario's Star tournament
Boolean HasWonFinalChallenge(void) {
    s32 firstHole;
    TournamentStanding *standing;
    s32 contestantIndex;

    if (g_GameState.gameMode == GAME_MODE_GET_CHARACTER) {
        if (GetMatchWinner(0, 1) == MATCH_SIDE_PLAYER) {
            if (g_Players[1].characterIndex == CHARACTER_BOWSER) {
                return TRUE;
            }
        }
    }
    else if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
        if (g_GameState.courseIndex == COURSE_RECORD_BOO_VALLEY ||
            g_GameState.courseIndex == COURSE_RECORD_MARIOS_STAR) {
            firstHole = GetFirstHoleIndex();
            RankTournament(
                COURSE_HOLE_COUNT,
                0,
                GetNetScoreToPar(firstHole + GetRoundHoleCount(), 0),
                g_GameState.roundSeed
            );
            for (contestantIndex = 0; contestantIndex != TOURNAMENT_CONTESTANT_COUNT;
                contestantIndex++) {
                standing = &g_TournamentStandings[contestantIndex];
                if (standing->contestantId == PLAYER_CONTESTANT &&
                    (standing->place & PLACE_MASK) == 1) {
                    return TRUE;
                }
            }
        }
    }

    return FALSE;
}

// Plus the number of the Ring Shot course that opens, its UnlockNotice
#define UNLOCK_NOTICE_RING_SHOT_COURSE_BASE (UNLOCK_NOTICE_COURSES - 1)

#define PLAYER_CHARACTER_SLOT 0
// The hole index after the front nine
#define TOURNAMENT_HALF_TURN_HOLE 9

// How long the hole's intro waits before it moves on by itself
#define HOLE_INTRO_FRAMES 240
#define SHORT_HOLE_INTRO_FRAMES 90
// How soon the debug flags that advance play move it on
#define AUTO_ADVANCE_FRAMES 10
#define AUTO_ADVANCE_RESULTS_FRAMES 90

// Whether the round has played its last hole, once g_GameState.holeIndex has
// moved past it
#define HasPlayedLastHole() (g_GameState.holeIndex == GetFirstHoleIndex() + GetRoundHoleCount())
#define IsAButtonPressed() (g_MergedController.pressedButtons & A_BUTTON)
// Whether the Ring Shot hole has just earned the last star, which rolls the
// staff
#define IsStaffRollDueAfterRingShot() \
    ((HasEveryRingShotStar() && !g_HadEveryRingShotStar) || \
        IsGameFlagSet(GAME_FLAG_SHOW_STAFF_ROLL))

// The debug cup picker's text position
#define DEBUG_CUP_TEXT_COLUMN 3
#define DEBUG_CUP_TEXT_ROW 3

// The hole intro camera circles the hole at these distances
// In binary angle units
#define INTRO_ORBIT_ANGLE_PER_FRAME 256
#define IntroOrbitAngle() \
    (((f_PlayStateFrameCount * INTRO_ORBIT_ANGLE_PER_FRAME) & BINARY_ANGLE_MASK) * \
        BINARY_ANGLE_TO_RADIANS)
#define DEBUG_ORBIT_RADIUS 1500.0f
#define DEBUG_ORBIT_HEIGHT (-1500.0f)
#define INTRO_ORBIT_RADIUS 3000.0f
#define INTRO_ORBIT_HEIGHT (-3000.0f)
#define MINI_GOLF_ORBIT_RADIUS 600.0f
#define MINI_GOLF_ORBIT_HEIGHT (-300.0f)

// Runs play for a frame: loads each hole, shows its intro and flyover, takes
// the turns, and steps through the scorecard, notices and menus between
// holes until the round ends and play returns to a menu or the staff roll.
void UpdatePlay(void) {
    Boolean repeats;
    Boolean again;
    Boolean hasHuman;
    s32 playerSlot;
    s32 playerIndex;
    SelectResult result;
    Boolean clearsHud;
    HoleEndStep nextStep;

    repeats = FALSE;
    f_PlayFrameCount++;
    if (g_Players[0].controllerIndex < MAXCONTROLLERS) {
        g_TurnController = g_ControllerStates[g_Players[0].controllerIndex];
    }
    else {
        g_TurnController.stickX = 0;
        g_TurnController.stickY = 0;
        g_TurnController.buttons = 0;
        g_TurnController.repeatedButtons = 0;
        g_TurnController.pressedButtons = 0;
    }

    // Menus between holes answer to any human player
    hasHuman = FALSE;
    g_MergedController.buttons = 0;
    g_MergedController.heldButtons = 0;
    g_MergedController.pressedButtons = 0;
    g_MergedController.repeatedButtons = 0;
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        if (g_Players[playerIndex].controllerIndex < MAXCONTROLLERS) {
            g_MergedController.buttons |=
                g_ControllerStates[g_Players[playerIndex].controllerIndex].buttons;
            g_MergedController.heldButtons |=
                g_ControllerStates[g_Players[playerIndex].controllerIndex].heldButtons;
            g_MergedController.pressedButtons |=
                g_ControllerStates[g_Players[playerIndex].controllerIndex].pressedButtons;
            g_MergedController.repeatedButtons |=
                g_ControllerStates[g_Players[playerIndex].controllerIndex].repeatedButtons;
            hasHuman = TRUE;
        }
    }

    if (!hasHuman) {
        g_MergedController.buttons = g_ControllerStates[0].buttons;
        g_MergedController.heldButtons = g_ControllerStates[0].heldButtons;
        g_MergedController.pressedButtons = g_ControllerStates[0].pressedButtons;
        g_MergedController.repeatedButtons = g_ControllerStates[0].repeatedButtons;
    }

    // A step that hands over to another within the same frame sets repeat
    again = repeats;
    do {
        if (again) {
            osSyncPrintf("noexit\n");
        }

        repeats = FALSE;
        switch (g_GameState.step) {
            case GAME_STATE_START:
                g_GameState.step = GAME_STATE_LOAD_HOLE;
            // Falls through
            case GAME_STATE_LOAD_HOLE:
                StopLoaderThread();
                StartLoaderThread();
                g_GameState.step = GAME_STATE_WAIT_FOR_HOLE;
                osSyncPrintf("MAKEHOLE2\n");
                break;
            case GAME_STATE_RELOAD_HOLE:
                if (g_HasLoaderFinished == TRUE) {
                    g_GameState.step = GAME_STATE_WAIT_FOR_HOLE;
                }

                break;
            case GAME_STATE_WAIT_FOR_HOLE:
                if (g_HasLoaderFinished == TRUE) {
                    if (!IsSubModuleLoaded()) {
                        break;
                    }

                    g_GameState.step = GAME_STATE_HOLE_INTRO;
                    ResetGolfBalls();
                    SetGameFlag(GAME_FLAG_HOLE_LOADED);
                    if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                        if (GetWipeKind() == WIPE_CLOSING) {
                            StartWipe(WIPE_OPENING_FAST);
                        }

                        SetPlankMotion(TRUE);
                    }
                    else {
                        StartWipe(WIPE_OPENING_FAST);
                    }

                    memset(&g_RoundHud, 0, sizeof(RoundHud));
                    StartHoleIntro();
                    StartHoleCamerasAndSong();
                    f_PlayStateFrameCount = 0;
                }
                else {
                    f_PlayStateFrameCount++;
                    if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                        if (g_IsModuleAlive[MODULE_COURSE_SELECT] &&
                            UpdateHoleSelect(
                                g_MergedController.pressedButtons,
                                g_MergedController.repeatedButtons
                            ) == SELECT_MOVED) {
                            LoadWipeShape(g_GameState.courseIndex);
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            if (GetWipeProgress() == WIPE_OPEN_PROGRESS) {
                                StartWipe(WIPE_CLOSING);
                            }

                            g_GameState.step = GAME_STATE_LOAD_HOLE;
                            repeats = TRUE;
                        }
                    }
                    else if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
                        g_GameState.replayKind == REPLAY_KIND_NONE &&
                        g_IsModuleAlive[MODULE_COURSE_SELECT]) {
                        result = UpdateRingShotSelect(
                            g_MergedController.pressedButtons,
                            g_MergedController.repeatedButtons
                        );
                        if (result == SELECT_MOVED) {
                            LoadWipeShape(g_GameState.courseIndex);
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            SetPlankMotion(FALSE);
                            g_GameState.step = GAME_STATE_LOAD_HOLE;
                            repeats = TRUE;
                        }
                    }
                }

                break;
            case GAME_STATE_HOLE_INTRO:
                f_IsPlayStateBackedUp = FALSE;
                nuGfxDisplayOn();
                if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                    if (g_IsModuleAlive[MODULE_COURSE_SELECT]) {
                        result = UpdateHoleSelect(
                            g_MergedController.pressedButtons,
                            g_MergedController.repeatedButtons
                        );
                        switch (result) {
                            case SELECT_MOVED:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                StartWipe(WIPE_CLOSING);
                                LoadWipeShape(g_GameState.courseIndex);
                                g_GameState.step = GAME_STATE_LOAD_HOLE;
                                repeats = TRUE;
                                break;
                            case SELECT_BACK:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                f_PlayExit = PLAY_EXIT_QUIT;
                                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                                g_GameState.step = GAME_STATE_HOLE_END;
                                f_HoleEndStep = HOLE_END_STEP_START;
                                ClearHoleIntroText();
                                break;
                            case SELECT_IDLE:
                                if (GetWipeProgress() == WIPE_OPEN_PROGRESS && IsAButtonPressed()) {
                                    g_GameState.step = GAME_STATE_END_INTRO;
                                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                                }

                                break;
                        }
                    }
                }
                else if (g_GameState.gameMode == GAME_MODE_RING_SHOT &&
                    g_GameState.replayKind == REPLAY_KIND_NONE) {
                    if (g_IsModuleAlive[MODULE_COURSE_SELECT]) {
                        result = UpdateRingShotSelect(
                            g_MergedController.pressedButtons,
                            g_MergedController.repeatedButtons
                        );
                        switch (result) {
                            case SELECT_MOVED:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                SetPlankMotion(FALSE);
                                g_GameState.step = GAME_STATE_LOAD_HOLE;
                                repeats = TRUE;
                                LoadWipeShape(g_GameState.courseIndex);
                                break;
                            case SELECT_BACK:
                                SetMenuBackdrop(MENU_BACKDROP_STILL);
                                f_PlayExit = PLAY_EXIT_QUIT;
                                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                                g_GameState.step = GAME_STATE_HOLE_END;
                                f_HoleEndStep = HOLE_END_STEP_START;
                                ClearHoleIntroText();
                                break;
                            case SELECT_IDLE:
                                if (ArePlanksOpen() &&
                                    GetWipeProgress() == WIPE_OPEN_PROGRESS &&
                                    IsAButtonPressed()) {
                                    g_GameState.step = GAME_STATE_END_INTRO;
                                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                                }

                                break;
                        }
                    }
                }
                else if (g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                    if (g_IsModuleAlive[MODULE_CLUB_SLOTS] &&
                        (f_PlayStateFrameCount >= SHORT_HOLE_INTRO_FRAMES || IsAButtonPressed())) {
                        if (!g_GameState.isRoundContinued) {
                            g_GameState.step = GAME_STATE_SLOTS;
                            LoadClubSlots();
                        }
                        else {
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            g_GameState.step = GAME_STATE_END_INTRO;
                        }
                    }
                }
                else if (g_GameState.replayKind != REPLAY_KIND_NONE) {
                    if (f_PlayStateFrameCount >= SHORT_HOLE_INTRO_FRAMES || IsAButtonPressed()) {
                        if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                            StartHoleFlyover();
                            StopHoleIntro();
                            g_GameState.step = GAME_STATE_TAKING_TURNS;
                            StopHoleFlyover();
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            f_PlayStateFrameCount = 0;
                        }
                        else {
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            g_GameState.step = GAME_STATE_END_INTRO;
                        }
                    }
                }
                else if (GetWipeProgress() == WIPE_OPEN_PROGRESS) {
                    if (f_PlayStateFrameCount >= HOLE_INTRO_FRAMES ||
                        IsAButtonPressed() ||
                        (IsPlayAdvancingItself() && f_PlayStateFrameCount >= AUTO_ADVANCE_FRAMES)) {
                        if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                            StartHoleFlyover();
                            StopHoleIntro();
                            g_GameState.step = GAME_STATE_TAKING_TURNS;
                            StopHoleFlyover();
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            f_PlayStateFrameCount = 0;
                        }
                        else {
                            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                            g_GameState.step = GAME_STATE_END_INTRO;
                        }
                    }
                }

                f_PlayStateFrameCount++;

                // The camera circles the hole while its intro is shown
                {
                    Vec3 lookAt;
                    Vec3 eye;
                    char text[DEBUG_TEXT_BUFFER_SIZE];

                    // Debug: circles the chosen cup position instead, and moves the cup
                    // there
                    if (IsGameFlagSet(GAME_FLAG_CHOOSE_CUP)) {
                        if (g_ControllerData[0].trigger & L_TRIG) {
                            f_DebugCupIndex--;
                        }

                        if (g_ControllerData[0].trigger & R_TRIG) {
                            f_DebugCupIndex++;
                        }

                        f_DebugCupIndex &= CUP_POSITION_COUNT - 1;
                        sprintf(text, "PinNo%d ", f_DebugCupIndex);
                        PrintDebugText(text, DEBUG_CUP_TEXT_COLUMN, DEBUG_CUP_TEXT_ROW);
                        g_HoleSetup.cupPosition[AXIS_X] =
                            g_HolePoints.cupPositions[f_DebugCupIndex][AXIS_X];
                        g_HoleSetup.cupPosition[AXIS_Z] =
                            g_HolePoints.cupPositions[f_DebugCupIndex][AXIS_Z];
                        g_HoleSetup.cupPosition[AXIS_Y] = GetGroundHeight(
                            g_HoleSetup.cupPosition[AXIS_X],
                            g_HoleSetup.cupPosition[AXIS_Z]
                        );
                        lookAt[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] * COURSE_TO_WORLD_SCALE;
                        lookAt[AXIS_Y] = g_HoleSetup.cupPosition[AXIS_Y] * COURSE_TO_WORLD_SCALE;
                        lookAt[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z] * COURSE_TO_WORLD_SCALE;
                        eye[AXIS_X] =
                            (cosf(IntroOrbitAngle()) * DEBUG_ORBIT_RADIUS) + lookAt[AXIS_X];
                        eye[AXIS_Y] = lookAt[AXIS_Y] + DEBUG_ORBIT_HEIGHT;
                        eye[AXIS_Z] =
                            (sinf(IntroOrbitAngle()) * DEBUG_ORBIT_RADIUS) + lookAt[AXIS_Z];
                    }
                    else {
                        lookAt[AXIS_X] =
                            (g_HoleSetup.cupPosition[AXIS_X] + g_HoleSetup.teePosition[AXIS_X]) *
                            (COURSE_TO_WORLD_SCALE / 2);
                        lookAt[AXIS_Y] =
                            (g_HoleSetup.cupPosition[AXIS_Y] + g_HoleSetup.teePosition[AXIS_Y]) *
                            (COURSE_TO_WORLD_SCALE / 2);
                        lookAt[AXIS_Z] =
                            (g_HoleSetup.cupPosition[AXIS_Z] + g_HoleSetup.teePosition[AXIS_Z]) *
                            (COURSE_TO_WORLD_SCALE / 2);
                        if (!g_GameState.isMiniGolf) {
                            eye[AXIS_X] =
                                (cosf(IntroOrbitAngle()) * INTRO_ORBIT_RADIUS) + lookAt[AXIS_X];
                            eye[AXIS_Y] = lookAt[AXIS_Y] + INTRO_ORBIT_HEIGHT;
                            eye[AXIS_Z] =
                                (sinf(IntroOrbitAngle()) * INTRO_ORBIT_RADIUS) + lookAt[AXIS_Z];
                        }
                        else {
                            eye[AXIS_X] =
                                (cosf(IntroOrbitAngle()) * MINI_GOLF_ORBIT_RADIUS) + lookAt[AXIS_X];
                            eye[AXIS_Y] = lookAt[AXIS_Y] + MINI_GOLF_ORBIT_HEIGHT;
                            eye[AXIS_Z] =
                                (sinf(IntroOrbitAngle()) * MINI_GOLF_ORBIT_RADIUS) + lookAt[AXIS_Z];
                        }
                    }

                    g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_HOLE_INTRO;
                    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = eye[AXIS_X];
                    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = eye[AXIS_Y];
                    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = eye[AXIS_Z];
                    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = lookAt[AXIS_X];
                    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = lookAt[AXIS_Y];
                    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = lookAt[AXIS_Z];
                }

                break;
            case GAME_STATE_SLOTS:
                if (UpdateClubSlots() == TRUE) {
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    if (g_GameState.gameMode == GAME_MODE_CLUB_SLOTS &&
                        !g_GameState.isRoundContinued) {
                        FreeClubSlots();
                    }

                    g_GameState.step = GAME_STATE_END_INTRO;
                }

                break;
            case GAME_STATE_END_INTRO:
                StepHoleFlyover();
                g_GameState.step = GAME_STATE_FLYOVER;
                StopHoleIntro();
                StartHoleFlyover();
                break;
            case GAME_STATE_FLYOVER:
                g_RoundHud.isHoleIntroTextShown = FALSE;
                g_IsTrainingHeadingKept = FALSE;
                g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
                g_Cameras[INSET_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
                if (StepHoleFlyover() ||
                    (IsHudAnimationDone() && (IsAButtonPressed() || IsPlayAdvancingItself()))) {
                    g_GameState.step = GAME_STATE_TAKING_TURNS;
                    StopHoleFlyover();
                }

                f_PlayStateFrameCount = 0;
                break;
            case GAME_STATE_UNUSED_IDLE:
                f_PlayStateFrameCount++;
                break;
            case GAME_STATE_TAKING_TURNS:
                UnloadOverlay(GetOverlayOfModule(MODULE_HOLE_BUILDER));
            // Falls through
            case GAME_STATE_REPEATING_HOLE:
                if (!f_IsPlayStateBackedUp) {
                    g_ContestMovieState = CONTEST_MOVIES_WAITING;
                    ClearContestWinner();
                    f_IsPlayStateBackedUp = TRUE;
                    BackUpPlayState();
                    g_WasDonkeyKongUnlocked = IsDonkeyKongUnlocked();
                    g_WasMapleUnlocked = IsMapleUnlocked();
                    g_WasMetalMarioUnlocked = IsMetalMarioUnlocked();
                    g_HadEveryRingShotStar = HasEveryRingShotStar();
                    g_RingShotCourseOpenedAtStart = GetRingShotCourseOpenedAtStarCount();
                }

                g_GameState.isRoundContinued = FALSE;
                g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
                g_Cameras[INSET_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
                if (g_IsModuleAlive[MODULE_PLAY] || ReportModuleNotAlive(MODULE_PLAY)) {
                    UpdateTurns();
                }

                PlayContestMovies();
                if (IsHoleOver()) {
                    if (g_GameState.step == GAME_STATE_REPEATING_HOLE) {
                        ChooseConditionsAndInitHole();
                    }
                    else {
                        g_GameState.step = GAME_STATE_HOLE_END;
                        f_HoleEndStep = HOLE_END_STEP_START;
                        f_PlayStateFrameCount = 0;
                    }
                }

                if (g_GameState.step == GAME_STATE_HOLE_END) {
                    UnloadOverlay(GetOverlayOfModule(MODULE_SHOT_SIMULATION));
                    SetNextTeeOrder();
                    UpdateContest(TRUE);
                    if (g_GameState.gameMode == GAME_MODE_TOURNAMENT &&
                        g_Players[0].holeStatus == HOLE_STATUS_HOLED_OUT) {
                        RecordHoleStats();
                        if (g_Players[0].strokeCount < g_HoleSetup.par) {
                            AddBirdieBadge();
                        }
                    }

                    if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                        (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                            g_GameState.gameMode == GAME_MODE_CLUB_SLOTS)) {
                        GroupPlayerMark results[PLAYER_COUNT_MAX];

                        if (GetGroupHoleResult(results) != TRUE) {
                            for (playerSlot = 0; playerSlot < g_GameState.playerCount;
                                playerSlot++) {
                                g_Players[playerSlot].holes[g_GameState.holeIndex].strokeCount = 0;
                            }
                        }
                        else {
                            for (playerSlot = 0; playerSlot < g_GameState.playerCount;
                                playerSlot++) {
                                if (results[playerSlot] != TRUE) {
                                    g_Players[playerSlot].holes[g_GameState.holeIndex].strokeCount =
                                        0;
                                }
                                else {
                                    g_Players[playerSlot].holes[g_GameState.holeIndex].strokeCount =
                                        TRUE;
                                }
                            }
                        }
                    }
                }

                break;
            case GAME_STATE_HOLE_END:
                f_PlayStateFrameCount++;
                clearsHud = TRUE;
                switch (f_HoleEndStep) {
                    case HOLE_END_STEP_START:
                        f_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                        f_PlayStateFrameCount = 0;
                        g_RoundHud.scorecardPage = -1;
                        g_RoundHud.isHoleIntroTextShown = FALSE;
                        StopHoleIntro();
                        for (playerSlot = 0; playerSlot < g_GameState.playerCount; playerSlot++) {
                            g_Players[playerSlot].holeStatus = HOLE_STATUS_PLAYING;
                        }

                        if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            if (f_PlayExit == PLAY_EXIT_NONE) {
                                g_GameState.holeIndex =
                                    (g_GameState.holeIndex + 1) % COURSE_HOLE_COUNT;
                            }
                        }
                        else {
                            g_GameState.holeIndex++;
                        }

                        if (g_GameState.gameMode == GAME_MODE_RECORD_REPLAY ||
                            g_GameState.replayKind != REPLAY_KIND_NONE) {
                            g_GameState.replayKind = REPLAY_KIND_NONE;
                            QuitPlayToTitle();
                            if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                                f_PlayExit = PLAY_EXIT_RING_SHOT;
                            }
                            else {
                                f_PlayExit = PLAY_EXIT_BEST_SHOTS;
                            }

                            StartWipe(WIPE_CLOSING);
                            clearsHud = FALSE;
                            f_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                        }
                        else if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                            if (f_PlayExit == PLAY_EXIT_NONE) {
                                g_GameState.holeIndex %= RING_SHOT_HOLE_COUNT;
                                RememberPlayedHole();
                            }

                            if (f_PlayExit == PLAY_EXIT_NONE) {
                                if (!f_IsRingShotCleared) {
                                    nextStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                                }
                                else if ((IsDonkeyKongUnlocked() && !g_WasDonkeyKongUnlocked) ||
                                    IsGameFlagSet(GAME_FLAG_CHOOSE_UNLOCK_NOTICE)) {
                                    LoadUnlockNotice(UNLOCK_NOTICE_DONKEY_KONG);
                                    nextStep = HOLE_END_STEP_DONKEY_KONG_NOTICE;
                                }
                                else if (!g_RingShotCourseOpenedAtStart &&
                                    GetRingShotCourseOpenedAtStarCount()) {
                                    LoadUnlockNotice(
                                        GetRingShotCourseOpenedAtStarCount() +
                                        UNLOCK_NOTICE_RING_SHOT_COURSE_BASE
                                    );
                                    nextStep = HOLE_END_STEP_RING_SHOT_COURSE_NOTICE;
                                }
                                else if (g_HasBroughtOverCharacter) {
                                    nextStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                                }
                                else {
                                    f_HoleEndStep = HOLE_END_STEP_LOAD_RING_SHOT;
                                    g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                                    g_PendingSubModuleId = MODULE_SAVE_MENU;
                                    // Past the step's store, to match: setting nextStep here
                                    // instead, before or after the module swap, gives 15 words
                                    // differing, 3 in shape, and each branch setting
                                    // f_HoleEndStep itself 24, 16 in shape
                                    goto DIM_BACKDROP;
                                }

                                f_HoleEndStep = nextStep;

DIM_BACKDROP:
                                SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                                clearsHud = FALSE;
                            }
                            else {
                                clearsHud = FALSE;
                                if (f_PlayExit == PLAY_EXIT_QUIT) {
                                    f_HoleEndStep = HOLE_END_STEP_QUIT;
                                }
                            }
                        }
                        else if (g_GameState.gameMode == GAME_MODE_TOURNAMENT) {
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = MODULE_SCORECARD;
                            f_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            f_HoleResultsKind = HOLE_RESULTS_TOURNAMENT;
                            if (IsMapleUnlocked() && !g_WasMapleUnlocked) {
                                LoadUnlockNotice(UNLOCK_NOTICE_MAPLE);
                                f_HoleEndStep = HOLE_END_STEP_UNLOCK_NOTICE;
                            }
                            else if (IsMetalMarioUnlocked() && !g_WasMetalMarioUnlocked) {
                                LoadUnlockNotice(UNLOCK_NOTICE_METAL_MARIO);
                                f_HoleEndStep = HOLE_END_STEP_UNLOCK_NOTICE;
                            }
                            else if (IsGameFlagSet(GAME_FLAG_CHOOSE_UNLOCK_NOTICE)) {
                                LoadUnlockNotice(UNLOCK_NOTICE_METAL_MARIO);
                                f_HoleEndStep = HOLE_END_STEP_UNLOCK_NOTICE;
                            }
                            else if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) ||
                                HasPlayedLastHole()) {
                                RecordCourseRound();
                                RecordTournamentPlace();
                                f_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        }
                        else if (g_GameState.gameMode == GAME_MODE_SPEED_GOLF) {
                            f_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            f_HoleResultsKind = HOLE_RESULTS_SPEED_GOLF;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) ||
                                HasPlayedLastHole()) {
                                f_PlayExit = PLAY_EXIT_ROUND_OVER;
                                InsertBestTime(
                                    g_GameState.courseIndex,
                                    g_GameState.stopwatchFrameCount,
                                    GetNetScoreToPar(COURSE_HOLE_COUNT, 0),
                                    g_Players[0].characterIndex,
                                    g_Players[0].paletteIndex
                                );
                            }
                        }
                        else if (g_GameState.gameMode == GAME_MODE_TRAINING) {
                            f_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                            clearsHud = FALSE;
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            StartWipe(WIPE_CLOSING);
                        }
                        else if (IsOneOnOneMatchMode(g_GameState.gameMode)) {
                            f_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            f_HoleResultsKind = HOLE_RESULTS_MATCH;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) ||
                                GetMatchWinner(0, 1) != MATCH_SIDE_NONE) {
                                f_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        }
                        else if (g_GameState.gameMode == GAME_MODE_SKINS_MATCH ||
                            g_GameState.gameMode == GAME_MODE_CLUB_SLOTS) {
                            f_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            f_HoleResultsKind = HOLE_RESULTS_SKINS;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) ||
                                HasPlayedLastHole()) {
                                f_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        }
                        else {
                            f_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                            g_PendingSubModuleId = MODULE_SCORECARD;
                            SetMenuBackdrop(MENU_BACKDROP_DIMMING);
                            f_HoleResultsKind = HOLE_RESULTS_STROKE;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE)) {
                                f_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                            else if (HasPlayedLastHole()) {
                                if (IsGameFlagSet(GAME_FLAG_REPEAT_ROUND)) {
                                    g_GameState.holeIndex = GetFirstHoleIndex();
                                }
                                else {
                                    f_PlayExit = PLAY_EXIT_ROUND_OVER;
                                }
                            }
                        }

                        if (clearsHud) {
                            g_RoundHud.isWindIndicatorShown = FALSE;
                            g_RoundHud.isHoleInfoShown = FALSE;
                            g_RoundHud.isLieIconShown = FALSE;
                            g_RoundHud.isClubHeadIconShown = FALSE;
                            g_RoundHud.isUnreadItemShown = FALSE;
                            g_RoundHud.isClubPanelShown = FALSE;
                            g_RoundHud.isShotLengthShown = FALSE;
                            g_RoundHud.isProgressShown = FALSE;
                        }

                        g_PauseScreen = PAUSE_SCREEN_SCORECARD;
                        break;
                    case HOLE_END_STEP_DONKEY_KONG_NOTICE:
                        if (UpdateUnlockNotice()) {
                            FreeUnlockNotice();
                            if (!g_RingShotCourseOpenedAtStart &&
                                GetRingShotCourseOpenedAtStarCount()) {
                                LoadUnlockNotice(
                                    GetRingShotCourseOpenedAtStarCount() +
                                    UNLOCK_NOTICE_RING_SHOT_COURSE_BASE
                                );
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_COURSE_NOTICE;
                            }
                            else if (g_HasBroughtOverCharacter) {
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                            }
                            else {
                                f_HoleEndStep = HOLE_END_STEP_LOAD_RING_SHOT;
                                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                                g_PendingSubModuleId = MODULE_SAVE_MENU;
                            }
                        }

                        break;
                    case HOLE_END_STEP_RING_SHOT_COURSE_NOTICE:
                        if (UpdateUnlockNotice()) {
                            FreeUnlockNotice();
                            if (g_HasBroughtOverCharacter) {
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                            }
                            else {
                                f_HoleEndStep = HOLE_END_STEP_LOAD_RING_SHOT;
                                g_SubModuleState = SUB_MODULE_SWAP_PENDING;
                                g_PendingSubModuleId = MODULE_SAVE_MENU;
                            }
                        }

                        break;
                    case HOLE_END_STEP_UNLOCK_NOTICE:
                        if (UpdateUnlockNotice()) {
                            FreeUnlockNotice();
                            f_HoleEndStep = HOLE_END_STEP_LOAD_SCORECARD;
                            if (IsGameFlagSet(GAME_FLAG_END_ROUND_AFTER_HOLE) ||
                                HasPlayedLastHole()) {
                                RecordCourseRound();
                                RecordTournamentPlace();
                                f_PlayExit = PLAY_EXIT_ROUND_OVER;
                            }
                        }

                        break;
                    case HOLE_END_STEP_UNUSED_SCORECARD_WAIT:
                        if (IsAButtonPressed() ||
                            (IsPlayAdvancingItself() &&
                                f_PlayStateFrameCount > AUTO_ADVANCE_RESULTS_FRAMES)) {
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            g_PauseScreen = PAUSE_SCREEN_SCORECARD;
                            ShowFlipMessage(-2);
                            f_HoleEndStep = HOLE_END_STEP_SCORECARD;
                            g_RoundHud.unusedHoleEndValue = 0;
                            FreeResultCharacters();
                            OpenScorecard(
                                GetCourseRecordIndex(g_GameState.courseIndex),
                                g_GameState.holeIndex
                            );
                            StartScorecardTally(0);
                            if (f_PlayExit == PLAY_EXIT_NONE) {
                                StartLoaderThread();
                            }
                        }

                        break;
                    case HOLE_END_STEP_LOAD_SCORECARD:
                        if (IsSubModuleLoaded()) {
                            f_HoleEndStep = HOLE_END_STEP_FREE_SLOT;
                            OpenScorecard(
                                GetCourseRecordIndex(g_GameState.courseIndex),
                                g_GameState.holeIndex
                            );
                            StartScorecardTally(f_HoleResultsKind);
                        }

                        break;
                    case HOLE_END_STEP_FREE_SLOT:
                        if (IsCharacterSlotBusy(PLAYER_CHARACTER_SLOT)) {
                            FreeCharacterSlot(PLAYER_CHARACTER_SLOT);
                            break;
                        }

                        if (f_PlayExit == PLAY_EXIT_NONE) {
                            if (g_GameState.replayKind == REPLAY_KIND_NONE &&
                                IsOneOnOneMatchMode(g_GameState.gameMode) &&
                                HasPlayedLastHole()) {
                                g_GameState.holeIndex = GetFirstHoleIndex();
                                g_GameState.isSuddenDeath = TRUE;
                            }

                            StartLoaderThread();
                        }

                        if (g_RoundHud.scorecardPage != 0) {
                            f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                        }
                        else {
                            f_HoleEndStep = HOLE_END_STEP_SCORECARD;
                        }

                        break;
                    case HOLE_END_STEP_LOAD_RING_SHOT:
                        if (IsSubModuleLoaded()) {
                            f_HoleEndStep = HOLE_END_STEP_RING_SHOT_FREE_SLOT;
                            OpenSaveMenu(SAVE_MENU_REPLAYS);
                        }

                        break;
                    case HOLE_END_STEP_RING_SHOT_FREE_SLOT:
                        if (IsCharacterSlotBusy(PLAYER_CHARACTER_SLOT)) {
                            FreeCharacterSlot(PLAYER_CHARACTER_SLOT);
                            break;
                        }

                        if (f_PlayExit == PLAY_EXIT_NONE) {
                            StartLoaderThread();
                            if (f_IsRingShotCleared && !g_HasBroughtOverCharacter) {
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                            }
                            else {
                                StartWipe(WIPE_CLOSING);
                                if (IsStaffRollDueAfterRingShot()) {
                                    f_HoleEndStep = HOLE_END_STEP_STAFF_ROLL;
                                }
                                else {
                                    f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                                }
                            }
                        }
                        else {
                            f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                            StartWipe(WIPE_CLOSING);
                        }

                        break;
                    case HOLE_END_STEP_RING_SHOT_RESULTS:
                        switch (UpdateReplaySaveMenu(
                            &g_ControllerStates[g_CurrentShot.controllerIndex]
                        )) {
                            case REPLAY_MENU_CLOSED:
                                StartWipe(WIPE_CLOSING);
                                if (IsStaffRollDueAfterRingShot()) {
                                    f_HoleEndStep = HOLE_END_STEP_STAFF_ROLL;
                                }
                                else {
                                    f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                                }

                                break;
                            case REPLAY_MENU_PROMPTING:
                                f_HoleEndStep = HOLE_END_STEP_PROMPT;
                                break;
                        }

                        break;
                    case HOLE_END_STEP_PROMPT:
                        switch (UpdatePrompt(&g_TurnController)) {
                            case PROMPT_RESULT_SKIP_REPLAY:
                                LeaveReplaySaveMenu();
                                // The result, 6, is HOLE_END_STEP_RING_SHOT_FREE_SLOT. Storing
                                // it matches: storing the step's name shares the other arms'
                                // store and is a word shorter
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                                OpenPrompt(NO_PROMPT);
                                break;
                            case PROMPT_RESULT_SAVE_REPLAY:
                                SaveReplayToSlot();
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                                OpenPrompt(NO_PROMPT);
                                break;
                            case PROMPT_RESULT_STAY_IN_SAVE_MENU:
                                g_PauseScreen = PAUSE_SCREEN_SAVE_MENU;
                                OpenPrompt(NO_PROMPT);
                                f_HoleEndStep = HOLE_END_STEP_RING_SHOT_RESULTS;
                                break;
                        }

                        break;
                    case HOLE_END_STEP_SCORECARD:
                        if (UpdateShownScorecard(g_MergedController.repeatedButtons)) {
                            if (IsAButtonPressed() ||
                                (IsPlayAdvancingItself() &&
                                    f_PlayStateFrameCount > AUTO_ADVANCE_FRAMES &&
                                    f_PlayExit != PLAY_EXIT_ROUND_OVER)) {
                                if (f_PlayExit == PLAY_EXIT_ROUND_OVER) {
                                    f_HoleEndStep = HOLE_END_STEP_FINAL_RESULTS;
                                    TallyCoursePoints();
                                    FreeScorecard();
                                    SetMenuBackdrop(MENU_BACKDROP_STILL);
                                    OpenRoundResults(OPEN_ROUND_RESULTS);
                                }
                                else {
                                    f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                                    FreeScorecard();
                                    SetMenuBackdrop(MENU_BACKDROP_STILL);
                                    if (g_GameState.gameMode == GAME_MODE_TOURNAMENT &&
                                        g_GameState.holeIndex == TOURNAMENT_HALF_TURN_HOLE) {
                                        ShowHalfTurn();
                                    }
                                    else {
                                        StartWipe(WIPE_CLOSING);
                                    }
                                }
                            }
                        }

                        break;
                    case HOLE_END_STEP_FINAL_RESULTS:
                        if (UpdateRoundResults(&g_MergedController)) {
                            if (HasWonFinalChallenge() ||
                                IsGameFlagSet(GAME_FLAG_SHOW_STAFF_ROLL)) {
                                f_HoleEndStep = HOLE_END_STEP_STAFF_ROLL;
                            }
                            else {
                                f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                            }

                            StartWipe(WIPE_CLOSING);
                            SetMenuBackdrop(MENU_BACKDROP_STILL);
                            f_PlayExit = PLAY_EXIT_TITLE;
                        }

                        break;
                    case HOLE_END_STEP_STAFF_ROLL:
                        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                            f_HoleEndStep = HOLE_END_STEP_LOAD_NEXT_HOLE;
                            StartWipe(WIPE_OPENING);
                            ClearTextSlots();
                            f_HoleEndStep = HOLE_END_STEP_START;
                            ClearShotEffects(TRUE);
                            g_Scene = SCENE_SWAPPING_MODULES;
                            g_NextScene = SCENE_STAFF_ROLL;
                            g_SceneModuleToUnload = MODULE_PLAY;
                            g_SceneModuleToLoad = MODULE_STAFF_ROLL;
                            UnloadSubModule();
                            EnterMenuAtTitle();
                            StopLoaderThread();
                            f_PlayExit = PLAY_EXIT_NONE;
                            SetMenuBackdrop(MENU_BACKDROP_NONE);
                            SetFramebufferCount(TRIPLE_BUFFERING);
                            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                        }

                        break;
                    case HOLE_END_STEP_WIPE_OUT:
                        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS &&
                            IsHalfTurnDismissed(&g_MergedController)) {
                            ForceStopRumble();
                            f_HoleEndStep = HOLE_END_STEP_LOAD_NEXT_HOLE;
                            if (f_PlayExit != PLAY_EXIT_NONE) {
                                StartWipe(WIPE_OPENING);
                                ClearTextSlots();
                                f_HoleEndStep = HOLE_END_STEP_START;
                                ClearShotEffects(TRUE);
                                g_Scene = SCENE_SWAPPING_MODULES;
                                g_NextScene = SCENE_MENU;
                                g_SceneModuleToUnload = MODULE_PLAY;
                                g_SceneModuleToLoad = MODULE_MENU;
                                UnloadSubModule();
                                switch (f_PlayExit) {
                                    case PLAY_EXIT_QUIT:
                                        EnterMenuAtModeSelect();
                                        break;
                                    case PLAY_EXIT_COURSE_SELECT:
                                        EnterMenuAtCourseSelect();
                                        break;
                                    case PLAY_EXIT_BLANK_SCREEN:
                                        EnterMenuAtBlankScreen();
                                        break;
                                    case PLAY_EXIT_TITLE:
                                        EnterMenuAtTitle();
                                        break;
                                    case PLAY_EXIT_BEST_SHOTS:
                                        EnterMenuAtBestShots();
                                        break;
                                    case PLAY_EXIT_RING_SHOT:
                                        EnterMenuAtRingShotRecords();
                                        break;
                                }

                                StopLoaderThread();
                                f_PlayExit = PLAY_EXIT_NONE;
                                SetMenuBackdrop(MENU_BACKDROP_NONE);
                                SetFramebufferCount(TRIPLE_BUFFERING);
                                SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                            }
                            else {
                                RequestSubModule();
                            }
                        }

                        break;
                    case HOLE_END_STEP_LOAD_NEXT_HOLE:
                        if (g_HasLoaderFinished == TRUE && IsSubModuleLoaded()) {
                            g_GameState.step = GAME_STATE_HOLE_INTRO;
                            SetGameFlag(GAME_FLAG_HOLE_LOADED);
                            memset(&g_RoundHud, 0, sizeof(RoundHud));
                            SetMenuBackdrop(MENU_BACKDROP_NONE);
                            g_PauseScreen = PAUSE_SCREEN_NONE;
                            SetFramebufferCount(TRIPLE_BUFFERING);
                            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
                            StartHoleIntro();
                            StartHoleCamerasAndSong();
                            if (g_GameState.gameMode == GAME_MODE_RING_SHOT) {
                                SetPlankMotion(TRUE);
                            }

                            StartWipe(WIPE_OPENING);
                            f_PlayStateFrameCount = 0;
                            f_HoleEndStep = HOLE_END_STEP_START;
                        }

                        break;
                    case HOLE_END_STEP_QUIT:
                        if (IsCharacterSlotBusy(PLAYER_CHARACTER_SLOT)) {
                            FreeCharacterSlot(PLAYER_CHARACTER_SLOT);
                            break;
                        }

                        StartWipe(WIPE_CLOSING);
                        f_HoleEndStep = HOLE_END_STEP_WIPE_OUT;
                        break;
                }

                break;
        }

        again = repeats;
    }
    while (again);
}

// Fills f_GroundNoiseTexels with random intensities from base up
void FillGroundNoise(s32 range, s32 base) {
    s32 texelIndex = 0;
    s32 row;
    s32 column;
    s32 intensity;

    for (row = 0; row < GROUND_NOISE_SIZE; row++) {
        for (column = 0; column < GROUND_NOISE_SIZE; column++) {
            intensity = (rand() % range) + base;
            if (intensity > COLOR_CHANNEL_MAX) {
                intensity = COLOR_CHANNEL_MAX;
            }

            f_GroundNoiseTexels[texelIndex++] = intensity;
        }
    }
}

void InitExpansionPakFeatures(void) {
    if (g_HasExpansionPak) {
        FillWaveTexels();
    }

    FillGroundNoise(GROUND_NOISE_RANGE, GROUND_NOISE_BASE);
}

// Sets the fog range and color from g_SceneFog, or from g_DebugFog,
// with no fog alpha, while GAME_FLAG_SKY_EDITOR is set
void SetSceneFog(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    if (IsGameFlagSet(GAME_FLAG_SKY_EDITOR)) {
        gSPFogPosition(gfx++, g_DebugFog.near, g_DebugFog.far);
        gDPSetFogColor(
            gfx++,
            g_DebugFog.color[RED_CHANNEL],
            g_DebugFog.color[GREEN_CHANNEL],
            g_DebugFog.color[BLUE_CHANNEL],
            0
        );
    }
    else {
        gSPFogPosition(gfx++, g_SceneFog.near, g_SceneFog.far);
        gDPSetFogColor(
            gfx++,
            g_SceneFog.color[RED_CHANNEL],
            g_SceneFog.color[GREEN_CHANNEL],
            g_SceneFog.color[BLUE_CHANNEL],
            0xFF
        );
    }

    *displayListHead = gfx;
}

void CopyDoublePair(f64 *from, f64 *to) {
    to[0] = from[0];
    to[1] = from[1];
}

// Unless on a mini-golf course, marks the ground tile holding a point
// and the tiles around it in g_IsGroundTileNearby
void MarkNearbyGroundTiles(u32 x, u32 z) {
    u32 column;
    u32 row;
    s32 rowOffset;
    s32 columnOffset;

    if (!g_GameState.isMiniGolf) {
        column = x >> GROUND_TILE_SHIFT;
        row = z >> GROUND_TILE_SHIFT;
        if (column < GROUND_TILE_COLUMN_COUNT) {
            if (row < GROUND_TILE_ROW_COUNT) {
                for (rowOffset = -1; rowOffset < 2; rowOffset++) {
                    for (columnOffset = -1; columnOffset < 2; columnOffset++) {
                        if (column + columnOffset < GROUND_TILE_COLUMN_COUNT &&
                            row + rowOffset < GROUND_TILE_ROW_COUNT) {
                            g_IsGroundTileNearby
                                [((row + rowOffset) * GROUND_TILE_COLUMN_COUNT) +
                                    (column + columnOffset)] = TRUE;
                        }
                    }
                }
            }
        }
    }
}

void ClearNearbyGroundTiles(void) {
    bzero(g_IsGroundTileNearby, sizeof(g_IsGroundTileNearby));
}

// The test ground texture repeats, mirrored, every this many units
#define TEST_TEXTURE_TILE_SIZE 512

// Gives each ground point texture coordinates from its x and z, mirrored on
// every other tile so that the texture tiles seamlessly, under
// GAME_FLAG_TEST_GROUND_TEXTURE.
//
// Each coordinate is copied to an s32, and its offset in the tile is taken
// as a remainder before the tile's number as a quotient. GCC works the
// remainder out as the coordinate less the quotient times the size, with the
// quotient in the offset's own register on the way. cse then finds that the
// tile's division is the one just done, and makes the tile a copy of that
// quotient: the original's move of each tile number from $a0 to $v1. Read
// straight from the point, the coordinate is a short, and the remainder is
// taken in 16 bits and sign-extended.
//
// One offset and one copy serve both axes. With an offset for each, the
// offsets take $v0 and the coordinates $a0, where the original has $a0 and
// $a1.
//
// An offset of the tile's size stores the size's texel coordinate, which is
// what the other arm would store; without the test the original's beq is
// gone.
//
// Measured against the match, as words differing and how many of them in
// shape:
//
//   The tile's quotient before the offset's remainder       34 (4)
//   The coordinate read from the point in each, with or
//   without a cast to s32                                   52 (10)
//   The copy as an s16                                      52 (10)
//   The copy as a u16                                       36 (28)
//   The copy as a u32                                       26 (20)
//   A copy for each axis, or one declared in a block
//   around each                                             50 (4)
//   An offset for each axis, or so declared                 44 (0)
//   The offset as an s16                                    44 (12)
//   The offset as a u16                                     42 (10)
//   The offset as the coordinate less the tile times the
//   size, after the tile                                    46 (6)
//   The same from the point, with an offset for each axis
//   and no copy (the old draft)                             30 (6)
//   The quotient set in the offset and copied to the
//   tile, then the offset as the coordinate less the
//   tile times the size                                     34 (4)
//   The offset as coordinate & (size - 1)                   33 (11)
//   An inline function for the mirrored offset              54 (8)
//   The same, taking the coordinate as an s16               52 (16)
//   No test of the offset against the size                  36 (22)
//   That test as ==, with its arms swapped                   8 (4)
//   0x100 added in each arm of the test                     25 (17)
//   The coordinates read through GetGroundPoint again       23 (17)
//   The row loop or the column loop testing !=               4 (4)
//
// Also matching: the three locals declared in any order; the tile as an
// s16, u16 or u32, one for each axis, or declared in a block; no tile at
// all, with the quotient written in the mirror's test; that test as & 1 or
// != 0; the offset as a u32; and the mirror or the offset's test as a
// conditional expression.
void RebuildGroundTextureCoordinates(void) {
    Vtx_t point;
    s32 row;
    s32 column;
    s32 tileIndex;
    s32 offset;
    s32 coordinate;

    ClearGameFlag(GAME_FLAG_HOLE_LOADED);
    for (row = 0; row < GROUND_POINT_ROW_COUNT; row++) {
        for (column = 0; column < GROUND_POINT_COLUMN_COUNT; column++) {
            point = GetGroundPoint(column, row)->v;
            // The remainder comes before the quotient, both taken from an s32
            // copy, to match
            coordinate = point.ob[AXIS_X];
            offset = coordinate % TEST_TEXTURE_TILE_SIZE;
            tileIndex = coordinate / TEST_TEXTURE_TILE_SIZE;
            if (tileIndex % 2) {
                offset = TEST_TEXTURE_TILE_SIZE - offset;
            }

            if (offset != TEST_TEXTURE_TILE_SIZE) {
                point.tc[TEXTURE_AXIS_S] = TexelCoordinate(offset);
            }
            else {
                point.tc[TEXTURE_AXIS_S] = TexelCoordinate(TEST_TEXTURE_TILE_SIZE);
            }

            point.tc[TEXTURE_AXIS_S] += 0x100;
            coordinate = point.ob[AXIS_Z];
            offset = coordinate % TEST_TEXTURE_TILE_SIZE;
            tileIndex = coordinate / TEST_TEXTURE_TILE_SIZE;
            if (tileIndex % 2) {
                offset = TEST_TEXTURE_TILE_SIZE - offset;
            }

            if (offset != TEST_TEXTURE_TILE_SIZE) {
                point.tc[TEXTURE_AXIS_T] = TexelCoordinate(offset);
            }
            else {
                point.tc[TEXTURE_AXIS_T] = TexelCoordinate(TEST_TEXTURE_TILE_SIZE);
            }

            point.tc[TEXTURE_AXIS_T] += 0x100;
            SetGroundPoint(column, row, &point);
        }
    }
}

// Whether the neighbor makes it snap the edge's midpoints
#define IsCoarserThanFine(tileIndex) \
    (g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_COARSE || \
        g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_MEDIUM)

// Sets a vertex halfway between two others
#define SetAverageVertex(midpoint, from, to) \
    { \
        (midpoint)->v.ob[AXIS_X] = ((from)->v.ob[AXIS_X] + (to)->v.ob[AXIS_X]) / 2; \
        (midpoint)->v.ob[AXIS_Y] = ((from)->v.ob[AXIS_Y] + (to)->v.ob[AXIS_Y]) / 2; \
        (midpoint)->v.ob[AXIS_Z] = ((from)->v.ob[AXIS_Z] + (to)->v.ob[AXIS_Z]) / 2; \
        (midpoint)->v.flag = 0; \
        (midpoint)->v.tc[TEXTURE_AXIS_S] = \
            ((from)->v.tc[TEXTURE_AXIS_S] + (to)->v.tc[TEXTURE_AXIS_S]) / 2; \
        (midpoint)->v.tc[TEXTURE_AXIS_T] = \
            ((from)->v.tc[TEXTURE_AXIS_T] + (to)->v.tc[TEXTURE_AXIS_T]) / 2; \
    }

// Loads a row's odd vertices as averages of its even ones, so that they line up
// with a coarse row
#define LoadAveragedRow(row, slot) \
    { \
        Vtx *even = (row); \
        SetAverageVertex(&vertices[0], &even[0], &even[1]); \
        SetAverageVertex(&vertices[1], &even[1], &even[2]); \
        SetAverageVertex(&vertices[2], &even[2], &even[3]); \
        SetAverageVertex(&vertices[3], &even[3], &even[4]); \
        gSPVertex(gfx++, vertices, 4, slot); \
        vertices += 4; \
    }

// Loads a tile's texture, skipping its one-texel border. The tile is the
// groupColumn-th from the one whose S starts at firstS.
#define LoadTileTexture(tileIndex, firstS, groupColumn, row) \
    { \
        gDPLoadMultiBlock( \
            gfx++, \
            TextureAddress(&g_WorkArea.hole.tiles[tileIndex]), \
            0, \
            1, \
            G_IM_FMT_RGBA, \
            G_IM_SIZ_16b, \
            GROUND_TILE_TEXEL_ROW_LENGTH, \
            GROUND_TILE_TEXEL_ROW_COUNT, \
            0, \
            G_TX_MIRROR | G_TX_WRAP, \
            G_TX_NOMIRROR | G_TX_CLAMP, \
            6, \
            G_TX_NOMASK, \
            G_TX_NOLOD, \
            G_TX_NOLOD \
        ); \
        gDPSetTileSize( \
            gfx++, \
            1, \
            (firstS) + ((groupColumn) * TILE_COORDINATE_SPAN) + BORDERED_TILE_FIRST_COORDINATE, \
            ((row) * TILE_COORDINATE_SPAN) + BORDERED_TILE_FIRST_COORDINATE, \
            (firstS) + (((groupColumn) + 1) * TILE_COORDINATE_SPAN) + BORDERED_TILE_S_PAST_NEXT, \
            (((row) + 1) * TILE_COORDINATE_SPAN) + BORDERED_TILE_T_PAST_NEXT \
        ); \
    }

// Loads the noise texture that DrawGround blends over the tiles
#define LoadNoiseTexture(shiftS, shiftT) \
    gDPLoadMultiBlock( \
        gfx++, \
        TextureAddress(f_GroundNoiseTexels), \
        NOISE_TMEM, \
        G_TX_RENDERTILE, \
        G_IM_FMT_I, \
        G_IM_SIZ_8b, \
        GROUND_NOISE_SIZE, \
        GROUND_NOISE_SIZE, \
        0, \
        G_TX_MIRROR, \
        G_TX_MIRROR, \
        TextureMask(GROUND_NOISE_SIZE), \
        TextureMask(GROUND_NOISE_SIZE), \
        shiftS, \
        shiftT \
    )

// Draws the ground's tiles, each at a detail that its distance picks. The
// tiles go down two halves in rows of four, and a fine tile snaps the edge
// vertices it shares with coarser neighbors so that no gaps open between them.
//
// To match, with the words that differ from the original's otherwise and how
// many of them differ in shape:
// - GridVertex is an inline function that returns a local, so the compiler
//   works out a half's first column, the column after it and that column's
//   parity once per half, and keeps each on the stack, as the original does.
//   As a macro it gives 843 words (97 in shape) and a frame 0x48 bytes
//   smaller, and returning the address without the local 712 (12).
// - The half's first column is written out at each use. In a local set once
//   per half it gives 1100 words (68) and a frame 0x10 bytes smaller.
// - The fine tiles work out their column at each test for the ground's left
//   and right edges: in a local, 602 words (12).
// - The fine and the medium groups each declare their own k: one declared for
//   the row, or with the routine's locals, gives 272 words (50).
// - The loop over a fine tile's vertex segments has a counter of its own and
//   sets the segment's offset into a local. Counted in j it gives 64 words
//   and in segmentIndex 44; with the offset written as gSPSegment's argument,
//   53 (21).
// - vertices, start and current are declared in that order, and firstTileS
//   with them rather than in the half's block, so their stack slots fall in
//   the original's order: start and current first gives 46 words, and
//   firstTileS in the half's block 52.
// - The culling loop takes the tile's address into a local (10 words, 2 in
//   shape, without it) and sets the hidden detail in the visibility test's
//   else arm (6, all in shape, when set ahead of the test).
// - A medium tile among fine ones draws its quads a row at a time, where
//   DrawTileQuads goes down the columns: with DrawTileQuads, 16 words.
// - That tile's vertex columns add the tile's offset to the half's first
//   column: the other way round, 2 words.
//
// LoadTileTexture adds the tile's place in its group to firstTileS, the
// half's first S, since the original steps two registers by 0x80 in each tile
// loop: written as the tile's column times 128, loop.c can't make the shift a
// giv. A fine tile drawn from its display list steps the tile index itself and
// continues, as the original goes straight from there to the next tile.
void DrawGround(CameraMatrices *matrices, TerrainStyle terrainStyle) {
    UBoolean8 snapsEdge[2][TILE_VERTEX_SEGMENT_COUNT]; // For the current and the last fine tile
    Gfx *gfx;
    s32 half;
    Vtx *vertices;
    u32 start;
    u8 currentSnapSet;
    s32 firstTileS;
    f32 fineScale;
    f32 radius;
    s32 visibility;
    GroundDetail detail;
    GroundDetail detailMax;
    GroundDetail previousDetail;
    s32 edgeIndex;
    s32 culledTileIndex;
    s32 culledOffset;
    s32 segmentIndex;
    s32 edgeSegment;
    s32 row;
    s32 groupTileIndex;
    s32 detailTileIndex;
    s32 pass;
    s32 mediumPass;
    s32 passIndex;
    s32 tileIndex;
    s32 topSlot;
    s32 middleSlot;
    s32 bottomSlot;
    s32 spareSlot;
    GroundTile *tiles;
    GroundTile *tile;

    // SetGroundCombineMode is a nested function, a GNU C extension. It
    // blends the noise texture over the tile texture.
    void SetGroundCombineMode(s32 unusedFirstSetting UNUSED, s32 unusedSecondSetting UNUSED) {
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPSetCombineLERP(
            gfx++,
            TEXEL0,
            0,
            TEXEL1,
            0,
            ENVIRONMENT,
            SHADE,
            TEXEL1,
            ENVIRONMENT,
            COMBINED,
            TEXEL0,
            COMBINED_ALPHA,
            TEXEL0,
            0,
            0,
            0,
            1
        );
    }

    currentSnapSet = 0;
    vertices = g_VertexListHead;
    gfx = g_DisplayListHead;
    start = osGetCount();
    if (IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE)) {
        DrawHoleEditorGround(matrices, terrainStyle);
        return;
    }

    if (!g_HasLoaderFinished) {
        return;
    }

    g_GroundDrawFrame = g_FrameCount;
    if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
        RebuildGroundTextureCoordinates();
    }

    if (IsGameFlagSet(GAME_FLAG_CYCLE_CLIP_RATIO)) {
        char text[CLIP_RATIO_TEXT_SIZE];

        sprintf(text, "CLIPRATIO=%d", f_GroundClipRatio);
        PrintDebugText(text, CLIP_RATIO_TEXT_COLUMN, CLIP_RATIO_TEXT_ROW);
        switch (f_GroundClipRatio) {
            case 1:
                gSPClipRatio(gfx++, FRUSTRATIO_1);
                break;
            case 2:
                gSPClipRatio(gfx++, FRUSTRATIO_2);
                break;
            case 3:
                gSPClipRatio(gfx++, FRUSTRATIO_3);
                break;
            case 4:
                gSPClipRatio(gfx++, FRUSTRATIO_4);
                break;
            case 5:
                gSPClipRatio(gfx++, FRUSTRATIO_5);
                break;
            case 6:
                gSPClipRatio(gfx++, FRUSTRATIO_6);
                break;
        }

        if (g_ControllerData[1].trigger & A_BUTTON) {
            f_GroundClipRatio = (f_GroundClipRatio % CLIP_RATIO_COUNT) + 1;
        }
    }
    else {
        gSPClipRatio(gfx++, FRUSTRATIO_1);
    }

    if (IsGameFlagSet(GAME_FLAG_HIDE_GROUND)) {
        return;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipelineMode(gfx++, G_PM_1PRIMITIVE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gSPLoadGeometryMode(gfx++, 0);
    if (g_GameState.isMiniGolf) {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
    }
    else {
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
    }

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureLOD(gfx++, G_TL_TILE);
    gDPPipeSync(gfx++);
    gDPSetTextureDetail(gfx++, G_TD_CLAMP);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    if (terrainStyle == TERRAIN_STYLE_NORMAL) {
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_2CYCLE);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_FOG_SHADE_A, G_RM_AA_ZB_TEX_EDGE2);
        gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_CULL_BACK | G_FOG | G_SHADING_SMOOTH);
        SetSceneFog(&gfx);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
            if (g_GameState.isMiniGolf) {
                LoadNoiseTexture(15, G_TX_NOLOD);
            }
            else {
                LoadNoiseTexture(13, 13);
            }
        }
        else {
            LoadNoiseTexture(13, 15);
        }

        SetGroundCombineMode(0, 0);
        gDPSetEnvColor(gfx++, 0, 0, 0, NOISE_BLEND_ALPHA);
        gDPPipeSync(gfx++);
    }

    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    g_GroundFarthestDepth = 0.0f;
    g_GroundNearDepthEighth = NO_NEAR_DEPTH;
    fineScale = FINE_DEPTH_SCALE;
    if (IsCloseUpCameraMode(g_Cameras[MAIN_CAMERA].mode)) {
        fineScale = ZOOMED_FINE_DEPTH_SCALE;
    }

    detailMax = GROUND_DETAIL_COARSE;
    tiles = g_WorkArea.hole.tiles;
    for (culledTileIndex = 0; culledTileIndex < GROUND_TILE_COUNT; culledTileIndex++) {
        tile = &tiles[culledTileIndex];
        radius = (g_GroundTileHeightRanges[culledTileIndex] >= GROUND_TILE_RADIUS_MIN) ?
            g_GroundTileHeightRanges[culledTileIndex] :
            GROUND_TILE_RADIUS_MIN;
        visibility = GetSphereVisibility(
            tile->center[AXIS_X],
            tile->center[AXIS_Y],
            tile->center[AXIS_Z],
            radius
        );
        if (visibility < 0) {
            if (tile->midpointBlockIndex >= 0 &&
                (g_IsGroundTileNearby[culledTileIndex] ||
                    visibility * fineScale < g_GroundTileHeightRanges[culledTileIndex])) {
                detail = GROUND_DETAIL_FINE;
            }
            else if (visibility * MEDIUM_DEPTH_SCALE < g_GroundTileHeightRanges[culledTileIndex]) {
                detail = GROUND_DETAIL_MEDIUM;
            }
            else {
                detail = GROUND_DETAIL_COARSE;
            }
        }
        else {
            detail = GROUND_DETAIL_HIDDEN;
        }

        // The map scenes draw every visible tile finely
        if (IsGroundEditorScene(g_Scene) &&
            IsMapTerrainStyle(terrainStyle) &&
            detail != GROUND_DETAIL_HIDDEN) {
            detail = GROUND_DETAIL_FINE;
        }

        if (detailMax < detail) {
            detailMax = detail;
        }

        tile->detail = detail;
        if ((culledTileIndex & (GROUND_TILE_GROUP_SIZE - 1)) == GROUND_TILE_GROUP_SIZE - 1) {
            // Coarse tiles in a group with finer ones are drawn at medium detail
            if (detailMax > GROUND_DETAIL_COARSE) {
                for (culledOffset = 0; culledOffset < GROUND_TILE_GROUP_SIZE; culledOffset++) {
                    if (tiles[culledTileIndex - culledOffset].detail == GROUND_DETAIL_COARSE) {
                        tiles[culledTileIndex - culledOffset].detail = GROUND_DETAIL_MEDIUM;
                    }
                }
            }

            detailMax = GROUND_DETAIL_COARSE;
        }

        if (detail > GROUND_DETAIL_HIDDEN) {
            if (visibility < g_GroundFarthestDepth) {
                g_GroundFarthestDepth = visibility;
            }

            if (g_GroundNearDepthEighth < visibility / NEAR_DEPTH_DIVISOR) {
                g_GroundNearDepthEighth = visibility / NEAR_DEPTH_DIVISOR;
            }
        }
    }

    ClearNearbyGroundTiles();
    if (IsGameFlagSet(GAME_FLAG_TEST_GROUND_TEXTURE)) {
        osSyncPrintf("%f\n", g_GroundFarthestDepth);
    }

    for (edgeIndex = 0; edgeIndex < ARRAY_COUNT(snapsEdge); edgeIndex++) {
        for (segmentIndex = 0; segmentIndex < TILE_VERTEX_SEGMENT_COUNT; segmentIndex++) {
            snapsEdge[edgeIndex][segmentIndex] = FALSE;
        }
    }

    gSPSegment(gfx++, 1, 0);
    gSPSegment(gfx++, 2, 0);
    gSPSegment(gfx++, 3, 0);
    gSPSegment(gfx++, 4, 0);
    gSPSegment(gfx++, 5, 0);
    gSPSegment(gfx++, 6, 0);
    gSPSegment(gfx++, 7, 0);
    gSPSegment(gfx++, 8, 0);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    SetGroundCombineMode(0, 0);
    for (half = 0; half < GROUND_HALF_COUNT; half++) {
        previousDetail = GROUND_DETAIL_HIDDEN;
        tileIndex = half * GROUND_TILE_GROUP_SIZE;
        topSlot = FIRST_TOP_SLOT;
        firstTileS = tileIndex * TILE_COORDINATE_SPAN;
        middleSlot = FIRST_MIDDLE_SLOT;
        bottomSlot = FIRST_BOTTOM_SLOT;
        if (IsGameFlagSet(GAME_FLAG_LOG_GROUND_ROWS)) {
            osSyncPrintf("DG[");
            if (half != 0) {
                break;
            }
        }

        for (row = 0; row < GROUND_TILE_ROW_COUNT; row++) {
            GroundDetail groupDetail;

            groupDetail = GROUND_DETAIL_COARSE;
            for (detailTileIndex = 0; detailTileIndex < GROUND_TILE_GROUP_SIZE; detailTileIndex++) {
                if (groupDetail < g_WorkArea.hole.tiles[tileIndex + detailTileIndex].detail) {
                    groupDetail = g_WorkArea.hole.tiles[tileIndex + detailTileIndex].detail;
                }
            }

            if (groupDetail >= GROUND_DETAIL_FINE) {
                s32 groupOffset;

                for (groupOffset = 0; groupOffset < GROUND_TILE_GROUP_SIZE; groupOffset++) {
                    if (IsGameFlagSet(GAME_FLAG_LOG_GROUND_ROWS)) {
                        osSyncPrintf("%d", g_WorkArea.hole.tiles[tileIndex].detail);
                    }
                    if (g_WorkArea.hole.tiles[tileIndex].detail == GROUND_DETAIL_FINE &&
                        g_WorkArea.hole.tiles[tileIndex].midpointBlockIndex >= 0) {
                        if (IsGameFlagSet(GAME_FLAG_HOLE_LOADED)) {
                            if ((half * GROUND_TILE_GROUP_SIZE) + groupOffset != 0) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_LEFT] =
                                    IsCoarse(tileIndex - 1);
                                snapsEdge[currentSnapSet][TILE_EDGE_LEFT] =
                                    IsCoarserThanFine(tileIndex - 1);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_LEFT] = FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_LEFT] = FALSE;
                            }

                            if (row != 0) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_TOP] =
                                    IsCoarse(tileIndex - GROUND_TILE_COLUMN_COUNT);
                                snapsEdge[currentSnapSet][TILE_EDGE_TOP] =
                                    IsCoarserThanFine(tileIndex - GROUND_TILE_COLUMN_COUNT);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_TOP] = FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_TOP] = FALSE;
                            }

                            if (row != GROUND_TILE_ROW_COUNT - 1) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_BOTTOM] =
                                    IsCoarse(tileIndex + GROUND_TILE_COLUMN_COUNT);
                                snapsEdge[currentSnapSet][TILE_EDGE_BOTTOM] =
                                    IsCoarserThanFine(tileIndex + GROUND_TILE_COLUMN_COUNT);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_BOTTOM] =
                                    FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_BOTTOM] = FALSE;
                            }

                            if ((half * GROUND_TILE_GROUP_SIZE) + groupOffset !=
                                GROUND_TILE_COLUMN_COUNT - 1) {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_RIGHT] =
                                    IsCoarse(tileIndex + 1);
                                snapsEdge[currentSnapSet][TILE_EDGE_RIGHT] =
                                    IsCoarserThanFine(tileIndex + 1);
                            }
                            else {
                                snapsEdge[currentSnapSet][TILE_EDGE_COUNT + TILE_EDGE_RIGHT] =
                                    FALSE;
                                snapsEdge[currentSnapSet][TILE_EDGE_RIGHT] = FALSE;
                            }

                            for (edgeSegment = 0; edgeSegment < TILE_VERTEX_SEGMENT_COUNT;
                                edgeSegment++) {
                                // Moves a snapped edge's vertices, or moves back those snapped
                                // for the last tile
                                if (snapsEdge[currentSnapSet][edgeSegment] ||
                                    snapsEdge[currentSnapSet ^ 1][edgeSegment]) {
                                    s32 offset;

                                    offset = snapsEdge[currentSnapSet][edgeSegment] ?
                                        g_CoarseVertexOffsets[tileIndex][edgeSegment] :
                                        0;
                                    gSPSegment(gfx++, edgeSegment + 1, offset);
                                }
                            }

                            currentSnapSet ^= 1;
                            gSPDisplayList(gfx++, g_GroundTileDisplayLists[tileIndex]);
                            tileIndex++;
                            continue;
                        }
                    }
                    else if (g_WorkArea.hole.tiles[tileIndex].detail > GROUND_DETAIL_HIDDEN) {
                        // A medium tile among fine ones loads only its own vertices
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupOffset * 2), row * 2),
                            2,
                            0
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupOffset * 2) + 1, row * 2),
                            1,
                            2
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupOffset * 2), (row * 2) + 1),
                            2,
                            3
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(
                                HalfFirstColumn(half) + (groupOffset * 2) + 1,
                                (row * 2) + 1
                            ),
                            1,
                            5
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(HalfFirstColumn(half) + (groupOffset * 2), (row * 2) + 2),
                            2,
                            6
                        );
                        gSPVertex(
                            gfx++,
                            GridVertex(
                                HalfFirstColumn(half) + (groupOffset * 2) + 1,
                                (row * 2) + 2
                            ),
                            1,
                            8
                        );
                        for (mediumPass = 0; mediumPass < GROUND_TILE_PASSES_MAX; mediumPass++) {
                            LoadTileTexture(tileIndex, firstTileS, groupOffset, row);
                            // The quads go a row at a time, where DrawTileQuads goes
                            // down the columns
                            gSP2Triangles(gfx++, 0, 2, 3, 0, 2, 5, 3, 0);
                            gSP2Triangles(gfx++, 2, 1, 5, 0, 1, 4, 5, 0);
                            gSP2Triangles(gfx++, 3, 5, 6, 0, 5, 8, 6, 0);
                            gSP2Triangles(gfx++, 5, 4, 8, 0, 4, 7, 8, 0);
                            if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                break;
                            }
                        }
                    }

                    tileIndex++;
                }
            }
            else if (groupDetail > GROUND_DETAIL_COARSE) {
                s32 groupOffset;

                spareSlot = middleSlot;
                middleSlot = topSlot;
                topSlot = bottomSlot;
                bottomSlot = spareSlot;
                gDPPipeSync(gfx++);

                // The last row's bottom vertices are still in the cache, unless it was
                // fine or skipped
                if (previousDetail <= GROUND_DETAIL_HIDDEN ||
                    previousDetail == GROUND_DETAIL_FINE) {
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half), row * 2),
                        GROUND_TILE_GROUP_SIZE + 1,
                        topSlot
                    );
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half) + 1, row * 2),
                        GROUND_TILE_GROUP_SIZE,
                        topSlot + ODD_SLOT
                    );
                }
                else if (previousDetail == GROUND_DETAIL_COARSE) {
                    LoadAveragedRow(GridVertex(HalfFirstColumn(half), row * 2), topSlot + ODD_SLOT);
                }

                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half), (row * 2) + 1),
                    GROUND_TILE_GROUP_SIZE + 1,
                    middleSlot
                );
                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half) + 1, (row * 2) + 1),
                    GROUND_TILE_GROUP_SIZE,
                    middleSlot + ODD_SLOT
                );
                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half), (row * 2) + 2),
                    GROUND_TILE_GROUP_SIZE + 1,
                    bottomSlot
                );
                if (row != GROUND_TILE_ROW_COUNT - 1 &&
                    g_WorkArea.hole.tiles[tileIndex + GROUND_TILE_COLUMN_COUNT].detail ==
                        GROUND_DETAIL_COARSE) {
                    LoadAveragedRow(
                        GridVertex(HalfFirstColumn(half), (row * 2) + 2),
                        bottomSlot + ODD_SLOT
                    );
                }
                else {
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half) + 1, (row * 2) + 2),
                        GROUND_TILE_GROUP_SIZE,
                        bottomSlot + ODD_SLOT
                    );
                }

                for (groupOffset = 0; groupOffset < GROUND_TILE_GROUP_SIZE; groupOffset++) {
                    if (g_WorkArea.hole.tiles[tileIndex].detail > GROUND_DETAIL_HIDDEN) {
                        for (pass = 0; pass < GROUND_TILE_PASSES_MAX; pass++) {
                            LoadTileTexture(tileIndex, firstTileS, groupOffset, row);
                            DrawTileQuads(
                                topSlot,
                                topSlot + ODD_SLOT,
                                topSlot + 1,
                                middleSlot,
                                middleSlot + ODD_SLOT,
                                middleSlot + 1,
                                bottomSlot,
                                bottomSlot + ODD_SLOT,
                                bottomSlot + 1
                            );
                            if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                break;
                            }
                        }
                    }

                    topSlot++;
                    middleSlot++;
                    bottomSlot++;
                    tileIndex++;
                }

                topSlot -= GROUND_TILE_GROUP_SIZE;
                bottomSlot -= GROUND_TILE_GROUP_SIZE;
                middleSlot -= GROUND_TILE_GROUP_SIZE;
            }
            else if (groupDetail >= GROUND_DETAIL_COARSE) {
                spareSlot = middleSlot;
                middleSlot = topSlot;
                topSlot = bottomSlot;
                bottomSlot = spareSlot;
                if (previousDetail <= GROUND_DETAIL_HIDDEN ||
                    previousDetail == GROUND_DETAIL_FINE) {
                    gSPVertex(
                        gfx++,
                        GridVertex(HalfFirstColumn(half), row * 2),
                        GROUND_TILE_GROUP_SIZE + 1,
                        topSlot
                    );
                }

                gSPVertex(
                    gfx++,
                    GridVertex(HalfFirstColumn(half), (row * 2) + 2),
                    GROUND_TILE_GROUP_SIZE + 1,
                    bottomSlot
                );
                for (groupTileIndex = 0; groupTileIndex < GROUND_TILE_GROUP_SIZE;
                    groupTileIndex++) {
                    if (g_WorkArea.hole.tiles[tileIndex].detail > GROUND_DETAIL_HIDDEN) {
                        for (passIndex = 0; passIndex < GROUND_TILE_PASSES_MAX; passIndex++) {
                            LoadTileTexture(tileIndex, firstTileS, groupTileIndex, row);
                            gSP2Triangles(
                                gfx++,
                                topSlot,
                                topSlot + 1,
                                bottomSlot,
                                0,
                                topSlot + 1,
                                bottomSlot + 1,
                                bottomSlot,
                                0
                            );
                            if (g_WorkArea.hole.tiles[tileIndex].extraPassCount <= 0) {
                                break;
                            }
                        }
                    }

                    topSlot++;
                    bottomSlot++;
                    tileIndex++;
                }

                topSlot -= GROUND_TILE_GROUP_SIZE;
                bottomSlot -= GROUND_TILE_GROUP_SIZE;
            }
            else {
                tileIndex += GROUND_TILE_GROUP_SIZE;
            }

            previousDetail = groupDetail;

            // Skips the other half's group
            tileIndex += GROUND_TILE_GROUP_SIZE;
            if (IsGameFlagSet(GAME_FLAG_LOG_GROUND_ROWS)) {
                osSyncPrintf("]\n");
            }
        }
    }

    g_VertexListHead = vertices;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipelineMode(gfx++, G_PM_NPRIMITIVE);
    gDPPipeSync(gfx++);
    g_DisplayListHead = gfx;
    if (IsGameFlagSet(GAME_FLAG_TIME_GROUND)) {
        start = osGetCount() - start;
        PrintDebugPaddedHex(
            start / GROUND_TIMING_TICKS,
            GROUND_TIMING_COLUMN,
            f_GroundTimingRow + GROUND_TIMING_ROW
        );
        f_GroundTimingRow = 1 - f_GroundTimingRow;
    }
}
