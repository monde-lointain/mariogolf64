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
void DrawBlankFrame(void);
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

#endif
