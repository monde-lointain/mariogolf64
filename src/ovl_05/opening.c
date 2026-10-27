#include "common.h"

#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "camera_editor.h"
#include "character_slot.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "hole_display.h"
#include "libc.h"
#include "math_util.h"
#include "sound.h"
#include "variables.h"

/* The view that the opening draws its characters through */
#define OPENING_VIEW_INDEX 2

/* The size of the heap that the opening's billboards are allocated from */
#define OPENING_BILLBOARD_HEAP_SIZE 0x7038

#define ASSET_SHADOW_TEXTURE 0x4C3

/* The face-printed golf balls that the opening shows, two at a time */
#define OPENING_BALL_IMAGE_COUNT 5
#define OPENING_BALL_SLOT_COUNT 2
/* A 64x64 CI8 image with its header and palette */
#define OPENING_BALL_IMAGE_SIZE 0x1208

/* The character name logos that the opening shows, two at a time */
#define OPENING_NAME_LOGO_COUNT 7
#define OPENING_NAME_SLOT_COUNT 2
/* A 128x64 CI8 image with its header and palette */
#define OPENING_NAME_LOGO_SIZE 0x2208

typedef struct OpeningNameLogo {
  s32 assetId;
  s32 unk04;
  s32 unk08;
  s32 unk0C;
  s32 unk10;
  s32 unk14;
  s32 unk18;
} OpeningNameLogo; /* size = 0x1C */

extern s32 D_8020AC4C_ovl_05;
extern s32 D_8020AC50_ovl_05;
extern f32 D_8020AC5C_ovl_05;
extern s32 D_8020AC60_ovl_05;
extern s32 D_8020AC64_ovl_05;
extern u8 D_80212830_ovl_05[];
extern u8 D_80213A40_ovl_05[];
extern OpeningNameLogo f_OpeningNameLogos[OPENING_NAME_LOGO_COUNT];
extern u8 D_8020E400_ovl_05[OPENING_NAME_SLOT_COUNT][OPENING_NAME_LOGO_SIZE];
extern s32 D_80212810_ovl_05[OPENING_NAME_SLOT_COUNT];
extern s32 D_80212818_ovl_05[OPENING_NAME_SLOT_COUNT];
extern s32 D_80212820_ovl_05[OPENING_NAME_SLOT_COUNT];
extern s32 D_8020AD70_ovl_05[OPENING_BALL_SLOT_COUNT];
extern f32 D_8020AD80_ovl_05[OPENING_BALL_SLOT_COUNT];
extern s32 f_OpeningBallAssets[OPENING_BALL_IMAGE_COUNT];
extern u8 D_80214C50_ovl_05[OPENING_BALL_SLOT_COUNT][OPENING_BALL_IMAGE_SIZE];
/* The buffers of character slots 1 and 2 */
extern u8* f_OpeningCharacterBuffers[2];
extern void* f_OpeningBillboardHeap;
extern s32 D_8022BFA8_ovl_05;
extern u8* g_ShadowTexture;
extern s32 f_CometScreenPoint[2];
/* A 32x32 CI8 image with its header and palette */
extern u8 f_CometHeadImage[];
/* A 48x128 I4 image with its header */
extern u8 f_CometTrailImage[];
extern Mtx f_CometTrailMatrices[40];
extern f32 f_CometTrailStartX;
extern f32 f_CometTrailStartY;

extern s32 f_OpeningFrameParity;
extern s32 f_IsLookAtMarkerShown;
extern f32 g_LookAtMarkerScale;
extern Vtx f_LookAtMarkerVertices[6][4];
extern Mtx f_LookAtMarkerTranslation;
extern Mtx f_LookAtMarkerScaling;

extern s32 f_IsCometFlying;
extern s32 f_CometAngle;
extern s32 D_8020AD68_ovl_05;
extern f32 D_8020B260_ovl_05;
extern Vec3 D_8021706C_ovl_05;
extern CoursePoint D_8022BF08_ovl_05;

void AssignCharacterBuffers(void);
void func_801F5454_ovl_05(CameraMatrices* matrices);
void func_801FA8D8_ovl_05(Gfx** displayListHead);
void func_801FBB94_ovl_05(Gfx** displayListHead, CameraMatrices* matrices);
void func_801FE2A0_ovl_05(Gfx** displayListHead);
void func_801FE8F8_ovl_05(Gfx** displayListHead);
void func_801FF70C_ovl_05(Gfx** displayListHead);
void func_801FFC40_ovl_05(Gfx** displayListHead);
void func_80200CBC_ovl_05(Gfx** displayListHead, s32 arg1);
void func_80202D34_ovl_05(Gfx** displayListHead);
void func_80200B18_ovl_05(void);

void func_801F4A40_ovl_05(void) {
  D_8020AC4C_ovl_05++;
  if (D_8020AC4C_ovl_05 == 11) {
    D_8020AC4C_ovl_05 = 0;
  }
}

void func_801F4A6C_ovl_05(void) {
  if (D_8020AC60_ovl_05 != 0) {
    PlaySoundWithEffects(0x51, 12, 48 - (s32)(D_8020AC5C_ovl_05 * 0.2f), 0,
                         127 - (D_8020AC50_ovl_05 - 160) / 2);
  }
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F4AEC_ovl_05);

void LoadOpening(void) {
  EncodedAsset asset;
  s32 bufferIndex;

  osSyncPrintf("ConstOpening\n");
  f_OpeningBillboardHeap = AllocMemory(OPENING_BILLBOARD_HEAP_SIZE);
  InitBillboardManager(f_OpeningBillboardHeap, OPENING_BILLBOARD_HEAP_SIZE);
  D_8022BFA8_ovl_05 = 31;
  for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(f_OpeningCharacterBuffers);
       bufferIndex++) {
    f_OpeningCharacterBuffers[bufferIndex] =
        AllocMemory(GetCharacterBufferSize());
    SetCharacterBuffer(bufferIndex + 1, f_OpeningCharacterBuffers[bufferIndex]);
  }
  SetCharacterBuffer(3, (u8*)g_DepthBuffer);
  g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
  ReadEncodedAsset(g_ShadowTexture, &asset);
  D_8020AC64_ovl_05 = 0;
  LoadEffectImages();
  func_80200B18_ovl_05();
}

void FreeOpening(void) {
  s32 bufferIndex;

  FreeMemory(&g_ShadowTexture);
  for (bufferIndex = 0; bufferIndex < ARRAY_COUNT(f_OpeningCharacterBuffers);
       bufferIndex++) {
    FreeMemory(&f_OpeningCharacterBuffers[bufferIndex]);
  }
  AssignCharacterBuffers();
  FreeMemory(&f_OpeningBillboardHeap);
  SetFillColor(255, 255, 255);
  FreeEffectImages();
}

/* The caller passes the frame parity too, which goes unused */
void DrawOpeningCharacters(CameraMatrices* matrices, Gfx** displayListHead,
                           s32 frameParity) {
  s32 slotIndex = 0;

  g_DisplayListHead = *displayListHead;
  do {
    DrawCharacterAndShadow(matrices, OPENING_VIEW_INDEX, slotIndex);
    slotIndex++;
  } while (slotIndex < CHARACTER_SLOT_COUNT);
  *displayListHead = g_DisplayListHead;
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F51F0_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F5454_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F58EC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801F6788_ovl_05);

/* Draws the camera editor's look-at marker, the characters and the opening's
 * overlays */
void DrawOpening(CameraMatrices* matrices, Gfx** displayListHead) {
  /* Unused; they give the stack frame the original's size */
  Mat4 unusedMatrix;
  Vec3 unusedPoints[4];
  Gfx* gfx = *displayListHead;

  f_OpeningFrameParity ^= 1;
  if (f_IsLookAtMarkerShown) {
    guTranslate(&f_LookAtMarkerTranslation, g_Cameras[0].view.lookAt[0],
                g_Cameras[0].view.lookAt[1], g_Cameras[0].view.lookAt[2]);
    guScale(&f_LookAtMarkerScaling, g_LookAtMarkerScale, g_LookAtMarkerScale,
            g_LookAtMarkerScale);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_OPA_SURF, G_RM_ZB_OPA_SURF2);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LookAtMarkerTranslation),
              G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&f_LookAtMarkerScaling),
              G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_LookAtMarkerVertices[0], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[1], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[2], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[3], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[4], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPVertex(gfx++, f_LookAtMarkerVertices[5], 4, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSPPopMatrix(gfx++, G_MTX_MODELVIEW);
    gDPPipeSync(gfx++);
  }
  DrawOpeningCharacters(matrices, &gfx, f_OpeningFrameParity);
  g_DisplayListHead = gfx;
  func_801F5454_ovl_05(matrices);
  gfx = g_DisplayListHead;
  DrawLensFlare(&gfx);
  func_801FE2A0_ovl_05(&gfx);
  func_801FF70C_ovl_05(&gfx);
  func_801FE8F8_ovl_05(&gfx);
  func_801FBB94_ovl_05(&gfx, matrices);
  func_801FFC40_ovl_05(&gfx);
  func_80202D34_ovl_05(&gfx);
  func_801FA8D8_ovl_05(&gfx);
  func_80200CBC_ovl_05(&gfx, 0);
  *displayListHead = gfx;
}

/* Loads name logo logoIndex into slot and resets the slot */
void func_801FA83C_ovl_05(s32 logoIndex, s32 slot) {
  EncodedAsset asset;

  OpenEncodedAsset(f_OpeningNameLogos[logoIndex].assetId, &asset);
  ReadEncodedAsset(D_8020E400_ovl_05[slot], &asset);
  D_80212810_ovl_05[slot] = logoIndex;
  D_80212818_ovl_05[slot] = 0;
  D_80212820_ovl_05[slot] = 0;
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FA8D8_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FB864_ovl_05);

/* Loads ball image ballIndex into slot and resets the slot */
void func_801FBB10_ovl_05(s32 ballIndex, s32 slot) {
  EncodedAsset asset;

  OpenEncodedAsset(f_OpeningBallAssets[ballIndex], &asset);
  ReadEncodedAsset(D_80214C50_ovl_05[slot], &asset);
  D_8020AD70_ovl_05[slot] = 0;
  D_8020AD80_ovl_05[slot] = 1.0f;
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FBB94_ovl_05);

void func_801FE270_ovl_05(void) {
  EncodedAsset asset;

  OpenEncodedAsset(0x908, &asset);
  ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FE2A0_ovl_05);

/* Loads the comet's images and gathers its trail at the comet */
void LoadComet(void) {
  EncodedAsset asset;
  s32 pass;
  s32 i;

  OpenEncodedAsset(0x64B, &asset);
  ReadEncodedAsset(f_CometHeadImage, &asset);
  OpenEncodedAsset(0x6AF, &asset);
  ReadEncodedAsset(f_CometTrailImage, &asset);
  for (pass = 0; pass < 2; pass++) {
    for (i = 0; i < 40; i++) {
      guTranslate(&f_CometTrailMatrices[i], f_CometScreenPoint[0] - 160,
                  120 - f_CometScreenPoint[1], -300.0f);
    }
  }
  f_CometTrailStartX = f_CometScreenPoint[0] - 320;
  f_CometTrailStartY = 120 - f_CometScreenPoint[1];
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FE8F8_ovl_05);

/* Flies the comet across the screen in loops until it has turned through 540
 * degrees */
void MoveComet(void) {
  f32 angle;

  if (f_IsCometFlying) {
    angle = DEGREES_TO_RADIANS(f_CometAngle);
    f_CometAngle += 12;
    if (f_CometAngle >= 541) {
      f_IsCometFlying = FALSE;
    }
    f_CometScreenPoint[0] += 8 + (s32)(cos(angle) * 20.0);
    f_CometScreenPoint[1] -= 2 + (s32)(sin(angle) * 16.0);
  }
}

void func_801FF6DC_ovl_05(void) {
  EncodedAsset asset;

  OpenEncodedAsset(0x93D, &asset);
  ReadEncodedAsset(D_80212830_ovl_05, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FF70C_ovl_05);

void func_801FFC10_ovl_05(void) {
  EncodedAsset asset;

  OpenEncodedAsset(0x939, &asset);
  ReadEncodedAsset(D_80213A40_ovl_05, &asset);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_801FFC40_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_802008D8_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80200B18_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80200CBC_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening", func_80202D34_ovl_05);
