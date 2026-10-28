/*
 * Plays a character's sounds from its table of sound IDs and volumes, on a
 * channel for each character slot.
 */
#include "common.h"

#include "character.h"
#include "sound.h"

/* Each character's sounds are pairs of a sound ID and the volume it plays at */
#define CHARACTER_SOUND_VALUE_COUNT 34

/* The volume argument that asks for the sound's own volume */
#define CHARACTER_SOUND_VOLUME -1

/* Character sounds play on channels from this one, one for each slot */
#define FIRST_CHARACTER_SOUND_CHANNEL 4

/* The one character whose sounds play without reverb */
#define DRY_CHARACTER 3
#define CHARACTER_SOUND_REVERB 0x14

extern s16 f_CharacterSounds[CHARACTER_COUNT][CHARACTER_SOUND_VALUE_COUNT];

/* Plays a character's sound, where soundIndex is the sound's pair's first value
 */
void PlayCharacterSound(
    s32 character, s32 slot, s32 soundIndex, s32 volume, s32 pan) {
  s32 reverb = (character != DRY_CHARACTER) ? CHARACTER_SOUND_REVERB : 0;

  if (volume == CHARACTER_SOUND_VOLUME) {
    PlaySoundWithEffects(f_CharacterSounds[character][soundIndex],
        slot + FIRST_CHARACTER_SOUND_CHANNEL,
        f_CharacterSounds[character][soundIndex + 1], reverb, pan);
  } else {
    PlaySoundWithEffects(f_CharacterSounds[character][soundIndex],
        slot + FIRST_CHARACTER_SOUND_CHANNEL, volume, reverb, pan);
  }
}
