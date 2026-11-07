#ifndef MUSIC_H
#define MUSIC_H

#include <PR/libmus.h>
#include <ultra64.h>

#include "sound.h"

/* The music player, and the songs and sound banks it loads */

void InitMusic(musConfig *config);
void LoadSampleBank(u8 *pointerBankRom, u32 pointerBankSize, u8 *waveBankRom);
void LoadSong(u32 romAddress, u32 size);
void LoadSoundEffectBank(u8 *rom, u32 size);
musHandle StartLoadedSong(void);
musHandle StartSoundEffect(SoundId soundId);

#endif
