/*
 * N_MICRO body of _n_loadOutputBuffer's resample (chorus) path: the compact
 * n_aResample takes the source DMEM buffer and ratio directly, replacing the
 * full build's aSetBuffer + aResample pair. Resamples the modulated
 * source-sample chunk back to one output frame for the chorus sweep.
 */
    {
      s16 tmp;
      tmp = buff >> 8;  // pack the DMEM buffer offset for the micro resample command
      n_aResample(ptr++, osVirtualToPhysical(d->rs->state), d->rs->first, ratio,
                  rbuff + (ramalign << 1), tmp);
    }
