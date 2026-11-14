#include "tournament_menu.h"

#include <ultra64.h>

#include "address.h"
#include "asset.h"
#include "character.h"
#include "compiler.h"
#include "controller.h"
#include "debug_text.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "hole_display.h"
#include "language.h"
#include "main_menu.h"
#include "menu.h"
#include "menu_memory.h"
#include "menu_window.h"
#include "password.h"
#include "player.h"
#include "save.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"

#define ASSET_CODE_ENTRY_BAR 0x93E
#define ASSET_CODE_ENTRY_GLYPHS 0x942

#define TOURNAMENT_BANNER_WIDTH 256
#define TOURNAMENT_BANNER_STRIP_HEIGHT 32
#define TOURNAMENT_BANNER_STRIP_COUNT 2

#define CLUB_COURSE_ICON_LIMIT 10

#define MENU_WINDOW_PICTURE_COUNT 15
#define MENU_WINDOW_PALETTE_COUNT 4

#define CODE_UPPER_PANEL_LEFT 28
#define CODE_UPPER_PANEL_TOP 56
#define CODE_UPPER_PANEL_WIDTH 264
#define CODE_UPPER_PANEL_STRIP_HEIGHT 32
#define CODE_LOWER_PANEL_LEFT 20
#define CODE_LOWER_PANEL_TOP 160
#define CODE_LOWER_PANEL_WIDTH 280
#define CODE_LOWER_PANEL_STRIP_HEIGHT 20
#define CODE_ENTRY_BAR_WIDTH 256
#define CODE_ENTRY_BAR_STRIP_HEIGHT 8
#define CODE_ENTRY_BAR_STRIP_COUNT 4
#define CODE_ENTRY_BAR_LEFT 32
#define CODE_ENTRY_BAR_TOP 140
#define CODE_HAND_FIRST_X 16
#define CODE_HAND_FIRST_Y 56
#define CODE_KEY_SPACING_X 25
#define CODE_KEY_SPACING_Y 19
#define CODE_HAND_STEP_FRAMES 1
#define CODE_HAND_REACH 4
#define CODE_HAND_MOVING_RIGHT 0
#define CODE_HAND_MOVING_LEFT 1
#define CLUB_TOURNAMENT_CODE_COUNT 36
#define CLUB_TOURNAMENT_CODE_LENGTH 8

enum ClubCodeState {
    CLUB_CODE_STATE_ENTERING,
    CLUB_CODE_STATE_INCORRECT
};

typedef s32 ClubCodeState;

#define MESSAGE_ENTER_CODE 0x165

#define CODE_KEYBOARD_ROW_COUNT 4
#define CODE_KEYBOARD_COLUMN_COUNT 10
#define CODE_KEYBOARD_END_ROW 3
#define CODE_KEYBOARD_END_COLUMN 9
#define CODE_KEYBOARD_FIRST_BLANK_COLUMN 6
#define CODE_KEYBOARD_BACK_COLUMN 8
#define CODE_KEY_BLANK 0xFD
#define CODE_KEY_BACK 0xFE
#define CODE_KEY_END 0xFF
#define CODE_GLYPH_SIZE 16
#define CODE_GLYPH_FIRST_X 40
#define CODE_GLYPH_FIRST_Y 62
#define CODE_GLYPH_NONE (-1)

enum ClubCodePhase {
    CLUB_CODE_PHASE_TYPING,
    CLUB_CODE_PHASE_DETAILS,
    CLUB_CODE_PHASE_INCORRECT
};

typedef s32 ClubCodePhase;

#define CLUB_CODE_NAME_MASK 0x7F
#define CLUB_CODE_NINTENDO_SHIFT 7
#define CLUB_TOURNAMENT_NAME_COUNT 124

#define CODE_DEBUG_TEXT_COLUMN 2
#define CODE_DEBUG_TEXT_FIRST_ROW 3

enum CodeKeyResult {
    CODE_KEY_RESULT_TOO_SHORT = -1,
    CODE_KEY_RESULT_TYPING,
    CODE_KEY_RESULT_DONE
};

typedef s32 CodeKeyResult;

#define CLUB_CODE_FIRST_X 97
#define CLUB_CODE_Y 150
#define CLUB_CODE_CHARACTER_WIDTH 24
#define CLUB_CODE_NO_CHARACTER 0xFF

#define CODE_CURSOR_FIRST_X (-42)
#define CODE_CURSOR_Y (-50.0f)
#define CODE_CURSOR_SCALE 0.6f

#define CLUB_CODE_PROMPT_X 56
#define CLUB_CODE_INCORRECT_X 24
#define CLUB_CODE_MESSAGE_Y 180

#define CLUB_WINDOW_WIDTH 280
#define CLUB_WINDOW_HEIGHT 180
#define CLUB_WINDOW_CENTER_X (SCREEN_WIDTH / 2)
#define CLUB_WINDOW_CENTER_Y (SCREEN_HEIGHT / 2)
#define CLUB_WINDOW_STYLE_PLAIN 0
#define CLUB_WINDOW_STYLE_NINTENDO 1
#define CLUB_WINDOW_STYLE_PICTURE 2
#define CLUB_NUMBER_Y 68
#define CLUB_TEXT_X (SCREEN_WIDTH / 2)
#define CLUB_NAME_Y 94
#define CLUB_COURSE_Y 112
#define CLUB_MODE_Y 128
#define CLUB_NAME_SLOT 2
#define CLUB_COURSE_SLOT 4
#define CLUB_MODE_SLOT 5
#define CLUB_TEXT_SLOT_COUNT 11
#define CLUB_CHOICE_YES 0
#define CLUB_CHOICE_NO 1

static ImageAsset *f_WideCloudImage = NULL;
static ImageAsset *f_YesNoImage = NULL;
static ImageAsset *f_NintendoBadge = NULL;
static ImageAsset *f_ClubCourseIcon = NULL;
static ImageAsset *f_MenuWindowImage = NULL;
static PaletteColor *f_MenuWindowPalette = NULL;
static MenuWindowState f_MenuWindowState = MENU_WINDOW_OPENING;
static f32 f_MenuWindowScaleX = 0.0f;
static f32 f_MenuWindowScaleY = 0.0f;
static s32 f_UnusedWordsAfterWindowScale[2] UNUSED = {0, 0};
static s32 f_YesNoPulse = 0;
static s32 f_YesNoPulseDirection = 0;
static AssetId f_MenuWindowImageAssets[MENU_WINDOW_PICTURE_COUNT] = {
    0x07C0,
    0x0750,
    0x07A4,
    0x0742,
    0x0788,
    0x07B2,
    0x07CE,
    0x07D8,
    0x076C,
    0x0796,
    0x07EA,
    0x075E,
    0x077A,
    0x0988,
    0x0958,
};
static SignedAssetId f_MenuWindowPaletteFiles[MENU_WINDOW_PICTURE_COUNT]
                                             [MENU_WINDOW_PALETTE_COUNT] = {
                                                 {0x07B8, 0x07B9, 0x07BA, 0x07BB},
                                                 {0x0748, 0x0749, 0x074A, 0x074B},
                                                 {0x079C, 0x079D, 0x079E, 0x079F},
                                                 {0x073A, 0x073B, 0x073C, 0x073D},
                                                 {0x0780, 0x0781, 0x0782, 0x0783},
                                                 {0x07AA, 0x07AB, 0x07AC, 0x07AD},
                                                 {0x07C6, 0x07C7, 0x07C8, 0x07C9},
                                                 {0x07D4, 0x07D5, 0x07D6, 0x07D7},
                                                 {0x0764, 0x0765, 0x0766, 0x0767},
                                                 {0x078E, 0x078F, 0x0790, 0x0791},
                                                 {0x07E2, 0x07E3, 0x07E4, 0x07E5},
                                                 {0x0756, 0x0757, 0x0758, 0x0759},
                                                 {0x0772, 0x0773, 0x0774, 0x0775},
                                                 {0x0980, 0x0981, 0x0982, 0x0983},
                                                 {0x0957, 0x0957, 0x0957, 0x0957},
};
static s32 f_UnusedWordAfterPaletteFiles UNUSED = 0;
static u8 f_IsClubTournamentEntered = 0;
static ImageAsset *f_NamePlateImage = NULL;
static ImageAsset *f_CodeEntryBarImage = NULL;
static ImageAsset *f_CodeGlyphsImage = NULL;
static ImageAsset *f_PointingHandImage = NULL;
static ImageAsset *f_CodeCursorBox = NULL;
static s32 f_UnusedWordAfterCursorBox UNUSED = 0;
static s32 f_CodeHandMotion = 0;
static s32 f_CodeHandReach = 0;
static s32 f_CodeHandStepTimer = 0;
static s32 f_EnteredCodeLength = 0;
static ClubCodeState f_ClubCodeState = CLUB_CODE_STATE_ENTERING;
static ClubCodePhase f_ClubTournamentPhase = CLUB_CODE_PHASE_TYPING;
static s32 f_DecodedNameIndex = 0;
static s32 f_UnusedDoubledHandReach = 0;
static s32 f_UnusedCodeEntryWord = 0;
static u8 f_ClubWindowChoice = CLUB_CHOICE_YES;
static u8 f_CodeKeys[CODE_KEYBOARD_ROW_COUNT][CODE_KEYBOARD_COLUMN_COUNT] = {
    {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'},
    {'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J'},
    {'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T'},
    {'U', 'V', 'W', 'X', 'Y', 'Z', CODE_KEY_BLANK, CODE_KEY_BLANK, CODE_KEY_BACK, CODE_KEY_END},
};
static s8 f_CodeKeyGlyphs[CODE_KEYBOARD_ROW_COUNT][CODE_KEYBOARD_COLUMN_COUNT] = {
    {0, 1, 2, 3, 4, 5, 6, 7, 8, 9},
    {10, 11, 12, 13, 14, 15, 16, 17, 18, 19},
    {20, 21, 22, 23, 24, 25, 26, 27, 28, 29},
    {30, 31, 32, 33, 34, 35, CODE_GLYPH_NONE, CODE_GLYPH_NONE, 36, 37},
};
static u8 f_EnteredCode[CLUB_TOURNAMENT_CODE_LENGTH] = {
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
    CLUB_CODE_NO_CHARACTER,
};
static u8 f_ClubTournamentCodes[CLUB_TOURNAMENT_CODE_COUNT][CLUB_TOURNAMENT_CODE_LENGTH] = {
    {'0', '0', '0', '0', '0', '0', '0', '0'},
    {'K', 'O', 'D', 'E', 'R', 'A', CLUB_CODE_NO_CHARACTER, CLUB_CODE_NO_CHARACTER},
    {'T', 'A', 'K', 'A', 'M', 'A', 'T', 'U'},
    {'K',
        'A',
        'O',
        'R',
        'U',
        CLUB_CODE_NO_CHARACTER,
        CLUB_CODE_NO_CHARACTER,
        CLUB_CODE_NO_CHARACTER},
    {'M', 'U', 'R', 'A', 'I', 'S', 'H', 'I'},
    {'N',
        'O',
        'R',
        'I',
        'O',
        CLUB_CODE_NO_CHARACTER,
        CLUB_CODE_NO_CHARACTER,
        CLUB_CODE_NO_CHARACTER},
    {'6', '6', '6', '6', '6', '6', '6', '6'},
    {'7', '7', '7', '7', '7', '7', '7', '7'},
    {'8', '8', '8', '8', '8', '8', '8', '8'},
    {'9', '9', '9', '9', '9', '9', '9', '9'},
    {'A', 'A', 'A', 'A', 'A', 'A', 'A', 'A'},
    {'B', 'B', 'B', 'B', 'B', 'B', 'B', 'B'},
    {'C', 'C', 'C', 'C', 'C', 'C', 'C', 'C'},
    {'D', 'D', 'D', 'D', 'D', 'D', 'D', 'D'},
    {'E', 'E', 'E', 'E', 'E', 'E', 'E', 'E'},
    {'F', 'F', 'F', 'F', 'F', 'F', 'F', 'F'},
    {'G', 'G', 'G', 'G', 'G', 'G', 'G', 'G'},
    {'H', 'H', 'H', 'H', 'H', 'H', 'H', 'H'},
    {'I', 'I', 'I', 'I', 'I', 'I', 'I', 'I'},
    {'J', 'J', 'J', 'J', 'J', 'J', 'J', 'J'},
    {'K', 'K', 'K', 'K', 'K', 'K', 'K', 'K'},
    {'L', 'L', 'L', 'L', 'L', 'L', 'L', 'L'},
    {'M', 'M', 'M', 'M', 'M', 'M', 'M', 'M'},
    {'N', 'N', 'N', 'N', 'N', 'N', 'N', 'N'},
    {'O', 'O', 'O', 'O', 'O', 'O', 'O', 'O'},
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
    {'Q', 'Q', 'Q', 'Q', 'Q', 'Q', 'Q', 'Q'},
    {'R', 'R', 'R', 'R', 'R', 'R', 'R', 'R'},
    {'S', 'S', 'S', 'S', 'S', 'S', 'S', 'S'},
    {'T', 'T', 'T', 'T', 'T', 'T', 'T', 'T'},
    {'U', 'U', 'U', 'U', 'U', 'U', 'U', 'U'},
    {'V', 'V', 'V', 'V', 'V', 'V', 'V', 'V'},
    {'W', 'W', 'W', 'W', 'W', 'W', 'W', 'W'},
    {'X', 'X', 'X', 'X', 'X', 'X', 'X', 'X'},
    {'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y', 'Y'},
    {'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z', 'Z'},
};
static s32 f_TournamentCharacterChoices[14] UNUSED = {
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
};
static s32 f_EditedCodeByteIndex = 0;
static Vtx f_CodeCursorVertices[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-48, 48, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-48, 0, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{0, 0, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{0, 48, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
static s32 f_UnusedWordsAfterCursorVertices[2] UNUSED = {0, 0};
static f32 f_CodeCursorPhases[MENU_CURSOR_VERTEX_COUNT] = {0.0f, 2.0f, 4.0f, 6.0f};

void DrawClubWindow(
    Gfx **displayListHead,
    s32 width,
    s32 height,
    s32 centerX,
    s32 centerY,
    s32 okTextShown,
    s32 chosenButton,
    s32 windowStyle
);
void DrawCodePanels(Gfx **displayListHead);
void DrawCodeHand(Gfx **displayListHead);
void DrawCodeKeys(Gfx **displayListHead);
void DrawEnteredCode(Gfx **displayListHead);
void DrawCodePrompt(Gfx **displayListHead);
void DrawCodeCursor(Gfx **displayListHead);
CodeKeyResult PressCodeKey(void);
s32 DeleteCodeCharacter(void);

static Mtx f_CodeCursorProjection;
static Mtx f_CodeCursorTranslation;
static Mtx f_CodeCursorRotation;

static u8 f_UnusedBufferBeforeCursorScale[0x80] UNUSED;

static Mtx f_CodeCursorScale;

void ResetMenuWindow(void) {
    f_MenuWindowState = MENU_WINDOW_OPENING;
    f_MenuWindowScaleX = 0.0f;
    f_MenuWindowScaleY = 0.0f;
    f_YesNoPulse = 0;
    f_YesNoPulseDirection = 0;
}

void ShowMenuWindow(void) {
    f_YesNoPulse = 0;
    f_YesNoPulseDirection = 0;
    f_MenuWindowState = MENU_WINDOW_OPEN;
    f_MenuWindowScaleX = 1.0f;
    f_MenuWindowScaleY = 1.0f;
}

void CloseMenuWindow(void) {
    f_MenuWindowState = MENU_WINDOW_CLOSING;
}

s32 IsMenuWindowOpen(void) {
    MenuWindowCheck check;

    if (f_MenuWindowState == MENU_WINDOW_OPENING) {
        check = MENU_WINDOW_CHECK_MOVING;
    }
    else {
        check = f_MenuWindowState == MENU_WINDOW_OPEN;
    }

    return check;
}

s32 IsMenuWindowClosed(void) {
    MenuWindowCheck check;

    if (f_MenuWindowState == MENU_WINDOW_CLOSING) {
        check = MENU_WINDOW_CHECK_MOVING;
    }
    else {
        check = f_MenuWindowState == MENU_WINDOW_CLOSED;
    }

    return check;
}

void LoadTournamentBuffer(void *buffer, TournamentBuffer bufferKind) {
    EncodedAsset asset;
    AssetFile file;

    GetMenuMemory();
    switch (bufferKind) {
        case TOURNAMENT_BUFFER_WIDE_CLOUD:
            f_WideCloudImage = buffer;
            OpenEncodedAsset(ASSET_WIDE_CLOUD, &asset);
            ReadEncodedAsset(f_WideCloudImage, &asset);
            break;
        case TOURNAMENT_BUFFER_YES_NO:
            f_YesNoImage = buffer;
            OpenEncodedAsset(ASSET_YES_NO, &asset);
            ReadEncodedAsset(f_YesNoImage, &asset);
            break;
        case TOURNAMENT_BUFFER_NINTENDO_BADGE:
            f_NintendoBadge = buffer;
            OpenEncodedAsset(ASSET_NINTENDO_BADGE, &asset);
            ReadEncodedAsset(f_NintendoBadge, &asset);
            break;
        case TOURNAMENT_BUFFER_ICON:
            f_ClubCourseIcon = buffer;
            OpenEncodedAsset(g_TournamentIconAssets[0], &asset);
            ReadEncodedAsset(f_ClubCourseIcon, &asset);
            break;
        case TOURNAMENT_BUFFER_MENU_WINDOW:
            f_MenuWindowImage = buffer;
            OpenEncodedAsset(
                f_MenuWindowImageAssets[ARRAY_COUNT(f_MenuWindowImageAssets) - 1],
                &asset
            );
            ReadEncodedAsset(f_MenuWindowImage, &asset);
            break;
        case TOURNAMENT_BUFFER_ROW_FILE:
            f_MenuWindowPalette = buffer;
            OpenAssetFile(f_MenuWindowPaletteFiles[MENU_WINDOW_PICTURE_COUNT - 1][0], &file);
            ReadAssetFile(f_MenuWindowPalette, (s16)GetAssetFileSize(&file), &file);
            break;
    }

    ResetMenuWindow();
}

const s32 f_UnusedConstantWords[2] = {0, 0};

void LoadClubCourseIcon(s32 imageIndex) {
    EncodedAsset asset;

    if (imageIndex < 0) {
        imageIndex = 0;
    }

    if (imageIndex >= CLUB_COURSE_ICON_LIMIT) {
        imageIndex = CLUB_COURSE_ICON_LIMIT - 1;
    }

    OpenEncodedAsset(g_TournamentIconAssets[imageIndex], &asset);
    ReadEncodedAsset(f_ClubCourseIcon, &asset);
}

void LoadMenuWindowImage(s32 imageIndex) {
    EncodedAsset asset;

    if (imageIndex < 0) {
        imageIndex = 0;
    }

    if (imageIndex >= ARRAY_COUNT(f_MenuWindowImageAssets)) {
        imageIndex = ARRAY_COUNT(f_MenuWindowImageAssets) - 1;
    }

    OpenEncodedAsset(f_MenuWindowImageAssets[imageIndex], &asset);
    ReadEncodedAsset(f_MenuWindowImage, &asset);
}

void LoadMenuWindowPalette(s32 row, s32 column) {
    AssetFile file;

    if (row < 0) {
        row = 0;
    }

    if (row >= MENU_WINDOW_PICTURE_COUNT) {
        row = MENU_WINDOW_PICTURE_COUNT - 1;
    }

    if (column < 0) {
        column = 0;
    }

    if (column >= MENU_WINDOW_PALETTE_COUNT) {
        column = MENU_WINDOW_PALETTE_COUNT - 1;
    }

    OpenAssetFile(f_MenuWindowPaletteFiles[row][column], &file);
    ReadAssetFile(f_MenuWindowPalette, (s16)GetAssetFileSize(&file), &file);
}

void GetSaveDataAndMenuMemory(void) {
    GetSaveData();
    GetMenuMemory();
}

#define MENU_WINDOW_SCALE_STEP 0.4f
#define MENU_WINDOW_CLOSED_SCALE 0.1f
#define MENU_WINDOW_STRIP_COUNT 2
#define MENU_WINDOW_IMAGE_WIDTH 256
#define MENU_WINDOW_STRIP_HEIGHT 32
#define YES_NO_BUTTON_COUNT 2
#define YES_NO_BUTTON_SHOWN_WIDTH 48.0f
#define YES_NO_BUTTON_SHOWN_HEIGHT 16.0f
#define YES_NO_BUTTON_OFFSET 30.0f
#define YES_NO_PULSE_STEP 2
#define YES_NO_PULSE_LIMIT 16
#define YES_NO_PULSE_GROWING 0
#define YES_NO_PULSE_SHRINKING 1
#define MESSAGE_OK 0x16B
#define OK_TEXT_OFFSET_X 8
#define OK_TEXT_OFFSET_Y 30
#define OK_TEXT_SLOT 10
#define OK_TEXT_RED 95
#define OK_TEXT_GREEN 63
#define OK_TEXT_BLUE 255
#define OK_TEXT_ALPHA 255

void DrawYesNoWindow(
    Gfx **displayListHead,
    s32 width,
    s32 height,
    s32 centerX,
    s32 centerY,
    s32 okTextShown,
    s32 chosenButton
) {
    Gfx *gfx = *displayListHead;
    f32 windowWidth;
    f32 windowHalfHeight;
    f32 buttonWidth;
    f32 buttonHeight;
    s32 halfWidth;
    s32 halfHeight;
    s32 buttonHalfWidth;
    s32 buttonHalfHeight;
    s16 index;
    s32 buttonX;
    s32 buttonY;
    s32 pulse;
    s32 growth;

    GetMenuMemory();
    switch (f_MenuWindowState) {
        case MENU_WINDOW_OPENING:
            f_MenuWindowScaleX += MENU_WINDOW_SCALE_STEP;
            f_MenuWindowScaleY += MENU_WINDOW_SCALE_STEP;
            if (f_MenuWindowScaleX > 1.0f) {
                f_MenuWindowScaleX = 1.0f;
            }

            if (f_MenuWindowScaleY > 1.0f) {
                f_MenuWindowScaleY = 1.0f;
            }

            windowWidth = width * f_MenuWindowScaleX;
            windowHalfHeight = (height / 2) * f_MenuWindowScaleY;
            buttonWidth = f_MenuWindowScaleX * YES_NO_BUTTON_SHOWN_WIDTH;
            buttonHeight = f_MenuWindowScaleY * YES_NO_BUTTON_SHOWN_HEIGHT;
            if (f_MenuWindowScaleX == 1.0f && f_MenuWindowScaleY == 1.0f) {
                f_MenuWindowState = MENU_WINDOW_OPEN;
            }

            break;
        case MENU_WINDOW_OPEN:
            windowWidth = width;
            windowHalfHeight = height / 2;
            buttonWidth = YES_NO_BUTTON_SHOWN_WIDTH;
            buttonHeight = YES_NO_BUTTON_SHOWN_HEIGHT;
            break;
        case MENU_WINDOW_CLOSING:
            f_MenuWindowScaleX -= MENU_WINDOW_SCALE_STEP;
            f_MenuWindowScaleY -= MENU_WINDOW_SCALE_STEP;
            if (f_MenuWindowScaleX < MENU_WINDOW_SCALE_STEP) {
                f_MenuWindowScaleX = MENU_WINDOW_CLOSED_SCALE;
            }

            if (f_MenuWindowScaleY < MENU_WINDOW_SCALE_STEP) {
                f_MenuWindowScaleY = MENU_WINDOW_CLOSED_SCALE;
            }

            windowWidth = width * f_MenuWindowScaleX;
            windowHalfHeight = (height / 2) * f_MenuWindowScaleY;
            buttonWidth = f_MenuWindowScaleX * YES_NO_BUTTON_SHOWN_WIDTH;
            buttonHeight = f_MenuWindowScaleY * YES_NO_BUTTON_SHOWN_HEIGHT;
            if (f_MenuWindowScaleX <= MENU_WINDOW_CLOSED_SCALE &&
                f_MenuWindowScaleY <= MENU_WINDOW_CLOSED_SCALE) {
                f_MenuWindowState = MENU_WINDOW_CLOSED;
            }

            break;
        default:
            return;
    }

    halfWidth = (s32)windowWidth / 2;
    halfHeight = windowHalfHeight;
    buttonHalfWidth = (s32)buttonWidth / 2;
    buttonHalfHeight = (s32)buttonHeight / 2;
    halfWidth = (halfWidth <= 0) ? 1 : halfWidth;
    halfHeight = (halfHeight <= 0) ? 1 : halfHeight;
    buttonHalfWidth = (buttonHalfWidth <= 0) ? 1 : buttonHalfWidth;
    buttonHalfHeight = (buttonHalfHeight <= 0) ? 1 : buttonHalfHeight;

    windowWidth = (windowWidth <= 0.0f) ? 1.0f : windowWidth;
    windowHalfHeight = (windowHalfHeight <= 0.0f) ? 1.0f : windowHalfHeight;
    buttonWidth = (buttonWidth <= 0.0f) ? 1.0f : buttonWidth;
    buttonHeight = (buttonHeight <= 0.0f) ? 1.0f : buttonHeight;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 191, 191, 191, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);
    for (index = 0; index != MENU_WINDOW_STRIP_COUNT; index++) {
        LoadNibbleTextureBlock(
            &gfx,
            ImageBody(f_WideCloudImage) +
                (index * MENU_WINDOW_IMAGE_WIDTH * MENU_WINDOW_STRIP_HEIGHT / 2),
            G_IM_FMT_I,
            MENU_WINDOW_IMAGE_WIDTH,
            MENU_WINDOW_STRIP_HEIGHT,
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
            QuarterPixels(centerX - halfWidth),
            QuarterPixels(centerY - halfHeight + (index * halfHeight)),
            QuarterPixels(centerX + halfWidth),
            QuarterPixels(centerY - halfHeight + (index * halfHeight) + halfHeight),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(MENU_WINDOW_IMAGE_WIDTH, (s32)windowWidth),
            TexelRunStep(MENU_WINDOW_STRIP_HEIGHT, (s32)windowHalfHeight)
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    LoadLargePalette(&gfx, ImageBody(f_YesNoImage));
    for (index = 0; index != YES_NO_BUTTON_COUNT; index++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_YesNoImage) + (index * YES_NO_BUTTON_WIDTH * YES_NO_BUTTON_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            YES_NO_BUTTON_WIDTH,
            YES_NO_BUTTON_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        if (index == 0) {
            buttonX = centerX - (s32)(f_MenuWindowScaleX * YES_NO_BUTTON_OFFSET);
        }
        else {
            buttonX = centerX + (s32)(f_MenuWindowScaleX * YES_NO_BUTTON_OFFSET);
        }

        buttonY = centerY + windowHalfHeight - (s32)(f_MenuWindowScaleY * YES_NO_BUTTON_OFFSET);
        growth = 0;
        if (index == chosenButton) {
            pulse = f_YesNoPulse;
            growth = pulse / 2;
        }
        else {
            pulse = 0;
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(buttonX - buttonHalfWidth - growth),
            QuarterPixels(buttonY - buttonHalfHeight - growth),
            QuarterPixels(buttonX + buttonHalfWidth + growth),
            QuarterPixels(buttonY + buttonHalfHeight + growth),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(YES_NO_BUTTON_WIDTH, (s32)(buttonWidth + pulse)),
            TexelRunStep(YES_NO_BUTTON_HEIGHT, (s32)(buttonHeight + pulse))
        );
        gDPPipeSync(gfx++);
    }

    if (f_YesNoPulseDirection == YES_NO_PULSE_GROWING) {
        f_YesNoPulse += YES_NO_PULSE_STEP;
    }

    if (f_YesNoPulseDirection == YES_NO_PULSE_SHRINKING) {
        f_YesNoPulse -= YES_NO_PULSE_STEP;
    }

    if (f_YesNoPulse == YES_NO_PULSE_LIMIT) {
        f_YesNoPulseDirection = YES_NO_PULSE_SHRINKING;
    }

    if (f_YesNoPulse == 0) {
        f_YesNoPulseDirection = YES_NO_PULSE_GROWING;
    }

    if (okTextShown != NO_OK_TEXT && IsMenuWindowOpen()) {
        SetTextSlot(
            centerX + OK_TEXT_OFFSET_X,
            buttonY - OK_TEXT_OFFSET_Y,
            MESSAGE_OK,
            TEXT_SLOT_LARGE_FONT,
            OK_TEXT_SLOT,
            TEXT_FLAG_CENTERED,
            OK_TEXT_RED,
            OK_TEXT_GREEN,
            OK_TEXT_BLUE,
            OK_TEXT_ALPHA
        );
    }

    *displayListHead = gfx;
}

#define NINTENDO_BADGE_OFFSET 26.0f
#define WINDOW_PICTURE_SIZE 32
#define WINDOW_IMAGE_OFFSET_X 40.0f
#define WINDOW_IMAGE_OFFSET_Y 50.0f

void DrawClubWindow(
    Gfx **displayListHead,
    s32 width,
    s32 height,
    s32 centerX,
    s32 centerY,
    s32 okTextShown UNUSED,
    s32 chosenButton,
    s32 windowStyle
) {
    Gfx *gfx = *displayListHead;
    f32 windowWidth;
    f32 windowLeft;
    f32 windowHalfHeight;
    f32 buttonWidth;
    f32 buttonHeight;
    f32 badgeWidth;
    f32 badgeStripHeight;
    f32 courseWidth;
    f32 courseStripHeight;
    f32 pictureWidth;
    f32 pictureHeight;
    s32 halfWidth;
    s32 halfHeight;
    s32 buttonHalfWidth;
    s32 buttonHalfHeight;
    s32 badgeHalfWidth;
    s32 badgeStripRows;
    s32 courseHalfWidth;
    s32 courseStripRows;
    s32 pictureHalfWidth;
    s32 pictureHalfHeight;
    s16 index;
    s32 buttonX;
    s32 buttonY;
    s32 pulse;
    s32 growth;
    s32 badgeX;
    s32 badgeTop;
    s32 courseX;
    s32 courseTop;
    s32 pictureX;
    s32 pictureY;

    GetMenuMemory();
    switch (f_MenuWindowState) {
        case MENU_WINDOW_OPENING:
            f_MenuWindowScaleX += MENU_WINDOW_SCALE_STEP;
            f_MenuWindowScaleY += MENU_WINDOW_SCALE_STEP;
            if (f_MenuWindowScaleX > 1.0f) {
                f_MenuWindowScaleX = 1.0f;
            }

            if (f_MenuWindowScaleY > 1.0f) {
                f_MenuWindowScaleY = 1.0f;
            }

            windowWidth = width * f_MenuWindowScaleX;
            windowHalfHeight = (height / 2) * f_MenuWindowScaleY;
            buttonWidth = f_MenuWindowScaleX * YES_NO_BUTTON_SHOWN_WIDTH;
            buttonHeight = f_MenuWindowScaleY * YES_NO_BUTTON_SHOWN_HEIGHT;
            badgeWidth = f_MenuWindowScaleX * NINTENDO_BADGE_WIDTH;
            badgeStripHeight = f_MenuWindowScaleY * NINTENDO_BADGE_STRIP_HEIGHT;
            courseWidth = f_MenuWindowScaleX * COURSE_ICON_WIDTH;
            courseStripHeight = f_MenuWindowScaleY * COURSE_ICON_STRIP_HEIGHT;
            pictureWidth = f_MenuWindowScaleX * WINDOW_PICTURE_SIZE;
            pictureHeight = f_MenuWindowScaleY * WINDOW_PICTURE_SIZE;
            if (f_MenuWindowScaleX == 1.0f && f_MenuWindowScaleY == 1.0f) {
                f_MenuWindowState = MENU_WINDOW_OPEN;
            }

            break;
        case MENU_WINDOW_OPEN:
            windowWidth = width;
            windowHalfHeight = height / 2;
            buttonWidth = YES_NO_BUTTON_SHOWN_WIDTH;
            buttonHeight = YES_NO_BUTTON_SHOWN_HEIGHT;
            badgeWidth = NINTENDO_BADGE_WIDTH;
            badgeStripHeight = NINTENDO_BADGE_STRIP_HEIGHT;
            courseWidth = COURSE_ICON_WIDTH;
            courseStripHeight = COURSE_ICON_STRIP_HEIGHT;
            pictureWidth = WINDOW_PICTURE_SIZE;
            pictureHeight = WINDOW_PICTURE_SIZE;
            break;
        case MENU_WINDOW_CLOSING:
            f_MenuWindowScaleX -= MENU_WINDOW_SCALE_STEP;
            f_MenuWindowScaleY -= MENU_WINDOW_SCALE_STEP;
            if (f_MenuWindowScaleX < MENU_WINDOW_SCALE_STEP) {
                f_MenuWindowScaleX = MENU_WINDOW_CLOSED_SCALE;
            }

            if (f_MenuWindowScaleY < MENU_WINDOW_SCALE_STEP) {
                f_MenuWindowScaleY = MENU_WINDOW_CLOSED_SCALE;
            }

            windowWidth = width * f_MenuWindowScaleX;
            windowHalfHeight = (height / 2) * f_MenuWindowScaleY;
            buttonWidth = f_MenuWindowScaleX * YES_NO_BUTTON_SHOWN_WIDTH;
            buttonHeight = f_MenuWindowScaleY * YES_NO_BUTTON_SHOWN_HEIGHT;
            badgeWidth = f_MenuWindowScaleX * NINTENDO_BADGE_WIDTH;
            badgeStripHeight = f_MenuWindowScaleY * NINTENDO_BADGE_STRIP_HEIGHT;
            courseWidth = f_MenuWindowScaleX * COURSE_ICON_WIDTH;
            courseStripHeight = f_MenuWindowScaleY * COURSE_ICON_STRIP_HEIGHT;
            pictureWidth = f_MenuWindowScaleX * WINDOW_PICTURE_SIZE;
            pictureHeight = f_MenuWindowScaleY * WINDOW_PICTURE_SIZE;
            if (f_MenuWindowScaleX <= MENU_WINDOW_CLOSED_SCALE &&
                f_MenuWindowScaleY <= MENU_WINDOW_CLOSED_SCALE) {
                f_MenuWindowState = MENU_WINDOW_CLOSED;
            }

            break;
        default:
            return;
    }

    halfWidth = (s32)windowWidth / 2;
    halfHeight = windowHalfHeight;
    buttonHalfWidth = (s32)buttonWidth / 2;
    buttonHalfHeight = (s32)buttonHeight / 2;
    badgeHalfWidth = (s32)badgeWidth / 2;
    badgeStripRows = badgeStripHeight;
    courseHalfWidth = (s32)courseWidth / 2;
    courseStripRows = courseStripHeight;
    pictureHalfWidth = (s32)pictureWidth / 2;
    pictureHalfHeight = (s32)pictureHeight / 2;

    halfWidth = (halfWidth <= 0) ? 1 : halfWidth;
    halfHeight = (halfHeight <= 0) ? 1 : halfHeight;
    buttonHalfWidth = (buttonHalfWidth <= 0) ? 1 : buttonHalfWidth;
    buttonHalfHeight = (buttonHalfHeight <= 0) ? 1 : buttonHalfHeight;
    badgeHalfWidth = (badgeHalfWidth <= 0) ? 1 : badgeHalfWidth;
    badgeStripRows = (badgeStripRows <= 0) ? 1 : badgeStripRows;
    courseHalfWidth = (courseHalfWidth <= 0) ? 1 : courseHalfWidth;
    courseStripRows = (courseStripRows <= 0) ? 1 : courseStripRows;
    pictureHalfWidth = (pictureHalfWidth <= 0) ? 1 : pictureHalfWidth;
    pictureHalfHeight = (pictureHalfHeight <= 0) ? 1 : pictureHalfHeight;

    windowWidth = (windowWidth <= 0.0f) ? 1.0f : windowWidth;
    windowHalfHeight = (windowHalfHeight <= 0.0f) ? 1.0f : windowHalfHeight;
    buttonWidth = (buttonWidth <= 0.0f) ? 1.0f : buttonWidth;
    buttonHeight = (buttonHeight <= 0.0f) ? 1.0f : buttonHeight;
    badgeWidth = (badgeWidth <= 0.0f) ? 1.0f : badgeWidth;
    badgeStripHeight = (badgeStripHeight <= 0.0f) ? 1.0f : badgeStripHeight;
    courseWidth = (courseWidth <= 0.0f) ? 1.0f : courseWidth;
    courseStripHeight = (courseStripHeight <= 0.0f) ? 1.0f : courseStripHeight;
    pictureWidth = (pictureWidth <= 0.0f) ? 1.0f : pictureWidth;
    pictureHeight = (pictureHeight <= 0.0f) ? 1.0f : pictureHeight;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 191, 191, 191, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);
    for (index = 0; index != MENU_WINDOW_STRIP_COUNT; index++) {
        LoadNibbleTextureBlock(
            &gfx,
            ImageBody(f_WideCloudImage) +
                (index * MENU_WINDOW_IMAGE_WIDTH * MENU_WINDOW_STRIP_HEIGHT / 2),
            G_IM_FMT_I,
            MENU_WINDOW_IMAGE_WIDTH,
            MENU_WINDOW_STRIP_HEIGHT,
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
            QuarterPixels(centerX - halfWidth),
            QuarterPixels(centerY - halfHeight + (index * halfHeight)),
            QuarterPixels(centerX + halfWidth),
            QuarterPixels(centerY - halfHeight + (index * halfHeight) + halfHeight),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(MENU_WINDOW_IMAGE_WIDTH, (s32)windowWidth),
            TexelRunStep(MENU_WINDOW_STRIP_HEIGHT, (s32)windowHalfHeight)
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    LoadLargePalette(&gfx, ImageBody(f_YesNoImage));
    for (index = 0; index != YES_NO_BUTTON_COUNT; index++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_YesNoImage) + (index * YES_NO_BUTTON_WIDTH * YES_NO_BUTTON_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            YES_NO_BUTTON_WIDTH,
            YES_NO_BUTTON_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        if (index == 0) {
            buttonX = centerX - (s32)(f_MenuWindowScaleX * YES_NO_BUTTON_OFFSET);
        }
        else {
            buttonX = centerX + (s32)(f_MenuWindowScaleX * YES_NO_BUTTON_OFFSET);
        }

        buttonY = centerY + windowHalfHeight - (s32)(f_MenuWindowScaleY * YES_NO_BUTTON_OFFSET);
        growth = 0;
        if (index == chosenButton) {
            pulse = f_YesNoPulse;
            growth = pulse / 2;
        }
        else {
            pulse = 0;
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(buttonX - buttonHalfWidth - growth),
            QuarterPixels(buttonY - buttonHalfHeight - growth),
            QuarterPixels(buttonX + buttonHalfWidth + growth),
            QuarterPixels(buttonY + buttonHalfHeight + growth),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(YES_NO_BUTTON_WIDTH, (s32)(buttonWidth + pulse)),
            TexelRunStep(YES_NO_BUTTON_HEIGHT, (s32)(buttonHeight + pulse))
        );
        gDPPipeSync(gfx++);
    }

    if (f_YesNoPulseDirection == YES_NO_PULSE_GROWING) {
        f_YesNoPulse += YES_NO_PULSE_STEP;
    }

    if (f_YesNoPulseDirection == YES_NO_PULSE_SHRINKING) {
        f_YesNoPulse -= YES_NO_PULSE_STEP;
    }

    if (f_YesNoPulse == YES_NO_PULSE_LIMIT) {
        f_YesNoPulseDirection = YES_NO_PULSE_SHRINKING;
    }

    if (f_YesNoPulse == 0) {
        f_YesNoPulseDirection = YES_NO_PULSE_GROWING;
    }

    if (windowStyle == CLUB_WINDOW_STYLE_NINTENDO ||
        windowStyle == (CLUB_WINDOW_STYLE_PICTURE | CLUB_WINDOW_STYLE_NINTENDO)) {
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        LoadLargePalette(&gfx, ImageBody(f_NintendoBadge));
        badgeX = centerX;
        badgeTop =
            (s32)(centerY - windowHalfHeight + (s32)(f_MenuWindowScaleY * NINTENDO_BADGE_OFFSET)) -
            badgeStripRows;
        for (index = 0; index != NINTENDO_BADGE_STRIP_COUNT; index++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NintendoBadge) +
                    (index * NINTENDO_BADGE_WIDTH * NINTENDO_BADGE_STRIP_HEIGHT),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NINTENDO_BADGE_WIDTH,
                NINTENDO_BADGE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(badgeX - badgeHalfWidth),
                QuarterPixels(badgeTop + (index * badgeStripRows)),
                QuarterPixels(badgeX + badgeHalfWidth),
                QuarterPixels(badgeTop + (index * badgeStripRows) + badgeStripRows),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NINTENDO_BADGE_WIDTH, (s32)badgeWidth),
                TexelRunStep(NINTENDO_BADGE_STRIP_HEIGHT, (s32)badgeStripHeight)
            );
            gDPPipeSync(gfx++);
        }
    }

    windowLeft = centerX - (windowWidth * 0.5f);
    courseTop =
        (s32)(centerY + windowHalfHeight - (s32)(f_MenuWindowScaleY * WINDOW_IMAGE_OFFSET_Y)) -
        courseStripRows;
    courseX = windowLeft + (s32)(f_MenuWindowScaleX * WINDOW_IMAGE_OFFSET_X);
    LoadLargePalette(&gfx, ImageBody(f_ClubCourseIcon));
    for (index = 0; index != COURSE_ICON_STRIP_COUNT; index++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_ClubCourseIcon) +
                (index * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            COURSE_ICON_WIDTH,
            COURSE_ICON_STRIP_HEIGHT,
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
            QuarterPixels(courseX - courseHalfWidth),
            QuarterPixels(courseTop + (index * courseStripRows)),
            QuarterPixels(courseX + courseHalfWidth),
            QuarterPixels(courseTop + (index * courseStripRows) + courseStripRows),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(COURSE_ICON_WIDTH, (s32)courseWidth),
            TexelRunStep(COURSE_ICON_STRIP_HEIGHT, (s32)courseStripHeight)
        );
        gDPPipeSync(gfx++);
    }

    pictureX = centerX + (windowWidth * 0.5f) - (s32)(f_MenuWindowScaleX * WINDOW_IMAGE_OFFSET_X);
    pictureY = centerY + windowHalfHeight - (s32)(f_MenuWindowScaleY + WINDOW_IMAGE_OFFSET_Y);

    LoadLargePalette(&gfx, f_MenuWindowPalette);
    LoadTextureBlock(
        &gfx,
        f_MenuWindowImage,
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        WINDOW_PICTURE_SIZE,
        WINDOW_PICTURE_SIZE,
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
        QuarterPixels(pictureX - pictureHalfWidth),
        QuarterPixels(pictureY - pictureHalfHeight),
        QuarterPixels(pictureX + pictureHalfWidth),
        QuarterPixels(pictureY + pictureHalfHeight),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(WINDOW_PICTURE_SIZE, (s32)pictureWidth),
        TexelRunStep(WINDOW_PICTURE_SIZE, (s32)pictureHeight)
    );
    gDPPipeSync(gfx++);

    if (IsMenuWindowOpen()) {
        SetTextSlot(
            centerX + OK_TEXT_OFFSET_X,
            buttonY - OK_TEXT_OFFSET_Y,
            MESSAGE_OK,
            TEXT_SLOT_LARGE_FONT,
            OK_TEXT_SLOT,
            TEXT_FLAG_CENTERED,
            OK_TEXT_RED,
            OK_TEXT_GREEN,
            OK_TEXT_BLUE,
            OK_TEXT_ALPHA
        );
    }

    *displayListHead = gfx;
}

void DrawUnusedWideCloud(
    Gfx **displayListHead,
    s32 centerX,
    s32 centerY,
    s32 halfWidth,
    s32 stripHeight,
    s32 width,
    s32 shownStripHeight
) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 filterOrHalfWidth;
    s32 stripTop = centerY - stripHeight;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    filterOrHalfWidth = G_TF_BILERP;
    gDPSetTextureFilter(gfx++, filterOrHalfWidth);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetEnvColor(gfx++, 191, 191, 191, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
    gDPSetCombineMode(gfx++, G_CC_BLENDPEDECALA, G_CC_BLENDPEDECALA);
    for (stripIndex = 0; stripIndex != TOURNAMENT_BANNER_STRIP_COUNT; stripIndex++) {
        LoadNibbleTextureBlock(
            &gfx,
            ImageBody(f_WideCloudImage) +
                (stripIndex * TOURNAMENT_BANNER_WIDTH * TOURNAMENT_BANNER_STRIP_HEIGHT / 2),
            G_IM_FMT_I,
            TOURNAMENT_BANNER_WIDTH,
            TOURNAMENT_BANNER_STRIP_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        filterOrHalfWidth = halfWidth;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(centerX - halfWidth),
            QuarterPixels(stripTop),
            QuarterPixels(centerX + filterOrHalfWidth),
            QuarterPixels(stripTop + stripHeight),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(TOURNAMENT_BANNER_WIDTH, width),
            TexelRunStep(TOURNAMENT_BANNER_STRIP_HEIGHT, shownStripHeight)
        );
        stripTop += stripHeight;
        gDPPipeSync(gfx++);
    }
}

void LoadCodeEntryBuffer(void *buffer, CodeEntryBuffer bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case CODE_ENTRY_BUFFER_NAME_PLATE:
            f_NamePlateImage = buffer;
            OpenEncodedAsset(ASSET_NAME_PLATE, &asset);
            ReadEncodedAsset(f_NamePlateImage, &asset);
            break;
        case CODE_ENTRY_BUFFER_BAR:
            f_CodeEntryBarImage = buffer;
            OpenEncodedAsset(ASSET_CODE_ENTRY_BAR, &asset);
            ReadEncodedAsset(f_CodeEntryBarImage, &asset);
            break;
        case CODE_ENTRY_BUFFER_GLYPHS:
            f_CodeGlyphsImage = buffer;
            OpenEncodedAsset(ASSET_CODE_ENTRY_GLYPHS, &asset);
            ReadEncodedAsset(f_CodeGlyphsImage, &asset);
            break;
        case CODE_ENTRY_BUFFER_POINTING_HAND:
            f_PointingHandImage = buffer;
            OpenEncodedAsset(ASSET_POINTING_HAND, &asset);
            ReadEncodedAsset(f_PointingHandImage, &asset);
            break;
        case CODE_ENTRY_BUFFER_CURSOR_BOX:
            f_CodeCursorBox = buffer;
            OpenEncodedAsset(ASSET_CURSOR_BOX, &asset);
            ReadEncodedAsset(f_CodeCursorBox, &asset);
            break;
    }
}

void ResetCodeEntry(void) {
    MenuMemory *memory = GetMenuMemory();
    s32 codeLetterIndex;

    memory->codeKeyColumn = 0;
    memory->codeKeyRow = 0;
    f_DecodedNameIndex = 0;
    for (codeLetterIndex = 0; codeLetterIndex != CLUB_TOURNAMENT_CODE_LENGTH; codeLetterIndex++) {
        f_EnteredCode[codeLetterIndex] = CLUB_CODE_NO_CHARACTER;
    }

    f_EnteredCodeLength = 0;
    f_ClubCodeState = CLUB_CODE_STATE_ENTERING;
    f_CodeHandMotion = 0;
    f_CodeHandReach = 0;
    f_CodeHandStepTimer = 0;
    f_UnusedDoubledHandReach = 0;
    f_ClubTournamentPhase = 0;
    f_ClubWindowChoice = 0;
    f_IsClubTournamentEntered = TRUE;
    f_UnusedCodeEntryWord = 0;
}

void UpdateClubWindow(void);

void UpdateClubTournament(void) {
    MenuMemory *memory = GetMenuMemory();
    s32 index;
    s32 row;
    s32 column;
    CodeKeyResult keyResult;
    Boolean isInvalid;
    TournamentRules *rules;
    char debugText[DEBUG_TEXT_BUFFER_SIZE];

    if (IsHudAnimationDone() != TRUE) {
        return;
    }

    if (f_ClubTournamentPhase == CLUB_CODE_PHASE_INCORRECT) {
        if (!IsTextSlotRevealed(0)) {
            return;
        }

        if (g_MenuRepeatButtons & A_BUTTON) {
            f_ClubTournamentPhase = CLUB_CODE_PHASE_TYPING;
            f_ClubCodeState = CLUB_CODE_STATE_ENTERING;
            ResetCodeEntry();
        }
        else if (g_MenuRepeatButtons & B_BUTTON) {
            f_ClubTournamentPhase = CLUB_CODE_PHASE_TYPING;
            f_ClubCodeState = CLUB_CODE_STATE_ENTERING;
        }

        return;
    }

    if (f_ClubTournamentPhase != CLUB_CODE_PHASE_TYPING) {
        UpdateClubWindow();
        return;
    }

    if (IsGameFlagSet(GAME_FLAG_EDIT_TOURNAMENT_CODE)) {
        if (g_MenuControllerData[1].button & L_JPAD) {
            f_EditedCodeByteIndex =
                (f_EditedCodeByteIndex + TOURNAMENT_CODE_SIZE - 1) % TOURNAMENT_CODE_SIZE;
        }

        if (g_MenuControllerData[1].button & R_JPAD) {
            f_EditedCodeByteIndex = (f_EditedCodeByteIndex + 1) % TOURNAMENT_CODE_SIZE;
        }

        if (g_MenuControllerData[1].button & U_JPAD) {
            g_TournamentCode[f_EditedCodeByteIndex]++;
        }

        g_TournamentCode[f_EditedCodeByteIndex]++;
        if (g_MenuControllerData[1].button & D_JPAD) {
            g_TournamentCode[f_EditedCodeByteIndex]--;
        }

        g_TournamentCode[f_EditedCodeByteIndex]--;
        for (index = 0; index < TOURNAMENT_CODE_SIZE; index++) {
            if (f_EditedCodeByteIndex == index) {
                SetDebugTextHighlight(TRUE);
            }

            sprintf(debugText, "P%d[%d]  ", index, g_TournamentCode[index]);
            PrintDebugText(debugText, CODE_DEBUG_TEXT_COLUMN, index + CODE_DEBUG_TEXT_FIRST_ROW);
        }
    }

    if (g_MenuRepeatButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        keyResult = PressCodeKey();
        if (keyResult == CODE_KEY_RESULT_DONE) {
            if (IsGameFlagSet(GAME_FLAG_EDIT_TOURNAMENT_CODE)) {
                isInvalid = FALSE;
            }
            else {
                isInvalid = DecodeTournamentPassword(g_TournamentCode, (char *)f_EnteredCode);
            }

            if (!isInvalid) {
                f_DecodedNameIndex = g_TournamentCode[0] & CLUB_CODE_NAME_MASK;
                if (f_DecodedNameIndex >= CLUB_TOURNAMENT_NAME_COUNT) {
                    f_DecodedNameIndex = 0;
                }

                g_IsNintendoClubTournament = g_TournamentCode[0] >> CLUB_CODE_NINTENDO_SHIFT;
                g_ClubTournamentNameIndex = f_DecodedNameIndex;
                rules = GetTournamentRules(g_TournamentCode[1]);
                g_ClubTournamentRules = *rules;
                g_ClubTournamentNumber = g_TournamentCode[2] + 1;
                osSyncPrintf(
                    "bkTour %x, %x, %x\n",
                    g_TournamentCode[0],
                    g_TournamentCode[1],
                    g_TournamentCode[2]
                );
                PlaySound(SOUND_GO, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                ResetMenuWindow();
                LoadClubCourseIcon(GetClubTournamentCourse());
                f_ClubTournamentPhase = CLUB_CODE_PHASE_DETAILS;
                if (GetClubTournamentCharacter() >= 0) {
                    LoadMenuWindowImage(GetClubTournamentCharacter());
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 0);
                }
                else {
                    LoadMenuWindowImage(CHARACTER_KID);
                    LoadMenuWindowPalette(CHARACTER_KID, 0);
                }

                return;
            }

            PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            f_ClubTournamentPhase = CLUB_CODE_PHASE_INCORRECT;
            f_ClubCodeState = CLUB_CODE_STATE_INCORRECT;
        }
        else if (keyResult == CODE_KEY_RESULT_TOO_SHORT) {
            PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            f_ClubTournamentPhase = CLUB_CODE_PHASE_INCORRECT;
            f_ClubCodeState = CLUB_CODE_STATE_INCORRECT;
        }
    }
    else if (g_MenuRepeatButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);

        if (DeleteCodeCharacter()) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            g_MenuScreen = MENU_SCREEN_MAIN;
            f_IsClubTournamentEntered = FALSE;
            g_IsNintendoClubTournament = FALSE;
            for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    0,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
            }
        }
    }
    else if (g_MenuRepeatButtons & U_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        memory->codeKeyRow--;
        if (memory->codeKeyRow < 0) {
            memory->codeKeyRow = CODE_KEYBOARD_ROW_COUNT - 1;
        }

        row = memory->codeKeyRow;
        column = memory->codeKeyColumn;
        if (f_CodeKeys[row][column] == CODE_KEY_BLANK) {
            memory->codeKeyRow = CODE_KEYBOARD_END_ROW - 1;
        }
    }
    else if (g_MenuRepeatButtons & D_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        row = ++memory->codeKeyRow;
        column = memory->codeKeyColumn;
        if (f_CodeKeys[row][column] == CODE_KEY_BLANK) {
            memory->codeKeyRow = 0;
        }

        if (memory->codeKeyRow >= CODE_KEYBOARD_ROW_COUNT) {
            memory->codeKeyRow = 0;
        }
    }
    else if (g_MenuRepeatButtons & L_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        row = memory->codeKeyRow;
        column = --memory->codeKeyColumn;
        if (f_CodeKeys[row][column] == CODE_KEY_BLANK) {
            memory->codeKeyColumn = CODE_KEYBOARD_FIRST_BLANK_COLUMN - 1;
        }

        if (memory->codeKeyColumn < 0) {
            memory->codeKeyColumn = CODE_KEYBOARD_COLUMN_COUNT - 1;
        }
    }
    else if (g_MenuRepeatButtons & R_JPAD) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        row = memory->codeKeyRow;
        column = ++memory->codeKeyColumn;
        if (f_CodeKeys[row][column] == CODE_KEY_BLANK) {
            memory->codeKeyColumn = CODE_KEYBOARD_BACK_COLUMN;
        }

        if (memory->codeKeyColumn >= CODE_KEYBOARD_COLUMN_COUNT) {
            memory->codeKeyColumn = 0;
        }
    }
}

s32 IsClubCharacterChoosable(void);

void UpdateClubWindow(void) {
    Boolean isHudAnimationDone;
    s32 index;
    u16 buttons;
    s32 pressedButtons;

    GetMenuMemory();
    isHudAnimationDone = IsHudAnimationDone();
    if (isHudAnimationDone != TRUE) {
        return;
    }

    if (GetWipeKind() != WIPE_NONE) {
        for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        if (GetWipeProgress() == WIPE_CLOSED_PROGRESS) {
            g_GameState.roundSeed =
                (g_TournamentCode[0] << 16) | (g_TournamentCode[1] << 8) | g_TournamentCode[2];
            StartLoadingChosenHole();
            g_MenuScreen = MENU_SCREEN_EXIT;
        }

        return;
    }

    if (IsMenuWindowOpen() == MENU_WINDOW_CHECK_MOVING) {
        return;
    }

    if (IsMenuWindowClosed() == MENU_WINDOW_CHECK_MOVING) {
        return;
    }

    if (IsMenuWindowClosed()) {
        f_ClubWindowChoice = CLUB_CHOICE_NO;
        for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }

        f_ClubTournamentPhase = FALSE;
        f_ClubWindowChoice = CLUB_CHOICE_YES;
        return;
    }

    pressedButtons = g_MenuRepeatButtons;
    if (pressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        if (f_ClubWindowChoice == CLUB_CHOICE_YES) {
            PlaySound(SOUND_ENTER_TOURNAMENT, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_GameState.isClubTournament = TRUE;
            g_ChosenPlayerCount = 1;
            if (IsClubCharacterChoosable()) {
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                StartLoadingChosenHole();
                g_MenuScreen = MENU_SCREEN_TOURNAMENT_CHARACTER_SELECT;
            }
            else {
                for (index = 0; index != g_ControllerCount; index++) {
                    if (g_MenuControllerData[index].button & A_BUTTON) {
                        g_PlayerControllerIndices[0] = index;
                        break;
                    }
                }

                StartLoadingChosenHole();
                g_ChoosingPlayerIndex = 1;
                g_GameState.playerCount = 1;
                buttons = g_ControllerStates[0].buttons | g_ControllerStates[1].buttons |
                    g_ControllerStates[2].buttons | g_ControllerStates[3].buttons;
                if (buttons & L_CBUTTONS) {
                    g_Players[0].paletteIndex = LEFT_C_PALETTE;
                }
                else if (buttons & D_CBUTTONS) {
                    g_Players[0].paletteIndex = DOWN_C_PALETTE;
                }
                else if (buttons & R_CBUTTONS) {
                    g_Players[0].paletteIndex = RIGHT_C_PALETTE;
                }
                else {
                    g_Players[0].paletteIndex = UP_C_PALETTE;
                }

                LoadWipeShape(GetClubTournamentCourse());
                StartWipe(WIPE_CLOSING);
            }

            for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    0,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
            }
        }
        else {
            PlaySound(SOUND_READY, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            ResetCodeEntry();
            CloseMenuWindow();
            for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(
                    0,
                    0,
                    NO_MESSAGE,
                    TEXT_SLOT_LARGE_FONT,
                    index,
                    0,
                    255,
                    255,
                    255,
                    OPAQUE_ALPHA
                );
            }
        }
    }
    else if (pressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        f_ClubWindowChoice = CLUB_CHOICE_YES;
        f_ClubTournamentPhase = FALSE;
        for (index = 0; index != CLUB_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(
                0,
                0,
                NO_MESSAGE,
                TEXT_SLOT_LARGE_FONT,
                index,
                0,
                255,
                255,
                255,
                OPAQUE_ALPHA
            );
        }
    }
    else if ((pressedButtons & L_JPAD) || ((u16)pressedButtons & R_JPAD)) {
        PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        f_ClubWindowChoice ^= 1;
    }
}

void DrawClubNumber(Gfx **displayListHead, s32 x, s32 top, s32 number) {
    Gfx *gfx = *displayListHead;
    u8 *image = GetMenuImageBuffer();
    char *digit;
    s32 width;
    PlaceSuffix suffix;
    s32 glyph;
    u8 character;
    s32 lastDigitSuffix;

    sprintf(g_PrintBuffer, "%d", number);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImageBody(image));
    width = 0;
    for (digit = g_PrintBuffer; *digit != '\0'; digit++) {
        width += PLACE_GLYPH_SPACING;
    }

    x -= (PLACE_GLYPH_SIZE / 2) + (width / 2);
    width = 0;
    for (digit = g_PrintBuffer; *digit != '\0'; digit++, width += PLACE_GLYPH_SPACING) {
        character = *digit;
        if (character == ' ') {
            continue;
        }
        glyph = character - '0';
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(LargeImageTexels(image)),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            PLACE_GLYPH_SIZE,
            PLACE_GLYPHS_HEIGHT,
            0,
            glyph * PLACE_GLYPH_SIZE,
            PLACE_GLYPH_SIZE - 1,
            glyph * PLACE_GLYPH_SIZE + PLACE_GLYPH_SIZE - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(x + width),
            QuarterPixels(top),
            QuarterPixels(x + width + PLACE_GLYPH_SIZE),
            QuarterPixels(top + PLACE_GLYPH_SIZE),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(glyph * PLACE_GLYPH_SIZE),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    suffix = number;
    if (suffix > PLACES_PER_HUNDRED) {
        suffix %= PLACES_PER_HUNDRED;
    }

    suffix--;
    if (suffix >= PLACE_SUFFIX_TH) {
        if (suffix < PLACE_TEENS_END) {
            suffix = PLACE_SUFFIX_TH;
        }
        else if (suffix < PLACES_PER_HUNDRED) {
            lastDigitSuffix = suffix % DECIMAL_BASE;
            suffix = PLACE_SUFFIX_TH;
            if (lastDigitSuffix < PLACE_SUFFIX_TH) {
                suffix = lastDigitSuffix;
            }
        }
    }

    gDPLoadTextureTile(
        gfx++,
        TextureAddress(LargeImageTexels(image)),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PLACE_GLYPH_SIZE,
        PLACE_GLYPHS_HEIGHT,
        0,
        (suffix + PLACE_SUFFIX_GLYPH_FIRST) * PLACE_GLYPH_SIZE,
        PLACE_GLYPH_SIZE - 1,
        (suffix + PLACE_SUFFIX_GLYPH_FIRST) * PLACE_GLYPH_SIZE + PLACE_GLYPH_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(x + width),
        QuarterPixels(top),
        QuarterPixels(x + width + PLACE_GLYPH_SIZE),
        QuarterPixels(top + PLACE_GLYPH_SIZE),
        G_TX_RENDERTILE,
        0,
        TexelCoordinate((suffix + PLACE_SUFFIX_GLYPH_FIRST) * PLACE_GLYPH_SIZE),
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    *displayListHead = gfx;
}

void DrawCodeEntryBar(Gfx **displayListHead);

void DrawClubTournament(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 width = 0;
    s32 height = 0;
    s32 windowStyle;
    u16 buttons;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    DrawCodePanels(&gfx);
    DrawCodeEntryBar(&gfx);
    DrawCodeKeys(&gfx);
    DrawEnteredCode(&gfx);
    DrawCodeHand(&gfx);
    DrawCodeCursor(&gfx);
    DrawCodePrompt(&gfx);
    if (f_ClubTournamentPhase == 1) {
        if (IsNintendoClubTournament()) {
            windowStyle = (GetClubTournamentCharacter() != -1) ?
                CLUB_WINDOW_STYLE_PICTURE | CLUB_WINDOW_STYLE_NINTENDO :
                CLUB_WINDOW_STYLE_NINTENDO;
            if (windowStyle == (CLUB_WINDOW_STYLE_PICTURE | CLUB_WINDOW_STYLE_NINTENDO) &&
                GetWipeKind() == WIPE_NONE) {
                buttons = g_ControllerStates[0].buttons | g_ControllerStates[1].buttons |
                    g_ControllerStates[2].buttons | g_ControllerStates[3].buttons;
                if (buttons & L_CBUTTONS) {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 1);
                }
                else if (buttons & D_CBUTTONS) {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 2);
                }
                else if (buttons & R_CBUTTONS) {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 3);
                }
                else {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 0);
                }
            }

            DrawClubWindow(
                &gfx,
                CLUB_WINDOW_WIDTH,
                CLUB_WINDOW_HEIGHT,
                CLUB_WINDOW_CENTER_X,
                CLUB_WINDOW_CENTER_Y,
                0,
                f_ClubWindowChoice,
                windowStyle
            );
            DrawClubNumber(&gfx, CLUB_TEXT_X, CLUB_NUMBER_Y, g_ClubTournamentNumber);
        }
        else {
            windowStyle = (GetClubTournamentCharacter() != -1) ? CLUB_WINDOW_STYLE_PICTURE :
                                                                 CLUB_WINDOW_STYLE_PLAIN;
            if (windowStyle != CLUB_WINDOW_STYLE_PLAIN && GetWipeKind() == WIPE_NONE) {
                buttons = g_ControllerStates[0].buttons | g_ControllerStates[1].buttons |
                    g_ControllerStates[2].buttons | g_ControllerStates[3].buttons;
                if (buttons & L_CBUTTONS) {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 1);
                }
                else if (buttons & D_CBUTTONS) {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 2);
                }
                else if (buttons & R_CBUTTONS) {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 3);
                }
                else {
                    LoadMenuWindowPalette(GetClubTournamentCharacter(), 0);
                }
            }

            DrawClubWindow(
                &gfx,
                CLUB_WINDOW_WIDTH,
                CLUB_WINDOW_HEIGHT,
                CLUB_WINDOW_CENTER_X,
                CLUB_WINDOW_CENTER_Y,
                0,
                f_ClubWindowChoice,
                windowStyle
            );
            DrawClubNumber(&gfx, CLUB_TEXT_X, CLUB_NUMBER_Y, g_ClubTournamentNumber);
        }

        if (IsMenuWindowOpen() != MENU_WINDOW_CHECK_MOVING && !IsMenuWindowClosed()) {
            MeasureMessage(
                g_ClubTournamentNameIndex + MESSAGE_FIRST_CLUB_TOURNAMENT_NAME,
                FONT_SIZE_LARGE,
                &width,
                &height,
                0
            );
            MeasureMessage(
                GetClubTournamentCourseTitleMessage(),
                FONT_SIZE_LARGE,
                &width,
                &height,
                0
            );
            MeasureMessage(GetClubTournamentModeMessage(), FONT_SIZE_LARGE, &width, &height, 0);
            SetTextSlot(
                CLUB_TEXT_X,
                CLUB_NAME_Y,
                g_ClubTournamentNameIndex + MESSAGE_FIRST_CLUB_TOURNAMENT_NAME,
                TEXT_SLOT_LARGE_FONT,
                CLUB_NAME_SLOT,
                TEXT_FLAG_CENTERED,
                95,
                63,
                255,
                OPAQUE_ALPHA
            );
            SetTextSlot(
                CLUB_TEXT_X,
                CLUB_COURSE_Y,
                GetClubTournamentCourseTitleMessage(),
                TEXT_SLOT_LARGE_FONT,
                CLUB_COURSE_SLOT,
                TEXT_FLAG_CENTERED,
                43,
                255,
                75,
                OPAQUE_ALPHA
            );
            SetTextSlot(
                CLUB_TEXT_X,
                CLUB_MODE_Y,
                GetClubTournamentModeMessage(),
                TEXT_SLOT_LARGE_FONT,
                CLUB_MODE_SLOT,
                TEXT_FLAG_CENTERED,
                255,
                63,
                127,
                OPAQUE_ALPHA
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCodeEntryBar(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_CodeEntryBarImage)));
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != CODE_ENTRY_BAR_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_CodeEntryBarImage) +
                (stripIndex * CODE_ENTRY_BAR_WIDTH * CODE_ENTRY_BAR_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            CODE_ENTRY_BAR_WIDTH,
            CODE_ENTRY_BAR_STRIP_HEIGHT,
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
            QuarterPixels(CODE_ENTRY_BAR_LEFT),
            QuarterPixels(CODE_ENTRY_BAR_TOP + (stripIndex * CODE_ENTRY_BAR_STRIP_HEIGHT)),
            QuarterPixels(CODE_ENTRY_BAR_LEFT + CODE_ENTRY_BAR_WIDTH),
            QuarterPixels(
                CODE_ENTRY_BAR_TOP + (stripIndex * CODE_ENTRY_BAR_STRIP_HEIGHT) +
                CODE_ENTRY_BAR_STRIP_HEIGHT
            ),
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

void DrawCodePanels(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImageBody(f_NamePlateImage)));
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_NamePlateImage) +
                (stripIndex * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
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
            QuarterPixels(CODE_UPPER_PANEL_LEFT),
            QuarterPixels(CODE_UPPER_PANEL_TOP + (stripIndex * CODE_UPPER_PANEL_STRIP_HEIGHT)),
            QuarterPixels(CODE_UPPER_PANEL_LEFT + CODE_UPPER_PANEL_WIDTH),
            QuarterPixels(
                CODE_UPPER_PANEL_TOP + (stripIndex * CODE_UPPER_PANEL_STRIP_HEIGHT) +
                CODE_UPPER_PANEL_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, CODE_UPPER_PANEL_WIDTH),
            TexelRunStep(NAME_PLATE_STRIP_HEIGHT, CODE_UPPER_PANEL_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(CODE_LOWER_PANEL_LEFT),
            QuarterPixels(CODE_LOWER_PANEL_TOP + (stripIndex * CODE_LOWER_PANEL_STRIP_HEIGHT)),
            QuarterPixels(CODE_LOWER_PANEL_LEFT + CODE_LOWER_PANEL_WIDTH),
            QuarterPixels(
                CODE_LOWER_PANEL_TOP + (stripIndex * CODE_LOWER_PANEL_STRIP_HEIGHT) +
                CODE_LOWER_PANEL_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, CODE_LOWER_PANEL_WIDTH),
            TexelRunStep(NAME_PLATE_STRIP_HEIGHT, CODE_LOWER_PANEL_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawCodeHand(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s32 left;
    s32 top;

    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_PointingHandImage)));
    gDPLoadTextureTile_4b(
        gfx++,
        TextureAddress(ImageTexels(f_PointingHandImage)),
        G_IM_FMT_CI,
        POINTING_HAND_SIZE,
        POINTING_HAND_SIZE,
        0,
        0,
        POINTING_HAND_SIZE - 1,
        POINTING_HAND_SIZE - 1,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    f_CodeHandStepTimer++;
    if (f_CodeHandStepTimer == CODE_HAND_STEP_FRAMES) {
        f_CodeHandStepTimer = 0;
        switch (f_CodeHandMotion) {
            case CODE_HAND_MOVING_RIGHT:
                f_CodeHandReach++;
                if (f_CodeHandReach == CODE_HAND_REACH) {
                    f_CodeHandMotion = CODE_HAND_MOVING_LEFT;
                }

                break;
            case CODE_HAND_MOVING_LEFT:
                f_CodeHandReach--;
                if (f_CodeHandReach == 0) {
                    f_CodeHandMotion = CODE_HAND_MOVING_RIGHT;
                }

                break;
            default:
                f_CodeHandMotion = CODE_HAND_MOVING_RIGHT;
                f_CodeHandReach = 0;
                break;
        }
    }

    left = (memory->codeKeyColumn * CODE_KEY_SPACING_X) + CODE_HAND_FIRST_X + f_CodeHandReach;
    top = (memory->codeKeyRow * CODE_KEY_SPACING_Y) + CODE_HAND_FIRST_Y;
    gSPScisTextureRectangle(
        gfx++,
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + POINTING_HAND_SIZE),
        QuarterPixels(top + POINTING_HAND_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawCodeKeys(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s32 row;
    s32 column;
    s8 glyph;
    s32 growth;
    s32 size;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImageBody(f_CodeGlyphsImage)));
    for (row = 0; row != CODE_KEYBOARD_ROW_COUNT; row++) {
        for (column = 0; column != CODE_KEYBOARD_COLUMN_COUNT; column++) {
            glyph = f_CodeKeyGlyphs[row][column];
            if (glyph == CODE_GLYPH_NONE) {
                continue;
            }

            if (memory->codeKeyColumn == column && memory->codeKeyRow == row) {
                gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 0, OPAQUE_ALPHA);

                growth = 0;
                size = CODE_GLYPH_SIZE;
            }
            else {
                gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
                growth = 0;
                size = CODE_GLYPH_SIZE;
            }

            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(f_CodeGlyphsImage) +
                    (glyph * (CODE_GLYPH_SIZE * CODE_GLYPH_SIZE / 2))
                ),
                G_IM_FMT_CI,
                CODE_GLYPH_SIZE,
                CODE_GLYPH_SIZE,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels((column * CODE_KEY_SPACING_X) + CODE_GLYPH_FIRST_X - growth),
                QuarterPixels((row * CODE_KEY_SPACING_Y) + CODE_GLYPH_FIRST_Y - growth),
                QuarterPixels(
                    (column * CODE_KEY_SPACING_X) + CODE_GLYPH_FIRST_X + CODE_GLYPH_SIZE + growth
                ),
                QuarterPixels(
                    (row * CODE_KEY_SPACING_Y) + CODE_GLYPH_FIRST_Y + CODE_GLYPH_SIZE + growth
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(CODE_GLYPH_SIZE, size),
                TexelRunStep(CODE_GLYPH_SIZE, size)
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    f_UnusedDoubledHandReach = f_CodeHandReach * 2;
    *displayListHead = gfx;
}

void DrawEnteredCode(Gfx **displayListHead) {
    s32 codeLetterIndex;
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    for (codeLetterIndex = 0; codeLetterIndex != CLUB_TOURNAMENT_CODE_LENGTH; codeLetterIndex++) {
        if (f_EnteredCode[codeLetterIndex] != CLUB_CODE_NO_CHARACTER) {
            sprintf(g_PrintBuffer, "%c", f_EnteredCode[codeLetterIndex]);
            DrawLargeTextWithShadow(
                &gfx,
                (codeLetterIndex * CLUB_CODE_CHARACTER_WIDTH) + CLUB_CODE_FIRST_X,
                CLUB_CODE_Y,
                g_PrintBuffer,
                -1,
                -1,
                0
            );
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

CodeKeyResult PressCodeKey(void) {
    MenuMemory *memory = GetMenuMemory();
    s32 column = memory->codeKeyColumn;
    CodeKeyResult keyResult = CODE_KEY_RESULT_TYPING;
    u8 key = f_CodeKeys[memory->codeKeyRow][column];

    switch (key) {
        case CODE_KEY_BACK:
            if (f_EnteredCodeLength > 0) {
                if (f_EnteredCodeLength == CLUB_TOURNAMENT_CODE_LENGTH) {
                    f_EnteredCodeLength = CLUB_TOURNAMENT_CODE_LENGTH - 1;
                }

                f_EnteredCode[f_EnteredCodeLength] = CLUB_CODE_NO_CHARACTER;
                f_EnteredCodeLength--;
            }
            else {
                f_EnteredCode[0] = CLUB_CODE_NO_CHARACTER;
            }

            break;
        case CODE_KEY_BLANK:
            break;
        case CODE_KEY_END:
            if (f_EnteredCodeLength < CLUB_TOURNAMENT_CODE_LENGTH) {
                return CODE_KEY_RESULT_TOO_SHORT;
            }

            keyResult = CODE_KEY_RESULT_DONE;
            break;
        default:
            if (f_EnteredCodeLength < CLUB_TOURNAMENT_CODE_LENGTH) {
                f_EnteredCode[f_EnteredCodeLength] = key;
                f_EnteredCodeLength++;
                if (f_EnteredCodeLength == CLUB_TOURNAMENT_CODE_LENGTH) {
                    memory->codeKeyColumn = CODE_KEYBOARD_END_COLUMN;
                    memory->codeKeyRow = CODE_KEYBOARD_END_ROW;
                }
            }
            else {
                f_EnteredCode[f_EnteredCodeLength - 1] = key;
            }

            break;
    }

    return keyResult;
}

s32 DeleteCodeCharacter(void) {
    Boolean isEmpty = TRUE;

    if (f_EnteredCodeLength > 0) {
        if (f_EnteredCodeLength == CLUB_TOURNAMENT_CODE_LENGTH) {
            f_EnteredCodeLength = CLUB_TOURNAMENT_CODE_LENGTH - 1;
        }

        f_EnteredCode[f_EnteredCodeLength] = CLUB_CODE_NO_CHARACTER;
        f_EnteredCodeLength--;
        isEmpty = FALSE;
    }
    else if (f_EnteredCode[0] != CLUB_CODE_NO_CHARACTER) {
        f_EnteredCode[0] = CLUB_CODE_NO_CHARACTER;
        isEmpty = FALSE;
    }

    return isEmpty;
}

void DrawCodePrompt(Gfx **displayListHead) {
    s32 x = 0;
    Gfx *gfx = *displayListHead;
    s32 messageOffset;
    TextFlags flags;

    switch (f_ClubCodeState) {
        case CLUB_CODE_STATE_ENTERING:
            messageOffset = 0;
            x = CLUB_CODE_PROMPT_X;
            flags = !IsMenuWindowOpen() ? TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_TYPED :
                                          TEXT_FLAG_DRAWN_BY_OWNER;
            break;
        case CLUB_CODE_STATE_INCORRECT:
            messageOffset = 1;
            x = CLUB_CODE_INCORRECT_X;
            flags = TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_TYPED;
            break;
        default:
            messageOffset = -1;
            flags = TEXT_FLAG_DRAWN_BY_OWNER | TEXT_FLAG_TYPED;
            break;
    }

    if (messageOffset >= 0) {
        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        SetTextSlot(
            x,
            CLUB_CODE_MESSAGE_Y,
            MESSAGE_ENTER_CODE + messageOffset,
            TEXT_SLOT_LARGE_FONT,
            0,
            flags,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, 0);
    }

    *displayListHead = gfx;
}

s32 FindEnteredCodeIndex(void) {
    s32 foundIndex = -1;
    s32 codeIndex;
    s32 matchCount;
    s32 codeLetterIndex;

    for (codeIndex = 0; codeIndex != CLUB_TOURNAMENT_CODE_COUNT; codeIndex++) {
        matchCount = 0;
        for (codeLetterIndex = 0; codeLetterIndex != CLUB_TOURNAMENT_CODE_LENGTH;
            codeLetterIndex++) {
            if (f_EnteredCode[codeLetterIndex] !=
                f_ClubTournamentCodes[codeIndex][codeLetterIndex]) {
                break;
            }

            matchCount++;
        }

        if (matchCount == CLUB_TOURNAMENT_CODE_LENGTH) {
            foundIndex = codeIndex;
            break;
        }
    }

    if (foundIndex == -1) {
        f_ClubCodeState = CLUB_CODE_STATE_INCORRECT;
    }

    return foundIndex;
}

void DrawCodeCursor(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 vertexIndex;
    s32 red;
    s32 blue;
    s32 green;
    s32 codeLetterIndex;

    for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT; vertexIndex++) {
        f_CodeCursorPhases[vertexIndex] += MENU_CURSOR_PHASE_STEP;
        red = (s32)(sinf(f_CodeCursorPhases[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        blue = (s32)(sinf(f_CodeCursorPhases[vertexIndex] + (M_PI / 2)) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        green = (s32)(sinf(f_CodeCursorPhases[vertexIndex] + M_PI) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        f_CodeCursorVertices[vertexIndex].v.cn[0] = red;
        f_CodeCursorVertices[vertexIndex].v.cn[2] = blue;
        f_CodeCursorVertices[vertexIndex].v.cn[1] = green;
    }

    guOrtho(
        &f_CodeCursorProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CURSOR_NEAR,
        MENU_CURSOR_FAR,
        1.0f
    );
    codeLetterIndex = f_EnteredCodeLength;
    if (codeLetterIndex >= CLUB_TOURNAMENT_CODE_LENGTH) {
        codeLetterIndex = CLUB_TOURNAMENT_CODE_LENGTH - 1;
    }

    guTranslate(
        &f_CodeCursorTranslation,
        (codeLetterIndex * CLUB_CODE_CHARACTER_WIDTH) + CODE_CURSOR_FIRST_X,
        CODE_CURSOR_Y,
        MENU_CURSOR_Z
    );
    guRotate(&f_CodeCursorRotation, 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&f_CodeCursorScale, CODE_CURSOR_SCALE, CODE_CURSOR_SCALE, 1.0f);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_CodeCursorBox)),
        G_IM_FMT_IA,
        MENU_CURSOR_SIZE,
        MENU_CURSOR_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    AppendSyncedRdpState(gfx, gDPSetCycleType, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPSetCombineMode(gfx++, G_CC_SHADEDECALA, G_CC_SHADEDECALA);
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_CodeCursorProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_CodeCursorTranslation),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_CodeCursorRotation),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_CodeCursorScale),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_CodeCursorVertices, MENU_CURSOR_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

s32 IsClubCharacterChoosable(void) {
    return g_ClubTournamentRules.characterIndex == -1;
}

s32 GetClubTournamentCharacter(void) {
    return g_ClubTournamentRules.characterIndex;
}

s32 TakeClubTournamentEntry(void) {
    if (f_IsClubTournamentEntered) {
        f_IsClubTournamentEntered = FALSE;
        return TRUE;
    }

    return FALSE;
}

void DoNothingInTournamentMenu(void) {
}
