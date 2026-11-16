/*
 * NuSystem's reads of the cartridge and its save memory, for the port. ROM
 * reads copy from the cartridge at once. The SRAM is an image in memory,
 * loaded from the platform's storage when the game first opens it and
 * stored back after each write.
 */

#include <string.h>

#include "nusys_internal.h"
#include "port/nusys_host.h"
#include "port/os_host.h"

OSPiHandle *nuPiCartHandle;
OSPiHandle *nuPiSramHandle;

static OSPiHandle f_SramHandle;
static u8 f_SramImage[PC_SRAM_SIZE];
static int f_IsSramLoaded;

void nuPiInit(void) {
    nuPiCartHandle = osCartRomInit();
}

void nuPiReadRom(u32 romAddress, void *buffer, u32 size) {
    PcReadCartridge(romAddress, buffer, size);
}

void nuPiInitSram(void) {
    f_SramHandle.type = DEVICE_TYPE_SRAM;
    f_SramHandle.baseAddress = PHYS_TO_K1(PI_DOM2_ADDR2);
    nuPiSramHandle = &f_SramHandle;
    if (!f_IsSramLoaded) {
        PcLoadSaveMemory(f_SramImage, PC_SRAM_SIZE);
        f_IsSramLoaded = TRUE;
    }
}

// The offset into the SRAM of an address in the cartridge's second domain
static u32 SramOffset(u32 address, u32 size) {
    u32 offset = (address & 0x0FFFFFFF) - PI_DOM2_ADDR2;

    if (offset > PC_SRAM_SIZE || size > PC_SRAM_SIZE - offset) {
        PcFatalError("SRAM access of %u bytes at %#x is out of range", size, address);
    }

    return offset;
}

void nuPiReadWriteSram(u32 address, void *buffer, u32 size, s32 direction) {
    u32 offset = SramOffset(address, size);

    if (!f_IsSramLoaded) {
        nuPiInitSram();
    }

    if (direction == OS_READ) {
        memcpy(buffer, &f_SramImage[offset], size);
    }
    else {
        memcpy(&f_SramImage[offset], buffer, size);
        PcStoreSaveMemory(f_SramImage, PC_SRAM_SIZE);
    }
}
