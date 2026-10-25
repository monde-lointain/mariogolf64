#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "burst.h"
#include "camera.h"
#include "character.h"
#include "course.h"
#include "effects.h"
#include "functions.h"
#include "heap.h"
#include "libc.h"
#include "pin.h"
#include "round_hud.h"
#include "screen.h"
#include "scene.h"
#include "sound.h"
#include "variables.h"

/* sSceneryState values */
#define SCENERY_UNLOADED 0
#define SCENERY_BILLBOARDS_READY 1 /* The billboard heap is set up */
#define SCENERY_LOADED 2           /* The images of the hole's scenery are loaded too */

/* Bytes of the billboard manager's heap */
#define BILLBOARD_HEAP_SIZE 0x7118

/* Bytes of each buffer the scenery images come with */
#define SCENERY_BUFFER_A_SIZE 0x128
#define SCENERY_BUFFER_B_SIZE 0x1208

/* Images of the round's scenery, effects and HUD, named after their asset ids until their users are decompiled */
#define ASSET_ROUND_IMAGE_541 0x541
#define ASSET_ROUND_IMAGE_5A4 0x5A4
#define ASSET_ROUND_IMAGE_5E3 0x5E3
#define ASSET_ROUND_IMAGE_972 0x972
#define ASSET_ROUND_IMAGE_93C 0x93C
#define ASSET_ROUND_IMAGE_4DB 0x4DB
#define ASSET_SHADOW_TEXTURE 0x4C3
#define ASSET_ROUND_IMAGE_4E3 0x4E3
#define ASSET_ROUND_IMAGE_648 0x648
#define ASSET_ROUND_IMAGE_563 0x563
#define ASSET_ROUND_IMAGE_564 0x564
#define ASSET_ROUND_IMAGE_562 0x562
#define ASSET_ROUND_IMAGE_57C 0x57C
#define ASSET_ROUND_IMAGE_97B 0x97B
#define ASSET_ROUND_IMAGE_725 0x725
#define ASSET_ROUND_IMAGE_568 0x568
#define ASSET_ROUND_IMAGE_56B 0x56B
#define ASSET_ROUND_IMAGE_56A 0x56A
#define ASSET_ROUND_IMAGE_569 0x569
#define ASSET_ROUND_IMAGE_723 0x723
#define ASSET_ROUND_IMAGE_724 0x724

/* The sound of the rain, played at full volume when a hole starts in the rain */
#define RAIN_SOUND 0
#define RAIN_SOUND_START_VOLUME 100

/* The image of the hole's par, one per par from 3 to 5 */
#define ASSET_PAR_THREE_IMAGE 0x565
#define ASSET_PAR_FOUR_IMAGE 0x566
#define ASSET_PAR_FIVE_IMAGE 0x567

/* The first asset of each course record's hole images, one per hole */
extern u16 sHoleImageAssets[COURSE_RECORD_COUNT];
extern void *sHoleImage;
extern void *sParImage;

/* Set while the overlay of the course selection is loaded for the hole */
extern s32 sIsCourseSelectLoaded;
extern f32 g_RainSoundVolume;

/*
 * The flag of the wind indicator is a grid of vertices that the wind waves.
 * Its front and back sides each have one grid per matrix buffer; the back's
 * texture runs the other way.
 */
#define WIND_FLAG_COLUMN_COUNT 6
#define WIND_FLAG_ROW_COUNT 5
#define WIND_FLAG_VERTEX_COUNT (WIND_FLAG_COLUMN_COUNT * WIND_FLAG_ROW_COUNT)
#define WIND_FLAG_SIDE_COUNT 2
#define MATRIX_BUFFER_COUNT 2
#define WIND_FLAG_CELL_SIZE 160
#define WIND_FLAG_TEXEL_COLUMN_STEP 0x266 /* In s10.5 texture coordinates */
#define WIND_FLAG_TEXEL_ROW_STEP 0x2A0
#define WIND_FLAG_BACK_TEXEL_START 0xC00
#define WIND_FLAG_START_SHADE 0xB7

/* The near and far planes of the screen's orthographic projection */
#define SCREEN_ORTHO_NEAR 1.0f
#define SCREEN_ORTHO_FAR 1000.0f

extern Vtx sWindFlagVertices[WIND_FLAG_SIDE_COUNT][MATRIX_BUFFER_COUNT][WIND_FLAG_VERTEX_COUNT];
extern u8 sWindFlagShades[WIND_FLAG_VERTEX_COUNT];
extern s32 D_800C5478;
extern Mtx D_80105278;
extern u16 g_ScreenEffectPerspNorm;
extern f32 D_801B7F38[2];
extern f32 D_801B7F40[2];
extern f32 D_801B7F48[2];
extern f32 D_801B7F50[2];
extern f32 D_801B7F58[2];

/*
 * A solid arrow: a triangular head on a rectangular shaft, from -160 to 160
 * along y, with a face at each z of +-40, and a larger outline around it.
 * The body's texture runs along y, a texel row every
 * ARROW_UNITS_PER_TEXEL_ROW units.
 */
#define ARROW_COUNT 2
#define ARROW_VERTEX_COUNT 14
#define ARROW_UNITS_PER_TEXEL_ROW 10
#define ARROW_CENTER_TEXEL_ROW 16 /* The texel row at y = 0 */
#define TEXEL_COORDINATE_SHIFT 5 /* Texture coordinates are in s10.5 fixed point */

typedef struct ArrowShape {
    /* 0x000 */ Vtx body[ARROW_VERTEX_COUNT];
    /* 0x0E0 */ Vtx outline[ARROW_VERTEX_COUNT];
} ArrowShape; /* size = 0x1C0 */

extern ArrowShape sArrows[ARROW_COUNT];
extern s32 g_MatrixBufferIndex;

extern void *sSceneryBillboardHeap;
extern void *D_801B55DC;
extern void *D_800E2188;
extern void *D_80132360;
extern void *D_800FED0C;
extern void *D_800FF4F0;
extern void *D_8012D428;
extern void *g_ShadowTexture;
extern s32 D_800C59D8;
extern void *D_800E2140;
extern void *D_800E2144[2];
extern s32 D_800C59DC;
extern void *D_800E2138;
extern void *D_800E2158;
extern void *D_800E214C[3];
extern void *D_800E2184;
extern void *D_800E2164;
extern void *D_800E2168;
extern void *D_800E216C;
extern void *D_800E2170;
extern void *D_800E2174;
extern void *D_800E217C;
extern void *D_800E2178;
extern void *D_80105214;
extern s32 sSceneryState;
extern u8 D_801061B8[0x8C];

void func_80216B74(s32);
void func_8008E1B4(void);
void LoadCourseSelect(s32);
void FreeCourseSelect(void);
void func_801FD624(void);
void func_8020D6F0(void);
void func_80216EDC(void);
void func_8020E740(void);
void func_8021E5F4(void);
void func_8020D784(void);
void func_80216F8C(void);
void func_8020E78C(void);
void func_8021E6B8(void);

/* Sets up what the hole's drawing needs before its first frame */
void PrepareHoleDisplay(void) {
    s32 i;

    g_MatrixBufferIndex = 0;
    for (i = 0; i != ARROW_VERTEX_COUNT; i++) {
        sArrows[0].body[i].v.tc[1] =
            (sArrows[0].body[i].v.ob[1] / ARROW_UNITS_PER_TEXEL_ROW + ARROW_CENTER_TEXEL_ROW) << TEXEL_COORDINATE_SHIFT;
        sArrows[1].body[i].v.tc[1] =
            (sArrows[1].body[i].v.ob[1] / ARROW_UNITS_PER_TEXEL_ROW + ARROW_CENTER_TEXEL_ROW) << TEXEL_COORDINATE_SHIFT;
    }
    BuildPinShadow();
    func_8008E1B4();
}

/* Lays out the wind flag and builds the projections the HUD draws with */
#ifdef NON_MATCHING
void InitRoundHud(void) {
    s32 grid;
    s32 row;
    s32 column;
    Vtx *vertex;

    D_800C5478 = 0;
    for (grid = 0; grid != WIND_FLAG_SIDE_COUNT * MATRIX_BUFFER_COUNT; grid++) {
        vertex = sWindFlagVertices[0][grid];
        for (row = 0; row != WIND_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != WIND_FLAG_COLUMN_COUNT; column++) {
                vertex->v.ob[0] = column * WIND_FLAG_CELL_SIZE;
                vertex->v.ob[1] = -row * WIND_FLAG_CELL_SIZE;
                vertex->v.ob[2] = 0;
                vertex->v.tc[0] = column * WIND_FLAG_TEXEL_COLUMN_STEP;
                vertex->v.tc[1] = row * WIND_FLAG_TEXEL_ROW_STEP;
                vertex->v.cn[0] = WIND_FLAG_START_SHADE;
                vertex->v.cn[1] = WIND_FLAG_START_SHADE;
                vertex->v.cn[2] = WIND_FLAG_START_SHADE;
                vertex->v.cn[3] = WIND_FLAG_START_SHADE;
                vertex++;
            }
        }
    }
    for (grid = 0; grid != MATRIX_BUFFER_COUNT; grid++) {
        vertex = sWindFlagVertices[1][grid];
        for (row = 0; row != WIND_FLAG_ROW_COUNT; row++) {
            for (column = 0; column != WIND_FLAG_COLUMN_COUNT; column++) {
                vertex->v.tc[0] = WIND_FLAG_BACK_TEXEL_START - column * WIND_FLAG_TEXEL_COLUMN_STEP;
                vertex++;
            }
        }
    }
    for (column = 0; column != WIND_FLAG_VERTEX_COUNT; column++) {
        sWindFlagShades[column] = WIND_FLAG_START_SHADE;
    }
    guOrtho(&g_ScreenOrthoMatrix, -SCREEN_WIDTH / 2, SCREEN_WIDTH / 2, -SCREEN_HEIGHT / 2, SCREEN_HEIGHT / 2,
            SCREEN_ORTHO_NEAR, SCREEN_ORTHO_FAR, 1.0f);
    guPerspective(&D_80105278, &g_ScreenEffectPerspNorm, 10.0f, 4.0f / 3.0f, 10.0f, 30000.0f, 1.0f);
    guPerspective(&g_ScreenEffectProjection, &g_ScreenEffectPerspNorm, 45.0f, 4.0f / 3.0f, 10.0f, 8000.0f, 1.0f);
    g_ViewedWindAngles[0] = 0.0f;
    D_801B7F38[0] = 0.0f;
    D_801B7F48[0] = 0.0f;
    D_801B7F50[0] = 0.0f;
    D_801B7F58[0] = 0.0f;
    D_801B7F40[0] = 0.0f;
    g_ViewedWindAngles[1] = 0.0f;
    D_801B7F38[1] = 0.0f;
    D_801B7F48[1] = 0.0f;
    D_801B7F50[1] = 0.0f;
    D_801B7F58[1] = 0.0f;
    D_801B7F40[1] = 0.0f;
    g_InsetView.animation = INSET_VIEW_STILL;
    g_InsetView.step = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/round_hud", InitRoundHud);
#endif

/* Loads the face of a player's character in the colours of the given palette */
void LoadPlayerFace(s32 playerIndex, s32 character, s32 paletteIndex) {
    EncodedAsset asset;
    AssetFile file;

    OpenEncodedAsset(g_CharacterImageAssets[character] + CHARACTER_FACE_IMAGE_OFFSET, &asset);
    ReadEncodedAsset(g_PlayerFaceImages[playerIndex], &asset);
    OpenAssetFile(g_CharacterPaletteAssets[character] + paletteIndex, &file);
    ReadAssetFile(g_PlayerFacePalettes[playerIndex], GetAssetFileSize(&file), &file);
}

/* Frees the previous hole's mode images and loads the scenery images if they are not loaded */
void EndHoleIntro(void) {
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        if (sIsCourseSelectLoaded == TRUE) {
            sIsCourseSelectLoaded = FALSE;
            FreeCourseSelect();
        }
    } else if (g_GameMode == GAME_MODE_UNK3) {
        if (sIsCourseSelectLoaded == TRUE) {
            sIsCourseSelectLoaded = FALSE;
            FreeCourseSelect();
        }
    } else {
        FreeHoleIntroImages();
    }
    if (sSceneryState == SCENERY_BILLBOARDS_READY) {
        LoadSceneryImages();
    }
}

/* Starts the hole's weather and loads the images of the hole and its par */
void StartHoleIntro(void) {
    EncodedAsset asset;

    EndWeather();
    StartWeather(WEATHER_RAIN);
    if (g_HoleConditions.isRaining == TRUE) {
        PlaySound(RAIN_SOUND, RAIN_SOUND_CHANNEL, RAIN_SOUND_START_VOLUME);
        g_RainSoundVolume = RAIN_SOUND_START_VOLUME;
    } else {
        StopSound(RAIN_SOUND_CHANNEL);
    }
    RevertBillboardManager();
    OpenEncodedAsset(sHoleImageAssets[GetCourseRecordIndex()] + GetHoleIndexInRecord(), &asset);
    ReadEncodedAsset(sHoleImage, &asset);
    switch ((s8)g_CourseRecords[GetCourseRecordIndex()].holes[GetHoleRecordIndex()].par) {
        case 3:
            OpenEncodedAsset(ASSET_PAR_THREE_IMAGE, &asset);
            break;
        case 4:
            OpenEncodedAsset(ASSET_PAR_FOUR_IMAGE, &asset);
            break;
        case 5:
            OpenEncodedAsset(ASSET_PAR_FIVE_IMAGE, &asset);
            break;
    }
    ReadEncodedAsset(sParImage, &asset);
    if (sSceneryState == SCENERY_LOADED) {
        FreeSceneryImages();
    }
    if (g_GameMode == GAME_MODE_RING_SHOT) {
        if (sIsCourseSelectLoaded == FALSE) {
            sIsCourseSelectLoaded = TRUE;
            LoadCourseSelect(0);
        }
    } else if (g_GameMode == GAME_MODE_UNK3) {
        if (sIsCourseSelectLoaded == FALSE) {
            sIsCourseSelectLoaded = TRUE;
            LoadCourseSelect(1);
        }
    } else {
        LoadHoleIntroImages();
    }
}

/* Loads the images the round's scenery, effects and HUD are drawn with */
void LoadSceneryImages(void) {
    EncodedAsset asset;

    sSceneryState = SCENERY_LOADED;
    func_801FD624();
    LoadEffectImages();
    func_8020D6F0();
    func_80216EDC();
    func_8020E740();
    func_8021E5F4();
    D_801B55DC = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_541, &asset));
    ReadEncodedAsset(D_801B55DC, &asset);
    D_800E2188 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_5A4, &asset));
    ReadEncodedAsset(D_800E2188, &asset);
    D_80132360 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_5E3, &asset));
    ReadEncodedAsset(D_80132360, &asset);
    D_800FED0C = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_972, &asset));
    ReadEncodedAsset(D_800FED0C, &asset);
    D_800FF4F0 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_93C, &asset));
    ReadEncodedAsset(D_800FF4F0, &asset);
    D_8012D428 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_4DB, &asset));
    ReadEncodedAsset(D_8012D428, &asset);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &asset));
    ReadEncodedAsset(g_ShadowTexture, &asset);
    D_800C59D8 = -1;
    D_800E2140 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_4E3, &asset));
    ReadEncodedAsset(D_800E2140, &asset);
    D_800E2144[0] = AllocMemory(SCENERY_BUFFER_A_SIZE);
    D_800E2144[1] = AllocMemory(SCENERY_BUFFER_A_SIZE);
    D_800C59DC = -1;
    D_800E2138 = AllocMemory(SCENERY_BUFFER_B_SIZE);
    D_800E2158 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_648, &asset));
    ReadEncodedAsset(D_800E2158, &asset);
    D_800E214C[0] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_563, &asset));
    ReadEncodedAsset(D_800E214C[0], &asset);
    D_800E214C[1] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_564, &asset));
    ReadEncodedAsset(D_800E214C[1], &asset);
    D_800E214C[2] = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_562, &asset));
    ReadEncodedAsset(D_800E214C[2], &asset);
    D_800E2184 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_57C, &asset));
    ReadEncodedAsset(D_800E2184, &asset);
    D_800E2164 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_97B, &asset));
    ReadEncodedAsset(D_800E2164, &asset);
    D_800E2168 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_725, &asset));
    ReadEncodedAsset(D_800E2168, &asset);
    D_800E216C = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_568, &asset));
    ReadEncodedAsset(D_800E216C, &asset);
    D_800E2170 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56B, &asset));
    ReadEncodedAsset(D_800E2170, &asset);
    D_800E2174 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_56A, &asset));
    ReadEncodedAsset(D_800E2174, &asset);
    D_800E217C = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_569, &asset));
    ReadEncodedAsset(D_800E217C, &asset);
    D_800E2178 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_723, &asset));
    ReadEncodedAsset(D_800E2178, &asset);
    D_80105214 = AllocMemory(OpenEncodedAsset(ASSET_ROUND_IMAGE_724, &asset));
    ReadEncodedAsset(D_80105214, &asset);
    LoadBursts(GetCourseRecordIndex());
}

/* Frees what LoadSceneryImages loaded */
void FreeSceneryImages(void) {
    if (sSceneryState == SCENERY_LOADED) {
        sSceneryState = SCENERY_BILLBOARDS_READY;
        FreeEffectImages();
        func_8020D784();
        func_80216F8C();
        func_8020E78C();
        func_8021E6B8();
        FreeMemory(&D_801B55DC);
        FreeMemory(&D_800E2188);
        FreeMemory(&D_80132360);
        FreeMemory(&D_800FED0C);
        FreeMemory(&D_800FF4F0);
        FreeMemory(&D_8012D428);
        FreeMemory(&g_ShadowTexture);
        FreeMemory(&D_800E2140);
        FreeMemory(&D_800E2144[0]);
        FreeMemory(&D_800E2144[1]);
        FreeMemory(&D_800E2138);
        FreeMemory(&D_800E2158);
        FreeMemory(&D_800E214C[0]);
        FreeMemory(&D_800E214C[1]);
        FreeMemory(&D_800E214C[2]);
        FreeMemory(&D_800E2184);
        FreeMemory(&D_800E2164);
        FreeMemory(&D_800E2168);
        FreeMemory(&D_800E216C);
        FreeMemory(&D_800E2170);
        FreeMemory(&D_800E2174);
        FreeMemory(&D_800E217C);
        FreeMemory(&D_800E2178);
        FreeMemory(&D_80105214);
        FreeBursts();
    }
}

/* Sets up the billboard manager's heap, once per round */
void InitScenery(void) {
    EncodedAsset unusedAsset; /* Left from code that loaded assets here */

    if (sSceneryState == SCENERY_UNLOADED) {
        sSceneryBillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
        InitBillboardManager(sSceneryBillboardHeap, BILLBOARD_HEAP_SIZE);
        sSceneryState = SCENERY_BILLBOARDS_READY;
    }
}

/* Frees everything the round's scenery holds, when its overlay is unloaded */
void FreeScenery(void) {
    if (sSceneryState != SCENERY_UNLOADED) {
        DestroyBillboardManager();
        FreeMemory(&sSceneryBillboardHeap);
        EndWeather();
        func_80216B74(1);
        FreeSceneryImages();
        sSceneryState = SCENERY_UNLOADED;
        memset(D_801061B8, 0, sizeof(D_801061B8));
    }
}

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80080E7C);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80081284);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80081550);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008156C);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80081C90);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80081D4C);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80081EF8);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_800824E4);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1B10);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80082534);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80083A48);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80083AC8);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_800842C0);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80084468);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80084EBC);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_800852A8);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008534C);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80085F98);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008658C);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008679C);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80086864);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_800874D8);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80087BAC);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80087BE4);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80087CB0);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_800880A0);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80088890);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80088A90);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1BB8);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1BC8);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80088BDC);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_80089094);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1C84);

INCLUDE_RODATA("asm/nonmatchings/round_hud", D_800D1C98);

INCLUDE_RODATA("asm/nonmatchings/round_hud", jtbl_800D1CA0);

INCLUDE_ASM("asm/nonmatchings/round_hud", LoadHoleIntroImages);

INCLUDE_ASM("asm/nonmatchings/round_hud", FreeHoleIntroImages);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008C6B0);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008CD30);

INCLUDE_ASM("asm/nonmatchings/round_hud", func_8008D0DC);
