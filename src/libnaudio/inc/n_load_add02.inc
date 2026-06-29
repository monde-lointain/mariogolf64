  // N_MICRO command-list fragment, spliced into _decodeChunk after the
  // (optional) loop-state restore. One n_aADPCMdec command carries the buffer
  // setup (output offset, sample count, input alignment) and decodes, replacing
  // the aSetBuffer + aADPCMdec pair the non-micro path emits.
  n_aADPCMdec(ptr++, K0_TO_PHYS(f->dc_state), flags, tsam << 1, dramAlign, outp);
