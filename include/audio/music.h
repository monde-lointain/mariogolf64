#ifndef MUSIC_H
#define MUSIC_H

#include <PR/libmus.h>
#include <ultra64.h>

#include "audio/sound.h"

void InitMusic(musConfig *config);
void LoadSampleBank(u8 *pointerBankRom, u32 pointerBankSize, u8 *waveBankRom);
void LoadSong(u32 romAddress, u32 size);
void LoadSoundEffectBank(u8 *bankRom, u32 bankSize);
musHandle StartLoadedSong(void);
musHandle StartSoundEffect(SoundId soundId);

#endif
