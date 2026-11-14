#ifndef MEMORY_ADDRESS_H
#define MEMORY_ADDRESS_H

#include <ultra64.h>

#ifdef __IS_KMC__
#    define PhysicalAddress(address) OS_K0_TO_PHYSICAL(address)
#else
#    define PhysicalAddress(address) ((u32)(address) - K0BASE)
#endif

#define BytesBetween(start, end) ((u32)(end) - (u32)(start))
#define AddressPast(pointer, byteCount) ((u8 *)(pointer) + (byteCount))

#define TextureAddress(address) ((u32)(address) & ~7)
#define RenderTargetAddress(address) ((u32)(address) & ~7)

#endif
