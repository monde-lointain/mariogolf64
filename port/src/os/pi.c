/*
 * The peripheral interface: DMA from the cartridge's ROM.
 *
 * A transfer completes at once, as a memory copy, and its message is sent
 * before the call returns. The SRAM has its own interface in the port's
 * NuSystem, so writes to devices other than the ROM are refused.
 */

#include <SDL3/SDL.h>

#include "os_internal.h"
#include "port/os_host.h"

// Where the PI maps the cartridge's ROM
#define CART_ROM_BASE 0x10000000u
#define CART_ROM_ADDRESS_MASK 0x0FFFFFFFu

static const u8 *f_Rom;
static size_t f_RomSize;
static OSPiHandle f_CartHandle;

void PcInsertCartridge(const u8 *rom, size_t size) {
    SDL_assert_always(rom != NULL && size != 0);
    f_Rom = rom;
    f_RomSize = size;
}

const u8 *PcGetCartridgeRom(size_t *size) {
    if (size != NULL) {
        *size = f_RomSize;
    }

    return f_Rom;
}

void PcReadCartridge(u32 romOffset, void *destination, u32 size) {
    u8 *bytes = destination;
    size_t available = 0;

    SDL_assert(destination != NULL || size == 0);
    if (romOffset < f_RomSize) {
        available = SDL_min(f_RomSize - romOffset, (size_t)size);
        SDL_memcpy(bytes, f_Rom + romOffset, available);
    }

    if (available < size) {
        SDL_memset(bytes + available, 0, size - available);
    }
}

OSPiHandle *osCartRomInit(void) {
    f_CartHandle.type = DEVICE_TYPE_CART;
    f_CartHandle.baseAddress = PHYS_TO_K1(CART_ROM_BASE);
    f_CartHandle.domain = PI_DOMAIN1;
    return &f_CartHandle;
}

// The ROM's offset of a device address, which may or may not carry the
// cartridge's PI base
static u32 RomOffset(u32 deviceAddress) {
    return deviceAddress & CART_ROM_ADDRESS_MASK;
}

static s32 StartDma(OSIoMesg *request, s32 direction) {
    SDL_assert(request != NULL);
    if (direction != OS_READ) {
        PcFatalError("PI writes to the cartridge aren't supported");
    }

    PcReadCartridge(RomOffset(request->devAddr), request->dramAddr, request->size);
    if (request->hdr.retQueue != NULL) {
        osSendMesg(request->hdr.retQueue, (OSMesg)request, OS_MESG_NOBLOCK);
    }

    return 0;
}

s32 osEPiStartDma(OSPiHandle *handle, OSIoMesg *request, s32 direction) {
    SDL_assert(handle == NULL || handle->type == DEVICE_TYPE_CART);
    request->piHandle = handle;
    return StartDma(request, direction);
}

s32 osPiStartDma(
    OSIoMesg *request,
    s32 priority,
    s32 direction,
    u32 deviceAddress,
    void *dramAddress,
    u32 size,
    OSMesgQueue *queue
) {
    request->hdr.pri = (u8)priority;
    request->hdr.retQueue = queue;
    request->devAddr = deviceAddress;
    request->dramAddr = dramAddress;
    request->size = size;
    request->piHandle = NULL;
    return StartDma(request, direction);
}

void osCreatePiManager(OSPri priority, OSMesgQueue *queue, OSMesg *messages, s32 count) {
    (void)priority;
    (void)queue;
    (void)messages;
    (void)count;
}
