#include <string.h>
#include <ultra64.h>

// The original passes DrawCharacterFace's draw mode here as s32; see player_stats.h.
#define WIDENS_FACE_DRAW_MODE

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
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "menu_arrows.h"
#include "menu_memory.h"
#include "menu_number.h"
#include "play_record.h"
#include "player.h"
#include "player_stats.h"
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

#define COURSE_THUMBNAIL_WIDTH 64
#define COURSE_THUMBNAIL_STRIP_HEIGHT 32
#define COURSE_THUMBNAIL_STRIP_COUNT 2
#define COURSE_THUMBNAIL_LEFT 40
#define COURSE_THUMBNAIL_TOP 182
#define COURSE_THUMBNAIL_SHOWN_WIDTH 52
#define COURSE_THUMBNAIL_SHOWN_HEIGHT 18
#define ASSET_FIRST_GAME_BOARD 0x82A
#define ASSET_SECOND_GAME_BOARD 0x82B
#define ASSET_THIRD_GAME_BOARD 0x82C

enum SaveMenuGame {
    SAVE_MENU_GAME_TOURNAMENT,
    SAVE_MENU_GAME_GET_CHARACTER,
    SAVE_MENU_GAME_STROKE
};

#define SAVE_MENU_SLOT_COUNT 5
#define COURSE_IMAGE_BUFFER_COUNT 2

enum {
    COURSE_IMAGE_BUFFER_FIRST,
    COURSE_IMAGE_BUFFER_SECOND
};

enum {
    SAVED_GAME_FIRST,
    SAVED_GAME_SECOND,
    SAVED_GAME_THIRD
};

#define SAVE_MENU_IMAGE_COUNT 6
#define ASSET_MOVIE_CAMERA_ICON 0x944
#define SAVE_ICON_WIDTH 64
#define SAVE_ICON_STRIP_HEIGHT 32
#define SAVE_ICON_STRIP_COUNT 2
#define SAVE_ICON_SHOWN_WIDTH 32
#define SAVE_ICON_SHOWN_STRIP_HEIGHT 12

enum SaveMenuMode {
    NO_SAVE_MENU_MODE = -1,
    SAVE_MENU_MODE_TOURNAMENT,
    SAVE_MENU_MODE_GET_CHARACTER,
    SAVE_MENU_MODE_STROKE,
    SAVE_MENU_MODE_SPEED_GOLF,
    SAVE_MENU_MODE_MATCH_GAME,
    SAVE_MENU_MODE_CLUB_SLOTS,
    SAVE_MENU_MODE_RING_SHOT,
    SAVE_MENU_MODE_TRAINING,
    SAVE_MENU_MODE_SKINS_MATCH,
    SAVE_MENU_MODE_UNLISTED_STROKE,
    // A saved game is mini golf by its isMiniGolf flag, not by its game mode,
    // so the modes before this one are those with a game mode.
    SAVE_MENU_MODE_MINI_GOLF,
    SAVE_MENU_MODE_COUNT
};

typedef s32 SaveMenuMode;
typedef s8 SaveMenuMode8;

extern const char f_SaveMenuNameFormat[];

void ResetSavedGameControllers(void);

#define SAVE_MENU_CHOSEN_IMAGE_COUNT 2
#define ASSET_SAVE_MENU_FRAME 0x70D
#define COURSE_WORD_OFFSET_X (-5)
#define COURSE_DIGIT_OFFSET_X 33

#define RING_SHOT_TEXT_SLOT_COUNT 6
#define RING_SHOT_REPLAY_ROW RING_SHOT_COURSE_COUNT
#define RING_SHOT_ROW_COUNT (RING_SHOT_REPLAY_ROW + 1)
#define RING_SHOT_LAST_FIRST_CHARACTER 14

#define NO_CONTROLLER (-1)
#define COMPUTER_CONTROLLER MAXCONTROLLERS

s16 FindNextReplaySlot(s8 startSlot, UBoolean8 isForward);
s16 GetChosenReplaySlot(void);
s32 GetOpenRingShotCourseCount(void);
void DrawCourseLabel(
    Gfx **displayListHead,
    CourseIndex courseIndex,
    s32 left,
    s32 top,
    UBoolean8 showsCourseWord
);
void DrawRingShotRowIcon(Gfx **displayListHead, s32 imageIndex, s32 left, s32 top);

static s32 f_RingShotArrowFrame = 0;
static s32 f_RingShotArrowTimer = 0;
static s32 f_UnusedRingShotArrowWord = 0;
static s32 f_UnusedWordAfterArrowTimer UNUSED = 0;
static UCharacterIndex8 f_RingShotCharacterOrder[CHARACTER_COUNT] = {
    CHARACTER_PLUM,
    CHARACTER_CHARLIE,
    CHARACTER_PEACH,
    CHARACTER_BABY_MARIO,
    CHARACTER_LUIGI,
    CHARACTER_YOSHI,
    CHARACTER_SONNY,
    CHARACTER_WARIO,
    CHARACTER_HARRY,
    CHARACTER_MARIO,
    CHARACTER_MAPLE,
    CHARACTER_DONKEY_KONG,
    CHARACTER_BOWSER,
    CHARACTER_METAL_MARIO,
    CHARACTER_KID,
    CHARACTER_SHERRY,
    CHARACTER_AZALEA,
    CHARACTER_JOE,
};
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
static f32 f_ReplayHuePhase = 0.0f;
static s16 f_RingShotIconPositions[RING_SHOT_COURSE_COUNT][RING_SHOT_ROW_COUNT] = {
    {164, 0, 0, 0, 0, 0, 122},
    {102, 144, 0, 0, 0, 0, 186},
    {78, 120, 162, 0, 0, 0, 204},
    {60, 102, 144, 186, 0, 0, 226},
    {42, 84, 126, 168, 210, 0, 252},
    {22, 60, 102, 144, 186, 228, 268},
};
static s32 f_UnusedWordsAfterIconPositions[2] UNUSED = {0, 0};
static s16 f_SavedPlayerControllers[PLAYER_COUNT_MAX] =
    {NO_CONTROLLER, NO_CONTROLLER, NO_CONTROLLER, NO_CONTROLLER};
static s16 f_AssignedHumanCount = 0;
static s16 f_HumanPlayerCount = PLAYER_COUNT_MAX;
static UBoolean8 f_IsGameChosen = FALSE;
static UBoolean8 f_AreControllersAssigned = FALSE;
static u8 f_ShownCourseBufferIndex = 0;
static SaveMenuMode f_SavedGameModes[SAVED_GAME_COUNT] =
    {NO_SAVE_MENU_MODE, NO_SAVE_MENU_MODE, NO_SAVE_MENU_MODE};
static WideAssetId f_ModeImageAssets[SAVE_MENU_MODE_COUNT] =
    {0x0623, 0x0629, 0x0599, 0x05AF, 0x0631, 0x0665, 0x05B7, 0x05DD, 0x065E, 0x0599, 0x06FC};
static u32 f_SaveMenuGameModes[SAVE_MENU_MODE_MINI_GOLF] = {
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
static s32 f_SaveMenuModeIcons[SAVE_MENU_MODE_COUNT] = {
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
static s32 f_UnusedWordsAfterModeIcons[4] UNUSED = {0, 0, 0, 0};
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
static f32 f_BoardBobPhase = 0.0f;
static s32 f_UnusedSaveMenuIconPositions[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX][2] UNUSED = {
    {{42, 140}, {76, 120}, {206, 120}, {248, 140}},
    {{42, 140}, {248, 140}, {206, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
};
static s32 f_UnusedSaveMenuModeIconValues[11] UNUSED =
    {110, 109, 117, 97, 106, 105, 105, 97, 84, 124, 99};
static char f_UnusedHoleLabel[] UNUSED = "Hole";
static s32 f_SavedGameModeLabelOffsets[MODE_ICON_COUNT] =
    {85, 85, 41, 60, 55, 60, 75, 66, 59, 0, 61};
static f32 f_SavedGolferAngles[3] = {0.0f, 0.0f, 0.0f};
static s32 f_UnusedWordAfterSavedGolferAngles UNUSED = 0;
static Vec3 f_SavedGolferPositions[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX] = {
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
static s32 f_SavedGameControllerIconPositions[PLAYER_COUNT_MAX][PLAYER_COUNT_MAX][2] = {
    {{42, 140}, {76, 120}, {206, 120}, {248, 140}},
    {{42, 140}, {248, 140}, {206, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
    {{42, 140}, {76, 120}, {211, 120}, {248, 140}},
};
static s32 f_UnusedWordsAfterControllerIcons[2] UNUSED = {0, 0};

static ImageAsset *f_RingShotNamePlate;
static ImageAsset *f_RingShotRainbowBox;
static ImageAsset *f_RingShotCourseNumbers;
static ImageAsset *f_RingShotGreenArrows;
static ImageAsset **f_RingShotCourseIcons;
static s16 f_RecordedReplaySlots[SAVE_MENU_SLOT_COUNT];

static u8 f_UnusedBufferBeforeBoardCamera[0x3D0] UNUSED;

static Mat4 f_BoardProjection;
static Mat4 f_BoardViewing;
static Mtx f_SavedGolferTransforms[PLAYER_COUNT_MAX][DRAW_BUFFER_COUNT];
static Mtx f_SavedGolfersProjection;
static f32 f_SavedGolferScales[PLAYER_COUNT_MAX];
static LookAt f_SavedGolfersLookAts[DRAW_BUFFER_COUNT];
static ImageAsset **f_SavedGameModeImages;
static ImageAsset **f_ChosenModeIconBuffers;
static ImageAsset *f_SavedGamesNamePlate;
static ImageAsset *f_GameModeLabels;
static ImageAsset *f_ControllerIcons;
static ImageAsset *f_PlayerLabels;

static u8 f_UnusedBytesAfterPlayerLabels[0x4] UNUSED;

static ImageAsset *f_CourseImageBuffers[COURSE_IMAGE_BUFFER_COUNT];

static u8 f_UnusedBytesAfterCourseImages[0x4] UNUSED;

static ImageAsset *f_GameBoards[SAVED_GAME_COUNT];
static s16 f_HumanPlayerIndices[PLAYER_COUNT_MAX];
static s32 f_ChosenModeIconScale;
static s32 f_ChosenModeIconHalfSize;

static u8 f_UnusedBytesAfterModeIconSize[0x4] UNUSED;
static u8 f_UnusedBufferAfterModeIconSize[0x88] UNUSED;

PaletteColor g_MenuCharacterPalettes[CHARACTER_COUNT][LARGE_PALETTE_COLOR_COUNT];
PaletteColor g_ReplayFacePalettes[SAVE_MENU_SLOT_COUNT][LARGE_PALETTE_COLOR_COUNT];

void LoadRingShotRecordBuffer(void *buffer, RingShotRecordBuffer bufferKind) {
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

void LoadRingShotCourseIcons(ImageAsset **buffers) {
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
#define MESSAGE_FIRST_RING_SHOT_CLEARERS 0x1E
#define RING_SHOT_REPLAY_TEXT_X 124
#define RING_SHOT_REPLAY_TEXT_Y 84
#define RING_SHOT_REPLAY_TEXT_FONT 2
#define REPLAY_HUE_STEP DegreesToRadians(5)
#define REPLAY_HUE_MIDDLE 127
#define REPLAY_HUE_SWING 127.0f
#define REPLAY_GLOW_DIVISOR 4

void DrawRingShotRecords(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s16 index;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s32 openCourseCount = GetOpenRingShotCourseCount();
    s16 row;
    s16 column;
    s16 itemIndex;
    s16 listIndex;
    s16 drop;
    CharacterIndex characterIndex;
    CharacterIndex8 replayCharacter;
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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_RingShotNamePlate)));
    for (index = 0; index != RING_SHOT_STRIP_COUNT; index++) {
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
    }

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
                IN_OWN_BLOCK({
                    characterIndex = f_RingShotCharacterOrder[listIndex];
                    index = FACE_PLAIN;
                    if (save->ringShotClearCounts[row][column][characterIndex] == 0) {
                        index = FACE_DARKENED;
                    }

                    DrawCharacterFace(
                        &gfx,
                        g_CharacterFaceImages[characterIndex],
                        g_MenuCharacterPalettes[characterIndex],
                        RING_SHOT_FACE_X + (column * RING_SHOT_COLUMN_SPACING),
                        RING_SHOT_FACE_Y + (itemIndex * RING_SHOT_FACE_SPACING),
                        characterIndex,
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
                });
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
                for (listIndex = 0; listIndex != RING_SHOT_EMPTY_DASH_COUNT; listIndex++) {
                    DrawMenuSign(
                        &gfx,
                        RING_SHOT_EMPTY_DASH_X + (listIndex * RING_SHOT_EMPTY_DASH_SPACING),
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
        MESSAGE_FIRST_RING_SHOT_CLEARERS + memory->ringShotRow,
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

static const char f_UnreferencedRodata[8] = {0};

s32 IsRingShotRecorded(RingShotRecord *record);

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

void LoadRingShotRecordFaces(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    s32 index;
    CharacterIndex characterIndex;
    s32 paletteIndex;

    for (index = 0; index != CHARACTER_COUNT; index++) {
        OpenEncodedAsset(g_CharacterFaceAssets[index], &asset);
        ReadEncodedAsset(g_CharacterFaceImages[index], &asset);
        OpenAssetFile(g_CharacterPaletteFiles[index][0], &file);
        ReadAssetFile(g_MenuCharacterPalettes[index], GetAssetFileSize(&file), &file);
    }

    for (index = 0; index != SAVE_MENU_SLOT_COUNT; index++) {
        if (f_RecordedReplaySlots[index] != -1) {
            characterIndex = save->ringShotRecords[f_RecordedReplaySlots[index]].characterIndex;
            paletteIndex = save->ringShotRecords[f_RecordedReplaySlots[index]].paletteIndex;
            if (characterIndex < 0) {
                characterIndex = 0;
            }

            if (characterIndex >= CHARACTER_COUNT) {
                characterIndex = CHARACTER_COUNT - 1;
            }

            if (paletteIndex < 0) {
                paletteIndex = 0;
            }

            if (paletteIndex >= CHARACTER_PALETTE_COUNT) {
                paletteIndex = CHARACTER_PALETTE_COUNT - 1;
            }
        }
        else {
            paletteIndex = 0;
            characterIndex = 0;
        }

        OpenAssetFile(g_CharacterPaletteFiles[characterIndex][paletteIndex], &file);
        ReadAssetFile(g_ReplayFacePalettes[index], GetAssetFileSize(&file), &file);
    }
}

s16 FindNextReplaySlot(s8 startSlot, UBoolean8 isForward) {
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

s16 GetChosenReplaySlot(void) {
    return f_RecordedReplaySlots[GetMenuMemory()->ringShotReplaySlot];
}

s32 IsRingShotRecorded(RingShotRecord *record) {
    return (record->scoreKind & SCORE_KIND_RECORDED) != 0;
}

void DrawCourseLabel(
    Gfx **displayListHead,
    CourseIndex courseIndex,
    s32 left,
    s32 top,
    UBoolean8 showsCourseWord
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_RingShotCourseNumbers)));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(f_RingShotCourseNumbers)),
        G_IM_FMT_CI,
        COURSE_NUMBERS_WIDTH,
        COURSE_NUMBERS_HEIGHT,
        0,
        0,
        COURSE_NUMBERS_WIDTH - 1,
        COURSE_NUMBERS_HEIGHT - 1,
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
            QuarterPixels(top + COURSE_NUMBERS_HEIGHT),
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
        QuarterPixels(top + COURSE_NUMBERS_HEIGHT),
        G_TX_RENDERTILE,
        TexelCoordinate(courseIndex * COURSE_DIGIT_WIDTH),
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_RingShotCourseIcons[imageIndex])));
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

    if (starCount >= RING_SHOT_STARS_PER_OPENED_COURSE * 5) {
        return 6;
    }

    if (starCount >= RING_SHOT_STARS_PER_OPENED_COURSE * 4) {
        return 5;
    }

    if (starCount >= RING_SHOT_STARS_PER_OPENED_COURSE * 3) {
        return 4;
    }

    if (starCount >= RING_SHOT_STARS_PER_OPENED_COURSE * 2) {
        return 3;
    }

    if (starCount >= RING_SHOT_STARS_PER_OPENED_COURSE) {
        return 2;
    }

    return 1;
}

void LoadSaveGameBuffer(void *buffer, SaveGameBuffer bufferKind) {
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

void ReadSavedGames(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s8 gameIndex;
    SaveMenuMode8 mode;

    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        f_SavedGameModes[gameIndex] = NO_SAVE_MENU_MODE;
    }

    f_AreControllersAssigned = FALSE;
    InitSram();
    TransferSaveData(save, FALSE);
    for (gameIndex = 0; gameIndex != SAVED_GAME_COUNT; gameIndex++) {
        if (save->savedGames[gameIndex].isValid) {
            if (save->savedGames[gameIndex].state.isMiniGolf) {
                f_SavedGameModes[gameIndex] = SAVE_MENU_MODE_MINI_GOLF;
            }
            else {
                for (mode = 0; mode != SAVE_MENU_MODE_MINI_GOLF; mode++) {
                    if (save->savedGames[gameIndex].state.gameMode == f_SaveMenuGameModes[mode]) {
                        f_SavedGameModes[gameIndex] = mode;
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

void ResetSavedGameControllers(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s8 playerIndex;
    s8 playerCount;
    s8 humanCount;
    s8 gameIndex;
    Player *savedPlayers;

    for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
        f_SavedPlayerControllers[playerIndex] = NO_CONTROLLER;
        f_HumanPlayerIndices[playerIndex] = -1;
    }

    f_AssignedHumanCount = 0;
    gameIndex = memory->savedGameIndex;
    savedPlayers = save->savedPlayers[gameIndex];
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
    f_IsGameChosen = FALSE;
    for (playerIndex = 0; playerIndex != MAXCONTROLLERS; playerIndex++) {
        g_IsMenuCharacterLoaded[playerIndex] = FALSE;
    }
}

void LoadSaveMenuBuffers(ImageAsset **buffers, SaveMenuBuffer bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = GetMenuMemory();
    s32 imageIndex;
    SaveMenuMode mode;

    switch (bufferKind) {
        case SAVE_MENU_BUFFER_SLOTS:
            f_SavedGameModeImages = buffers;
            for (imageIndex = 0; imageIndex != SAVED_GAME_COUNT; imageIndex++) {
                mode = f_SavedGameModes[imageIndex];
                if (mode == NO_SAVE_MENU_MODE) {
                    mode = SAVE_MENU_MODE_TOURNAMENT;
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
                    mode = SAVE_MENU_MODE_TOURNAMENT;
                }

                OpenEncodedAsset(f_ModeImageAssets[mode], &asset);
                ReadEncodedAsset(f_ChosenModeIconBuffers[imageIndex], &asset);
            }

            break;
    }
}

void LoadGameModeLabelBuffer(void *buffer, GameModeLabelBuffer bufferKind) {
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

#define SAVED_GAME_TEXT_SLOT_COUNT 4
#define SAVED_GAME_TURN_FRAMES 7

#define CHOSEN_GAME (save->savedGames[memory->savedGameIndex])
#define LoadChosenGameImages() \
    { \
        OpenEncodedAsset( \
            g_ModeIconAnimations[( \
                u8 \
            )f_SaveMenuModeIcons[f_SavedGameModes[memory->savedGameIndex]]][g_AnimatedIconFrame], \
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

void UpdateSavedGames(void) {
    MenuMemory *memory = GetMenuMemory();
    SaveData *save = GetSaveData();
    EncodedAsset asset;
    AssetFile file;
    u8 unusedStack[0xA0] UNUSED;
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
            for (i = 0; i != PLAYER_COUNT_MAX; i++) {
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
                g_AnimatedIconFrame = 0;
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
                g_AnimatedIconFrame = 0;
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

s32 IsChosenGameEmpty(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    SavedGame *game = &save->savedGames[memory->savedGameIndex];

    return -(game->isValid == FALSE);
}

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

s32 ContinueSavedGame(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    Player *savedPlayer;
    s32 playerIndex;
    CharacterIndex characterIndex;
    u16 controllerIndex;

    if (f_SavedGameModes[memory->savedGameIndex] == NO_SAVE_MENU_MODE) {
        return -1;
    }

    if (SaveOrContinueGame(memory->savedGameIndex, TRUE) == -1) {
        return -1;
    }

    savedPlayer = save->savedPlayers[memory->savedGameIndex];
    for (playerIndex = 0; playerIndex != PLAYER_COUNT_MAX; playerIndex++) {
        controllerIndex = f_SavedPlayerControllers[playerIndex];
        g_PlayerControllerIndices[playerIndex] = controllerIndex;
        g_Players[playerIndex].controllerIndex = controllerIndex;
        sprintf(g_Players[playerIndex].name, f_SaveMenuNameFormat, savedPlayer->name);
        characterIndex = savedPlayer->characterIndex;
        g_ChosenCharacters[playerIndex] = characterIndex;
        g_Players[playerIndex].characterIndex = characterIndex;
        LoadPlayerFace(playerIndex, characterIndex, g_Players[playerIndex].paletteIndex);
        savedPlayer++;
    }

    if (!IsGameFlagSet(GAME_FLAG_KEEP_SAVED_GAMES)) {
        memset(&save->savedGames[memory->savedGameIndex], 0, sizeof(SavedGame));
    }

    return 0;
}

const char f_SaveMenuNameFormat[] = "%s";

#define SAVED_GAMES_BACKDROP_LAYERS \
    (MENU_LAYER_COURSE_SCENE | MENU_LAYER_IGNORED | MENU_LAYER_BANNER | MENU_LAYER_LARGE_BANNER)

#define GAME_BOARD_WIDTH 64
#define GAME_BOARD_STRIP_HEIGHT 32
#define GAME_BOARD_STRIP_COUNT 2
#define MODE_ICON_OFFSET_X 8
#define MODE_ICON_OFFSET_Y 14

#define BOARD_TURN_STEP_DEGREES 15
#define MODE_ICON_HALF_SIZE 24.0f
#define MODE_ICON_DROP 4
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
#define SAVED_GAME_COURSE_X 110
#define SAVED_GAME_COURSE_Y 187
#define MESSAGE_FIRST_TOURNAMENT_NAME 0x6F
#define FIRST_PEACHS_CASTLE_MINI_GOLF_COURSE (MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD)
#define SAVED_GAME_HOLE_X 180
#define SAVED_GAME_HOLE_Y 203
#define CONTROLLER_PROMPT_X 70
#define CONTINUE_PROMPT_X 54
#define PROMPT_Y 184
#define MESSAGE_CONTROLLER_PROMPT 0x37
#define MESSAGE_CONTINUE_PROMPT 0x38
#define EMPTY_GAME_TEXT_X 100
#define EMPTY_GAME_TEXT_Y 186
#define MESSAGE_EMPTY_SAVED_GAME 0x26

void DrawChosenGameBoard(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale);
void DrawSavedGolfers(Gfx **displayListHead);
void DrawPlayerControllers(Gfx **displayListHead);
void DrawSavedCourseImage(Gfx **displayListHead);
void DrawSavedMiniGolfImage(Gfx **displayListHead);

void DrawSavedGames(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspectiveNormalization;
    f32 boardX[COURSE_COUNT];
    f32 boardY[COURSE_COUNT];
    f32 boardScale[COURSE_COUNT];
    s32 unusedSlots[COURSE_COUNT] UNUSED;
    s32 drawOrder[COURSE_COUNT];
    u8 unusedStack[8] UNUSED;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
    s32 boardIndex;
    s32 key;
    Radians angle;
    f32 circleX;
    s32 drawnIndex;
    s32 halfSize;
    s32 texelStep;
    s32 iconHalfSize;
    s32 iconStep;
    f32 bobOffset;
    s32 stripLeft;
    s32 stripShownWidth;
    s32 gameIndex;
    s32 index;
    s32 labelX;
    s32 iconIndex;
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
        &perspectiveNormalization,
        WHEEL_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        WHEEL_NEAR_PLANE,
        WHEEL_FAR_PLANE,
        1.0f
    );
    guLookAtF(
        f_BoardViewing,
        0.0f,
        CAROUSEL_EYE_HEIGHT,
        CAROUSEL_EYE_DISTANCE,
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

    for (boardIndex = 0; boardIndex != SAVED_GAME_COUNT; boardIndex++) {
        ScreenPoint point;
        f32 radiusX = CAROUSEL_RADIUS_X;
        s32 boardDegrees;

        slotOffset = (memory->savedGameIndex - boardIndex) % SAVED_GAME_COUNT;
        boardDegrees = DEGREES_PER_TURN / SAVED_GAME_COUNT;
        angle = -DegreesToRadians(
            (slotOffset * boardDegrees) + (memory->savedGameTurn * BOARD_TURN_STEP_DEGREES)
        );
        circleX = sinf(angle) * radiusX;
        if (ProjectPoint(circleX, 0.0f, cosf(angle) * CAROUSEL_RADIUS_Z, &point) < 0) {
            boardX[boardIndex] = point.x;
            boardY[boardIndex] = point.y - WHEEL_RAISE;
            boardScale[boardIndex] = point.scaleX;
        }
    }

    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        drawOrder[index] = index;
    }

    for (index = 1; index != SAVED_GAME_COUNT; index++) {
        key = drawOrder[index];
        for (boardIndex = index - 1; boardIndex >= 0 && boardY[drawOrder[boardIndex]] > boardY[key];
            boardIndex--) {
            drawOrder[boardIndex + 1] = drawOrder[boardIndex];
        }

        drawOrder[boardIndex + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);

    f_BoardBobPhase += CAROUSEL_BOB_PHASE_STEP;
    for (boardIndex = 0; boardIndex != SAVED_GAME_COUNT; boardIndex++) {
        drawnIndex = drawOrder[boardIndex];
        halfSize = boardScale[drawnIndex] * CAROUSEL_BOARD_HALF_SIZE;
        texelStep = (f32)(GAME_BOARD_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
        f_ChosenModeIconHalfSize = boardScale[drawnIndex] * MODE_ICON_HALF_SIZE;
        f_ChosenModeIconScale =
            (f32)(MODE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / f_ChosenModeIconHalfSize;
        iconHalfSize = f_ChosenModeIconHalfSize;
        iconStep = f_ChosenModeIconScale;
        if (boardIndex == SAVED_GAME_COUNT) {
            bobOffset = sinf(f_BoardBobPhase) * CAROUSEL_BOB_HEIGHT;
        }
        else {
            bobOffset = 0.0f;
        }

        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        if ((memory->savedGameIndex != drawnIndex) || (memory->savedGameTurn != 0)) {
            if (memory->savedGameIndex == drawnIndex) {
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
                    UNCHOSEN_CAROUSEL_BOARD_SHADE,
                    UNCHOSEN_CAROUSEL_BOARD_SHADE,
                    UNCHOSEN_CAROUSEL_BOARD_SHADE,
                    OPAQUE_ALPHA
                );
            }

            gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_GameBoards[drawnIndex])));
            for (index = 0; index != GAME_BOARD_STRIP_COUNT; index++) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(
                        LargeImageTexels(f_GameBoards[drawnIndex]) +
                        (index * GAME_BOARD_WIDTH * GAME_BOARD_STRIP_HEIGHT)
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
                    QuarterPixels((s32)(boardX[drawnIndex] - halfSize)),
                    QuarterPixels((s32)(boardY[drawnIndex] - halfSize + (index * halfSize) +
                        bobOffset)),
                    QuarterPixels((s32)(boardX[drawnIndex] + halfSize)),
                    QuarterPixels((s32)(boardY[drawnIndex] + (index * halfSize) + bobOffset)),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    texelStep,
                    texelStep
                );
                gDPPipeSync(gfx++);
            }

            if (f_SavedGameModes[drawnIndex] != NO_SAVE_MENU_MODE) {
                ImageAsset *modeImage = f_SavedGameModeImages[drawnIndex];

                gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(modeImage)));
                for (index = 0; index != MODE_ICON_STRIP_COUNT; index++) {
                    gDPLoadTextureBlock(
                        gfx++,
                        TextureAddress(
                            modeImage + LARGE_PALETTED_TEXELS_OFFSET +
                            (index * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT)
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
                        QuarterPixels((s32)(boardX[drawnIndex] - iconHalfSize)),
                        QuarterPixels(
                            (s32)(boardY[drawnIndex] - iconHalfSize + (index * iconHalfSize) +
                                bobOffset) +
                            MODE_ICON_DROP
                        ),
                        QuarterPixels((s32)(boardX[drawnIndex] + iconHalfSize)),
                        QuarterPixels(
                            (s32)(boardY[drawnIndex] + (index * iconHalfSize) + bobOffset) +
                            MODE_ICON_DROP
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
                boardX[drawnIndex] - halfSize,
                boardY[drawnIndex] - halfSize + bobOffset,
                halfSize,
                texelStep
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

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_SavedGamesNamePlate)));
    for (index = 0; index != SAVED_GAME_STRIP_COUNT; index++) {
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
                (index * SAVED_GAME_STRIP_WIDTH * SAVED_GAME_STRIP_HEIGHT)
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
            QuarterPixels(SAVED_GAME_STRIP_TOP + (index * SAVED_GAME_STRIP_HEIGHT)),
            QuarterPixels(stripLeft + stripShownWidth),
            QuarterPixels(
                SAVED_GAME_STRIP_TOP + SAVED_GAME_STRIP_HEIGHT + (index * SAVED_GAME_STRIP_HEIGHT)
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
    index = memory->savedGameIndex;
    if (f_SavedGameModes[index] != NO_SAVE_MENU_MODE) {
        labelX = (playerCount == 1) ? SINGLE_PLAYER_MODE_LABEL_X : MODE_LABEL_X;
        iconIndex = f_SaveMenuModeIcons[f_SavedGameModes[index]];
        gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_GameModeLabels)));
        gDPLoadTextureBlock_4b(
            gfx++,
            TextureAddress(
                ImageTexels(f_GameModeLabels) +
                (f_SaveMenuModeIcons[f_SavedGameModes[index]] * MODE_LABEL_IMAGE_SIZE)
            ),
            G_IM_FMT_CI,
            MODE_LABEL_WIDTH,
            MODE_LABEL_HEIGHT,
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
            QuarterPixels(labelX - f_SavedGameModeLabelOffsets[iconIndex]),
            QuarterPixels(MODE_LABEL_TOP),
            QuarterPixels(labelX - f_SavedGameModeLabelOffsets[iconIndex] + MODE_LABEL_WIDTH),
            QuarterPixels(MODE_LABEL_TOP + MODE_LABEL_HEIGHT),
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
            index = CHOSEN_GAME.state.courseIndex;
            boardIndex = CHOSEN_GAME.state.holeIndex;
            if (CHOSEN_GAME.state.gameMode == GAME_MODE_TOURNAMENT) {
                SetTextSlot(
                    SAVED_GAME_COURSE_X,
                    SAVED_GAME_COURSE_Y,
                    MESSAGE_FIRST_TOURNAMENT_NAME + index,
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
                    index = (index >= FIRST_PEACHS_CASTLE_MINI_GOLF_COURSE) ?
                        COURSE_RECORD_PEACHS_CASTLE :
                        COURSE_RECORD_LUIGIS_GARDEN;
                }

                sprintf(g_PrintBuffer, f_SaveMenuNameFormat, g_CourseRecords[index].name);
                DrawLargeText(&gfx, SAVED_GAME_COURSE_X, SAVED_GAME_COURSE_Y, g_PrintBuffer);
            }

            SetTextSlotValue(boardIndex + 1, 1, 0);
            SetTextSlot(
                SAVED_GAME_HOLE_X,
                SAVED_GAME_HOLE_Y,
                MESSAGE_HOLE_NUMBER,
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
            if (f_AreControllersAssigned) {
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
            else {
                SetTextSlotValue(f_HumanPlayerIndices[f_AssignedHumanCount] + 1, 0, 0);

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
                for (index = 1; index != SAVED_GAME_TEXT_SLOT_COUNT; index++) {
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

void DrawChosenGameBoard(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
    MenuMemory *memory = GetMenuMemory();
    Gfx *gfx = *displayListHead;
    s32 iconHalfSize = f_ChosenModeIconHalfSize;
    s32 iconScale = f_ChosenModeIconScale;
    EncodedAsset asset;
    s32 stripIndex;
    ImageAsset *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_GameBoards[memory->savedGameIndex])));
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
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));
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
            g_AnimatedIconFrame++;
            if (g_AnimatedIconFrame >= MODE_ICON_FRAME_COUNT) {
                g_AnimatedIconFrame = 0;
            }

            if (g_ModeIconAnimations[iconIndex][g_AnimatedIconFrame] == END_OF_ICON_FRAMES) {
                g_AnimatedIconFrame = 0;
            }

            g_AnimatedIconBufferIndex = g_AnimatedIconBufferIndex == 0;
            OpenEncodedAsset(g_ModeIconAnimations[iconIndex][g_AnimatedIconFrame], &asset);
            ReadEncodedAsset(f_ChosenModeIconBuffers[g_AnimatedIconBufferIndex], &asset);
        }
    }

    *displayListHead = gfx;
}

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

void DrawPlayerControllers(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 playerCount = save->savedGames[memory->savedGameIndex].state.playerCount;
    s32 position[2];
    s32 playerIndex;
    s32 labelIndex;
    s16 controllerIndex;
    ImageAsset *image;

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
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(image)));
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        if (f_SavedPlayerControllers[playerIndex] != NO_CONTROLLER) {
            position[AXIS_X] = f_SavedGameControllerIconPositions[playerCount - 1][playerIndex][0];
            position[AXIS_Y] = f_SavedGameControllerIconPositions[playerCount - 1][playerIndex][1];
            controllerIndex = f_SavedPlayerControllers[playerIndex];
            if (controllerIndex != COMPUTER_CONTROLLER) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress(
                        LargeImageTexels(image) + (controllerIndex * CONTROLLER_ICON_IMAGE_SIZE)
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
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(image)));
    for (playerIndex = 0; playerIndex != playerCount; playerIndex++) {
        position[AXIS_X] = f_SavedGameControllerIconPositions[playerCount - 1][playerIndex][0];
        position[AXIS_Y] = f_SavedGameControllerIconPositions[playerCount - 1][playerIndex][1];
        if (f_SavedPlayerControllers[playerIndex] == COMPUTER_CONTROLLER) {
            if (save->savedGames[memory->savedGameIndex].state.gameMode ==
                GAME_MODE_GET_CHARACTER) {
                labelIndex = PLAYER_LABEL_COMPUTER;
            }
            else {
                labelIndex = playerIndex + PLAYER_LABEL_FIRST_COMPUTER;
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

void DrawSavedCourseImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    GetSaveData();
    GetMenuMemory();
    gDPLoadTLUT_pal256(
        gfx++,
        TextureAddress(ImagePalette(f_CourseImageBuffers[f_ShownCourseBufferIndex]))
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

void DrawSavedMiniGolfImage(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;
    PaletteColor *palette;

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
