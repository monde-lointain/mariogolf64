/*
 * Sound effects, songs and the character sounds that wait for an
 * animation, all played through libmus, with the interrupts masked around
 * each libmus call that changes a sound.
 */
#include "common.h"

#include "sound.h"

#include <PR/libmus.h>

#include "asset.h"
#include "character.h"
#include "controller.h"
#include "debug_text.h"
#include "functions.h"
#include "game_flags.h"
#include "save.h"
#include "variables.h"

/* A channel's or the song's handle when there is nothing to control */
#define NO_HANDLE -1
/* Sound IDs are below this */
#define SOUND_COUNT 0x215
/* Song IDs above this play the last song */
#define LAST_SONG_ID 50
/* The echo in libmus's list of effects */
#define ECHO_EFFECT_INDEX 5
/* libmus's master volume for songs */
#define SONG_MASTER_VOLUME 12000

/* The reverb of sound effects */
#define ECHO_REVERB 6
#define DEFAULT_REVERB 2
#define NO_REVERB 0
#define ECHOING_SOUND_COUNT 34

/* Character sounds, which play after a delay set for each character */
#define QUEUED_CHARACTER_SOUND_COUNT 4
#define MAX_CHARACTER_SOUND_DELAY 60

/* The delay editor, on the second controller with GAME_FLAG_UNK5F set */
#define DELAY_EDITOR_CONTROLLER 1
#define DELAY_EDITOR_COLUMN 19
#define DELAY_EDITOR_FIRST_ROW 5
#define DELAY_EDITOR_KIND_ROW 4

/* f_PendingSongChange's states */
#define SONG_CHANGE_NONE 0
#define SONG_CHANGE_SWITCH 1
#define SONG_CHANGE_STOP 2
/* Frames from a switch's request to the old song's stop */
#define SONG_SWITCH_DELAY 2
/* A character sound delay that has run out, its sound played */
#define PLAYED_DELAY (-1)

typedef struct QueuedCharacterSound {
  /* 0x00 */ CharacterId character;
  /* 0x04 */ s32 delay; /* Frames until it plays; negative once played */
  /* 0x08 */ s32 kind;
  /* 0x0C */ SoundVolume volume;
  /* 0x10 */ SoundPan pan;
} QueuedCharacterSound; /* size = 0x14 */

/* A song change for UpdateAudio to make once its delay runs out */
typedef struct SongChange {
  /* 0x00 */ s32 state;
  /* 0x04 */ SongId songId;
  /* 0x08 */ s32 flags;
  /* 0x0C */ s32 delay;
} SongChange; /* size = 0x10 */

extern u8 g_SoundPointerBankRomStart[];
extern u8 g_SoundPointerBankRomEnd[];
extern u8 g_SoundWaveBankRomStart[];
extern u8 g_SoundEffectBankRomStart[];
extern u8 g_SoundEffectBankRomEnd[];

SoundChannel g_NextSoundChannel = 0;
static s32 f_SongHandle = NO_HANDLE;
/* Read by no routine */
static s32 D_800C0E68 = -1;
/* Only the first handle starts as NO_HANDLE */
static s32 f_SoundHandles[SOUND_CHANNEL_COUNT] = {NO_HANDLE};
static u32 f_SoundFadeFrameCounts[SOUND_CHANNEL_COUNT] = {0};
static f32 f_SoundVolumes[SOUND_CHANNEL_COUNT] = {0.0f};
/* Written with the handles but never read */
static s32 f_SongHandleCopy = NO_HANDLE;
static s32 f_SoundHandleCopies[SOUND_CHANNEL_COUNT] = {NO_HANDLE};
static s32 f_EchoingSoundIds[ECHOING_SOUND_COUNT] = {0x7B, 0x7C, 0x7D, 0x7E,
    0x7F, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A,
    0x8B, 0x8C, 0x8D, 0x8E, 0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
    0x98, 0x99, 0x9A, 0x9B, 0x9C, 0x9D};
/* Frames from a character sound's request until it plays, by kind and character
 */
static u16 f_CharacterSoundDelays[CHARACTER_SOUND_KIND_COUNT][CHARACTER_COUNT] =
    {
        {4, 3, 4, 4, 4, 6, 4, 3, 4, 4, 7, 6, 6, 4, 4, 4, 1, 4},
};
static s32 f_PreviewSoundDelays[CHARACTER_COUNT] = {PLAYED_DELAY, PLAYED_DELAY,
    PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY,
    PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY,
    PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY, PLAYED_DELAY,
    PLAYED_DELAY};
static QueuedCharacterSound
    f_QueuedCharacterSounds[QUEUED_CHARACTER_SOUND_COUNT] = {
        {0, PLAYED_DELAY, 0, 0, 0},
        {0, PLAYED_DELAY, 0, 0, 0},
        {0, PLAYED_DELAY, 0, 0, 0},
        {0, PLAYED_DELAY, 0, 0, 0},
};
/* The index in each character's sound table of each kind of sound */
static u8 f_CharacterSoundIndexes[CHARACTER_SOUND_KIND_COUNT] = {
    CHARACTER_SOUND_FULL_SHOT, CHARACTER_SOUND_EXCELLENT,
    CHARACTER_SOUND_CALL_OUT_RIGHT, CHARACTER_SOUND_CALL_OUT_C_UP,
    CHARACTER_SOUND_CALL_OUT_C_DOWN, CHARACTER_SOUND_CALL_OUT_C_LEFT,
    CHARACTER_SOUND_CALL_OUT_C_RIGHT};
static CharacterId f_DelayEditorCharacter = 0;
static s32 f_DelayEditorKind = 0;
static SongChange f_PendingSongChange = {SONG_CHANGE_NONE, 0, 0, 0};
static SongId f_SongId = NO_SONG;
/* The start and end of each song in the ROM, in turn */
static u32 f_SongRomBounds[(LAST_SONG_ID + 1) * 2] = {
    (u32)g_SoundEffectBankRomEnd,
    0xD9FB20,
    0xD9FB20,
    0xDA0640,
    0xDA0640,
    0xDA0D70,
    0xDA0D70,
    0xDA1900,
    0xDA1900,
    0xDA2000,
    0xDA2000,
    0xDA2620,
    0xDA2620,
    0xDA3590,
    0xDA3590,
    0xDA4040,
    0xDA4040,
    0xDA50A0,
    0xDA50A0,
    0xDA73E0,
    0xDA73E0,
    0xDA8A40,
    0xDA8A40,
    0xDA9E40,
    0xDA9E40,
    0xDB2070,
    0xDB2070,
    0xDB9A00,
    0xDB9A00,
    0xDBC0D0,
    0xDBC0D0,
    0xDBDC40,
    0xDBDC40,
    0xDBF230,
    0xDBF230,
    0xDC3010,
    0xDC3010,
    0xDC6E50,
    0xDC6E50,
    0xDCCC90,
    0xDCCC90,
    0xDCE3C0,
    0xDCE3C0,
    0xDD0B10,
    0xDD0B10,
    0xDD16A0,
    0xDD16A0,
    0xDD24E0,
    0xDD24E0,
    0xDD35C0,
    0xDD35C0,
    0xDD4320,
    0xDD4320,
    0xDD5240,
    0xDD5240,
    0xDD6390,
    0xDD6390,
    0xDD7550,
    0xDD7550,
    0xDE1280,
    0xDE1280,
    0xDE76A0,
    0xDE76A0,
    0xDF1030,
    0xDF1030,
    0xDF6A00,
    0xDF6A00,
    0xDFC1F0,
    0xDFC1F0,
    0xE03170,
    0xE03170,
    0xE08660,
    0xE08660,
    0xE0D6F0,
    0xE0D6F0,
    0xE14780,
    0xE14780,
    0xE1CCD0,
    0xE1CCD0,
    0xE210F0,
    0xE210F0,
    0xE25410,
    0xE25410,
    0xE2B6B0,
    0xE2B6B0,
    0xE2CF90,
    0xE2CF90,
    0xE2D7A0,
    0xE2D7A0,
    0xE329E0,
    0xE329E0,
    0xE33210,
    0xE33210,
    0xE33F40,
    0xE33F40,
    0xE37E40,
    0xE37E40,
    0xE3FCD0,
    0xE3FCD0,
    0xE470C0,
    0xE470C0,
    (u32)g_AssetArchiveRomStart,
};
/* Read by no routine */
static s32 D_800C1348[10] = {5, 0, 5, 5, 0, 5, 5, 5, 0, 0};
static u32 f_LastAudioFrame;

musHandle StartSoundEffect(SoundId soundId);
musHandle StartLoadedSong(void);
void LoadSong(u32 romAddress, u32 size);
void LoadSampleBank(u8* pointerBankRom, u32 pointerBankSize, u8* waveBankRom);
void LoadSoundEffectBank(u8* rom, u32 size);
void InitMusic(musConfig* config);
s32 IsMusicOffChosen(void);
s32 ChangeCustomEffect(s32 type);

void PlaySound(SoundId soundId, SoundChannel channel, SoundVolume volume) {
  s32* handle;
  s32 mask;
  s32 index;

  if (!IsGameFlagSet(GAME_FLAG_SILENCE_SOUND_EFFECTS)) {
    mask = osSetIntMask(OS_IM_NONE);
    f_SoundVolumes[channel] = volume;
    if (soundId < SOUND_COUNT) {
      handle = &f_SoundHandles[channel];
      if (*handle != NO_HANDLE) {
        MusHandleStop(*handle, 0);
        *handle = StartSoundEffect(soundId);
        f_SoundHandleCopies[channel] = *handle;
        MusHandleSetVolume(*handle, volume);
        MusHandleSetFreqOffset(*handle, 0.0f);
        MusHandleSetPan(*handle, SOUND_FULL_SCALE);
      }

      if (g_NextSoundChannel == channel) {
        g_NextSoundChannel = channel ^ 1;
      }
    }

    osSetIntMask(mask);
    if (soundId < SOUND_COUNT) {
      for (index = 0; index != ECHOING_SOUND_COUNT; index++) {
        if (f_EchoingSoundIds[index] == soundId) {
          break;
        }
      }

      if (index < ECHOING_SOUND_COUNT) {
        if (f_EchoingSoundIds[index] == SOUND_WASTE_AREA_LANDING) {
          SetSoundReverb(channel, NO_REVERB);
        } else {
          SetSoundReverb(channel, ECHO_REVERB);
        }

        SetSoundFrequencyOffset(channel, 0.0f);
      } else {
        if (soundId == SOUND_UNK90) {
          SetSoundReverb(channel, ECHO_REVERB);
        } else {
          SetSoundReverb(channel, DEFAULT_REVERB);
        }
      }
    }
  }
}

/*
 * Unlike PlaySound, starts the sound even on a channel without a handle.
 * The reverb goes only to the sound being stopped; the new one gets the
 * reverb PlaySound would give it.
 */
void PlaySoundWithEffects(SoundId soundId, SoundChannel channel,
    SoundVolume volume, SoundReverb reverb, SoundPan pan) {
  s32* handle;
  s32 mask;
  s32 index;

  if (!IsGameFlagSet(GAME_FLAG_SILENCE_SOUND_EFFECTS)) {
    mask = osSetIntMask(OS_IM_NONE);
    f_SoundVolumes[channel] = volume;
    if (soundId < SOUND_COUNT) {
      handle = &f_SoundHandles[channel];
      if (*handle != NO_HANDLE) {
        MusHandleStop(*handle, 0);
        MusHandleSetReverb(*handle, reverb);
      }

      *handle = StartSoundEffect(soundId);
      f_SoundHandleCopies[channel] = *handle;
      MusHandleSetFreqOffset(*handle, 0.0f);
      MusHandleSetVolume(*handle, volume);
      MusHandleSetPan(*handle, pan);
      if (g_NextSoundChannel == channel) {
        g_NextSoundChannel = channel ^ 1;
      }
    }

    osSetIntMask(mask);
    if (soundId < SOUND_COUNT) {
      for (index = 0; index != ECHOING_SOUND_COUNT; index++) {
        if (f_EchoingSoundIds[index] == soundId) {
          break;
        }
      }

      if (index < ECHOING_SOUND_COUNT) {
        if (f_EchoingSoundIds[index] == SOUND_WASTE_AREA_LANDING) {
          SetSoundReverb(channel, NO_REVERB);
        } else {
          SetSoundReverb(channel, ECHO_REVERB);
        }

        SetSoundFrequencyOffset(channel, 0.0f);
      } else {
        if (soundId == SOUND_UNK90) {
          SetSoundReverb(channel, ECHO_REVERB);
        } else {
          SetSoundReverb(channel, DEFAULT_REVERB);
        }
      }
    }
  }
}

void StopSound(SoundChannel channel) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SoundHandles[channel] != NO_HANDLE) {
    MusHandleStop(f_SoundHandles[channel], 0);
  }

  osSetIntMask(mask);
}

void SetSoundPan(SoundChannel channel, SoundPan pan) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SoundHandles[channel] != NO_HANDLE) {
    MusHandleSetPan(f_SoundHandles[channel], pan);
  }

  osSetIntMask(mask);
}

void SetSoundVolume(SoundChannel channel, SoundVolume volume) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SoundHandles[channel] != NO_HANDLE) {
    MusHandleSetVolume(f_SoundHandles[channel], volume);
  }

  osSetIntMask(mask);
}

void SetSoundReverb(SoundChannel channel, SoundReverb reverb) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SoundHandles[channel] != NO_HANDLE) {
    MusHandleSetReverb(f_SoundHandles[channel], reverb);
  }

  osSetIntMask(mask);
}

/* UpdateAudio lowers the volume to nothing over the frames */
void FadeOutSound(SoundChannel channel, u32 frameCount) {
  f_SoundFadeFrameCounts[channel] = frameCount;
}

void SetSoundTempo(SoundChannel channel, s32 tempo) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SoundHandles[channel] != NO_HANDLE) {
    MusHandleSetTempo(f_SoundHandles[channel], tempo);
  }

  osSetIntMask(mask);
}

void QueueCharacterSound(CharacterId character, s32 slot, s32 kind,
    SoundVolume volume, SoundPan pan) {
  f_QueuedCharacterSounds[slot].delay = f_CharacterSoundDelays[kind][character];
  f_QueuedCharacterSounds[slot].character = character;
  f_QueuedCharacterSounds[slot].kind = kind;
  f_QueuedCharacterSounds[slot].volume = volume;
  f_QueuedCharacterSounds[slot].pan = pan;
}

static void PreviewCharacterSound(CharacterId character, s32 kind) {
  f_PreviewSoundDelays[character] = f_CharacterSoundDelays[kind][character];
}

/*
 * Plays the queued character sounds whose delays have run out. With
 * GAME_FLAG_UNK5F set, the second controller instead edits the delays of
 * a kind of sound and plays the characters' sounds of it to try them.
 */
static void UpdateCharacterSounds(void) {
  s32 index;

  if (!IsGameFlagSet(GAME_FLAG_UNK5F)) {
    for (index = 0; index != QUEUED_CHARACTER_SOUND_COUNT; index++) {
      if (f_QueuedCharacterSounds[index].delay == 0) {
        PlayCharacterSound(f_QueuedCharacterSounds[index].character, index,
            f_CharacterSoundIndexes[f_QueuedCharacterSounds[index].kind],
            f_QueuedCharacterSounds[index].volume,
            f_QueuedCharacterSounds[index].pan);
      }

      if (f_QueuedCharacterSounds[index].delay >= 0) {
        f_QueuedCharacterSounds[index].delay--;
      }
    }

    return;
  }

  for (index = 0; index != CHARACTER_COUNT; index++) {
    if (f_PreviewSoundDelays[index] == 0) {
      PlayCharacterSound(index, index % QUEUED_CHARACTER_SOUND_COUNT,
          f_CharacterSoundIndexes[f_DelayEditorKind], SOUND_FULL_SCALE,
          SOUND_FULL_SCALE);
    }

    if (f_PreviewSoundDelays[index] >= 0) {
      f_PreviewSoundDelays[index]--;
    }
  }

  if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons &
          L_CBUTTONS) &&
      f_DelayEditorKind > 0) {
    f_DelayEditorKind--;
  }

  if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons &
          R_CBUTTONS) &&
      f_DelayEditorKind < CHARACTER_SOUND_KIND_COUNT - 1) {
    f_DelayEditorKind++;
  }

  if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & R_TRIG) {
    PreviewCharacterSound(
        f_DelayEditorCharacter % CHARACTER_COUNT, f_DelayEditorKind);
  }

  if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & A_BUTTON) {
    PreviewCharacterSound(
        f_DelayEditorCharacter % CHARACTER_COUNT, f_DelayEditorKind);
    PreviewCharacterSound(
        (f_DelayEditorCharacter + 1) % CHARACTER_COUNT, f_DelayEditorKind);
  }

  if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & B_BUTTON) {
    PreviewCharacterSound(
        f_DelayEditorCharacter % CHARACTER_COUNT, f_DelayEditorKind);
    PreviewCharacterSound(
        (f_DelayEditorCharacter + 1) % CHARACTER_COUNT, f_DelayEditorKind);
    PreviewCharacterSound(
        (f_DelayEditorCharacter + 2) % CHARACTER_COUNT, f_DelayEditorKind);
    PreviewCharacterSound(
        (f_DelayEditorCharacter + 3) % CHARACTER_COUNT, f_DelayEditorKind);
  }

  PrintDebugText(" ", DELAY_EDITOR_COLUMN,
      f_DelayEditorCharacter + DELAY_EDITOR_FIRST_ROW);
  if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & U_JPAD) &&
      f_DelayEditorCharacter > 0) {
    f_DelayEditorCharacter--;
  }

  if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & D_JPAD) &&
      f_DelayEditorCharacter < CHARACTER_COUNT - 1) {
    f_DelayEditorCharacter++;
  }

  if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & R_JPAD) {
    if (f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter] <
        MAX_CHARACTER_SOUND_DELAY) {
      f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter]++;
    }
  }

  if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & L_JPAD) {
    if (f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter] !=
        0) {
      f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter]--;
    }
  }

  sprintf(g_PrintBuffer, "NO %3d ", f_DelayEditorKind);
  PrintDebugText(g_PrintBuffer, DELAY_EDITOR_COLUMN + 1, DELAY_EDITOR_KIND_ROW);
  for (index = 0; index != CHARACTER_COUNT; index++) {
    sprintf(g_PrintBuffer, "%3d ",
        f_CharacterSoundDelays[f_DelayEditorKind][index]);
    PrintDebugText(
        g_PrintBuffer, DELAY_EDITOR_COLUMN + 1, index + DELAY_EDITOR_FIRST_ROW);
  }

  PrintDebugText(">", DELAY_EDITOR_COLUMN,
      f_DelayEditorCharacter + DELAY_EDITOR_FIRST_ROW);
}

void SetSoundFrequencyOffset(SoundChannel channel, f32 offset) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SoundHandles[channel] != NO_HANDLE) {
    MusHandleSetFreqOffset(f_SoundHandles[channel], offset);
  }

  osSetIntMask(mask);
}

/*
 * Steps the fades, the character sounds and any song change, once per
 * audio frame. It returns no value; the s32 is there to match, as the
 * original seems to have declared one.
 */
s32 UpdateAudio(void) {
  u32 frame;
  u32 frameCount;
  SoundChannel channel;

  frame = MusGetFrameCount();
  if (frame == f_LastAudioFrame) {
    osSyncPrintf("Ouch!%d,%d,%d,%d!!!!!!!!!!!!!!!!\n",
        f_PendingSongChange.state, f_PendingSongChange.songId,
        f_PendingSongChange.flags, f_PendingSongChange.delay);
    return;
  }

  f_LastAudioFrame = frame;
  for (channel = 0; channel != SOUND_CHANNEL_COUNT; channel++) {
    frameCount = f_SoundFadeFrameCounts[channel];
    if (frameCount != 0) {
      f_SoundFadeFrameCounts[channel] = frameCount - 1;
      f_SoundVolumes[channel] =
          f_SoundVolumes[channel] * (f32)(frameCount - 1) / (f32)frameCount;
      SetSoundVolume(channel, f_SoundVolumes[channel]);
    }
  }

  UpdateCharacterSounds();
  if (f_PendingSongChange.state != SONG_CHANGE_NONE &&
      f_PendingSongChange.delay != 0 && --f_PendingSongChange.delay == 0) {
    if (f_PendingSongChange.state == SONG_CHANGE_SWITCH) {
      if (f_SongHandle != NO_HANDLE) {
        s32 mask = osSetIntMask(OS_IM_NONE);
        MusHandleStop(f_SongHandle, 0);
        osSetIntMask(mask);
        f_SongHandle = NO_HANDLE;
        f_PendingSongChange.delay = 1;
        f_SongId = NO_SONG;
        return;
      }

      f_PendingSongChange.state = SONG_CHANGE_NONE;
      f_SongId = NO_SONG;
      PlaySong(f_PendingSongChange.songId, f_PendingSongChange.flags);
    } else if (f_PendingSongChange.state == SONG_CHANGE_STOP) {
      if (f_SongHandle != NO_HANDLE) {
        s32 mask = osSetIntMask(OS_IM_NONE);
        MusHandleStop(f_SongHandle, 0);
        osSetIntMask(mask);
        f_SongHandle = NO_HANDLE;
        f_PendingSongChange.delay = 0;
      }

      f_PendingSongChange.state = SONG_CHANGE_NONE;
    }
  }
}

void StopSong(s32 fadeFrameCount) {
  s32 mask = osSetIntMask(OS_IM_NONE);

  if (f_SongHandle != NO_HANDLE) {
    MusHandleStop(f_SongHandle, fadeFrameCount);
  }

  osSetIntMask(mask);
}

/* Stops the song on UpdateAudio's next frame */
void RequestSongStop(void) {
  f_PendingSongChange.state = SONG_CHANGE_STOP;
  f_PendingSongChange.delay = 1;
  f_SongId = NO_SONG;
}

void SetSongVolume(SoundVolume volume) {
  s32 mask;

  if (f_SongHandle != NO_HANDLE) {
    mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetVolume(f_SongHandle, volume);
    osSetIntMask(mask);
  }
}

void SetSongReverb(SoundReverb reverb) {
  s32 mask;

  if (f_SongHandle != NO_HANDLE) {
    mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetReverb(f_SongHandle, reverb);
    osSetIntMask(mask);
  }
}

void SetSongTempo(s32 tempo) {
  s32 mask;

  if (f_SongHandle != NO_HANDLE) {
    mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetTempo(f_SongHandle, tempo);
    osSetIntMask(mask);
  }
}

void SetSongFrequencyOffset(f32 offset) {
  s32 mask;

  if (f_SongHandle != NO_HANDLE) {
    mask = osSetIntMask(OS_IM_NONE);
    MusHandleSetFreqOffset(f_SongHandle, offset);
    osSetIntMask(mask);
  }
}

SongId GetSongId(void) {
  return f_SongId;
}

/*
 * Starts a song. While another song plays, or this one with
 * SONG_FLAG_RESTART, has UpdateAudio stop it and start this one instead.
 */
void PlaySong(SongId songId, s32 flags) {
  if (!IsGameFlagSet(GAME_FLAG_SILENCE_SONGS)) {
    if (IsMusicOffChosen() && (flags & SONG_FLAG_UNK10)) {
      RequestSongStop();
      return;
    }

    if (f_SongHandle != NO_HANDLE) {
      if ((flags & SONG_FLAG_RESTART) || f_SongId != songId) {
        f_PendingSongChange.state = SONG_CHANGE_SWITCH;
        f_PendingSongChange.songId = songId;
        f_PendingSongChange.flags = flags;
        f_PendingSongChange.delay = SONG_SWITCH_DELAY;
      }
    } else if (f_PendingSongChange.state == SONG_CHANGE_SWITCH) {
      f_PendingSongChange.songId = songId;
      f_PendingSongChange.flags = flags;
    } else {
      osSyncPrintf("SeqGo %d\n", songId);
      if (!IsMusicOffChosen() || !(flags & SONG_FLAG_UNK10)) {
        f_SongId = songId;
        if (songId > LAST_SONG_ID) {
          songId = LAST_SONG_ID;
        }

        LoadSong(f_SongRomBounds[songId * 2],
            f_SongRomBounds[(songId * 2) + 1] - f_SongRomBounds[songId * 2]);
        MusSetMasterVolume(MUSFLAG_SONGS, SONG_MASTER_VOLUME);
        f_SongHandle = StartLoadedSong();
        f_SongHandleCopy = f_SongHandle;
        if (g_SoundOptionBits & OPTION_BIT_MONO) {
          MusSetStereo(FALSE);
        } else {
          MusSetStereo(TRUE);
        }
      }
    }
  }
}

void InitAudio(void) {
  SoundChannel channel;

  InitMusic(NULL);
  f_PendingSongChange.state = SONG_CHANGE_NONE;
  f_SongHandle = NO_HANDLE;
  f_SongId = NO_SONG;
  LoadSampleBank(g_SoundPointerBankRomStart,
      g_SoundPointerBankRomEnd - g_SoundPointerBankRomStart,
      g_SoundWaveBankRomStart);
  LoadSoundEffectBank(g_SoundEffectBankRomStart,
      g_SoundEffectBankRomEnd - g_SoundEffectBankRomStart);
  for (channel = 0; channel < SOUND_CHANNEL_COUNT; channel++) {
    f_SoundHandles[channel] = 0;
    f_SoundFadeFrameCounts[channel] = 0;
  }

  ChangeCustomEffect(ECHO_EFFECT_INDEX);
}

/* Stand-ins for libaudio's synthesizer, which libmus replaces */
void alSynNew(ALSynth* synth, ALSynConfig* config) {
  osSyncPrintf("alSynNew\n");
}

void alSynDelete(ALSynth* synth) {
  osSyncPrintf("alSynDelete\n");
}

void __freeParam(struct ALParam_s* param) {}

void _freePVoice(ALSynth* synth, struct PVoice_s* voice) {}
