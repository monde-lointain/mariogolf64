#ifndef _CONTROLLER_GBPAK_H
#define _CONTROLLER_GBPAK_H

#include "PR/ultratypes.h"

extern OSTimer __osGbpakTimer;
extern OSMesg __osGbpakTimerMsg;
extern OSMesgQueue __osGbpakTimerQ;

extern s32 __osGbpakSetBank(OSPfs* pfs, u8 bank);

#endif
