/*
 * The clubhouse's character table, screen 15: the characters' wins against each
 * other, and a popup of two characters' faces with their wins.
 */

#include "menus/character_table.h"

#include <ultra64.h>

// This file passes DrawCharacterFace's character unnarrowed; see
// player_stats.h
#define WIDENS_FACE_CHARACTER_INDEX

#include "audio/sound.h"
#include "characters/character.h"
#include "menus/main_menu.h"
#include "menus/menu.h"
#include "menus/menu_arrows.h"
#include "menus/menu_memory.h"
#include "menus/menu_number.h"
#include "menus/player_stats.h"
#include "menus/save_screens.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/sky_and_hud_animations.h"
#include "system/asset.h"
#include "system/compiler.h"
#include "system/memory_address.h"

// The table of screen 15: its columns and rows, and how many of each fit on the
// screen
#define TABLE_COLUMN_COUNT 14
#define TABLE_SHOWN_COLUMN_COUNT 7
#define TABLE_ROW_COUNT 18
#define TABLE_SHOWN_ROW_COUNT 5

// LoadCharacterTableBuffer reads this beside ASSET_CURSOR_BOX and
// ASSET_SPINNING_STAR
#define ASSET_TABLE_ARROWS 0x94A

// DrawTableStar squeezes the star's image to fit a table cell
#define TABLE_STAR_SHOWN_WIDTH 24
#define TABLE_STAR_SHOWN_HEIGHT 20

// The corner of the screen where the table's popup goes
enum TablePopupCorner {
    TABLE_POPUP_TOP_LEFT,
    TABLE_POPUP_TOP_RIGHT,
    TABLE_POPUP_BOTTOM_LEFT,
    TABLE_POPUP_BOTTOM_RIGHT
};

typedef s32 TablePopupCorner;

#define TABLE_POPUP_CORNER_COUNT 4
// Where the popup goes, and the cursor places that move it
#define TABLE_POPUP_LEFT_X 68
#define TABLE_POPUP_RIGHT_X 164
#define TABLE_POPUP_TOP_Y 84
#define TABLE_POPUP_BOTTOM_Y 170
#define TABLE_POPUP_LEFT_COLUMN_LIMIT 4
#define TABLE_POPUP_RIGHT_COLUMN 3
#define TABLE_POPUP_TOP_ROW_LIMIT 2
#define TABLE_POPUP_BOTTOM_ROW 3
// The popup is the menu banner shrunk to this size, with two faces and their
// wins against each other
#define TABLE_POPUP_WIDTH 120
#define TABLE_POPUP_STRIP_HEIGHT 20
#define TABLE_POPUP_ROW_FACE_X 16
#define TABLE_POPUP_COLUMN_FACE_X 74
#define TABLE_POPUP_FACE_Y 6
#define TABLE_POPUP_SIGN_X 48
#define TABLE_POPUP_SIGN_Y 12
#define TABLE_POPUP_WINS_Y 36

// The characters that MarkShownTableCharacters shows on the table, the first
// ones being those of the character grid
#define TABLE_CHARACTER_HIDDEN (-1)
#define TABLE_CHARACTER_SHOWN 1
#define GRID_CHARACTER_COUNT 13
#define TABLE_CELL_COUNT 17

// A cell of the character grid
typedef struct TableCell {
    s32 column;
    s32 row;
} TableCell;

// The lines between the table's cells, each from one corner to the next
#define TABLE_LINE_COUNT 12

// DrawCharacterTable draws the table over a dimmed panel, with a face beside
// each shown row and above each column
#define TABLE_PANEL_LEFT 24
#define TABLE_PANEL_TOP 50
#define TABLE_PANEL_RIGHT 302
#define TABLE_PANEL_BOTTOM 230
#define TABLE_CELL_WIDTH 35
#define TABLE_CELL_HEIGHT 30
#define TABLE_ROW_FACE_X 28
#define TABLE_FIRST_ROW_FACE_Y 82
#define TABLE_FIRST_COLUMN_FACE_X 62
#define TABLE_COLUMN_FACE_Y 52
#define TABLE_FIRST_STAR_Y 85
#define TABLE_FIRST_SIGN_X 67
#define TABLE_FIRST_SIGN_Y 86
#define TABLE_FIRST_DASH_Y 88
#define TABLE_CURSOR_X 74
#define TABLE_CURSOR_Y 93
// Where the arrows that show the table goes on past the screen sit
#define TABLE_ROW_ARROW_X 40
#define TABLE_UP_ARROW_Y 88
#define TABLE_DOWN_ARROW_Y 222
#define TABLE_COLUMN_ARROW_Y 68
#define TABLE_LEFT_ARROW_X 64
#define TABLE_RIGHT_ARROW_X 296
#define TABLE_ARROW_BLINK_FRAMES 4

void DrawTableCursor(Gfx **displayListHead, s32 x, s32 y);
void DrawTableLines(Gfx **displayListHead);
void DrawTablePopup(Gfx **displayListHead);
void DrawTableStar(Gfx **displayListHead, s32 left, s32 top);

// The popup's top-left corner on the screen, set with its corner
static s32 f_TablePopupLeft = TABLE_POPUP_LEFT_X;
static s32 f_TablePopupTop = TABLE_POPUP_TOP_Y;
// The TABLE_POPUP_* corner where the table's popup is
static TablePopupCorner f_TablePopupCorner = TABLE_POPUP_TOP_LEFT;
// The arrows' blink frame, flipped every TABLE_ARROW_BLINK_FRAMES frames
static s32 f_TableArrowFrame = 0;
static s32 f_TableArrowTimer = 0;
// Reset with the arrows' blink, but read by no routine
static s32 f_UnreadTableArrowState = 0;
// The character in each place of the table: the CharacterId of each
// CharacterChoice, as character_select.c keeps its own copy
static CharacterIndex f_ChoiceCharacters[CHARACTER_COUNT] = {
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
// The table cursor's corners, whose colors DrawTableCursor cycles
static Vtx f_TableCursorVertices[MENU_CURSOR_VERTEX_COUNT] = {
    {{{-48, 48, 0}, 0, {0, 0}, {0xFF, 0x00, 0x00, 0xFF}}},
    {{{-48, 0, 0}, 0, {0, 3072}, {0x00, 0xFF, 0x00, 0xFF}}},
    {{{0, 0, 0}, 0, {3072, 3072}, {0x00, 0x00, 0xFF, 0xFF}}},
    {{{0, 48, 0}, 0, {3072, 0}, {0xFF, 0xFF, 0x00, 0xFF}}},
};
// Read by no routine
static s32 f_UnreadTablePair[2] UNUSED = {0, 0};
// The angle, in radians, of each corner's color cycle
static f32 f_TableCursorPhases[MENU_CURSOR_VERTEX_COUNT] = {0.0f, 2.0f, 4.0f, 6.0f};
// The top-left and bottom-right corners of each line between the table's
// cells: five one-pixel rows, then seven one-pixel columns
static ScreenCorner f_TableLineCorners[TABLE_LINE_COUNT * 2] = {
    {24, 80},
    {300, 81},
    {24, 110},
    {300, 111},
    {24, 140},
    {300, 141},
    {24, 170},
    {300, 171},
    {24, 200},
    {300, 201},
    {56, 52},
    {57, 228},
    {91, 52},
    {92, 228},
    {126, 52},
    {127, 228},
    {161, 52},
    {162, 228},
    {196, 52},
    {197, 228},
    {231, 52},
    {232, 228},
    {266, 52},
    {267, 228},
};
// Read by no routine
static s32 f_UnreadTablePoint[2] UNUSED = {64, 84};
// Which of the cursor's shown cells each popup corner would cover
static UBoolean8 f_DoesTablePopupCoverCell[TABLE_POPUP_CORNER_COUNT][TABLE_SHOWN_ROW_COUNT]
                                          [TABLE_SHOWN_COLUMN_COUNT] = {
                                              {
                                                  {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE},
                                                  {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                              },
                                              {
                                                  {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE},
                                                  {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                              },
                                              {
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE},
                                                  {TRUE, TRUE, TRUE, TRUE, FALSE, FALSE, FALSE},
                                              },
                                              {
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, FALSE, FALSE, FALSE, FALSE},
                                                  {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE},
                                                  {FALSE, FALSE, FALSE, TRUE, TRUE, TRUE, TRUE},
                                              },
};

// The camera and placement that DrawTableCursor draws the cursor with
static Mtx f_TableCursorProjection;
static Mtx f_TableCursorTranslation;
static Mtx f_TableCursorRotation;

// Read by no routine
static u8 f_UnreadTableBytes[0x80] UNUSED;

static Mtx f_TableCursorScale;

// Read by no routine
static u8 f_UnreadTableWord[0x4] UNUSED;

// The images that LoadCharacterTableBuffer is given
static ImageAsset *f_TableCursorBox;
static ImageAsset *f_TablePopupBanner;
static ImageAsset *f_TableArrows;
static ImageAsset *f_TableStar;
// Whether the table shows or darkens each place's character
static s32 f_TableCharacterVisibility[CHARACTER_COUNT];

void LoadCharacterTableBuffer(void *buffer, TableBuffer bufferKind) {
    EncodedAsset asset;
    MenuMemory *memory = GetMenuMemory();
    s32 x;
    s32 y;

    switch (bufferKind) {
        case TABLE_BUFFER_CURSOR:
            f_TableCursorBox = buffer;
            OpenEncodedAsset(ASSET_CURSOR_BOX, &asset);
            ReadEncodedAsset(f_TableCursorBox, &asset);
            break;
        case TABLE_BUFFER_UNREAD:
            f_TablePopupBanner = buffer;
            break;
        case TABLE_BUFFER_ARROWS:
            f_TableArrows = buffer;
            OpenEncodedAsset(ASSET_TABLE_ARROWS, &asset);
            ReadEncodedAsset(f_TableArrows, &asset);
            f_TableArrowFrame = 0;
            f_TableArrowTimer = 0;
            f_UnreadTableArrowState = 0;
            break;
        case TABLE_BUFFER_STAR:
            f_TableStar = buffer;
            OpenEncodedAsset(ASSET_SPINNING_STAR, &asset);
            ReadEncodedAsset(f_TableStar, &asset);
            break;
    }

    // The popup goes to the quarter of the screen away from the cursor
    if (memory->tableColumn < TABLE_POPUP_LEFT_COLUMN_LIMIT &&
        memory->tableRow < TABLE_POPUP_TOP_ROW_LIMIT) {
        x = TABLE_POPUP_RIGHT_X;
        y = TABLE_POPUP_BOTTOM_Y;
        f_TablePopupCorner = TABLE_POPUP_BOTTOM_RIGHT;
    }
    else if (memory->tableColumn >= TABLE_POPUP_RIGHT_COLUMN &&
        memory->tableRow < TABLE_POPUP_TOP_ROW_LIMIT) {
        x = TABLE_POPUP_LEFT_X;
        y = TABLE_POPUP_BOTTOM_Y;
        f_TablePopupCorner = TABLE_POPUP_BOTTOM_LEFT;
    }
    else if (memory->tableColumn < TABLE_POPUP_LEFT_COLUMN_LIMIT &&
        memory->tableRow >= TABLE_POPUP_BOTTOM_ROW) {
        x = TABLE_POPUP_RIGHT_X;
        y = TABLE_POPUP_TOP_Y;
        f_TablePopupCorner = TABLE_POPUP_TOP_RIGHT;
    }
    else {
        x = TABLE_POPUP_LEFT_X;
        y = TABLE_POPUP_TOP_Y;
        f_TablePopupCorner = TABLE_POPUP_TOP_LEFT;
    }

    f_TablePopupLeft = x;
    f_TablePopupTop = y;
}

void UpdateCharacterTable(void) {
    MenuMemory *memory = GetMenuMemory();
    s8 firstColumn;
    s8 firstRow;
    s8 column;
    s8 row;

    if (IsHudAnimationDone() == TRUE) {
        if (g_MenuPressedButtons & B_BUTTON) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
            return;
        }

        firstColumn = memory->tableFirstColumn;
        firstRow = memory->tableFirstRow;
        column = memory->tableColumn;
        row = memory->tableRow;
        if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (column == TABLE_SHOWN_COLUMN_COUNT - 1 &&
                firstColumn < TABLE_COLUMN_COUNT - TABLE_SHOWN_COLUMN_COUNT) {
                memory->tableFirstColumn++;
            }

            memory->tableColumn++;
            if (memory->tableColumn >= TABLE_SHOWN_COLUMN_COUNT) {
                memory->tableColumn = TABLE_SHOWN_COLUMN_COUNT - 1;
            }
        }
        else if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (column == 0 && firstColumn > 0) {
                memory->tableFirstColumn--;
            }

            memory->tableColumn--;
            if (memory->tableColumn < 0) {
                memory->tableColumn = 0;
            }
        }
        else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (row == TABLE_SHOWN_ROW_COUNT - 1 &&
                firstRow < TABLE_ROW_COUNT - TABLE_SHOWN_ROW_COUNT) {
                memory->tableFirstRow++;
            }

            memory->tableRow++;
            if (memory->tableRow >= TABLE_SHOWN_ROW_COUNT) {
                memory->tableRow = TABLE_SHOWN_ROW_COUNT - 1;
            }
        }
        else if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (row == 0 && firstRow > 0) {
                memory->tableFirstRow--;
            }

            memory->tableRow--;
            if (memory->tableRow < 0) {
                memory->tableRow = 0;
            }
        }
    }
}

void DrawCharacterTable(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s16 shownRow;
    s16 shownColumn;
    s16 row;
    s16 column;
    CharacterIndex16 rowCharacter;
    CharacterIndex16 columnCharacter;
    s32 columnWins;
    s32 rowWins;
    u8 drawMode;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(TABLE_PANEL_LEFT),
        QuarterPixels(TABLE_PANEL_TOP),
        QuarterPixels(TABLE_PANEL_RIGHT),
        QuarterPixels(TABLE_PANEL_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    DrawTableLines(&gfx);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    row = memory->tableFirstRow;
    for (shownRow = 0; shownRow != TABLE_SHOWN_ROW_COUNT; shownRow++) {
        rowCharacter = f_ChoiceCharacters[row];
        drawMode = FACE_DARKENED;
        if (f_TableCharacterVisibility[row] == TABLE_CHARACTER_SHOWN) {
            drawMode = FACE_PLAIN;
        }

        DrawCharacterFace(
            &gfx,
            g_CharacterFaceImages[rowCharacter],
            g_MenuCharacterPalettes[rowCharacter],
            TABLE_ROW_FACE_X,
            (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_ROW_FACE_Y,
            rowCharacter,
            drawMode
        );
        column = memory->tableFirstColumn;
        for (shownColumn = 0; shownColumn != TABLE_SHOWN_COLUMN_COUNT; shownColumn++) {
            columnCharacter = f_ChoiceCharacters[column];
            drawMode = FACE_DARKENED;
            if (f_TableCharacterVisibility[column] != TABLE_CHARACTER_HIDDEN) {
                drawMode = FACE_PLAIN;
            }

            if (shownRow == 0) {
                DrawCharacterFace(
                    &gfx,
                    g_CharacterFaceImages[columnCharacter],
                    g_MenuCharacterPalettes[columnCharacter],
                    (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_COLUMN_FACE_X,
                    TABLE_COLUMN_FACE_Y,
                    columnCharacter,
                    drawMode
                );
            }

            columnWins = save->records.awards.matchups[rowCharacter][columnCharacter].lossCount &
                WIN_COUNT_MASK;
            rowWins = save->records.awards.matchups[rowCharacter][columnCharacter].winBits &
                WIN_COUNT_MASK;
            // A pairing with the flag set shows the star, and the others show who has
            // won more
            if (save->records.awards.matchups[rowCharacter][columnCharacter].winBits &
                WON_UNSAVED_FLAG) {
                DrawTableStar(
                    &gfx,
                    (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_COLUMN_FACE_X,
                    (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_STAR_Y
                );
            }
            else if (columnWins != 0) {
                DrawMenuSign(
                    &gfx,
                    (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_SIGN_X,
                    (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_SIGN_Y,
                    MENU_SIGN_TIMES
                );
            }
            else if (rowWins != 0) {
                DrawMenuSign(
                    &gfx,
                    (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_SIGN_X,
                    (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_SIGN_Y,
                    MENU_SIGN_CIRCLE
                );
            }
            else {
                DrawMenuSign(
                    &gfx,
                    (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_SIGN_X,
                    (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_DASH_Y,
                    MENU_SIGN_DASH
                );
            }

            column++;
        }
        row++;
    }
    if (memory->tableFirstRow > 0) {
        DrawGreenArrow(
            &gfx,
            f_TableArrows,
            f_TableArrowFrame,
            MENU_ARROW_UP,
            TABLE_ROW_ARROW_X,
            TABLE_UP_ARROW_Y
        );
    }

    if (memory->tableFirstRow < TABLE_ROW_COUNT - TABLE_SHOWN_ROW_COUNT) {
        DrawGreenArrow(
            &gfx,
            f_TableArrows,
            f_TableArrowFrame,
            MENU_ARROW_DOWN,
            TABLE_ROW_ARROW_X,
            TABLE_DOWN_ARROW_Y
        );
    }

    if (memory->tableFirstColumn > 0) {
        DrawGreenArrow(
            &gfx,
            f_TableArrows,
            f_TableArrowFrame,
            MENU_ARROW_LEFT,
            TABLE_LEFT_ARROW_X,
            TABLE_COLUMN_ARROW_Y
        );
    }

    if (memory->tableFirstColumn < TABLE_COLUMN_COUNT - TABLE_SHOWN_COLUMN_COUNT) {
        DrawGreenArrow(
            &gfx,
            f_TableArrows,
            f_TableArrowFrame,
            MENU_ARROW_RIGHT,
            TABLE_RIGHT_ARROW_X,
            TABLE_COLUMN_ARROW_Y
        );
    }

    f_TableArrowTimer++;
    if (f_TableArrowTimer == TABLE_ARROW_BLINK_FRAMES) {
        f_TableArrowTimer = 0;
        f_TableArrowFrame = f_TableArrowFrame == 0;
    }

    DrawTablePopup(&gfx);
    DrawTableCursor(
        &gfx,
        (memory->tableColumn * TABLE_CELL_WIDTH) + TABLE_CURSOR_X,
        (memory->tableRow * TABLE_CELL_HEIGHT) + TABLE_CURSOR_Y
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws the color-cycling cursor at (x, y), on the chosen table cell
void DrawTableCursor(Gfx **displayListHead, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 vertexIndex;
    s32 red;
    s32 blue;
    s32 green;

    for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT; vertexIndex++) {
        f_TableCursorPhases[vertexIndex] += MENU_CURSOR_PHASE_STEP;
        red = (s32)(sinf(f_TableCursorPhases[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        blue =
            (s32)(sinf(f_TableCursorPhases[vertexIndex] + (M_PI / 2)) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        green = (s32)(sinf(f_TableCursorPhases[vertexIndex] + M_PI) * MENU_CURSOR_COLOR_SWING) +
            MENU_CURSOR_COLOR_MIDDLE;
        f_TableCursorVertices[vertexIndex].v.cn[RED_CHANNEL] = red;
        f_TableCursorVertices[vertexIndex].v.cn[BLUE_CHANNEL] = blue;
        f_TableCursorVertices[vertexIndex].v.cn[GREEN_CHANNEL] = green;
    }

    guOrtho(
        &f_TableCursorProjection,
        -(SCREEN_WIDTH / 2),
        SCREEN_WIDTH / 2,
        -(SCREEN_HEIGHT / 2),
        SCREEN_HEIGHT / 2,
        MENU_CURSOR_NEAR,
        MENU_CURSOR_FAR,
        1.0f
    );
    guTranslate(
        &f_TableCursorTranslation,
        x - ((SCREEN_WIDTH / 2) - (MENU_CURSOR_SIZE / 2)),
        ((SCREEN_HEIGHT / 2) - (MENU_CURSOR_SIZE / 2)) - y,
        MENU_CURSOR_Z
    );
    guRotate(&f_TableCursorRotation, 0.0f, 0.0f, 1.0f, 0.0f);
    guScale(&f_TableCursorScale, 1.0f, MENU_CURSOR_SCALE_Y, 1.0f);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageBody(f_TableCursorBox)),
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
        PhysicalAddress(&f_TableCursorProjection),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_TableCursorTranslation),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_TableCursorRotation),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gSPMatrix(
        gfx++,
        PhysicalAddress(&f_TableCursorScale),
        G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
    );
    gDPPipeSync(gfx++);
    gSPVertex(gfx++, f_TableCursorVertices, MENU_CURSOR_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws the white lines between the table's cells
void DrawTableLines(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 index;

    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    for (index = 0; index != TABLE_LINE_COUNT * 2; index += 2) {
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels(f_TableLineCorners[index].x),
            QuarterPixels(f_TableLineCorners[index].y),
            QuarterPixels(f_TableLineCorners[index + 1].x),
            QuarterPixels(f_TableLineCorners[index + 1].y),
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

// Draws the popup with the faces of the cursor cell's two characters and their
// wins against each other, first moving it to the opposite corner if it would
// cover the cursor.
//
// Each count's call is written once in every arm of a test that compares
// the two counts, three arms for the row's and two for the column's, as
// code that once told the leader's count from the other's might leave it.
// The arms are the same, so jump2 merges them again and drops the tests,
// but that comes after the registers are given out, and until then each
// test and each copy of a call is one more reference to the counts. flow
// counts 10 for rowWins and 9 for columnWins where one call each makes 5
// and 5, and the copies lengthen their lives from 61 and 85 insns to 105
// and 115. In global-alloc that lifts rowWins' priority from 1639 to 2857
// and columnWins' from 1176 to 2347, around the 2727 of the face images'
// base and above the 1886 of the column's character. So rowWins is placed
// first and takes $s0, the base $s1, columnWins $s2 and the character $s3,
// as in the original. With one call each the base is first, then the
// character, and the counts take $s2 and $s3.
//
// A do-while (0) around a count's drawing weighs its references too, but
// leaves its life alone: columnWins then has 1176, 1647, 2823 or 3176,
// with 5, 7, 8 or 9 references, and never comes between the base and the
// character.
//
// index serves the strip loop and both counts' offsets, to match.
//
// Measured against the match, in words differing (and how many in shape):
//
//   One call for each count (the old draft)                 48 (2)
//   Two arms for each count                                 24 (2)
//   Three arms for each count                               22 (0)
//   The row's three arms, with one call for the column's    24 (0)
//   The column's two arms, with one call for the row's      38 (2)
//   Two arms for the row's count and three for the column's 22 (0)
//   The row's second test as rowWins == columnWins          48 (12)
//   The row's second test as columnWins != 0                24 (2)
//   The row's tests as rowWins != 0 and columnWins != 0     59 (9)
//   The column's test as columnWins != 0                    13 (7)
//   The arms around the row's digits test as well           61 (21), frame -48
//   The arms around the column's digits test as well        32 (10)
//   The sign's call in two arms as well                     34 (2)
//   The sign's call in three arms, with one for each count  58 (2)
//   One call each, the row's count in a do-while (0)        24 (0)
//   The same, with the column's count and the sign in one   22 (0)
//   The same, with the row's call alone in its do-while     18 (0)
//   One call each, each count in a do-while (0)             26 (2)
//   The column's call in each arm of its digits test        49 (45)
//   The row's digits test without its repeated arm          73 (11), frame +24
//   The row's digits test as three separate ifs             153 (17), frame +16
//   The row's digits test without its lower bounds          45 (3), frame +8
//   The column's digits test without its lower bounds       8 (6)
//   An offset variable apart from index for the counts      38 (0)
//   rowWins read before columnWins                          8 (0)
//   The column's character read before the row's            10 (2)
//   The counts as s16                                       87 (19)
//   The counts as u8                                        161 (65), frame +16
//   The counts as u32                                       4 (4)
//   The counts read through a MatchupRecord pointer         37 (17)
//   A local for the column's index                          32 (4)
//   The face modes as an if and an else on == hidden        10 (4)
//   A face mode variable of its own for the column          6 (0)
//   memory declared before save                             46 (2)
//
// These match as well: the row's two tests in the other order, or its
// second spelled columnWins > rowWins; the column's test as <, ==, != or
// >= between the counts, or as rowWins != 0; two arms for the row's count
// with three for the column's on columnWins > rowWins and rowWins != 0;
// the counts declared in the other order; the row's index written out in
// place of rowIndex; the face modes as conditional expressions, as an if
// and an else on != hidden, or as an s32.
void DrawTablePopup(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    // Counts the banner strips, then holds the x of each count of wins, to
    // match
    s32 index;
    s32 row;
    CharacterIndex rowCharacter;
    CharacterIndex columnCharacter;
    s32 rowWins;
    s32 columnWins;
    u8 faceMode;

    if (f_DoesTablePopupCoverCell[f_TablePopupCorner][memory->tableRow][memory->tableColumn] ==
        TRUE) {
        switch (f_TablePopupCorner) {
            case TABLE_POPUP_TOP_LEFT:
                f_TablePopupLeft = TABLE_POPUP_RIGHT_X;
                f_TablePopupTop = TABLE_POPUP_BOTTOM_Y;
                f_TablePopupCorner = TABLE_POPUP_BOTTOM_RIGHT;
                break;
            case TABLE_POPUP_TOP_RIGHT:
                f_TablePopupLeft = TABLE_POPUP_LEFT_X;
                f_TablePopupTop = TABLE_POPUP_BOTTOM_Y;
                f_TablePopupCorner = TABLE_POPUP_BOTTOM_LEFT;
                break;
            case TABLE_POPUP_BOTTOM_LEFT:
                f_TablePopupLeft = TABLE_POPUP_RIGHT_X;
                f_TablePopupTop = TABLE_POPUP_TOP_Y;
                f_TablePopupCorner = TABLE_POPUP_TOP_RIGHT;
                break;
            case TABLE_POPUP_BOTTOM_RIGHT:
                f_TablePopupLeft = TABLE_POPUP_LEFT_X;
                f_TablePopupTop = TABLE_POPUP_TOP_Y;
                f_TablePopupCorner = TABLE_POPUP_TOP_LEFT;
                break;
        }
    }

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_TablePopupBanner)));
    for (index = 0; index != NAME_PLATE_STRIP_COUNT; index++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_TablePopupBanner) +
                index * NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT
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
            QuarterPixels(f_TablePopupLeft),
            QuarterPixels(f_TablePopupTop + index * TABLE_POPUP_STRIP_HEIGHT),
            QuarterPixels(f_TablePopupLeft + TABLE_POPUP_WIDTH),
            QuarterPixels(
                f_TablePopupTop + index * TABLE_POPUP_STRIP_HEIGHT + TABLE_POPUP_STRIP_HEIGHT
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TexelRunStep(NAME_PLATE_WIDTH, TABLE_POPUP_WIDTH),
            TexelRunStep(NAME_PLATE_STRIP_HEIGHT, TABLE_POPUP_STRIP_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    row = memory->tableFirstRow + memory->tableRow;
    rowCharacter = f_ChoiceCharacters[row];
    columnCharacter = f_ChoiceCharacters[memory->tableFirstColumn + memory->tableColumn];
    columnWins =
        save->records.awards.matchups[rowCharacter][columnCharacter].lossCount & WIN_COUNT_MASK;
    rowWins = save->records.awards.matchups[rowCharacter][columnCharacter].winBits & WIN_COUNT_MASK;
    faceMode = FACE_DARKENED;
    if (f_TableCharacterVisibility[row] != TABLE_CHARACTER_HIDDEN) {
        faceMode = FACE_PLAIN;
    }

    DrawCharacterFace(
        &gfx,
        g_CharacterFaceImages[rowCharacter],
        g_MenuCharacterPalettes[rowCharacter],
        f_TablePopupLeft + TABLE_POPUP_ROW_FACE_X,
        f_TablePopupTop + TABLE_POPUP_FACE_Y,
        rowCharacter,
        faceMode
    );
    faceMode = FACE_DARKENED;
    if (f_TableCharacterVisibility[memory->tableFirstColumn + memory->tableColumn] !=
        TABLE_CHARACTER_HIDDEN) {
        faceMode = FACE_PLAIN;
    }

    DrawCharacterFace(
        &gfx,
        g_CharacterFaceImages[columnCharacter],
        g_MenuCharacterPalettes[columnCharacter],
        f_TablePopupLeft + TABLE_POPUP_COLUMN_FACE_X,
        f_TablePopupTop + TABLE_POPUP_FACE_Y,
        columnCharacter,
        faceMode
    );
    if (rowWins < 10) {
        index = 20;
    }
    else if (rowWins >= 10 && rowWins < 100) {
        index = 12;
    }
    else if (rowWins >= 100 && rowWins < 1000) {
        index = 4;
    }
    else {
        // The same offset again, to match
        index = 4;
    }

    // Each arm draws the same number, to match
    if (rowWins > columnWins) {
        DrawUnalignedMenuNumber(
            &gfx,
            f_TablePopupLeft + index,
            f_TablePopupTop + TABLE_POPUP_WINS_Y,
            rowWins,
            MENU_NUMBER_RED_TINT
        );
    }
    else if (rowWins < columnWins) {
        DrawUnalignedMenuNumber(
            &gfx,
            f_TablePopupLeft + index,
            f_TablePopupTop + TABLE_POPUP_WINS_Y,
            rowWins,
            MENU_NUMBER_RED_TINT
        );
    }
    else {
        DrawUnalignedMenuNumber(
            &gfx,
            f_TablePopupLeft + index,
            f_TablePopupTop + TABLE_POPUP_WINS_Y,
            rowWins,
            MENU_NUMBER_RED_TINT
        );
    }

    if (columnWins < 10) {
        index = 80;
    }
    else if (columnWins >= 10 && columnWins < 100) {
        index = 72;
    }
    else if (columnWins >= 100 && columnWins < 1000) {
        index = 64;
    }
    else {
        index = 58;
    }

    // As with the row's count, to match
    if (columnWins > rowWins) {
        DrawUnalignedMenuNumber(
            &gfx,
            f_TablePopupLeft + index,
            f_TablePopupTop + TABLE_POPUP_WINS_Y,
            columnWins,
            MENU_NUMBER_GREEN_TINT
        );
    }
    else {
        DrawUnalignedMenuNumber(
            &gfx,
            f_TablePopupLeft + index,
            f_TablePopupTop + TABLE_POPUP_WINS_Y,
            columnWins,
            MENU_NUMBER_GREEN_TINT
        );
    }

    DrawMenuSign(
        &gfx,
        f_TablePopupLeft + TABLE_POPUP_SIGN_X,
        f_TablePopupTop + TABLE_POPUP_SIGN_Y,
        MENU_SIGN_TIMES
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Marks which characters the table shows: those of the character grid that
// have been unlocked, and the others once they have won a game.
void MarkShownTableCharacters(void) {
    SaveData *save = GetSaveData();
    TableCell cells[TABLE_CELL_COUNT] = {
        {0, 0},
        {0, 1},
        {1, 0},
        {1, 1},
        {2, 0},
        {2, 1},
        {3, 0},
        {3, 1},
        {4, 0},
        {4, 1},
        {5, 0},
        {5, 1},
        {6, 0},
        {0, 2},
        {1, 2},
        {2, 2},
        {3, 2},
    };
    s32 index;
    s32 recordCount;
    s32 row;
    s32 column;

    BuildCharacterGrid(1, FALSE);
    for (index = 0; index != CHARACTER_COUNT; index++) {
        f_TableCharacterVisibility[index] = TABLE_CHARACTER_HIDDEN;
    }

    for (index = 0; index != GRID_CHARACTER_COUNT; index++) {
        row = cells[index].row;
        column = cells[index].column;
        if (g_CharacterGrid[row][column] >= 0) {
            f_TableCharacterVisibility[index] = TABLE_CHARACTER_SHOWN;
        }
    }

    for (row = GRID_CHARACTER_COUNT; row != CHARACTER_COUNT; row++) {
        column = 0;
        recordCount = 0;
        for (; column != CHARACTER_COUNT; column++) {
            recordCount +=
                (save->records.awards.matchups[row][column].winBits & WIN_COUNT_MASK) != 0;
        }

        // The original stores these one entry early, over the grid's last character
        if (recordCount > 0) {
            f_TableCharacterVisibility[row - 1] = TABLE_CHARACTER_SHOWN;
        }
    }
}

// Draws the spinning star's image squeezed into the table cell at (left, top)
void DrawTableStar(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_TableStar)));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_TableStar)),
        G_IM_FMT_CI,
        STAR_WIDTH,
        STAR_HEIGHT,
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
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + TABLE_STAR_SHOWN_WIDTH),
        QuarterPixels(top + TABLE_STAR_SHOWN_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(STAR_WIDTH, TABLE_STAR_SHOWN_WIDTH),
        TexelRunStep(STAR_HEIGHT, TABLE_STAR_SHOWN_HEIGHT)
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Reads every character's face and first palette for the table to draw
void LoadTableFaces(void) {
    EncodedAsset asset;
    AssetFile file;
    CharacterIndex characterIndex;

    GetSaveData();
    for (characterIndex = 0; characterIndex != CHARACTER_COUNT; characterIndex++) {
        OpenEncodedAsset(g_CharacterFaceAssets[characterIndex], &asset);
        ReadEncodedAsset(g_CharacterFaceImages[characterIndex], &asset);
        OpenAssetFile(g_CharacterPaletteFiles[characterIndex][0], &file);
        ReadAssetFile(g_MenuCharacterPalettes[characterIndex], GetAssetFileSize(&file), &file);
    }
}
