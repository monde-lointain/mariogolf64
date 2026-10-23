#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "common.h"
#include "controller.h"
#include "course.h"
#include "heap.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "variables.h"

/*
 * The pause screens kept in this overlay: the save menu, which lists either
 * the three saved games or the five replay records beside each one's
 * mode icon, and the Controls screen, a picture of the controller.
 */

/* What OpenSaveMenu lists, and sMenuKind */
#define SAVE_MENU_CLOSED 0
#define SAVE_MENU_GAMES 1
#define SAVE_MENU_REPLAYS 2

/* sMenuState values */
#define SAVE_MENU_STATE_OPENING 0     /* The rows slide in */
#define SAVE_MENU_STATE_CHOOSING 1
#define SAVE_MENU_STATE_BACKING_OUT 2 /* The rows slide out */
#define SAVE_MENU_STATE_BACKED_OUT 3
#define SAVE_MENU_STATE_SAVED 4       /* Waits before leaving */
#define SAVE_MENU_STATE_LEAVING 5     /* The rows slide out, and the menu closes */

/* Indices of the game-mode icon animations in D_800C229C */
#define MODE_ICON_TOURNAMENT 0
#define MODE_ICON_GET_CHARACTER 1
#define MODE_ICON_STROKE 2
#define MODE_ICON_MINI_GOLF 5
#define MODE_ICON_MATCH_GAME 6
#define MODE_ICON_SKINS_MATCH 7
#define MODE_ICON_CLUB_SLOTS 10
#define NO_MODE_ICON -1
#define MODE_ICON_COUNT 11
/* Each icon's frames are asset ids, ended by END_OF_ICON_FRAMES */
#define MODE_ICON_FRAME_COUNT 27
#define END_OF_ICON_FRAMES -1
/* The frames that each frame of the chosen icon shows for */
#define ICON_FRAME_PERIOD 5

/* The game modes that sModeIcons covers */
#define GAME_MODE_ICON_COUNT 12
#define MODE_NAME_COUNT 7

/* Asset ids of the Controls screen's picture */
#define CONTROLS_PICTURE 0x92A
#define MINI_GOLF_CONTROLS_PICTURE 0x96A
#define CONTROLS_PICTURE_WIDTH 288
#define CONTROLS_PICTURE_LEFT ((SCREEN_WIDTH - CONTROLS_PICTURE_WIDTH) / 2)
/* The picture is loaded into texture memory a few rows at a time */
#define CONTROLS_STRIP_HEIGHT 7
#define CONTROLS_STRIP_COUNT 32
#define CONTROLS_SONG 0x17

#define SOUND_CONTROLS_OPEN 0x72
#define SOUND_CONTROLS_CLOSE 0x73
#define SOUND_MENU_CONFIRM 0x5D
#define SOUND_MENU_MOVE 0x5C
#define SOUND_SAVE_MENU_OPEN 0x47
#define SOUND_SAVE_MENU_CLOSE 0x48
#define SOUND_SAVED 0x3B
#define MENU_SOUND_CHANNEL 15
#define MENU_SOUND_VOLUME 0x64
#define MENU_SOUND_REVERB 0x14
#define CONTROLS_SOUND_PAN 0x7F

/* The frames that a slide takes, and the frame of its sound */
#define SLIDE_FRAMES 14
#define SLIDE_SOUND_FRAME 3
/* Rows slide out slowly until past this, then speed up */
#define SLIDE_OUT_START_OFFSET -1
#define SLIDE_OUT_START_STEP 4

/* The replay records keep their character's image when this bit of unk36 is set */
#define RECORD_IN_USE 0x80
/* Added to a character's D_800C547C entry for the image shown in the menu */
#define CHARACTER_IMAGE_OFFSET 9
/* A character image for each player of each saved game, or for each replay record */
#define CHARACTER_IMAGE_COUNT (SAVED_GAME_COUNT * MAX_PLAYER_COUNT)
/* Albatross, Hole in One, Eagle, Birdie, Par and Chip In */
#define REPLAY_LABEL_COUNT 6

#define FULL_SONG_VOLUME 0x7F
/* How loud the song plays behind the saved games */
#define SAVE_MENU_SONG_VOLUME 0x60
#define SAVE_MENU_SOUND_PAN 0xFF

#define SAVE_MENU_PANEL_IMAGE 0x521
#define SAVE_MENU_TITLE_IMAGE 0x6A5
#define COURSE_NUMBERS_IMAGE 0x70E
/* Where func_800719A0 is told to put the cleared messages */
#define MESSAGE_CENTER_X 160
#define MESSAGE_CENTER_Y 114
/* Given this as its text, func_800719A0 clears the slot */
#define NO_TEXT -1

/* D_800C306C's value while no pause screen is shown */
#define PAUSE_SCREEN_NONE 0
#define PAUSE_SCREEN_UNK6 6

/* The prompts that func_8006BA24 opens */
#define PROMPT_OVERWRITE_GAME 0
#define PROMPT_END_GAME 1
#define PROMPT_OVERWRITE_REPLAY 2
#define PROMPT_SKIP_REPLAY 5

/* What UpdateReplaySaveMenu returns */
#define REPLAY_MENU_OPEN 0
#define REPLAY_MENU_CLOSED 1
#define REPLAY_MENU_PROMPTING 2

/* The rows below the saved games and the replay records */
#define END_GAME_ROW SAVED_GAME_COUNT
#define SKIP_REPLAY_ROW SAVE_RECORD_COUNT
#define SLIDE_ROW_COUNT 4
/* The frames that the menu waits after saving before it slides out */
#define SAVED_WAIT_FRAMES 15

/* The game menu's rows: the saved games, then End Game */
#define GAME_ROW_COUNT (SAVED_GAME_COUNT + 1)
#define FIRST_GAME_ROW_TOP 50
#define GAME_ROW_SPACING 47
#define GAME_PANEL_STRIP_HEIGHT 15
/*
 * The panels' middles, less their rows' offsets; each panel reaches half
 * its height above and below its middle
 */
#define GAME_PANEL_MIDDLE 72
#define END_GAME_PANEL_MIDDLE 64
#define END_GAME_PANEL_STRIP_HEIGHT 9
/*
 * The title image is drawn in strips that each overlap the next by two
 * rows, as it sways in a figure eight around its resting place
 */
#define TITLE_IMAGE_WIDTH 160
#define TITLE_STRIP_COUNT 3
#define TITLE_STRIP_HEIGHT 10
#define TITLE_STRIP_SIZE (TITLE_IMAGE_WIDTH * TITLE_STRIP_HEIGHT)
#define TITLE_STRIP_DRAWN_HEIGHT 12
#define TITLE_LAST_STRIP_HEIGHT 7
#define TITLE_IMAGE_LEFT 70
#define TITLE_SWAY_LEFT 10.0f
#define TITLE_SWAY_TOP 18.0f
#define TITLE_SWAY_DISTANCE 5.0f
/* Texture coordinates have five fraction bits, so a texel is 32 */
#define TEXEL_COORDINATE_ONE 32
#define HALF_TEXEL_COORDINATE 16.0f
#define SWAY_DEGREES_PER_FRAME 10
/* The mode icons are drawn at half size, in two halves */
#define MODE_ICON_SIZE 64
#define MODE_ICON_PART_HEIGHT 32
#define MODE_ICON_PART_COUNT 2
#define MODE_ICON_LEFT 26
#define MODE_ICON_TOP 54
#define HALF_SIZE_TEXEL_STEP (2 << 10)
#define GOLFERS_LEFT 240
#define GOLFER_SPACING 26
#define GOLFER_TOP 52
#define MODE_TEXT_LEFT 62
#define MODE_TEXT_TOP 57
#define COURSE_TEXT_TOP 74
#define GAME_HOLE_TEXT_LEFT 240
#define GAME_HOLE_TEXT_TOP 78
#define EMPTY_GAME_TEXT_TOP 64
#define END_GAME_TEXT_LEFT 116
#define END_GAME_TEXT_TOP 199
/* The cursor's frame pulses between these alphas */
#define CURSOR_ALPHA_MIDDLE 127
#define CURSOR_ALPHA_SWING 64.0f
/* func_800738BC's font size for the course names */
#define COURSE_NAME_FONT_SIZE 12
/* Each mini-golf course record has three courses */
#define MINI_GOLF_GREEN 0
#define MINI_GOLF_SLOW 1
#define MINI_GOLF_FAST 2
#define LUIGIS_GARDEN_COURSE_INDEX MINI_GOLF_COURSE_INDEX
#define PEACHS_CASTLE_COURSE_INDEX (MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD)
/* Indices of sModeNames */
#define MODE_NAME_TOURNAMENT 0
#define MODE_NAME_GET_CHARACTER 1
#define MODE_NAME_STROKE 2
#define MODE_NAME_MATCH_GAME 3
#define MODE_NAME_SKINS_MATCH 4
#define MODE_NAME_CLUB_SLOTS 5
#define MODE_NAME_MINI_GOLF 6

/* The replay menu's rows: the records, then Hole Out */
#define REPLAY_ROW_COUNT (SAVE_RECORD_COUNT + 1)
#define FIRST_REPLAY_ROW_TOP 36
#define REPLAY_ROW_SPACING 34
/* The panel image is stretched over each row a strip at a time */
#define PANEL_IMAGE_WIDTH 200
#define PANEL_IMAGE_HEIGHT 30
#define PANEL_STRIP_COUNT 3
#define PANEL_STRIP_HEIGHT (PANEL_IMAGE_HEIGHT / PANEL_STRIP_COUNT)
#define PANEL_STRIP_SIZE (PANEL_IMAGE_WIDTH * PANEL_STRIP_HEIGHT)
#define WIDE_PANEL_LEFT 24
#define WIDE_PANEL_WIDTH 272
#define RECORD_PANEL_STRIP_HEIGHT 11
#define NARROW_PANEL_LEFT 80
#define NARROW_PANEL_WIDTH 160
#define HOLE_OUT_PANEL_STRIP_HEIGHT 8
#define TITLE_PANEL_LEFT 96
#define TITLE_PANEL_TOP 13
#define TITLE_PANEL_WIDTH 128
#define TITLE_PANEL_STRIP_HEIGHT 9
#define TITLE_TEXT_LEFT 120
#define TITLE_TEXT_TOP 18
#define TITLE_BOB_HEIGHT 4.0f
#define TITLE_BOB_DEGREES_PER_FRAME 10
/* The rows' shades; the chosen one is brighter */
#define CHOSEN_PANEL_SHADE 0xEF
#define OTHER_PANEL_SHADE 0xBF
#define CURSOR_FRAME_THICKNESS 2
/* The course numbers image holds the digits 1 to 6, then the word COURSE */
#define COURSE_NUMBERS_WIDTH 136
#define COURSE_NUMBERS_HEIGHT 16
#define COURSE_DIGIT_WIDTH 16
#define COURSE_DIGIT_COUNT 6
#define COURSE_WORD_TEXEL_LEFT (COURSE_DIGIT_COUNT * COURSE_DIGIT_WIDTH)
#define COURSE_WORD_WIDTH 40
#define COURSE_WORD_LEFT 64
#define COURSE_TOP 36
#define LABEL_LEFT 128
#define LABEL_TOP 38
#define CHARACTER_IMAGE_WIDTH 32
#define CHARACTER_IMAGE_HEIGHT 24
#define CHARACTER_LEFT 32
#define CHARACTER_TOP 40
#define HOLE_TEXT_LEFT 64
#define HOLE_TEXT_TOP 53
#define EMPTY_TEXT_TOP 47
#define HOLE_OUT_TEXT_LEFT 128
#define HOLE_OUT_TEXT_TOP 212
/* The text slots of func_800719A0 that the records' hole texts use */
#define FIRST_RECORD_TEXT_SLOT 2
/* The same hole texts that the course select shows for Ring Shot */
#define RING_SHOT_TEXT_FIRST_IMAGE 0x4B
#define RING_SHOT_HOLE_COUNT 6
/* An image header's width and height are halfwords; these are their low bytes */
#define IMAGE_WIDTH_LOW_BYTE 3
#define IMAGE_HEIGHT_LOW_BYTE 5

/* Unk68.unk36's low bits: the kind of shot the record replays */
#define RECORD_KIND_MASK 0xF
#define RECORD_KIND_HOLE_IN_ONE 0
#define RECORD_KIND_ALBATROSS 1
#define RECORD_KIND_EAGLE 2
#define RECORD_KIND_BIRDIE 3
#define RECORD_KIND_PAR 4
/* Indices of sReplayLabelImages */
#define REPLAY_LABEL_ALBATROSS 0
#define REPLAY_LABEL_HOLE_IN_ONE 1
#define REPLAY_LABEL_EAGLE 2
#define REPLAY_LABEL_BIRDIE 3
#define REPLAY_LABEL_PAR 4
#define NO_REPLAY_LABEL -1

/*
 * This overlay reads the characters and variants of saved players and
 * records as signed, though UnkB8 and Unk68 keep them unsigned; the casts
 * are there to match.
 */
extern s32 D_800C306C;
extern u16 D_800C547C[];
extern s16 D_800C54A0[];
extern Unk800BA9D8 D_80105F40;
extern s8 D_801061BD;
extern s8 D_801061BE;
extern s8 D_801061BF;
extern s8 D_801061C0;
extern s8 D_801061C1;
extern s8 D_801061C2;
extern s8 D_801061C3;
extern s8 D_801061C4;
extern s8 D_801061C5;

extern s32 sSlideOffsets[4];
extern void *sCharacterFiles[CHARACTER_IMAGE_COUNT];
extern void *sCharacterImages[CHARACTER_IMAGE_COUNT];
extern void *sPanelImage;
extern void *sModeIconImages[SAVED_GAME_COUNT][2];
extern void *sReplayLabelImages[REPLAY_LABEL_COUNT];
extern void *sCourseNumbersImage;
extern void *sSaveMenuTitleImage;
extern s32 sIconFrames[SAVED_GAME_COUNT];
extern s16 g_ModeIconAnimations[MODE_ICON_COUNT][MODE_ICON_FRAME_COUNT];

void func_8005B28C(s32 recordIndex, Unk68 *record);
void func_8005D0D8(void);
void func_800719A0(s32 x, s32 y, s32 image, s32, s32 slot, s32, s32, s32, s32, s32);
void InvalidateBrokenSavedGames(void);
s32 SaveOrContinueGame(s32 slotIndex, s32 isContinuing);
void func_800934CC(s32);
void func_80060358(void);
s32 func_8006BA10(void);
void func_8006BA24(s32 prompt);
void SetFramebufferCount(s32 count);
void CloseControlsScreen(void);
void func_8006A2C0(Gfx **displayListHead, s32 xl, s32 yl, s32 xh, s32 yh, s32 tile, s32 s, s32 t, s32 dsdx,
                   s32 dtdy);
void func_8006A4A0(Gfx **displayListHead, s32 palette, void *tlut);
void func_8006A548(Gfx **displayListHead, void *tlut);
void func_8006A5E4(Gfx **displayListHead, void *texels, s32 format, s32 width, s32 height, s32 palette, s32 cms,
                   s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);
void func_80071608(Gfx **displayListHead);
void func_80071CE4(Gfx **displayListHead, s32 slot);
void func_800734F0(Gfx **displayListHead, s32);
void func_800747B0(Gfx **displayListHead, s16 x, s16 y, char *text);
void func_8007624C(Gfx **displayListHead, s32 x, s32 y, char *text, s32, s32, s32 fontSize);
s32 func_800738BC(char *text, s32 fontSize);
void func_80073F24(Gfx **displayListHead, s16 x, s16 y, char *text);
void func_8006A84C(Gfx **displayListHead, void *texels, s32 format, s32 size, s32 width, s32 height, s32 palette,
                   s32 cms, s32 cmt, s32 masks, s32 maskt, s32 shifts, s32 shiftt);

/* The MODE_ICON_* index by GameState.gameMode; modes that the menu never names hold 0 or 3 */
s8 sModeIcons[GAME_MODE_ICON_COUNT] = {
    MODE_ICON_STROKE,      0, MODE_ICON_MATCH_GAME,  0, MODE_ICON_GET_CHARACTER, MODE_ICON_TOURNAMENT,
    MODE_ICON_MINI_GOLF,   3, 0,                     0, MODE_ICON_SKINS_MATCH,   MODE_ICON_CLUB_SLOTS,
};
s32 sMenuKind = SAVE_MENU_CLOSED;
s32 sIsControlsShown = FALSE;
s32 sCursorIndex = 0;
s32 sStateFrame = 0;
s32 sMenuState = SAVE_MENU_STATE_OPENING;
/* Indexed in the order of DrawGameSaveMenu's cases */
char *sModeNames[MODE_NAME_COUNT] = {
    "Tournament", "Get Character", "Stroke", "Match Game", "Skins Match", "Club Slots", "Mini-Golf",
};
char sNoSavedGameText[] = "There is no saved data.";
char sNoReplayText[] = "There is no replay data.";
char sEndGameText[] = "End Game";
char sSaveMenuText[] = "Save Menu";
s32 sIconImageIndex = 0;
s32 sUnusedSaveMenuWord = 0;
s32 sResumedSongId = NO_SONG;
s16 sReplayLabelIds[REPLAY_LABEL_COUNT] = { 0x697, 0x698, 0x699, 0x69A, 0x69B, 0x69C };
s32 sIconFrameTimer = 0;
f32 sPanelWaveAngle = 0.0f;
f32 sTitleSwayAngle = 0.0f;
f32 sCursorPulseAngle = 0.0f;
f32 sReplayTitleBobAngle = 0.0f;

/* Returns the MODE_ICON_* index of a saved game's mode, or NO_MODE_ICON */
s32 GetModeIconIndex(GameState *state) {
    s32 iconIndex;

    if (state->isMiniGolf) {
        iconIndex = MODE_ICON_MINI_GOLF;
    } else {
        switch (state->gameMode) {
            case GAME_MODE_TOURNAMENT:
                iconIndex = MODE_ICON_TOURNAMENT;
                break;
            case GAME_MODE_GET_CHARACTER:
                iconIndex = MODE_ICON_GET_CHARACTER;
                break;
            case GAME_MODE_STROKE:
                iconIndex = MODE_ICON_STROKE;
                break;
            case GAME_MODE_MATCH_GAME:
                iconIndex = MODE_ICON_MATCH_GAME;
                break;
            case GAME_MODE_SKINS_MATCH:
                iconIndex = MODE_ICON_SKINS_MATCH;
                break;
            case GAME_MODE_CLUB_SLOTS:
                iconIndex = MODE_ICON_CLUB_SLOTS;
                break;
            default:
                iconIndex = NO_MODE_ICON;
                break;
        }
    }
    return iconIndex;
}

/*
 * Opens the save menu on the saved games (SAVE_MENU_GAMES) or the replay
 * records (SAVE_MENU_REPLAYS), loading its images and putting the cursor on
 * the first free slot.
 */
#ifdef NON_MATCHING
void OpenSaveMenu(s32 kind) {
    EncodedAsset encodedAsset;
    AssetFile file;
    SaveData *save;
    s32 slot;
    s32 playerIndex;
    s32 iconIndex;
    Unk68 *record;
    UnkB8 *player;
    GameState *state;
    u16 frameId;
    void *picture;
    s32 fileSize;

    save = GetSaveData();
    if (kind == SAVE_MENU_GAMES) {
        InvalidateBrokenSavedGames();
    } else {
        func_8005D0D8();
    }
    if (kind == SAVE_MENU_GAMES) {
        SetSongVolume(SAVE_MENU_SONG_VOLUME);
    }
    ReadEncodedAsset(sPanelImage = AllocMemory(OpenEncodedAsset(SAVE_MENU_PANEL_IMAGE, &encodedAsset)), &encodedAsset);
    ReadEncodedAsset(sSaveMenuTitleImage = AllocMemory(OpenEncodedAsset(SAVE_MENU_TITLE_IMAGE, &encodedAsset)),
                     &encodedAsset);
    PlaySoundWithEffects(SOUND_SAVE_MENU_OPEN, g_NextSoundChannel, MENU_SOUND_VOLUME, MENU_SOUND_REVERB,
                         SAVE_MENU_SOUND_PAN);
    for (slot = 0; slot != CHARACTER_IMAGE_COUNT; slot++) {
        ReadEncodedAsset(sCharacterImages[slot] = AllocMemory(OpenEncodedAsset(
                             D_800C547C[CHARACTER_PLUM] + CHARACTER_IMAGE_OFFSET, &encodedAsset)),
                         &encodedAsset);
        OpenAssetFile(D_800C54A0[CHARACTER_PLUM], &file);
        fileSize = GetAssetFileSize(&file);
        ReadAssetFile(sCharacterFiles[slot] = AllocMemory(fileSize), fileSize, &file);
    }
    if (kind == SAVE_MENU_GAMES) {
        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (save->savedGames[slot].isValid &&
                (iconIndex = GetModeIconIndex(&save->savedGames[slot].state)) != NO_MODE_ICON) {
                ReadEncodedAsset(sModeIconImages[slot][0] = AllocMemory(OpenEncodedAsset(
                                     g_ModeIconAnimations[iconIndex][0], &encodedAsset)),
                                 &encodedAsset);
                frameId = g_ModeIconAnimations[iconIndex][1];
            } else {
                ReadEncodedAsset(sModeIconImages[slot][0] = AllocMemory(OpenEncodedAsset(
                                     g_ModeIconAnimations[0][0], &encodedAsset)),
                                 &encodedAsset);
                frameId = g_ModeIconAnimations[0][1];
            }
            sModeIconImages[slot][1] = picture = AllocMemory(OpenEncodedAsset(frameId, &encodedAsset));
            ReadEncodedAsset(picture, &encodedAsset);
        }
    } else {
        for (slot = 0; slot != REPLAY_LABEL_COUNT; slot++) {
            ReadEncodedAsset(sReplayLabelImages[slot] = AllocMemory(OpenEncodedAsset(sReplayLabelIds[slot],
                                                                                     &encodedAsset)),
                             &encodedAsset);
        }
        ReadEncodedAsset(sCourseNumbersImage = AllocMemory(OpenEncodedAsset(COURSE_NUMBERS_IMAGE, &encodedAsset)),
                         &encodedAsset);
    }
    if (kind == SAVE_MENU_GAMES) {
        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (save->savedGames[slot].isValid) {
                state = &save->savedGames[slot].state;
                for (playerIndex = 0; playerIndex != state->playerCount; playerIndex++) {
                    player = &save->savedPlayers[slot][playerIndex];
                    OpenEncodedAsset(D_800C547C[(s8)player->unk1D] + CHARACTER_IMAGE_OFFSET, &encodedAsset);
                    ReadEncodedAsset(sCharacterImages[slot * MAX_PLAYER_COUNT + playerIndex], &encodedAsset);
                    OpenAssetFile(D_800C54A0[(s8)player->unk1D] + (s8)player->unk7B, &file);
                    ReadAssetFile(sCharacterFiles[slot * MAX_PLAYER_COUNT + playerIndex], GetAssetFileSize(&file),
                                  &file);
                }
            }
        }
    } else {
        for (slot = 0; slot != SAVE_RECORD_COUNT; slot++) {
            record = &save->unk1DF0[slot];
            if (record->unk36 & RECORD_IN_USE) {
                OpenEncodedAsset(D_800C547C[(s8)record->unk37] + CHARACTER_IMAGE_OFFSET, &encodedAsset);
                ReadEncodedAsset(sCharacterImages[slot], &encodedAsset);
                OpenAssetFile(D_800C54A0[(s8)record->unk37] + (s8)record->unk39, &file);
                ReadAssetFile(sCharacterFiles[slot], GetAssetFileSize(&file), &file);
            }
        }
    }
    sIconImageIndex = 0;
    sIconFrames[0] = 1;
    sIconFrames[1] = 1;
    sIconFrames[2] = 1;
    if (kind == SAVE_MENU_GAMES) {
        sCursorIndex = END_GAME_ROW;
        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (!save->savedGames[slot].isValid) {
                sCursorIndex = slot;
                break;
            }
        }
    } else {
        sCursorIndex = SKIP_REPLAY_ROW;
        for (slot = 0; slot != SAVE_RECORD_COUNT; slot++) {
            if (!(save->unk1DF0[slot].unk36 & RECORD_IN_USE)) {
                sCursorIndex = slot;
                break;
            }
        }
    }
    sStateFrame = 0;
    sMenuState = SAVE_MENU_STATE_OPENING;
    sSlideOffsets[0] = SCREEN_WIDTH;
    sSlideOffsets[1] = SCREEN_WIDTH;
    sSlideOffsets[2] = SCREEN_WIDTH;
    sSlideOffsets[3] = SCREEN_WIDTH;
    sMenuKind = kind;
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", OpenSaveMenu);
#endif

void CloseSaveMenu(void) {
    s32 index;

    SetSongVolume(FULL_SONG_VOLUME);
    FreeMemory(&sPanelImage);
    FreeMemory(&sSaveMenuTitleImage);
    if (sMenuKind == SAVE_MENU_GAMES) {
        for (index = 0; index != SAVED_GAME_COUNT; index++) {
            FreeMemory(&sModeIconImages[index][0]);
            FreeMemory(&sModeIconImages[index][1]);
        }
    } else {
        for (index = 0; index != REPLAY_LABEL_COUNT; index++) {
            FreeMemory(&sReplayLabelImages[index]);
        }
        FreeMemory(&sCourseNumbersImage);
    }
    for (index = 0; index != CHARACTER_IMAGE_COUNT; index++) {
        FreeMemory(&sCharacterImages[index]);
        FreeMemory(&sCharacterFiles[index]);
    }
    sMenuKind = SAVE_MENU_CLOSED;
    func_800719A0(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, 1, 0, 0, 0, 0, 0, 0);
    func_800719A0(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, 1, 1, 0, 0, 0, 0, 0);
    func_800719A0(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, 1, 2, 0, 0, 0, 0, 0);
    func_800719A0(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, 1, 3, 0, 0, 0, 0, 0);
    func_800719A0(MESSAGE_CENTER_X, MESSAGE_CENTER_Y, NO_TEXT, 1, 4, 0, 0, 0, 0, 0);
}

/*
 * Every few frames, or at once when told to, shows the next frame of the
 * chosen saved game's mode icon and the first frame of the others. Each
 * icon has two images, read into in turn.
 */
#ifdef NON_MATCHING
void AnimateModeIcons(s32 isImmediate) {
    EncodedAsset encodedAsset;
    SaveData *save;
    SavedGame *game;
    s32 slot;
    void **images;

    save = GetSaveData();
    if ((++sIconFrameTimer >= ICON_FRAME_PERIOD) | (isImmediate == TRUE)) {
        sIconFrameTimer = 0;
        for (slot = 0, images = sModeIconImages[0]; slot != SAVED_GAME_COUNT; slot++, images += 2) {
            game = &save->savedGames[slot];
            if (!save->savedGames[slot].isValid) {
                continue;
            }
            if (game->state.isMiniGolf) {
                if (sCursorIndex == slot) {
                    if (g_ModeIconAnimations[MODE_ICON_MINI_GOLF][++sIconFrames[slot]] == END_OF_ICON_FRAMES) {
                        sIconFrames[slot] = 0;
                    }
                    OpenEncodedAsset(g_ModeIconAnimations[MODE_ICON_MINI_GOLF][sIconFrames[slot]], &encodedAsset);
                } else {
                    OpenEncodedAsset(g_ModeIconAnimations[MODE_ICON_MINI_GOLF][0], &encodedAsset);
                }
            } else if (sCursorIndex == slot) {
                if (g_ModeIconAnimations[sModeIcons[game->state.gameMode]][++sIconFrames[slot]] == END_OF_ICON_FRAMES) {
                    sIconFrames[slot] = 0;
                }
                OpenEncodedAsset(g_ModeIconAnimations[sModeIcons[game->state.gameMode]][sIconFrames[slot]],
                                 &encodedAsset);
            } else {
                OpenEncodedAsset(g_ModeIconAnimations[sModeIcons[game->state.gameMode]][0], &encodedAsset);
            }
            ReadEncodedAsset(images[sIconImageIndex], &encodedAsset);
        }
        sIconImageIndex ^= 1;
    }
}

#else
INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", AnimateModeIcons);
#endif

/*
 * Draws the saved games: each row's panel, mode icon, golfers, mode,
 * course and hole, the End Game row, the swaying title and the cursor's
 * pulsing frame, all slid by sSlideOffsets.
 */
void DrawGameSaveMenu(Gfx **displayListHead) {
    Gfx *gfx;
    SaveData *save;
    s32 row;
    /* Counts rows, icon halves or golfers; sharing one variable is there to match */
    s32 index;
    s32 strip;
    s32 stripHeight;
    f32 titleLeftExact;
    f32 titleTopExact;
    s32 titleLeft;
    s32 titleTop;
    s32 titleTexelLeft;
    s32 titleTexelTop;
    s32 drawnHeight;
    s32 imageLeft;

    save = GetSaveData();
    gfx = *displayListHead;
    if (sMenuKind == SAVE_MENU_GAMES) {
        AnimateModeIcons(FALSE);

        /* The panels, stretched from the panel image a strip at a time */
        gDPPipeSync(gfx++);
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        /* The wave's height is never used */
        sinf(sPanelWaveAngle);
        sPanelWaveAngle += DEGREES_TO_RADIANS(SWAY_DEGREES_PER_FRAME);
        func_8006A548(&gfx, (u8 *)sPanelImage + IMAGE_HEADER_SIZE);
        for (strip = 0; strip != PANEL_STRIP_COUNT; strip++) {
            func_8006A84C(&gfx, (u8 *)sPanelImage + LARGE_PALETTED_TEXELS_OFFSET + strip * PANEL_STRIP_SIZE,
                          G_IM_FMT_CI, G_IM_SIZ_8b, PANEL_IMAGE_WIDTH, PANEL_STRIP_HEIGHT, 0, G_TX_MIRROR,
                          G_TX_MIRROR, 0, 0, 0, 0);
            for (index = 0; index != SAVED_GAME_COUNT; index++) {
                if (index == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
                                    CHOSEN_PANEL_SHADE);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                stripHeight = GAME_PANEL_STRIP_HEIGHT;
                func_8006A2C0(&gfx, (sSlideOffsets[index] + WIDE_PANEL_LEFT) << 2,
                              (GAME_PANEL_MIDDLE + index * GAME_ROW_SPACING + strip * stripHeight -
                               stripHeight * PANEL_STRIP_COUNT / 2) << 2,
                              (sSlideOffsets[index] + WIDE_PANEL_LEFT + WIDE_PANEL_WIDTH) << 2,
                              (GAME_PANEL_MIDDLE + index * GAME_ROW_SPACING + strip * stripHeight -
                               stripHeight * PANEL_STRIP_COUNT / 2 + stripHeight) << 2,
                              G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / WIDE_PANEL_WIDTH,
                              (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
            }
            if (sCursorIndex == index) {
                gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
                                CHOSEN_PANEL_SHADE);
            } else {
                gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
            }
            stripHeight = END_GAME_PANEL_STRIP_HEIGHT;
            func_8006A2C0(&gfx, (sSlideOffsets[index] + NARROW_PANEL_LEFT) << 2,
                          (END_GAME_PANEL_MIDDLE + index * GAME_ROW_SPACING + strip * stripHeight -
                               stripHeight * PANEL_STRIP_COUNT / 2) << 2,
                          (sSlideOffsets[index] + NARROW_PANEL_LEFT + NARROW_PANEL_WIDTH) << 2,
                          (END_GAME_PANEL_MIDDLE + index * GAME_ROW_SPACING + strip * stripHeight -
                               stripHeight * PANEL_STRIP_COUNT / 2 + stripHeight) << 2,
                          G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / NARROW_PANEL_WIDTH,
                          (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
            gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
                            CHOSEN_PANEL_SHADE);
        }

        /* The title, swaying to a fraction of a pixel */
        titleLeftExact = cosf(sTitleSwayAngle * 0.5f) * TITLE_SWAY_DISTANCE + TITLE_SWAY_LEFT;
        titleTopExact = sinf(sTitleSwayAngle) * TITLE_SWAY_DISTANCE + TITLE_SWAY_TOP;
        titleLeft = titleLeftExact < 0.0f ? titleLeftExact + 0.5f : titleLeftExact - 0.5f;
        titleTop = titleTopExact < 0.0f ? titleTopExact + 0.5f : titleTopExact - 0.5f;
        titleTexelLeft = (s32)(HALF_TEXEL_COORDINATE - (titleLeftExact - titleLeft) * HALF_TEXEL_COORDINATE);
        titleTexelTop = (s32)(HALF_TEXEL_COORDINATE - (titleTopExact - titleTop) * HALF_TEXEL_COORDINATE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
        sTitleSwayAngle += DEGREES_TO_RADIANS(SWAY_DEGREES_PER_FRAME);
        func_8006A548(&gfx, (u8 *)sSaveMenuTitleImage + IMAGE_HEADER_SIZE);
        imageLeft = TITLE_IMAGE_LEFT;
        /* The strips are counted with row; reusing it is there to match */
        for (row = 0; row != TITLE_STRIP_COUNT; row++) {
            drawnHeight = TITLE_STRIP_DRAWN_HEIGHT;
            if (row == TITLE_STRIP_COUNT - 1) {
                drawnHeight = TITLE_LAST_STRIP_HEIGHT;
            }
            gDPLoadTextureTile(gfx++,
                               ((u32)sSaveMenuTitleImage + LARGE_PALETTED_TEXELS_OFFSET + row * TITLE_STRIP_SIZE) &
                                   ~7,
                               G_IM_FMT_CI, G_IM_SIZ_8b, TITLE_IMAGE_WIDTH, TITLE_STRIP_DRAWN_HEIGHT, 0, 0,
                               TITLE_IMAGE_WIDTH - 1, TITLE_STRIP_DRAWN_HEIGHT - 1, 0, 0, 0, 0, 0, 0, 0);
            gSPTextureRectangle(gfx++, (titleLeft + imageLeft - sSlideOffsets[1]) << 2,
                                (titleTop + row * TITLE_STRIP_HEIGHT) << 2,
                                (titleLeft + (imageLeft + TITLE_IMAGE_WIDTH) - sSlideOffsets[1]) << 2,
                                (row * TITLE_STRIP_HEIGHT + drawnHeight + titleTop) << 2, G_TX_RENDERTILE,
                                titleTexelLeft + TEXEL_COORDINATE_ONE,
                                titleTexelTop + TEXEL_COORDINATE_ONE, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);

        /* Each saved game's mode icon, from the image not being loaded into */
        for (row = 0; row != SAVED_GAME_COUNT; row++) {
            func_8006A548(&gfx, (u8 *)sModeIconImages[row][sIconImageIndex ^ 1] + IMAGE_HEADER_SIZE);
            if (save->savedGames[row].isValid) {
                gDPPipeSync(gfx++);
                if (row == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
                                    CHOSEN_PANEL_SHADE);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                gDPPipeSync(gfx++);
                for (index = 0; index != MODE_ICON_PART_COUNT; index++) {
                    func_8006A84C(&gfx,
                                  (u8 *)sModeIconImages[row][sIconImageIndex ^ 1] + LARGE_PALETTED_TEXELS_OFFSET +
                                      index * MODE_ICON_PART_HEIGHT * MODE_ICON_SIZE,
                                  G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_SIZE, MODE_ICON_PART_HEIGHT, 0, G_TX_MIRROR,
                                  G_TX_MIRROR, 0, 0, 0, 0);
                    func_8006A2C0(&gfx, (sSlideOffsets[row] + MODE_ICON_LEFT) << 2,
                                  (MODE_ICON_TOP + row * GAME_ROW_SPACING + index * (MODE_ICON_PART_HEIGHT / 2)) << 2,
                                  (sSlideOffsets[row] + MODE_ICON_LEFT + MODE_ICON_SIZE / 2) << 2,
                                  (MODE_ICON_TOP + row * GAME_ROW_SPACING + index * (MODE_ICON_PART_HEIGHT / 2) +
                                   MODE_ICON_PART_HEIGHT / 2) << 2,
                                  G_TX_RENDERTILE, 0, 0, HALF_SIZE_TEXEL_STEP, HALF_SIZE_TEXEL_STEP);
                }
            }
        }

        /* Each saved game's golfers, side by side about GOLFERS_LEFT */
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        for (row = 0; row != SAVED_GAME_COUNT; row++) {
            if (save->savedGames[row].isValid) {
                GameState *state;
                s32 golfersLeft;
                s32 golfersRight;

                gDPPipeSync(gfx++);
                golfersLeft = GOLFERS_LEFT;
                golfersRight = GOLFERS_LEFT + CHARACTER_IMAGE_WIDTH;
                state = &save->savedGames[row].state;
                if (row == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                gDPPipeSync(gfx++);
                for (index = 0; index != state->playerCount; index++) {
                    func_8006A548(&gfx, sCharacterFiles[row * MAX_PLAYER_COUNT + index]);
                    func_8006A84C(&gfx, sCharacterImages[row * MAX_PLAYER_COUNT + index], G_IM_FMT_CI,
                                  G_IM_SIZ_8b, CHARACTER_IMAGE_WIDTH, CHARACTER_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0,
                                  0);
                    func_8006A2C0(&gfx,
                                  (index * GOLFER_SPACING + (sSlideOffsets[row] + golfersLeft) -
                                   state->playerCount * (GOLFER_SPACING / 2)) << 2,
                                  (GOLFER_TOP + row * GAME_ROW_SPACING) << 2,
                                  (index * GOLFER_SPACING + (sSlideOffsets[row] + golfersRight) -
                                   state->playerCount * (GOLFER_SPACING / 2)) << 2,
                                  (GOLFER_TOP + row * GAME_ROW_SPACING + CHARACTER_IMAGE_HEIGHT) << 2,
                                  G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                }
            }
        }

        /* The cursor's frame around the chosen row, pulsing */
        {
            s32 rowHeights[GAME_ROW_COUNT] = { 42, 42, 42, 25 };
            s32 rowHalfWidths[GAME_ROW_COUNT] = { 136, 136, 136, 80 };
            s32 centerX;
            s32 rowTop;

            sCursorPulseAngle += DEGREES_TO_RADIANS(SWAY_DEGREES_PER_FRAME);
            centerX = SCREEN_WIDTH / 2;
            rowTop = FIRST_GAME_ROW_TOP;
            gDPSetPrimColor(gfx++, 0, 0, 0x00, 0xBF, 0xFF,
                            (s32)(sinf(sCursorPulseAngle) * CURSOR_ALPHA_SWING) + CURSOR_ALPHA_MIDDLE);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            func_8006A2C0(&gfx, (sSlideOffsets[sCursorIndex] + centerX - rowHalfWidths[sCursorIndex]) << 2,
                          (rowTop + sCursorIndex * GAME_ROW_SPACING) << 2,
                          (rowHalfWidths[sCursorIndex] + (sSlideOffsets[sCursorIndex] + centerX)) << 2,
                          (rowTop + sCursorIndex * GAME_ROW_SPACING + CURSOR_FRAME_THICKNESS) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            func_8006A2C0(&gfx, (sSlideOffsets[sCursorIndex] + centerX - rowHalfWidths[sCursorIndex]) << 2,
                          (rowHeights[sCursorIndex] + (rowTop + sCursorIndex * GAME_ROW_SPACING)) << 2,
                          (rowHalfWidths[sCursorIndex] + (sSlideOffsets[sCursorIndex] + centerX)) << 2,
                          (rowHeights[sCursorIndex] + (rowTop + sCursorIndex * GAME_ROW_SPACING) +
                           CURSOR_FRAME_THICKNESS) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            func_8006A2C0(&gfx, (sSlideOffsets[sCursorIndex] + centerX - rowHalfWidths[sCursorIndex]) << 2,
                          (rowTop + sCursorIndex * GAME_ROW_SPACING) << 2,
                          (sSlideOffsets[sCursorIndex] + centerX - rowHalfWidths[sCursorIndex] +
                           CURSOR_FRAME_THICKNESS) << 2,
                          (rowHeights[sCursorIndex] + (rowTop + sCursorIndex * GAME_ROW_SPACING) +
                           CURSOR_FRAME_THICKNESS) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            func_8006A2C0(&gfx,
                          (rowHalfWidths[sCursorIndex] + (sSlideOffsets[sCursorIndex] + centerX) -
                           CURSOR_FRAME_THICKNESS) << 2,
                          (rowTop + sCursorIndex * GAME_ROW_SPACING) << 2,
                          (rowHalfWidths[sCursorIndex] + (sSlideOffsets[sCursorIndex] + centerX)) << 2,
                          (rowHeights[sCursorIndex] + (rowTop + sCursorIndex * GAME_ROW_SPACING) +
                           CURSOR_FRAME_THICKNESS) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }

        /* Each saved game's mode, course and hole, or that the slot is empty */
        func_800734F0(&gfx, 0);
        for (row = 0; row != SAVED_GAME_COUNT; row++) {
            gDPPipeSync(gfx++);
            if (row == sCursorIndex) {
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            } else {
                gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
            }
            gDPPipeSync(gfx++);
            if (save->savedGames[row].isValid) {
                GameState *state = &save->savedGames[row].state;

                if (state->isMiniGolf) {
                    sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_MINI_GOLF]);
                } else {
                    switch (state->gameMode) {
                        case GAME_MODE_TOURNAMENT:
                            sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_TOURNAMENT]);
                            break;
                        case GAME_MODE_GET_CHARACTER:
                            sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_GET_CHARACTER]);
                            break;
                        case GAME_MODE_STROKE:
                            sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_STROKE]);
                            break;
                        case GAME_MODE_MATCH_GAME:
                            sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_MATCH_GAME]);
                            break;
                        case GAME_MODE_SKINS_MATCH:
                            sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_SKINS_MATCH]);
                            break;
                        case GAME_MODE_CLUB_SLOTS:
                            sprintf(g_PrintBuffer, "%s", sModeNames[MODE_NAME_CLUB_SLOTS]);
                            break;
                    }
                }
                func_8007624C(&gfx, sSlideOffsets[row] + MODE_TEXT_LEFT, MODE_TEXT_TOP + row * GAME_ROW_SPACING,
                              g_PrintBuffer, 0xFFFF9300, 0x44FF0000, 0);
                if (state->isMiniGolf) {
                    if (state->courseIndex == LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_GREEN) {
                        sprintf(g_PrintBuffer, "%s GREEN", g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name);
                    }
                    if (state->courseIndex == LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_SLOW) {
                        sprintf(g_PrintBuffer, "%s SLOW", g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name);
                    }
                    if (state->courseIndex == LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_FAST) {
                        sprintf(g_PrintBuffer, "%s FAST", g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name);
                    }
                    if (state->courseIndex == PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_GREEN) {
                        sprintf(g_PrintBuffer, "%s GREEN", g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name);
                    }
                    if (state->courseIndex == PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_SLOW) {
                        sprintf(g_PrintBuffer, "%s SLOW", g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name);
                    }
                    if (state->courseIndex == PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_FAST) {
                        sprintf(g_PrintBuffer, "%s FAST", g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name);
                    }
                } else if (state->gameMode == GAME_MODE_TOURNAMENT) {
                    sprintf(g_PrintBuffer, "%s", g_TournamentNames[state->courseIndex]);
                } else {
                    sprintf(g_PrintBuffer, "%s", g_CourseRecords[state->courseIndex].name);
                }
                func_800738BC(g_PrintBuffer, COURSE_NAME_FONT_SIZE);
                func_8007624C(&gfx, sSlideOffsets[row] + MODE_TEXT_LEFT, COURSE_TEXT_TOP + row * GAME_ROW_SPACING,
                              g_PrintBuffer, 0x00FFFF00, 0xFFFFFF00, 0);
                gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, 0xFF);
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                sprintf(g_PrintBuffer, "Hole%d", state->holeIndex + 1);
                func_80073F24(&gfx, sSlideOffsets[row] + GAME_HOLE_TEXT_LEFT,
                              GAME_HOLE_TEXT_TOP + row * GAME_ROW_SPACING, g_PrintBuffer);
            } else {
                gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x2C, 0xFF);
                gDPSetPrimColor(gfx++, 0, 0, 0x40, 0xFF, 0x00, 0xFF);
                sprintf(g_PrintBuffer, "%s", sNoSavedGameText);
                func_800747B0(&gfx, sSlideOffsets[row] + SCREEN_WIDTH / 2, EMPTY_GAME_TEXT_TOP + row * GAME_ROW_SPACING,
                              g_PrintBuffer);
            }
        }
        gDPPipeSync(gfx++);
        func_8007624C(&gfx, sSlideOffsets[END_GAME_ROW] + END_GAME_TEXT_LEFT, END_GAME_TEXT_TOP, sEndGameText,
                      0xFF571300, 0xFFDB4C00, 0);
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0x00, 0xFF);
        gDPSetEnvColor(gfx++, 0xFF, 0x68, 0x00, 0xFF);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

/* Saves the current game in the chosen slot and loads its players' images */
void SaveGameToSlot(void) {
    EncodedAsset encodedAsset;
    AssetFile file;
    SaveData *save;
    SavedGame *game;
    GameState *state;
    UnkB8 *player;
    s32 playerIndex;

    save = GetSaveData();
    game = &save->savedGames[sCursorIndex];
    SaveOrContinueGame(sCursorIndex, FALSE);
    state = &game->state;
    sStateFrame = 0;
    InvalidateBrokenSavedGames();
    if (save->savedGames[sCursorIndex].isValid) {
        sMenuState = SAVE_MENU_STATE_SAVED;
        AnimateModeIcons(TRUE);
        for (playerIndex = 0; playerIndex != state->playerCount; playerIndex++) {
            player = &save->savedPlayers[sCursorIndex][playerIndex];
            OpenEncodedAsset(D_800C547C[(s8)player->unk1D] + CHARACTER_IMAGE_OFFSET, &encodedAsset);
            ReadEncodedAsset(sCharacterImages[sCursorIndex * MAX_PLAYER_COUNT + playerIndex], &encodedAsset);
            OpenAssetFile(D_800C54A0[(s8)player->unk1D] + (s8)player->unk7B, &file);
            ReadAssetFile(sCharacterFiles[sCursorIndex * MAX_PLAYER_COUNT + playerIndex], GetAssetFileSize(&file),
                          &file);
        }
    } else {
        sMenuState = SAVE_MENU_STATE_CHOOSING;
    }
    WriteSaveData();
}

void LeaveGameSaveMenu(void) {
    sStateFrame = 0;
    sMenuState = SAVE_MENU_STATE_LEAVING;
}

/* Stores the current replay in the chosen record and loads its character's images */
void SaveReplayToSlot(void) {
    EncodedAsset encodedAsset;
    AssetFile file;
    Unk68 *record;

    record = &GetSaveData()->unk1DF0[sCursorIndex];
    func_8005B28C(sCursorIndex, (Unk68 *)&D_80105F40);
    sStateFrame = 0;
    func_8005D0D8();
    if (record->unk36 & RECORD_IN_USE) {
        OpenEncodedAsset(D_800C547C[(s8)record->unk37] + CHARACTER_IMAGE_OFFSET, &encodedAsset);
        ReadEncodedAsset(sCharacterImages[sCursorIndex], &encodedAsset);
        OpenAssetFile(D_800C54A0[(s8)record->unk37] + (s8)record->unk39, &file);
        ReadAssetFile(sCharacterFiles[sCursorIndex], GetAssetFileSize(&file), &file);
        sMenuState = SAVE_MENU_STATE_SAVED;
    } else {
        sMenuState = SAVE_MENU_STATE_CHOOSING;
    }
    WriteSaveData();
}

void LeaveReplaySaveMenu(void) {
    sStateFrame = 0;
    sMenuState = SAVE_MENU_STATE_LEAVING;
}

/* Slides the saved games in and out and lets the player choose one to save over or end the game */
void UpdateGameSaveMenu(ControllerState *controller) {
    SaveData *save;
    s32 row;

    save = GetSaveData();
    sStateFrame++;
    switch (sMenuState) {
        case SAVE_MENU_STATE_OPENING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < sStateFrame && sSlideOffsets[row] > 0) {
                    if (sSlideOffsets[row] == 1) {
                        sSlideOffsets[row] = 0;
                    } else {
                        sSlideOffsets[row] += -sSlideOffsets[row] / 2;
                    }
                }
            }
            if (sStateFrame == SLIDE_FRAMES) {
                sMenuState = SAVE_MENU_STATE_CHOOSING;
            }
            break;
        case SAVE_MENU_STATE_CHOOSING:
            if (!func_8006BA10()) {
                break;
            }
            if (controller->pressedButtons & B_BUTTON) {
                PlaySoundWithEffects(SOUND_SAVE_MENU_CLOSE, g_NextSoundChannel, MENU_SOUND_VOLUME, MENU_SOUND_REVERB,
                                     0);
                AnimateModeIcons(TRUE);
                sStateFrame = 0;
                sMenuState = SAVE_MENU_STATE_BACKING_OUT;
                break;
            }
            if (controller->pressedButtons & A_BUTTON) {
                if (sCursorIndex >= 0 && sCursorIndex < SAVED_GAME_COUNT) {
                    if (save->savedGames[sCursorIndex].isValid) {
                        D_800C306C = PAUSE_SCREEN_UNK6;
                        func_8006BA24(PROMPT_OVERWRITE_GAME);
                        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
                    } else {
                        SaveGameToSlot();
                        PlaySound(SOUND_SAVED, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
                    }
                } else {
                    D_800C306C = PAUSE_SCREEN_UNK6;
                    func_8006BA24(PROMPT_END_GAME);
                }
                break;
            }
            if (controller->repeatedButtons & U_JPAD) {
                sCursorIndex--;
                AnimateModeIcons(TRUE);
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
            } else if (controller->repeatedButtons & D_JPAD) {
                sCursorIndex++;
                AnimateModeIcons(TRUE);
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
            }
            if (sCursorIndex < 0) {
                sCursorIndex = END_GAME_ROW;
            }
            if (sCursorIndex > END_GAME_ROW) {
                sCursorIndex = 0;
            }
            break;
        case SAVE_MENU_STATE_BACKING_OUT:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < sStateFrame) {
                    if (sSlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
                        sSlideOffsets[row] -= SLIDE_OUT_START_STEP;
                    } else {
                        sSlideOffsets[row] += sSlideOffsets[row] / 2;
                    }
                }
            }
            if (sStateFrame == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(SOUND_SAVE_MENU_CLOSE, g_NextSoundChannel, MENU_SOUND_VOLUME, MENU_SOUND_REVERB,
                                     0);
            }
            if (sStateFrame == SLIDE_FRAMES) {
                sMenuState = SAVE_MENU_STATE_BACKED_OUT;
            }
            break;
        case SAVE_MENU_STATE_BACKED_OUT:
            CloseSaveMenu();
            D_800C306C = PAUSE_SCREEN_NONE;
            func_800934CC(-1);
            SetFramebufferCount(3);
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            D_801061BD = TRUE;
            D_801061C4 = TRUE;
            D_801061BF = TRUE;
            D_801061BE = FALSE;
            D_801061C0 = TRUE;
            D_801061C1 = TRUE;
            D_801061C2 = TRUE;
            D_801061C3 = TRUE;
            D_801061C5 = TRUE;
            break;
        case SAVE_MENU_STATE_SAVED:
            if (sStateFrame == SAVED_WAIT_FRAMES) {
                sMenuState = SAVE_MENU_STATE_LEAVING;
                sStateFrame = 0;
            }
            break;
        case SAVE_MENU_STATE_LEAVING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < sStateFrame) {
                    if (sSlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
                        sSlideOffsets[row] -= SLIDE_OUT_START_STEP;
                    } else {
                        sSlideOffsets[row] += sSlideOffsets[row] / 2;
                    }
                }
            }
            if (sStateFrame == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(SOUND_SAVE_MENU_CLOSE, g_NextSoundChannel, MENU_SOUND_VOLUME, MENU_SOUND_REVERB,
                                     0);
            }
            if (sStateFrame == SLIDE_FRAMES) {
                D_800C306C = PAUSE_SCREEN_NONE;
                CloseSaveMenu();
                func_80060358();
            }
            break;
    }
}

/*
 * Slides the replay records in and out and lets the player choose one to
 * store the replay in. It returns a REPLAY_MENU_* value.
 */
s32 UpdateReplaySaveMenu(ControllerState *controller) {
    SaveData *save;
    s32 row;

    save = GetSaveData();
    sStateFrame++;
    switch (sMenuState) {
        case SAVE_MENU_STATE_OPENING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < sStateFrame && sSlideOffsets[row] > 0) {
                    if (sSlideOffsets[row] == 1) {
                        sSlideOffsets[row] = 0;
                    } else {
                        sSlideOffsets[row] += -sSlideOffsets[row] / 2;
                    }
                }
            }
            if (sStateFrame == SLIDE_FRAMES) {
                sMenuState = SAVE_MENU_STATE_CHOOSING;
            }
            break;
        case SAVE_MENU_STATE_CHOOSING:
            if (!func_8006BA10()) {
                break;
            }
            if (controller->pressedButtons & A_BUTTON) {
                if (sCursorIndex >= 0 && sCursorIndex < SAVE_RECORD_COUNT) {
                    /* The pointer arithmetic is there to match */
                    if ((save->unk1DF0 + sCursorIndex)->unk36 & RECORD_IN_USE) {
                        D_800C306C = PAUSE_SCREEN_UNK6;
                        func_8006BA24(PROMPT_OVERWRITE_REPLAY);
                        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
                        return REPLAY_MENU_PROMPTING;
                    }
                    SaveReplayToSlot();
                    PlaySound(SOUND_SAVED, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
                } else {
                    D_800C306C = PAUSE_SCREEN_UNK6;
                    func_8006BA24(PROMPT_SKIP_REPLAY);
                    return REPLAY_MENU_PROMPTING;
                }
            } else if (controller->pressedButtons & B_BUTTON) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
                sCursorIndex = SKIP_REPLAY_ROW;
            } else if (controller->repeatedButtons & U_JPAD) {
                sCursorIndex--;
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
            } else if (controller->repeatedButtons & D_JPAD) {
                sCursorIndex++;
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_SOUND_VOLUME);
            }
            if (sCursorIndex < 0) {
                sCursorIndex = SKIP_REPLAY_ROW;
            }
            if (sCursorIndex > SKIP_REPLAY_ROW) {
                sCursorIndex = 0;
            }
            break;
        case SAVE_MENU_STATE_SAVED:
            if (sStateFrame == SAVED_WAIT_FRAMES) {
                sMenuState = SAVE_MENU_STATE_LEAVING;
                sStateFrame = 0;
            }
            break;
        case SAVE_MENU_STATE_LEAVING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < sStateFrame) {
                    if (sSlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
                        sSlideOffsets[row] -= SLIDE_OUT_START_STEP;
                    } else {
                        sSlideOffsets[row] += sSlideOffsets[row] / 2;
                    }
                }
            }
            if (sStateFrame == SAVED_WAIT_FRAMES) {
                CloseSaveMenu();
                return REPLAY_MENU_CLOSED;
            }
            break;
    }
    return REPLAY_MENU_OPEN;
}

/*
 * Draws the replay records: each row's panel, course number, label,
 * golfer and hole text, the Hole Out row, the cursor's frame and the
 * bobbing Save Menu title, all slid by sSlideOffsets.
 */
void DrawReplaySaveMenu(Gfx **displayListHead) {
    s32 rowLabels[REPLAY_ROW_COUNT];
    s32 rowOffsets[REPLAY_ROW_COUNT];
    Gfx *gfx;
    SaveData *save;
    s32 row;
    /* Counts strips or records; sharing one variable is there to match */
    s32 index;
    s32 stripHeight;
    s32 titleBob;
    s32 titleHeight;
    s32 slideOffset;
    u8 *labelImage;
    s32 labelWidth;
    s32 labelHeight;
    s32 text;

    save = GetSaveData();
    gfx = *displayListHead;
    /* Rows slide in pairs, the first of each pair from the left */
    for (row = 0; row != REPLAY_ROW_COUNT; row++) {
        slideOffset = sSlideOffsets[row / 2];
        rowOffsets[row] = slideOffset;
        if (!(row & 1)) {
            rowOffsets[row] = -slideOffset;
        }
    }
    if (sMenuKind == SAVE_MENU_REPLAYS) {
        func_80071608(&gfx);

        /* The panels, stretched from the panel image a strip at a time */
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        func_8006A548(&gfx, (u8 *)sPanelImage + IMAGE_HEADER_SIZE);
        for (index = 0; index != PANEL_STRIP_COUNT; index++) {
            stripHeight = RECORD_PANEL_STRIP_HEIGHT;
            func_8006A84C(&gfx, (u8 *)sPanelImage + LARGE_PALETTED_TEXELS_OFFSET + index * PANEL_STRIP_SIZE,
                          G_IM_FMT_CI, G_IM_SIZ_8b, PANEL_IMAGE_WIDTH, PANEL_STRIP_HEIGHT, 0, G_TX_MIRROR,
                          G_TX_MIRROR, 0, 0, 0, 0);
            gDPPipeSync(gfx++);
            for (row = 0; row != SAVE_RECORD_COUNT; row++) {
                if (row == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
                                    CHOSEN_PANEL_SHADE);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                func_8006A2C0(&gfx, (rowOffsets[row] + WIDE_PANEL_LEFT) << 2,
                              (FIRST_REPLAY_ROW_TOP + row * REPLAY_ROW_SPACING + index * stripHeight) << 2,
                              (rowOffsets[row] + WIDE_PANEL_LEFT + WIDE_PANEL_WIDTH) << 2,
                              (FIRST_REPLAY_ROW_TOP + row * REPLAY_ROW_SPACING + index * stripHeight + stripHeight) << 2,
                              G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / WIDE_PANEL_WIDTH,
                              (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
            }
            stripHeight = HOLE_OUT_PANEL_STRIP_HEIGHT;
            gDPPipeSync(gfx++);
            if (sCursorIndex == row) {
                gDPSetPrimColor(gfx++, 0, 0, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE, CHOSEN_PANEL_SHADE,
                                CHOSEN_PANEL_SHADE);
            } else {
                gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
            }
            func_8006A2C0(&gfx, (rowOffsets[row] + NARROW_PANEL_LEFT) << 2,
                          (FIRST_REPLAY_ROW_TOP + row * REPLAY_ROW_SPACING + index * stripHeight) << 2,
                          (rowOffsets[row] + NARROW_PANEL_LEFT + NARROW_PANEL_WIDTH) << 2,
                          (FIRST_REPLAY_ROW_TOP + row * REPLAY_ROW_SPACING + index * stripHeight + stripHeight) << 2,
                          G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / NARROW_PANEL_WIDTH,
                          (PANEL_IMAGE_HEIGHT << 10) / (stripHeight * PANEL_STRIP_COUNT));
        }

        /* Each record's "COURSE" and course number */
        func_8006A4A0(&gfx, 0, (u8 *)sCourseNumbersImage + IMAGE_HEADER_SIZE);
        gDPLoadTextureTile_4b(gfx++, ((u32)sCourseNumbersImage + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
                              COURSE_NUMBERS_WIDTH, COURSE_NUMBERS_HEIGHT, 0, 0, COURSE_NUMBERS_WIDTH - 1,
                              COURSE_NUMBERS_HEIGHT - 1, 0, G_TX_MIRROR, G_TX_MIRROR, G_TX_NOMASK, G_TX_NOMASK,
                              G_TX_NOLOD, G_TX_NOLOD);
        for (index = 0; index != SAVE_RECORD_COUNT; index++) {
            Unk68 *record = &save->unk1DF0[index];

            if (!(record->unk36 & RECORD_IN_USE)) {
                rowLabels[index] = NO_REPLAY_LABEL;
            } else {
                gDPPipeSync(gfx++);
                if (index == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                gDPPipeSync(gfx++);
                func_8006A2C0(&gfx, (rowOffsets[index] + COURSE_WORD_LEFT) << 2,
                              (COURSE_TOP + index * REPLAY_ROW_SPACING) << 2,
                              (rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH) << 2,
                              (COURSE_TOP + index * REPLAY_ROW_SPACING + COURSE_NUMBERS_HEIGHT) << 2, G_TX_RENDERTILE,
                              COURSE_WORD_TEXEL_LEFT << 5, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                func_8006A2C0(&gfx, (rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH) << 2,
                              (COURSE_TOP + index * REPLAY_ROW_SPACING) << 2,
                              (rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH + COURSE_DIGIT_WIDTH) << 2,
                              (COURSE_TOP + index * REPLAY_ROW_SPACING + COURSE_NUMBERS_HEIGHT) << 2, G_TX_RENDERTILE,
                              ((s8)record->unk34 * COURSE_DIGIT_WIDTH) << 5, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
                switch (record->unk36 & RECORD_KIND_MASK) {
                    case RECORD_KIND_ALBATROSS:
                        rowLabels[index] = REPLAY_LABEL_ALBATROSS;
                        break;
                    case RECORD_KIND_HOLE_IN_ONE:
                        rowLabels[index] = REPLAY_LABEL_HOLE_IN_ONE;
                        break;
                    case RECORD_KIND_EAGLE:
                        rowLabels[index] = REPLAY_LABEL_EAGLE;
                        break;
                    case RECORD_KIND_BIRDIE:
                        rowLabels[index] = REPLAY_LABEL_BIRDIE;
                        break;
                    case RECORD_KIND_PAR:
                        rowLabels[index] = REPLAY_LABEL_PAR;
                        break;
                }
            }
        }

        /* Each record's label */
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        for (index = 0; index != SAVE_RECORD_COUNT; index++) {
            if (rowLabels[index] != NO_REPLAY_LABEL) {
                labelImage = sReplayLabelImages[rowLabels[index]];
                labelWidth = labelImage[IMAGE_WIDTH_LOW_BYTE];
                labelHeight = labelImage[IMAGE_HEIGHT_LOW_BYTE];
                gDPPipeSync(gfx++);
                if (index == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                gDPPipeSync(gfx++);
                func_8006A4A0(&gfx, 0, (u8 *)sReplayLabelImages[rowLabels[index]] + IMAGE_HEADER_SIZE);
                func_8006A5E4(&gfx, (u8 *)sReplayLabelImages[rowLabels[index]] + PALETTED_TEXELS_OFFSET, G_IM_FMT_CI,
                              labelWidth, labelHeight, 0, G_TX_MIRROR, G_TX_MIRROR, 0, 0, 0, 0);
                func_8006A2C0(&gfx, (rowOffsets[index] + LABEL_LEFT) << 2, (LABEL_TOP + index * REPLAY_ROW_SPACING) << 2,
                              (rowOffsets[index] + LABEL_LEFT + labelWidth) << 2,
                              (LABEL_TOP + index * REPLAY_ROW_SPACING + labelHeight) << 2, G_TX_RENDERTILE, 0, 0,
                              TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            }
        }

        /* Each record's golfer */
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        for (index = 0; index != SAVE_RECORD_COUNT; index++) {
            Unk68 *record = &save->unk1DF0[index];

            if (record->unk36 & RECORD_IN_USE) {
                gDPPipeSync(gfx++);
                if (index == sCursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
                } else {
                    gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
                }
                gDPPipeSync(gfx++);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                func_8006A548(&gfx, sCharacterFiles[index]);
                func_8006A84C(&gfx, sCharacterImages[index], G_IM_FMT_CI, G_IM_SIZ_8b, CHARACTER_IMAGE_WIDTH,
                              CHARACTER_IMAGE_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
                func_8006A2C0(&gfx, (rowOffsets[index] + CHARACTER_LEFT) << 2,
                              (CHARACTER_TOP + index * REPLAY_ROW_SPACING) << 2,
                              (rowOffsets[index] + CHARACTER_LEFT + CHARACTER_IMAGE_WIDTH) << 2,
                              (CHARACTER_TOP + index * REPLAY_ROW_SPACING + CHARACTER_IMAGE_HEIGHT) << 2, G_TX_RENDERTILE, 0,
                              0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            }
        }

        /* The cursor's frame around the chosen row */
        {
            s32 rowHeights[REPLAY_ROW_COUNT] = { 33, 33, 33, 33, 33, 24 };
            s32 rowHalfWidths[REPLAY_ROW_COUNT] = { 136, 136, 136, 136, 136, 80 };
            s32 centerX;

            centerX = SCREEN_WIDTH / 2;
            gDPSetPrimColor(gfx++, 0, 0, 0x00, 0xBF, 0xFF, 0xFF);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            func_8006A2C0(&gfx, (rowOffsets[sCursorIndex] - (rowHalfWidths[sCursorIndex] - centerX)) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING) << 2,
                          (rowOffsets[sCursorIndex] + (rowHalfWidths[sCursorIndex] + centerX)) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING + CURSOR_FRAME_THICKNESS) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            func_8006A2C0(&gfx, (rowOffsets[sCursorIndex] - (rowHalfWidths[sCursorIndex] - centerX)) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING + rowHeights[sCursorIndex] -
                           CURSOR_FRAME_THICKNESS) << 2,
                          (rowOffsets[sCursorIndex] + (rowHalfWidths[sCursorIndex] + centerX)) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING + rowHeights[sCursorIndex]) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            func_8006A2C0(&gfx, (rowOffsets[sCursorIndex] - (rowHalfWidths[sCursorIndex] - centerX)) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING) << 2,
                          (rowOffsets[sCursorIndex] - (rowHalfWidths[sCursorIndex] - centerX) +
                           CURSOR_FRAME_THICKNESS) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING + rowHeights[sCursorIndex]) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            func_8006A2C0(&gfx,
                          (rowOffsets[sCursorIndex] + (rowHalfWidths[sCursorIndex] + centerX) -
                           CURSOR_FRAME_THICKNESS) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING) << 2,
                          (rowOffsets[sCursorIndex] + (rowHalfWidths[sCursorIndex] + centerX)) << 2,
                          (FIRST_REPLAY_ROW_TOP + sCursorIndex * REPLAY_ROW_SPACING + rowHeights[sCursorIndex]) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }

        /* The title's panel, bobbing up and down */
        titleBob = sinf(sReplayTitleBobAngle) * TITLE_BOB_HEIGHT;
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        sReplayTitleBobAngle += DEGREES_TO_RADIANS(TITLE_BOB_DEGREES_PER_FRAME);
        func_8006A548(&gfx, (u8 *)sPanelImage + IMAGE_HEADER_SIZE);
        for (index = 0; index != PANEL_STRIP_COUNT; index++) {
            /* Unused; it is there to match */
            stripHeight = TITLE_PANEL_STRIP_HEIGHT;
            func_8006A84C(&gfx, (u8 *)sPanelImage + LARGE_PALETTED_TEXELS_OFFSET + index * PANEL_STRIP_SIZE,
                          G_IM_FMT_CI, G_IM_SIZ_8b, PANEL_IMAGE_WIDTH, PANEL_STRIP_HEIGHT, 0, G_TX_MIRROR,
                          G_TX_MIRROR, 0, 0, 0, 0);
            titleHeight = TITLE_PANEL_STRIP_HEIGHT * PANEL_STRIP_COUNT;
            func_8006A2C0(&gfx, (rowOffsets[1] + TITLE_PANEL_LEFT) << 2,
                          (titleBob + TITLE_PANEL_TOP + index * TITLE_PANEL_STRIP_HEIGHT) << 2,
                          (rowOffsets[1] + TITLE_PANEL_LEFT + TITLE_PANEL_WIDTH) << 2,
                          (titleBob + TITLE_PANEL_TOP + index * TITLE_PANEL_STRIP_HEIGHT +
                           TITLE_PANEL_STRIP_HEIGHT) << 2,
                          G_TX_RENDERTILE, 0, 0, (PANEL_IMAGE_WIDTH << 10) / TITLE_PANEL_WIDTH,
                          (PANEL_IMAGE_HEIGHT << 10) / titleHeight);
        }

        /* Each record's hole text, or that the row is empty */
        func_800734F0(&gfx, 0);
        for (index = 0; index != SAVE_RECORD_COUNT; index++) {
            Unk68 *record;

            gDPPipeSync(gfx++);
            record = &save->unk1DF0[index];
            if (index == sCursorIndex) {
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            } else {
                gDPSetPrimColor(gfx++, 0, 0, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, OTHER_PANEL_SHADE, 0xFF);
            }
            gDPPipeSync(gfx++);
            if (!(record->unk36 & RECORD_IN_USE)) {
                func_800734F0(&gfx, 0);
                gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x2C, 0xFF);
                gDPSetPrimColor(gfx++, 0, 0, 0x40, 0xFF, 0x00, 0xFF);
                /* The saved-game message is printed but never drawn */
                sprintf(g_PrintBuffer, "%s", sNoSavedGameText);
                func_800747B0(&gfx, rowOffsets[index] + SCREEN_WIDTH / 2, EMPTY_TEXT_TOP + index * REPLAY_ROW_SPACING,
                              sNoReplayText);
            } else {
                text = (s8)record->unk34 * RING_SHOT_HOLE_COUNT + (s8)record->unk35;
                func_80071608(&gfx);
                func_800719A0(rowOffsets[index] + HOLE_TEXT_LEFT, HOLE_TEXT_TOP + index * REPLAY_ROW_SPACING,
                              text + RING_SHOT_TEXT_FIRST_IMAGE, 1, FIRST_RECORD_TEXT_SLOT + index, 32, 0xFF, 0xFF,
                              0xFF, 0xFF);
                func_80071CE4(&gfx, FIRST_RECORD_TEXT_SLOT + index);
            }
        }
        func_800734F0(&gfx, 0);
        func_8007624C(&gfx, rowOffsets[5] + HOLE_OUT_TEXT_LEFT, HOLE_OUT_TEXT_TOP, "HoleOut", 0xFF571300,
                      0xFFDB4C00, 0);
        gDPPipeSync(gfx++);
        func_8007624C(&gfx, rowOffsets[1] + TITLE_TEXT_LEFT, titleBob + TITLE_TEXT_TOP, sSaveMenuText,
                      0xFF000000, 0xFFFFFF00, 0);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

void OpenControlsScreen(void) {
    EncodedAsset encodedAsset;
    void *picture;

    if (g_IsMiniGolf) {
        sPanelImage = picture = AllocMemory(OpenEncodedAsset(MINI_GOLF_CONTROLS_PICTURE, &encodedAsset));
    } else {
        sPanelImage = picture = AllocMemory(OpenEncodedAsset(CONTROLS_PICTURE, &encodedAsset));
    }
    ReadEncodedAsset(picture, &encodedAsset);
    sSlideOffsets[0] = SCREEN_HEIGHT;
    sIsControlsShown = TRUE;
    sStateFrame = 0;
    sResumedSongId = GetSongId();
    PlaySong(CONTROLS_SONG, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
}

void CloseControlsScreen(void) {
    FreeMemory(&sPanelImage);
    sIsControlsShown = FALSE;
    PlaySong(sResumedSongId, SONG_FLAG_UNK10 | SONG_FLAG_UNREAD);
}

/* Draws the Controls picture in strips, slid down by sSlideOffsets[0] */
void DrawControlsScreen(Gfx **displayListHead) {
    Gfx *gfx;
    s32 strip;
    s32 topRaise;
    s32 bottomRaise;

    gfx = *displayListHead;
    if (sIsControlsShown == TRUE) {
        gDPPipeSync(gfx++);
        gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        func_8006A548(&gfx, (u8 *)sPanelImage + IMAGE_HEADER_SIZE);
        for (strip = 0; strip != CONTROLS_STRIP_COUNT; strip++) {
            func_8006A84C(&gfx,
                          (u8 *)sPanelImage + LARGE_PALETTED_TEXELS_OFFSET +
                              strip * CONTROLS_STRIP_HEIGHT * CONTROLS_PICTURE_WIDTH,
                          G_IM_FMT_CI, G_IM_SIZ_8b, CONTROLS_PICTURE_WIDTH, CONTROLS_STRIP_HEIGHT, 0, G_TX_MIRROR,
                          G_TX_MIRROR, 0, 0, 0, 0);
            /* How far above the screen top the first strip's top and bottom edges sit */
            topRaise = sSlideOffsets[0] - SCREEN_BORDER;
            bottomRaise = sSlideOffsets[0] - (SCREEN_BORDER + CONTROLS_STRIP_HEIGHT);
            func_8006A2C0(&gfx, CONTROLS_PICTURE_LEFT << 2,
                          (strip * CONTROLS_STRIP_HEIGHT - topRaise) << 2,
                          (CONTROLS_PICTURE_LEFT + CONTROLS_PICTURE_WIDTH) << 2,
                          (strip * CONTROLS_STRIP_HEIGHT - bottomRaise) << 2,
                          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        }
        *displayListHead = gfx;
    }
}

void UpdateControlsScreen(ControllerState *controller) {
    sStateFrame++;
    switch (sMenuState) {
        case SAVE_MENU_STATE_OPENING:
            if (sSlideOffsets[0] > 0) {
                if (sSlideOffsets[0] == 1) {
                    sSlideOffsets[0] = 0;
                } else {
                    sSlideOffsets[0] += -sSlideOffsets[0] / 2;
                }
            }
            if (sStateFrame == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(SOUND_CONTROLS_OPEN, g_NextSoundChannel, MENU_SOUND_VOLUME, MENU_SOUND_REVERB,
                                     CONTROLS_SOUND_PAN);
            }
            if (sStateFrame == SLIDE_FRAMES) {
                sMenuState = SAVE_MENU_STATE_CHOOSING;
            }
            break;
        case SAVE_MENU_STATE_CHOOSING:
            if (controller->pressedButtons & (A_BUTTON | B_BUTTON)) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_SOUND_VOLUME);
                sStateFrame = 0;
                sMenuState = SAVE_MENU_STATE_BACKING_OUT;
            }
            break;
        case SAVE_MENU_STATE_BACKING_OUT:
            if (sSlideOffsets[0] >= SLIDE_OUT_START_OFFSET) {
                sSlideOffsets[0] -= SLIDE_OUT_START_STEP;
            } else {
                sSlideOffsets[0] += sSlideOffsets[0] / 2;
            }
            if (sStateFrame == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(SOUND_CONTROLS_CLOSE, g_NextSoundChannel, MENU_SOUND_VOLUME, MENU_SOUND_REVERB,
                                     CONTROLS_SOUND_PAN);
            }
            if (sStateFrame == SLIDE_FRAMES) {
                sMenuState = SAVE_MENU_STATE_BACKED_OUT;
            }
            break;
        case SAVE_MENU_STATE_BACKED_OUT:
            CloseControlsScreen();
            D_800C306C = PAUSE_SCREEN_NONE;
            func_800934CC(-1);
            SetFramebufferCount(3);
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            D_801061BD = TRUE;
            D_801061C4 = TRUE;
            D_801061BF = TRUE;
            D_801061BE = FALSE;
            D_801061C0 = TRUE;
            D_801061C1 = TRUE;
            D_801061C2 = TRUE;
            D_801061C3 = TRUE;
            D_801061C5 = TRUE;
            break;
    }
}
