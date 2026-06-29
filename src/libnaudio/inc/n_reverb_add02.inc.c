/*
 * N_MICRO body of _n_saveBuffer's ring-buffer save: the compact n_aSaveBuffer
 * folds the full build's aSetBuffer + aSaveBuffer pair into one command. The
 * first two saves run when the write crosses the delay-line end (pre-wrap
 * chunk, then the wrapped chunk to base); the else branch is the single
 * non-wrapping save.
 */
    n_aSaveBuffer(ptr++, before_end << 1, buff, osVirtualToPhysical(curr_ptr));
    n_aSaveBuffer(ptr++, after_end << 1, buff + (before_end << 1),
                  osVirtualToPhysical(r->base));
  }
  else {
    n_aSaveBuffer(ptr++, FIXED_SAMPLE << 1, buff, osVirtualToPhysical(curr_ptr));
  }
