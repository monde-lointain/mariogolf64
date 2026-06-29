  // N_MICRO command-list fragment, spliced into n_alSavePull's output stage.
  // n_aInterleave folds the L/R mix into a stereo buffer and n_aSaveBuffer DMAs
  // the FIXED_SAMPLE-frame stereo result out to DRAM, replacing the four
  // aSetBuffer / aInterleave / aSetBuffer / aSaveBuffer commands of the
  // non-micro path.
  n_aInterleave(ptr++);
  n_aSaveBuffer(ptr++, FIXED_SAMPLE << 2, 0, n_syn->sv_dramout);
