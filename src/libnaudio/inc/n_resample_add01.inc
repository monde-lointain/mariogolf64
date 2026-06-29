    // N_MICRO command-list fragment, spliced into n_alResamplePull's resample
    // branch. One n_aResample command both sets the buffer (input window inp,
    // FIXED_SAMPLE output samples) and resamples at pitch step incr, replacing
    // the aSetBuffer + aResample pair the non-micro path emits.
    n_aResample(ptr++, osVirtualToPhysical(e->rs_state), e->rs_first, incr, inp, 0);
