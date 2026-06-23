/*
 * filter.c
 *
 * Constructor for the ALFilter base object. Every node in the synthesis
 * pull-graph (decoder, resampler, envelope mixer, bus, effects) embeds an
 * ALFilter and chains to its upstream source through it.
 */
#include <libaudio.h>
#include "synthInternals.h"

/*
 * Initialize a filter node: record its command-list handler (pull function)
 * and parameter setter, and start it detached with no source and empty I/O
 * pointers. Callers wire up the source afterward via AL_FILTER_SET_SOURCE.
 */
void alFilterNew(ALFilter* f, ALCmdHandler h, ALSetParam s, s32 type) {
  f->source = 0;
  f->handler = h;
  f->setParam = s;
  f->inp = 0;
  f->outp = 0;
  f->type = type;
}
