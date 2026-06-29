    // N_MICRO command-list fragment, spliced into _decodeChunk's DMA-load
    // branch (nbytes > 0). A single n_aLoadBuffer both sets the DMEM input
    // window and DMAs the compressed bytes in, replacing the aSetBuffer +
    // aLoadBuffer pair the non-micro path emits.
    n_aLoadBuffer(ptr++, nbytes + 8 - (nbytes & 0x7), inp, dramLoc - dramAlign);
