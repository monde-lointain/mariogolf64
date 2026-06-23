/*
 * Peripheral-interface startup: bring up the PI manager and open a handle to
 * the cartridge ROM so the rest of nusys can DMA from the cart.
 */

#include <nusys.h>

extern OSMesgQueue PiMesgQ;
extern OSMesg PiMesgBuf[NU_PI_MESG_NUM];
extern OSPiHandle* nuPiCartHandle;

/*
 * Start the OS PI manager (the thread that serializes PI/DMA access) and cache
 * the cart ROM handle for later nuPiReadRom transfers.
 */
void nuPiInit(void) {
  osCreatePiManager((OSPri)OS_PRIORITY_PIMGR, &PiMesgQ, PiMesgBuf,
                    NU_PI_MESG_NUM);
  nuPiCartHandle = osCartRomInit();
}
