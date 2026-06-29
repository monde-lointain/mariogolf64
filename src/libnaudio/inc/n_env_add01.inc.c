    /*
     * Micro-command tail of _pullSubFrame's first-sub-frame (em_first) block,
     * #included where the non-micro build emits its aSetVolume / aEnvMixer
     * setup. Packs the envelope-init into four naudio commands: a left A_RATE
     * (left target + ramp rate), a left A_VOL (current left volume + the dry/wet
     * fx send), a right A_VOL (right target + ramp rate), and an A_INIT
     * n_aEnvMixer (current right volume + the voice's env-mixer state buffer
     * em_state). The closing brace ends the em_first block; the else arm emits
     * A_CONTINUE to resume the saved envelope on later sub-frames.
     */
    n_aSetVolume(ptr++, A_RATE, e->em_ltgt, e->em_lratm, e->em_lratl);
    n_aSetVolume(ptr++, A_LEFT | A_VOL, e->em_cvolL, e->em_dryamt, e->em_wetamt);
    n_aSetVolume(ptr++, A_RIGHT | A_VOL, e->em_rtgt, e->em_rratm, e->em_rratl);
    n_aEnvMixer(ptr++, A_INIT, e->em_cvolR, osVirtualToPhysical(e->em_state));
  }
  else n_aEnvMixer(ptr++, A_CONTINUE, 0, osVirtualToPhysical(e->em_state));
