#include "ultra64.h"

extern void osSyncPrintf(const char* fmt, ...);
extern const char D_800CA184[];

void __assert(const char* exp, const char* filename, int line) {
  osSyncPrintf(D_800CA184);
}
