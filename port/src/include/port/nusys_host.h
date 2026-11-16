/*
 * What the port's NuSystem needs from the rest of the port: a renderer to
 * run the game's display lists, the host's controllers, and somewhere to
 * keep the cartridge's save memory. The platform layer provides them; the
 * tests provide doubles.
 */

#ifndef PORT_NUSYS_HOST_H
#define PORT_NUSYS_HOST_H

#include <ultra64.h>

// The cartridge's battery-backed SRAM
#define PC_SRAM_SIZE 0x8000

// Runs a graphics task's display list to its end, as the RSP and RDP
// would, drawing into the color images that it names
void PcRunGfxTask(Gfx *displayList, u32 size);

// A bit for each controller port with a controller in it
u8 PcGetConnectedControllers(void);

// The state of the controller in a port: zeros for an empty port
void PcReadControllerPad(int port, OSContPad *pad);

// Whether the controller in a port can rumble, and turning its motor on
// and off
int PcHasRumble(int port);
void PcSetRumble(int port, int isOn);

// Fills image with the save memory kept from before, or leaves it alone
// when there is none
void PcLoadSaveMemory(u8 *image, u32 size);

// Keeps the save memory for the next run
void PcStoreSaveMemory(const u8 *image, u32 size);

// Starts the game on NuSystem's main thread: the N64's nuBoot. The main
// thread runs mainproc, which the game defines. The reset button is
// PcRaiseEvent(OS_EVENT_PRENMI).
void PcBootNuSystem(void);

#endif
