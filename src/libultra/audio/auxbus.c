/*
 * auxbus.c
 *
 * Auxiliary (wet/effects) bus filter. Sums all of its attached source voices
 * into the AUX stereo output buffers; the effects unit and main bus read from
 * there. The bus is a pass-through summing node with no processing of its own.
 */
#include <libaudio.h>
#include "synthInternals.h"

/*
 * Command-list handler: clear the AUX L/R output buffers, then pull each
 * source in turn so every voice accumulates into them. outCount is a sample
 * count; "<< 1" turns it into a byte count for the s16 buffers. Returns the
 * advanced command pointer.
 */
Acmd* alAuxBusPull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
                   Acmd* p) {
  Acmd* ptr = p;
  ALAuxBus* m = (ALAuxBus*)filter;
  ALFilter** sources = m->sources;
  s32 i;

  aClearBuffer(ptr++, AL_AUX_L_OUT, outCount << 1);
  aClearBuffer(ptr++, AL_AUX_R_OUT, outCount << 1);
  for (i = 0; i < m->sourceCount; i++) {
    ptr = (sources[i]->handler)(sources[i], outp, outCount, sampleOffset, ptr);
  }
  return ptr;
}

/* Parameter setter: the only supported op is attaching another source voice. */
s32 alAuxBusParam(void* filter, s32 paramID, void* param) {
  ALAuxBus* m = (ALAuxBus*)filter;
  ALFilter** sources = m->sources;

  switch (paramID) {
    case (AL_FILTER_ADD_SOURCE):
      sources[m->sourceCount++] = (ALFilter*)param;
      break;
    default:
      break;
  }
  return 0;
}
