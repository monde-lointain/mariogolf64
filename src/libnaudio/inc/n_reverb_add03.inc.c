/*
 * N_MICRO body of _n_filterBuffer: the compact n_aPoleFilter takes the source
 * DMEM buffer directly, replacing the full build's aSetBuffer + aPoleFilter
 * pair. Loads the pole coefficients, then filters the frame in place.
 */
  {
    s16 tmp;
    tmp = buff >> 8;  // pack the DMEM buffer offset for the micro pole-filter command
    n_aLoadADPCM(ptr++, 32, osVirtualToPhysical(lp->fcvec.fccoef));
    n_aPoleFilter(ptr++, lp->first, lp->fgain, tmp,
                  osVirtualToPhysical(lp->fstate));
  }
