#include "asset.h"
#include "billboard.h"
#include "common.h"
#include "controller.h"
#include "heap.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"

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

/* The game modes that sModeIcons covers */
#define GAME_MODE_ICON_COUNT 12
#define MODE_NAME_COUNT 7

/* Asset ids of the Controls screen's picture */
#define CONTROLS_PICTURE 0x92A
#define MINI_GOLF_CONTROLS_PICTURE 0x96A
#define CONTROLS_SONG 0x17

#define SOUND_CONTROLS_OPEN 0x72
#define SOUND_CONTROLS_CLOSE 0x73
#define SOUND_MENU_CONFIRM 0x5D
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

/* D_800C306C's value while no pause screen is shown */
#define PAUSE_SCREEN_NONE 0

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
extern void *sCharacterFiles[12];
extern void *sCharacterImages[12];
extern void *sPanelImage;

void func_8005B28C(s32 recordIndex, Unk68 *record);
void func_8005D0D8(void);
void func_800934CC(s32);
void SetFramebufferCount(s32 count);
void CloseControlsScreen(void);

/* The MODE_ICON_* index by GameState.gameMode; modes that the menu never names hold 0 or 3 */
u8 sModeIcons[GAME_MODE_ICON_COUNT] = {
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
char D_8024580C_ovl_12[] = "There is no saved data.";
char D_80245824_ovl_12[] = "There is no replay data.";
char D_80245840_ovl_12[] = "End Game";
char D_8024584C_ovl_12[] = "Save Menu";
s32 D_80245858_ovl_12[2] = { 0, 0 };
s32 sResumedSongId = NO_SONG;
s16 D_80245864_ovl_12[6] = { 0x697, 0x698, 0x699, 0x69A, 0x69B, 0x69C };
s32 D_80245870_ovl_12 = 0;
f32 D_80245874_ovl_12 = 0.0f;
f32 D_80245878_ovl_12 = 0.0f;
f32 D_8024587C_ovl_12 = 0.0f;
f32 D_80245880_ovl_12[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

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

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", OpenSaveMenu);

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", CloseSaveMenu);

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", AnimateModeIcons);

__asm__(".section .rodata");
const s32 D_80245918_ovl_12[4] = { 0x2A, 0x2A, 0x2A, 0x19 };
const s32 D_80245928_ovl_12[4] = { 0x88, 0x88, 0x88, 0x50 };
const char D_80245938_ovl_12[] = "%s";

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", DrawGameSaveMenu);

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", SaveGameToSlot);

void LeaveGameSaveMenu(void) {
    sStateFrame = 0;
    sMenuState = SAVE_MENU_STATE_LEAVING;
}

/*
 * Stores the current replay in the chosen record and loads its character's
 * images. The menu reads the record's character signed, though Unk68 keeps
 * it unsigned; the casts are there to match.
 */
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

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", UpdateGameSaveMenu);

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", UpdateReplaySaveMenu);

__asm__(".section .rodata");
const s32 D_802459C0_ovl_12[6] = { 0x21, 0x21, 0x21, 0x21, 0x21, 0x18 };
const s32 D_802459D8_ovl_12[6] = { 0x88, 0x88, 0x88, 0x88, 0x88, 0x50 };

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", DrawReplaySaveMenu);

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

INCLUDE_ASM("asm/nonmatchings/ovl_12/save_menu", DrawControlsScreen);

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
