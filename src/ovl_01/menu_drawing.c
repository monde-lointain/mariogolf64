#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "character.h"
#include "character_slot.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "game_flags.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu.h"
#include "save.h"
#include "screen.h"
#include "text.h"
#include "variables.h"

/* Labels in the shadowed font */
extern char g_ContinueLabelText[];
extern char g_BirdieBadgeLabelText[];

extern s32 D_8022F8C8_ovl_01;
extern s8 D_8022F8CD_ovl_01;
extern u8 D_8022F8CC_ovl_01; /* Which of D_801B93E0's buffers holds the star
                                frame shown */
extern void* D_801B93E0[];
extern s32 D_8022F848_ovl_01[]; /* The star's frames */
extern u8 D_8022F8CE_ovl_01;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern void* D_8010526C[2];
extern void* D_800FBE30[2];

/* The 24x24 icon that func_80207E28 animates, such as the birdie badge; each
 * frame shows for two calls */
#define ANIMATED_ICON_SIZE 24
#define ANIMATED_ICON_IMAGE_SIZE (ANIMATED_ICON_SIZE * ANIMATED_ICON_SIZE / 2)
#define ANIMATED_ICON_FRAME_COUNT 6
#define ANIMATED_ICON_FRAME_DURATION 2
/* func_802081E8 draws a row of spinning stars, one per hole, from D_801B93E0,
 * and enlarges the chosen hole's */
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
#define STAR_FRAME_COUNT 15
#define MAX_STAR_COUNT 6
#define CHOSEN_STAR_GROWTH 8
/* func_80200A40 draws the chosen mode's icon, 64x64 CI8, in two strips */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* Where func_802004A0 draws the mode icon, unscaled */
#define MODE_ICON_LEFT 168
#define MODE_ICON_TOP 96

/* func_80200EF0 draws the players' characters, and in Get Character the rival
 * as a spinning silhouette */
#define NO_RIVAL -1
#define RIVAL_SLOT 1
#define RIVAL_SPIN_STEP 10.0f
#define MENU_CHARACTERS_NEAR 1.0f
#define MENU_CHARACTERS_FAR 3000.0f
#define MENU_CHARACTERS_ORTHO_SCALE 10.0f
#define MENU_CHARACTER_LAYOUT_COUNT 4
#define MENU_CHARACTER_SLOT_COUNT 5

extern f32 D_8022FA54_ovl_01;
extern f32 D_8022FA58_ovl_01;
extern f32 D_8022FA5C_ovl_01;
extern f32 D_8022FA68_ovl_01[MENU_CHARACTER_LAYOUT_COUNT]
                            [MENU_CHARACTER_SLOT_COUNT][3];
extern s32
    D_8022FB58_ovl_01[CHARACTER_COUNT]; /* The character for each grid mark */
extern Mtx D_80231D60_ovl_01[MAX_PLAYER_COUNT][2];
extern Mtx D_80231F60_ovl_01;
extern f32 D_80231FA0_ovl_01[MAX_PLAYER_COUNT];
extern s32 D_8023BFC0_ovl_01[MAX_PLAYER_COUNT];
extern u8 D_801B70F0[MAXCONTROLLERS];

/* func_80208F8C draws a frame of the waving Toad (assets 0x710 onward), 48x51
 * CI8, in two strips */
#define TOAD_WIDTH 48
#define TOAD_STRIP_HEIGHT 30
#define TOAD_LAST_STRIP_HEIGHT 21
#define TOAD_STRIP_COUNT 2

/* Where the debug caddy select draws its title and names */
#define CADDY_SELECT_TITLE_X 64
#define CADDY_SELECT_TITLE_Y 32
#define CADDY_SELECT_NAME_X 96
#define CADDY_SELECT_FIRST_Y 64
#define CADDY_SELECT_LINE_HEIGHT 8

extern char* D_8022C0D0_ovl_01[CADDY_COUNT];
extern const char D_8023176C_ovl_01[];

/* The D_8022BE88 phase in which the course select's banners slide the other way
 */
#define MENU_PHASE_UNK7 7

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_801FE4D0_ovl_01);

/* Nothing calls this */
void func_802004A0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPPipeSync(gfx++);
  gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_NONE);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_POINT);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(
        gfx++,
        ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
         stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++, MODE_ICON_LEFT << 2,
        (MODE_ICON_TOP + stripIndex * MODE_ICON_STRIP_HEIGHT) << 2,
        (MODE_ICON_LEFT + MODE_ICON_WIDTH) << 2,
        (MODE_ICON_TOP + MODE_ICON_STRIP_HEIGHT +
         stripIndex * MODE_ICON_STRIP_HEIGHT)
            << 2,
        G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
    gDPPipeSync(gfx++);
  }
  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
    iconIndex = D_8022BEBC_ovl_01[D_80105B68 - 1][D_801323A0.modeChoice];
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }
    if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] ==
        END_OF_ICON_FRAMES) {
      D_8022F8C0_ovl_01 = 0;
    }
    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01],
                     &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
  }
  *displayListHead = gfx;
}

void func_80200A40_ovl_01(Gfx** displayListHead, s32 left, s32 top,
                          s32 halfSize, s32 scale) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  s32 stripIndex;
  void* image;
  u8 iconIndex;

  gDPPipeSync(gfx++);
  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(
        gfx++,
        ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
         stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, left << 2,
                            (top + stripIndex * halfSize) << 2,
                            (left + halfSize + halfSize) << 2,
                            (top + halfSize + stripIndex * halfSize) << 2,
                            G_TX_RENDERTILE, 0, 0, scale, scale);
    gDPPipeSync(gfx++);
  }
  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
    iconIndex = D_8022BEBC_ovl_01[D_80105B68 - 1][D_801323A0.modeChoice];
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }
    if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] ==
        END_OF_ICON_FRAMES) {
      D_8022F8C0_ovl_01 = 0;
    }
    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01],
                     &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
  }
  *displayListHead = gfx;
}

/*
 * Doesn't match: the original fills the delay slot of the rival test before
 * SetCharacterLighting with its first argument, and this leaves it empty.
 */
#ifdef NON_MATCHING
void func_80200EF0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 rival = NO_RIVAL;
  MenuMemory* memory;
  s32 characterCount;
  s32 row;
  s32 column;
  s32 layout;
  s32 character;
  s32 palette;
  s32 raise;
  u16 slot;
  Vec3 translation;
  Vec3 rotation;
  Vec3 scale;
  s32 unused[12];
  Mat4 transform;

  memory = func_8005AF74();
  characterCount = D_80105B68;
  if (D_8022BEBC_ovl_01[characterCount - 1][memory->modeChoice] ==
      MODE_ICON_GET_CHARACTER) {
    if (characterCount == 1) {
      for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
          if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
            rival = D_8022FB58_ovl_01[g_CharacterGrid[row][column] +
                                      MARKED_CHARACTER_OFFSET];
          }
        }
      }
    }
  }
  characterCount += (rival >= 0);
  switch (characterCount) {
    case 1:
      D_80231FA0_ovl_01[0] = 5.0f;
      break;
    case 2:
      D_80231FA0_ovl_01[0] = (rival >= 0) ? 5.0f : 4.0f;
      D_80231FA0_ovl_01[1] = 4.0f;
      break;
    case 3:
    case 4:
      D_80231FA0_ovl_01[0] = 3.5f;
      D_80231FA0_ovl_01[1] = 3.4f;
      D_80231FA0_ovl_01[2] = 3.4f;
      D_80231FA0_ovl_01[3] = 3.5f;
      break;
    default:
      D_80231FA0_ovl_01[0] = 4.0f;
      D_80231FA0_ovl_01[1] = 4.0f;
      D_80231FA0_ovl_01[2] = 4.0f;
      D_80231FA0_ovl_01[3] = 4.0f;
      break;
  }
  guOrtho(&D_80231F60_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2,
          -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2, MENU_CHARACTERS_NEAR,
          MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
  for (slot = 0; slot != characterCount; slot++) {
    if (D_8023BFC0_ovl_01[slot] > 0) {
      D_8023BFC0_ovl_01[slot]--;
    }
    if (D_8023BFC0_ovl_01[slot] == 0) {
      D_8023BFC0_ovl_01[slot] = -1;
      SetAnimationLooping(slot, FALSE);
      SetNextAnimation(slot, MENU_ANIMATION_FIDGET);
    }
    if (D_8023BFC0_ovl_01[slot] == -1) {
      if (GetAnimationEndFrame(slot, MENU_ANIMATION_FIDGET) ==
          GetAnimationFrame(slot)) {
        SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
        SetAnimationLooping(slot, TRUE);
        D_8023BFC0_ovl_01[slot] = guRandom() % FIDGET_DELAY_RANGE;
      }
    }
    if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
      character = rival;
      palette = 1;
    } else {
      character = (s8)g_Players[slot].characterIndex;
      palette = (s8)g_Players[slot].paletteIndex;
    }
    if (!D_801B70F0[slot]) {
      if (IsCharacterSlotBusy(slot)) {
        FreeCharacterSlot(slot);
      } else {
        LoadCharacter(slot, character + CHARACTER_COUNT, palette);
        D_801B70F0[slot] = TRUE;
        SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
        SetAnimationSpeed(slot, 1.0f);
        if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
          SetCharacterLighting(RIVAL_SLOT, CHARACTER_LIGHTING_BLACK);
        }
      }
    }
    layout = 0;
    if (rival < 0) {
      layout = characterCount - 1;
    }
    if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
      D_8022FA5C_ovl_01 -= RIVAL_SPIN_STEP;
      D_8022FA58_ovl_01 = D_8022FA5C_ovl_01;
    } else {
      D_8022FA58_ovl_01 = 0.0f;
    }
    rotation[0] = DEGREES_TO_RADIANS(D_8022FA54_ovl_01);
    rotation[1] = DEGREES_TO_RADIANS(D_8022FA58_ovl_01);
    rotation[2] = 0.0f;
    translation[0] = D_8022FA68_ovl_01[layout][slot][0];
    raise = (character == CHARACTER_BABY_MARIO) ? BABY_MARIO_RAISE : 0;
    translation[1] = D_8022FA68_ovl_01[layout][slot][1] + raise;
    translation[2] = D_8022FA68_ovl_01[layout][slot][2];
    scale[0] = D_80231FA0_ovl_01[slot];
    scale[1] = D_80231FA0_ovl_01[slot];
    scale[2] = D_80231FA0_ovl_01[slot];
    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(transform,
                         &D_80231D60_ovl_01[slot][g_DrawBufferIndex]);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231F60_ovl_01),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
    gSPMatrix(gfx++,
              OS_K0_TO_PHYSICAL(&D_80231D60_ovl_01[slot][g_DrawBufferIndex]),
              G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    g_DisplayListHead = gfx;
    DrawCharacter(slot);
    *displayListHead = g_DisplayListHead;
    gfx = *displayListHead;
    AnimateCharacter(slot);
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_80200EF0_ovl_01);
#endif

/*
 * A debug character viewer. START hides the board and turns on the controls:
 * the stick turns the character, the C buttons move it, A and B scale it, left
 * and right pick an animation, R plays it, Z with the C buttons steps through
 * it and L changes the character's kind.
 */
#define VIEWER_BOARD_KIND 11
#define VIEWER_NAME_X 32
#define VIEWER_NAME_Y 192
#define VIEWER_PLAYER_NUMBER_X 144
#define VIEWER_PLAYER_NAME_X 176
#define VIEWER_PLAYER_TOP 64
#define VIEWER_PLAYER_SPACING 18
#define VIEWER_ANIMATION_X 64
#define VIEWER_ANIMATION_Y 56
#define VIEWER_CLUB_X 144
#define VIEWER_POWER_Y 184
#define VIEWER_SPIN_Y 192
#define VIEWER_SIDE_SPIN_Y 200
#define VIEWER_STICK_DEAD_ZONE 20
#define VIEWER_STICK_DIVISOR 4
#define VIEWER_SCALE_STEP 0.05f
#define VIEWER_LAST_ANIMATION 34
/* From this animation on, the character's second model plays it */
#define VIEWER_FIRST_SECOND_MODEL_ANIMATION 12
#define VIEWER_SECOND_MODEL_OFFSET 18
#define VIEWER_KIND_COUNT 5
#define VIEWER_HALF_WIDTH 160.0f
#define VIEWER_HALF_HEIGHT 120.0f
#define VIEWER_NEAR 1.0f
#define VIEWER_FAR 3000.0f
#define VIEWER_PERSPECTIVE_SCALE 10.0f
#define VIEWER_BASE_SCALE 5.0f
#define VIEWER_BASE_OFFSET -64.0f
#define VIEWER_DEPTH -1000.0f

extern s32 D_8022BE24_ovl_01;
extern s8 D_8022BE81_ovl_01;
extern u8 D_8022BE94_ovl_01;
extern s32 D_8022FBA0_ovl_01;
extern s32 D_8022FBA4_ovl_01;
extern f32 D_8022FBA8_ovl_01;
extern f32 D_8022FBAC_ovl_01;
extern f32 D_8022FBB0_ovl_01;
extern u32 D_8022FBB4_ovl_01;
extern f32 D_8022FBB8_ovl_01;
extern f32 D_8022FBBC_ovl_01;
extern char D_802312E0_ovl_01[];
extern char D_802312E8_ovl_01[];
extern char D_802312F0_ovl_01[];
extern Mtx D_80231FB0_ovl_01[2];
extern Mtx D_80232030_ovl_01;

const char D_80231660_ovl_01[] = "%d:%s";
const char D_80231668_ovl_01[] = "%d:";
const char D_8023166C_ovl_01[] = "%d";
const char D_80231670_ovl_01[] = "POWER:%3d";
const char D_8023167C_ovl_01[] = "SPIN :%3d";

void func_80201574_ovl_01(Gfx** displayListHead) {
  Gfx* gfx;
  s32 index;
  s32 stickX;
  s32 stickY;
  s32 character;

  gfx = *displayListHead;

  if (g_ControllerStates[0].pressedButtons & START_BUTTON) {
    D_8022BE28_ovl_01 ^= 1;
  }
  if (D_8022BE28_ovl_01 == 0) {
    func_801FB54C_ovl_01(&gfx, VIEWER_BOARD_KIND);
  }
  SetUpTextDrawing(&gfx);
  gDPSetPrimColor(gfx++, 0, 0, 0x3F, 0x7F, 0xFF, 0xFF);
  gDPSetEnvColor(gfx++, 0x00, 0x10, 0x40, 0xFF);
  sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.unk2 + 1,
          D_8022BF70_ovl_01[D_801323A0.unk2]);
  DrawLargeText(&gfx, VIEWER_NAME_X, VIEWER_NAME_Y, g_PrintBuffer);
  for (index = 0; index != D_80105B68; index++) {
    sprintf(g_PrintBuffer, D_80231668_ovl_01, index + 1);
    DrawTextStub(&gfx, VIEWER_PLAYER_NUMBER_X,
                 VIEWER_PLAYER_TOP + index * VIEWER_PLAYER_SPACING,
                 g_PrintBuffer);
  }
  for (index = 0; index != D_801B7070; index++) {
    sprintf(g_PrintBuffer, D_80231514_ovl_01,
            D_8022BF70_ovl_01[D_8012F480[index]]);
    DrawTextStub(&gfx, VIEWER_PLAYER_NAME_X,
                 VIEWER_PLAYER_TOP + index * VIEWER_PLAYER_SPACING,
                 g_PrintBuffer);
  }
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  sprintf(g_PrintBuffer, D_8023166C_ovl_01, D_8022BE24_ovl_01);
  DrawShadowedText(&gfx, VIEWER_ANIMATION_X, VIEWER_ANIMATION_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, D_80231670_ovl_01,
          g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].distance);
  DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_POWER_Y, g_PrintBuffer);
  sprintf(g_PrintBuffer, D_8023167C_ovl_01,
          g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].spin);
  DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SPIN_Y, g_PrintBuffer);
  if (g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].sideSpin > 0) {
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y,
                     D_802312E8_ovl_01);
  } else if (g_Clubs[D_801323A0.unk2 * CLUB_SET_SIZE].sideSpin < 0) {
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y,
                     D_802312E0_ovl_01);
  } else {
    DrawShadowedText(&gfx, VIEWER_CLUB_X, VIEWER_SIDE_SPIN_Y,
                     D_802312F0_ovl_01);
  }
  if (D_8022BE28_ovl_01 == 0) {
    ClearGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
  } else {
    stickX = g_ControllerStates[0].stickX;
    stickY = g_ControllerStates[0].stickY;
    SetGameFlag(GAME_FLAG_SHOW_ANIMATION_FRAMES);
    if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
      if (stickX > 0) {
        stickX -= VIEWER_STICK_DEAD_ZONE;
        if (stickX < 0) {
          stickX = 0;
        }
        stickX /= VIEWER_STICK_DIVISOR;
      } else if (stickX < 0) {
        stickX += VIEWER_STICK_DEAD_ZONE;
        if (stickX > 0) {
          stickX = 0;
        }
        stickX /= VIEWER_STICK_DIVISOR;
      }
      if (stickY > 0) {
        stickY -= VIEWER_STICK_DEAD_ZONE;
        if (stickY < 0) {
          stickY = 0;
        }
        stickY /= VIEWER_STICK_DIVISOR;
      } else if (stickY < 0) {
        stickY += VIEWER_STICK_DEAD_ZONE;
        if (stickY > 0) {
          stickY = 0;
        }
        stickY /= VIEWER_STICK_DIVISOR;
      }
      D_8022FBA8_ovl_01 += stickY;
      D_8022FBAC_ovl_01 += stickX;
      if (g_ControllerStates[0].buttons & L_CBUTTONS) {
        D_8022FBB8_ovl_01 -= 1.0f;
      }
      if (g_ControllerStates[0].buttons & R_CBUTTONS) {
        D_8022FBB8_ovl_01 += 1.0f;
      }
      if (g_ControllerStates[0].buttons & U_CBUTTONS) {
        D_8022FBBC_ovl_01 += 1.0f;
      }
      if (g_ControllerStates[0].buttons & D_CBUTTONS) {
        D_8022FBBC_ovl_01 -= 1.0f;
      }
      if (g_ControllerStates[0].buttons & B_BUTTON) {
        D_8022FBB0_ovl_01 -= VIEWER_SCALE_STEP;
      }
      if (g_ControllerStates[0].buttons & A_BUTTON) {
        D_8022FBB0_ovl_01 += VIEWER_SCALE_STEP;
      }
    }
    if ((g_MenuPressedButtons & L_JPAD) && D_8022BE24_ovl_01 > 0) {
      D_8022BE24_ovl_01--;
      D_8022FBA4_ovl_01 = TRUE;
    }
    if ((g_MenuPressedButtons & R_JPAD) &&
        D_8022BE24_ovl_01 < VIEWER_LAST_ANIMATION) {
      D_8022BE24_ovl_01++;
      D_8022FBA4_ovl_01 = TRUE;
    }
    if (g_ControllerStates[0].buttons & Z_TRIG) {
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
      if (g_ControllerStates[0].repeatedButtons & L_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, -1.0f);
      }
      if (g_ControllerStates[0].repeatedButtons & R_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, 1.0f);
      }
      if (g_ControllerStates[0].buttons & U_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, 4.0f);
      }
      if (g_ControllerStates[0].buttons & D_CBUTTONS) {
        SetAnimationSpeed(D_8022FBB4_ovl_01, -4.0f);
      }
      AnimateAllCharacters();
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
    }
    if (g_ControllerStates[0].buttons & R_TRIG) {
      if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
        SetCharacterAnimation(D_8022FBB4_ovl_01, D_8022BE24_ovl_01);
      }
      SetAnimationSpeed(D_8022FBB4_ovl_01, 1.0f);
    }
    if ((g_MenuRepeatButtons & L_TRIG) && D_8022BE94_ovl_01 == FALSE) {
      D_8022BE81_ovl_01++;
      if (D_8022BE81_ovl_01 >= VIEWER_KIND_COUNT) {
        D_8022BE81_ovl_01 = 0;
      }
      if (D_8022BE81_ovl_01 < 0) {
        D_8022BE81_ovl_01 = VIEWER_KIND_COUNT - 1;
      }
      D_8022BE94_ovl_01 = TRUE;
    }
  }
  if (D_8022FBA0_ovl_01 != D_801323A0.unk2 || D_8022FBA4_ovl_01 == TRUE ||
      D_8022BE94_ovl_01 == TRUE) {
    if (IsCharacterSlotBusy(D_8022FBB4_ovl_01)) {
      FreeCharacterSlot(D_8022FBB4_ovl_01);
    } else {
      character = D_801323A0.unk2;
      D_8022BE94_ovl_01 = FALSE;
      D_8022FBA4_ovl_01 = FALSE;
      D_8022FBA0_ovl_01 = character;
      if (D_8022BE24_ovl_01 >= VIEWER_FIRST_SECOND_MODEL_ANIMATION) {
        LoadCharacter(D_8022FBB4_ovl_01, character + VIEWER_SECOND_MODEL_OFFSET,
                      D_8022BE81_ovl_01);
      } else {
        LoadCharacter(D_8022FBB4_ovl_01, character, D_8022BE81_ovl_01);
      }
      SetCharacterAnimation(D_8022FBB4_ovl_01, D_8022BE24_ovl_01);
      SetAnimationSpeed(D_8022FBB4_ovl_01, 0.0f);
    }
  }
  AnimateAllCharacters();
  guOrtho(&D_80232030_ovl_01, -VIEWER_HALF_WIDTH, VIEWER_HALF_WIDTH,
          -VIEWER_HALF_HEIGHT, VIEWER_HALF_HEIGHT, VIEWER_NEAR, VIEWER_FAR,
          VIEWER_PERSPECTIVE_SCALE);
  {
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Mat4 matrix;

    rotation[2] = 0;
    rotation[0] = DEGREES_TO_RADIANS(D_8022FBA8_ovl_01);
    rotation[1] = DEGREES_TO_RADIANS(D_8022FBAC_ovl_01);
    translation[0] = D_8022FBB8_ovl_01 + VIEWER_BASE_OFFSET;
    translation[1] = D_8022FBBC_ovl_01 + VIEWER_BASE_OFFSET;
    translation[2] = VIEWER_DEPTH;
    scale[0] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    scale[1] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    scale[2] = D_8022FBB0_ovl_01 + VIEWER_BASE_SCALE;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &D_80231FB0_ovl_01[g_DrawBufferIndex]);
  }
  gDPPipeSync(gfx++);
  gDPSetTexturePersp(gfx++, G_TP_PERSP);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPPipeSync(gfx++);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80232030_ovl_01),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
  gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231FB0_ovl_01[g_DrawBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
  gDPPipeSync(gfx++);
  g_DisplayListHead = gfx;
  DrawCharacter(D_8022FBB4_ovl_01);
  gfx = *displayListHead = g_DisplayListHead;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_80202000_ovl_01);

/*
 * func_802047F0 draws the course select's banners of D_801B60D0: one behind
 * the course's name, and while the holes can be chosen, a wider one below
 * it behind the choice made, which squeezes the first.
 */
#define COURSE_NAME_BANNER_LEFT 67
#define COURSE_NAME_BANNER_TOP 168
#define COURSE_NAME_BANNER_WIDTH 190
#define COURSE_NAME_BANNER_SQUEEZE 48
#define HOLES_BANNER_LEFT 47
#define HOLES_BANNER_TOP 190
#define HOLES_BANNER_WIDTH 230
#define HOLES_BANNER_STRIP_HEIGHT 11
#define COURSE_NAME_Y 176
#define HOLES_CHOICE_Y 202
/* A HOLES_CHOICE_* value plus this is the message that explains it */
#define HOLES_CHOICE_MESSAGE_BASE 0xC3
#define HOLES_CHOICE_TEXT_SLOT 1

extern char* D_8022FDEC_ovl_01[MINI_GOLF_RECORD_COUNT];
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];

/*
 * Doesn't match: the original sets the text drawing's argument before the
 * mini-golf test that picks the name's colours, and keeps the course name
 * banner's strip top in one register instead of copying it.
 */
#ifdef NON_MATCHING
void func_802047F0_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 slide = D_8023BFD0_ovl_01;
  /* Also the course's index into the name colours, to match the original's
   * registers */
  s32 stripIndex;
  s32 stripTop;
  s32 squeeze;
  s32 cameraModeTopByte;

  if (D_8022BE88_ovl_01 == MENU_PHASE_UNK7) {
    slide = -slide;
  }
  gDPPipeSync(gfx++);
  gDPSetTextureLUT(gfx++, G_TT_RGBA16);
  gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
  if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
      gDPLoadTextureBlock(
          gfx++,
          ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
           stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) &
              ~7,
          G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
          0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      stripTop = HOLES_BANNER_TOP + stripIndex * HOLES_BANNER_STRIP_HEIGHT;
      gSPScisTextureRectangle(
          gfx++, HOLES_BANNER_LEFT << 2, (slide + stripTop) << 2,
          (HOLES_BANNER_LEFT + HOLES_BANNER_WIDTH) << 2,
          (slide + stripTop + HOLES_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE,
          0, 0, (MENU_BANNER_WIDTH << 10) / HOLES_BANNER_WIDTH,
          (MENU_BANNER_STRIP_HEIGHT << 10) / HOLES_BANNER_STRIP_HEIGHT);
      gDPPipeSync(gfx++);
    }
  }
  squeeze = MAP_CAMERA_MODE_TOP_BYTE != 0 ? COURSE_NAME_BANNER_SQUEEZE : 0;
  for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
    gDPLoadTextureBlock(
        gfx++,
        ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
         stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT,
        0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    stripTop = COURSE_NAME_BANNER_TOP + stripIndex * MENU_BANNER_STRIP_HEIGHT;
    gSPScisTextureRectangle(
        gfx++, (COURSE_NAME_BANNER_LEFT + squeeze / 2) << 2,
        (slide + stripTop) << 2,
        (COURSE_NAME_BANNER_LEFT + COURSE_NAME_BANNER_WIDTH - squeeze / 2) << 2,
        (slide + stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0,
        0, (MENU_BANNER_WIDTH << 10) / (COURSE_NAME_BANNER_WIDTH - squeeze),
        TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }
  SetUpTextDrawing(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  if (!g_IsMiniGolf) {
    if (D_800FF4C0 == 1) {
      sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
              g_CourseRecords[D_801323A0.courseChoice].name);
    } else {
      sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
              g_TournamentNames[D_801323A0.courseChoice]);
    }
  } else {
    sprintf(g_PrintBuffer, D_80231660_ovl_01, g_MenuChoices.unk13 + 1,
            D_8022FDEC_ovl_01[g_MenuChoices.unk13]);
  }
  SetUpGradientTextDrawing(&gfx, 0);
  if (g_IsMiniGolf == TRUE) {
    stripIndex = g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX;
  } else {
    stripIndex = D_801323A0.courseChoice;
  }
  DrawLargeTextWithShadow(&gfx, SCREEN_WIDTH / 2, slide + COURSE_NAME_Y,
                          g_PrintBuffer, g_CourseNameEnvColors[stripIndex],
                          g_CourseNamePrimColors[stripIndex],
                          TEXT_FLAG_CENTERED);
  cameraModeTopByte = MAP_CAMERA_MODE_TOP_BYTE;
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  if (cameraModeTopByte == 1) {
    gDPPipeSync(gfx++);
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    SetTextSlot(SCREEN_WIDTH / 2, slide + HOLES_CHOICE_Y,
                g_MenuChoices.unk0 + HOLES_CHOICE_MESSAGE_BASE, 1,
                HOLES_CHOICE_TEXT_SLOT, TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20,
                255, 255, 255, 255);
    DrawLargeTextSlot(&gfx, HOLES_CHOICE_TEXT_SLOT);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_802047F0_ovl_01);
#endif

void DrawContinueLabel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  DrawShadowedText(&gfx, 96, 64, g_ContinueLabelText);
  *displayListHead = gfx;
}

/* The STATUS, HOW TO PLAY, CODE ENTRY and OPTIONS labels, 128 by 20 each, plain
 * in D_801B5CD0 and lit in D_8012D420 */
#define MAIN_CHOICE_LABEL_WIDTH 128
#define MAIN_CHOICE_LABEL_HEIGHT 20
#define MAIN_CHOICE_LABEL_IMAGE_SIZE \
  (MAIN_CHOICE_LABEL_WIDTH * MAIN_CHOICE_LABEL_HEIGHT / 2)

extern s32 D_8022FDF4_ovl_01[MAIN_CHOICE_COUNT][2];

void func_80205038_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;

  func_801FB54C_ovl_01(&gfx, 61);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_COUNT; choiceIndex++) {
    if (g_MenuChoices.mainChoice != choiceIndex) {
      gDPLoadTextureBlock_4b(
          gfx++,
          ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
           choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE) &
              ~7,
          G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++, D_8022FDF4_ovl_01[choiceIndex][0] << 2,
          D_8022FDF4_ovl_01[choiceIndex][1] << 2,
          (D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH) << 2,
          (D_8022FDF4_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  choiceIndex = g_MenuChoices.mainChoice;
  gDPLoadTextureBlock_4b(
      gfx++,
      ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
       choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE) &
          ~7,
      G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(
      gfx++, (D_8022FDF4_ovl_01[choiceIndex][0] - D_800FE33C / 2) << 2,
      (D_8022FDF4_ovl_01[choiceIndex][1] - D_800FE33C / 2) << 2,
      (D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH +
       D_800FE33C / 2)
          << 2,
      (D_8022FDF4_ovl_01[choiceIndex][1] +
       (MAIN_CHOICE_LABEL_HEIGHT + D_800FE33C / 2))
          << 2,
      G_TX_RENDERTILE, 0, 0,
      (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
      (MAIN_CHOICE_LABEL_HEIGHT << 10) /
          (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }
  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }
  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }
  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }
  *displayListHead = gfx;
}

/* The menu without Transmit skips the CODE ENTRY label */
extern s32 D_8022FE14_ovl_01[MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT][2];

void func_802058B8_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;
  s32 labelOffset;

  func_801FB54C_ovl_01(&gfx, 61);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT;
       choiceIndex++) {
    if (g_MenuChoices.mainChoiceWithoutTransmit != choiceIndex) {
      labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
      if (choiceIndex < MAIN_CHOICE_OPTIONS) {
        labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
      }
      gDPLoadTextureBlock_4b(
          gfx++,
          ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
           labelOffset) &
              ~7,
          G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
          G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++, D_8022FE14_ovl_01[choiceIndex][0] << 2,
          D_8022FE14_ovl_01[choiceIndex][1] << 2,
          (D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH) << 2,
          (D_8022FE14_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  choiceIndex = g_MenuChoices.mainChoiceWithoutTransmit;
  if (choiceIndex == MAIN_CHOICE_OPTIONS) {
    labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
  } else {
    labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
  }
  gDPLoadTextureBlock_4b(gfx++,
                         ((u32)D_8012D420[D_8022BE8E_ovl_01] +
                          PALETTED_TEXELS_OFFSET + labelOffset) &
                             ~7,
                         G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH,
                         MAIN_CHOICE_LABEL_HEIGHT, 0, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                         G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(
      gfx++, (D_8022FE14_ovl_01[choiceIndex][0] - D_800FE33C / 2) << 2,
      (D_8022FE14_ovl_01[choiceIndex][1] - D_800FE33C / 2) << 2,
      (D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH +
       D_800FE33C / 2)
          << 2,
      (D_8022FE14_ovl_01[choiceIndex][1] +
       (MAIN_CHOICE_LABEL_HEIGHT + D_800FE33C / 2))
          << 2,
      G_TX_RENDERTILE, 0, 0,
      (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
      (MAIN_CHOICE_LABEL_HEIGHT << 10) /
          (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }
  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }
  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }
  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }
  *displayListHead = gfx;
}

/* The clubhouse's room labels, 192 by 20 each, plain in D_801B5CD0 and lit in
 * D_8012D420 */
#define ROOM_LABEL_WIDTH 192
#define ROOM_LABEL_HEIGHT 20
#define ROOM_LABEL_IMAGE_SIZE (ROOM_LABEL_WIDTH * ROOM_LABEL_HEIGHT / 2)

/* Places for the clubhouse's rows, three more than it shows */
#define CLUBHOUSE_ROW_PLACE_COUNT 8

extern s32 D_8022FE2C_ovl_01[CLUBHOUSE_ROW_PLACE_COUNT][2];

void func_80206180_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 choiceIndex;
  s32 labelOffset;

  func_801FB54C_ovl_01(&gfx, 61);
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (choiceIndex = 0; choiceIndex != CLUBHOUSE_CHOICE_COUNT; choiceIndex++) {
    if (choiceIndex != g_MenuChoices.clubhouseChoice) {
      labelOffset = D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE;
      gDPLoadTextureBlock_4b(gfx++,
                             ((u32)D_801B5CD0[D_8022BE90_ovl_01] +
                              PALETTED_TEXELS_OFFSET + labelOffset) &
                                 ~7,
                             G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT,
                             0, G_TX_NOMIRROR | G_TX_WRAP,
                             G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                             G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
      gDPPipeSync(gfx++);
      gSPScisTextureRectangle(
          gfx++, D_8022FE2C_ovl_01[choiceIndex][0] << 2,
          D_8022FE2C_ovl_01[choiceIndex][1] << 2,
          (D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH) << 2,
          (D_8022FE2C_ovl_01[choiceIndex][1] + ROOM_LABEL_HEIGHT) << 2,
          G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
      gDPPipeSync(gfx++);
    }
  }
  gDPLoadTLUT_pal16(
      gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  choiceIndex = g_MenuChoices.clubhouseChoice;
  gDPLoadTextureBlock_4b(
      gfx++,
      ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
       D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE) &
          ~7,
      G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
      G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
      G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(
      gfx++, (D_8022FE2C_ovl_01[choiceIndex][0] - D_800FE33C / 2) << 2,
      (D_8022FE2C_ovl_01[choiceIndex][1] - D_800FE33C / 2) << 2,
      (D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH + D_800FE33C / 2)
          << 2,
      (D_8022FE2C_ovl_01[choiceIndex][1] + (ROOM_LABEL_HEIGHT + D_800FE33C / 2))
          << 2,
      G_TX_RENDERTILE, 0, 0,
      (ROOM_LABEL_WIDTH << 10) / (D_800FE33C + ROOM_LABEL_WIDTH),
      (ROOM_LABEL_HEIGHT << 10) / (D_800FE33C + ROOM_LABEL_HEIGHT));
  gDPPipeSync(gfx++);
  if (D_800FBDB9 == LABEL_GROWING) {
    D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
  }
  if (D_800FBDB9 == LABEL_SHRINKING) {
    D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
  }
  if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
    D_800FBDB9 = LABEL_SHRINKING;
  }
  if (D_800FE33C < 0) {
    D_800FBDB9 = LABEL_GROWING;
  }
  *displayListHead = gfx;
}

void func_80206A2C_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  func_801FB54C_ovl_01(&gfx, 61);
  *displayListHead = gfx;
}

const char D_802316EC_ovl_01[] = "Peach's Castle";
const char D_802316FC_ovl_01[] = "Luigi's Garden";

INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_80206A68_ovl_01);

void func_80207D08_ovl_01(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;
  s32 caddyIndex;

  func_801FB54C_ovl_01(&gfx, 61);
  BeginShadowedText(&gfx);
  SetTextColor(&gfx, 255, 255, 255);
  DrawShadowedText(&gfx, CADDY_SELECT_TITLE_X, CADDY_SELECT_TITLE_Y,
                   D_8023176C_ovl_01);
  for (caddyIndex = 0; caddyIndex != CADDY_COUNT; caddyIndex++) {
    if (g_MenuChoices.unk1 == caddyIndex) {
      SetTextColor(&gfx, 255, 255, 0);
    } else {
      SetTextColor(&gfx, 255, 255, 255);
    }
    DrawShadowedText(
        &gfx, CADDY_SELECT_NAME_X,
        caddyIndex * CADDY_SELECT_LINE_HEIGHT + CADDY_SELECT_FIRST_Y,
        D_8022C0D0_ovl_01[caddyIndex]);
  }
  gDPPipeSync(gfx++);
  *displayListHead = gfx;
}

/* Defined after the function, as the INCLUDE_ASM before it leaves the assembler
 * in .text */
const char D_8023176C_ovl_01[] = "CADDY SELECT";

void func_80207E18_ovl_01(Gfx** displayListHead) {}

void func_80207E20_ovl_01(void) {}

void func_80207E28_ovl_01(Gfx** displayListHead, void* image, s32 x, s32 y) {
  Gfx* gfx = *displayListHead;
  s32 unused[8];

  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPPipeSync(gfx++);
  gDPLoadTextureTile_4b(gfx++,
                        ((u32)image + PALETTED_TEXELS_OFFSET +
                         D_8022F8C8_ovl_01 * ANIMATED_ICON_IMAGE_SIZE) &
                            ~7,
                        G_IM_FMT_CI, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0,
                        0, ANIMATED_ICON_SIZE - 1, ANIMATED_ICON_SIZE - 1, 0,
                        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                        G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + ANIMATED_ICON_SIZE) << 2,
                          (y + ANIMATED_ICON_SIZE) << 2, G_TX_RENDERTILE, 0, 0,
                          TEXEL_STEP_ONE, TEXEL_STEP_ONE);
  gDPPipeSync(gfx++);
  D_8022F8CD_ovl_01++;
  if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
    D_8022F8CD_ovl_01 = 0;
    D_8022F8C8_ovl_01++;
    if (D_8022F8C8_ovl_01 >= ANIMATED_ICON_FRAME_COUNT) {
      D_8022F8C8_ovl_01 = 0;
    }
  }
  *displayListHead = gfx;
}

/*
 * Doesn't match: the two growths swap registers ($t1 and $t8), and the
 * right and bottom edges add their constants last.
 */
#ifdef NON_MATCHING
void func_802081E8_ovl_01(Gfx** displayListHead, s32 left, s32 top,
                          u8 starCount, u8 enlargesChosenHole) {
  Gfx* gfx = *displayListHead;
  EncodedAsset asset;
  void* image = D_801B93E0[D_8022F8CC_ovl_01];
  u8 starIndex = 0;
  s32 growWidth = 0;
  s32 growHeight;

  gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
                         G_IM_FMT_CI, STAR_WIDTH, STAR_HEIGHT, 0,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  if (starCount > MAX_STAR_COUNT) {
    starCount = MAX_STAR_COUNT;
  }
  if (starCount == 0) {
    starCount = 1;
  }
  for (; starIndex < starCount; starIndex++) {
    growWidth = 0;
    if (enlargesChosenHole == TRUE) {
      growHeight = 0;
      if (starIndex == g_MenuChoices.holeIndex) {
        growWidth = CHOSEN_STAR_GROWTH;
        growHeight = CHOSEN_STAR_GROWTH;
      }
    } else {
      growHeight = 0;
    }
    gSPScisTextureRectangle(
        gfx++, (left + starIndex * STAR_WIDTH - growWidth / 2) << 2,
        (top - growHeight / 2) << 2,
        (left + starIndex * STAR_WIDTH + growWidth / 2 + STAR_WIDTH) << 2,
        (top + growHeight / 2 + STAR_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
        (STAR_WIDTH << 10) / (growWidth + STAR_WIDTH),
        (STAR_HEIGHT << 10) / (growHeight + STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }
  D_8022F8CD_ovl_01++;
  if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
    D_8022F8CD_ovl_01 = 0;
    D_8022F8C8_ovl_01++;
    if (D_8022F8C8_ovl_01 >= STAR_FRAME_COUNT) {
      D_8022F8C8_ovl_01 = 0;
    }
    D_8022F8CC_ovl_01 = D_8022F8CC_ovl_01 == 0;
    OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C8_ovl_01], &asset);
    ReadEncodedAsset(D_801B93E0[D_8022F8CC_ovl_01], &asset);
  }
  *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/menu_drawing", func_802081E8_ovl_01);
#endif

/*
 * func_8020879C draws the course's Ring Shot holes as stars on a hexagon
 * around a centre, gold where the hole's star was won and silver where not,
 * with the chosen hole larger. Nothing calls it.
 */
#define RING_SHOT_STAR_RADIUS 56
#define RING_SHOT_STAR_WIDTH 32
#define RING_SHOT_STAR_HEIGHT 24
#define RING_SHOT_STAR_GROWTH 16
#define RING_SHOT_STAR_FRAME_DURATION 2
#define GOLD_STAR_PALETTE 0
#define SILVER_STAR_PALETTE 1
/* The silver star's texels follow the gold star's in texture memory */
#define SILVER_STAR_TMEM 128
#define SILVER_STAR_TILE 1

extern s32 D_8022F884_ovl_01[STAR_FRAME_COUNT]; /* The silver star's frames */
extern void* D_800FF180[2];

void func_8020879C_ovl_01(Gfx** displayListHead, s32 centerX, s32 centerY) {
  EncodedAsset asset;
  SaveData* save = GetSaveData();
  Gfx* gfx = *displayListHead;
  f32 starPositions[RING_SHOT_HOLE_COUNT][2] = {
      {0.0f, -1.0f}, {0.89f, -0.45f}, {0.89f, 0.45f},
      {0.0f, 1.0f},  {-0.89f, 0.45f}, {-0.89f, -0.45f},
  };
  void* image;
  s32 tile;
  u8 holeIndex;
  s32 growWidth;
  s32 growHeight;
  s32 left;
  s32 top;

  image = D_8010526C[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(gfx++, GOLD_STAR_PALETTE,
                    ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
                         G_IM_FMT_CI, RING_SHOT_STAR_WIDTH,
                         RING_SHOT_STAR_HEIGHT, GOLD_STAR_PALETTE,
                         G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP,
                         G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  image = D_800FF180[D_8022F8C4_ovl_01];
  gDPLoadTLUT_pal16(gfx++, SILVER_STAR_PALETTE,
                    ((u32)image + IMAGE_HEADER_SIZE) & ~7);
  gDPLoadMultiBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7,
                       SILVER_STAR_TMEM, SILVER_STAR_TILE, G_IM_FMT_CI,
                       RING_SHOT_STAR_WIDTH, RING_SHOT_STAR_HEIGHT,
                       SILVER_STAR_PALETTE, G_TX_NOMIRROR | G_TX_WRAP,
                       G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                       G_TX_NOLOD, G_TX_NOLOD);
  gDPPipeSync(gfx++);
  for (holeIndex = 0; holeIndex < RING_SHOT_HOLE_COUNT; holeIndex++) {
    growWidth = 0;
    if (holeIndex == g_MenuChoices.holeIndex) {
      growWidth = RING_SHOT_STAR_GROWTH;
      growHeight = RING_SHOT_STAR_GROWTH;
    } else {
      growHeight = 0;
    }
    left = starPositions[holeIndex][0] * RING_SHOT_STAR_RADIUS;
    top = starPositions[holeIndex][1] * RING_SHOT_STAR_RADIUS;
    left += centerX - RING_SHOT_STAR_WIDTH / 2;
    top += centerY - RING_SHOT_STAR_HEIGHT / 2;
    /* A hole without its star draws from the silver star's tile */
    tile = save->hasRingShotStar[g_MenuChoices.courseIndex][holeIndex] == FALSE;
    gSPScisTextureRectangle(
        gfx++, (left - growWidth / 2) << 2, (top - growHeight / 2) << 2,
        (left + RING_SHOT_STAR_WIDTH + growWidth / 2) << 2,
        (top + RING_SHOT_STAR_HEIGHT + growHeight / 2) << 2, tile, 0, 0,
        (RING_SHOT_STAR_WIDTH << 10) / (growWidth + RING_SHOT_STAR_WIDTH),
        (RING_SHOT_STAR_HEIGHT << 10) / (growHeight + RING_SHOT_STAR_HEIGHT));
    gDPPipeSync(gfx++);
  }
  D_8022F8C5_ovl_01++;
  if (D_8022F8C5_ovl_01 == RING_SHOT_STAR_FRAME_DURATION) {
    D_8022F8C5_ovl_01 = 0;
    D_8022F8C0_ovl_01++;
    if (D_8022F8C0_ovl_01 >= STAR_FRAME_COUNT) {
      D_8022F8C0_ovl_01 = 0;
    }
    D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
    OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
    OpenEncodedAsset(D_8022F884_ovl_01[D_8022F8C0_ovl_01], &asset);
    ReadEncodedAsset(D_800FF180[D_8022F8C4_ovl_01], &asset);
  }
  *displayListHead = gfx;
}

void func_80208F8C_ovl_01(Gfx** displayListHead, s32 left, s32 top) {
  Gfx* gfx = *displayListHead;
  s16 stripIndex;
  s32 rowCount;
  s32 unused[8];

  gDPPipeSync(gfx++);
  gDPPipeSync(gfx++);
  gDPSetCycleType(gfx++, G_CYC_1CYCLE);
  gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
  gDPPipeSync(gfx++);
  gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
  gDPPipeSync(gfx++);
  gDPSetTextureFilter(gfx++, G_TF_BILERP);
  gDPLoadTLUT_pal256(
      gfx++, ((u32)D_800FBE30[D_8022F8CE_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
  for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
    rowCount = TOAD_LAST_STRIP_HEIGHT;
    if (stripIndex == 0) {
      rowCount = TOAD_STRIP_HEIGHT;
    }
    gDPLoadTextureBlock(
        gfx++,
        ((u32)D_800FBE30[D_8022F8CE_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
         stripIndex * TOAD_WIDTH * TOAD_STRIP_HEIGHT) &
            ~7,
        G_IM_FMT_CI, G_IM_SIZ_8b, TOAD_WIDTH, rowCount, 0,
        G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
        G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(
        gfx++, left << 2, (top + stripIndex * TOAD_STRIP_HEIGHT) << 2,
        (left + TOAD_WIDTH) << 2,
        (top + stripIndex * TOAD_STRIP_HEIGHT + rowCount) << 2, G_TX_RENDERTILE,
        0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
  }
  *displayListHead = gfx;
}

void DrawBirdieBadgeLabel(Gfx** displayListHead) {
  Gfx* gfx = *displayListHead;

  DrawShadowedText(&gfx, 64, 32, g_BirdieBadgeLabelText);
  *displayListHead = gfx;
}

s32 GetOpenCourseCount(void) {
  s32 coursePoints = GetSaveData()->coursePoints;

  if (IsGameFlagSet(GAME_FLAG_OPEN_SIX_COURSES)) {
    return 6;
  }
  if (IsGameFlagSet(GAME_FLAG_OPEN_FIVE_COURSES)) {
    return 5;
  }
  if (IsGameFlagSet(GAME_FLAG_OPEN_FOUR_COURSES)) {
    return 4;
  }
  if (IsGameFlagSet(GAME_FLAG_OPEN_THREE_COURSES)) {
    return 3;
  }
  if (IsGameFlagSet(GAME_FLAG_OPEN_TWO_COURSES)) {
    return 2;
  }
  return CountOpenCourses(coursePoints);
}
