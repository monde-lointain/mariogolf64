#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <ultra64.h>

/* Routines whose defining file is not decompiled yet */

/* NuSystem */
void nuPiReadRom(u32 romAddr, void *buffer, u32 size);

/* Main segment */
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
