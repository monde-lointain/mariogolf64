/*
 * save.c
 *
 * Save filter: the tail of the synthesis chain. It pulls the final mix from
 * its source, interleaves the separate L/R buffers into stereo, and DMAs the
 * result out to a DRAM buffer for the audio interface to play.
 */
#include <libaudio.h>
#include "synthInternals.h"
#include <os.h>
#include <assert.h>
#ident "$Revision: 1.17 $"

/*
 * Command-list handler: pull the upstream mix, interleave MAIN L/R into a
 * single stereo stream, and emit a save of outCount stereo frames to the
 * filter's DRAM output address. outCount is a sample count; "<< 1" gives the
 * per-channel byte count and "<< 2" the interleaved-stereo byte count.
 */
Acmd* alSavePull(void* filter, s16* outp, s32 outCount, s32 sampleOffset,
                 Acmd* p) {
  Acmd* ptr = p;
  ALSave* f = (ALSave*)filter;
  ALFilter* source = f->filter.source;
#if BUILD_VERSION < VERSION_J
#line 33
#endif
#ifdef _DEBUG
  assert(f->filter.source);
#endif
  ptr = (*source->handler)(source, outp, outCount, sampleOffset, ptr);
  aSetBuffer(ptr++, 0, 0, 0, outCount << 1);
  aInterleave(ptr++, AL_MAIN_L_OUT, AL_MAIN_R_OUT);
  aSetBuffer(ptr++, 0, 0, 0, outCount << 2);
  aSaveBuffer(ptr++, f->dramout);
  return ptr;
}

/*
 * Parameter setter: wire up the upstream source, or set the DRAM destination
 * address the interleaved mix is written to.
 */
s32 alSaveParam(void* filter, s32 paramID, void* param) {
  ALSave* a = (ALSave*)filter;
  ALFilter* f = (ALFilter*)filter;
  s32 pp = (s32)param;

  switch (paramID) {
    case (AL_FILTER_SET_SOURCE):
      f->source = (ALFilter*)param;
      break;
    case (AL_FILTER_SET_DRAM):
      a->dramout = pp;
      break;
    default:
      break;
  }
  return 0;
}
