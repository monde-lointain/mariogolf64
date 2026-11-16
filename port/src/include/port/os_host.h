/*
 * What the port's libultra offers the platform layer, which plays the part
 * of the N64's hardware: its timers raise the interrupts, and it holds the
 * cartridge.
 *
 * These may be called from any host thread, the platform's included.
 */

#ifndef PORT_OS_HOST_H
#define PORT_OS_HOST_H

#include <stddef.h>
#include <ultra64.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#    define PC_NORETURN __attribute__((noreturn))
#    define PC_PRINTF_FORMAT(formatIndex, firstArgument) \
        __attribute__((format(printf, formatIndex, firstArgument)))
#else
#    define PC_NORETURN
#    define PC_PRINTF_FORMAT(formatIndex, firstArgument)
#endif

/* Readies the threads, the overlays' snapshots and the clock. Call once,
   before any other routine of libultra's. */
void PcInitOs(void);

/* Gives libultra the cartridge's ROM, in the N64's byte order. The memory
   must outlive the program's use of libultra. */
void PcInsertCartridge(const u8 *rom, size_t size);

/* The cartridge's ROM and its size, or NULL with no cartridge */
const u8 *PcGetCartridgeRom(size_t *size);

/* Copies size bytes of the cartridge's ROM at romOffset to destination,
   reading zeros past its end */
void PcReadCartridge(u32 romOffset, void *destination, u32 size);

/* Creates and starts a thread of the game's from a host thread of the
   platform's, as the N64's boot code starts the first */
void PcStartBootThread(OSThread *thread, void (*entry)(void *), void *argument, OSPri priority);

/* Raises an interrupt: sends the message set for the event, if any, and
   wakes the threads waiting for an interrupt */
void PcRaiseEvent(OSEvent event);

/* The vertical retrace: shows the frame buffer that osViSwapBuffer chose,
   sends the message that osViSetEvent set every retraceCount retraces, and
   raises the interrupt. Returns the frame buffer now shown, or NULL while
   the screen is black. */
void *PcRaiseRetrace(void);

/* Ends the program with a message, for errors that the port can't recover
   from */
PC_NORETURN void PcFatalError(const char *format, ...) PC_PRINTF_FORMAT(1, 2);

#ifdef __cplusplus
}
#endif

#endif
