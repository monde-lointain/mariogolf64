/*
 * libmus_config.h
 *
 * Build-time configuration for the libmus music/sound driver. Each knob here
 * is a feature toggle the rest of the library tests with #ifdef to compile in
 * (or omit) an optional capability. Edit this file to retune the build; the
 * sources never define these flags themselves.
 */
#ifndef _LIBMUS_CONFIG_H_
#define _LIBMUS_CONFIG_H_

// Compile in runtime effect switching: lets a song change its reverb/effect
// type while playing (enables player_fx.h and the ChangeCustomEffect path).
#define SUPPORT_FXCHANGE

#endif
