/*
 * Game-side wrapper that initializes the SRAM (cartridge battery-backed save)
 * device. Forwards to the nusys SRAM setup, which registers the save-RAM PI
 * handle so later read/write calls can reach it.
 */
#include "common.h"

extern void nuPiInitSram(void);

void func_80099490(void) { nuPiInitSram(); }
