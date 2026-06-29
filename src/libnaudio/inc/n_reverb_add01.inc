/*
 * N_MICRO body of _n_loadBuffer's ring-buffer load: the compact n_aLoadBuffer
 * folds the full build's aSetBuffer + aLoadBuffer pair into one command. The
 * first two loads run when the read crosses the delay-line end (pre-wrap chunk,
 * then the wrapped chunk from base); the else branch is the single
 * non-wrapping load.
 */
    n_aLoadBuffer(ptr++, before_end << 1, buff, osVirtualToPhysical(curr_ptr));
    n_aLoadBuffer(ptr++, after_end << 1, buff + (before_end << 1),
                  osVirtualToPhysical(r->base));
  }
  else {
    n_aLoadBuffer(ptr++, count << 1, buff, osVirtualToPhysical(curr_ptr));
  }
