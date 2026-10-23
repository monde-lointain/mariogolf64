#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <ultra64.h>

/* Routines whose defining file is not decompiled yet */

/* NuSystem */
void nuPiReadRom(u32 romAddr, void *buffer, u32 size);

/* Main segment */
void RunBootHook(void);
void InitAssetFiles(void);
void InitRumblePaks(void);
void StopAllRumble(void);
void ResetCharacterSlots(void);
s32 GetCharacterBufferSize(void);
void SetCharacterBuffer(s32 slotIndex, u8 *buffer);
void InitTerrainTextures(void);
void InitExpansionPakFeatures(void);
void InitEffectTable(void);
void StartGame(void);
void UpdateGame(void);
s32 DrawGame(void);
void DrawResetFade(void);
void UpdateOverlays(void);

/* The development debugger's entry, outside the game's code */
void EnterDevelopmentDebugger(s32 argument);
void LoadMenuResources(void);
void FreeMenuResources(void);
void func_80080DCC(void);
void func_80080E14(void);
/* Whether the view mode that the B button picks in debug builds is on */
s32 IsDebugBButtonView(void);

/* ovl_00 */
void func_801FD624_ovl_00(void);

/* ovl_05 */
void func_801F5018_ovl_05(void);
void func_801F5100_ovl_05(void);
void func_802033B0_ovl_05(void);

/* ovl_06 */
void LoadTitleScreen(void);
void UnloadTitleScreen(void);

/* ovl_07 */
void LoadBootLogos(void);

/* ovl_13 */
void func_801F4B20_ovl_13(void);
void func_801F4E70_ovl_13(void);

/* ovl_14 */
void LoadStaffRoll(void);
void UnloadStaffRoll(void);

/* RCP setup, in the file at 0xDCE0 */
#define BUFFER_CLEAR_DEPTH_ONLY 0 /* Clears the depth buffer and keeps the frame */
#define BUFFER_CLEAR_BY_LINE 1    /* Also fills the frame black, a line at a time */
#define BUFFER_CLEAR_WHOLE 2      /* Also fills the frame black at once */
void AppendRcpInit(s32 viewportIndex, Gfx **displayListHead);
void AppendBufferClear(s32 clearMode, Gfx **displayListHead);
void InitRcp(s32 viewportIndex);
void SetFillColor(s32 red, s32 green, s32 blue);
void FillScreen(void);
void ClearBuffers(s32 clearMode);

/*
 * Records the projection, viewing matrix and viewport, so the CPU can
 * project points as the RSP does. A NULL argument keeps the current one.
 */

/*
 * Returns the height of the ground under a point. Coordinates are in
 * 1/1024ths of a unit.
 */
s32 GetGroundHeight(s32 x, s32 z);

#define TRIANGLE_CORNER_COUNT 3

/* A triangle as GetTrianglePlane takes it: each axis's coordinates, by corner */
typedef struct PlaneTriangle {
    /* 0x00 */ s32 unk0;
    /* 0x04 */ s16 corners[AXIS_COUNT][TRIANGLE_CORNER_COUNT];
} PlaneTriangle; /* size = 0x18 */

/* The plane through a triangle: normal . point + distance = 0, with the normal's y at least 0 */
typedef struct TrianglePlane {
    /* 0x00 */ f32 normal[AXIS_COUNT];
    /* 0x0C */ f32 distance;
    /* 0x10 */ f32 unk10; /* Always 1 */
} TrianglePlane; /* size = 0x14 */

/* Returns 0, or -1 for a triangle without area, whose plane is then level */
s32 GetTrianglePlane(PlaneTriangle *triangle, TrianglePlane *plane);
/*
 * Finds the nearest hole triangle that a path hits. It sets the triangle,
 * whose unk0 then holds the HOLE_TRIANGLE_KIND_*, and the fraction along
 * the path, and returns whether one is hit.
 */
s32 FindHoleTriangleHit(s32 startX, s32 startY, s32 startZ, s32 endX, s32 endY, s32 endZ, PlaneTriangle *hit,
                        f32 *fraction);
/* Sets the fog factor and fog color */
void func_800625FC(Gfx **displayListHead);

/* Set a character's position, rotation and scale for a shot */
void func_800453E0(Vec3 position, Vec3 rotation, Vec3 scale);
void PlaceAtReactionSpot(Vec3 position, Vec3 rotation, Vec3 scale);

/* Sets the two lights and a third when that is enabled, returning whether it set them */
s32 AddReactionLight(Gfx **displayListHead, Lights2 *lights);

/* Draws the textures the course model reads through RSP segments */
void BuildDynamicTextures(Gfx **displayListHead);

void func_8004CDA0(void);
void func_8007B054(void);
void func_80078D94(void);
void func_8008E1B4(void);
void func_800605EC(void);

/* Scene modules' update routines, in overlays */
void func_801F4A40(void);
void func_801F4C78(void);
void UpdateTitleScreen(void);
void UpdateBootLogos(void);
void func_801F5F1C(void);
void func_801F6788(void);
void UpdateStaffRoll(void);

void func_80081284(void);

#endif
