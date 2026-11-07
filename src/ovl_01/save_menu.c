/*
 * Two of the menus' screens: the clubhouse's Ring Shot records, which list each
 * course's records and saved replays, and the saved games to continue, on
 * numbered boards with each game's mode, course and players.
 */
#include <string.h>
#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "character.h"
#include "character_slot.h"
#include "compiler.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "geometry.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "include_asm.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_arrows.h"
#include "menu_memory.h"
#include "menu_number.h"
#include "play_record.h"
#include "player.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "save_screens.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "sram.h"
#include "text.h"

/*
 * The saved game's 64 by 64 course image, shrunk in two strips;
 * DrawSavedCourseImage draws one with its own palette, DrawSavedMiniGolfImage
 * one without
 */
#define COURSE_THUMBNAIL_WIDTH 64
#define COURSE_THUMBNAIL_STRIP_HEIGHT 32
#define COURSE_THUMBNAIL_STRIP_COUNT 2
#define COURSE_THUMBNAIL_LEFT 40
#define COURSE_THUMBNAIL_TOP 182
#define COURSE_THUMBNAIL_SHOWN_WIDTH 52
#define COURSE_THUMBNAIL_SHOWN_HEIGHT 18
/* The wooden boards numbered 1 to 3, one for each saved game, 64 by 64 each */
#define ASSET_FIRST_GAME_BOARD 0x82A
#define ASSET_SECOND_GAME_BOARD 0x82B
#define ASSET_THIRD_GAME_BOARD 0x82C

/* The games that ContinueModeGame continues from the save's mode tables, by the
 * save menu's slot */
enum SaveMenuGame {
    SAVE_MENU_GAME_TOURNAMENT,
    SAVE_MENU_GAME_GET_CHARACTER,
    SAVE_MENU_GAME_STROKE
};

/* The slots that the save menu steps through, which hold -1 when empty */
#define SAVE_MENU_SLOT_COUNT 5
/* The saved games' course image is double buffered */
#define COURSE_IMAGE_BUFFER_COUNT 2

/* The course image buffers, of which the first is on screen first */
enum {
    COURSE_IMAGE_BUFFER_FIRST,
    COURSE_IMAGE_BUFFER_SECOND
};

/* The saved games, in the order of their boards */
enum {
    SAVED_GAME_FIRST,
    SAVED_GAME_SECOND,
    SAVED_GAME_THIRD
};

/* The images of g_CourseIconAssets that LoadRingShotCourseIcons reads, before
 * one of its own */
#define SAVE_MENU_IMAGE_COUNT 6
/* A 64x64 blue movie camera, read after the course icons */
#define ASSET_MOVIE_CAMERA_ICON 0x944
/* The 64x64 CI8 icons of f_RingShotCourseIcons, which DrawRingShotRowIcon
 * shrinks to 32x24 in two strips */
#define SAVE_ICON_WIDTH 64
#define SAVE_ICON_STRIP_HEIGHT 32
#define SAVE_ICON_STRIP_COUNT 2
#define SAVE_ICON_SHOWN_WIDTH 32
#define SAVE_ICON_SHOWN_STRIP_HEIGHT 12

/* The game modes that have an image of their own, in image order, then mini
 * golf's */
#define SAVE_MENU_MODE_COUNT 10
#define SAVE_MENU_MODE_MINI_GOLF 10
/* An empty saved game's mode */
#define NO_SAVE_MENU_MODE (-1)

extern const char f_SaveMenuNameFormat[];

void ResetSavedGameControllers(void);

/* The Ring Shot stars that each level of GetOpenRingShotCourseCount takes */
#define RING_SHOT_STARS_PER_LEVEL 4

/* What LoadSaveMenuBuffers reads for the chosen game, and the games' frame */
#define SAVE_MENU_CHOSEN_IMAGE_COUNT 2
#define ASSET_SAVE_MENU_FRAME 0x70D
/*
 * f_RingShotCourseNumbers holds the digits 1 to 6, 16 wide each, followed by
 * the word COURSE. DrawCourseLabel draws the word, then the course's digit
 * overlapping its end.
 */
#define COURSE_LABEL_WIDTH 136
#define COURSE_LABEL_HEIGHT 16
#define COURSE_DIGIT_WIDTH 16
#define COURSE_WORD_WIDTH 40
#define COURSE_WORD_OFFSET_X (-5)
#define COURSE_DIGIT_OFFSET_X 33

/* The Ring Shot records' text slots */
#define RING_SHOT_TEXT_SLOT_COUNT 6
/* The row after the courses, for the saved Ring Shot replays */
#define RING_SHOT_REPLAY_ROW RING_SHOT_COURSE_COUNT
/* The courses' rows and the replays' */
#define RING_SHOT_ROW_COUNT (RING_SHOT_REPLAY_ROW + 1)
/* The furthest a course's list of characters scrolls */
#define RING_SHOT_LAST_FIRST_CHARACTER 14

/* What f_SavedPlayerControllers holds for a player with no controller, or for
 * the computer */
#define NO_CONTROLLER (-1)
#define COMPUTER_CONTROLLER MAXCONTROLLERS

s16 FindNextReplaySlot(s8 startSlot, u8 isForward);
s16 GetChosenReplaySlot(void);
s32 GetOpenRingShotCourseCount(void);
/* DrawCharacterFace takes a u8 drawMode, but this file calls it without
 * narrowing the argument */
#ifdef AVOID_UB
void DrawCharacterFace(
    Gfx **displayListHead,
    void *texels,
    void *palette,
    s32 left,
    s32 top,
    s16 character,
    u8 drawMode
);
#else
void DrawCharacterFace(
    Gfx **displayListHead,
    void *texels,
    void *palette,
    s32 left,
    s32 top,
    s16 character,
    s32 drawMode
);
#endif
void DrawCourseLabel(Gfx **displayListHead, s32 courseIndex, s32 left, s32 top, u8 showsCourseWord);
void DrawRingShotRowIcon(Gfx **displayListHead, s32 imageIndex, s32 left, s32 top);

/* The Ring Shot records' blinking arrows' frame and its time shown */
static s32 f_RingShotArrowFrame = 0;
static s32 f_RingShotArrowTimer = 0;
/* Cleared with the Ring Shot arrows' frame, and read by no routine */
static s32 f_UnusedRingShotArrowWord = 0;
/* Read by no routine */
static s32 f_UnusedWordAfterArrowTimer UNUSED = 0;
/* The order that the Ring Shot records list the characters in */
static u8 f_RingShotCharacterOrder[CHARACTER_COUNT] =
    {0, 1, 5, 3, 4, 10, 6, 7, 8, 9, 2, 11, 12, 13, 14, 15, 16, 17};
/* Read by no routine; words holding 0xFF, 0xFF00FF, 0xFF0000 or 0 */
static s32 f_UnusedColorWords[11] UNUSED = {
    0x000000FF,
    0x00FF00FF,
    0x000000FF,
    0x00FF00FF,
    0x00000000,
    0x000000FF,
    0x000000FF,
    0x000000FF,
    0x00000000,
    0x00FF00FF,
    0x00FF0000,
};
/* The angle, in radians, of the sine waves that tint the chosen replay */
static f32 f_ReplayHuePhase = 0.0f;
/* Each open course count's icon positions, of which only being nonzero, for
 * a shown icon, is read */
static s16 f_RingShotIconPositions[RING_SHOT_COURSE_COUNT][RING_SHOT_ROW_COUNT] = {
    {164, 0, 0, 0, 0, 0, 122},
    {102, 144, 0, 0, 0, 0, 186},
    {78, 120, 162, 0, 0, 0, 204},
    {60, 102, 144, 186, 0, 0, 226},
    {42, 84, 126, 168, 210, 0, 252},
    {22, 60, 102, 144, 186, 228, 268},
};
/* Read by no routine */
static s32 f_UnusedWordsAfterIconPositions[2] UNUSED = {0, 0};
/* The controller each of the chosen saved game's players takes */
static s16 f_SavedPlayerControllers[MAX_PLAYER_COUNT] =
    {NO_CONTROLLER, NO_CONTROLLER, NO_CONTROLLER, NO_CONTROLLER};
/* How many of the human players have pressed A for a controller so far */
static s16 f_AssignedHumanCount = 0;
static s16 f_HumanPlayerCount = MAX_PLAYER_COUNT;
/* Set once A chooses a saved game, before its players take controllers */
static u8 f_IsGameChosen = 0;
/* Set once every human player has a controller, so A starts the game */
static u8 f_AreControllersAssigned = 0;
/* Which of f_CourseImageBuffers is on screen */
static u8 f_ShownCourseBufferIndex = 0;
/* Each saved game's save menu mode */
static s32 f_SavedGameModes[SAVED_GAME_COUNT] =
    {NO_SAVE_MENU_MODE, NO_SAVE_MENU_MODE, NO_SAVE_MENU_MODE};
/* The image of each save menu mode, mini golf last, held in words though asset
 * IDs are 16 bits */
static s32 f_ModeImageAssets[SAVE_MENU_MODE_MINI_GOLF + 1] =
    {0x0623, 0x0629, 0x0599, 0x05AF, 0x0631, 0x0665, 0x05B7, 0x05DD, 0x065E, 0x0599, 0x06FC};
/* The game mode of each save menu mode that has an image */
static u32 f_SaveMenuGameModes[SAVE_MENU_MODE_COUNT] = {
    GAME_MODE_TOURNAMENT,
    GAME_MODE_GET_CHARACTER,
    GAME_MODE_STROKE,
    GAME_MODE_SPEED_GOLF,
    GAME_MODE_MATCH_GAME,
    GAME_MODE_CLUB_SLOTS,
    GAME_MODE_RING_SHOT,
    GAME_MODE_TRAINING,
    GAME_MODE_SKINS_MATCH,
    GAME_MODE_UNLISTED_STROKE,
};
/* The mode icon of each save menu mode, mini golf last */
static s32 f_SaveMenuModeIcons[SAVE_MENU_MODE_MINI_GOLF + 1] = {
    MODE_ICON_TOURNAMENT,
    MODE_ICON_GET_CHARACTER,
    MODE_ICON_STROKE,
    MODE_ICON_SPEED_GOLF,
    MODE_ICON_MATCH_GAME,
    MODE_ICON_CLUB_SLOTS,
    MODE_ICON_RING_SHOT,
    MODE_ICON_TRAINING,
    MODE_ICON_SKINS_MATCH,
    MODE_ICON_UNLISTED_STROKE,
    MODE_ICON_MINI_GOLF,
};
/* Read by no routine */
static s32 f_UnusedWordsAfterModeIcons[4] UNUSED = {0, 0, 0, 0};
/* Read by no routine; the values of main_menu.c's f_MenuCharacterChoices */
static s32 f_SavedGamesCharacterChoices[CHARACTER_COUNT] UNUSED = {
    CHARACTER_CHOICE_PLUM,
    CHARACTER_CHOICE_CHARLIE,
    CHARACTER_CHOICE_MAPLE,
    CHARACTER_CHOICE_BABY_MARIO,
    CHARACTER_CHOICE_LUIGI,
    CHARACTER_CHOICE_PEACH,
    CHARACTER_CHOICE_SONNY,
    CHARACTER_CHOICE_WARIO,
    CHARACTER_CHOICE_HARRY,
    CHARACTER_CHOICE_MARIO,
    CHARACTER_CHOICE_YOSHI,
    CHARACTER_CHOICE_DONKEY_KONG,
    CHARACTER_CHOICE_BOWSER,
    CHARACTER_CHOICE_METAL_MARIO,
    CHARACTER_CHOICE_KID,
    CHARACTER_CHOICE_SHERRY,
    CHARACTER_CHOICE_AZALEA,
    CHARACTER_CHOICE_JOE,
};
/* The angle, in radians, of the sine wave that bobs the saved games' boards */
static f32 f_BoardBobPhase = 0.0f;
/* Read by no routine; the values of f_ControllerIconPositions */
static s32 f_UnusedSaveMenuIconPositions[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT][2] UNUSED = {
    {{42, 140}, {76, 120}, {206, 120}, {248, 140}},
    {{42, 140}, {248, 140}, {206, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
};
/* Read by no routine; the values of main_menu.c's f_UnusedModeIconValues */
static s32 f_UnusedSaveMenuModeIconValues[11] UNUSED =
    {110, 109, 117, 97, 106, 105, 105, 97, 84, 124, 99};
/* Read by no routine */
static char f_UnusedHoleLabel[] UNUSED = "Hole";
/* How far left of the label's place each mode icon's game mode label is drawn
 */
static s32 f_GameModeLabelOffsets[MODE_ICON_COUNT] = {85, 85, 41, 60, 55, 60, 75, 66, 59, 0, 61};
/* The saved games' golfers' turn, in degrees about X and Y */
static f32 f_SavedGolferAngles[3] = {0.0f, 0.0f, 0.0f};
/* Read by no routine */
static s32 f_UnusedWordAfterSavedGolferAngles UNUSED = 0;
/* Where DrawSavedGolfers stands each golfer, by the player count */
static Vec3 f_SavedGolferPositions[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT] = {
    {
        {-84.0f, -43.0f, -1000.0f},
        {64.0f, 0.0f, -1000.0f},
        {64.0f, 0.0f, -1000.0f},
        {64.0f, 0.0f, -1000.0f},
    },
    {
        {-106.0f, -32.0f, -1000.0f},
        {106.0f, -32.0f, -1000.0f},
        {-106.0f, -52.0f, -1000.0f},
        {106.0f, -52.0f, -1000.0f},
    },
    {
        {-106.0f, -52.0f, -1000.0f},
        {-76.0f, -8.0f, -1700.0f},
        {76.0f, -8.0f, -1700.0f},
        {106.0f, -52.0f, -1000.0f},
    },
    {
        {-106.0f, -52.0f, -1000.0f},
        {-76.0f, -8.0f, -1700.0f},
        {76.0f, -8.0f, -1700.0f},
        {106.0f, -52.0f, -1000.0f},
    },
};
/* Where a saved game's screen puts each player's controller icon and label, by
 * the player count */
static s32 f_ControllerIconPositions[MAX_PLAYER_COUNT][MAX_PLAYER_COUNT][2] = {
    {{42, 140}, {76, 120}, {206, 120}, {248, 140}},
    {{42, 140}, {248, 140}, {206, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
};
/* Read by no routine */
static s32 f_UnusedWordsAfterControllerIcons[2] UNUSED = {0, 0};

static void *f_RingShotNamePlate;
static void *f_RingShotRainbowBox;
static void *f_RingShotCourseNumbers;
static void *f_RingShotGreenArrows;
static void **f_RingShotCourseIcons;
/* Each Ring Shot record's slot if it holds a recording, or -1 */
static s16 f_RecordedReplaySlots[SAVE_MENU_SLOT_COUNT];

/* Read by no routine */
static u8 f_UnusedBufferBeforeBoardCamera[0x3D0] UNUSED;

/* The camera that DrawSavedGames views the boards with */
static Mat4 f_BoardProjection;
static Mat4 f_BoardViewing;
/* The camera and placement that DrawSavedGolfers draws each golfer with */
static Mtx f_SavedGolferTransforms[MAX_PLAYER_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_SavedGolfersProjection;
/* Each golfer's scale, which DrawSavedGolfers sets by the player count */
static f32 f_SavedGolferScales[MAX_PLAYER_COUNT];
static LookAt f_SavedGolfersLookAts[DRAW_BUFFER_COUNT];
static void **f_SavedGameModeImages;
static void **f_ChosenModeIconBuffers;
static void *f_SavedGamesNamePlate;
static void *f_GameModeLabels;
static void *f_ControllerIcons;
static void *f_PlayerLabels;

/* Read by no routine */
static u8 f_UnusedBytesAfterPlayerLabels[0x4] UNUSED;

/* The chosen game's course image, shown from one while the other loads */
static void *f_CourseImageBuffers[COURSE_IMAGE_BUFFER_COUNT];

/* Read by no routine */
static u8 f_UnusedBytesAfterCourseImages[0x4] UNUSED;

/* Each saved game's numbered board */
static void *f_GameBoards[SAVED_GAME_COUNT];
/* The chosen saved game's human players, in the order they take controllers */
static s16 f_HumanPlayerIndices[MAX_PLAYER_COUNT];
/* The drawn board's mode icon size, which DrawChosenGameBoard reads */
static s32 f_ChosenModeIconScale;
static s32 f_ChosenModeIconHalfSize;

/* Read by no routine; 2 objects, as one would be aligned past its start */
static u8 f_UnusedBytesAfterModeIconSize[0x4] UNUSED;
static u8 f_UnusedBufferAfterModeIconSize[0x88] UNUSED;

/* Not static: the original left these to the linker's common block */
u8 g_MenuCharacterPalettes[CHARACTER_COUNT][MENU_PALETTE_FILE_SIZE];
/* The face palette of each Ring Shot record's replay character */
u8 g_ReplayFacePalettes[SAVE_MENU_SLOT_COUNT][MENU_PALETTE_FILE_SIZE];

void LoadRingShotRecordBuffer(void *buffer, s16 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case RING_SHOT_RECORD_BUFFER_UNREAD:
            f_RingShotNamePlate = buffer;
            break;
        case RING_SHOT_RECORD_BUFFER_RAINBOW_BOX:
            f_RingShotRainbowBox = buffer;
            OpenEncodedAsset(ASSET_RAINBOW_BOX, &asset);
            ReadEncodedAsset(f_RingShotRainbowBox, &asset);
            break;
        case RING_SHOT_RECORD_BUFFER_COURSE_NUMBERS:
            f_RingShotCourseNumbers = buffer;
            OpenEncodedAsset(ASSET_COURSE_NUMBERS, &asset);
            ReadEncodedAsset(f_RingShotCourseNumbers, &asset);
            break;
        case RING_SHOT_RECORD_BUFFER_GREEN_ARROWS:
            f_RingShotGreenArrows = buffer;
            OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
            ReadEncodedAsset(f_RingShotGreenArrows, &asset);
            f_RingShotArrowFrame = 0;
            f_RingShotArrowTimer = 0;
            f_UnusedRingShotArrowWord = 0;
            break;
    }
}

/* Reads every course's icon, then a movie camera icon, into the Ring Shot
 * records' buffers */
void LoadRingShotCourseIcons(void **buffers) {
    EncodedAsset asset;
    s16 imageIndex;

    f_RingShotCourseIcons = buffers;
    for (imageIndex = 0; imageIndex != SAVE_MENU_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
        ReadEncodedAsset(f_RingShotCourseIcons[imageIndex], &asset);
    }

    OpenEncodedAsset(ASSET_MOVIE_CAMERA_ICON, &asset);
    ReadEncodedAsset(f_RingShotCourseIcons[SAVE_MENU_IMAGE_COUNT], &asset);
}

void UpdateRingShotRecords(void) {
    MenuMemory *memory = GetMenuMemory();
    SaveData *save = GetSaveData();
    s32 openCourseCount = GetOpenRingShotCourseCount();
    s32 index;
    s32 movedSlot;

    if (openCourseCount <= 0) {
        openCourseCount = 1;
    }

    if (openCourseCount > RING_SHOT_COURSE_COUNT) {
        openCourseCount = RING_SHOT_COURSE_COUNT;
    }

    if (IsHudAnimationDone() == TRUE) {
        if (GetWipeKind() != WIPE_NONE && !g_IsRingShotWipeOpening) {
            if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
                for (index = 0; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        index,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                g_MenuScreen = MENU_SCREEN_EXIT;
                g_GameState.replayKind = REPLAY_KIND_RING_SHOT;
                SetGameFlag(GAME_FLAG_LOADED_PLAY);
            }
        }
        else if (GetWipeKind() == WIPE_NONE || g_IsRingShotWipeOpening != TRUE) {
            if (g_MenuPressedButtons & A_BUTTON) {
                if (memory->ringShotRow == RING_SHOT_REPLAY_ROW && GetChosenReplaySlot() != -1) {
                    PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                    index = memory->ringShotReplaySlot;
                    memory->holeIndex =
                        save->ringShotRecords[f_RecordedReplaySlots[index]].holeIndex;
                    memory->courseIndex =
                        save->ringShotRecords[f_RecordedReplaySlots[index]].courseIndex;
                    g_IsChosenHoleLoadStarted = FALSE;
                    LoadWipeShape(save->ringShotRecords[index].courseIndex);
                    StartWipe(WIPE_CLOSING);
                    g_IsRingShotWipeOpening = FALSE;
                }
            }
            else if (g_MenuPressedButtons & B_BUTTON) {
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
                g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                for (index = 0; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        index,
                        TEXT_FLAG_TYPED,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }
            }
            else if (g_MenuRepeatButtons & L_JPAD) {
                for (index = 1; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        index,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                if (memory->ringShotRow == RING_SHOT_REPLAY_ROW) {
                    memory->ringShotRow = openCourseCount - 1;
                    return;
                }

                memory->ringShotRow--;
                if (memory->ringShotRow < 0) {
                    memory->ringShotRow = RING_SHOT_REPLAY_ROW;
                }
            }
            else if (g_MenuRepeatButtons & R_JPAD) {
                for (index = 1; index != RING_SHOT_TEXT_SLOT_COUNT; index++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        index,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                memory->ringShotRow++;
                if (memory->ringShotRow > RING_SHOT_REPLAY_ROW) {
                    memory->ringShotRow = 0;
                    return;
                }

                if (memory->ringShotRow > openCourseCount - 1) {
                    memory->ringShotRow = RING_SHOT_REPLAY_ROW;
                }
            }
            else if (g_MenuRepeatButtons & U_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                index = memory->ringShotRow;
                if (index == RING_SHOT_REPLAY_ROW) {
                    if (memory->ringShotReplaySlot > 0) {
                        movedSlot = FindNextReplaySlot(memory->ringShotReplaySlot, FALSE);
                        if (movedSlot != -1) {
                            memory->ringShotReplaySlot = movedSlot;
                        }

                        if (memory->ringShotReplaySlot < 0) {
                            memory->ringShotReplaySlot = SAVE_MENU_SLOT_COUNT - 1;
                        }
                    }
                }
                else if (memory->ringShotFirstCharacters[index] > 0) {
                    memory->ringShotFirstCharacters[index]--;
                }
            }
            else if (g_MenuRepeatButtons & D_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                index = memory->ringShotRow;
                if (index == RING_SHOT_REPLAY_ROW) {
                    if (memory->ringShotReplaySlot < SAVE_MENU_SLOT_COUNT - 1) {
                        movedSlot = FindNextReplaySlot(memory->ringShotReplaySlot, TRUE);
                        if (movedSlot != -1) {
                            memory->ringShotReplaySlot = movedSlot;
                        }

                        if (memory->ringShotReplaySlot >= SAVE_MENU_SLOT_COUNT) {
                            memory->ringShotReplaySlot = 0;
                        }
                    }
                }
                else if (memory->ringShotFirstCharacters[index] < RING_SHOT_LAST_FIRST_CHARACTER) {
                    memory->ringShotFirstCharacters[index]++;
                }
            }
        }
    }
}

/*
 * DrawRingShotRecords draws the Ring Shot records. A row of icons, one for each
 * open course and one for the saved replays, sits above a shaded panel, with
 * the blinking arrows around the chosen one. For a course the panel shows each
 * hole's number over a column of four characters from ringShotFirstCharacters
 * on, crossed out and darkened where they haven't cleared the hole, with arrows
 * where the list scrolls further. For the replays it shows each slot's
 * character face, course and a message for its course and hole, or dashes where
 * it is empty, and the chosen slot's message glows in shifting colors. Message
 * 0x1E plus the row is shown below the panel.
 */
#define RING_SHOT_PANEL_LEFT 24
#define RING_SHOT_PANEL_TOP 70
#define RING_SHOT_PANEL_RIGHT 302
#define RING_SHOT_PANEL_BOTTOM 202
#define RING_SHOT_STRIP_WIDTH 200
#define RING_SHOT_STRIP_HEIGHT 10
#define RING_SHOT_STRIP_COUNT 3
#define RING_SHOT_STRIP_LEFT 28
#define RING_SHOT_STRIP_RIGHT 298
#define RING_SHOT_STRIP_TOP 204
#define RING_SHOT_STRIP_SPACING 8
/* Each row's icon is drawn this many times over in the same place */
#define RING_SHOT_ICON_DRAW_COUNT 3
#define RING_SHOT_FIRST_ICON_X 22
#define RING_SHOT_REPLAY_ICON_X 268
#define RING_SHOT_ICON_SPACING 42
#define RING_SHOT_ICON_OFFSET_X 18
#define RING_SHOT_ICON_TOP 48
#define RING_SHOT_CHOSEN_ICON_DROP 8
#define RING_SHOT_ARROW_WIDTH 20
#define RING_SHOT_FIRST_ARROW_X 40
#define RING_SHOT_REPLAY_ARROW_X 284
#define RING_SHOT_ARROW_OFFSET_X 36
#define RING_SHOT_ARROW_Y 60
#define RING_SHOT_COLUMN_SPACING 42
#define RING_SHOT_HOLE_NUMBER_X 48
#define RING_SHOT_HOLE_NUMBER_Y 76
#define RING_SHOT_SHOWN_CHARACTER_COUNT 4
#define RING_SHOT_FACE_X 46
#define RING_SHOT_FACE_Y 94
#define RING_SHOT_FACE_SPACING 26
#define RING_SHOT_CROSS_X 52
#define RING_SHOT_CROSS_Y 100
/* The scroll arrows are drawn in this many columns */
#define RING_SHOT_SCROLL_COLUMN_COUNT 1
#define RING_SHOT_SCROLL_ARROW_X 36
#define RING_SHOT_UP_ARROW_Y 100
#define RING_SHOT_DOWN_ARROW_Y 190
#define RING_SHOT_SLOT_SPACING 25
#define RING_SHOT_REPLAY_FACE_X 44
#define RING_SHOT_REPLAY_FACE_Y 74
#define RING_SHOT_REPLAY_COURSE_X 76
#define RING_SHOT_REPLAY_COURSE_Y 82
#define RING_SHOT_EMPTY_DASH_COUNT 13
#define RING_SHOT_EMPTY_DASH_X 52
#define RING_SHOT_EMPTY_DASH_SPACING 18
#define RING_SHOT_EMPTY_DASH_Y 80
#define RING_SHOT_SLOT_NUMBER_X 26
#define RING_SHOT_SLOT_NUMBER_Y 82
#define RING_SHOT_TITLE_X 36
#define RING_SHOT_TITLE_Y 211
#define MESSAGE_FIRST_RING_SHOT_TITLE 0x1E
#define RING_SHOT_REPLAY_TEXT_X 124
#define RING_SHOT_REPLAY_TEXT_Y 84
#define MESSAGE_FIRST_RING_SHOT_HOLE 0x4B
/* Another size that draws in the large font, as TEXT_SLOT_LARGE_FONT does */
#define RING_SHOT_REPLAY_TEXT_FONT 2
/* The chosen replay's colors run around a circle of hues, a step per update,
 * with its glow a quarter as bright */
#define REPLAY_HUE_STEP DegreesToRadians(5)
#define REPLAY_HUE_MIDDLE 127
#define REPLAY_HUE_SWING 127.0f
#define REPLAY_GLOW_DIVISOR 4

/*
 * Doesn't match: the original gives several values other saved registers,
 * and keeps memory on the stack. The register oracle comes within 8 words
 * with 2 pseudos forced: the face loop's itemIndex and listIndex, each in
 * the other's register. Other orders of their declarations, setting
 * listIndex before row or in the loop's header, change nothing; counting the
 * empty slot's dashes with listIndex, column, drop, row or a counter of
 * their own gives 26 to 62 words, all in registers. 16 words differ, none in
 * shape.
 *
 * The row icon's draws are counted with index, which also counts the strips
 * and the replay slots and holds each face's draw mode; counting them with
 * column, the hole columns' counter, gives 26 words differing, and with a
 * counter of their own 62. Giving index's other uses variables of their own
 * costs words too: a strip counter gives 92, a face mode 248 (38 in shape)
 * and a slot index 66.
 *
 * The register oracle's split search finds itemIndex in different registers
 * in the face loop and the empty dashes' loop, but a counter of its own for
 * either gives 36 words differing.
 *
 * Stepping listIndex in the face's lookup, as
 * f_RingShotCharacterOrder[listIndex++], gives 20 words, 4 in shape;
 * indexing the order from listIndex plus itemIndex, with listIndex fixed,
 * gives 115 (15), and from the row's first character plus itemIndex, with
 * no listIndex, 53 (27).
 */
#ifdef NON_MATCHING
void DrawRingShotRecords(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    /* Counts the strips and a row icon's draws, holds each face's draw mode and
     * counts the replay slots, as the original's shared register needs */
    s16 index = 0;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s32 openCourseCount = GetOpenRingShotCourseCount();
    s16 row;
    s16 column;
    /* Counts the faces shown in a column, or an empty slot's dashes */
    s16 itemIndex;
    s16 listIndex;
    s16 drop;
    s32 character;
    s8 replayCharacter;
    s32 red;
    s32 green;
    s32 blue;
    s16 replayCourse;
    s16 replayHole;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);

    gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(RING_SHOT_PANEL_LEFT),
        QuarterPixels(RING_SHOT_PANEL_TOP),
        QuarterPixels(RING_SHOT_PANEL_RIGHT),
        QuarterPixels(RING_SHOT_PANEL_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);

    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_RingShotNamePlate)));
    do {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_RingShotNamePlate) +
                (index * RING_SHOT_STRIP_WIDTH * RING_SHOT_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            RING_SHOT_STRIP_WIDTH,
            RING_SHOT_STRIP_HEIGHT,
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
            QuarterPixels(RING_SHOT_STRIP_LEFT),
            QuarterPixels(RING_SHOT_STRIP_TOP + (index * RING_SHOT_STRIP_SPACING)),
            QuarterPixels(RING_SHOT_STRIP_RIGHT),
            QuarterPixels(
                RING_SHOT_STRIP_TOP + RING_SHOT_STRIP_SPACING + (index * RING_SHOT_STRIP_SPACING)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(RING_SHOT_STRIP_WIDTH, RING_SHOT_STRIP_RIGHT - RING_SHOT_STRIP_LEFT),
            TexelRunStep(RING_SHOT_STRIP_HEIGHT, RING_SHOT_STRIP_SPACING)
        );
        gDPPipeSync(gfx++);
        index++;
    }
    while (index != RING_SHOT_STRIP_COUNT);

    if (openCourseCount <= 0) {
        openCourseCount = 1;
    }

    if (openCourseCount > RING_SHOT_COURSE_COUNT) {
        openCourseCount = RING_SHOT_COURSE_COUNT;
    }

    for (row = 0; row != RING_SHOT_ROW_COUNT; row++) {
        drop = (row == memory->ringShotRow) * RING_SHOT_CHOSEN_ICON_DROP;
        if ((row < openCourseCount) || (row == RING_SHOT_REPLAY_ROW)) {
            if (f_RingShotIconPositions[openCourseCount - 1][row] != 0) {
                /*
                 * The icon loop in a block of its own, as a macro's do-while would
                 * make it, to match: without it, or with plain braces, three saved
                 * values take each other's registers; 32 words differ, against 16
                 */
                IN_OWN_BLOCK(for (index = 0; index != RING_SHOT_ICON_DRAW_COUNT; index++) {
                    if (row == 0) {
                        DrawRingShotRowIcon(
                            &gfx,
                            0,
                            RING_SHOT_FIRST_ICON_X,
                            RING_SHOT_ICON_TOP + drop
                        );
                    }
                    else if (row == RING_SHOT_REPLAY_ROW) {
                        DrawRingShotRowIcon(
                            &gfx,
                            RING_SHOT_REPLAY_ROW,
                            RING_SHOT_REPLAY_ICON_X,
                            RING_SHOT_ICON_TOP + drop
                        );
                    }
                    else {
                        DrawRingShotRowIcon(
                            &gfx,
                            row,
                            (row * RING_SHOT_ICON_SPACING) + RING_SHOT_ICON_OFFSET_X,
                            RING_SHOT_ICON_TOP + drop
                        );
                    }

                    gDPPipeSync(gfx++);
                });
            }
        }

        if (row == memory->ringShotRow) {
            if (row == 0) {
                DrawBlinkingSideArrows(
                    &gfx,
                    f_RingShotGreenArrows,
                    &f_RingShotArrowFrame,
                    &f_RingShotArrowTimer,
                    RING_SHOT_ARROW_WIDTH,
                    RING_SHOT_FIRST_ARROW_X,
                    RING_SHOT_ARROW_Y
                );
            }
            else if (row == RING_SHOT_REPLAY_ROW) {
                DrawBlinkingSideArrows(
                    &gfx,
                    f_RingShotGreenArrows,
                    &f_RingShotArrowFrame,
                    &f_RingShotArrowTimer,
                    RING_SHOT_ARROW_WIDTH,
                    RING_SHOT_REPLAY_ARROW_X,
                    RING_SHOT_ARROW_Y
                );
            }
            else {
                DrawBlinkingSideArrows(
                    &gfx,
                    f_RingShotGreenArrows,
                    &f_RingShotArrowFrame,
                    &f_RingShotArrowTimer,
                    RING_SHOT_ARROW_WIDTH,
                    (row * RING_SHOT_ICON_SPACING) + RING_SHOT_ARROW_OFFSET_X,
                    RING_SHOT_ARROW_Y
                );
            }
        }
    }

    if (memory->ringShotRow != RING_SHOT_REPLAY_ROW) {
        for (column = 0; column != RING_SHOT_HOLE_COUNT; column++) {
            DrawOneDigitMenuNumber(
                &gfx,
                RING_SHOT_HOLE_NUMBER_X + (column * RING_SHOT_COLUMN_SPACING),
                RING_SHOT_HOLE_NUMBER_Y,
                column + 1,
                MENU_NUMBER_PLAIN
            );

            row = memory->ringShotRow;
            listIndex = memory->ringShotFirstCharacters[row];
            for (itemIndex = 0; itemIndex != RING_SHOT_SHOWN_CHARACTER_COUNT; itemIndex++) {
                character = f_RingShotCharacterOrder[listIndex];
                index = FACE_PLAIN;
                if (save->ringShotClearCounts[row][column][character] == 0) {
                    index = FACE_DARKENED;
                }

                DrawCharacterFace(
                    &gfx,
                    g_CharacterFaceImages[character],
                    g_MenuCharacterPalettes[character],
                    RING_SHOT_FACE_X + (column * RING_SHOT_COLUMN_SPACING),
                    RING_SHOT_FACE_Y + (itemIndex * RING_SHOT_FACE_SPACING),
                    character,
                    index
                );
                if (index == FACE_DARKENED) {
                    DrawMenuSign(
                        &gfx,
                        RING_SHOT_CROSS_X + (column * RING_SHOT_COLUMN_SPACING),
                        RING_SHOT_CROSS_Y + (itemIndex * RING_SHOT_FACE_SPACING),
                        MENU_SIGN_TIMES
                    );
                }

                listIndex++;
            }
        }

        for (column = 0; column != RING_SHOT_SCROLL_COLUMN_COUNT; column++) {
            if (memory->ringShotFirstCharacters[memory->ringShotRow] > 0) {
                DrawGreenArrow(
                    &gfx,
                    f_RingShotGreenArrows,
                    f_RingShotArrowFrame,
                    MENU_ARROW_UP,
                    RING_SHOT_SCROLL_ARROW_X + (column * RING_SHOT_COLUMN_SPACING),
                    RING_SHOT_UP_ARROW_Y
                );
            }

            if (memory->ringShotFirstCharacters[memory->ringShotRow] <
                RING_SHOT_LAST_FIRST_CHARACTER) {
                DrawGreenArrow(
                    &gfx,
                    f_RingShotGreenArrows,
                    f_RingShotArrowFrame,
                    MENU_ARROW_DOWN,
                    RING_SHOT_SCROLL_ARROW_X + (column * RING_SHOT_COLUMN_SPACING),
                    RING_SHOT_DOWN_ARROW_Y
                );
            }
        }
    }
    else {
        for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
            if (f_RecordedReplaySlots[index] != -1) {
                replayCharacter =
                    save->ringShotRecords[f_RecordedReplaySlots[index]].characterIndex;
                DrawCharacterFace(
                    &gfx,
                    g_CharacterFaceImages[replayCharacter],
                    g_ReplayFacePalettes[index],
                    RING_SHOT_REPLAY_FACE_X,
                    RING_SHOT_REPLAY_FACE_Y + (index * RING_SHOT_SLOT_SPACING),
                    replayCharacter,
                    FACE_PLAIN
                );
                DrawCourseLabel(
                    &gfx,
                    save->ringShotRecords[f_RecordedReplaySlots[index]].courseIndex,
                    RING_SHOT_REPLAY_COURSE_X,
                    RING_SHOT_REPLAY_COURSE_Y + (index * RING_SHOT_SLOT_SPACING),
                    TRUE
                );
            }
            else {
                for (itemIndex = 0; itemIndex != RING_SHOT_EMPTY_DASH_COUNT; itemIndex++) {
                    DrawMenuSign(
                        &gfx,
                        RING_SHOT_EMPTY_DASH_X + (itemIndex * RING_SHOT_EMPTY_DASH_SPACING),
                        RING_SHOT_EMPTY_DASH_Y + (index * RING_SHOT_SLOT_SPACING),
                        MENU_SIGN_DASH
                    );
                }
            }

            DrawOneDigitMenuNumber(
                &gfx,
                RING_SHOT_SLOT_NUMBER_X,
                RING_SHOT_SLOT_NUMBER_Y + (index * RING_SHOT_SLOT_SPACING),
                index + 1,
                MENU_NUMBER_PLAIN
            );
        }
    }

    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextSlot(
        RING_SHOT_TITLE_X,
        RING_SHOT_TITLE_Y,
        MESSAGE_FIRST_RING_SHOT_TITLE + memory->ringShotRow,
        TEXT_SLOT_LARGE_FONT,
        0,
        TEXT_FLAG_TYPED,
        255,
        255,
        255,
        OPAQUE_ALPHA
    );
    if (memory->ringShotRow == RING_SHOT_REPLAY_ROW) {
        for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
            if (f_RecordedReplaySlots[index] != -1) {
                s32 redSwing;
                s32 greenSwing;
                s32 blueSwing;

                f_ReplayHuePhase += REPLAY_HUE_STEP;

                redSwing = sinf(f_ReplayHuePhase) * REPLAY_HUE_SWING;
                blueSwing = sinf(f_ReplayHuePhase + (M_PI / 2)) * REPLAY_HUE_SWING;
                red = redSwing + REPLAY_HUE_MIDDLE;
                blue = blueSwing + REPLAY_HUE_MIDDLE;
                greenSwing = sinf(f_ReplayHuePhase + M_PI) * REPLAY_HUE_SWING;
                green = greenSwing + REPLAY_HUE_MIDDLE;
                if (index == memory->ringShotReplaySlot) {
                    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, OPAQUE_ALPHA);
                    gDPSetEnvColor(
                        gfx++,
                        red / REPLAY_GLOW_DIVISOR,
                        green / REPLAY_GLOW_DIVISOR,
                        blue / REPLAY_GLOW_DIVISOR,
                        OPAQUE_ALPHA
                    );
                }
                else {
                    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
                    gDPSetEnvColor(gfx++, 0, 0, 0, OPAQUE_ALPHA);
                }

                replayCourse = save->ringShotRecords[f_RecordedReplaySlots[index]].courseIndex;
                replayHole = save->ringShotRecords[f_RecordedReplaySlots[index]].holeIndex;
                if (index == memory->ringShotReplaySlot) {
                    SetTextSlot(
                        RING_SHOT_REPLAY_TEXT_X,
                        RING_SHOT_REPLAY_TEXT_Y + (index * RING_SHOT_SLOT_SPACING),
                        ((replayCourse * RING_SHOT_HOLE_COUNT) + MESSAGE_FIRST_RING_SHOT_HOLE) +
                            replayHole,
                        RING_SHOT_REPLAY_TEXT_FONT,
                        index + 1,
                        0,
                        red,
                        green,
                        blue,
                        OPAQUE_ALPHA
                    );
                }
                else {
                    SetTextSlot(
                        RING_SHOT_REPLAY_TEXT_X,
                        RING_SHOT_REPLAY_TEXT_Y + (index * RING_SHOT_SLOT_SPACING),
                        ((replayCourse * RING_SHOT_HOLE_COUNT) + MESSAGE_FIRST_RING_SHOT_HOLE) +
                            replayHole,
                        RING_SHOT_REPLAY_TEXT_FONT,
                        index + 1,
                        0,
                        255,
                        255,
                        255,
                        OPAQUE_ALPHA
                    );
                }
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", DrawRingShotRecords);
#endif

s32 IsRingShotRecorded(RingShotRecord *record);

/*
 * Reads the save data from SRAM and lists the Ring Shot records that hold a
 * recording, moving the chosen replay onto the last of them if its own is
 * empty.
 */
void ReadRingShotReplays(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s8 slot;

    InitSram();
    TransferSaveData(save, FALSE);
    for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
        f_RecordedReplaySlots[slot] = -1;
    }

    for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
        if ((s16)IsRingShotRecorded(&save->ringShotRecords[slot])) {
            f_RecordedReplaySlots[slot] = slot;
        }
        else {
            f_RecordedReplaySlots[slot] = -1;
        }
    }

    if (f_RecordedReplaySlots[memory->ringShotReplaySlot] == -1) {
        for (slot = 0; slot != SAVE_MENU_SLOT_COUNT; slot++) {
            if (f_RecordedReplaySlots[slot] != -1) {
                memory->ringShotReplaySlot = slot;
            }
        }
    }
}

/*
 * Reads every character's face and first palette, then the palette of each
 * recorded Ring Shot record's character, clamped to the known ones.
 */
void LoadRingShotRecordFaces(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 index;
    CharacterId character;
    s32 palette;

    for (index = 0; index != CHARACTER_COUNT; index++) {
        OpenEncodedAsset(g_CharacterFaceAssets[index], &asset);
        ReadEncodedAsset(g_CharacterFaceImages[index], &asset);
        OpenAssetFile(g_CharacterPaletteFiles[index][0], &file);
        ReadAssetFile(g_MenuCharacterPalettes[index], GetAssetFileSize(&file), &file);
    }

    for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
        if (f_RecordedReplaySlots[index] != -1) {
            character = save->ringShotRecords[f_RecordedReplaySlots[index]].characterIndex;
            palette = save->ringShotRecords[f_RecordedReplaySlots[index]].paletteIndex;
            if (character < 0) {
                character = 0;
            }

            if (character >= CHARACTER_COUNT) {
                character = CHARACTER_COUNT - 1;
            }

            if (palette < 0) {
                palette = 0;
            }

            if (palette >= CHARACTER_PALETTE_COUNT) {
                palette = CHARACTER_PALETTE_COUNT - 1;
            }
        }
        else {
            palette = 0;
            character = 0;
        }

        OpenAssetFile(g_CharacterPaletteFiles[character][palette], &file);
        ReadAssetFile(g_ReplayFacePalettes[index], GetAssetFileSize(&file), &file);
    }
}

/*
 * Returns the next slot after startSlot, going forward or back and wrapping,
 * that holds a replay, or -1 if none does
 */
s16 FindNextReplaySlot(s8 startSlot, u8 isForward) {
    s32 slot = startSlot;
    s32 step;
    s32 tryCount;

    if (slot < 0) {
        return -1;
    }

    if (slot >= SAVE_MENU_SLOT_COUNT) {
        return -1;
    }

    step = isForward ? 1 : -1;
    slot += step;
    for (tryCount = 0; tryCount != SAVE_MENU_SLOT_COUNT - 1; tryCount++) {
        if (slot >= SAVE_MENU_SLOT_COUNT) {
            slot = 0;
        }

        if (slot < 0) {
            slot = SAVE_MENU_SLOT_COUNT - 1;
        }

        if (f_RecordedReplaySlots[slot] != -1) {
            break;
        }

        slot += step;
    }

    if (tryCount == SAVE_MENU_SLOT_COUNT - 1) {
        return -1;
    }

    return slot;
}

/* Returns the chosen replay's record, or -1 if it holds no recording */
s16 GetChosenReplaySlot(void) {
    return f_RecordedReplaySlots[GetMenuMemory()->ringShotReplaySlot];
}

/* Tells whether the record holds a recording of the shots */
s32 IsRingShotRecorded(RingShotRecord *record) {
    return (record->scoreKind & SCORE_KIND_RECORDED) != 0;
}

void DrawCourseLabel(
    Gfx **displayListHead,
    s32 courseIndex,
    s32 left,
    s32 top,
    u8 showsCourseWord
) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_RingShotCourseNumbers)));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(f_RingShotCourseNumbers)),
        G_IM_FMT_CI,
        COURSE_LABEL_WIDTH,
        COURSE_LABEL_HEIGHT,
        0,
        0,
        COURSE_LABEL_WIDTH - 1,
        COURSE_LABEL_HEIGHT - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    if (showsCourseWord == TRUE) {
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(left + COURSE_WORD_OFFSET_X),
            QuarterPixels(top),
            QuarterPixels(left + COURSE_WORD_OFFSET_X + COURSE_WORD_WIDTH),
            QuarterPixels(top + COURSE_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            TexelCoordinate(COURSE_DIGIT_WIDTH * COURSE_COUNT),
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(left + COURSE_DIGIT_OFFSET_X),
        QuarterPixels(top),
        QuarterPixels(left + COURSE_DIGIT_OFFSET_X + COURSE_DIGIT_WIDTH),
        QuarterPixels(top + COURSE_LABEL_HEIGHT),
        G_TX_RENDERTILE,
        TexelCoordinate(courseIndex * COURSE_DIGIT_WIDTH),
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/*
 * Draws a Ring Shot records row's icon, a course's or the replays' camera, from
 * f_RingShotCourseIcons, shrunk to 32 by 24
 */
void DrawRingShotRowIcon(Gfx **displayListHead, s32 imageIndex, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_RingShotCourseIcons[imageIndex])));
    for (stripIndex = 0; stripIndex != SAVE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_RingShotCourseIcons[imageIndex]) +
                (stripIndex * SAVE_ICON_WIDTH * SAVE_ICON_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            SAVE_ICON_WIDTH,
            SAVE_ICON_STRIP_HEIGHT,
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
            QuarterPixels(left),
            QuarterPixels(top + (stripIndex * SAVE_ICON_SHOWN_STRIP_HEIGHT)),
            QuarterPixels(left + SAVE_ICON_SHOWN_WIDTH),
            QuarterPixels(
                top + (stripIndex * SAVE_ICON_SHOWN_STRIP_HEIGHT) + SAVE_ICON_SHOWN_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(SAVE_ICON_WIDTH, SAVE_ICON_SHOWN_WIDTH),
            TexelRunStep(SAVE_ICON_STRIP_HEIGHT, SAVE_ICON_SHOWN_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

/* Returns how many Ring Shot courses are open: one more for each level of the
 * stars won */
s32 GetOpenRingShotCourseCount(void) {
    SaveData *save = GetSaveData();
    s32 starCount = 0;
    CourseIndex courseIndex;
    s32 holeIndex;

    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->records.hasRingShotStar[courseIndex][holeIndex] > 0;
        }
    }

    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 5) {
        return 6;
    }

    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 4) {
        return 5;
    }

    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 3) {
        return 4;
    }

    if (starCount >= RING_SHOT_STARS_PER_LEVEL * 2) {
        return 3;
    }

    if (starCount >= RING_SHOT_STARS_PER_LEVEL) {
        return 2;
    }

    return 1;
}

void LoadSaveGameBuffer(void *buffer, u8 bufferKind) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s32 gameIndex;
    s32 miniGolfIndex;

    switch (bufferKind) {
        case SAVE_GAME_BUFFER_CONTROLLER_ICONS:
            f_ControllerIcons = buffer;
            OpenEncodedAsset(ASSET_CONTROLLER_ICONS, &asset);
            ReadEncodedAsset(f_ControllerIcons, &asset);
            break;
        case SAVE_GAME_BUFFER_PLAYER_LABELS:
            f_PlayerLabels = buffer;
            OpenEncodedAsset(ASSET_PLAYER_LABELS, &asset);
            ReadEncodedAsset(f_PlayerLabels, &asset);
            break;
        case SAVE_GAME_BUFFER_FIRST_BOARD:
            f_GameBoards[SAVED_GAME_FIRST] = buffer;
            OpenEncodedAsset(ASSET_FIRST_GAME_BOARD, &asset);
            ReadEncodedAsset(f_GameBoards[SAVED_GAME_FIRST], &asset);
            break;
        case SAVE_GAME_BUFFER_SECOND_BOARD:
            f_GameBoards[SAVED_GAME_SECOND] = buffer;
            OpenEncodedAsset(ASSET_SECOND_GAME_BOARD, &asset);
            ReadEncodedAsset(f_GameBoards[SAVED_GAME_SECOND], &asset);
            break;
        case SAVE_GAME_BUFFER_THIRD_BOARD:
            f_GameBoards[SAVED_GAME_THIRD] = buffer;
            OpenEncodedAsset(ASSET_THIRD_GAME_BOARD, &asset);
            ReadEncodedAsset(f_GameBoards[SAVED_GAME_THIRD], &asset);
            break;
        case SAVE_GAME_BUFFER_COURSE:
            f_CourseImageBuffers[COURSE_IMAGE_BUFFER_FIRST] = buffer;
            gameIndex = memory->savedGameIndex;
            if (gameIndex < 0) {
                gameIndex = 0;
            }

            if (gameIndex >= SAVED_GAME_COUNT) {
                gameIndex = SAVED_GAME_COUNT - 1;
            }

            if (!save->savedGames[gameIndex].isValid) {
                break;
            }

            if (!save->savedGames[gameIndex].state.isMiniGolf) {
                OpenEncodedAsset(
                    g_CourseIconAssets[save->savedGames[gameIndex].state.courseIndex],
                    &asset
                );
                ReadEncodedAsset(f_CourseImageBuffers[COURSE_IMAGE_BUFFER_FIRST], &asset);
            }
            else {
                /*
                 * Subtracting in a second statement is needed to match: one
                 * statement, with or without a course index local, gives 16 words
                 * differing, and indexing with the difference itself 13
                 */
                miniGolfIndex = save->savedGames[gameIndex].state.courseIndex;
                miniGolfIndex -= MINI_GOLF_COURSE_INDEX;
                OpenEncodedAsset(g_MiniGolfCourseIconAssets[miniGolfIndex], &asset);
                ReadEncodedAsset(f_CourseImageBuffers[COURSE_IMAGE_BUFFER_FIRST], &asset);
                OpenAssetFile(g_MiniGolfDiscFiles[miniGolfIndex], &file);
                ReadAssetFile(g_MiniGolfDiscImages[0], GetAssetFileSize(&file), &file);
            }

            break;
        case SAVE_GAME_BUFFER_CHOSEN_COURSE:
            f_CourseImageBuffers[COURSE_IMAGE_BUFFER_SECOND] = buffer;
            gameIndex = memory->savedGameIndex;
            if (gameIndex < 0) {
                gameIndex = 0;
            }

            if (gameIndex >= SAVED_GAME_COUNT) {
                gameIndex = SAVED_GAME_COUNT - 1;
            }

            if (!save->savedGames[gameIndex].isValid) {
                break;
            }

            if (!save->savedGames[gameIndex].state.isMiniGolf) {
                OpenEncodedAsset(
                    g_CourseIconAssets[save->savedGames[gameIndex].state.courseIndex],
                    &asset
                );
                ReadEncodedAsset(f_CourseImageBuffers[COURSE_IMAGE_BUFFER_SECOND], &asset);
            }
            else {
                /*
                 * Subtracting in a second statement is needed to match: one
                 * statement, with or without a course index local, gives 16 words
                 * differing, and indexing with the difference itself 13
                 */
                miniGolfIndex = save->savedGames[gameIndex].state.courseIndex;
                miniGolfIndex -= MINI_GOLF_COURSE_INDEX;
                OpenEncodedAsset(g_MiniGolfCourseIconAssets[miniGolfIndex], &asset);
                ReadEncodedAsset(f_CourseImageBuffers[COURSE_IMAGE_BUFFER_SECOND], &asset);
                OpenAssetFile(g_MiniGolfDiscFiles[miniGolfIndex], &file);
                ReadAssetFile(g_MiniGolfDiscImages[0], GetAssetFileSize(&file), &file);
            }

            break;
    }

    f_ShownCourseBufferIndex = COURSE_IMAGE_BUFFER_FIRST;
}

/*
 * Reads the save data from SRAM, finds each saved game's mode image, chooses
 * the first saved game and clears its players' controllers. The records screens
 * run it too, for the save data.
 */
void ReadSavedGames(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s8 gameIndex;
    s8 modeIndex;

    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        f_SavedGameModes[gameIndex] = NO_SAVE_MENU_MODE;
    }

    f_AreControllersAssigned = 0;
    InitSram();
    TransferSaveData(save, FALSE);
    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        if (save->savedGames[gameIndex].isValid) {
            if (save->savedGames[gameIndex].state.isMiniGolf) {
                f_SavedGameModes[gameIndex] = SAVE_MENU_MODE_MINI_GOLF;
            }
            else {
                for (modeIndex = 0; modeIndex != SAVE_MENU_MODE_COUNT; modeIndex++) {
                    if (save->savedGames[gameIndex].state.gameMode ==
                        f_SaveMenuGameModes[modeIndex]) {
                        f_SavedGameModes[gameIndex] = modeIndex;
                        break;
                    }
                }
            }
        }
    }

    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        if (f_SavedGameModes[gameIndex] != NO_SAVE_MENU_MODE) {
            break;
        }
    }

    if (gameIndex == SAVED_GAME_COUNT) {
        memory->savedGameIndex = 0;
    }
    else {
        memory->savedGameIndex = gameIndex;
    }

    ResetSavedGameControllers();
}

/*
 * Marks the chosen saved game's computer players as such and its other players
 * as having no controller yet, and lists those other players in order.
 */
void ResetSavedGameControllers(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s8 playerIndex;
    s8 playerCount;
    s8 humanCount;
    s8 gameIndex;
    Player *savedPlayers;

    for (playerIndex = 0; playerIndex != MAX_PLAYER_COUNT; playerIndex++) {
        f_SavedPlayerControllers[playerIndex] = NO_CONTROLLER;
        f_HumanPlayerIndices[playerIndex] = -1;
    }

    f_AssignedHumanCount = 0;
    gameIndex = memory->savedGameIndex;
    savedPlayers = save->savedPlayers[gameIndex];
    /*
     * Through f_HumanPlayerCount, to match: reading the count into playerCount
     * first gives 92 words differing, 10 in shape
     */
    f_HumanPlayerCount = save->savedGames[gameIndex].state.playerCount;
    playerCount = f_HumanPlayerCount;
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        if (savedPlayers[playerIndex].isComputer) {
            f_SavedPlayerControllers[playerIndex] = COMPUTER_CONTROLLER;
            f_HumanPlayerCount--;
        }
        else {
            f_SavedPlayerControllers[playerIndex] = NO_CONTROLLER;
        }
    }

    for (playerIndex = 0, humanCount = 0; playerIndex != playerCount; playerIndex++) {
        if (f_SavedPlayerControllers[playerIndex] != COMPUTER_CONTROLLER) {
            f_HumanPlayerIndices[humanCount] = playerIndex;
            humanCount++;
        }
    }

    f_AssignedHumanCount = 0;
    f_IsGameChosen = 0;
    for (playerIndex = 0; playerIndex != MAXCONTROLLERS; playerIndex++) {
        g_IsMenuCharacterLoaded[playerIndex] = 0;
    }
}

/* Keeps the save menu's buffers of bufferKind and reads the saved games'
 * mode images, and the frame, or the chosen game's icons into them */
void LoadSaveMenuBuffers(void **buffers, u8 bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = GetMenuMemory();
    s32 imageIndex;
    s32 mode;

    switch (bufferKind) {
        case SAVE_MENU_BUFFER_SLOTS:
            f_SavedGameModeImages = buffers;
            for (imageIndex = 0; imageIndex != SAVED_GAME_COUNT; imageIndex++) {
                mode = f_SavedGameModes[imageIndex];
                if (mode == NO_SAVE_MENU_MODE) {
                    mode = 0;
                }

                OpenEncodedAsset(f_ModeImageAssets[mode], &asset);
                ReadEncodedAsset(f_SavedGameModeImages[imageIndex], &asset);
            }

            OpenEncodedAsset(ASSET_SAVE_MENU_FRAME, &asset);
            ReadEncodedAsset(f_SavedGameModeImages[SAVED_GAME_COUNT], &asset);
            break;
        case SAVE_MENU_BUFFER_CHOSEN:
            f_ChosenModeIconBuffers = buffers;
            for (imageIndex = 0; imageIndex != SAVE_MENU_CHOSEN_IMAGE_COUNT; imageIndex++) {
                mode = f_SavedGameModes[memory->savedGameIndex];
                if (mode == NO_SAVE_MENU_MODE) {
                    mode = 0;
                }

                OpenEncodedAsset(f_ModeImageAssets[mode], &asset);
                ReadEncodedAsset(f_ChosenModeIconBuffers[imageIndex], &asset);
            }

            break;
    }
}

/* Keeps the name plate's buffer, or reads the game mode labels into theirs */
void LoadGameModeLabelBuffer(void *buffer, u8 bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case GAME_MODE_LABEL_BUFFER_NAME_PLATE:
            f_SavedGamesNamePlate = buffer;
            break;
        case GAME_MODE_LABEL_BUFFER_LABELS:
            f_GameModeLabels = buffer;
            OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
            ReadEncodedAsset(f_GameModeLabels, &asset);
            break;
    }
}

s32 ContinueSavedGame(void);

/* The saved-games screen's text slots */
#define SAVED_GAME_TEXT_SLOT_COUNT 4
/* How many updates the boards take to turn to the next saved game */
#define SAVED_GAME_TURN_FRAMES 7

/* The saved game the boards show */
#define CHOSEN_GAME (save->savedGames[memory->savedGameIndex])
/*
 * Reads the chosen saved game's mode icon, then its course image into the
 * course buffer not on screen, which then becomes the one on screen
 */
#define LoadChosenGameImages() \
    { \
        OpenEncodedAsset( \
            g_ModeIconAnimations[(u8) \
                    f_SaveMenuModeIcons[f_SavedGameModes[memory->savedGameIndex]]] \
                                [g_AnimatedIconFrameIndex], \
            &asset \
        ); \
        ReadEncodedAsset(f_ChosenModeIconBuffers[g_AnimatedIconBufferIndex], &asset); \
        if (CHOSEN_GAME.isValid) { \
            if (!CHOSEN_GAME.state.isMiniGolf) { \
                OpenEncodedAsset(g_CourseIconAssets[CHOSEN_GAME.state.courseIndex], &asset); \
                ReadEncodedAsset(f_CourseImageBuffers[f_ShownCourseBufferIndex ^ 1], &asset); \
                f_ShownCourseBufferIndex ^= 1; \
            } \
            else { \
                miniGolfIndex = CHOSEN_GAME.state.courseIndex; \
                miniGolfIndex -= MINI_GOLF_COURSE_INDEX; \
                OpenEncodedAsset(g_MiniGolfCourseIconAssets[miniGolfIndex], &asset); \
                ReadEncodedAsset(f_CourseImageBuffers[f_ShownCourseBufferIndex ^ 1], &asset); \
                OpenAssetFile(g_MiniGolfDiscFiles[miniGolfIndex], &file); \
                ReadAssetFile(g_MiniGolfDiscImages[0], GetAssetFileSize(&file), &file); \
                f_ShownCourseBufferIndex ^= 1; \
            } \
        } \
    }

/*
 * Handles the saved-games screen's buttons. The left and right buttons turn
 * the boards to another saved game. On a game with a mode, each A press gives
 * the next human player the controller that pressed it (all of them get
 * controller 1 when only one is plugged in); once every human has one, A
 * closes the wipe to continue the game. B steps back through the players,
 * then leaves for the title screen. A press while the boards turn finishes
 * the turn instead.
 */
void UpdateSavedGames(void) {
    MenuMemory *memory = GetMenuMemory();
    SaveData *save = GetSaveData();
    EncodedAsset asset;
    AssetFile file;
    /* Pads the frame to match; without it 28 words differ */
    u8 unusedStack[0xA0] UNUSED;
    /*
     * One index for the text slots, players, controllers and humans, to match:
     * a variable of its own for the text slots gives 99 words differing (9 in
     * shape), for the players 20 (2), for the humans 44 (2), for the
     * controllers 46 (6), and for each 110 (10)
     */
    s32 i;
    s32 miniGolfIndex;

    if (GetWipeKind() != WIPE_NONE) {
        for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                i,
                0,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
        }

        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            StopLoaderThread();
            FreeAllCharacterSlots();
            g_IsChosenHoleLoadStarted = FALSE;
            for (i = 1; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    i,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }

            g_MenuScreen = MENU_SCREEN_EXIT;
            for (i = 0; i != MAX_PLAYER_COUNT; i++) {
                g_PlayerControllerIndices[i] = f_SavedPlayerControllers[i];
            }

            CheckControllersAndSetPans();
            ResumeBets();
        }
    }
    else if (IsHudAnimationDone() == TRUE) {
        if (f_AreControllersAssigned) {
            if (g_MenuPressedButtons & A_BUTTON) {
                if (memory->savedGameTurn != 0) {
                    memory->savedGameTurn = 0;
                    return;
                }

                FreeAllCharacterSlots();
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                LoadWipeShape(save->savedGames[memory->savedGameIndex].state.courseIndex);
                StartWipe(WIPE_CLOSING);
                ContinueSavedGame();
                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        i,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                f_AreControllersAssigned = FALSE;
                return;
            }

            if (g_MenuPressedButtons & B_BUTTON) {
                if (memory->savedGameTurn != 0) {
                    memory->savedGameTurn = 0;
                }

                f_AreControllersAssigned = FALSE;
                PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        i,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                if (f_HumanPlayerCount == 1 || g_ControllerCount == 1) {
                    f_AssignedHumanCount = 0;
                    for (i = 0; i != f_HumanPlayerCount; i++) {
                        f_SavedPlayerControllers[f_HumanPlayerIndices[i]] = NO_CONTROLLER;
                    }

                    for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                        SetTextSlot(
                            0,
                            0,
                            NO_MESSAGE,
                            TEXT_SLOT_LARGE_FONT,
                            i,
                            0,
                            0xFF,
                            0xFF,
                            0xFF,
                            OPAQUE_ALPHA
                        );
                    }

                    f_IsGameChosen = FALSE;
                    return;
                }

                f_AssignedHumanCount--;
                f_SavedPlayerControllers[f_HumanPlayerIndices[f_AssignedHumanCount]] =
                    NO_CONTROLLER;
            }
        }
        else if (g_MenuPressedButtons & A_BUTTON) {
            if (memory->savedGameTurn != 0) {
                PlaySound(
                    f_SavedGameModes[memory->savedGameIndex] != NO_SAVE_MENU_MODE ?
                        SOUND_MENU_CONFIRM :
                        SOUND_MENU_BLOCKED,
                    MENU_SOUND_CHANNEL,
                    MENU_VOLUME_SOFT
                );
                memory->savedGameTurn = 0;
                return;
            }

            if (f_SavedGameModes[memory->savedGameIndex] != NO_SAVE_MENU_MODE) {
                PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                if (!f_IsGameChosen) {
                    f_IsGameChosen = TRUE;
                    if (f_HumanPlayerCount == 1) {
                        for (i = 0; i != g_ControllerCount; i++) {
                            if (g_MenuControllerData[i].button & A_BUTTON) {
                                f_SavedPlayerControllers
                                    [f_HumanPlayerIndices[f_AssignedHumanCount]] = i;
                                f_AssignedHumanCount++;
                                f_AreControllersAssigned = TRUE;
                                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                                    SetTextSlot(
                                        0,
                                        0,
                                        NO_MESSAGE,
                                        TEXT_SLOT_LARGE_FONT,
                                        i,
                                        0,
                                        0xFF,
                                        0xFF,
                                        0xFF,
                                        OPAQUE_ALPHA
                                    );
                                }

                                break;
                            }
                        }
                    }
                    else if (g_ControllerCount == 1) {
                        for (i = f_AssignedHumanCount; i != f_HumanPlayerCount; i++) {
                            f_SavedPlayerControllers[f_HumanPlayerIndices[f_AssignedHumanCount]] =
                                0;
                            f_AssignedHumanCount++;
                        }

                        f_AreControllersAssigned = TRUE;
                        for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                            SetTextSlot(
                                0,
                                0,
                                NO_MESSAGE,
                                TEXT_SLOT_LARGE_FONT,
                                i,
                                0,
                                0xFF,
                                0xFF,
                                0xFF,
                                OPAQUE_ALPHA
                            );
                        }
                    }

                    for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                        SetTextSlot(
                            0,
                            0,
                            NO_MESSAGE,
                            TEXT_SLOT_LARGE_FONT,
                            i,
                            0,
                            0xFF,
                            0xFF,
                            0xFF,
                            OPAQUE_ALPHA
                        );
                    }

                    return;
                }

                for (i = 0; i != g_ControllerCount; i++) {
                    if (g_MenuControllerData[i].button & A_BUTTON) {
                        f_SavedPlayerControllers[f_HumanPlayerIndices[f_AssignedHumanCount]] = i;
                        f_AssignedHumanCount++;
                        break;
                    }
                }

                if (f_AssignedHumanCount == f_HumanPlayerCount) {
                    f_AreControllersAssigned = TRUE;
                    for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                        SetTextSlot(
                            0,
                            0,
                            NO_MESSAGE,
                            TEXT_SLOT_LARGE_FONT,
                            i,
                            0,
                            0xFF,
                            0xFF,
                            0xFF,
                            OPAQUE_ALPHA
                        );
                    }
                }
            }
            else {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            }
        }
        else if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            if (memory->savedGameTurn != 0) {
                memory->savedGameTurn = 0;
            }

            if (f_AssignedHumanCount <= 0) {
                StartHudAnimation(HUD_ANIMATION_FADE_OUT);
                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        i,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                g_MenuScreen = MENU_SCREEN_TITLE;
                StopLoaderThread();
                FreeAllCharacterSlots();
                g_IsChosenHoleLoadStarted = FALSE;
            }

            f_AssignedHumanCount--;
            f_SavedPlayerControllers[f_HumanPlayerIndices[f_AssignedHumanCount]] = NO_CONTROLLER;
            if (f_AssignedHumanCount <= 0) {
                f_IsGameChosen = FALSE;
                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        i,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }
            }
        }
        else if (memory->savedGameTurn == 0) {
            if (g_MenuRepeatButtons & L_JPAD) {
                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        i,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                memory->savedGameIndex--;
                if (memory->savedGameIndex < 0) {
                    memory->savedGameIndex = SAVED_GAME_COUNT - 1;
                }

                memory->savedGameTurn = SAVED_GAME_TURN_FRAMES;
                g_AnimatedIconFrameIndex = 0;
                ResetSavedGameControllers();
                LoadChosenGameImages();
            }
            else if (g_MenuRepeatButtons & R_JPAD) {
                for (i = 0; i != SAVED_GAME_TEXT_SLOT_COUNT; i++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        i,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }

                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                memory->savedGameIndex++;
                if (memory->savedGameIndex >= SAVED_GAME_COUNT) {
                    memory->savedGameIndex = 0;
                }

                memory->savedGameTurn = -SAVED_GAME_TURN_FRAMES;
                ResetSavedGameControllers();
                g_AnimatedIconFrameIndex = 0;
                LoadChosenGameImages();
            }
        }
        else {
            if (memory->savedGameTurn < 0) {
                memory->savedGameTurn++;
            }

            if (memory->savedGameTurn > 0) {
                memory->savedGameTurn--;
            }
        }
    }
}

/* Returns -1 when the chosen saved game slot holds no game, and 0 otherwise */
s32 IsChosenGameEmpty(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    SavedGame *game = &save->savedGames[memory->savedGameIndex];

    return -(game->isValid == FALSE);
}

/*
 * Copies the chosen slot's game from the save's mode tables into the
 * players, their records and the game state, to resume at loading the
 * hole. It returns -1 when that game is empty or has too many players.
 * Nothing calls it.
 */
s32 ContinueModeGame(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    SavedGame *game;
    Player *players;
    PlayRecord *records;
    s32 playerCount = 0;
    s32 index;

    switch (memory->savedGameIndex) {
        case SAVE_MENU_GAME_TOURNAMENT:
            game = &save->modeGames[MODE5_GAME_INDEX];
            records = &save->modeRecords[MODE5_RECORD_INDEX];
            players = &save->modePlayers[MODE5_PLAYER_INDEX];
            playerCount = MODE5_PLAYER_COUNT;
            break;
        case SAVE_MENU_GAME_GET_CHARACTER:
            game = &save->modeGames[MODE4_GAME_INDEX];
            records = &save->modeRecords[MODE4_RECORD_INDEX];
            players = &save->modePlayers[MODE4_PLAYER_INDEX];
            playerCount = MODE4_PLAYER_COUNT;
            break;
        case SAVE_MENU_GAME_STROKE:
            game = &save->modeGames[MODE0_GAME_INDEX];
            records = &save->modeRecords[MODE0_RECORD_INDEX];
            players = &save->modePlayers[MODE0_PLAYER_INDEX];
            playerCount = MODE0_PLAYER_COUNT;
            break;
#ifdef AVOID_UB
        default:
            return -1;
#endif
    }

    if (!game->isValid) {
        return -1;
    }

    if (game->state.playerCount > playerCount) {
        return -1;
    }

    playerCount = game->state.playerCount;
    for (index = 0; index != playerCount; index++) {
        g_Players[index] = players[index];
        g_PlayRecords[index] = records[index];
    }

    g_GameState = game->state;
    g_GameState.step = GAME_STATE_LOAD_HOLE;
    SetGameFlag(GAME_FLAG_LOADED_PLAY);
    g_HoleSetup = game->hole;
    g_GameState.isRoundContinued = ROUND_CONTINUED;
    return 0;
}

/*
 * Resumes the chosen saved game with the controllers its players chose,
 * and erases it from the save unless GAME_FLAG_KEEP_SAVED_GAMES is set.
 * It returns -1 when the game can't be resumed.
 */
s32 ContinueSavedGame(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    Player *savedPlayer;
    s32 playerIndex;
    CharacterId character;
    u16 controller;

    if (f_SavedGameModes[memory->savedGameIndex] == NO_SAVE_MENU_MODE) {
        return -1;
    }

    if (SaveOrContinueGame(memory->savedGameIndex, TRUE) == -1) {
        return -1;
    }

    savedPlayer = save->savedPlayers[memory->savedGameIndex];
    for (playerIndex = 0; playerIndex != MAX_PLAYER_COUNT; playerIndex++) {
        controller = f_SavedPlayerControllers[playerIndex];
        g_PlayerControllerIndices[playerIndex] = controller;
        g_Players[playerIndex].controllerIndex = controller;
        sprintf(g_Players[playerIndex].name, f_SaveMenuNameFormat, savedPlayer->name);
        character = savedPlayer->characterIndex;
        g_ChosenCharacters[playerIndex] = character;
        g_Players[playerIndex].characterIndex = character;
        LoadPlayerFace(playerIndex, character, g_Players[playerIndex].paletteIndex);
        savedPlayer++;
    }

    if (!IsGameFlagSet(GAME_FLAG_KEEP_SAVED_GAMES)) {
        memset(&save->savedGames[memory->savedGameIndex], 0, sizeof(SavedGame));
    }

    return 0;
}

const char f_SaveMenuNameFormat[] = "%s";

/* The backdrop layers that DrawLargeBannerBackdropLayers draws behind the saved
 * games */
#define SAVED_GAMES_BACKDROP_LAYERS \
    (MENU_LAYER_SCENERY | MENU_LAYER_IGNORED | MENU_LAYER_BANNER | MENU_LAYER_LARGE_BANNER)

/* The numbered boards of f_GameBoards: 64x64 CI8, in two strips */
#define GAME_BOARD_WIDTH 64
#define GAME_BOARD_STRIP_HEIGHT 32
#define GAME_BOARD_STRIP_COUNT 2
/* The animated mode icon of f_ChosenModeIconBuffers, drawn on the board */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* Where the mode icon sits on the board */
#define MODE_ICON_OFFSET_X 8
#define MODE_ICON_OFFSET_Y 14

/*
 * The boards stand on a circle, seen from above, that each update of
 * MenuMemory.savedGameTurn turns by a step. They are projected like the
 * course wheel, from a nearer eye.
 */
#define BOARD_EYE_HEIGHT 70.0f
#define BOARD_EYE_DISTANCE 60.0f
#define BOARD_RADIUS_X 48.0f
#define BOARD_RADIUS_Z 40.0f
#define BOARD_TURN_STEP_DEGREES 15
#define GAME_BOARD_HALF_SIZE 32.0f
/* The mode icons on the boards not chosen are smaller, and lower */
#define MODE_ICON_HALF_SIZE 24.0f
#define MODE_ICON_DROP 4
/* The boards not chosen, and the chosen one while it turns, are darkened */
#define UNCHOSEN_BOARD_SHADE 127
/* The bob that a fourth board would have, a step of its phase per update */
#define BOARD_BOB_PHASE_STEP 0.1f
#define BOARD_BOB_HEIGHT 8.0f
/* The 200 by 10 strips of f_SavedGamesNamePlate, drawn narrower by the player
 * count */
#define SAVED_GAME_STRIP_WIDTH 200
#define SAVED_GAME_STRIP_HEIGHT 10
#define SAVED_GAME_STRIP_COUNT 3
#define SAVED_GAME_STRIP_TOP 152
#define SAVED_GAME_STRIP_LEFT 80
#define SAVED_GAME_TWO_PLAYER_STRIP_LEFT 70
#define SAVED_GAME_STRIP_SHOWN_WIDTH 160
#define SAVED_GAME_FEW_PLAYER_STRIP_SHOWN_WIDTH 180
#define SAVED_GAME_ARROW_GAP 80
#define SAVED_GAME_ARROW_Y 124
/* The rows of the game modes' image, 192 by 20 CI4 each */
#define GAME_MODE_LABEL_WIDTH 192
#define GAME_MODE_LABEL_HEIGHT 20
#define GAME_MODE_LABEL_IMAGE_SIZE ((GAME_MODE_LABEL_WIDTH * GAME_MODE_LABEL_HEIGHT) / 2)
#define GAME_MODE_LABEL_X 160
#define SINGLE_PLAYER_GAME_MODE_LABEL_X 170
#define GAME_MODE_LABEL_TOP 156
#define SAVED_GAME_COURSE_X 110
#define SAVED_GAME_COURSE_Y 187
/* A tournament shows a message of each course in place of its name */
#define MESSAGE_FIRST_TOURNAMENT_COURSE 0x6F
/* Mini golf courses from this one use Peach's Castle's name */
#define FIRST_PEACHS_CASTLE_MINI_GOLF_COURSE (MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD)
/* Shows the slot's first value, here the saved hole's number */
#define SAVED_GAME_HOLE_X 180
#define SAVED_GAME_HOLE_Y 203
#define MESSAGE_SAVED_GAME_HOLE 0x76
/* Shown while the next human player is given a controller, then once every
 * human has one */
#define CONTROLLER_PROMPT_X 70
#define CONTINUE_PROMPT_X 54
#define PROMPT_Y 184
#define MESSAGE_CONTROLLER_PROMPT 0x37
#define MESSAGE_CONTINUE_PROMPT 0x38
/* Shown when the chosen saved game is empty */
#define EMPTY_GAME_TEXT_X 100
#define EMPTY_GAME_TEXT_Y 186
#define MESSAGE_EMPTY_SAVED_GAME 0x26

void DrawChosenGameBoard(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale);
void DrawSavedGolfers(Gfx **displayListHead);
void DrawPlayerControllers(Gfx **displayListHead);
void DrawSavedCourseImage(Gfx **displayListHead);
void DrawSavedMiniGolfImage(Gfx **displayListHead);

/*
 * Draws the saved-games screen: the characters, the three numbered boards
 * turning on their circle with each game's mode icon, the chosen game's mode,
 * course and hole, and the prompt for giving out the controllers.
 *
 * Doesn't match: the original keeps the menu memory pointer on the stack and
 * the mode icons' texel step in a saved register. Like the original, ours loads
 * the degrees-to-radians factor on each pass of the board loop only when
 * boardDegrees is the loop's own: set before the loop, loop.c hoists the factor
 * instead (403 words, 151 in shape). The original still multiplies by a 120 it
 * loads in the loop, where ours keeps it in a saved register; a literal 120 is
 * expanded into shifts (417 words, 163 in shape). Each mode icon's image is
 * read once into modeImage, as the original reads it once for the palette and
 * the strips; reading the table at each use gives 401 words, 149 in shape.
 * Reading the name plate into a local too, as a permuter output does, gives 331
 * words, 131 in shape, or 328 and 132 for its strips alone. The register
 * oracle, forcing 31 pseudos' registers, left 237 words at best when the draft
 * gave 307, so the code differs too. Adding the strip's offset to the mode
 * icon's texels, rather than adding the texel offset last, took the draft from
 * 307 words differing to 293, 121 in shape to 115; any order that adds the
 * texel offset to modeImage first gives 293, and a texels pointer set before
 * the strip loop 320. Clearing the text slots with the strips' counter took the
 * draft to 255 words, 115 in shape, and the register oracle's closest result
 * from 227 words, forcing 30 pseudos, to 219, forcing 24.
 *
 * The draft then gave 255 words, 115 in shape. The game mode label's x was
 * held in courseIndex; in a labelX local of its own, 287 words differ, but
 * 91 in shape, and the register oracle's closest result falls from 219
 * words, forcing 24 pseudos, to 183, forcing 28: the code comes closer to
 * the original's, though more registers differ. 287 words differ, 91 in
 * shape.
 *
 * The register oracle reports courseIndex lining up with $s1, the register
 * the original gives the shared counter, and the hole index with $s6.
 * Counting the boards and strips with courseIndex in place of holeIndex
 * gives 597 words (99 in shape). The report lists gfx, the strip counter and
 * two other values as lining up with $s1 too, so it is weak. A counter of
 * its own, declared before or after holeIndex, which then holds only the
 * chosen game's hole, also gives 597 words (99 in shape), so holeIndex keeps
 * both roles.
 *
 * A u16 playerCount gives 223 words, but 97 in shape. The original reloads
 * the count from its stack slot and sign-extends it with sll and sra by 16
 * before the first player-count test, as the s16 does, where the u16 uses
 * the reloaded half as it is, so the count is signed.
 *
 * The original keeps 48.0, the board circle's x radius, in $f24 through the
 * board loop, and the multiplier for the % 3 in $s7, but loads 120 and the
 * degrees-to-radians factor on each pass. This draft keeps 120 and the
 * multiplier out of the loop instead and loads 48.0 on each pass, so its
 * frame lacks the $f24 slot and is 8 bytes smaller. The loop dump shows why:
 * loop.c moves boardDegrees' 120 and then the multiplier, lowering its
 * threshold by 3 with each move, and then finds moving the factor, 48.0 and
 * 40.0 not desirable. Re-measured on the current draft, boardDegrees set
 * before the loop gives 289 words (93 in shape), with the original's frame
 * and $f24 saved, but loop.c then moves the multiplier and the factor rather
 * than 48.0, and the register oracle's closest result rises from 183 words,
 * forcing 28 pseudos, to 195, forcing 29. A literal 120 gives 303 words (105
 * in shape). Writing the radius as the product's first factor, for x or for
 * both axes, or the x product as ProjectPoint's argument leaves 287.
 *
 * The chosen game's mode label works out its x first, before its offset
 * and texture, as a sweep moving each assignment found. That takes the draft from 287 words to 213,
 * 91 in shape either way, and the register oracle's closest from 183 words, forcing 28 pseudos, to
 * 183 forcing 17.
 *
 * The prompts test for a chosen game that isn't empty first, and set the
 * empty game's text in the else, as a permuter output does. That takes the
 * draft from 213 words (91 in shape) to 207 (89), and the register oracle's
 * closest from 183 words to 177, forcing 17 pseudos either way. 207 words
 * differ, 89 in shape.
 */
#ifdef NON_MATCHING
void DrawSavedGames(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspNorm;
    /* Sized like the course wheel's, of which the boards use three */
    f32 boardX[COURSE_COUNT];
    f32 boardY[COURSE_COUNT];
    f32 boardScale[COURSE_COUNT];
    /* Pads the frame to match; without it 669 words differ, against 287 */
    s32 unusedSlots[COURSE_COUNT] UNUSED;
    s32 drawOrder[COURSE_COUNT];
    /* Pads the frame to match; without it 659 words differ, against 287 */
    u8 unusedStack[8] UNUSED;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
    s32 index;
    /*
     * Also the text slots that the controller prompt clears, to match: index
     * for them gives 325 words differing against the draft's 287, 91 in shape
     * both
     */
    s32 stripOrTextSlotIndex;
    s32 key;
    Radians angle;
    f32 circleX;
    s32 slot;
    s32 halfSize;
    s32 step;
    s32 iconHalfSize;
    s32 iconStep;
    f32 bob;
    s32 stripLeft;
    s32 stripShownWidth;
    s32 gameIndex;
    s32 courseIndex;
    /*
     * Also the board index in the sorting loops and the strip index in the
     * boards' and icons' loops, to match: a board variable for the sorting
     * loops gives 369 words differing (91 in shape), and a stripIndex for the
     * strip loops 541 (99), against the draft's 287 (91)
     */
    s32 holeIndex;
    s32 labelX;
    s32 *chosenMode;
    s32 *labelOffset;
    u32 modeLabelTexels;
    s32 slotOffset;

    DrawLargeBannerBackdropLayers(&gfx, SAVED_GAMES_BACKDROP_LAYERS);
    BuildDynamicTextures(&gfx);
    DrawSavedGolfers(&gfx);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    DrawPlayerControllers(&gfx);

    guPerspectiveF(
        f_BoardProjection,
        &perspNorm,
        WHEEL_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        WHEEL_NEAR_PLANE,
        WHEEL_FAR_PLANE,
        1.0f
    );
    guLookAtF(
        f_BoardViewing,
        0.0f,
        BOARD_EYE_HEIGHT,
        BOARD_EYE_DISTANCE,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f
    );
    SetCpuProjectionF(f_BoardProjection, f_BoardViewing, g_Viewports);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);

    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        ScreenPoint point;
        s32 boardDegrees = DEGREES_PER_TURN / SAVED_GAME_COUNT;

        slotOffset = (memory->savedGameIndex - index) % SAVED_GAME_COUNT;
        angle = -DegreesToRadians(
            (slotOffset * boardDegrees) + (memory->savedGameTurn * BOARD_TURN_STEP_DEGREES)
        );
        circleX = sinf(angle) * BOARD_RADIUS_X;
        if (ProjectPoint(circleX, 0.0f, cosf(angle) * BOARD_RADIUS_Z, &point) < 0) {
            boardX[index] = point.x;
            boardY[index] = point.y - WHEEL_RAISE;
            boardScale[index] = point.scaleX;
        }
    }

    for (holeIndex = 0; holeIndex != SAVED_GAME_COUNT; holeIndex++) {
        drawOrder[holeIndex] = holeIndex;
    }

    /* Draw the farthest boards first, so the nearer ones cover them */
    for (holeIndex = 1; holeIndex != SAVED_GAME_COUNT; holeIndex++) {
        key = drawOrder[holeIndex];
        for (index = holeIndex - 1; index >= 0 && boardY[drawOrder[index]] > boardY[key]; index--) {
            drawOrder[index + 1] = drawOrder[index];
        }

        drawOrder[index + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    f_BoardBobPhase += BOARD_BOB_PHASE_STEP;
    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        slot = drawOrder[index];
        iconHalfSize = boardScale[slot] * MODE_ICON_HALF_SIZE;
        halfSize = boardScale[slot] * GAME_BOARD_HALF_SIZE;
        f_ChosenModeIconHalfSize = iconHalfSize;
        iconStep = (f32)(MODE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / iconHalfSize;
        f_ChosenModeIconScale = iconStep;
        step = (f32)(GAME_BOARD_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
        /* The loop stops before SAVED_GAME_COUNT, so no board bobs */
        if (index == SAVED_GAME_COUNT) {
            bob = sinf(f_BoardBobPhase) * BOARD_BOB_HEIGHT;
        }
        else {
            bob = 0.0f;
        }

        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        if ((memory->savedGameIndex != slot) || (memory->savedGameTurn != 0)) {
            if (memory->savedGameIndex == slot) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            }
            else {
                gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    UNCHOSEN_BOARD_SHADE,
                    UNCHOSEN_BOARD_SHADE,
                    UNCHOSEN_BOARD_SHADE,
                    OPAQUE_ALPHA
                );
            }

            gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_GameBoards[slot])));
            for (holeIndex = 0; holeIndex != GAME_BOARD_STRIP_COUNT; holeIndex++) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(
                        LargeImageTexels(f_GameBoards[slot]) +
                        (holeIndex * GAME_BOARD_WIDTH * GAME_BOARD_STRIP_HEIGHT)
                    ),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    GAME_BOARD_WIDTH,
                    GAME_BOARD_STRIP_HEIGHT,
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
                    QuarterPixels((s32)(boardX[slot] - halfSize)),
                    QuarterPixels((s32)(boardY[slot] - halfSize + (holeIndex * halfSize) + bob)),
                    QuarterPixels((s32)(boardX[slot] + halfSize)),
                    QuarterPixels((s32)(boardY[slot] + (holeIndex * halfSize) + bob)),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    step,
                    step
                );
                gDPPipeSync(gfx++);
            }

            if (f_SavedGameModes[slot] != NO_SAVE_MENU_MODE) {
                u8 *modeImage = f_SavedGameModeImages[slot];

                gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(modeImage)));
                for (holeIndex = 0; holeIndex != MODE_ICON_STRIP_COUNT; holeIndex++) {
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(
                            modeImage + LARGE_PALETTED_TEXELS_OFFSET +
                            (holeIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)
                        ),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        MODE_ICON_WIDTH,
                        MODE_ICON_STRIP_HEIGHT,
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
                        QuarterPixels((s32)(boardX[slot] - iconHalfSize)),
                        QuarterPixels(
                            (s32)(boardY[slot] - iconHalfSize + (holeIndex * iconHalfSize) + bob) +
                            MODE_ICON_DROP
                        ),
                        QuarterPixels((s32)(boardX[slot] + iconHalfSize)),
                        QuarterPixels(
                            (s32)(boardY[slot] + (holeIndex * iconHalfSize) + bob) + MODE_ICON_DROP
                        ),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        iconStep,
                        iconStep
                    );
                    gDPPipeSync(gfx++);
                }
            }
        }
        else {
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            DrawChosenGameBoard(
                &gfx,
                boardX[slot] - halfSize,
                boardY[slot] - halfSize + bob,
                halfSize,
                step
            );
        }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_SavedGamesNamePlate)));
    for (stripOrTextSlotIndex = 0; stripOrTextSlotIndex != SAVED_GAME_STRIP_COUNT;
        stripOrTextSlotIndex++) {
        if (playerCount == 1) {
            stripLeft = SAVED_GAME_STRIP_LEFT;
            stripShownWidth = SAVED_GAME_FEW_PLAYER_STRIP_SHOWN_WIDTH;
        }
        else if (playerCount == 2) {
            stripLeft = SAVED_GAME_TWO_PLAYER_STRIP_LEFT;
            stripShownWidth = SAVED_GAME_FEW_PLAYER_STRIP_SHOWN_WIDTH;
        }
        else {
            stripLeft = SAVED_GAME_STRIP_LEFT;
            stripShownWidth = SAVED_GAME_STRIP_SHOWN_WIDTH;
        }

        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_SavedGamesNamePlate) +
                (stripOrTextSlotIndex * SAVED_GAME_STRIP_WIDTH * SAVED_GAME_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            SAVED_GAME_STRIP_WIDTH,
            SAVED_GAME_STRIP_HEIGHT,
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
            QuarterPixels(stripLeft),
            QuarterPixels(SAVED_GAME_STRIP_TOP + (stripOrTextSlotIndex * SAVED_GAME_STRIP_HEIGHT)),
            QuarterPixels(stripLeft + stripShownWidth),
            QuarterPixels(
                SAVED_GAME_STRIP_TOP + SAVED_GAME_STRIP_HEIGHT +
                (stripOrTextSlotIndex * SAVED_GAME_STRIP_HEIGHT)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(SAVED_GAME_STRIP_WIDTH, stripShownWidth),
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    DrawCenteredPageArrows(&gfx, SAVED_GAME_ARROW_GAP, SAVED_GAME_ARROW_Y);
    chosenMode = &f_SavedGameModes[memory->savedGameIndex];
    if (*chosenMode != NO_SAVE_MENU_MODE) {
        labelX = (playerCount == 1) ? SINGLE_PLAYER_GAME_MODE_LABEL_X : GAME_MODE_LABEL_X;
        labelOffset = &f_GameModeLabelOffsets[f_SaveMenuModeIcons[*chosenMode]];
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_GameModeLabels)));
        modeLabelTexels = TextureAddress(
            ImageTexels(f_GameModeLabels) +
            (f_SaveMenuModeIcons[*chosenMode] * GAME_MODE_LABEL_IMAGE_SIZE)
        );
        gDPLoadTextureBlock_4b(
            gfx++,
            modeLabelTexels,
            G_IM_FMT_CI,
            GAME_MODE_LABEL_WIDTH,
            GAME_MODE_LABEL_HEIGHT,
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
            QuarterPixels(labelX - *labelOffset),
            QuarterPixels(GAME_MODE_LABEL_TOP),
            QuarterPixels(labelX - *labelOffset + GAME_MODE_LABEL_WIDTH),
            QuarterPixels(GAME_MODE_LABEL_TOP + GAME_MODE_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    if (!IsChosenGameEmpty()) {
        if (!f_IsGameChosen) {
            gameIndex = memory->savedGameIndex;
            if (gameIndex < 0) {
                gameIndex = 0;
            }

            if (gameIndex >= SAVED_GAME_COUNT) {
                gameIndex = SAVED_GAME_COUNT - 1;
            }

            if (save->savedGames[gameIndex].isValid) {
                if (!save->savedGames[gameIndex].state.isMiniGolf) {
                    DrawSavedCourseImage(&gfx);
                }
                else {
                    DrawSavedMiniGolfImage(&gfx);
                }
            }

            SetUpTextDrawing(&gfx);
            SetTextColor(&gfx, 255, 255, 255);
            courseIndex = CHOSEN_GAME.state.courseIndex;
            holeIndex = CHOSEN_GAME.state.holeIndex;
            if (CHOSEN_GAME.state.gameMode == GAME_MODE_TOURNAMENT) {
                SetTextSlot(
                    SAVED_GAME_COURSE_X,
                    SAVED_GAME_COURSE_Y,
                    MESSAGE_FIRST_TOURNAMENT_COURSE + courseIndex,
                    TEXT_SLOT_LARGE_FONT,
                    0,
                    0,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
            else {
                if (CHOSEN_GAME.state.isMiniGolf == TRUE) {
                    courseIndex = (courseIndex >= FIRST_PEACHS_CASTLE_MINI_GOLF_COURSE) ?
                        COURSE_RECORD_PEACHS_CASTLE :
                        COURSE_RECORD_LUIGIS_GARDEN;
                }

                sprintf(g_PrintBuffer, f_SaveMenuNameFormat, g_CourseRecords[courseIndex].name);
                DrawLargeText(&gfx, SAVED_GAME_COURSE_X, SAVED_GAME_COURSE_Y, g_PrintBuffer);
            }

            SetTextSlotValue(holeIndex + 1, 1, 0);
            SetTextSlot(
                SAVED_GAME_HOLE_X,
                SAVED_GAME_HOLE_Y,
                MESSAGE_SAVED_GAME_HOLE,
                TEXT_SLOT_LARGE_FONT,
                1,
                0,
                0xFF,
                0xFF,
                0xFF,
                OPAQUE_ALPHA
            );
        }
        else if (GetWipeKind() == 0) {
            if (!f_AreControllersAssigned) {
                SetTextSlotValue(f_HumanPlayerIndices[f_AssignedHumanCount] + 1, 0, 0);

                /* The prompt names the player in the player's own color */
                if (f_HumanPlayerIndices[f_AssignedHumanCount] == 0) {
                    SetTextPaletteColor(255, 127, 127, 0);
                }

                if (f_HumanPlayerIndices[f_AssignedHumanCount] == 1) {
                    SetTextPaletteColor(0, 255, 255, 0);
                }

                if (f_HumanPlayerIndices[f_AssignedHumanCount] == 2) {
                    SetTextPaletteColor(255, 255, 0, 0);
                }

                if (f_HumanPlayerIndices[f_AssignedHumanCount] == 3) {
                    SetTextPaletteColor(63, 255, 63, 0);
                }

                SetTextSlot(
                    CONTROLLER_PROMPT_X,
                    PROMPT_Y,
                    MESSAGE_CONTROLLER_PROMPT,
                    TEXT_SLOT_LARGE_FONT,
                    0,
                    TEXT_FLAG_TYPED,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
                for (stripOrTextSlotIndex = 1; stripOrTextSlotIndex != SAVED_GAME_TEXT_SLOT_COUNT;
                    stripOrTextSlotIndex++) {
                    SetTextSlot(
                        0,
                        0,
                        NO_MESSAGE,
                        TEXT_SLOT_LARGE_FONT,
                        stripOrTextSlotIndex,
                        0,
                        0xFF,
                        0xFF,
                        0xFF,
                        OPAQUE_ALPHA
                    );
                }
            }
            else {
                SetTextSlot(
                    CONTINUE_PROMPT_X,
                    PROMPT_Y,
                    MESSAGE_CONTINUE_PROMPT,
                    TEXT_SLOT_LARGE_FONT,
                    0,
                    TEXT_FLAG_TYPED,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
            }
        }
    }
    else {
        SetTextSlot(
            EMPTY_GAME_TEXT_X,
            EMPTY_GAME_TEXT_Y,
            MESSAGE_EMPTY_SAVED_GAME,
            TEXT_SLOT_LARGE_FONT,
            0,
            TEXT_FLAG_TYPED,
            0xFF,
            0xFF,
            0xFF,
            OPAQUE_ALPHA
        );
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/save_menu", DrawSavedGames);
#endif

/*
 * Draws the chosen saved game's board, halfSize * 2 pixels square, at (left,
 * top) and, if the game has a mode image, the mode's icon on top, reading the
 * icon's next frame in turn
 */
void DrawChosenGameBoard(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
    MenuMemory *memory = GetMenuMemory();
    Gfx *gfx = *displayListHead;
    s32 iconHalfSize = f_ChosenModeIconHalfSize;
    s32 iconScale = f_ChosenModeIconScale;
    EncodedAsset asset;
    s32 stripIndex;
    void *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_GameBoards[memory->savedGameIndex])));
    for (stripIndex = 0; stripIndex != GAME_BOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_GameBoards[memory->savedGameIndex]) +
                (stripIndex * GAME_BOARD_WIDTH * GAME_BOARD_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            GAME_BOARD_WIDTH,
            GAME_BOARD_STRIP_HEIGHT,
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
            QuarterPixels(left),
            QuarterPixels(top + (stripIndex * halfSize)),
            QuarterPixels(left + halfSize + halfSize),
            QuarterPixels(top + halfSize + (stripIndex * halfSize)),
            G_TX_RENDERTILE,
            0,
            0,
            scale,
            scale
        );
        gDPPipeSync(gfx++);
    }

    if (f_SavedGameModes[memory->savedGameIndex] != NO_SAVE_MENU_MODE) {
        image = f_ChosenModeIconBuffers[g_AnimatedIconBufferIndex];
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));
        for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(image) +
                    (stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                MODE_ICON_WIDTH,
                MODE_ICON_STRIP_HEIGHT,
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
                QuarterPixels(left + MODE_ICON_OFFSET_X),
                QuarterPixels(top + (stripIndex * iconHalfSize) + MODE_ICON_OFFSET_Y),
                QuarterPixels(left + iconHalfSize + iconHalfSize + MODE_ICON_OFFSET_X),
                QuarterPixels(
                    top + iconHalfSize + (stripIndex * iconHalfSize) + MODE_ICON_OFFSET_Y
                ),
                G_TX_RENDERTILE,
                0,
                0,
                iconScale,
                iconScale
            );
            gDPPipeSync(gfx++);
        }

        g_AnimatedIconFrameTime++;
        if (g_AnimatedIconFrameTime == MODE_ICON_FRAME_DURATION) {
            iconIndex = f_SaveMenuModeIcons[f_SavedGameModes[memory->savedGameIndex]];
            g_AnimatedIconFrameTime = 0;
            g_AnimatedIconFrameIndex++;
            if (g_AnimatedIconFrameIndex >= MODE_ICON_FRAME_COUNT) {
                g_AnimatedIconFrameIndex = 0;
            }

            if (g_ModeIconAnimations[iconIndex][g_AnimatedIconFrameIndex] == END_OF_ICON_FRAMES) {
                g_AnimatedIconFrameIndex = 0;
            }

            g_AnimatedIconBufferIndex = g_AnimatedIconBufferIndex == 0;
            OpenEncodedAsset(g_ModeIconAnimations[iconIndex][g_AnimatedIconFrameIndex], &asset);
            ReadEncodedAsset(f_ChosenModeIconBuffers[g_AnimatedIconBufferIndex], &asset);
        }
    }

    *displayListHead = gfx;
}

/*
 * Draws the golfers of the selected saved game, loading each one's model
 * once its character slot is free. A golfer stands and, when its fidget
 * timer runs out, plays the fidget animation once before waiting a random
 * while again.
 */
void DrawSavedGolfers(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
    Player *players = save->savedPlayers[memory->savedGameIndex];
    u16 playerIndex;
    s32 raise;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    /* Pads the frame to match; without it 62 words differ */
    s32 unused[12] UNUSED;
    Mat4 transform;
    Mat4 viewing;

    if (!save->savedGames[memory->savedGameIndex].isValid) {
        return;
    }

    switch (playerCount) {
        case 1:
            f_SavedGolferScales[0] = 5.0f;
            break;
        case 2:
            f_SavedGolferScales[0] = 4.0f;
            f_SavedGolferScales[1] = 4.0f;
            break;
        case 3:
        case 4:
            f_SavedGolferScales[0] = 3.5f;
            f_SavedGolferScales[1] = 3.4f;
            f_SavedGolferScales[2] = 3.4f;
            f_SavedGolferScales[3] = 3.5f;
            break;
        default:
            f_SavedGolferScales[0] = 4.0f;
            f_SavedGolferScales[1] = 4.0f;
            f_SavedGolferScales[2] = 4.0f;
            f_SavedGolferScales[3] = 4.0f;
            break;
    }

    guOrtho(
        &f_SavedGolfersProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CHARACTERS_NEAR,
        MENU_CHARACTERS_FAR,
        MENU_CHARACTERS_ORTHO_SCALE
    );
    guLookAtReflectF(
        viewing,
        &f_SavedGolfersLookAts[g_DrawBufferIndex],
        0.0f,
        MENU_CHARACTERS_EYE_Y,
        MENU_CHARACTERS_EYE_Z,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f
    );
    gSPLookAt(gfx++, &f_SavedGolfersLookAts[g_DrawBufferIndex]);
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        if (g_MenuFidgetDelays[playerIndex] > 0) {
            g_MenuFidgetDelays[playerIndex]--;
        }

        if (g_MenuFidgetDelays[playerIndex] == 0) {
            g_MenuFidgetDelays[playerIndex] = FIDGETING;
            SetAnimationLooping(playerIndex, FALSE);
            SetNextAnimation(playerIndex, MENU_ANIMATION_FIDGET);
        }

        if (g_MenuFidgetDelays[playerIndex] == FIDGETING) {
            if (GetAnimationEndFrame(playerIndex, MENU_ANIMATION_FIDGET) ==
                GetAnimationFrame(playerIndex)) {
                SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
                SetAnimationLooping(playerIndex, TRUE);
                g_MenuFidgetDelays[playerIndex] = guRandom() % FIDGET_DELAY_RANGE;
            }
        }

        if (!g_IsMenuCharacterLoaded[playerIndex]) {
            if (IsCharacterSlotBusy(playerIndex)) {
                FreeCharacterSlot(playerIndex);
            }
            else {
                LoadCharacter(
                    playerIndex,
                    players[playerIndex].characterIndex + CHARACTER_COUNT,
                    players[playerIndex].paletteIndex
                );
                g_IsMenuCharacterLoaded[playerIndex] = TRUE;
                SetCharacterAnimation(playerIndex, MENU_ANIMATION_STANDING);
                SetAnimationSpeed(playerIndex, 1.0f);
            }
        }

        rotation[AXIS_X] = DegreesToRadians(f_SavedGolferAngles[0]);
        rotation[AXIS_Y] = DegreesToRadians(f_SavedGolferAngles[1]);
        rotation[AXIS_Z] = 0.0f;
        raise =
            (players[playerIndex].characterIndex == CHARACTER_BABY_MARIO) ? BABY_MARIO_RAISE : 0;
        translation[AXIS_X] = f_SavedGolferPositions[playerCount - 1][playerIndex][0];
        translation[AXIS_Y] = f_SavedGolferPositions[playerCount - 1][playerIndex][1] + raise;
        translation[AXIS_Z] = f_SavedGolferPositions[playerCount - 1][playerIndex][2];
        scale[AXIS_X] = f_SavedGolferScales[playerIndex];
        scale[AXIS_Y] = f_SavedGolferScales[playerIndex];
        scale[AXIS_Z] = f_SavedGolferScales[playerIndex];
        BuildTransformMatrix(transform, rotation, translation, scale);
        ConvertMatrixToFixed(transform, &f_SavedGolferTransforms[playerIndex][g_DrawBufferIndex]);
        AnimateCharacter(playerIndex);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_SavedGolfersProjection),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
        );
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_SavedGolferTransforms[playerIndex][g_DrawBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gDPPipeSync(gfx++);
        g_DisplayListHead = gfx;
        DrawCharacter(playerIndex);
        *displayListHead = g_DisplayListHead;
        gfx = *displayListHead;
    }
}

/*
 * Draws the chosen saved game's players' controller icons, once each has
 * pressed A on one, and their player or computer labels.
 */
void DrawPlayerControllers(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
    s32 position[2];
    s32 playerIndex;
    s32 labelIndex;
    s16 controller;
    void *image;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    image = f_ControllerIcons;
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(image)));
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        if (f_SavedPlayerControllers[playerIndex] != NO_CONTROLLER) {
            position[AXIS_X] = f_ControllerIconPositions[playerCount - 1][playerIndex][0];
            position[AXIS_Y] = f_ControllerIconPositions[playerCount - 1][playerIndex][1];
            controller = f_SavedPlayerControllers[playerIndex];
            if (controller != COMPUTER_CONTROLLER) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(
                        LargeImageTexels(image) + (controller * CONTROLLER_ICON_IMAGE_SIZE)
                    ),
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    CONTROLLER_ICON_SIZE,
                    CONTROLLER_ICON_SIZE,
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
                    QuarterPixels(position[AXIS_X]),
                    QuarterPixels(position[AXIS_Y]),
                    QuarterPixels(position[AXIS_X] + CONTROLLER_ICON_SIZE),
                    QuarterPixels(position[AXIS_Y] + CONTROLLER_ICON_SIZE),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                gDPPipeSync(gfx++);
            }
        }
    }

    image = f_PlayerLabels;
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(image)));
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        position[AXIS_X] = f_ControllerIconPositions[playerCount - 1][playerIndex][0];
        position[AXIS_Y] = f_ControllerIconPositions[playerCount - 1][playerIndex][1];
        if (f_SavedPlayerControllers[playerIndex] == COMPUTER_CONTROLLER) {
            if (save->savedGames[memory->savedGameIndex].state.gameMode ==
                GAME_MODE_GET_CHARACTER) {
                labelIndex = PLAYER_LABEL_COM;
            }
            else {
                labelIndex = playerIndex + PLAYER_LABEL_COMPUTER_FIRST;
            }

            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(image) + (labelIndex * PLAYER_LABEL_IMAGE_SIZE)),
                G_IM_FMT_CI,
                PLAYER_LABEL_WIDTH,
                PLAYER_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }
        else {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(ImageTexels(image) + (playerIndex * PLAYER_LABEL_IMAGE_SIZE)),
                G_IM_FMT_CI,
                PLAYER_LABEL_WIDTH,
                PLAYER_LABEL_HEIGHT,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
        }

        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(position[AXIS_X] + PLAYER_LABEL_OFFSET_X),
            QuarterPixels(position[AXIS_Y] + PLAYER_LABEL_OFFSET_Y),
            QuarterPixels(position[AXIS_X] + PLAYER_LABEL_OFFSET_X + PLAYER_LABEL_WIDTH),
            QuarterPixels(position[AXIS_Y] + PLAYER_LABEL_OFFSET_Y + PLAYER_LABEL_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

/* Draws the chosen saved game's course image, shrunk to a thumbnail */
void DrawSavedCourseImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    GetSaveData();
    GetMenuMemory();
    gDPLoadTLUT_pal256(
        gfx++,
        TextureAddress(ImageBody(f_CourseImageBuffers[f_ShownCourseBufferIndex]))
    );
    for (stripIndex = 0; stripIndex != COURSE_THUMBNAIL_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_CourseImageBuffers[f_ShownCourseBufferIndex]) +
                (stripIndex * COURSE_THUMBNAIL_WIDTH * COURSE_THUMBNAIL_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            COURSE_THUMBNAIL_WIDTH,
            COURSE_THUMBNAIL_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = COURSE_THUMBNAIL_TOP + (stripIndex * COURSE_THUMBNAIL_SHOWN_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(COURSE_THUMBNAIL_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(COURSE_THUMBNAIL_LEFT + COURSE_THUMBNAIL_SHOWN_WIDTH),
            QuarterPixels(stripTop + COURSE_THUMBNAIL_SHOWN_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(COURSE_THUMBNAIL_WIDTH, COURSE_THUMBNAIL_SHOWN_WIDTH),
            TexelRunStep(COURSE_THUMBNAIL_STRIP_HEIGHT, COURSE_THUMBNAIL_SHOWN_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

/* Draws the chosen saved mini golf game's course image with its own palette */
void DrawSavedMiniGolfImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;
    void *palette;

    GetSaveData();
    GetMenuMemory();
    palette = g_MiniGolfDiscImages[0];
    gDPLoadTLUT_pal256(gfx++, TextureAddress(palette));
    for (stripIndex = 0; stripIndex != COURSE_THUMBNAIL_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                (u32)f_CourseImageBuffers[f_ShownCourseBufferIndex] +
                (stripIndex * COURSE_THUMBNAIL_WIDTH * COURSE_THUMBNAIL_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            COURSE_THUMBNAIL_WIDTH,
            COURSE_THUMBNAIL_STRIP_HEIGHT,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        stripTop = COURSE_THUMBNAIL_TOP + (stripIndex * COURSE_THUMBNAIL_SHOWN_HEIGHT);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(COURSE_THUMBNAIL_LEFT),
            QuarterPixels(stripTop),
            QuarterPixels(COURSE_THUMBNAIL_LEFT + COURSE_THUMBNAIL_SHOWN_WIDTH),
            QuarterPixels(stripTop + COURSE_THUMBNAIL_SHOWN_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(COURSE_THUMBNAIL_WIDTH, COURSE_THUMBNAIL_SHOWN_WIDTH),
            TexelRunStep(COURSE_THUMBNAIL_STRIP_HEIGHT, COURSE_THUMBNAIL_SHOWN_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}
