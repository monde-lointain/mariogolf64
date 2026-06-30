/*
 * libmus_config.h
 *
 * Compile-time configuration for the libmus sequenced-music player. Each flag
 * here selects an optional feature: defining it links the code that implements
 * the feature, while leaving it undefined compiles that code out to shrink the
 * audio image. The flags are evaluated only at build time (no run-time cost),
 * so this header is included first by every libmus translation unit that
 * branches on one.
 */

#ifndef _LIBMUS_CONFIG_H_
#define _LIBMUS_CONFIG_H_

/*
 * Enable swappable custom audio effects (the reverb/FX block in player_fx.h).
 * When defined, the player can replace the synthesizer's active effect at run
 * time: the FIFOCMD_CHANGEFX command and the Fchangefx sequence opcode both
 * call ChangeCustomEffect(), and the custom alInit / FX-allocation path is
 * built in. Undefined, those two effect-change paths compile to no-ops and the
 * stock synthesizer effect is used.
 */
#define SUPPORT_FXCHANGE

#endif /* _LIBMUS_CONFIG_H_ */
