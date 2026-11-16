/*
 * The slots that hold the loaded characters: their alpha and highlights, their
 * joint matrices, freeing them once no display list uses them, and the
 * positions of points on their models.
 */

#include "characters/character_model.h"

#include <ultra64.h>

#include "camera/camera.h"
#include "characters/model_layouts.h"
#include "course/hole_setup.h"
#include "game/game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "screen/display_list.h"
#include "shot/club.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/rom_files.h"

// How many frames a drawn slot stays busy
#define BUSY_FRAME_COUNT 2

// Like G_RM_AA_ZB_XLU_SURF, but also updating the depth buffer
#define CHARACTER_RENDER_MODE_TRANSLUCENT \
    (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL | G_RM_PASS)
#define CHARACTER_RENDER_MODE_TRANSLUCENT2 \
    (AA_EN | Z_CMP | Z_UPD | IM_RD | CLR_ON_CVG | CVG_DST_WRAP | ZMODE_XLU | FORCE_BL | \
        GBL_c2(G_BL_CLR_IN, G_BL_A_IN, G_BL_CLR_MEM, G_BL_1MA))

// The sides, in texels, of the hilite textures
#define HIGHLIGHT_TEXTURE_SIZE 32
#define HIGHLIGHT_SECOND_TILE 1

#define ModelChosenDisplayList(model) \
    ((Gfx *)(model)->segmentAddresses[CHARACTER_MODEL_CHOSEN_DISPLAY_LIST])
#define ModelDisplayListChoices(model) \
    ((u32 *)(model)->segmentAddresses[CHARACTER_MODEL_DISPLAY_LIST_CHOICES])

// osMapTLB's physical address for a page left unmapped
#define NO_PHYSICAL_PAGE (-1)
// osMapTLB's address space id for a global mapping
#define NO_TLB_ASID (-1)

#define MODEL_TO_WORLD_SCALE 0.01f
#define SLOT_POSITION_SCALE (1.0f / 1024)

CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];
s32 g_CurrentAnimationEndFrame;
f32 g_CurrentAnimationFrame;
Mtx *g_CurrentJointMatrices;

// The slot's first rotation is taken away from this roll
static LookAt f_HighlightLookAts[DRAW_BUFFER_COUNT];
static Hilite f_Highlights[DRAW_BUFFER_COUNT];

// Sets the characters' alpha, drawing them translucent below 0xFF, and their
// combiner for the weather
void SetCharacterAlpha(s32 alpha) {
    Gfx *gfx;
    s32 colorIndex;

    gfx = &g_CharacterEnvColorDisplayLists[0][0];
    for (colorIndex = 0; colorIndex != ENV_COLOR_COUNT; colorIndex++) {
        gDPSetEnvColor(
            gfx++,
            g_CharacterEnvColors[colorIndex][RED_CHANNEL],
            g_CharacterEnvColors[colorIndex][GREEN_CHANNEL],
            g_CharacterEnvColors[colorIndex][BLUE_CHANNEL],
            alpha
        );
        gSPEndDisplayList(gfx++);
    }

    gfx = g_CharacterRenderModeDisplayList;
    if ((alpha & OPAQUE_ALPHA) == OPAQUE_ALPHA) {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
    }
    else {
        gDPPipeSync(gfx++);
        gDPSetRenderMode(
            gfx++,
            CHARACTER_RENDER_MODE_TRANSLUCENT,
            CHARACTER_RENDER_MODE_TRANSLUCENT2
        );
    }

    gfx = g_CharacterCombineDisplayList;
    if (g_HoleSetup.conditions.isRaining) {
        gDPSetCombineLERP(
            gfx++,
            PRIMITIVE,
            0,
            SHADE,
            0,
            ENVIRONMENT,
            0,
            SHADE,
            0,
            PRIMITIVE,
            0,
            SHADE,
            0,
            ENVIRONMENT,
            0,
            SHADE,
            0
        );
    }
    else {
        gDPSetCombineLERP(
            gfx++,
            PRIMITIVE,
            0,
            SHADE,
            0,
            ENVIRONMENT,
            0,
            SHADE,
            0,
            ENVIRONMENT,
            COMBINED,
            TEXEL0,
            COMBINED,
            0,
            0,
            0,
            COMBINED
        );
    }
}

// Points the characters' highlights at the scene light, as seen from the main
// camera. The matrices are unused.
void SetCharacterHighlight(CameraMatrices *matrices UNUSED) {
    Mat4 lookAtMatrix;
    Vec3 lightDirection;
    Radians lightPitch;
    Radians lightYaw;
    Gfx *gfx;

    lightPitch = g_HoleSetup.conditions.lightPitch * BINARY_ANGLE_TO_RADIANS;
    lightYaw = g_HoleSetup.conditions.lightYaw * BINARY_ANGLE_TO_RADIANS;
    lightDirection[AXIS_X] = -sinf(lightPitch) * cosf(lightYaw) * LIGHT_DIRECTION_SCALE;
    lightDirection[AXIS_Y] = -cosf(lightPitch) * LIGHT_DIRECTION_SCALE;
    lightDirection[AXIS_Z] = -sinf(lightPitch) * sinf(lightYaw) * LIGHT_DIRECTION_SCALE;
    guLookAtHiliteF(
        lookAtMatrix,
        &f_HighlightLookAts[g_DrawBufferIndex],
        &f_Highlights[g_DrawBufferIndex],
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_X],
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y],
        g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z],
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X],
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y],
        g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z],
        0.0f,
        -1.0f,
        0.0f,
        (s8)lightDirection[AXIS_X],
        (s8)lightDirection[AXIS_Y],
        (s8)lightDirection[AXIS_Z],
        (s8)lightDirection[AXIS_X],
        (s8)lightDirection[AXIS_Y],
        (s8)lightDirection[AXIS_Z],
        HIGHLIGHT_TEXTURE_SIZE,
        HIGHLIGHT_TEXTURE_SIZE
    );
    gfx = g_CharacterHighlightDisplayList;
    gDPSetHilite1Tile(
        gfx++,
        G_TX_RENDERTILE,
        &f_Highlights[g_DrawBufferIndex],
        HIGHLIGHT_TEXTURE_SIZE,
        HIGHLIGHT_TEXTURE_SIZE
    );
    gDPSetHilite2Tile(
        gfx++,
        HIGHLIGHT_SECOND_TILE,
        &f_Highlights[g_DrawBufferIndex],
        HIGHLIGHT_TEXTURE_SIZE,
        HIGHLIGHT_TEXTURE_SIZE
    );
    gSPEndDisplayList(gfx++);
}

// Maps pageCount pages from virtualAddress to physicalAddress, starting at TLB
// entry firstTlbIndex. The pages are SMALLEST_PAGE_SIZE << pageMode bytes; odd
// page modes map each page as two TLB pages of half that size. Returns the next
// free TLB entry, or -1 if the addresses aren't page-aligned or the mode is
// invalid.
s32 SetMultiTlb(
    s32 firstTlbIndex,
    TlbPageMode pageMode,
    u32 pageCount,
    u32 virtualAddress,
    UnsignedAddress physicalAddress
) {
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
        case TLB_PAGE_MODE_8K:
            pageSize >>= 1;
            pageCount *= 2;

        // fallthrough
        case TLB_PAGE_MODE_4K:
            pageMask = OS_PM_4K;
            break;
        case TLB_PAGE_MODE_32K:
            pageSize >>= 1;
            pageCount *= 2;

        // fallthrough
        case TLB_PAGE_MODE_16K:
            pageMask = OS_PM_16K;
            break;
        case TLB_PAGE_MODE_128K:
            pageSize >>= 1;
            pageCount *= 2;

        // fallthrough
        case TLB_PAGE_MODE_64K:
            pageMask = OS_PM_64K;
            break;
        case TLB_PAGE_MODE_512K:
            pageSize >>= 1;
            pageCount *= 2;

        // fallthrough
        case TLB_PAGE_MODE_256K:
            pageMask = OS_PM_256K;
            break;
        default:
            osSyncPrintf("kSetMultiTLB : Invalid Page Mode\n");
            return -1;
    }

    // A TLB entry maps an even and odd page pair, so a lone odd page goes first
    if (virtualAddress & pageSize) {
        osUnmapTLB(tlbIndex);
        osMapTLB(
            tlbIndex++,
            pageMask,
            (void *)virtualAddress,
            NO_PHYSICAL_PAGE,
            physicalAddress,
            NO_TLB_ASID
        );
        virtualAddress += pageSize;
        physicalAddress += pageSize;
        pageCount--;
    }
    while (pageCount >= 2) {
        osUnmapTLB(tlbIndex);
        osMapTLB(
            tlbIndex++,
            pageMask,
            (void *)virtualAddress,
            physicalAddress,
            physicalAddress + pageSize,
            NO_TLB_ASID
        );
        virtualAddress += pageSize * 2;
        pageCount -= 2;
        physicalAddress += pageSize * 2;
    }

    if (pageCount != 0) {
        osUnmapTLB(tlbIndex);
        osMapTLB(
            tlbIndex++,
            pageMask,
            (void *)virtualAddress,
            physicalAddress,
            NO_PHYSICAL_PAGE,
            NO_TLB_ASID
        );
    }

    return tlbIndex;
}

// A character model's file in ROM, whose bounds the linker script defines
#define DeclareModelFile(name) DECLARE_ROM_FILE(character_models_##name)
#define ModelRomBounds(name) \
    (u32)(AddressNumber(character_models_##name##_ROM_START)), \
        (u32)(AddressNumber(character_models_##name##_ROM_END))

DeclareModelFile(plum);
DeclareModelFile(charlie);
DeclareModelFile(maple);
DeclareModelFile(baby_mario);
DeclareModelFile(luigi);
DeclareModelFile(peach);
DeclareModelFile(sonny);
DeclareModelFile(wario);
DeclareModelFile(harry);
DeclareModelFile(mario);
DeclareModelFile(yoshi);
DeclareModelFile(donkey_kong);
DeclareModelFile(bowser);
DeclareModelFile(metal_mario);
DeclareModelFile(kid);
DeclareModelFile(sherry);
DeclareModelFile(azalea);
DeclareModelFile(joe);
DeclareModelFile(plum_reaction);
DeclareModelFile(charlie_reaction);
DeclareModelFile(maple_reaction);
DeclareModelFile(baby_mario_reaction);
DeclareModelFile(luigi_reaction);
DeclareModelFile(peach_reaction);
DeclareModelFile(sonny_reaction);
DeclareModelFile(wario_reaction);
DeclareModelFile(harry_reaction);
DeclareModelFile(mario_reaction);
DeclareModelFile(yoshi_reaction);
DeclareModelFile(donkey_kong_reaction);
DeclareModelFile(bowser_reaction);
DeclareModelFile(metal_mario_reaction);
DeclareModelFile(kid_reaction);
DeclareModelFile(sherry_reaction);
DeclareModelFile(azalea_reaction);
DeclareModelFile(joe_reaction);
DeclareModelFile(plum_palette_1);
DeclareModelFile(charlie_palette_1);
DeclareModelFile(maple_palette_1);
DeclareModelFile(baby_mario_palette_1);
DeclareModelFile(luigi_palette_1);
DeclareModelFile(peach_palette_1);
DeclareModelFile(sonny_palette_1);
DeclareModelFile(wario_palette_1);
DeclareModelFile(harry_palette_1);
DeclareModelFile(mario_palette_1);
DeclareModelFile(yoshi_palette_1);
DeclareModelFile(donkey_kong_palette_1);
DeclareModelFile(bowser_palette_1);
DeclareModelFile(metal_mario_palette_1);
DeclareModelFile(kid_palette_1);
DeclareModelFile(sherry_palette_1);
DeclareModelFile(azalea_palette_1);
DeclareModelFile(joe_palette_1);
DeclareModelFile(plum_reaction_palette_1);
DeclareModelFile(charlie_reaction_palette_1);
DeclareModelFile(maple_reaction_palette_1);
DeclareModelFile(baby_mario_reaction_palette_1);
DeclareModelFile(luigi_reaction_palette_1);
DeclareModelFile(peach_reaction_palette_1);
DeclareModelFile(sonny_reaction_palette_1);
DeclareModelFile(wario_reaction_palette_1);
DeclareModelFile(harry_reaction_palette_1);
DeclareModelFile(mario_reaction_palette_1);
DeclareModelFile(yoshi_reaction_palette_1);
DeclareModelFile(donkey_kong_reaction_palette_1);
DeclareModelFile(bowser_reaction_palette_1);
DeclareModelFile(metal_mario_reaction_palette_1);
DeclareModelFile(kid_reaction_palette_1);
DeclareModelFile(sherry_reaction_palette_1);
DeclareModelFile(azalea_reaction_palette_1);
DeclareModelFile(joe_reaction_palette_1);
DeclareModelFile(plum_palette_2);
DeclareModelFile(charlie_palette_2);
DeclareModelFile(maple_palette_2);
DeclareModelFile(baby_mario_palette_2);
DeclareModelFile(luigi_palette_2);
DeclareModelFile(peach_palette_2);
DeclareModelFile(sonny_palette_2);
DeclareModelFile(wario_palette_2);
DeclareModelFile(harry_palette_2);
DeclareModelFile(mario_palette_2);
DeclareModelFile(yoshi_palette_2);
DeclareModelFile(donkey_kong_palette_2);
DeclareModelFile(bowser_palette_2);
DeclareModelFile(metal_mario_palette_2);
DeclareModelFile(kid_palette_2);
DeclareModelFile(sherry_palette_2);
DeclareModelFile(azalea_palette_2);
DeclareModelFile(joe_palette_2);
DeclareModelFile(plum_reaction_palette_2);
DeclareModelFile(charlie_reaction_palette_2);
DeclareModelFile(maple_reaction_palette_2);
DeclareModelFile(baby_mario_reaction_palette_2);
DeclareModelFile(luigi_reaction_palette_2);
DeclareModelFile(peach_reaction_palette_2);
DeclareModelFile(sonny_reaction_palette_2);
DeclareModelFile(wario_reaction_palette_2);
DeclareModelFile(harry_reaction_palette_2);
DeclareModelFile(mario_reaction_palette_2);
DeclareModelFile(yoshi_reaction_palette_2);
DeclareModelFile(donkey_kong_reaction_palette_2);
DeclareModelFile(bowser_reaction_palette_2);
DeclareModelFile(metal_mario_reaction_palette_2);
DeclareModelFile(kid_reaction_palette_2);
DeclareModelFile(sherry_reaction_palette_2);
DeclareModelFile(azalea_reaction_palette_2);
DeclareModelFile(joe_reaction_palette_2);
DeclareModelFile(plum_palette_3);
DeclareModelFile(charlie_palette_3);
DeclareModelFile(maple_palette_3);
DeclareModelFile(baby_mario_palette_3);
DeclareModelFile(luigi_palette_3);
DeclareModelFile(peach_palette_3);
DeclareModelFile(sonny_palette_3);
DeclareModelFile(wario_palette_3);
DeclareModelFile(harry_palette_3);
DeclareModelFile(mario_palette_3);
DeclareModelFile(yoshi_palette_3);
DeclareModelFile(donkey_kong_palette_3);
DeclareModelFile(bowser_palette_3);
DeclareModelFile(metal_mario_palette_3);
DeclareModelFile(kid_palette_3);
DeclareModelFile(sherry_palette_3);
DeclareModelFile(azalea_palette_3);
DeclareModelFile(joe_palette_3);
DeclareModelFile(plum_reaction_palette_3);
DeclareModelFile(charlie_reaction_palette_3);
DeclareModelFile(maple_reaction_palette_3);
DeclareModelFile(baby_mario_reaction_palette_3);
DeclareModelFile(luigi_reaction_palette_3);
DeclareModelFile(peach_reaction_palette_3);
DeclareModelFile(sonny_reaction_palette_3);
DeclareModelFile(wario_reaction_palette_3);
DeclareModelFile(harry_reaction_palette_3);
DeclareModelFile(mario_reaction_palette_3);
DeclareModelFile(yoshi_reaction_palette_3);
DeclareModelFile(donkey_kong_reaction_palette_3);
DeclareModelFile(bowser_reaction_palette_3);
DeclareModelFile(metal_mario_reaction_palette_3);
DeclareModelFile(kid_reaction_palette_3);
DeclareModelFile(sherry_reaction_palette_3);
DeclareModelFile(azalea_reaction_palette_3);
DeclareModelFile(joe_reaction_palette_3);
DeclareModelFile(opening_mario);
DeclareModelFile(opening_yoshi);
DeclareModelFile(opening_baby_mario);
DeclareModelFile(opening_donkey_kong);
DeclareModelFile(opening_plum);
DeclareModelFile(opening_peach);
DeclareModelFile(opening_luigi);
DeclareModelFile(opening_model_15);
DeclareModelFile(opening_toad);
DeclareModelFile(opening_model_17);

const CharacterModel g_CharacterModels[CHARACTER_MODEL_KIND_COUNT][CHARACTER_MODELS_PER_KIND] = {
    {
        {ModelRomBounds(plum), PLUM_MODEL_LAYOUT},
        {ModelRomBounds(charlie), CHARLIE_MODEL_LAYOUT},
        {ModelRomBounds(maple), MAPLE_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario), BABY_MARIO_MODEL_LAYOUT},
        {ModelRomBounds(luigi), LUIGI_MODEL_LAYOUT},
        {ModelRomBounds(peach), PEACH_MODEL_LAYOUT},
        {ModelRomBounds(sonny), SONNY_MODEL_LAYOUT},
        {ModelRomBounds(wario), WARIO_MODEL_LAYOUT},
        {ModelRomBounds(harry), HARRY_MODEL_LAYOUT},
        {ModelRomBounds(mario), MARIO_MODEL_LAYOUT},
        {ModelRomBounds(yoshi), YOSHI_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong), DONKEY_KONG_MODEL_LAYOUT},
        {ModelRomBounds(bowser), BOWSER_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario), METAL_MARIO_MODEL_LAYOUT},
        {ModelRomBounds(kid), KID_MODEL_LAYOUT},
        {ModelRomBounds(sherry), SHERRY_MODEL_LAYOUT},
        {ModelRomBounds(azalea), AZALEA_MODEL_LAYOUT},
        {ModelRomBounds(joe), JOE_MODEL_LAYOUT},
        {ModelRomBounds(plum_reaction), PLUM_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(charlie_reaction), CHARLIE_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(maple_reaction), MAPLE_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_reaction), BABY_MARIO_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(luigi_reaction), LUIGI_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(peach_reaction), PEACH_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(sonny_reaction), SONNY_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(wario_reaction), WARIO_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(harry_reaction), HARRY_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(mario_reaction), MARIO_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_reaction), YOSHI_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_reaction), DONKEY_KONG_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(bowser_reaction), BOWSER_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_reaction), METAL_MARIO_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(kid_reaction), KID_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(sherry_reaction), SHERRY_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(azalea_reaction), AZALEA_REACTION_MODEL_LAYOUT},
        {ModelRomBounds(joe_reaction), JOE_REACTION_MODEL_LAYOUT},
    },
    {
        {ModelRomBounds(plum_palette_1), PLUM_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(charlie_palette_1), CHARLIE_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(maple_palette_1), MAPLE_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_palette_1), BABY_MARIO_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(luigi_palette_1), LUIGI_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(peach_palette_1), PEACH_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(sonny_palette_1), SONNY_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(wario_palette_1), WARIO_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(harry_palette_1), HARRY_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(mario_palette_1), MARIO_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_palette_1), YOSHI_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_palette_1), DONKEY_KONG_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(bowser_palette_1), BOWSER_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_palette_1), METAL_MARIO_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(kid_palette_1), KID_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(sherry_palette_1), SHERRY_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(azalea_palette_1), AZALEA_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(joe_palette_1), JOE_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(plum_reaction_palette_1), PLUM_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(charlie_reaction_palette_1), CHARLIE_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(maple_reaction_palette_1), MAPLE_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_reaction_palette_1), BABY_MARIO_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(luigi_reaction_palette_1), LUIGI_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(peach_reaction_palette_1), PEACH_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(sonny_reaction_palette_1), SONNY_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(wario_reaction_palette_1), WARIO_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(harry_reaction_palette_1), HARRY_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(mario_reaction_palette_1), MARIO_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_reaction_palette_1), YOSHI_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_reaction_palette_1),
            DONKEY_KONG_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(bowser_reaction_palette_1), BOWSER_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_reaction_palette_1),
            METAL_MARIO_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(kid_reaction_palette_1), KID_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(sherry_reaction_palette_1), SHERRY_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(azalea_reaction_palette_1), AZALEA_REACTION_PALETTE_1_MODEL_LAYOUT},
        {ModelRomBounds(joe_reaction_palette_1), JOE_REACTION_PALETTE_1_MODEL_LAYOUT},
    },
    {
        {ModelRomBounds(plum_palette_2), PLUM_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(charlie_palette_2), CHARLIE_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(maple_palette_2), MAPLE_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_palette_2), BABY_MARIO_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(luigi_palette_2), LUIGI_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(peach_palette_2), PEACH_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(sonny_palette_2), SONNY_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(wario_palette_2), WARIO_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(harry_palette_2), HARRY_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(mario_palette_2), MARIO_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_palette_2), YOSHI_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_palette_2), DONKEY_KONG_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(bowser_palette_2), BOWSER_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_palette_2), METAL_MARIO_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(kid_palette_2), KID_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(sherry_palette_2), SHERRY_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(azalea_palette_2), AZALEA_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(joe_palette_2), JOE_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(plum_reaction_palette_2), PLUM_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(charlie_reaction_palette_2), CHARLIE_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(maple_reaction_palette_2), MAPLE_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_reaction_palette_2), BABY_MARIO_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(luigi_reaction_palette_2), LUIGI_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(peach_reaction_palette_2), PEACH_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(sonny_reaction_palette_2), SONNY_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(wario_reaction_palette_2), WARIO_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(harry_reaction_palette_2), HARRY_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(mario_reaction_palette_2), MARIO_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_reaction_palette_2), YOSHI_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_reaction_palette_2),
            DONKEY_KONG_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(bowser_reaction_palette_2), BOWSER_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_reaction_palette_2),
            METAL_MARIO_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(kid_reaction_palette_2), KID_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(sherry_reaction_palette_2), SHERRY_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(azalea_reaction_palette_2), AZALEA_REACTION_PALETTE_2_MODEL_LAYOUT},
        {ModelRomBounds(joe_reaction_palette_2), JOE_REACTION_PALETTE_2_MODEL_LAYOUT},
    },
    {
        {ModelRomBounds(plum_palette_3), PLUM_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(charlie_palette_3), CHARLIE_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(maple_palette_3), MAPLE_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_palette_3), BABY_MARIO_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(luigi_palette_3), LUIGI_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(peach_palette_3), PEACH_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(sonny_palette_3), SONNY_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(wario_palette_3), WARIO_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(harry_palette_3), HARRY_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(mario_palette_3), MARIO_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_palette_3), YOSHI_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_palette_3), DONKEY_KONG_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(bowser_palette_3), BOWSER_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_palette_3), METAL_MARIO_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(kid_palette_3), KID_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(sherry_palette_3), SHERRY_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(azalea_palette_3), AZALEA_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(joe_palette_3), JOE_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(plum_reaction_palette_3), PLUM_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(charlie_reaction_palette_3), CHARLIE_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(maple_reaction_palette_3), MAPLE_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(baby_mario_reaction_palette_3), BABY_MARIO_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(luigi_reaction_palette_3), LUIGI_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(peach_reaction_palette_3), PEACH_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(sonny_reaction_palette_3), SONNY_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(wario_reaction_palette_3), WARIO_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(harry_reaction_palette_3), HARRY_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(mario_reaction_palette_3), MARIO_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(yoshi_reaction_palette_3), YOSHI_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(donkey_kong_reaction_palette_3),
            DONKEY_KONG_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(bowser_reaction_palette_3), BOWSER_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(metal_mario_reaction_palette_3),
            METAL_MARIO_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(kid_reaction_palette_3), KID_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(sherry_reaction_palette_3), SHERRY_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(azalea_reaction_palette_3), AZALEA_REACTION_PALETTE_3_MODEL_LAYOUT},
        {ModelRomBounds(joe_reaction_palette_3), JOE_REACTION_PALETTE_3_MODEL_LAYOUT},
    },
};

// Chosen instead of g_CharacterModels for a model kind of
// CHARACTER_MODEL_KIND_COUNT or more
const CharacterModel g_OpeningCharacterModels[OPENING_CHARACTER_MODEL_COUNT] = {
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_mario),
        {
            0x020124B8,
            0x02000018,
            0x020053CC,
            0x02005348,
            0x02012670,
            0x02012730,
            0x020126E4,
            0x020126C0,
            0x020126F0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_mario),
        0x02012B10,
        0x02013B10,
    },
    {
        ModelRomBounds(opening_yoshi),
        {
            0x02007DF8,
            0x02000540,
            0x02001E84,
            0x02001E6C,
            0x02007F6C,
            0x02007FD4,
            0x02007FAC,
            0x02007FA8,
            0x02007FB4,
            0x02000528,
            0x0200052C,
            0x02000530,
            0x02000534,
            0x02000538,
            0x0200053C,
        },
        100,
        ModelRomBounds(opening_yoshi),
        0x02008318,
        0x02008B18,
    },
    {
        ModelRomBounds(opening_baby_mario),
        {
            0x02009088,
            0x02000018,
            0x020033BC,
            0x02003398,
            0x0200920C,
            0x02009288,
            0x0200924C,
            0x02009248,
            0x02009258,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        60,
        ModelRomBounds(opening_baby_mario),
        0x02009600,
        0x02009E00,
    },
    {
        ModelRomBounds(opening_donkey_kong),
        {
            0x020085C8,
            0x02000018,
            0x02003CF4,
            0x02003CDC,
            0x0200877C,
            0x020087F8,
            0x020087BC,
            0x020087B8,
            0x020087C0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        120,
        ModelRomBounds(opening_donkey_kong),
        0x02008BD8,
        0x020093D8,
    },
    {
        ModelRomBounds(opening_plum),
        {
            0x0200A888,
            0x02000018,
            0x02002FD4,
            0x02002F98,
            0x0200AA9C,
            0x0200AB00,
            0x0200AADC,
            0x0200AAD8,
            0x0200AAE0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        60,
        ModelRomBounds(opening_plum),
        0x0200AFE0,
        0x0200B7E0,
    },
    {
        ModelRomBounds(opening_peach),
        {
            0x0200C7C0,
            0x02000018,
            0x020024BC,
            0x02002450,
            0x0200C9DC,
            0x0200CA58,
            0x0200CA1C,
            0x0200CA18,
            0x0200CA20,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        60,
        ModelRomBounds(opening_peach),
        0x0200CF38,
        0x0200D738,
    },
    {
        ModelRomBounds(opening_luigi),
        {
            0x02011338,
            0x02000018,
            0x02006314,
            0x020062CC,
            0x020114EC,
            0x020115A0,
            0x0201154C,
            0x02011528,
            0x02011558,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        70,
        ModelRomBounds(opening_luigi),
        0x02011980,
        0x02012980,
    },
    {
        ModelRomBounds(opening_model_15),
        {
            0x02010050,
            0x02000018,
            0x02004100,
            0x02003880,
            0x02010218,
            0x02010288,
            0x02010250,
            0x02010240,
            0x02010258,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        100,
        ModelRomBounds(opening_model_15),
        0x02010668,
        0x02011668,
    },
    {
        ModelRomBounds(opening_toad),
        {
            0x02005340,
            0x02000018,
            0x020025F4,
            0x020025DC,
            0x0200548C,
            0x02005500,
            0x020054CC,
            0x020054C8,
            0x020054D0,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        60,
        ModelRomBounds(opening_toad),
        0x02005810,
        0x02006010,
    },
    {
        ModelRomBounds(opening_model_17),
        {
            0x0200D0A0,
            0x02000018,
            0x02004270,
            0x020041DC,
            0x0200D2A8,
            0x0200D34C,
            0x0200D308,
            0x0200D2E4,
            0x0200D314,
            0x02000000,
            0x02000004,
            0x02000008,
            0x0200000C,
            0x02000010,
            0x02000014,
        },
        100,
        ModelRomBounds(opening_model_17),
        0x0200D790,
        0x0200E790,
    },
};

// Builds the slot's joint matrices for g_CurrentAnimationFrame into
// g_CurrentJointMatrices. Each joint's translation, rotation and scale is
// interpolated between the keys around the frame, or held at the nearest key
// outside them or past g_CurrentAnimationEndFrame.
void AnimateCharacterJoints(u32 slotIndex) {
    // Nothing uses them, but the original's frame has the room for them
    Mat4 unusedMatrices[3] UNUSED;
    Mat4 translationMatrix;
    Mat4 jointMatrix;
    Mat4 scaleMatrix;
    Quaternion rotation;
    JointAnimation *joint;
    Mtx *matrix;
    f32 x;
    f32 y;
    f32 z;
    f32 progress;
    f32 remaining;
    f32 dot;
    Radians angle;
    f32 spanWeight;
    f32 spanFactor;
    f32 nextFactor;
    f32 sinAngle;
    Boolean16 isInterpolated;
    s32 keyIndex;
    s16 spanIndex;
    s16 nextSpanIndex;

    joint = (JointAnimation *)GetUsedCharacterSlot(slotIndex)
                ->model->segmentAddresses[CHARACTER_MODEL_JOINTS];
    matrix = g_CurrentJointMatrices;
    if (joint->translationKeyCount == END_OF_JOINT_ANIMATIONS) {
        return;
    }

    do {
        isInterpolated = TRUE;
        if (joint->translationSpan.endFrame < g_CurrentAnimationFrame ||
            g_CurrentAnimationFrame < joint->translationSpan.startFrame) {
            keyIndex = 1;
            if (g_CurrentAnimationFrame < joint->translationFrames[0]) {
                isInterpolated = FALSE;
                x = joint->translations[0][AXIS_X];
                y = joint->translations[0][AXIS_Y];
                z = joint->translations[0][AXIS_Z];
            }
            else {
                for (; keyIndex < joint->translationKeyCount; keyIndex++) {
                    if (g_CurrentAnimationFrame < joint->translationFrames[keyIndex]) {
                        joint->translationSpan.startKeyIndex = keyIndex - 1;
                        joint->translationSpan.startFrame = joint->translationFrames[keyIndex - 1];
                        joint->translationSpan.endFrame = joint->translationFrames[keyIndex];
                        break;
                    }
                }

                if (keyIndex == joint->translationKeyCount) {
                    keyIndex--;
                    isInterpolated = FALSE;
                    x = joint->translations[keyIndex][AXIS_X];
                    y = joint->translations[keyIndex][AXIS_Y];
                    z = joint->translations[keyIndex][AXIS_Z];
                }
            }
        }

        if (isInterpolated) {
            if (g_CurrentAnimationEndFrame < joint->translationSpan.endFrame) {
                x = joint->translations[joint->translationSpan.startKeyIndex][AXIS_X];
                y = joint->translations[joint->translationSpan.startKeyIndex][AXIS_Y];
                z = joint->translations[joint->translationSpan.startKeyIndex][AXIS_Z];
            }
            else {
                progress = (g_CurrentAnimationFrame - joint->translationSpan.startFrame) /
                    (joint->translationSpan.endFrame - joint->translationSpan.startFrame);
                spanIndex = joint->translationSpan.startKeyIndex;
                nextSpanIndex = spanIndex + 1;
                remaining = 1.0 - progress;
                x = (remaining * joint->translations[spanIndex][AXIS_X]) +
                    (progress * joint->translations[nextSpanIndex][AXIS_X]);
                y = (remaining * joint->translations[spanIndex][AXIS_Y]) +
                    (progress * joint->translations[nextSpanIndex][AXIS_Y]);
                z = (remaining * joint->translations[spanIndex][AXIS_Z]) +
                    (progress * joint->translations[nextSpanIndex][AXIS_Z]);
            }
        }

        guTranslateF(translationMatrix, x, y, z);

        isInterpolated = TRUE;
        if (joint->rotationSpan.endFrame < g_CurrentAnimationFrame ||
            g_CurrentAnimationFrame < joint->rotationSpan.startFrame) {
            keyIndex = 1;
            if (g_CurrentAnimationFrame < joint->rotationFrames[0]) {
                isInterpolated = FALSE;
                rotation[AXIS_X] = joint->rotations[0][AXIS_X] * QUATERNION_SCALE;
                rotation[AXIS_Y] = joint->rotations[0][AXIS_Y] * QUATERNION_SCALE;
                rotation[AXIS_Z] = joint->rotations[0][AXIS_Z] * QUATERNION_SCALE;
                rotation[QUATERNION_W] = joint->rotations[0][QUATERNION_W] * QUATERNION_SCALE;
            }
            else {
                for (; keyIndex < joint->rotationKeyCount; keyIndex++) {
                    if (g_CurrentAnimationFrame < joint->rotationFrames[keyIndex]) {
                        joint->rotationSpan.startKeyIndex = keyIndex - 1;
                        joint->rotationSpan.startFrame = joint->rotationFrames[keyIndex - 1];
                        joint->rotationSpan.endFrame = joint->rotationFrames[keyIndex];
                        break;
                    }
                }

                if (keyIndex == joint->rotationKeyCount) {
                    keyIndex--;
                    isInterpolated = FALSE;
                    rotation[AXIS_X] = joint->rotations[keyIndex][AXIS_X] * QUATERNION_SCALE;
                    rotation[AXIS_Y] = joint->rotations[keyIndex][AXIS_Y] * QUATERNION_SCALE;
                    rotation[AXIS_Z] = joint->rotations[keyIndex][AXIS_Z] * QUATERNION_SCALE;
                    rotation[QUATERNION_W] =
                        joint->rotations[keyIndex][QUATERNION_W] * QUATERNION_SCALE;
                }
            }
        }

        if (isInterpolated) {
            if (g_CurrentAnimationEndFrame < joint->rotationSpan.endFrame) {
                rotation[AXIS_X] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][0] * QUATERNION_SCALE;
                rotation[AXIS_Y] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][1] * QUATERNION_SCALE;
                rotation[AXIS_Z] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][2] * QUATERNION_SCALE;
                rotation[QUATERNION_W] =
                    joint->rotations[joint->rotationSpan.startKeyIndex][3] * QUATERNION_SCALE;
            }
            else {
                // Its own, as the original allocates it within the branch
                f32 rotationProgress;
                // A variable of its own to match: with rotationProgress scaled in
                // place, 35 words differ from the original's (27 in shape)
                Radians coveredAngle;

                spanIndex = joint->rotationSpan.startKeyIndex;
                nextSpanIndex = spanIndex + 1;
                dot = ((joint->rotations[spanIndex][AXIS_X] * QUATERNION_PRODUCT_SCALE) *
                          joint->rotations[nextSpanIndex][AXIS_X]) +
                    ((joint->rotations[spanIndex][AXIS_Y] * QUATERNION_PRODUCT_SCALE) *
                        joint->rotations[nextSpanIndex][AXIS_Y]) +
                    ((joint->rotations[spanIndex][AXIS_Z] * QUATERNION_PRODUCT_SCALE) *
                        joint->rotations[nextSpanIndex][AXIS_Z]) +
                    ((joint->rotations[spanIndex][QUATERNION_W] * QUATERNION_PRODUCT_SCALE) *
                        joint->rotations[nextSpanIndex][QUATERNION_W]);
                rotationProgress = (g_CurrentAnimationFrame - joint->rotationSpan.startFrame) /
                    (joint->rotationSpan.endFrame - joint->rotationSpan.startFrame);
                // Nearly equal rotations are blended linearly, where the sines below
                // would be near zero. progress holds the next span's weight from
                // here to match: with a variable of its own, 20 words differ from
                // the original's
                if (1.0 - dot < SLERP_THRESHOLD) {
                    progress = rotationProgress;
                    spanWeight = 1.0 - progress;
                }
                else {
                    angle = acosf(dot);
                    coveredAngle = rotationProgress * angle;
                    sinAngle = sinf(angle);
                    progress = sinf(coveredAngle) / sinAngle;
                    spanWeight = sinf(angle - coveredAngle) / sinAngle;
                }

                spanFactor = spanWeight * QUATERNION_SCALE;
                nextFactor = progress * QUATERNION_SCALE;
                rotation[AXIS_X] = (spanFactor * joint->rotations[spanIndex][AXIS_X]) +
                    (nextFactor * joint->rotations[nextSpanIndex][AXIS_X]);
                rotation[AXIS_Y] = (spanFactor * joint->rotations[spanIndex][AXIS_Y]) +
                    (nextFactor * joint->rotations[nextSpanIndex][AXIS_Y]);
                rotation[AXIS_Z] = (spanFactor * joint->rotations[spanIndex][AXIS_Z]) +
                    (nextFactor * joint->rotations[nextSpanIndex][AXIS_Z]);
                rotation[QUATERNION_W] = (spanFactor * joint->rotations[spanIndex][QUATERNION_W]) +
                    (nextFactor * joint->rotations[nextSpanIndex][QUATERNION_W]);
            }
        }

        QuaternionToMatrix(rotation, jointMatrix);

        isInterpolated = TRUE;
        if (joint->scaleSpan.endFrame < g_CurrentAnimationFrame ||
            g_CurrentAnimationFrame < joint->scaleSpan.startFrame) {
            keyIndex = 1;
            if (g_CurrentAnimationFrame < joint->scaleFrames[0]) {
                isInterpolated = FALSE;
                x = joint->scales[0][AXIS_X];
                y = joint->scales[0][AXIS_Y];
                z = joint->scales[0][AXIS_Z];
            }
            else {
                for (; keyIndex < joint->scaleKeyCount; keyIndex++) {
                    if (g_CurrentAnimationFrame < joint->scaleFrames[keyIndex]) {
                        joint->scaleSpan.startKeyIndex = keyIndex - 1;
                        joint->scaleSpan.startFrame = joint->scaleFrames[keyIndex - 1];
                        joint->scaleSpan.endFrame = joint->scaleFrames[keyIndex];
                        break;
                    }
                }

                if (keyIndex == joint->scaleKeyCount) {
                    keyIndex--;
                    isInterpolated = FALSE;
                    x = joint->scales[keyIndex][AXIS_X];
                    y = joint->scales[keyIndex][AXIS_Y];
                    z = joint->scales[keyIndex][AXIS_Z];
                }
            }
        }

        if (isInterpolated) {
            if (g_CurrentAnimationEndFrame < joint->scaleSpan.endFrame) {
                x = joint->scales[joint->scaleSpan.startKeyIndex][AXIS_X];
                y = joint->scales[joint->scaleSpan.startKeyIndex][AXIS_Y];
                z = joint->scales[joint->scaleSpan.startKeyIndex][AXIS_Z];
            }
            else {
                progress = (g_CurrentAnimationFrame - joint->scaleSpan.startFrame) /
                    (joint->scaleSpan.endFrame - joint->scaleSpan.startFrame);
                spanIndex = joint->scaleSpan.startKeyIndex;
                nextSpanIndex = spanIndex + 1;
                remaining = 1.0 - progress;
                x = (remaining * joint->scales[spanIndex][AXIS_X]) +
                    (progress * joint->scales[nextSpanIndex][AXIS_X]);
                y = (remaining * joint->scales[spanIndex][AXIS_Y]) +
                    (progress * joint->scales[nextSpanIndex][AXIS_Y]);
                z = (remaining * joint->scales[spanIndex][AXIS_Z]) +
                    (progress * joint->scales[nextSpanIndex][AXIS_Z]);
            }
        }

        guScaleF(scaleMatrix, x, y, z);
        guMtxCatF(scaleMatrix, jointMatrix, jointMatrix);
        guMtxCatF(jointMatrix, translationMatrix, jointMatrix);
        guMtxF2L(jointMatrix, matrix++);
        joint++;
    }
    while (joint->translationKeyCount != END_OF_JOINT_ANIMATIONS);
}

NO_RESULT FreeCharacterSlot(u32 slotIndex) {
    osSyncPrintf("D PC[%d]\n", slotIndex);
    if (slotIndex < CHARACTER_SLOT_COUNT) {
        g_CharacterSlots[slotIndex].modelIndex = NO_CHARACTER_MODEL;
    }
}

// A slot stays busy for two frames after it was last drawn, while the display
// list may still use it
Boolean IsCharacterSlotBusy(u32 slotIndex) {
    Boolean isBusy;

    if (slotIndex >= CHARACTER_SLOT_COUNT) {
        return TRUE;
    }

    isBusy = FALSE;
    if (g_CharacterSlots[slotIndex].modelIndex != NO_CHARACTER_MODEL ||
        g_FrameCount - g_CharacterSlots[slotIndex].lastDrawnFrame < BUSY_FRAME_COUNT) {
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

// Returns the slot, or NULL if it is out of range or empty
CharacterSlot *GetUsedCharacterSlot(u32 slotIndex) {
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

// Points the model's chosen display list at one of its display list choices.
// The slot's model buffer must already be mapped. Returns 0, or -1 if the slot
// is empty or the model has no such choice.
Outcome SelectCharacterDisplayList(u32 slotIndex, s32 choiceIndex) {
    CharacterSlot *slot;
    s32 checkedIndex;

    slot = GetUsedCharacterSlot(slotIndex);
    if (slot != NULL) {
        checkedIndex = 0;
        if (choiceIndex == NO_DISPLAY_LIST_CHOICE) {
            ModelChosenDisplayList(slot->model)->words.w0 = G_ENDDL << 24;
            ModelChosenDisplayList(slot->model)->words.w1 = 0;
            return OUTCOME_OK;
        }

        for (; checkedIndex < choiceIndex; checkedIndex++) {
            if (ModelDisplayListChoices(slot->model)[checkedIndex] == END_OF_DISPLAY_LIST_CHOICES) {
                return OUTCOME_FAILED;
            }
        }

        if (ModelDisplayListChoices(slot->model)[checkedIndex] == END_OF_DISPLAY_LIST_CHOICES) {
            return OUTCOME_FAILED;
        }

        ModelChosenDisplayList(slot->model)->words.w0 = G_DL << 24;
        ModelChosenDisplayList(slot->model)->words.w1 =
            ModelDisplayListChoices(slot->model)[checkedIndex];
        return OUTCOME_OK;
    }

    return OUTCOME_FAILED;
}

// Returns the club's CLUB_KIND, or -1 if the slot is empty
ClubCategory GetCharacterClubCategory(u32 slotIndex, ClubIndex clubIndex) {
    CharacterSlot *slot;
    ClubCategory category;

    category = CLUB_CATEGORY_NONE;
    slot = GetUsedCharacterSlot(slotIndex);
    if (slot != NULL) {
        g_CharacterModelIndex = slot->modelIndex;
        switch (GetClub(clubIndex)->category) {
            case CLUB_CATEGORY_WOOD:
                category = CLUB_CATEGORY_WOOD;
                break;
            case CLUB_CATEGORY_IRON:
                category = CLUB_CATEGORY_IRON;
                break;
            case CLUB_CATEGORY_WEDGE:
                category = CLUB_CATEGORY_WEDGE;
                break;
            case CLUB_CATEGORY_PUTTER:
                category = CLUB_CATEGORY_PUTTER;
                break;
            default:
                category = CLUB_CATEGORY_NONE;
                break;
        }
    }

    return category;
}

// Finds the point's position on the model, before the slot's own transform.
// Returns 0, or 1 if the model has no such point, or -1 if the slot is empty.
// Leaves the position zero if not found.
PointLookup GetCharacterPointModelPosition(u32 slotIndex, s32 pointId, f32 *position) {
    Mtx pointMatrix;
    Mtx offsetMatrix;
    CharacterSlot *slot;
    CharacterPoint *point;
    s16 *jointPath;

    slot = GetUsedCharacterSlot(slotIndex);
    if (slot != NULL) {
        SetMultiTlb(
            0,
            TLB_PAGE_MODE_4K,
            CHARACTER_MODEL_PAGE_COUNT,
            CHARACTER_MODEL_ADDRESS,
            osVirtualToPhysical(slot->modelBuffer)
        );
        point = (CharacterPoint *)slot->model->segmentAddresses[CHARACTER_MODEL_POINTS];
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
            guTranslate(&offsetMatrix, point->offset[0], point->offset[1], point->offset[2]);
            guMtxCatL(&offsetMatrix, &pointMatrix, &pointMatrix);
            guMtxXFML(
                &pointMatrix,
                0.0f,
                0.0f,
                0.0f,
                &position[AXIS_X],
                &position[AXIS_Y],
                &position[AXIS_Z]
            );
            position[AXIS_X] *= MODEL_TO_WORLD_SCALE;
            position[AXIS_Y] *= MODEL_TO_WORLD_SCALE;
            position[AXIS_Z] *= MODEL_TO_WORLD_SCALE;
            return POINT_LOOKUP_FOUND;
        }

        // The chained assignment is there to match: three separate stores, in
        // either order, in both places give 17 words differing, 13 in shape
        position[AXIS_X] = position[AXIS_Y] = position[AXIS_Z] = 0.0f;
        return POINT_LOOKUP_NO_POINT;
    }

    // The chained assignment is there to match: three separate stores, in
    // either order, in both places give 17 words differing, 13 in shape
    position[AXIS_X] = position[AXIS_Y] = position[AXIS_Z] = 0.0f;
    return POINT_LOOKUP_NO_SLOT;
}

void GetCharacterSlotMatrix(CharacterSlot *slot, Mat4 matrix) {
    Mat4 translation;
    Mat4 transform;

    guTranslateF(
        translation,
        slot->position[AXIS_X] * SLOT_POSITION_SCALE,
        slot->position[AXIS_Y] * SLOT_POSITION_SCALE,
        slot->position[AXIS_Z] * SLOT_POSITION_SCALE
    );
    guRotateRPYF(
        transform,
        (s32)(ROLL_ORIGIN_DEGREES - (s32)(slot->rotation[AXIS_X] * RADIANS_TO_DEGREES)),
        (s32)(slot->rotation[AXIS_Y] * RADIANS_TO_DEGREES),
        (s32)(slot->rotation[AXIS_Z] * RADIANS_TO_DEGREES)
    );
    guMtxCatF(transform, translation, translation);
    guScaleF(transform, slot->scale[AXIS_X], slot->scale[AXIS_Y], slot->scale[AXIS_Z]);
    guMtxCatF(transform, translation, matrix);
}

// Like GetCharacterPointModelPosition, but in the world
PointLookup GetCharacterPointPosition(u32 slotIndex, s32 pointId, f32 *position) {
    Mat4 matrix;
    Vec3 modelPosition;
    PointLookup result;

    result = GetCharacterPointModelPosition(slotIndex, pointId, modelPosition);
    if (result < POINT_LOOKUP_FOUND) {
        // The chained assignment is there to match: three separate stores, in
        // either order, give 8 words differing, all in shape
        position[AXIS_X] = position[AXIS_Y] = position[AXIS_Z] = 0.0f;
    }
    else {
        GetCharacterSlotMatrix(GetUsedCharacterSlot(slotIndex), matrix);
        guMtxXFMF(
            matrix,
            modelPosition[AXIS_X],
            modelPosition[AXIS_Y],
            modelPosition[AXIS_Z],
            &position[AXIS_X],
            &position[AXIS_Y],
            &position[AXIS_Z]
        );
    }

    return result;
}

// The model position of the point the character's shadow lies under
void GetCharacterShadowPoint(u32 slotIndex, f32 *position) {
    GetCharacterPointModelPosition(slotIndex, CHARACTER_POINT_SHADOW, position);
}
