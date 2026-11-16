/*
 * Arithmetic on addresses: the bytes between two, an address some bytes
 * past another, the physical address that the RCP reads, and the
 * alignment that the RDP needs
 */

#ifndef MEMORY_ADDRESS_H
#define MEMORY_ADDRESS_H

#include <ultra64.h>

// The physical address of a KSEG0 address, which the RCP reads. Ultralib's
// OS_K0_TO_PHYSICAL subtracts from a char pointer, which takes it out of its
// object, so other builds than the matching one subtract from the address as
// a number, which KMC GCC compiles differently.
// The native port's RCP reads the host's memory, so there an address is
// its own physical address.
#if defined(__IS_KMC__)
#    define PhysicalAddress(address) OS_K0_TO_PHYSICAL(address)
#elif defined(TARGET_PC)
#    define PhysicalAddress(address) ((uintptr_t)(address))
#else
#    define PhysicalAddress(address) ((u32)(address) - K0BASE)
#endif

// An address as a number, as wide as the host's addresses
#ifdef TARGET_PC
#    define AddressNumber(address) ((uintptr_t)(address))
#else
#    define AddressNumber(address) ((u32)(address))
#endif

// The bytes from start up to end, which may point at different types or into
// different objects, such as the linker's symbols. It subtracts addresses, as
// subtracting pointers into different objects is undefined.
#define BytesBetween(start, end) ((u32)(AddressNumber(end) - AddressNumber(start)))
// The address byteCount bytes past pointer, whatever it points at
#define AddressPast(pointer, byteCount) ((u8 *)(pointer) + (byteCount))

// The RDP loads textures from 8-byte boundaries
#define TextureAddress(address) (AddressNumber(address) & ~7)
// The RDP draws into color and depth images from 8-byte boundaries too
#define RenderTargetAddress(address) (AddressNumber(address) & ~7)

#endif
