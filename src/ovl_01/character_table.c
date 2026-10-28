/*
 * The clubhouse's character table, screen 15: the characters' wins against each
 * other, and a popup of two characters' faces with their wins.
 */
#include "common.h"

#include "asset.h"
#include "character.h"
#include "hole_display.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "screen.h"
#include "sound.h"

void func_8005C674(s32 arg0, s32 arg1);

/* The table of screen 15: its columns and rows, and how many of each fit on the
 * screen */
#define TABLE_COLUMN_COUNT 14
#define TABLE_SHOWN_COLUMN_COUNT 7
#define TABLE_ROW_COUNT 18
#define TABLE_SHOWN_ROW_COUNT 5

/* The buffers that func_802159C0 keeps, by the kind its caller gives */
#define TABLE_BUFFER_CURSOR 1
#define TABLE_BUFFER_UNREAD 2
#define TABLE_BUFFER_ARROWS 3
#define TABLE_BUFFER_STAR 4
#define ASSET_TABLE_CURSOR 0x6EF
#define ASSET_TABLE_ARROWS 0x94A
#define ASSET_TABLE_STAR 0x5BE

/* func_80217464 squeezes the star's image to fit a table cell */
#define TABLE_STAR_IMAGE_WIDTH 32
#define TABLE_STAR_IMAGE_HEIGHT 24
#define TABLE_STAR_WIDTH 24
#define TABLE_STAR_HEIGHT 20

/* Where the table's popup goes, and the cursor places that move it */
#define TABLE_POPUP_TOP_LEFT 0
#define TABLE_POPUP_TOP_RIGHT 1
#define TABLE_POPUP_BOTTOM_LEFT 2
#define TABLE_POPUP_BOTTOM_RIGHT 3
#define TABLE_POPUP_CORNER_COUNT 4
#define TABLE_POPUP_LEFT_X 68
#define TABLE_POPUP_RIGHT_X 164
#define TABLE_POPUP_TOP_Y 84
#define TABLE_POPUP_BOTTOM_Y 170
#define TABLE_POPUP_LEFT_COLUMN_LIMIT 4
#define TABLE_POPUP_RIGHT_COLUMN 3
#define TABLE_POPUP_TOP_ROW_LIMIT 2
#define TABLE_POPUP_BOTTOM_ROW 3
/* The popup is the menu banner shrunk to this size, with two faces and their
 * wins against each other */
#define TABLE_POPUP_WIDTH 120
#define TABLE_POPUP_STRIP_HEIGHT 20
#define TABLE_POPUP_ROW_FACE_X 16
#define TABLE_POPUP_COLUMN_FACE_X 74
#define TABLE_POPUP_FACE_Y 6
#define TABLE_POPUP_SIGN_X 48
#define TABLE_POPUP_SIGN_Y 12
#define TABLE_POPUP_WINS_Y 36

/* The characters that func_8021730C shows on the table, the first ones being
 * those of the character grid */
#define TABLE_CHARACTER_HIDDEN (-1)
#define TABLE_CHARACTER_SHOWN 1
#define GRID_CHARACTER_COUNT 13
#define TABLE_CELL_COUNT 17

/* A cell of the character grid */
typedef struct TableCell {
  /* 0x0 */ s32 column;
  /* 0x4 */ s32 row;
} TableCell; /* size = 0x8 */

/* The lines between the table's cells, each from one corner to the next */
#define TABLE_LINE_COUNT 12

typedef struct TableCorner {
  /* 0x0 */ s32 x;
  /* 0x4 */ s32 y;
} TableCorner; /* size = 0x8 */

/* The most bytes of each character's file that func_802177D0 keeps */
#define MENU_SCENE_FILE_SIZE 0x200

/* func_80215DC4 draws the table over a dimmed panel, with a face beside each
 * shown row and above each column */
#define TABLE_BOARD_KIND 11
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
/* The arrows that show the table goes on past the screen, as func_80212FEC
 * numbers them */
#define TABLE_ARROW_UP 0
#define TABLE_ARROW_DOWN 1
#define TABLE_ARROW_RIGHT 2
#define TABLE_ARROW_LEFT 3
#define TABLE_ROW_ARROW_X 40
#define TABLE_UP_ARROW_Y 88
#define TABLE_DOWN_ARROW_Y 222
#define TABLE_COLUMN_ARROW_Y 68
#define TABLE_LEFT_ARROW_X 64
#define TABLE_RIGHT_ARROW_X 296
#define TABLE_ARROW_BLINK_FRAMES 4

extern void* D_8012D430[CHARACTER_COUNT];
extern s32 D_80230190_ovl_01;
extern s32 D_80230194_ovl_01;
extern s32 D_80230198_ovl_01;
extern s32 D_8023019C_ovl_01;
extern s32 D_802301A0_ovl_01;
extern s32 D_802301A4_ovl_01;
extern TableCorner D_80230248_ovl_01[TABLE_LINE_COUNT * 2];
extern u16 D_802308A4_ovl_01[CHARACTER_COUNT];
extern s16 D_802308C8_ovl_01[CHARACTER_COUNT][CHARACTER_PALETTE_COUNT];
extern void* D_8023AC64_ovl_01;
extern Vtx D_802301F0_ovl_01[MENU_CURSOR_VERTEX_COUNT];
extern f32 D_80230238_ovl_01[MENU_CURSOR_VERTEX_COUNT]; /* The phase of each
                                                           corner's colour */
extern Mtx D_8023AAE0_ovl_01;
extern Mtx D_8023AB20_ovl_01;
extern Mtx D_8023AB60_ovl_01;
extern Mtx D_8023AC20_ovl_01;
extern void* D_8023AC68_ovl_01;
extern void* D_8023AC6C_ovl_01;
extern void* D_8023AC70_ovl_01;
extern s32 D_8023AC78_ovl_01[CHARACTER_COUNT];
extern u8 D_8023BFF0_ovl_01[CHARACTER_COUNT][MENU_SCENE_FILE_SIZE];
extern s32 D_802301A8_ovl_01[CHARACTER_COUNT]; /* The character in each place of
                                                  the table */
/* Which of the cursor's shown cells each popup corner would cover */
extern u8 D_80230310_ovl_01[TABLE_POPUP_CORNER_COUNT][TABLE_SHOWN_ROW_COUNT]
                           [TABLE_SHOWN_COLUMN_COUNT];
/* func_8021F124 takes an s16 character, but this file calls it without
 * narrowing the argument */
void func_8021F124_ovl_01(Gfx** displayListHead, void* texels, void* palette,
    s32 left, s32 top, s32 character, u8 drawMode);
void func_80210DF4_ovl_01(
    Gfx** displayListHead, s32 x, s32 y, s32 value, u8 arg4);
void func_801FB54C_ovl_01(Gfx** displayListHead, s32 kind);
void func_802123E0_ovl_01(Gfx** displayListHead, s32 x, s32 y, u8 sign);
void func_80212FEC_ovl_01(Gfx** displayListHead, void* image, s32 blinkFrame,
    s32 direction, s32 centerX, s32 centerY);
void func_8021640C_ovl_01(Gfx** displayListHead, s32 x, s32 y);
void func_802169CC_ovl_01(Gfx** displayListHead);
void func_80216C14_ovl_01(Gfx** displayListHead);
void func_80217464_ovl_01(Gfx** displayListHead, s32 left, s32 top);

void func_802159C0_ovl_01(void* buffer, s16 bufferKind) {
  EncodedAsset asset;
  MenuMemory* memory = func_8005AF74();
  s32 x;
  s32 y;

  switch (bufferKind) {
    case TABLE_BUFFER_CURSOR:
      D_8023AC64_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_TABLE_CURSOR, &asset);
      ReadEncodedAsset(D_8023AC64_ovl_01, &asset);
      break;
    case TABLE_BUFFER_UNREAD:
      D_8023AC68_ovl_01 = buffer;
      break;
    case TABLE_BUFFER_ARROWS:
      D_8023AC6C_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_TABLE_ARROWS, &asset);
      ReadEncodedAsset(D_8023AC6C_ovl_01, &asset);
      D_8023019C_ovl_01 = 0;
      D_802301A0_ovl_01 = 0;
      D_802301A4_ovl_01 = 0;
      break;
    case TABLE_BUFFER_STAR:
      D_8023AC70_ovl_01 = buffer;
      OpenEncodedAsset(ASSET_TABLE_STAR, &asset);
      ReadEncodedAsset(D_8023AC70_ovl_01, &asset);
      break;
  }

  /* The popup goes to the quarter of the screen away from the cursor */
  if (memory->tableColumn < TABLE_POPUP_LEFT_COLUMN_LIMIT &&
      memory->tableRow < TABLE_POPUP_TOP_ROW_LIMIT) {
    x = TABLE_POPUP_RIGHT_X;
    y = TABLE_POPUP_BOTTOM_Y;
    D_80230198_ovl_01 = TABLE_POPUP_BOTTOM_RIGHT;
  } else if (memory->tableColumn >= TABLE_POPUP_RIGHT_COLUMN &&
      memory->tableRow < TABLE_POPUP_TOP_ROW_LIMIT) {
    x = TABLE_POPUP_LEFT_X;
    y = TABLE_POPUP_BOTTOM_Y;
    D_80230198_ovl_01 = TABLE_POPUP_BOTTOM_LEFT;
  } else if (memory->tableColumn < TABLE_POPUP_LEFT_COLUMN_LIMIT &&
      memory->tableRow >= TABLE_POPUP_BOTTOM_ROW) {
    x = TABLE_POPUP_RIGHT_X;
    y = TABLE_POPUP_TOP_Y;
    D_80230198_ovl_01 = TABLE_POPUP_TOP_RIGHT;
  } else {
    x = TABLE_POPUP_LEFT_X;
    y = TABLE_POPUP_TOP_Y;
    D_80230198_ovl_01 = TABLE_POPUP_TOP_LEFT;
  }

  D_80230190_ovl_01 = x;
  D_80230194_ovl_01 = y;
}

void func_80215B84_ovl_01(void) {
  MenuMemory* memory = func_8005AF74();
  s8 firstColumn;
  s8 firstRow;
  s8 column;
  s8 row;

  if (IsHudAnimationDone() == TRUE) {
    if (g_MenuPressedButtons & B_BUTTON) {
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
      StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
      g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
      return;
    }

    firstColumn = memory->tableFirstColumn;
    firstRow = memory->tableFirstRow;
    column = memory->tableColumn;
    row = memory->tableRow;
    if (g_MenuRepeatButtons & R_JPAD) {
      PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (column == TABLE_SHOWN_COLUMN_COUNT - 1 &&
          firstColumn < TABLE_COLUMN_COUNT - TABLE_SHOWN_COLUMN_COUNT) {
        memory->tableFirstColumn++;
      }

      memory->tableColumn++;
      if (memory->tableColumn >= TABLE_SHOWN_COLUMN_COUNT) {
        memory->tableColumn = TABLE_SHOWN_COLUMN_COUNT - 1;
      }
    } else if (g_MenuRepeatButtons & L_JPAD) {
      PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (column == 0 && firstColumn > 0) {
        memory->tableFirstColumn--;
      }

      memory->tableColumn--;
      if (memory->tableColumn < 0) {
        memory->tableColumn = 0;
      }
    } else if (g_MenuRepeatButtons & D_JPAD) {
      PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
      if (row == TABLE_SHOWN_ROW_COUNT - 1 &&
          firstRow < TABLE_ROW_COUNT - TABLE_SHOWN_ROW_COUNT) {
        memory->tableFirstRow++;
      }

      memory->tableRow++;
      if (memory->tableRow >= TABLE_SHOWN_ROW_COUNT) {
        memory->tableRow = TABLE_SHOWN_ROW_COUNT - 1;
      }
    } else if (g_MenuRepeatButtons & U_JPAD) {
      PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
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

void func_80215DC4_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s16 shownRow;
  s16 shownColumn;
  s16 row;
  s16 column;
  s16 rowCharacter;
  s16 columnCharacter;
  s32 columnWins;
  s32 rowWins;
  u8 drawMode;

  func_801FB54C_ovl_01(&gfx, TABLE_BOARD_KIND);
  gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  gSPTextureRectangle(gfx++, TABLE_PANEL_LEFT << 2, TABLE_PANEL_TOP << 2,
      TABLE_PANEL_RIGHT << 2, TABLE_PANEL_BOTTOM << 2, G_TX_RENDERTILE, 0, 0,
      1 << 10, 1 << 10);
  gDPPipeSync(gfx++);
  func_802169CC_ovl_01(&gfx);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  shownRow = 0;
  row = memory->tableFirstRow;
  do {
    rowCharacter = D_802301A8_ovl_01[row];
    drawMode = FACE_DARKENED;
    if (D_8023AC78_ovl_01[row] == TABLE_CHARACTER_SHOWN) {
      drawMode = FACE_PLAIN;
    }

    func_8021F124_ovl_01(&gfx, D_8012D430[rowCharacter],
        D_8023BFF0_ovl_01[rowCharacter], TABLE_ROW_FACE_X,
        (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_ROW_FACE_Y, rowCharacter,
        drawMode);
    shownColumn = 0;
    column = memory->tableFirstColumn;
    do {
      columnCharacter = D_802301A8_ovl_01[column];
      drawMode = FACE_DARKENED;
      if (D_8023AC78_ovl_01[column] != TABLE_CHARACTER_HIDDEN) {
        drawMode = FACE_PLAIN;
      }

      if (shownRow == 0) {
        func_8021F124_ovl_01(&gfx, D_8012D430[columnCharacter],
            D_8023BFF0_ovl_01[columnCharacter],
            (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_COLUMN_FACE_X,
            TABLE_COLUMN_FACE_Y, columnCharacter, drawMode);
      }

      columnWins = save->unk990.unkAF0[rowCharacter][columnCharacter].unk0 &
          WIN_COUNT_MASK;
      rowWins = save->unk990.unkAF0[rowCharacter][columnCharacter].unk1 &
          WIN_COUNT_MASK;
      /* A pairing with the flag set shows the star, and the others show who has
       * won more */
      if (save->unk990.unkAF0[rowCharacter][columnCharacter].unk1 &
          UNKAF0_FLAG) {
        func_80217464_ovl_01(&gfx,
            (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_COLUMN_FACE_X,
            (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_STAR_Y);
      } else if (columnWins != 0) {
        func_802123E0_ovl_01(&gfx,
            (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_SIGN_X,
            (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_SIGN_Y,
            MENU_SIGN_TIMES);
      } else if (rowWins != 0) {
        func_802123E0_ovl_01(&gfx,
            (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_SIGN_X,
            (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_SIGN_Y,
            MENU_SIGN_CIRCLE);
      } else {
        func_802123E0_ovl_01(&gfx,
            (shownColumn * TABLE_CELL_WIDTH) + TABLE_FIRST_SIGN_X,
            (shownRow * TABLE_CELL_HEIGHT) + TABLE_FIRST_DASH_Y,
            MENU_SIGN_DASH);
      }

      shownColumn++;
      column++;
    } while (shownColumn != TABLE_SHOWN_COLUMN_COUNT);
    row++;
    shownRow++;
  } while (shownRow != TABLE_SHOWN_ROW_COUNT);
  if (memory->tableFirstRow > 0) {
    func_80212FEC_ovl_01(&gfx, D_8023AC6C_ovl_01, D_8023019C_ovl_01,
        TABLE_ARROW_UP, TABLE_ROW_ARROW_X, TABLE_UP_ARROW_Y);
  }

  if (memory->tableFirstRow < TABLE_ROW_COUNT - TABLE_SHOWN_ROW_COUNT) {
    func_80212FEC_ovl_01(&gfx, D_8023AC6C_ovl_01, D_8023019C_ovl_01,
        TABLE_ARROW_DOWN, TABLE_ROW_ARROW_X, TABLE_DOWN_ARROW_Y);
  }

  if (memory->tableFirstColumn > 0) {
    func_80212FEC_ovl_01(&gfx, D_8023AC6C_ovl_01, D_8023019C_ovl_01,
        TABLE_ARROW_LEFT, TABLE_LEFT_ARROW_X, TABLE_COLUMN_ARROW_Y);
  }

  if (memory->tableFirstColumn <
      TABLE_COLUMN_COUNT - TABLE_SHOWN_COLUMN_COUNT) {
    func_80212FEC_ovl_01(&gfx, D_8023AC6C_ovl_01, D_8023019C_ovl_01,
        TABLE_ARROW_RIGHT, TABLE_RIGHT_ARROW_X, TABLE_COLUMN_ARROW_Y);
  }

  D_802301A0_ovl_01++;
  if (D_802301A0_ovl_01 == TABLE_ARROW_BLINK_FRAMES) {
    D_802301A0_ovl_01 = 0;
    D_8023019C_ovl_01 = D_8023019C_ovl_01 == 0;
  }

  func_80216C14_ovl_01(&gfx);
  func_8021640C_ovl_01(&gfx,
      (memory->tableColumn * TABLE_CELL_WIDTH) + TABLE_CURSOR_X,
      (memory->tableRow * TABLE_CELL_HEIGHT) + TABLE_CURSOR_Y);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/*
 * Doesn't match: the stack slots of the pointers to the second pipe sync
 * after the image and the cycle type are swapped.
 */
#ifdef NON_MATCHING
void func_8021640C_ovl_01(Gfx** displayListHead, s32 x, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 vertexIndex;
  s32 red;
  s32 blue;
  s32 green;

  for (vertexIndex = 0; vertexIndex != MENU_CURSOR_VERTEX_COUNT;
      vertexIndex++) {
    D_80230238_ovl_01[vertexIndex] += MENU_CURSOR_PHASE_STEP;
    red =
        (s32)(sinf(D_80230238_ovl_01[vertexIndex]) * MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    blue = (s32)(sinf(D_80230238_ovl_01[vertexIndex] + (M_PI / 2)) *
               MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    green = (s32)(sinf(D_80230238_ovl_01[vertexIndex] + M_PI) *
                MENU_CURSOR_COLOR_SWING) +
        MENU_CURSOR_COLOR_MIDDLE;
    D_802301F0_ovl_01[vertexIndex].v.cn[0] = red;
    D_802301F0_ovl_01[vertexIndex].v.cn[2] = blue;
    D_802301F0_ovl_01[vertexIndex].v.cn[1] = green;
  }

  guOrtho(&D_8023AAE0_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
      -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CURSOR_NEAR,
      MENU_CURSOR_FAR, 1.0f);
  guTranslate(&D_8023AB20_ovl_01,
      x - ((SCREEN_WIDTH / 2) - (MENU_CURSOR_SIZE / 2)),
      ((SCREEN_HEIGHT / 2) - (MENU_CURSOR_SIZE / 2)) - y, MENU_CURSOR_Z);
  guRotate(&D_8023AB60_ovl_01, 0.0f, 0.0f, 1.0f, 0.0f);
  guScale(&D_8023AC20_ovl_01, 1.0f, MENU_CURSOR_SCALE_Y, 1.0f);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_NONE);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8023AC64_ovl_01 + IMAGE_HEADER_SIZE) & ~7, G_IM_FMT_IA,
      MENU_CURSOR_SIZE, MENU_CURSOR_SIZE, 0, G_TX_NOMIRROR | G_TX_WRAP,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
      G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
  gDPSetCombineMode(gfx++, G_CC_SHADEDECALA, G_CC_SHADEDECALA);
  gSPClearGeometryMode(gfx++, 0xFFFFFFFF);
  gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023AAE0_ovl_01),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023AB20_ovl_01),
      G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023AB60_ovl_01),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_8023AC20_ovl_01),
      G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  gSPVertex(gfx++, D_802301F0_ovl_01, MENU_CURSOR_VERTEX_COUNT, 0);
  gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_table", func_8021640C_ovl_01);
#endif

void func_802169CC_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 index;

  gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, 255);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
  gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
  gDPPipeSync(gfx++);
  for (index = 0; index != TABLE_LINE_COUNT * 2; index += 2) {
    gSPScisTextureRectangle(gfx++, D_80230248_ovl_01[index].x << 2,
        D_80230248_ovl_01[index].y << 2, D_80230248_ovl_01[index + 1].x << 2,
        D_80230248_ovl_01[index + 1].y << 2, G_TX_RENDERTILE, 0, 0, 1 << 10,
        1 << 10);
    gDPPipeSync(gfx++);
  }

  *displayListHead = gfx;
}

/*
 * Doesn't match: the row and column characters and their wins swap
 * registers among $s0-$s3, 25 diff lines.
 */
#ifdef NON_MATCHING
void func_80216C14_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  SaveData* save = GetSaveData();
  MenuMemory* memory = func_8005AF74();
  s32 index; /* Counts the banner strips, then holds the x of each count of wins
              */
  s32 rowIndex;
  s32 rowCharacter;
  s32 columnCharacter;
  s32 rowWins;
  s32 columnWins;
  u8 faceMode;

  if (D_80230310_ovl_01[D_80230198_ovl_01][memory->tableRow]
                       [memory->tableColumn] == TRUE) {
    switch (D_80230198_ovl_01) {
      case TABLE_POPUP_TOP_LEFT:
        D_80230190_ovl_01 = TABLE_POPUP_RIGHT_X;
        D_80230194_ovl_01 = TABLE_POPUP_BOTTOM_Y;
        D_80230198_ovl_01 = TABLE_POPUP_BOTTOM_RIGHT;
        break;
      case TABLE_POPUP_TOP_RIGHT:
        D_80230190_ovl_01 = TABLE_POPUP_LEFT_X;
        D_80230194_ovl_01 = TABLE_POPUP_BOTTOM_Y;
        D_80230198_ovl_01 = TABLE_POPUP_BOTTOM_LEFT;
        break;
      case TABLE_POPUP_BOTTOM_LEFT:
        D_80230190_ovl_01 = TABLE_POPUP_RIGHT_X;
        D_80230194_ovl_01 = TABLE_POPUP_TOP_Y;
        D_80230198_ovl_01 = TABLE_POPUP_TOP_RIGHT;
        break;
      case TABLE_POPUP_BOTTOM_RIGHT:
        D_80230190_ovl_01 = TABLE_POPUP_LEFT_X;
        D_80230194_ovl_01 = TABLE_POPUP_TOP_Y;
        D_80230198_ovl_01 = TABLE_POPUP_TOP_LEFT;
        break;
    }
  }

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, ((u32)D_8023AC68_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  for (index = 0; index != MENU_BANNER_STRIP_COUNT; index++) {
    gDPLoadTextureBlock(gfx++,
        ((u32)D_8023AC68_ovl_01 + LARGE_PALETTED_TEXELS_OFFSET +
            index * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, D_80230190_ovl_01 << 2,
        (D_80230194_ovl_01 + index * TABLE_POPUP_STRIP_HEIGHT) << 2,
        (D_80230190_ovl_01 + TABLE_POPUP_WIDTH) << 2,
        (D_80230194_ovl_01 + index * TABLE_POPUP_STRIP_HEIGHT +
            TABLE_POPUP_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, (MENU_BANNER_WIDTH << 10) / TABLE_POPUP_WIDTH,
        (MENU_BANNER_STRIP_HEIGHT << 10) / TABLE_POPUP_STRIP_HEIGHT);
    gDPPipeSync(gfx++);
  }

  rowIndex = memory->tableFirstRow + memory->tableRow;
  rowCharacter = D_802301A8_ovl_01[rowIndex];
  columnCharacter =
      D_802301A8_ovl_01[memory->tableFirstColumn + memory->tableColumn];
  columnWins = save->unk990.unkAF0[rowCharacter][columnCharacter].unk0 & 0x7F;
  rowWins = save->unk990.unkAF0[rowCharacter][columnCharacter].unk1 & 0x7F;
  faceMode = FACE_DARKENED;
  if (D_8023AC78_ovl_01[rowIndex] != TABLE_CHARACTER_HIDDEN) {
    faceMode = FACE_PLAIN;
  }

  func_8021F124_ovl_01(&gfx, D_8012D430[rowCharacter],
      D_8023BFF0_ovl_01[rowCharacter],
      D_80230190_ovl_01 + TABLE_POPUP_ROW_FACE_X,
      D_80230194_ovl_01 + TABLE_POPUP_FACE_Y, rowCharacter, faceMode);
  faceMode = FACE_DARKENED;
  if (D_8023AC78_ovl_01[memory->tableFirstColumn + memory->tableColumn] !=
      TABLE_CHARACTER_HIDDEN) {
    faceMode = FACE_PLAIN;
  }

  func_8021F124_ovl_01(&gfx, D_8012D430[columnCharacter],
      D_8023BFF0_ovl_01[columnCharacter],
      D_80230190_ovl_01 + TABLE_POPUP_COLUMN_FACE_X,
      D_80230194_ovl_01 + TABLE_POPUP_FACE_Y, columnCharacter, faceMode);
  if (rowWins < 10) {
    index = 20;
  } else if (rowWins >= 10 && rowWins < 100) {
    index = 12;
  } else if (rowWins >= 100 && rowWins < 1000) {
    index = 4;
  } else {
    index = 4;
  }

  func_80210DF4_ovl_01(&gfx, D_80230190_ovl_01 + index,
      D_80230194_ovl_01 + TABLE_POPUP_WINS_Y, rowWins, MENU_NUMBER_RED_TINT);
  if (columnWins < 10) {
    index = 80;
  } else if (columnWins >= 10 && columnWins < 100) {
    index = 72;
  } else if (columnWins >= 100 && columnWins < 1000) {
    index = 64;
  } else {
    index = 58;
  }

  func_80210DF4_ovl_01(&gfx, D_80230190_ovl_01 + index,
      D_80230194_ovl_01 + TABLE_POPUP_WINS_Y, columnWins,
      MENU_NUMBER_GREEN_TINT);
  func_802123E0_ovl_01(&gfx, D_80230190_ovl_01 + TABLE_POPUP_SIGN_X,
      D_80230194_ovl_01 + TABLE_POPUP_SIGN_Y, MENU_SIGN_TIMES);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/character_table", func_80216C14_ovl_01);
#endif

void func_8021730C_ovl_01(void) {
  SaveData* save = GetSaveData();
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

  func_8005C674(1, 0);
  for (index = 0; index != CHARACTER_COUNT; index++) {
    D_8023AC78_ovl_01[index] = TABLE_CHARACTER_HIDDEN;
  }

  for (index = 0; index != GRID_CHARACTER_COUNT; index++) {
    row = cells[index].row;
    column = cells[index].column;
    if (g_CharacterGrid[row][column] >= 0) {
      D_8023AC78_ovl_01[index] = TABLE_CHARACTER_SHOWN;
    }
  }

  for (row = GRID_CHARACTER_COUNT; row != CHARACTER_COUNT; row++) {
    column = 0;
    recordCount = 0;
    for (; column != SAVE_UNKAF0_COLUMN_COUNT; column++) {
      recordCount += (save->unk990.unkAF0[row][column].unk1 & 0x7F) != 0;
    }

    /* The original stores these one entry early, over the grid's last character
     */
    if (recordCount > 0) {
      D_8023AC78_ovl_01[row - 1] = TABLE_CHARACTER_SHOWN;
    }
  }
}

void func_80217464_ovl_01(Gfx** displayListHead, s32 left, s32 top) {
  Gfx* gfx = *displayListHead;

  gDPPipeSync(gfx++);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8023AC70_ovl_01 + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(gfx++,
      ((u32)D_8023AC70_ovl_01 + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
      TABLE_STAR_IMAGE_WIDTH, TABLE_STAR_IMAGE_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gSPScisTextureRectangle(gfx++, left << 2, top << 2,
      (left + TABLE_STAR_WIDTH) << 2, (top + TABLE_STAR_HEIGHT) << 2,
      G_TX_RENDERTILE, 0, 0, (TABLE_STAR_IMAGE_WIDTH << 10) / TABLE_STAR_WIDTH,
      (TABLE_STAR_IMAGE_HEIGHT << 10) / TABLE_STAR_HEIGHT);
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

void func_802177D0_ovl_01(void) {
  EncodedAsset asset;
  AssetFile file;
  s32 characterIndex;

  GetSaveData();
  for (characterIndex = 0; characterIndex != CHARACTER_COUNT;
      characterIndex++) {
    OpenEncodedAsset(D_802308A4_ovl_01[characterIndex], &asset);
    ReadEncodedAsset(D_8012D430[characterIndex], &asset);
    OpenAssetFile(D_802308C8_ovl_01[characterIndex][0], &file);
    ReadAssetFile(
        D_8023BFF0_ovl_01[characterIndex], GetAssetFileSize(&file), &file);
  }
}
