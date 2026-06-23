/*
 * mainbus.c
 *
 * Main (dry) output bus filter. Sums its attached source voices into the MAIN
 * stereo buffers and folds in the AUX (wet/effects) buffers, producing the
 * final mix that is saved out to the DAC.
 */
#include <libaudio.h>
#include "synthInternals.h"

/*
 * Command-list handler: clear the MAIN L/R buffers, then for each source pull
 * its samples and mix the AUX L/R buffers into MAIN at near-unity gain
 * (0x7fff). outCount is a sample count; "<< 1" makes it a byte count for the
 * s16 buffers. Returns the advanced command pointer.
 */
Acmd* alMainBusPull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
                    Acmd* p) {
  Acmd* ptr = p;
  ALMainBus* m = (ALMainBus*)filter;
  ALFilter** sources = m->sources;
  s32 i;

  aClearBuffer(ptr++, AL_MAIN_L_OUT, outCount << 1);
  aClearBuffer(ptr++, AL_MAIN_R_OUT, outCount << 1);
  for (i = 0; i < m->sourceCount; i++) {
    ptr = (sources[i]->handler)(sources[i], outp, outCount, sampleOffset, ptr);
    aSetBuffer(ptr++, 0, 0, 0, outCount << 1);
    aMix(ptr++, 0, 0x7fff, AL_AUX_L_OUT, AL_MAIN_L_OUT);
    aMix(ptr++, 0, 0x7fff, AL_AUX_R_OUT, AL_MAIN_R_OUT);
  }
  return ptr;
}

/* Parameter setter: the only supported op is attaching another source voice. */
s32 alMainBusParam(void* filter, s32 paramID, void* param) {
  ALMainBus* m = (ALMainBus*)filter;
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
