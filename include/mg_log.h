#ifndef MG_LOG_H
#define MG_LOG_H

#include "mg_type.h"

#include "ultra64.h"

extern bool bit_is_set(u32);

#define LOG_INFO(message) \
    do \
    { \
        if (!bit_is_set(0x4DU)) \
        { \
            osSyncPrintf(message); \
        } \
    } while (0)

#define LOG_INFO1(message, a1) \
    do \
    { \
        if (!bit_is_set(0x4DU)) \
        { \
            osSyncPrintf(message, a1); \
        } \
    } while (0)

#define LOG_INFO2(message, a1, a2) \
    if (!bit_is_set(0x4DU)) \
    { \
        osSyncPrintf(message, a1, a2); \
    }

#define LOG_INFO3(message, a1, a2, a3) \
    if (!bit_is_set(0x4DU)) \
    { \
        osSyncPrintf(message, a1, a2, a3); \
    } 

#endif
