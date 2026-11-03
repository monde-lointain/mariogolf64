/*
 * The slots that hold the loaded characters: their alpha and highlights, their
 * joint matrices, freeing them once no display list uses them, and the
 * positions of points on their models.
 */
#include "common.h"

#include "character_slot.h"

#include "camera.h"
#include "club.h"
#include "game.h"
#include "main.h"
#include "math_util.h"
#include "save.h"

/* How many frames a drawn slot stays busy */
#define BUSY_FRAME_COUNT 2

/* Like G_RM_AA_ZB_XLU_SURF, but also updating the depth buffer */
#define CHARACTER_RENDER_MODE_TRANSLUCENT \
  (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | \
      FORCE_BL | G_RM_PASS)
#define CHARACTER_RENDER_MODE_TRANSLUCENT2 \
  (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | \
      FORCE_BL | GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

/* The sides, in texels, of the hilite textures */
#define HILITE_TEXTURE_SIZE 32
#define HILITE_SECOND_TILE 1

#define ModelChosenDisplayList(model) \
  ((Gfx*)(model)->segmentAddresses[CHARACTER_MODEL_CHOSEN_DISPLAY_LIST])
#define ModelDisplayListChoices(model) \
  ((u32*)(model)->segmentAddresses[CHARACTER_MODEL_DISPLAY_LIST_CHOICES])

/* osMapTLB's physical address for a page left unmapped */
#define NO_PHYSICAL_PAGE (-1)
/* osMapTLB's address space id for a global mapping */
#define NO_TLB_ASID (-1)

#define MODEL_TO_WORLD_SCALE 0.01f
#define SLOT_POSITION_SCALE (1.0f / 1024)

CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];
s32 g_CurrentAnimationEndFrame;
f32 g_CurrentAnimationFrame;
Mtx* g_CurrentJointMatrices;

/* The slot's first rotation is taken away from this roll */
static LookAt f_HiliteLookAts[DRAW_BUFFER_COUNT];
static Hilite f_Hilites[DRAW_BUFFER_COUNT];

/* Sets the characters' alpha, drawing them translucent below 0xFF, and their
 * combiner for the weather */
void SetCharacterAlpha(s32 alpha) {
  Gfx* gfx;
  s32 i;

  gfx = &g_CharacterEnvColorDisplayLists[0][0];
  for (i = 0; i != ENV_COLOR_COUNT; i++) {
    gDPSetEnvColor(gfx++, g_CharacterEnvColors[i][0],
        g_CharacterEnvColors[i][1], g_CharacterEnvColors[i][2], alpha);
    gSPEndDisplayList(gfx++);
  }

  gfx = g_CharacterRenderModeDisplayList;
  if ((alpha & 0xFF) == 0xFF) {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
  } else {
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, CHARACTER_RENDER_MODE_TRANSLUCENT,
        CHARACTER_RENDER_MODE_TRANSLUCENT2);
  }

  gfx = g_CharacterCombineDisplayList;
  if (g_HoleSetup.conditions.isRaining) {
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, SHADE, 0, ENVIRONMENT, 0, SHADE, 0,
        PRIMITIVE, 0, SHADE, 0, ENVIRONMENT, 0, SHADE, 0);
  } else {
    gDPSetCombineLERP(gfx++, PRIMITIVE, 0, SHADE, 0, ENVIRONMENT, 0, SHADE, 0,
        ENVIRONMENT, COMBINED, TEXEL0, COMBINED, 0, 0, 0, COMBINED);
  }
}

/*
 * Points the characters' highlights at the scene light, as seen from the main
 * camera. The matrices are unused.
 */
void SetCharacterHilite(CameraMatrices* matrices UNUSED) {
  f32 lookAtMatrix[4][4];
  f32 lightDirection[3];
  Radians lightPitch;
  Radians lightYaw;
  Gfx* gfx;

  lightPitch = g_HoleSetup.conditions.lightPitch * BINARY_ANGLE_TO_RADIANS;
  lightYaw = g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS;
  lightDirection[0] =
      -sinf(lightPitch) * cosf(lightYaw) * LIGHT_DIRECTION_SCALE;
  lightDirection[1] = -cosf(lightPitch) * LIGHT_DIRECTION_SCALE;
  lightDirection[2] =
      -sinf(lightPitch) * sinf(lightYaw) * LIGHT_DIRECTION_SCALE;
  guLookAtHiliteF(lookAtMatrix, &f_HiliteLookAts[g_DrawBufferIndex],
      &f_Hilites[g_DrawBufferIndex], g_Cameras[0].view.eye[0],
      g_Cameras[0].view.eye[1], g_Cameras[0].view.eye[2],
      g_Cameras[0].view.lookAt[0], g_Cameras[0].view.lookAt[1],
      g_Cameras[0].view.lookAt[2], 0.0f, -1.0f, 0.0f, (s8)lightDirection[0],
      (s8)lightDirection[1], (s8)lightDirection[2], (s8)lightDirection[0],
      (s8)lightDirection[1], (s8)lightDirection[2], HILITE_TEXTURE_SIZE,
      HILITE_TEXTURE_SIZE);
  gfx = g_CharacterHiliteDisplayList;
  gDPSetHilite1Tile(gfx++, G_TX_RENDERTILE, &f_Hilites[g_DrawBufferIndex],
      HILITE_TEXTURE_SIZE, HILITE_TEXTURE_SIZE);
  gDPSetHilite2Tile(gfx++, HILITE_SECOND_TILE, &f_Hilites[g_DrawBufferIndex],
      HILITE_TEXTURE_SIZE, HILITE_TEXTURE_SIZE);
  gSPEndDisplayList(gfx++);
}

/*
 * Maps pageCount pages from virtualAddress to physicalAddress, starting at TLB
 * entry firstTlbIndex. The pages are SMALLEST_PAGE_SIZE << pageMode bytes; odd
 * page modes map each page as two TLB pages of half that size. Returns the next
 * free TLB entry, or -1 if the addresses aren't page-aligned or the mode is
 * invalid.
 */
s32 SetMultiTlb(s32 firstTlbIndex, s32 pageMode, u32 pageCount,
    u32 virtualAddress, u32 physicalAddress) {
  s32 tlbIndex;
  u32 pageSize;
  OSPageMask pageMask;

  tlbIndex = firstTlbIndex;
  pageSize = SMALLEST_PAGE_SIZE << pageMode;
  if ((virtualAddress | physicalAddress) & (pageSize - 1)) {
    osSyncPrintf("kSetMultiTLB : Invalid Virtual or Physical Address\n");
    return -1;
  }

  switch (pageMode) {
    case 1:
      pageSize >>= 1;
      pageCount *= 2;

      /* fallthrough */
    case 0:
      pageMask = OS_PM_4K;
      break;
    case 3:
      pageSize >>= 1;
      pageCount *= 2;

      /* fallthrough */
    case 2:
      pageMask = OS_PM_16K;
      break;
    case 5:
      pageSize >>= 1;
      pageCount *= 2;

      /* fallthrough */
    case 4:
      pageMask = OS_PM_64K;
      break;
    case 7:
      pageSize >>= 1;
      pageCount *= 2;

      /* fallthrough */
    case 6:
      pageMask = OS_PM_256K;
      break;
    default:
      osSyncPrintf("kSetMultiTLB : Invalid Page Mode\n");
      return -1;
  }

  /* A TLB entry maps an even and odd page pair, so a lone odd page goes first
   */
  if (virtualAddress & pageSize) {
    osUnmapTLB(tlbIndex);
    osMapTLB(tlbIndex++, pageMask, (void*)virtualAddress, NO_PHYSICAL_PAGE,
        physicalAddress, NO_TLB_ASID);
    virtualAddress += pageSize;
    physicalAddress += pageSize;
    pageCount--;
  }
  while (pageCount >= 2) {
    osUnmapTLB(tlbIndex);
    osMapTLB(tlbIndex++, pageMask, (void*)virtualAddress, physicalAddress,
        physicalAddress + pageSize, NO_TLB_ASID);
    virtualAddress += pageSize * 2;
    pageCount -= 2;
    physicalAddress += pageSize * 2;
  }

  if (pageCount != 0) {
    osUnmapTLB(tlbIndex);
    osMapTLB(tlbIndex++, pageMask, (void*)virtualAddress, physicalAddress,
        NO_PHYSICAL_PAGE, NO_TLB_ASID);
  }

  return tlbIndex;
}

const CharacterModel
    g_CharacterModels[CHARACTER_MODEL_KIND_COUNT][CHARACTER_MODELS_PER_KIND] = {
        {
            {
                0x01652EB0,
                0x0165BD50,
                {0x02010F60, 0x02000540, 0x02005DD8, 0x02005488, 0x020111CC,
                    0x020112D4, 0x0201129C, 0x02011280, 0x020112A4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01652EB0,
                0x0165BD50,
                0x02011750,
                0x02012750,
            },
            {
                0x015EE790,
                0x015F6EC0,
                {0x02010C80, 0x02000540, 0x02004B10, 0x020041C0, 0x02010EF0,
                    0x02010FE8, 0x02010FB8, 0x02010FA4, 0x02010FC0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015EE790,
                0x015F6EC0,
                0x02011460,
                0x02012460,
            },
            {
                0x01606090,
                0x0160ED90,
                {0x02012BB0, 0x02000540, 0x02006FA8, 0x02006658, 0x02012E20,
                    0x02012F54, 0x02012F08, 0x02012ED4, 0x02012F14, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01606090,
                0x0160ED90,
                0x020133D0,
                0x020143D0,
            },
            {
                0x016A1220,
                0x016A73A0,
                {0x0200FF90, 0x02000540, 0x02005818, 0x02004F10, 0x020101C4,
                    0x020102D4, 0x02010294, 0x02010278, 0x0201029C, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                70,
                0,
                0x016A1220,
                0x016A73A0,
                0x02010750,
                0x02011750,
            },
            {
                0x01545D80,
                0x0154C190,
                {0x0200EFD0, 0x02000540, 0x02005660, 0x02004D10, 0x0200F1F8,
                    0x0200F318, 0x0200F2D4, 0x0200F2AC, 0x0200F2E0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01545D80,
                0x0154C190,
                0x0200F6F8,
                0x020106F8,
            },
            {
                0x015D5A90,
                0x015DE6D0,
                {0x02011260, 0x02000540, 0x02005278, 0x02004920, 0x02011500,
                    0x02011628, 0x020115D8, 0x020115B4, 0x020115E0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015D5A90,
                0x015DE6D0,
                0x02011B08,
                0x02012B08,
            },
            {
                0x0167A3C0,
                0x01681F70,
                {0x02011BE0, 0x02000540, 0x02006720, 0x02005DD0, 0x02011E50,
                    0x02011F58, 0x02011F18, 0x02011F04, 0x02011F20, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0167A3C0,
                0x01681F70,
                0x020123D0,
                0x020133D0,
            },
            {
                0x015544C0,
                0x0155D0C0,
                {0x02011530, 0x02000540, 0x02005660, 0x02004D10, 0x020117B0,
                    0x020118C8, 0x02011888, 0x02011864, 0x02011890, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015544C0,
                0x0155D0C0,
                0x02011D78,
                0x02012D78,
            },
            {
                0x01644C90,
                0x0164C520,
                {0x0200F640, 0x02000540, 0x02005A58, 0x02005108, 0x0200F868,
                    0x0200F978, 0x0200F940, 0x0200F91C, 0x0200F948, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01644C90,
                0x0164C520,
                0x0200FD58,
                0x02010D58,
            },
            {
                0x01533380,
                0x015384B0,
                {0x0200C598, 0x02000540, 0x02004E58, 0x02004508, 0x0200C7BC,
                    0x0200C8CC, 0x0200C894, 0x0200C870, 0x0200C89C, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01533380,
                0x015384B0,
                0x0200CCA8,
                0x0200DCA8,
            },
            {
                0x016C88B0,
                0x016CE3C0,
                {0x0200E358, 0x02000540, 0x020040F8, 0x020037C8, 0x0200E5AC,
                    0x0200E6C4, 0x0200E68C, 0x0200E660, 0x0200E694, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016C88B0,
                0x016CE3C0,
                0x0200EB08,
                0x0200FB08,
            },
            {
                0x0161B430,
                0x01622860,
                {0x020119E8, 0x02000540, 0x020074D0, 0x02006B68, 0x02011C48,
                    0x02011D78, 0x02011D1C, 0x02011CFC, 0x02011D28, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                120,
                0,
                0x0161B430,
                0x01622860,
                0x020121C0,
                0x020131C0,
            },
            {
                0x016B72B0,
                0x016BDB40,
                {0x02010148, 0x02000540, 0x02005ED8, 0x02005570, 0x0201031C,
                    0x0201044C, 0x020103EC, 0x020103D0, 0x020103F4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                150,
                0,
                0x016B72B0,
                0x016BDB40,
                0x02010790,
                0x02011790,
            },
            {
                0x01715F00,
                0x0171A110,
                {0x02007900, 0x02000018, 0x02000898, 0x02000078, 0x02007B3C,
                    0x02007C00, 0x02007BF4, 0x02007BF0, 0x02007BF8, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01715F00,
                0x0171A110,
                0x02007FE0,
                0x020087E0,
            },
            {
                0x015A4050,
                0x015AB320,
                {0x0200FB28, 0x02000540, 0x02005668, 0x02004D10, 0x0200FD8C,
                    0x0200FEB0, 0x0200FE6C, 0x0200FE40, 0x0200FE78, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015A4050,
                0x015AB320,
                0x020102F8,
                0x020112F8,
            },
            {
                0x015C67B0,
                0x015CD7B0,
                {0x0200EFF8, 0x02000540, 0x02004AD8, 0x02004188, 0x0200F200,
                    0x0200F308, 0x0200F2D0, 0x0200F2B4, 0x0200F2D8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015C67B0,
                0x015CD7B0,
                0x0200F718,
                0x02010718,
            },
            {
                0x0156A510,
                0x01575410,
                {0x020131E8, 0x02000540, 0x02004050, 0x02003700, 0x02013454,
                    0x02013540, 0x0201350C, 0x02013508, 0x02013518, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0156A510,
                0x01575410,
                0x020139B8,
                0x020141B8,
            },
            {
                0x01630880,
                0x01638FA0,
                {0x02012A48, 0x02000540, 0x02006A20, 0x020060D0, 0x02012CB4,
                    0x02012DA8, 0x02012D6C, 0x02012D68, 0x02012D70, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01630880,
                0x01638FA0,
                0x02013220,
                0x02013A20,
            },
            {
                0x016651E0,
                0x0166EDE0,
                {0x02010F98, 0x02000018, 0x02005F4C, 0x02005EF8, 0x02011168,
                    0x02011204, 0x020111C0, 0x020111A4, 0x020111CC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x016651E0,
                0x0166EDE0,
                0x02011618,
                0x02012618,
            },
            {
                0x015F7E90,
                0x01605EE0,
                {0x02019188, 0x02000018, 0x0200A60C, 0x0200A5B8, 0x02019314,
                    0x020193F8, 0x0201937C, 0x02019350, 0x02019398, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015F7E90,
                0x01605EE0,
                0x02019770,
                0x0201A770,
            },
            {
                0x01610E80,
                0x0161AC20,
                {0x0201A6D0, 0x02000018, 0x0200CFF8, 0x0200CFA4, 0x0201A8C4,
                    0x0201A9C4, 0x0201A930, 0x0201A900, 0x0201A94C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01610E80,
                0x0161AC20,
                0x0201AE40,
                0x0201BE40,
            },
            {
                0x016A8840,
                0x016B0170,
                {0x02013ED0, 0x02000018, 0x02009D64, 0x02009D10, 0x0201409C,
                    0x020141B4, 0x02014134, 0x02014100, 0x0201414C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                70,
                0,
                0x016A8840,
                0x016B0170,
                0x02014590,
                0x02015590,
            },
            {
                0x0154C850,
                0x01552B20,
                {0x02010020, 0x02000018, 0x02006F84, 0x02006F30, 0x020101C4,
                    0x02010284, 0x02010224, 0x02010200, 0x0201023C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0154C850,
                0x01552B20,
                0x02010630,
                0x02011630,
            },
            {
                0x015E2E00,
                0x015EC1B0,
                {0x020160C8, 0x02000018, 0x0200AB6C, 0x0200AB18, 0x0201626C,
                    0x02016380, 0x020162D4, 0x020162A8, 0x020162F8, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015E2E00,
                0x015EC1B0,
                0x02016728,
                0x02017728,
            },
            {
                0x01697C80,
                0x016A0220,
                {0x02011898, 0x02000018, 0x020067D8, 0x02006784, 0x02011A80,
                    0x02011B3C, 0x02011AF0, 0x02011ABC, 0x02011AFC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01697C80,
                0x016A0220,
                0x02011F80,
                0x02012F80,
            },
            {
                0x0155DB00,
                0x015690A0,
                {0x020153E8, 0x020004D8, 0x02005EDC, 0x02005E88, 0x020155BC,
                    0x02015690, 0x02015630, 0x0201560C, 0x02015648, 0x020004C0,
                    0x020004C4, 0x020004C8, 0x020004CC, 0x020004D0, 0x020004D4},
                100,
                0,
                0x0155DB00,
                0x015690A0,
                0x02015AA0,
                0x02016AA0,
            },
            {
                0x0164CBE0,
                0x016525C0,
                {0x0200BD58, 0x02000018, 0x02004C18, 0x02004BC4, 0x0200BF34,
                    0x0200BFF0, 0x0200BFA8, 0x0200BF84, 0x0200BFC0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0164CBE0,
                0x016525C0,
                0x0200C400,
                0x0200D400,
            },
            {
                0x0153DEA0,
                0x01543EC0,
                {0x020109B8, 0x02000018, 0x020071F0, 0x02007158, 0x02010BC4,
                    0x02010CA0, 0x02010C38, 0x02010C14, 0x02010C50, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0153DEA0,
                0x01543EC0,
                0x020110E8,
                0x020120E8,
            },
            {
                0x016DBA80,
                0x016E2A10,
                {0x020103C0, 0x02000540, 0x02005DBC, 0x02005D68, 0x02010554,
                    0x02010618, 0x020105BC, 0x02010590, 0x020105D0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016DBA80,
                0x016E2A10,
                0x02010990,
                0x02011990,
            },
            {
                0x01624100,
                0x0162D080,
                {0x020130F0, 0x02000018, 0x02007574, 0x02007520, 0x02013334,
                    0x0201344C, 0x020133EC, 0x020133D4, 0x020133F4, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                120,
                0,
                0x01624100,
                0x0162D080,
                0x020138F8,
                0x020148F8,
            },
            {
                0x016BF760,
                0x016C7100,
                {0x0200FD50, 0x02000018, 0x02005890, 0x0200583C, 0x0200FED8,
                    0x0200FFB0, 0x0200FF44, 0x0200FF28, 0x0200FF50, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                150,
                0,
                0x016BF760,
                0x016C7100,
                0x020102F0,
                0x020112F0,
            },
            {
                0x0171A110,
                0x0171EB40,
                {0x020089A8, 0x02000018, 0x02000308, 0x02000270, 0x02008BCC,
                    0x02008C48, 0x02008C20, 0x02008C1C, 0x02008C38, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0171A110,
                0x0171EB40,
                0x02009090,
                0x02009890,
            },
            {
                0x015B43A0,
                0x015B9750,
                {0x0200F620, 0x02000018, 0x02007FF4, 0x02007FA0, 0x0200F794,
                    0x0200F888, 0x0200F80C, 0x0200F7D0, 0x0200F828, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015B43A0,
                0x015B9750,
                0x0200FBC8,
                0x02010BC8,
            },
            {
                0x015CE6E0,
                0x015D53C0,
                {0x02012760, 0x02000018, 0x02007C78, 0x02007C24, 0x020129C0,
                    0x02012B20, 0x02012AA0, 0x02012A74, 0x02012AC0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015CE6E0,
                0x015D53C0,
                0x02013000,
                0x02014000,
            },
            {
                0x01598100,
                0x015A19A0,
                {0x02019B68, 0x02000018, 0x0200CFEC, 0x0200CF98, 0x02019DE4,
                    0x02019EE8, 0x02019E50, 0x02019E20, 0x02019E70, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01598100,
                0x015A19A0,
                0x0201A468,
                0x0201B468,
            },
            {
                0x0163A970,
                0x016432D0,
                {0x02010930, 0x02000540, 0x020069BC, 0x02006968, 0x02010AF0,
                    0x02010B74, 0x02010B30, 0x02010B2C, 0x02010B34, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0163A970,
                0x016432D0,
                0x02010F50,
                0x02011750,
            },
        },
        {
            {
                0x0165BD50,
                0x0165C0C0,
                {0x02010F70, 0x02000540, 0x02005DD8, 0x02005488, 0x020111DC,
                    0x020112E4, 0x020112AC, 0x02011290, 0x020112B4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01652EB0,
                0x0165BD50,
                0x02011760,
                0x02012760,
            },
            {
                0x015F6EC0,
                0x015F7400,
                {0x02010C80, 0x02000540, 0x02004B10, 0x020041C0, 0x02010EF0,
                    0x02010FE8, 0x02010FB8, 0x02010FA4, 0x02010FC0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015EE790,
                0x015F6EC0,
                0x02011460,
                0x02012460,
            },
            {
                0x0160ED90,
                0x0160F870,
                {0x02012B40, 0x02000540, 0x02006FA8, 0x02006658, 0x02012DB0,
                    0x02012EDC, 0x02012E90, 0x02012E64, 0x02012E9C, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01606090,
                0x0160ED90,
                0x02013358,
                0x02014358,
            },
            {
                0x016A73A0,
                0x016A7A50,
                {0x0200FF78, 0x02000540, 0x02005800, 0x02004F10, 0x020101AC,
                    0x020102BC, 0x0201027C, 0x02010260, 0x02010284, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                70,
                0,
                0x016A1220,
                0x016A73A0,
                0x02010738,
                0x02011738,
            },
            {
                0x0154C190,
                0x0154C370,
                {0x0200EFD0, 0x02000540, 0x02005660, 0x02004D10, 0x0200F1F8,
                    0x0200F318, 0x0200F2D4, 0x0200F2AC, 0x0200F2E0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01545D80,
                0x0154C190,
                0x0200F6F8,
                0x020106F8,
            },
            {
                0x015DE6D0,
                0x015E0570,
                {0x02011338, 0x02000540, 0x02005278, 0x02004920, 0x020115D8,
                    0x02011700, 0x020116B0, 0x0201168C, 0x020116B8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015D5A90,
                0x015DE6D0,
                0x02011BE0,
                0x02012BE0,
            },
            {
                0x01681F70,
                0x01689380,
                {0x0200FC10, 0x02000540, 0x02004910, 0x02003FC0, 0x0200FE80,
                    0x0200FF68, 0x0200FF38, 0x0200FF34, 0x0200FF40, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0167A3C0,
                0x01681F70,
                0x020103E0,
                0x02010BE0,
            },
            {
                0x0155D0C0,
                0x0155D450,
                {0x02011530, 0x02000540, 0x02005660, 0x02004D10, 0x020117B0,
                    0x020118C8, 0x02011888, 0x02011864, 0x02011890, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015544C0,
                0x0155D0C0,
                0x02011D78,
                0x02012D78,
            },
            {
                0x0164C520,
                0x0164C5F0,
                {0x0200F640, 0x02000540, 0x02005A58, 0x02005108, 0x0200F868,
                    0x0200F978, 0x0200F940, 0x0200F91C, 0x0200F948, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01644C90,
                0x0164C520,
                0x0200FD58,
                0x02010D58,
            },
            {
                0x015384B0,
                0x01538880,
                {0x0200C4E0, 0x02000540, 0x02004E58, 0x02004508, 0x0200C704,
                    0x0200C814, 0x0200C7DC, 0x0200C7B8, 0x0200C7E4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01533380,
                0x015384B0,
                0x0200CBF0,
                0x0200DBF0,
            },
            {
                0x016CE3C0,
                0x016D2C20,
                {0x0200E668, 0x02000540, 0x020040F8, 0x020037C8, 0x0200E8BC,
                    0x0200E9D4, 0x0200E99C, 0x0200E970, 0x0200E9A4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016C88B0,
                0x016CE3C0,
                0x0200EE18,
                0x0200FE18,
            },
            {
                0x01622860,
                0x01622E00,
                {0x020119E8, 0x02000540, 0x020074D0, 0x02006B68, 0x02011C48,
                    0x02011D78, 0x02011D1C, 0x02011CFC, 0x02011D28, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                120,
                0,
                0x0161B430,
                0x01622860,
                0x020121C0,
                0x020131C0,
            },
            {
                0x016BDB40,
                0x016BE230,
                {0x02010148, 0x02000540, 0x02005ED8, 0x02005570, 0x0201031C,
                    0x0201044C, 0x020103EC, 0x020103D0, 0x020103F4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                150,
                0,
                0x016B72B0,
                0x016BDB40,
                0x02010790,
                0x02011790,
            },
            {
                0x0171EB40,
                0x0171EF00,
                {0x02007820, 0x02000018, 0x02000868, 0x02000078, 0x02007A5C,
                    0x02007B20, 0x02007B14, 0x02007B10, 0x02007B18, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01715F00,
                0x0171A110,
                0x02007F00,
                0x02008700,
            },
            {
                0x015AB320,
                0x015AC1B0,
                {0x0200FB18, 0x02000540, 0x02005668, 0x02004D10, 0x0200FD7C,
                    0x0200FEA0, 0x0200FE5C, 0x0200FE30, 0x0200FE68, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015A4050,
                0x015AB320,
                0x020102E8,
                0x020112E8,
            },
            {
                0x015CD7B0,
                0x015CDCB0,
                {0x0200F000, 0x02000540, 0x02004AE0, 0x02004188, 0x0200F208,
                    0x0200F310, 0x0200F2D8, 0x0200F2BC, 0x0200F2E0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015C67B0,
                0x015CD7B0,
                0x0200F720,
                0x02010720,
            },
            {
                0x01575410,
                0x01580D90,
                {0x02016498, 0x02000540, 0x02006FA8, 0x02006658, 0x02016704,
                    0x02016830, 0x020167E4, 0x020167B8, 0x020167F0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0156A510,
                0x01575410,
                0x02016CA8,
                0x02017CA8,
            },
            {
                0x01638FA0,
                0x01639330,
                {0x02012A48, 0x02000540, 0x02006A20, 0x020060D0, 0x02012CB4,
                    0x02012DA8, 0x02012D6C, 0x02012D68, 0x02012D70, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01630880,
                0x01638FA0,
                0x02013220,
                0x02013A20,
            },
            {
                0x0166EDE0,
                0x0166F670,
                {0x02010FD0, 0x02000018, 0x02005F4C, 0x02005EF8, 0x020111A0,
                    0x0201123C, 0x020111F8, 0x020111DC, 0x02011204, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x016651E0,
                0x0166EDE0,
                0x02011650,
                0x02012650,
            },
            {
                0x01605EE0,
                0x01605F70,
                {0x02019188, 0x02000018, 0x0200A60C, 0x0200A5B8, 0x02019314,
                    0x020193F8, 0x0201937C, 0x02019350, 0x02019398, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015F7E90,
                0x01605EE0,
                0x02019770,
                0x0201A770,
            },
            {
                0x0161AC20,
                0x0161AEC0,
                {0x0201A6D0, 0x02000018, 0x0200CFF8, 0x0200CFA4, 0x0201A8C4,
                    0x0201A9C4, 0x0201A930, 0x0201A900, 0x0201A94C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01610E80,
                0x0161AC20,
                0x0201AE40,
                0x0201BE40,
            },
            {
                0x016B0170,
                0x016B0A60,
                {0x02013EB0, 0x02000018, 0x02009D64, 0x02009D10, 0x0201407C,
                    0x02014194, 0x02014114, 0x020140E0, 0x0201412C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                70,
                0,
                0x016A8840,
                0x016B0170,
                0x02014570,
                0x02015570,
            },
            {
                0x01552B20,
                0x015533C0,
                {0x02010018, 0x02000018, 0x02006F84, 0x02006F30, 0x020101BC,
                    0x0201027C, 0x0201021C, 0x020101F8, 0x02010234, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0154C850,
                0x01552B20,
                0x02010628,
                0x02011628,
            },
            {
                0x015EC1B0,
                0x015ECE50,
                {0x02015FD8, 0x02000018, 0x0200AB6C, 0x0200AB18, 0x0201617C,
                    0x02016290, 0x020161E4, 0x020161B8, 0x02016208, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015E2E00,
                0x015EC1B0,
                0x02016638,
                0x02017638,
            },
            {
                0x016A0220,
                0x016A0500,
                {0x02011898, 0x02000018, 0x020067D8, 0x02006784, 0x02011A80,
                    0x02011B3C, 0x02011AF0, 0x02011ABC, 0x02011AFC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01697C80,
                0x016A0220,
                0x02011F80,
                0x02012F80,
            },
            {
                0x015690A0,
                0x01569760,
                {0x02015448, 0x02000540, 0x02005F44, 0x02005EF0, 0x0201561C,
                    0x020156F0, 0x02015690, 0x0201566C, 0x020156A8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0155DB00,
                0x015690A0,
                0x02015B00,
                0x02016B00,
            },
            {
                0x016525C0,
                0x01652690,
                {0x0200BD58, 0x02000018, 0x02004C18, 0x02004BC4, 0x0200BF34,
                    0x0200BFF0, 0x0200BFA8, 0x0200BF84, 0x0200BFC0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0164CBE0,
                0x016525C0,
                0x0200C400,
                0x0200D400,
            },
            {
                0x01543EC0,
                0x015448E0,
                {0x020109C0, 0x02000018, 0x020071F0, 0x02007158, 0x02010BCC,
                    0x02010CA8, 0x02010C40, 0x02010C1C, 0x02010C58, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0153DEA0,
                0x01543EC0,
                0x020110F0,
                0x020120F0,
            },
            {
                0x016E2A10,
                0x016E3460,
                {0x02010380, 0x02000540, 0x02005DBC, 0x02005D68, 0x02010514,
                    0x020105D8, 0x0201057C, 0x02010550, 0x02010590, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016DBA80,
                0x016E2A10,
                0x02010950,
                0x02011950,
            },
            {
                0x0162D080,
                0x0162E020,
                {0x020130E8, 0x02000018, 0x02007574, 0x02007520, 0x0201332C,
                    0x02013444, 0x020133E4, 0x020133CC, 0x020133EC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                120,
                0,
                0x01624100,
                0x0162D080,
                0x020138F0,
                0x020148F0,
            },
            {
                0x016C7100,
                0x016C78C0,
                {0x0200FD50, 0x02000018, 0x02005890, 0x0200583C, 0x0200FED8,
                    0x0200FFB0, 0x0200FF44, 0x0200FF28, 0x0200FF50, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                150,
                0,
                0x016BF760,
                0x016C7100,
                0x020102F0,
                0x020112F0,
            },
            {
                0x0171EF00,
                0x0171F1F0,
                {0x020089B0, 0x02000018, 0x02000308, 0x02000270, 0x02008BD4,
                    0x02008C50, 0x02008C28, 0x02008C24, 0x02008C40, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0171A110,
                0x0171EB40,
                0x02009098,
                0x02009898,
            },
            {
                0x015B9750,
                0x015BDCC0,
                {0x0200FE40, 0x02000018, 0x020087FC, 0x020087A8, 0x0200FFB4,
                    0x020100B0, 0x0201002C, 0x0200FFF0, 0x02010048, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015B43A0,
                0x015B9750,
                0x020103F0,
                0x020113F0,
            },
            {
                0x015D53C0,
                0x015D5470,
                {0x02012760, 0x02000018, 0x02007C78, 0x02007C24, 0x020129C0,
                    0x02012B20, 0x02012AA0, 0x02012A74, 0x02012AC0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015CE6E0,
                0x015D53C0,
                0x02013000,
                0x02014000,
            },
            {
                0x015A19A0,
                0x015A2670,
                {0x02019B98, 0x02000018, 0x0200CFEC, 0x0200CF98, 0x02019E14,
                    0x02019F18, 0x02019E80, 0x02019E50, 0x02019EA0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01598100,
                0x015A19A0,
                0x0201A498,
                0x0201B498,
            },
            {
                0x016432D0,
                0x01643930,
                {0x020108C0, 0x020004D0, 0x0200694C, 0x020068F8, 0x02010A80,
                    0x02010B04, 0x02010AC0, 0x02010ABC, 0x02010AC4, 0x020004B8,
                    0x020004BC, 0x020004C0, 0x020004C4, 0x020004C8, 0x020004CC},
                100,
                0,
                0x0163A970,
                0x016432D0,
                0x02010EE0,
                0x020116E0,
            },
        },
        {
            {
                0x0165C0C0,
                0x0165C430,
                {0x02010F70, 0x02000540, 0x02005DD8, 0x02005488, 0x020111DC,
                    0x020112E4, 0x020112AC, 0x02011290, 0x020112B4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01652EB0,
                0x0165BD50,
                0x02011760,
                0x02012760,
            },
            {
                0x015F7400,
                0x015F7950,
                {0x02010C80, 0x02000540, 0x02004B10, 0x020041C0, 0x02010EF0,
                    0x02010FE8, 0x02010FB8, 0x02010FA4, 0x02010FC0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015EE790,
                0x015F6EC0,
                0x02011460,
                0x02012460,
            },
            {
                0x0160F870,
                0x01610390,
                {0x02012B40, 0x02000540, 0x02006FA8, 0x02006658, 0x02012DB0,
                    0x02012EDC, 0x02012E90, 0x02012E64, 0x02012E9C, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01606090,
                0x0160ED90,
                0x02013358,
                0x02014358,
            },
            {
                0x016A7A50,
                0x016A8190,
                {0x0200FF68, 0x02000540, 0x02005800, 0x02004F10, 0x0201019C,
                    0x020102AC, 0x0201026C, 0x02010250, 0x02010274, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                70,
                0,
                0x016A1220,
                0x016A73A0,
                0x02010728,
                0x02011728,
            },
            {
                0x0154C370,
                0x0154C530,
                {0x0200EFD0, 0x02000540, 0x02005660, 0x02004D10, 0x0200F1F8,
                    0x0200F318, 0x0200F2D4, 0x0200F2AC, 0x0200F2E0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01545D80,
                0x0154C190,
                0x0200F6F8,
                0x020106F8,
            },
            {
                0x015E0570,
                0x015E19B0,
                {0x020112F0, 0x02000540, 0x02005278, 0x02004920, 0x02011590,
                    0x020116B8, 0x02011668, 0x02011644, 0x02011670, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015D5A90,
                0x015DE6D0,
                0x02011B98,
                0x02012B98,
            },
            {
                0x01689380,
                0x01690730,
                {0x0200FBC8, 0x02000540, 0x020048C8, 0x02003FC0, 0x0200FE38,
                    0x0200FF20, 0x0200FEF0, 0x0200FEEC, 0x0200FEF8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0167A3C0,
                0x01681F70,
                0x02010398,
                0x02010B98,
            },
            {
                0x0155D450,
                0x0155D7E0,
                {0x02011530, 0x02000540, 0x02005660, 0x02004D10, 0x020117B0,
                    0x020118C8, 0x02011888, 0x02011864, 0x02011890, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015544C0,
                0x0155D0C0,
                0x02011D78,
                0x02012D78,
            },
            {
                0x0164C5F0,
                0x0164CB00,
                {0x0200F648, 0x02000540, 0x02005A40, 0x02005108, 0x0200F870,
                    0x0200F980, 0x0200F948, 0x0200F924, 0x0200F950, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01644C90,
                0x0164C520,
                0x0200FD60,
                0x02010D60,
            },
            {
                0x01538880,
                0x01538CA0,
                {0x0200C4E0, 0x02000540, 0x02004E58, 0x02004508, 0x0200C704,
                    0x0200C814, 0x0200C7DC, 0x0200C7B8, 0x0200C7E4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01533380,
                0x015384B0,
                0x0200CBF0,
                0x0200DBF0,
            },
            {
                0x016D2C20,
                0x016D7650,
                {0x0200E668, 0x02000540, 0x02004110, 0x020037C8, 0x0200E8BC,
                    0x0200E9D4, 0x0200E99C, 0x0200E970, 0x0200E9A4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016C88B0,
                0x016CE3C0,
                0x0200EE18,
                0x0200FE18,
            },
            {
                0x01622E00,
                0x016237F0,
                {0x020119E8, 0x02000540, 0x020074D0, 0x02006B68, 0x02011C48,
                    0x02011D78, 0x02011D1C, 0x02011CFC, 0x02011D28, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                120,
                0,
                0x0161B430,
                0x01622860,
                0x020121C0,
                0x020131C0,
            },
            {
                0x016BE230,
                0x016BF010,
                {0x020101F8, 0x02000540, 0x02005ED8, 0x02005570, 0x020103CC,
                    0x020104FC, 0x0201049C, 0x02010480, 0x020104A4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                150,
                0,
                0x016B72B0,
                0x016BDB40,
                0x02010840,
                0x02011840,
            },
            {
                0x0171F1F0,
                0x0171F210,
                {0x02007900, 0x02000018, 0x02000898, 0x02000078, 0x02007B3C,
                    0x02007C00, 0x02007BF4, 0x02007BF0, 0x02007BF8, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01715F00,
                0x0171A110,
                0x02007FE0,
                0x020087E0,
            },
            {
                0x015AC1B0,
                0x015AD030,
                {0x0200FB18, 0x02000540, 0x02005668, 0x02004D10, 0x0200FD7C,
                    0x0200FEA0, 0x0200FE5C, 0x0200FE30, 0x0200FE68, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015A4050,
                0x015AB320,
                0x020102E8,
                0x020112E8,
            },
            {
                0x015CDCB0,
                0x015CE220,
                {0x0200EFA8, 0x02000540, 0x02004AE0, 0x02004188, 0x0200F1B0,
                    0x0200F2B8, 0x0200F280, 0x0200F264, 0x0200F288, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015C67B0,
                0x015CD7B0,
                0x0200F6C8,
                0x020106C8,
            },
            {
                0x01580D90,
                0x0158C750,
                {0x02016490, 0x02000540, 0x02006FA0, 0x02006658, 0x020166FC,
                    0x02016828, 0x020167DC, 0x020167B0, 0x020167E8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0156A510,
                0x01575410,
                0x02016CA0,
                0x02017CA0,
            },
            {
                0x01639330,
                0x01639A30,
                {0x02012A88, 0x02000540, 0x02006A20, 0x020060D0, 0x02012CF4,
                    0x02012DE8, 0x02012DAC, 0x02012DA8, 0x02012DB0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01630880,
                0x01638FA0,
                0x02013260,
                0x02013A60,
            },
            {
                0x0166F670,
                0x0166FF00,
                {0x02010FD0, 0x02000018, 0x02005F4C, 0x02005EF8, 0x020111A0,
                    0x0201123C, 0x020111F8, 0x020111DC, 0x02011204, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x016651E0,
                0x0166EDE0,
                0x02011650,
                0x02012650,
            },
            {
                0x01605F70,
                0x01606000,
                {0x02019188, 0x02000018, 0x0200A60C, 0x0200A5B8, 0x02019314,
                    0x020193F8, 0x0201937C, 0x02019350, 0x02019398, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015F7E90,
                0x01605EE0,
                0x02019770,
                0x0201A770,
            },
            {
                0x0161AEC0,
                0x0161B180,
                {0x0201A6D0, 0x02000018, 0x0200CFF8, 0x0200CFA4, 0x0201A8C4,
                    0x0201A9C4, 0x0201A930, 0x0201A900, 0x0201A94C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01610E80,
                0x0161AC20,
                0x0201AE40,
                0x0201BE40,
            },
            {
                0x016B0A60,
                0x016B1300,
                {0x02013EA0, 0x02000018, 0x02009D64, 0x02009D10, 0x0201406C,
                    0x02014184, 0x02014104, 0x020140D0, 0x0201411C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                70,
                0,
                0x016A8840,
                0x016B0170,
                0x02014560,
                0x02015560,
            },
            {
                0x015533C0,
                0x01553C40,
                {0x02010018, 0x02000018, 0x02006F84, 0x02006F30, 0x020101BC,
                    0x0201027C, 0x0201021C, 0x020101F8, 0x02010234, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0154C850,
                0x01552B20,
                0x02010628,
                0x02011628,
            },
            {
                0x015ECE50,
                0x015EDAF0,
                {0x02015FD8, 0x02000018, 0x0200AB6C, 0x0200AB18, 0x0201617C,
                    0x02016290, 0x020161E4, 0x020161B8, 0x02016208, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015E2E00,
                0x015EC1B0,
                0x02016638,
                0x02017638,
            },
            {
                0x016A0500,
                0x016A0770,
                {0x02011898, 0x02000018, 0x020067D8, 0x02006784, 0x02011A80,
                    0x02011B3C, 0x02011AF0, 0x02011ABC, 0x02011AFC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01697C80,
                0x016A0220,
                0x02011F80,
                0x02012F80,
            },
            {
                0x01569760,
                0x01569E60,
                {0x02015448, 0x02000540, 0x02005F44, 0x02005EF0, 0x0201561C,
                    0x020156F0, 0x02015690, 0x0201566C, 0x020156A8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0155DB00,
                0x015690A0,
                0x02015B00,
                0x02016B00,
            },
            {
                0x01652690,
                0x01652A60,
                {0x0200BD68, 0x02000018, 0x02004C18, 0x02004BC4, 0x0200BF44,
                    0x0200C000, 0x0200BFB8, 0x0200BF94, 0x0200BFD0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0164CBE0,
                0x016525C0,
                0x0200C410,
                0x0200D410,
            },
            {
                0x015448E0,
                0x01545360,
                {0x020109C0, 0x02000018, 0x020071F0, 0x02007158, 0x02010BCC,
                    0x02010CA8, 0x02010C40, 0x02010C1C, 0x02010C58, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0153DEA0,
                0x01543EC0,
                0x020110F0,
                0x020120F0,
            },
            {
                0x016E3460,
                0x016E3D90,
                {0x02010380, 0x02000540, 0x02005DBC, 0x02005D68, 0x02010514,
                    0x020105D8, 0x0201057C, 0x02010550, 0x02010590, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016DBA80,
                0x016E2A10,
                0x02010950,
                0x02011950,
            },
            {
                0x0162E020,
                0x0162F580,
                {0x020130E8, 0x02000018, 0x02007574, 0x02007520, 0x0201332C,
                    0x02013444, 0x020133E4, 0x020133CC, 0x020133EC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                120,
                0,
                0x01624100,
                0x0162D080,
                0x020138F0,
                0x020148F0,
            },
            {
                0x016C78C0,
                0x016C8080,
                {0x0200FD50, 0x02000018, 0x02005890, 0x0200583C, 0x0200FED8,
                    0x0200FFB0, 0x0200FF44, 0x0200FF28, 0x0200FF50, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                150,
                0,
                0x016BF760,
                0x016C7100,
                0x020102F0,
                0x020112F0,
            },
            {
                0x0171F210,
                0x0171F500,
                {0x020089B0, 0x02000018, 0x02000308, 0x02000270, 0x02008BD4,
                    0x02008C50, 0x02008C28, 0x02008C24, 0x02008C40, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0171A110,
                0x0171EB40,
                0x02009098,
                0x02009898,
            },
            {
                0x015BDCC0,
                0x015C2240,
                {0x0200FE40, 0x02000018, 0x020087FC, 0x020087A8, 0x0200FFB4,
                    0x020100B0, 0x0201002C, 0x0200FFF0, 0x02010048, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015B43A0,
                0x015B9750,
                0x020103F0,
                0x020113F0,
            },
            {
                0x015D5470,
                0x015D59D0,
                {0x02012718, 0x02000018, 0x02007C78, 0x02007C24, 0x02012978,
                    0x02012AD8, 0x02012A58, 0x02012A2C, 0x02012A78, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015CE6E0,
                0x015D53C0,
                0x02012FB8,
                0x02013FB8,
            },
            {
                0x015A2670,
                0x015A3350,
                {0x02019B98, 0x02000018, 0x0200CFEC, 0x0200CF98, 0x02019E14,
                    0x02019F18, 0x02019E80, 0x02019E50, 0x02019EA0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01598100,
                0x015A19A0,
                0x0201A498,
                0x0201B498,
            },
            {
                0x01643930,
                0x01643D10,
                {0x02010980, 0x02000540, 0x020069BC, 0x02006968, 0x02010B40,
                    0x02010BC4, 0x02010B80, 0x02010B7C, 0x02010B84, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0163A970,
                0x016432D0,
                0x02010FA0,
                0x020117A0,
            },
        },
        {
            {
                0x0165C430,
                0x016651E0,
                {0x020109F8, 0x02000018, 0x02005860, 0x02004F60, 0x02010C64,
                    0x02010D6C, 0x02010D34, 0x02010D18, 0x02010D3C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01652EB0,
                0x0165BD50,
                0x020111E8,
                0x020121E8,
            },
            {
                0x015F7950,
                0x015F7E90,
                {0x02010C80, 0x02000540, 0x02004B10, 0x020041C0, 0x02010EF0,
                    0x02010FE8, 0x02010FB8, 0x02010FA4, 0x02010FC0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015EE790,
                0x015F6EC0,
                0x02011460,
                0x02012460,
            },
            {
                0x01610390,
                0x01610E80,
                {0x02012B40, 0x02000540, 0x02006FA8, 0x02006658, 0x02012DB0,
                    0x02012EDC, 0x02012E90, 0x02012E64, 0x02012E9C, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01606090,
                0x0160ED90,
                0x02013358,
                0x02014358,
            },
            {
                0x016A8190,
                0x016A8840,
                {0x0200FF78, 0x02000540, 0x02005800, 0x02004F10, 0x020101AC,
                    0x020102BC, 0x0201027C, 0x02010260, 0x02010284, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                70,
                0,
                0x016A1220,
                0x016A73A0,
                0x02010738,
                0x02011738,
            },
            {
                0x0154C530,
                0x0154C850,
                {0x0200EFD0, 0x02000540, 0x02005660, 0x02004D10, 0x0200F1F8,
                    0x0200F318, 0x0200F2D4, 0x0200F2AC, 0x0200F2E0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01545D80,
                0x0154C190,
                0x0200F6F8,
                0x020106F8,
            },
            {
                0x015E19B0,
                0x015E2E00,
                {0x020112F0, 0x02000540, 0x02005278, 0x02004920, 0x02011590,
                    0x020116B8, 0x02011668, 0x02011644, 0x02011670, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015D5A90,
                0x015DE6D0,
                0x02011B98,
                0x02012B98,
            },
            {
                0x01690730,
                0x01697C80,
                {0x0200FC10, 0x02000540, 0x02004910, 0x02003FC0, 0x0200FE80,
                    0x0200FF68, 0x0200FF38, 0x0200FF34, 0x0200FF40, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0167A3C0,
                0x01681F70,
                0x020103E0,
                0x02010BE0,
            },
            {
                0x0155D7E0,
                0x0155DB00,
                {0x02011530, 0x02000540, 0x02005660, 0x02004D10, 0x020117B0,
                    0x020118C8, 0x02011888, 0x02011864, 0x02011890, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015544C0,
                0x0155D0C0,
                0x02011D78,
                0x02012D78,
            },
            {
                0x0164CB00,
                0x0164CBE0,
                {0x0200F640, 0x02000540, 0x02005A58, 0x02005108, 0x0200F868,
                    0x0200F978, 0x0200F940, 0x0200F91C, 0x0200F948, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01644C90,
                0x0164C520,
                0x0200FD58,
                0x02010D58,
            },
            {
                0x01538CA0,
                0x0153DEA0,
                {0x0200BF68, 0x02000018, 0x020048E0, 0x02003FE0, 0x0200C18C,
                    0x0200C29C, 0x0200C264, 0x0200C240, 0x0200C26C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01533380,
                0x015384B0,
                0x0200C678,
                0x0200D678,
            },
            {
                0x016D7650,
                0x016DBA80,
                {0x0200E548, 0x02000540, 0x020040F8, 0x020037C8, 0x0200E79C,
                    0x0200E8B4, 0x0200E87C, 0x0200E850, 0x0200E884, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016C88B0,
                0x016CE3C0,
                0x0200ECF8,
                0x0200FCF8,
            },
            {
                0x016237F0,
                0x01624100,
                {0x020119E8, 0x02000540, 0x020074D0, 0x02006B68, 0x02011C48,
                    0x02011D78, 0x02011D1C, 0x02011CFC, 0x02011D28, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                120,
                0,
                0x0161B430,
                0x01622860,
                0x020121C0,
                0x020131C0,
            },
            {
                0x016BF010,
                0x016BF760,
                {0x02010148, 0x02000540, 0x02005ED8, 0x02005570, 0x0201031C,
                    0x0201044C, 0x020103EC, 0x020103D0, 0x020103F4, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                150,
                0,
                0x016B72B0,
                0x016BDB40,
                0x02010790,
                0x02011790,
            },
            {
                0x0171F500,
                0x0171F520,
                {0x02007900, 0x02000018, 0x02000898, 0x02000078, 0x02007B3C,
                    0x02007C00, 0x02007BF4, 0x02007BF0, 0x02007BF8, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01715F00,
                0x0171A110,
                0x02007FE0,
                0x020087E0,
            },
            {
                0x015AD030,
                0x015B43A0,
                {0x0200F5A0, 0x02000018, 0x020050F0, 0x020047E8, 0x0200F804,
                    0x0200F928, 0x0200F8E4, 0x0200F8B8, 0x0200F8F0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015A4050,
                0x015AB320,
                0x0200FD70,
                0x02010D70,
            },
            {
                0x015CE220,
                0x015CE6E0,
                {0x0200F060, 0x02000540, 0x02004AE0, 0x02004188, 0x0200F268,
                    0x0200F370, 0x0200F338, 0x0200F31C, 0x0200F340, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x015C67B0,
                0x015CD7B0,
                0x0200F780,
                0x02010780,
            },
            {
                0x0158C750,
                0x01598100,
                {0x02016498, 0x02000540, 0x02006FA8, 0x02006658, 0x02016704,
                    0x02016830, 0x020167E4, 0x020167B8, 0x020167F0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0156A510,
                0x01575410,
                0x02016CA8,
                0x02017CA8,
            },
            {
                0x01639A30,
                0x0163A970,
                {0x02012A88, 0x02000540, 0x02006A20, 0x020060D0, 0x02012CF4,
                    0x02012DE8, 0x02012DAC, 0x02012DA8, 0x02012DB0, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x01630880,
                0x01638FA0,
                0x02013260,
                0x02013A60,
            },
            {
                0x0166FF00,
                0x0167A3C0,
                {0x02012F38, 0x02000018, 0x02007EDC, 0x02007E88, 0x02013108,
                    0x020131B4, 0x02013160, 0x02013144, 0x0201316C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x016651E0,
                0x0166EDE0,
                0x020135C8,
                0x020145C8,
            },
            {
                0x01606000,
                0x01606090,
                {0x02019188, 0x02000018, 0x0200A60C, 0x0200A5B8, 0x02019314,
                    0x020193F8, 0x0201937C, 0x02019350, 0x02019398, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015F7E90,
                0x01605EE0,
                0x02019770,
                0x0201A770,
            },
            {
                0x0161B180,
                0x0161B430,
                {0x0201A6D0, 0x02000018, 0x0200CFF8, 0x0200CFA4, 0x0201A8C4,
                    0x0201A9C4, 0x0201A930, 0x0201A900, 0x0201A94C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01610E80,
                0x0161AC20,
                0x0201AE40,
                0x0201BE40,
            },
            {
                0x016B1300,
                0x016B1BF0,
                {0x02013EB0, 0x02000018, 0x02009D64, 0x02009D10, 0x0201407C,
                    0x02014194, 0x02014114, 0x020140E0, 0x0201412C, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                70,
                0,
                0x016A8840,
                0x016B0170,
                0x02014570,
                0x02015570,
            },
            {
                0x01553C40,
                0x015544C0,
                {0x02010018, 0x02000018, 0x02006F84, 0x02006F30, 0x020101BC,
                    0x0201027C, 0x0201021C, 0x020101F8, 0x02010234, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0154C850,
                0x01552B20,
                0x02010628,
                0x02011628,
            },
            {
                0x015EDAF0,
                0x015EE790,
                {0x02015FD8, 0x02000018, 0x0200AB6C, 0x0200AB18, 0x0201617C,
                    0x02016290, 0x020161E4, 0x020161B8, 0x02016208, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015E2E00,
                0x015EC1B0,
                0x02016638,
                0x02017638,
            },
            {
                0x016A0770,
                0x016A1220,
                {0x02011898, 0x02000018, 0x020067D8, 0x02006784, 0x02011A80,
                    0x02011B3C, 0x02011AF0, 0x02011ABC, 0x02011AFC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01697C80,
                0x016A0220,
                0x02011F80,
                0x02012F80,
            },
            {
                0x01569E60,
                0x0156A510,
                {0x02015448, 0x02000540, 0x02005F44, 0x02005EF0, 0x0201561C,
                    0x020156F0, 0x02015690, 0x0201566C, 0x020156A8, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x0155DB00,
                0x015690A0,
                0x02015B00,
                0x02016B00,
            },
            {
                0x01652A60,
                0x01652EB0,
                {0x0200BD40, 0x02000018, 0x02004C18, 0x02004BC4, 0x0200BF1C,
                    0x0200BFD8, 0x0200BF90, 0x0200BF6C, 0x0200BFA8, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0164CBE0,
                0x016525C0,
                0x0200C3E8,
                0x0200D3E8,
            },
            {
                0x01545360,
                0x01545D80,
                {0x020109C0, 0x02000018, 0x020071F0, 0x02007158, 0x02010BCC,
                    0x02010CA8, 0x02010C40, 0x02010C1C, 0x02010C58, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0153DEA0,
                0x01543EC0,
                0x020110F0,
                0x020120F0,
            },
            {
                0x016E3D90,
                0x016E46B0,
                {0x02010380, 0x02000540, 0x02005DBC, 0x02005D68, 0x02010514,
                    0x020105D8, 0x0201057C, 0x02010550, 0x02010590, 0x02000528,
                    0x0200052C, 0x02000530, 0x02000534, 0x02000538, 0x0200053C},
                100,
                0,
                0x016DBA80,
                0x016E2A10,
                0x02010950,
                0x02011950,
            },
            {
                0x0162F580,
                0x01630880,
                {0x020130E8, 0x02000018, 0x02007574, 0x02007520, 0x0201332C,
                    0x02013444, 0x020133E4, 0x020133CC, 0x020133EC, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                120,
                0,
                0x01624100,
                0x0162D080,
                0x020138F0,
                0x020148F0,
            },
            {
                0x016C8080,
                0x016C88B0,
                {0x0200FD50, 0x02000018, 0x02005890, 0x0200583C, 0x0200FED8,
                    0x0200FFB0, 0x0200FF44, 0x0200FF28, 0x0200FF50, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                150,
                0,
                0x016BF760,
                0x016C7100,
                0x020102F0,
                0x020112F0,
            },
            {
                0x0171F520,
                0x0171F540,
                {0x020089A8, 0x02000018, 0x02000308, 0x02000270, 0x02008BCC,
                    0x02008C48, 0x02008C20, 0x02008C1C, 0x02008C38, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x0171A110,
                0x0171EB40,
                0x02009090,
                0x02009890,
            },
            {
                0x015C2240,
                0x015C67B0,
                {0x0200FE40, 0x02000018, 0x020087FC, 0x020087A8, 0x0200FFB4,
                    0x020100B0, 0x0201002C, 0x0200FFF0, 0x02010048, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015B43A0,
                0x015B9750,
                0x020103F0,
                0x020113F0,
            },
            {
                0x015D59D0,
                0x015D5A90,
                {0x02012760, 0x02000018, 0x02007C78, 0x02007C24, 0x020129C0,
                    0x02012B20, 0x02012AA0, 0x02012A74, 0x02012AC0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x015CE6E0,
                0x015D53C0,
                0x02013000,
                0x02014000,
            },
            {
                0x015A3350,
                0x015A4050,
                {0x02019B98, 0x02000018, 0x0200CFEC, 0x0200CF98, 0x02019E14,
                    0x02019F18, 0x02019E80, 0x02019E50, 0x02019EA0, 0x02000000,
                    0x02000004, 0x02000008, 0x0200000C, 0x02000010, 0x02000014},
                100,
                0,
                0x01598100,
                0x015A19A0,
                0x0201A498,
                0x0201B498,
            },
            {
                0x01643D10,
                0x01644C90,
                {0x02010910, 0x020004D0, 0x0200694C, 0x020068F8, 0x02010AD0,
                    0x02010B54, 0x02010B10, 0x02010B0C, 0x02010B14, 0x020004B8,
                    0x020004BC, 0x020004C0, 0x020004C4, 0x020004C8, 0x020004CC},
                100,
                0,
                0x0163A970,
                0x016432D0,
                0x02010F30,
                0x02011730,
            },
        },
};

/* Chosen instead of g_CharacterModels for a model kind of
 * CHARACTER_MODEL_KIND_COUNT or more */
const CharacterModel g_OtherCharacterModels[OTHER_CHARACTER_MODEL_COUNT] = {
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x016E46B0,
        0x016ED4B0,
        {0x020124B8, 0x02000018, 0x020053CC, 0x02005348, 0x02012670, 0x02012730,
            0x020126E4, 0x020126C0, 0x020126F0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016E46B0,
        0x016ED4B0,
        0x02012B10,
        0x02013B10,
    },
    {
        0x0170CDB0,
        0x017107A0,
        {0x02007DF8, 0x02000540, 0x02001E84, 0x02001E6C, 0x02007F6C, 0x02007FD4,
            0x02007FAC, 0x02007FA8, 0x02007FB4, 0x02000528, 0x0200052C,
            0x02000530, 0x02000534, 0x02000538, 0x0200053C},
        100,
        0,
        0x0170CDB0,
        0x017107A0,
        0x02008318,
        0x02008B18,
    },
    {
        0x01704B40,
        0x01709770,
        {0x02009088, 0x02000018, 0x020033BC, 0x02003398, 0x0200920C, 0x02009288,
            0x0200924C, 0x02009248, 0x02009258, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        60,
        0,
        0x01704B40,
        0x01709770,
        0x02009600,
        0x02009E00,
    },
    {
        0x01709770,
        0x0170CDB0,
        {0x020085C8, 0x02000018, 0x02003CF4, 0x02003CDC, 0x0200877C, 0x020087F8,
            0x020087BC, 0x020087B8, 0x020087C0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        120,
        0,
        0x01709770,
        0x0170CDB0,
        0x02008BD8,
        0x020093D8,
    },
    {
        0x016F7950,
        0x016FD9F0,
        {0x0200A888, 0x02000018, 0x02002FD4, 0x02002F98, 0x0200AA9C, 0x0200AB00,
            0x0200AADC, 0x0200AAD8, 0x0200AAE0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        60,
        0,
        0x016F7950,
        0x016FD9F0,
        0x0200AFE0,
        0x0200B7E0,
    },
    {
        0x016FD9F0,
        0x01704B40,
        {0x0200C7C0, 0x02000018, 0x020024BC, 0x02002450, 0x0200C9DC, 0x0200CA58,
            0x0200CA1C, 0x0200CA18, 0x0200CA20, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        60,
        0,
        0x016FD9F0,
        0x01704B40,
        0x0200CF38,
        0x0200D738,
    },
    {
        0x016ED4B0,
        0x016F5610,
        {0x02011338, 0x02000018, 0x02006314, 0x020062CC, 0x020114EC, 0x020115A0,
            0x0201154C, 0x02011528, 0x02011558, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        70,
        0,
        0x016ED4B0,
        0x016F5610,
        0x02011980,
        0x02012980,
    },
    {
        0x0152B6C0,
        0x01533380,
        {0x02010050, 0x02000018, 0x02004100, 0x02003880, 0x02010218, 0x02010288,
            0x02010250, 0x02010240, 0x02010258, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        100,
        0,
        0x0152B6C0,
        0x01533380,
        0x02010668,
        0x02011668,
    },
    {
        0x016F5610,
        0x016F7950,
        {0x02005340, 0x02000018, 0x020025F4, 0x020025DC, 0x0200548C, 0x02005500,
            0x020054CC, 0x020054C8, 0x020054D0, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        60,
        0,
        0x016F5610,
        0x016F7950,
        0x02005810,
        0x02006010,
    },
    {
        0x017107A0,
        0x01715F00,
        {0x0200D0A0, 0x02000018, 0x02004270, 0x020041DC, 0x0200D2A8, 0x0200D34C,
            0x0200D308, 0x0200D2E4, 0x0200D314, 0x02000000, 0x02000004,
            0x02000008, 0x0200000C, 0x02000010, 0x02000014},
        100,
        0,
        0x017107A0,
        0x01715F00,
        0x0200D790,
        0x0200E790,
    },
};

/*
 * Builds the slot's joint matrices for g_CurrentAnimationFrame into
 * g_CurrentJointMatrices. Each joint's translation, rotation and scale is
 * interpolated between the keys around the frame, or held at the nearest key
 * outside them or past g_CurrentAnimationEndFrame.
 *
 * Doesn't match: GCC keeps frame in $f6, where the original keeps it in $f2,
 * which shifts the other floating-point registers. Reload also puts yy on
 * the stack, in an 8-byte slot above rotation, to free a float pair for a
 * hoisted constant; the original spills nothing, so its locals sit 8 bytes
 * higher (translationMatrix at 0xD0, rotation at 0x190). Pads of 40 to 52
 * floats give 228 to 280 words, and other orders of the quaternion's
 * products 208 to 342. The register oracle finds no closer allocation, so
 * the code differs too. The permuter's best (199 words, 27 in shape) scale
 * rotationProgress by the angle before acosf sets it. 206 words differ, 42
 * in shape.
 */
#ifdef NON_MATCHING
void AnimateCharacterJoints(u32 slotIndex) {
  /* Pads the frame to match; without it 282 words differ, against 206 */
  f32 unused[46] UNUSED;
  f32 translationMatrix[4][4];
  f32 jointMatrix[4][4];
  f32 scaleMatrix[4][4];
  f32 rotation[4];
  f32(*rotationMatrix)[4];
  JointAnimation* joint;
  Mtx* matrix;
  f32 frame;
  f32 rotationFrame;
  f32 x;
  f32 y;
  f32 z;
  f32 progress;
  f32 remaining;
  f32 dot;
  Radians angle;
  f32 nextWeight;
  f32 spanWeight;
  f32 sinAngle;
  f32 lengthSquared;
  f32 xs;
  f32 ys;
  f32 zs;
  f32 wy;
  f32 wz;
  f32 xx;
  f32 yy;
  f32 zz;
  f32 xy;
  f32 yz;
  f32 xz;
  s16 isInterpolated;
  s32 keyIndex;
  s16 spanIndex;
  s16 nextSpanIndex;

  joint = (JointAnimation*)GetUsedCharacterSlot(slotIndex)
              ->model->segmentAddresses[CHARACTER_MODEL_JOINTS];
  matrix = g_CurrentJointMatrices;
  if (joint->translationKeyCount == END_OF_JOINT_ANIMATIONS) {
    return;
  }

  do {
    frame = g_CurrentAnimationFrame;
    isInterpolated = TRUE;
    if (joint->translationSpan.endFrame < frame ||
        frame < joint->translationSpan.startFrame) {
      keyIndex = 1;
      if (frame < joint->translationFrames[0]) {
        isInterpolated = FALSE;
        x = joint->translations[0][0];
        y = joint->translations[0][1];
        z = joint->translations[0][2];
      } else {
        for (; keyIndex < joint->translationKeyCount; keyIndex++) {
          if (frame < joint->translationFrames[keyIndex]) {
            joint->translationSpan.index = keyIndex - 1;
            joint->translationSpan.startFrame =
                joint->translationFrames[keyIndex - 1];
            joint->translationSpan.endFrame =
                joint->translationFrames[keyIndex];
            break;
          }
        }

        if (keyIndex == joint->translationKeyCount) {
          keyIndex--;
          isInterpolated = FALSE;
          x = joint->translations[keyIndex][0];
          y = joint->translations[keyIndex][1];
          z = joint->translations[keyIndex][2];
        }
      }
    }

    if (isInterpolated) {
      if (g_CurrentAnimationEndFrame < joint->translationSpan.endFrame) {
        x = joint->translations[joint->translationSpan.index][0];
        y = joint->translations[joint->translationSpan.index][1];
        z = joint->translations[joint->translationSpan.index][2];
      } else {
        /*
         * This and the other two progress fractions read the global again,
         * though frame holds it, to match
         */
        progress =
            (g_CurrentAnimationFrame - joint->translationSpan.startFrame) /
            (joint->translationSpan.endFrame -
                joint->translationSpan.startFrame);
        spanIndex = joint->translationSpan.index;
        nextSpanIndex = spanIndex + 1;
        remaining = 1.0 - progress;
        x = (remaining * joint->translations[spanIndex][0]) +
            (progress * joint->translations[nextSpanIndex][0]);
        y = (remaining * joint->translations[spanIndex][1]) +
            (progress * joint->translations[nextSpanIndex][1]);
        z = (remaining * joint->translations[spanIndex][2]) +
            (progress * joint->translations[nextSpanIndex][2]);
      }
    }

    guTranslateF(translationMatrix, x, y, z);

    rotationFrame = g_CurrentAnimationFrame;
    isInterpolated = TRUE;
    if (joint->rotationSpan.endFrame < rotationFrame ||
        rotationFrame < joint->rotationSpan.startFrame) {
      keyIndex = 1;
      if (rotationFrame < joint->rotationFrames[0]) {
        isInterpolated = FALSE;
        rotation[0] = joint->rotations[0][0] * QUATERNION_SCALE;
        rotation[1] = joint->rotations[0][1] * QUATERNION_SCALE;
        rotation[2] = joint->rotations[0][2] * QUATERNION_SCALE;
        rotation[3] = joint->rotations[0][3] * QUATERNION_SCALE;
      } else {
        for (; keyIndex < joint->rotationKeyCount; keyIndex++) {
          if (rotationFrame < joint->rotationFrames[keyIndex]) {
            joint->rotationSpan.index = keyIndex - 1;
            joint->rotationSpan.startFrame =
                joint->rotationFrames[keyIndex - 1];
            joint->rotationSpan.endFrame = joint->rotationFrames[keyIndex];
            break;
          }
        }

        if (keyIndex == joint->rotationKeyCount) {
          keyIndex--;
          isInterpolated = FALSE;
          rotation[0] = joint->rotations[keyIndex][0] * QUATERNION_SCALE;
          rotation[1] = joint->rotations[keyIndex][1] * QUATERNION_SCALE;
          rotation[2] = joint->rotations[keyIndex][2] * QUATERNION_SCALE;
          rotation[3] = joint->rotations[keyIndex][3] * QUATERNION_SCALE;
        }
      }
    }

    if (isInterpolated) {
      if (g_CurrentAnimationEndFrame < joint->rotationSpan.endFrame) {
        rotation[0] =
            joint->rotations[joint->rotationSpan.index][0] * QUATERNION_SCALE;
        rotation[1] =
            joint->rotations[joint->rotationSpan.index][1] * QUATERNION_SCALE;
        rotation[2] =
            joint->rotations[joint->rotationSpan.index][2] * QUATERNION_SCALE;
        rotation[3] =
            joint->rotations[joint->rotationSpan.index][3] * QUATERNION_SCALE;
      } else {
        /* Its own, as the original allocates it within the branch */
        f32 rotationProgress;

        spanIndex = joint->rotationSpan.index;
        nextSpanIndex = spanIndex + 1;
        dot = ((joint->rotations[nextSpanIndex][0] * QUATERNION_PRODUCT_SCALE) *
                  joint->rotations[spanIndex][0]) +
            ((joint->rotations[nextSpanIndex][1] * QUATERNION_PRODUCT_SCALE) *
                joint->rotations[spanIndex][1]) +
            ((joint->rotations[nextSpanIndex][2] * QUATERNION_PRODUCT_SCALE) *
                joint->rotations[spanIndex][2]) +
            ((joint->rotations[nextSpanIndex][3] * QUATERNION_PRODUCT_SCALE) *
                joint->rotations[spanIndex][3]);
        rotationProgress =
            (g_CurrentAnimationFrame - joint->rotationSpan.startFrame) /
            (joint->rotationSpan.endFrame - joint->rotationSpan.startFrame);
        /* Nearly equal rotations are blended linearly, where the sines below
         * would be near zero */
        if (1.0 - dot < SLERP_THRESHOLD) {
          nextWeight = rotationProgress;
          spanWeight = 1.0 - nextWeight;
        } else {
          angle = acosf(dot);
          rotationProgress *= angle;
          sinAngle = sinf(angle);
          nextWeight = sinf(rotationProgress) / sinAngle;
          spanWeight = sinf(angle - rotationProgress) / sinAngle;
        }

        spanWeight *= QUATERNION_SCALE;
        nextWeight *= QUATERNION_SCALE;
        rotation[0] = (spanWeight * joint->rotations[spanIndex][0]) +
            (nextWeight * joint->rotations[nextSpanIndex][0]);
        rotation[1] = (spanWeight * joint->rotations[spanIndex][1]) +
            (nextWeight * joint->rotations[nextSpanIndex][1]);
        rotation[2] = (spanWeight * joint->rotations[spanIndex][2]) +
            (nextWeight * joint->rotations[nextSpanIndex][2]);
        rotation[3] = (spanWeight * joint->rotations[spanIndex][3]) +
            (nextWeight * joint->rotations[nextSpanIndex][3]);
      }
    }

    /* The rotation quaternion (x, y, z, w) as a matrix */
    lengthSquared = (rotation[2] * rotation[2]) + (rotation[3] * rotation[3]) +
        (rotation[0] * rotation[0]) + (rotation[1] * rotation[1]);
    /* dot holds the quaternion's scale from here, and progress holds the w
     * and x term, to match */
    if (lengthSquared > 0.0f) {
      dot = 2.0f / lengthSquared;
    } else {
      dot = 0.0f;
    }

    rotationMatrix = jointMatrix;
    xs = rotation[0] * dot;
    ys = rotation[1] * dot;
    zs = rotation[2] * dot;
    progress = rotation[3] * xs;
    wy = rotation[3] * ys;
    wz = rotation[3] * zs;
    xx = rotation[0] * xs;
    yy = rotation[1] * ys;
    zz = rotation[2] * zs;
    xy = rotation[0] * ys;
    yz = rotation[1] * zs;
    xz = rotation[0] * zs;
    rotationMatrix[0][0] = 1.0f - (yy + zz);
    rotationMatrix[0][1] = xy + wz;
    rotationMatrix[0][2] = xz - wy;
    rotationMatrix[0][3] = 0.0f;
    rotationMatrix[1][0] = xy - wz;
    rotationMatrix[1][1] = 1.0f - (xx + zz);
    rotationMatrix[1][2] = yz + progress;
    rotationMatrix[1][3] = 0.0f;
    rotationMatrix[2][0] = xz + wy;
    rotationMatrix[2][1] = yz - progress;
    rotationMatrix[2][2] = 1.0f - (xx + yy);
    rotationMatrix[2][3] = 0.0f;
    rotationMatrix[3][0] = 0.0f;
    rotationMatrix[3][2] = rotationMatrix[3][1] = 0.0f;
    rotationMatrix[3][3] = 1.0f;

    frame = g_CurrentAnimationFrame;
    isInterpolated = TRUE;
    if (joint->scaleSpan.endFrame < frame ||
        frame < joint->scaleSpan.startFrame) {
      keyIndex = 1;
      if (frame < joint->scaleFrames[0]) {
        isInterpolated = FALSE;
        x = joint->scales[0][0];
        y = joint->scales[0][1];
        z = joint->scales[0][2];
      } else {
        for (; keyIndex < joint->scaleKeyCount; keyIndex++) {
          if (frame < joint->scaleFrames[keyIndex]) {
            joint->scaleSpan.index = keyIndex - 1;
            joint->scaleSpan.startFrame = joint->scaleFrames[keyIndex - 1];
            joint->scaleSpan.endFrame = joint->scaleFrames[keyIndex];
            break;
          }
        }

        if (keyIndex == joint->scaleKeyCount) {
          keyIndex--;
          isInterpolated = FALSE;
          x = joint->scales[keyIndex][0];
          y = joint->scales[keyIndex][1];
          z = joint->scales[keyIndex][2];
        }
      }
    }

    if (isInterpolated) {
      if (g_CurrentAnimationEndFrame < joint->scaleSpan.endFrame) {
        x = joint->scales[joint->scaleSpan.index][0];
        y = joint->scales[joint->scaleSpan.index][1];
        z = joint->scales[joint->scaleSpan.index][2];
      } else {
        progress = (g_CurrentAnimationFrame - joint->scaleSpan.startFrame) /
            (joint->scaleSpan.endFrame - joint->scaleSpan.startFrame);
        spanIndex = joint->scaleSpan.index;
        nextSpanIndex = spanIndex + 1;
        remaining = 1.0 - progress;
        x = (remaining * joint->scales[spanIndex][0]) +
            (progress * joint->scales[nextSpanIndex][0]);
        y = (remaining * joint->scales[spanIndex][1]) +
            (progress * joint->scales[nextSpanIndex][1]);
        z = (remaining * joint->scales[spanIndex][2]) +
            (progress * joint->scales[nextSpanIndex][2]);
      }
    }

    guScaleF(scaleMatrix, x, y, z);
    guMtxCatF(scaleMatrix, jointMatrix, jointMatrix);
    guMtxCatF(jointMatrix, translationMatrix, jointMatrix);
    guMtxF2L(jointMatrix, matrix++);
    joint++;
  } while (joint->translationKeyCount != END_OF_JOINT_ANIMATIONS);
}

#else
/* The double 1.0, which AnimateCharacterJoints loads as two words */
const u32 f_JointAnimationOneHighWord = 0x3FF00000;
const u32 f_JointAnimationOneLowWord = 0;

INCLUDE_ASM("asm/nonmatchings/character_slot", AnimateCharacterJoints);
/*
 * GCC last wrote the constants above, so it thinks the assembler is still in
 * .rodata, but INCLUDE_ASM left it in .text. Agree with GCC so the string below
 * lands in .rodata.
 */
RESUME_RODATA_SECTION();
#endif

NO_RESULT FreeCharacterSlot(u32 slotIndex) {
  osSyncPrintf("D PC[%d]\n", slotIndex);
  if (slotIndex < CHARACTER_SLOT_COUNT) {
    g_CharacterSlots[slotIndex].modelIndex = NO_CHARACTER_MODEL;
  }
}

/* A slot stays busy for two frames after it was last drawn, while the display
 * list may still use it */
Boolean IsCharacterSlotBusy(u32 slotIndex) {
  Boolean isBusy;

  if (slotIndex >= CHARACTER_SLOT_COUNT) {
    return TRUE;
  }

  isBusy = FALSE;
  if (g_CharacterSlots[slotIndex].modelIndex != NO_CHARACTER_MODEL ||
      g_FrameCount - g_CharacterSlots[slotIndex].lastDrawnFrame <
          BUSY_FRAME_COUNT) {
    isBusy = TRUE;
  }

  return isBusy;
}

Boolean IsAnyCharacterSlotBusy(void) {
  s32 slotIndex;

  for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
    if (IsCharacterSlotBusy(slotIndex)) {
      return TRUE;
    }
  }

  return FALSE;
}

/* Returns the slot, or NULL if it is out of range or empty */
CharacterSlot* GetUsedCharacterSlot(u32 slotIndex) {
  if (slotIndex < CHARACTER_SLOT_COUNT &&
      g_CharacterSlots[slotIndex].modelIndex != NO_CHARACTER_MODEL) {
    return &g_CharacterSlots[slotIndex];
  }

  return NULL;
}

void FreeAllCharacterSlots(void) {
  s32 slotIndex;

  for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT; slotIndex++) {
    FreeCharacterSlot(slotIndex);
  }
}

/*
 * Points the model's chosen display list at one of its display list choices.
 * The slot's model buffer must already be mapped. Returns 0, or -1 if the slot
 * is empty or the model has no such choice.
 */
s32 SelectCharacterDisplayList(u32 slotIndex, s32 choiceIndex) {
  CharacterSlot* slot;
  s32 i;

  slot = GetUsedCharacterSlot(slotIndex);
  if (slot != NULL) {
    i = 0;
    if (choiceIndex == NO_DISPLAY_LIST_CHOICE) {
      ModelChosenDisplayList(slot->model)->words.w0 = G_ENDDL << 24;
      ModelChosenDisplayList(slot->model)->words.w1 = 0;
      return 0;
    }

    for (; i < choiceIndex; i++) {
      if (ModelDisplayListChoices(slot->model)[i] ==
          END_OF_DISPLAY_LIST_CHOICES) {
        return -1;
      }
    }

    if (ModelDisplayListChoices(slot->model)[i] ==
        END_OF_DISPLAY_LIST_CHOICES) {
      return -1;
    }

    ModelChosenDisplayList(slot->model)->words.w0 = G_DL << 24;
    ModelChosenDisplayList(slot->model)->words.w1 =
        ModelDisplayListChoices(slot->model)[i];
    return 0;
  }

  return -1;
}

/* Returns the club's CLUB_KIND, or -1 if the slot is empty */
ClubKind GetCharacterClubKind(u32 slotIndex, s32 clubIndex) {
  CharacterSlot* slot;
  s32 kind;

  kind = -1;
  slot = GetUsedCharacterSlot(slotIndex);
  if (slot != NULL) {
    g_CharacterModelIndex = slot->modelIndex;
    switch (GetClub(clubIndex)->kind) {
      case CLUB_KIND_WOOD:
        kind = CLUB_KIND_WOOD;
        break;
      case CLUB_KIND_IRON:
        kind = CLUB_KIND_IRON;
        break;
      case CLUB_KIND_WEDGE:
        kind = CLUB_KIND_WEDGE;
        break;
      case CLUB_KIND_PUTTER:
        kind = CLUB_KIND_PUTTER;
        break;
      default:
        kind = -1;
        break;
    }
  }

  return kind;
}

/*
 * Finds the point's position on the model, before the slot's own transform.
 * Returns 0, or 1 if the model has no such point, or -1 if the slot is empty.
 * Leaves the position zero if not found.
 */
s32 GetCharacterPointModelPosition(u32 slotIndex, s32 pointId, f32* position) {
  Mtx pointMatrix;
  Mtx offsetMatrix;
  CharacterSlot* slot;
  CharacterPoint* point;
  s16* jointPath;

  slot = GetUsedCharacterSlot(slotIndex);
  if (slot != NULL) {
    SetMultiTlb(0, TLB_PAGE_MODE_4K, CHARACTER_MODEL_PAGE_COUNT,
        CHARACTER_MODEL_ADDRESS, osVirtualToPhysical(slot->modelBuffer));
    point =
        (CharacterPoint*)slot->model->segmentAddresses[CHARACTER_MODEL_POINTS];
    while (point->jointPath != NULL) {
      if (point->id == pointId) {
        break;
      }

      point++;
    }

    if (point->jointPath != NULL) {
      jointPath = point->jointPath;
      guMtxIdent(&pointMatrix);
      for (; *jointPath != END_OF_JOINT_PATH; jointPath++) {
        guMtxCatL(&slot->jointMatrices[*jointPath], &pointMatrix, &pointMatrix);
      }

      guMtxIdent(&offsetMatrix);
      guTranslate(
          &offsetMatrix, point->offset[0], point->offset[1], point->offset[2]);
      guMtxCatL(&offsetMatrix, &pointMatrix, &pointMatrix);
      guMtxXFML(&pointMatrix, 0.0f, 0.0f, 0.0f, &position[AXIS_X],
          &position[AXIS_Y], &position[AXIS_Z]);
      position[AXIS_X] *= MODEL_TO_WORLD_SCALE;
      position[AXIS_Y] *= MODEL_TO_WORLD_SCALE;
      position[AXIS_Z] *= MODEL_TO_WORLD_SCALE;
      return 0;
    }

    /*
     * The chained assignment is there to match: three separate stores, in
     * either order, in both places give 17 words differing, 13 in shape
     */
    position[AXIS_X] = position[AXIS_Y] = position[AXIS_Z] = 0.0f;
    return 1;
  }

  /*
   * The chained assignment is there to match: three separate stores, in
   * either order, in both places give 17 words differing, 13 in shape
   */
  position[AXIS_X] = position[AXIS_Y] = position[AXIS_Z] = 0.0f;
  return -1;
}

void GetCharacterSlotMatrix(CharacterSlot* slot, Mat4 matrix) {
  Mat4 translation;
  Mat4 transform;

  guTranslateF(translation, slot->position[AXIS_X] * SLOT_POSITION_SCALE,
      slot->position[AXIS_Y] * SLOT_POSITION_SCALE,
      slot->position[AXIS_Z] * SLOT_POSITION_SCALE);
  guRotateRPYF(transform,
      (s32)(ROLL_ORIGIN_DEGREES -
          (s32)(slot->rotation[0] * RADIANS_TO_DEGREES)),
      (s32)(slot->rotation[1] * RADIANS_TO_DEGREES),
      (s32)(slot->rotation[2] * RADIANS_TO_DEGREES));
  guMtxCatF(transform, translation, translation);
  guScaleF(transform, slot->scale[0], slot->scale[1], slot->scale[2]);
  guMtxCatF(transform, translation, matrix);
}

/* Like GetCharacterPointModelPosition, but in the world */
s32 GetCharacterPointPosition(u32 slotIndex, s32 pointId, f32* position) {
  Mat4 matrix;
  f32 modelPosition[3];
  s32 result;

  result = GetCharacterPointModelPosition(slotIndex, pointId, modelPosition);
  if (result < 0) {
    /*
     * The chained assignment is there to match: three separate stores, in
     * either order, give 8 words differing, all in shape
     */
    position[AXIS_X] = position[AXIS_Y] = position[AXIS_Z] = 0.0f;
  } else {
    GetCharacterSlotMatrix(GetUsedCharacterSlot(slotIndex), matrix);
    guMtxXFMF(matrix, modelPosition[AXIS_X], modelPosition[AXIS_Y],
        modelPosition[AXIS_Z], &position[AXIS_X], &position[AXIS_Y],
        &position[AXIS_Z]);
  }

  return result;
}

/* The model position of the point the character's shadow lies under */
void GetCharacterShadowPoint(u32 slotIndex, f32* position) {
  GetCharacterPointModelPosition(slotIndex, CHARACTER_POINT_SHADOW, position);
}
