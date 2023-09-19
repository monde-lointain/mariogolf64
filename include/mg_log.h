#ifndef MG_LOG_H
#define MG_LOG_H

#include "ultra64.h"

extern s32 func_800299F4(u32);

#define LOG_INFO(message) \
    do \
    { \
        if (!func_800299F4(0x4DU)) \
        { \
            osSyncPrintf(message); \
        } \
    } while (0)

#define LOG_INFO1(message, a1) \
    do \
    { \
        if (!func_800299F4(0x4DU)) \
        { \
            osSyncPrintf(message, a1); \
        } \
    } while (0)

#define LOG_INFO2(message, a1, a2) \
    if (!func_800299F4(0x4DU)) \
    { \
        osSyncPrintf(message, a1, a2); \
    }

#define LOG_INFO3(message, a1, a2, a3) \
        if (!func_800299F4(0x4DU)) \
    { \
        osSyncPrintf(message, a1, a2, a3); \
    } 

#endif
