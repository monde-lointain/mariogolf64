/*
 * n_abi.h
 *
 * naudio audio-command (Acmd) assembly macros for libmus. Each macro writes one
 * RSP audio DSP operation into the 64-bit Acmd at pkt, packing the opcode and
 * operands into the command's two 32-bit words (w0/w1). _SHIFTL(value, shift,
 * width) places each operand into a width-bit field starting at bit `shift`;
 * the high byte of w0 always holds the A_* opcode. These "n_a" variants fold
 * the DMEM/DRAM buffer addresses directly into each command, matching the
 * naudio microcode's command layout (the stock ABI instead issues a separate
 * aSetBuffer). Each macro's operands are listed below with their word and bit
 * range, written w<n>[hi:lo].
 */
#ifndef _N_ABI__
#define _N_ABI__

// Only expose the assembly macros to C / C++ translation units.
#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/*
 * Decode a run of ADPCM samples from the per-voice decoder state.
 *   s = physical address of the decoder state   (w0[23:0])
 *   f = flags: A_INIT / A_LOOP / A_CONTINUE      (w1[31:28])
 *   c = number of bytes to decode                (w1[27:16])
 *   a = source DRAM alignment, low bits of addr  (w1[15:12])
 *   d = output DMEM address                      (w1[11:0])
 */
#define n_aADPCMdec(pkt, s, f, c, a, d)                           \
  {                                                               \
    Acmd* _a = (Acmd*)pkt;                                        \
                                                                  \
    _a->words.w0 = (_SHIFTL(A_ADPCM, 24, 8) | _SHIFTL(s, 0, 24)); \
    _a->words.w1 = (_SHIFTL(f, 28, 4) | _SHIFTL(c, 16, 12) |      \
                    _SHIFTL(a, 12, 4) | _SHIFTL(d, 0, 12));       \
  }

/*
 * Run the one-pole low-pass filter on the reverb feedback path.
 *   f = flags: first-frame A_INIT vs A_CONTINUE  (w0[23:16])
 *   g = filter gain                              (w0[15:0])
 *   t = DMEM buffer address operated on          (w1[31:24])
 *   s = physical address of the filter state     (w1[23:0])
 */
#define n_aPoleFilter(pkt, f, g, t, s)                                      \
  {                                                                         \
    Acmd* _a = (Acmd*)pkt;                                                  \
                                                                            \
    _a->words.w0 =                                                          \
        (_SHIFTL(A_POLEF, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(g, 0, 16));  \
    _a->words.w1 = (_SHIFTL(t, 24, 8) | _SHIFTL((unsigned int)(s), 0, 24)); \
  }

/*
 * Apply the envelope ramp loaded by the preceding n_aSetVolume commands and
 * mix the voice into the output buffers.
 *   f = flags: A_INIT vs A_CONTINUE              (w0[23:16])
 *   t = current right-channel volume on A_INIT   (w0[15:0])
 *       (the one mix value SetVolume cannot
 *       carry; 0 when continuing)
 *   s = physical address of the envmixer state   (w1)
 */
#define n_aEnvMixer(pkt, f, t, s)                                             \
  {                                                                           \
    Acmd* _a = (Acmd*)pkt;                                                    \
                                                                              \
    _a->words.w0 =                                                            \
        (_SHIFTL(A_ENVMIXER, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(t, 0, 16)); \
    _a->words.w1 = (unsigned int)(s);                                         \
  }

/*
 * Interleave the separate left/right mix buffers into one stereo stream. The
 * DMEM buffer addresses are implicit in this variant, so only the opcode (w0)
 * is written and w1 is left untouched.
 */
#define n_aInterleave(pkt)                       \
  {                                              \
    Acmd* _a = (Acmd*)pkt;                       \
                                                 \
    _a->words.w0 = _SHIFTL(A_INTERLEAVE, 24, 8); \
  }

/*
 * DMA a sample buffer from DRAM into DMEM.
 *   c = byte count to load                       (w0[23:12])
 *   d = destination DMEM address                 (w0[11:0])
 *   s = physical (DRAM) source address           (w1)
 */
#define n_aLoadBuffer(pkt, c, d, s)                                            \
  {                                                                            \
    Acmd* _a = (Acmd*)pkt;                                                     \
                                                                               \
    _a->words.w0 =                                                             \
        (_SHIFTL(A_LOADBUFF, 24, 8) | _SHIFTL(c, 12, 12) | _SHIFTL(d, 0, 12)); \
    _a->words.w1 = (unsigned int)(s);                                          \
  }

/*
 * Pitch-shift / resample a DMEM buffer.
 *   s = physical address of the resampler state  (w0[23:0])
 *   f = flags: first-frame select                (w1[31:30])
 *   p = 16-bit pitch increment (phase step)      (w1[29:14])
 *   i = input DMEM address                       (w1[13:2])
 *   o = output buffer selector                   (w1[1:0])
 */
#define n_aResample(pkt, s, f, p, i, o)                              \
  {                                                                  \
    Acmd* _a = (Acmd*)pkt;                                           \
                                                                     \
    _a->words.w0 = (_SHIFTL(A_RESAMPLE, 24, 8) | _SHIFTL(s, 0, 24)); \
    _a->words.w1 = (_SHIFTL(f, 30, 2) | _SHIFTL(p, 14, 16) |         \
                    _SHIFTL(i, 2, 12) | _SHIFTL(o, 0, 2));           \
  }

/*
 * DMA a sample buffer from DMEM back out to DRAM.
 *   c = byte count to save                       (w0[23:12])
 *   d = source DMEM address                      (w0[11:0])
 *   s = physical (DRAM) destination address      (w1)
 */
#define n_aSaveBuffer(pkt, c, d, s)                                            \
  {                                                                            \
    Acmd* _a = (Acmd*)pkt;                                                     \
                                                                               \
    _a->words.w0 =                                                             \
        (_SHIFTL(A_SAVEBUFF, 24, 8) | _SHIFTL(c, 12, 12) | _SHIFTL(d, 0, 12)); \
    _a->words.w1 = (unsigned int)(s);                                          \
  }

/*
 * Load one envelope operand triple into the mixer (issued before n_aEnvMixer).
 * The channel/mode flags in `f` (A_LEFT / A_RIGHT / A_AUX with A_VOL or A_RATE)
 * select what the three 16-bit operands carry: a current volume, a ramp target
 * and rate, or the dry/wet send amounts.
 *   f = channel / mode flags                     (w0[23:16])
 *   v = volume or ramp target                    (w0[15:0])
 *   t = ramp rate, mid word / dry send amount    (w1[31:16])
 *   r = ramp rate, low word / wet send amount    (w1[15:0])
 */
#define n_aSetVolume(pkt, f, v, t, r)                                       \
  {                                                                         \
    Acmd* _a = (Acmd*)pkt;                                                  \
                                                                            \
    _a->words.w0 =                                                          \
        (_SHIFTL(A_SETVOL, 24, 8) | _SHIFTL(f, 16, 8) | _SHIFTL(v, 0, 16)); \
    _a->words.w1 = _SHIFTL(t, 16, 16) | _SHIFTL(r, 0, 16);                  \
  }

/*
 * Load the ADPCM predictor codebook into the RSP.
 *   c = byte count of the codebook               (w0[23:0])
 *   d = physical (DRAM) address of the codebook  (w1)
 */
#define n_aLoadADPCM(pkt, c, d)                                     \
  {                                                                 \
    Acmd* _a = (Acmd*)pkt;                                          \
                                                                    \
    _a->words.w0 = _SHIFTL(A_LOADADPCM, 24, 8) | _SHIFTL(c, 0, 24); \
    _a->words.w1 = (unsigned int)d;                                 \
  }
#endif
#endif
