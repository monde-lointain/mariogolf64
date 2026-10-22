#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <ultra64.h>

/* Routines whose defining file is not decompiled yet */

/* NuSystem */
void nuPiReadRom(u32 romAddr, void *buffer, u32 size);

/* Main segment */
void RunBootHook(void);
void InitHeapRegions(s32 hasExpansionPak);
void InitAssetFiles(void);
void InitSaveData(void);
void InitRumblePaks(void);
void StopAllRumble(void);
void InitAudio(void);
void UpdateAudio(void);
void FreeCharacterSlot(s32 slotIndex);
void FreeAllCharacterSlots(void);
void ResetCharacterSlots(void);
s32 GetCharacterBufferSize(void);
void SetCharacterBuffer(s32 slotIndex, u8 *buffer);
void InitTerrainTextures(void);
void InitExpansionPakFeatures(void);
void ResetLoaderThread(void);
void InitEffectTable(void);
void LoadCourseObjects(void);
void StartGame(void);
void UpdateGame(void);
void DrawGame(void);
void DrawResetFade(void);
void UpdateOverlays(void);

/* The development debugger's entry, outside the game's code */
void EnterDevelopmentDebugger(s32 argument);
void func_8005A580(void);
void func_8005ACF8(void);
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
void func_801F4C84_ovl_06(void);
void func_801F4D48_ovl_06(void);

/* ovl_07 */
void func_801F4A40_ovl_07(void);

/* ovl_13 */
void func_801F4B20_ovl_13(void);
void func_801F4E70_ovl_13(void);

/* ovl_14 */
void func_801F4B9C_ovl_14(void);
void func_801F4E54_ovl_14(void);

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
void SetCpuProjection(Mtx *projection, Mtx *viewing, Vp *viewport);

void BuildTransformMatrix(f32 matrix[4][4], f32 *rotation, f32 *translation, f32 *scale);
void ConvertMatrixToFixed(f32 matrix[4][4], Mtx *fixedMatrix);
void func_80050DA0(s32 soundId, s32 channel, s32 volume, s32 reverb, s32 pan);
void func_8004D794(void);
void func_80071370(void);
void func_80081284(void);
void func_8003E400(void);

#endif
