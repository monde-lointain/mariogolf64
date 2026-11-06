/*
 * libultra's osInitialize, as the game's copy of libultra built it for a final
 * ROM. It is ultralib's 2.0J initialize.c, except that it also sets the watch
 * register with __osSetWatchLo, which ultralib's sources only do from 2.0K on,
 * so it can't be built from the ultralib submodule.
 */
#include "PR/os_internal.h"
#include "PR/os_version.h"
#include "PR/rcp.h"
#include "PRinternal/piint.h"

typedef struct {
  /* 0x0 */ unsigned int inst1;
  /* 0x4 */ unsigned int inst2;
  /* 0x8 */ unsigned int inst3;
  /* 0xC */ unsigned int inst4;
} __osExceptionVector;

extern __osExceptionVector __osExceptionPreamble[];

extern OSPiHandle __Dom1SpeedParam;
extern OSPiHandle __Dom2SpeedParam;

OSTime osClockRate = OS_CLOCK_RATE;
s32 osViClock = VI_NTSC_CLOCK;
u32 __osShutdown = 0;
u32 __OSGlobalIntMask = OS_IM_ALL;
u32 __osFinalrom;

static void createSpeedParam(void);

void osInitialize(void) {
  u32 pifdata;

  __osFinalrom = TRUE;

  /* Enables the FPU, flushes denormals to zero and traps invalid operations */
  __osSetSR(__osGetSR() | SR_CU1);
  __osSetFpcCsr(FPCSR_FS | FPCSR_EV | FPCSR_RM_RN);
  __osSetWatchLo(0x4900000);

  /* The last byte of the PIF RAM's controller area */
  while (__osSiRawReadIo(PIF_RAM_END - 3, &pifdata)) {
    ;
  }
  while (__osSiRawWriteIo(PIF_RAM_END - 3, pifdata | 8)) {
    ;
  }
  *(__osExceptionVector*)UT_VEC = *__osExceptionPreamble;
  *(__osExceptionVector*)XUT_VEC = *__osExceptionPreamble;
  *(__osExceptionVector*)ECC_VEC = *__osExceptionPreamble;
  *(__osExceptionVector*)E_VEC = *__osExceptionPreamble;
  osWritebackDCache(
      (void*)UT_VEC, E_VEC - UT_VEC + sizeof(__osExceptionVector));
  osInvalICache((void*)UT_VEC, E_VEC - UT_VEC + sizeof(__osExceptionVector));
  createSpeedParam();
  osUnmapTLBAll();
  osMapTLBRdb();
  osClockRate = osClockRate * 3 / 4;

  /* A cold reset */
  if (osResetType == 0) {
    bzero(osAppNMIBuffer, OS_APP_NMI_BUFSIZE);
  }

  if (osTvType == OS_TV_PAL) {
    osViClock = VI_PAL_CLOCK;
  } else if (osTvType == OS_TV_MPAL) {
    osViClock = VI_MPAL_CLOCK;
  } else {
    osViClock = VI_NTSC_CLOCK;
  }

  /* Waits until no RCP interrupt is pending */
  if (__osGetCause() & CAUSE_IP5) {
    while (TRUE) {
      ;
    }
  }

  IO_WRITE(AI_CONTROL_REG, AI_CONTROL_DMA_ON);
  IO_WRITE(AI_DACRATE_REG, AI_MAX_DAC_RATE - 1);
  IO_WRITE(AI_BITRATE_REG, AI_MAX_BIT_RATE - 1);
}

static void createSpeedParam(void) {
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
