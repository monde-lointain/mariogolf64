/*
 * Boot-time OS/hardware initialization.
 *
 * The very first ultralib code to run after the boot stub hands off. It brings
 * the CPU, RCP, and peripheral bus into a known state: enables the FPU,
 * installs the exception-vector preamble at the CPU's fixed vector addresses,
 * programs the PI bus-speed (DOM1/DOM2) parameters, sets up the TLB, derives
 * the CPU/VI clock rates, and configures the audio interface. The body is
 * heavily conditioned on BUILD_VERSION and _FINALROM because the boot contract
 * changed across SDK revisions and because development ROMs additionally hand
 * control to a PI-resident KMC monitor.
 *
 * Defines the OS-global clock/shutdown/interrupt-mask state shared across the
 * kernel.
 */

#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "PR/os_version.h"
#include "piint.h"
#undef __osInitialize_common

// The exception-vector preamble is a 4-instruction trampoline copied verbatim
// into each of the CPU's fixed exception entry points.
typedef struct {
  unsigned int inst1;
  unsigned int inst2;
  unsigned int inst3;
  unsigned int inst4;
} __osExceptionVector;

extern __osExceptionVector __osExceptionPreamble[];
extern OSPiHandle __Dom1SpeedParam;
extern OSPiHandle __Dom2SpeedParam;

// OS-global state. Clock/VI rates are recomputed below from the detected TV
// standard; the interrupt mask and shutdown flag start fully permissive.
OSTime osClockRate = OS_CLOCK_RATE;
s32 osViClock = VI_NTSC_CLOCK;
u32 __osShutdown = 0;
u32 __OSGlobalIntMask = OS_IM_ALL;

#ifdef _FINALROM
extern u32 __osFinalrom;
#else
u32 __kmc_pt_mode;
#if BUILD_VERSION >= VERSION_K
void* __printfunc = NULL;
#endif
#endif

#if BUILD_VERSION >= VERSION_K
#define INITIALIZE_FUNC __osInitialize_common
#define SPEED_PARAM_FUNC __createSpeedParam
#else
#define INITIALIZE_FUNC __osInitialize_common
#define SPEED_PARAM_FUNC create_speed_param
#if BUILD_VERSION >= VERSION_J
static void ptstart(void);
static void SPEED_PARAM_FUNC(void);
#endif
extern __osExceptionVector __ptExceptionPreamble[];
#endif

#if BUILD_VERSION >= VERSION_K
// Snapshot the PI bus-timing registers (cartridge access latency/pulse/page/
// release) for both domains so later DMA setup can restore them.
void SPEED_PARAM_FUNC(void) {
  __Dom1SpeedParam.type = DEVICE_TYPE_INIT;
  __Dom1SpeedParam.latency = IO_READ(PI_BSD_DOM1_LAT_REG);
  __Dom1SpeedParam.pulse = IO_READ(PI_BSD_DOM1_PWD_REG);
  __Dom1SpeedParam.pageSize = IO_READ(PI_BSD_DOM1_PGS_REG);
  __Dom1SpeedParam.relDuration = IO_READ(PI_BSD_DOM1_RLS_REG);
  __Dom2SpeedParam.type = DEVICE_TYPE_INIT;
  __Dom2SpeedParam.latency = IO_READ(PI_BSD_DOM2_LAT_REG);
  __Dom2SpeedParam.pulse = IO_READ(PI_BSD_DOM2_PWD_REG);
  __Dom2SpeedParam.pageSize = IO_READ(PI_BSD_DOM2_PGS_REG);
  __Dom2SpeedParam.relDuration = IO_READ(PI_BSD_DOM2_RLS_REG);
}
#endif

void INITIALIZE_FUNC() {
  u32 pifdata;
#if BUILD_VERSION < VERSION_K
  u32 clock = 0;
#endif

#ifdef _FINALROM
  __osFinalrom = TRUE;
#endif

  // Bring up the FPU: enable coprocessor 1, then put it in round-to-nearest
  // with flush-to-zero and invalid-operation reporting.
  __osSetSR(__osGetSR() | SR_CU1);
  __osSetFpcCsr(FPCSR_FS | FPCSR_EV | FPCSR_RM_RN);
#if BUILD_VERSION >= VERSION_J
  __osSetWatchLo(0x4900000);
#endif

  // Toggle a bit in PIF RAM to release the boot ROM / signal the PIF. The
  // raw SI accessors return non-zero while a transfer is still in flight, so
  // spin until each read and write completes.
  while (__osSiRawReadIo(PIF_RAM_END - 3, &pifdata)) {
    ;
  }
  while (__osSiRawWriteIo(PIF_RAM_END - 3, pifdata | 8)) {
    ;
  }

  // Install the exception preamble at all four CPU vector slots (TLB miss,
  // 64-bit TLB miss, cache error, and the general exception vector), then push
  // the writes out of the D-cache and flush the stale I-cache copies so the CPU
  // fetches the freshly written handlers.
  *(__osExceptionVector*)UT_VEC = *__osExceptionPreamble;
  *(__osExceptionVector*)XUT_VEC = *__osExceptionPreamble;
  *(__osExceptionVector*)ECC_VEC = *__osExceptionPreamble;
  *(__osExceptionVector*)E_VEC = *__osExceptionPreamble;
  osWritebackDCache((void*)UT_VEC,
                    E_VEC - UT_VEC + sizeof(__osExceptionVector));
  osInvalICache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(__osExceptionVector));

#if BUILD_VERSION >= VERSION_J
  // Newer boot path: snapshot the PI timing, then rebuild the TLB from scratch
  // and map the RDB (debug) region.
  SPEED_PARAM_FUNC();
  osUnmapTLBAll();
  osMapTLBRdb();
#else
  // Older boot path: map the RDB region, then read the CPU clock rate the boot
  // code stashed at physical offset 4 (low nibble is a flag, so mask it off).
  osMapTLBRdb();
  osPiRawReadIo(4, &clock);
  clock &= ~0xf;
  if (clock != 0) {
    osClockRate = clock;
  }
#endif

  // The published OS clock rate is three-quarters of the raw CPU count rate.
  osClockRate = osClockRate * 3 / 4;

  // On a cold boot (not an app-triggered NMI), clear the persistent NMI buffer.
  if (osResetType == 0) {
    bzero(osAppNMIBuffer, OS_APP_NMI_BUFSIZE);
  }

  // Pick the VI dot clock for the detected TV standard.
  if (osTvType == OS_TV_PAL) {
    osViClock = VI_PAL_CLOCK;
  } else if (osTvType == OS_TV_MPAL) {
    osViClock = VI_MPAL_CLOCK;
  } else {
    osViClock = VI_NTSC_CLOCK;
  }

#if BUILD_VERSION >= VERSION_J
  // A pending RCP interrupt (IP5) this early means an unrecoverable hardware
  // state, so hang deliberately rather than continue into an undefined boot.
  if (__osGetCause() & CAUSE_IP5) {
    while (TRUE) {
      ;
    }
  }

  // Bring the audio interface up and program it for the maximum DAC/bit rate.
  IO_WRITE(AI_CONTROL_REG, AI_CONTROL_DMA_ON);
  IO_WRITE(AI_DACRATE_REG, AI_MAX_DAC_RATE - 1);
  IO_WRITE(AI_BITRATE_REG, AI_MAX_BIT_RATE - 1);
#endif

#if BUILD_VERSION < VERSION_K && !defined(_FINALROM)
  // Development-ROM hand-off to the KMC PI monitor. The monitor lives in PI
  // address space and is only present on dev hardware; bail out entirely if its
  // signature word is absent.
  if (!__kmc_pt_mode) {
    int (*fnc)();
#if BUILD_VERSION < VERSION_J
    unsigned int c;
    unsigned int c1;
#endif
    unsigned int* src;
    unsigned int* dst;
    unsigned int monadr;
    volatile unsigned int* mon;
    volatile unsigned int* stat;

    stat = (unsigned*)0xbff08004;
    mon = (unsigned*)0xBFF00000;
    if (*mon != 0x4B4D4300) {
      return;
    }

    // Copy the monitor's exception preamble into the general-exception vector,
    // skipping two words in the middle of both source and destination (the
    // monitor leaves that gap intact), then flush the 0x24-byte window.
    src = (unsigned*)__ptExceptionPreamble;
    dst = (unsigned*)E_VEC;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    src += 2;
    dst += 2;
    *dst++ = *src++;
    *dst++ = *src++;
    *dst++ = *src++;
    osWritebackDCache(E_VEC, 0x24);
    osInvalICache(E_VEC, 0x24);
    __kmc_pt_mode = TRUE;

    // If the monitor is not already relocated (status bit clear) and points
    // somewhere other than its PI base, copy its 0x2000-byte image to the
    // target RAM address and jump to its entry (8 bytes past the relocated
    // base), handing it the signature and a zero argument.
    if ((*stat & 0x10) == 0) {
      monadr = *(mon + 1);
      if (monadr != 0xBFF00000) {
        unsigned int* src;
        unsigned int* dst = monadr | 0x20000000;
        unsigned int ct = 0x2000 / 4;
        src = 0xBFF00000;
        while (ct != 0) {
          *dst++ = *src++;
          ct--;
        }
      }
      fnc = monadr + 8;
      fnc(0x4B4D4300, 0);
    }
  }
#endif
}

#if !defined(_FINALROM) && BUILD_VERSION < VERSION_J
void ptstart(void) {}
#elif !defined(_FINALROM) && BUILD_VERSION < VERSION_K
static void ptstart(void) {}
#endif

#if BUILD_VERSION >= VERSION_K
// Probe for the host environment in development builds and route to the
// matching initializer: hardware MSP board, KMC PI monitor, ISViewer, or a bare
// emulator.
void __osInitialize_autodetect(void) {
#ifndef _FINALROM
  if (__checkHardware_msp()) {
    __osInitialize_msp();
  } else if (__checkHardware_kmc()) {
    __osInitialize_kmc();
  } else if (__checkHardware_isv()) {
    __osInitialize_isv();
  } else {
    __osInitialize_emu();
  }
#endif
}
#elif BUILD_VERSION == VERSION_J
// VERSION_J snapshots the PI bus-timing parameters for both cartridge domains;
// see the VERSION_K SPEED_PARAM_FUNC above for the field-by-field meaning.
static void SPEED_PARAM_FUNC(void) {
  __Dom1SpeedParam.type = DEVICE_TYPE_INIT;
  __Dom1SpeedParam.latency = IO_READ(PI_BSD_DOM1_LAT_REG);
  __Dom1SpeedParam.pulse = IO_READ(PI_BSD_DOM1_PWD_REG);
  __Dom1SpeedParam.pageSize = IO_READ(PI_BSD_DOM1_PGS_REG);
  __Dom1SpeedParam.relDuration = IO_READ(PI_BSD_DOM1_RLS_REG);
  __Dom2SpeedParam.type = DEVICE_TYPE_INIT;
  __Dom2SpeedParam.latency = IO_READ(PI_BSD_DOM2_LAT_REG);
  __Dom2SpeedParam.pulse = IO_READ(PI_BSD_DOM2_PWD_REG);
  __Dom2SpeedParam.pageSize = IO_READ(PI_BSD_DOM2_PGS_REG);
  __Dom2SpeedParam.relDuration = IO_READ(PI_BSD_DOM2_RLS_REG);
}
#endif
