#include "common.h"
#include "sound.h"
#include "controller.h"
#include "functions.h"
#include "game_flags.h"
#include "save.h"
#include "variables.h"
#include <PR/libmus.h>

/*
 * Sound effects, songs and the character sounds that wait for an
 * animation, all played through libmus, with the interrupts masked around
 * each libmus call that changes a sound.
 */

/* A channel's or the song's handle when there is nothing to control */
#define NO_HANDLE -1
/* sSongId when no song is playing */
#define NO_SONG -1
/* Sound IDs are below this */
#define SOUND_COUNT 0x215
/* Song IDs above this play the last song */
#define LAST_SONG_ID 50
/* The largest volume or pan scale, which keeps nearly all of the sound's own value */
#define FULL_SCALE 0x7F
/* The echo in libmus's list of effects */
#define ECHO_EFFECT_INDEX 5
/* libmus's master volume for songs */
#define SONG_MASTER_VOLUME 12000

/* The reverb of sound effects */
#define ECHO_REVERB 6
#define DEFAULT_REVERB 2
#define NO_REVERB 0
#define ECHOING_SOUND_COUNT 34
#define SOUND_ID_UNK5A 0x5A
#define SOUND_ID_UNK9A 0x9A

/* Character sounds, which play after a delay set for each character */
#define CHARACTER_COUNT 18
#define CHARACTER_SOUND_KIND_COUNT 7
#define QUEUED_CHARACTER_SOUND_COUNT 4
#define MAX_CHARACTER_SOUND_DELAY 60

/* The delay editor, on the second controller with GAME_FLAG_UNK5F set */
#define DELAY_EDITOR_CONTROLLER 1
#define DELAY_EDITOR_COLUMN 19
#define DELAY_EDITOR_FIRST_ROW 5
#define DELAY_EDITOR_KIND_ROW 4

/* sPendingSongChange's states */
#define SONG_CHANGE_NONE 0
#define SONG_CHANGE_SWITCH 1
#define SONG_CHANGE_STOP 2
/* Frames from a switch's request to the old song's stop */
#define SONG_SWITCH_DELAY 2

typedef struct QueuedCharacterSound {
    /* 0x00 */ s32 character;
    /* 0x04 */ s32 delay; /* Frames until it plays; negative once played */
    /* 0x08 */ s32 kind;
    /* 0x0C */ s32 volume;
    /* 0x10 */ s32 pan;
} QueuedCharacterSound; /* size = 0x14 */

/* A song change for UpdateAudio to make once its delay runs out */
typedef struct SongChange {
    /* 0x00 */ s32 state;
    /* 0x04 */ s32 songId;
    /* 0x08 */ s32 flags;
    /* 0x0C */ s32 delay;
} SongChange; /* size = 0x10 */

extern u8 g_SoundOptionBits;
extern u8 g_SoundPointerBankRomStart[];
extern u8 g_SoundPointerBankRomEnd[];
extern u8 g_SoundWaveBankRomStart[];
extern u8 g_SoundEffectBankRomStart[];
extern u8 g_SoundEffectBankRomEnd[];

extern s32 sSongHandle;
extern s32 sSoundHandles[SOUND_CHANNEL_COUNT];
extern u32 sSoundFadeFrameCounts[SOUND_CHANNEL_COUNT];
extern f32 sSoundVolumes[SOUND_CHANNEL_COUNT];
/* Written with the handles but never read */
extern s32 sSongHandleCopy;
extern s32 sSoundHandleCopies[SOUND_CHANNEL_COUNT];
extern s32 sEchoingSoundIds[ECHOING_SOUND_COUNT];
/* Frames from a character sound's request until it plays, by kind and character */
extern u16 sCharacterSoundDelays[CHARACTER_SOUND_KIND_COUNT][CHARACTER_COUNT];
extern s32 sPreviewSoundDelays[CHARACTER_COUNT];
extern QueuedCharacterSound sQueuedCharacterSounds[QUEUED_CHARACTER_SOUND_COUNT];
/* The index in each character's sound table of each kind of sound */
extern u8 sCharacterSoundIndexes[CHARACTER_SOUND_KIND_COUNT];
extern s32 sDelayEditorCharacter;
extern s32 sDelayEditorKind;
extern SongChange sPendingSongChange;
extern s32 sSongId;
/* The start and end of each song in the ROM, in turn */
extern u32 sSongRomBounds[(LAST_SONG_ID + 1) * 2];
extern u32 sLastAudioFrame;

void PlayCharacterSound(s32 character, s32 slot, s32 soundIndex, s32 volume, s32 pan);
musHandle StartSoundEffect(s32 soundId);
musHandle StartLoadedSong(void);
void LoadSong(u32 romAddress, u32 size);
void LoadSampleBank(u8 *pointerBankRom, u32 pointerBankSize, u8 *waveBankRom);
void LoadSoundEffectBank(u8 *rom, u32 size);
void func_8005ED8C(s32);
s32 func_8005DE00(void);
s32 ChangeCustomEffect(s32 type);

void PlaySound(s32 soundId, s32 channel, s32 volume) {
    s32 *handle;
    s32 mask;
    s32 index;

    if (!IsGameFlagSet(GAME_FLAG_SILENCE_SOUND_EFFECTS)) {
        mask = osSetIntMask(OS_IM_NONE);
        sSoundVolumes[channel] = volume;
        if (soundId < SOUND_COUNT) {
            handle = &sSoundHandles[channel];
            if (*handle != NO_HANDLE) {
                MusHandleStop(*handle, 0);
                sSoundHandleCopies[channel] = *handle = StartSoundEffect(soundId);
                MusHandleSetVolume(*handle, volume);
                MusHandleSetFreqOffset(*handle, 0.0f);
                MusHandleSetPan(*handle, FULL_SCALE);
            }
            if (g_NextSoundChannel == channel) {
                g_NextSoundChannel = channel ^ 1;
            }
        }
        osSetIntMask(mask);
        if (soundId < SOUND_COUNT) {
            for (index = 0; index != ECHOING_SOUND_COUNT; index++) {
                if (sEchoingSoundIds[index] == soundId) {
                    break;
                }
            }
            if (index < ECHOING_SOUND_COUNT) {
                if (sEchoingSoundIds[index] == SOUND_ID_UNK9A) {
                    SetSoundReverb(channel, NO_REVERB);
                } else {
                    SetSoundReverb(channel, ECHO_REVERB);
                }
                SetSoundFrequencyOffset(channel, 0.0f);
            } else {
                if (soundId == SOUND_ID_UNK5A) {
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
void PlaySoundWithEffects(s32 soundId, s32 channel, s32 volume, s32 reverb, s32 pan) {
    s32 *handle;
    s32 mask;
    s32 index;

    if (!IsGameFlagSet(GAME_FLAG_SILENCE_SOUND_EFFECTS)) {
        mask = osSetIntMask(OS_IM_NONE);
        sSoundVolumes[channel] = volume;
        if (soundId < SOUND_COUNT) {
            handle = &sSoundHandles[channel];
            if (*handle != NO_HANDLE) {
                MusHandleStop(*handle, 0);
                MusHandleSetReverb(*handle, reverb);
            }
            sSoundHandleCopies[channel] = *handle = StartSoundEffect(soundId);
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
                if (sEchoingSoundIds[index] == soundId) {
                    break;
                }
            }
            if (index < ECHOING_SOUND_COUNT) {
                if (sEchoingSoundIds[index] == SOUND_ID_UNK9A) {
                    SetSoundReverb(channel, NO_REVERB);
                } else {
                    SetSoundReverb(channel, ECHO_REVERB);
                }
                SetSoundFrequencyOffset(channel, 0.0f);
            } else {
                if (soundId == SOUND_ID_UNK5A) {
                    SetSoundReverb(channel, ECHO_REVERB);
                } else {
                    SetSoundReverb(channel, DEFAULT_REVERB);
                }
            }
        }
    }
}

void StopSound(s32 channel) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSoundHandles[channel] != NO_HANDLE) {
        MusHandleStop(sSoundHandles[channel], 0);
    }
    osSetIntMask(mask);
}

void SetSoundPan(s32 channel, s32 pan) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSoundHandles[channel] != NO_HANDLE) {
        MusHandleSetPan(sSoundHandles[channel], pan);
    }
    osSetIntMask(mask);
}

void SetSoundVolume(s32 channel, s32 volume) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSoundHandles[channel] != NO_HANDLE) {
        MusHandleSetVolume(sSoundHandles[channel], volume);
    }
    osSetIntMask(mask);
}

void SetSoundReverb(s32 channel, s32 reverb) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSoundHandles[channel] != NO_HANDLE) {
        MusHandleSetReverb(sSoundHandles[channel], reverb);
    }
    osSetIntMask(mask);
}

/* UpdateAudio lowers the volume to nothing over the frames */
void FadeOutSound(s32 channel, u32 frameCount) {
    sSoundFadeFrameCounts[channel] = frameCount;
}

void SetSoundTempo(s32 channel, s32 tempo) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSoundHandles[channel] != NO_HANDLE) {
        MusHandleSetTempo(sSoundHandles[channel], tempo);
    }
    osSetIntMask(mask);
}

void QueueCharacterSound(s32 character, s32 slot, s32 kind, s32 volume, s32 pan) {
    sQueuedCharacterSounds[slot].delay = sCharacterSoundDelays[kind][character];
    sQueuedCharacterSounds[slot].character = character;
    sQueuedCharacterSounds[slot].kind = kind;
    sQueuedCharacterSounds[slot].volume = volume;
    sQueuedCharacterSounds[slot].pan = pan;
}

static void PreviewCharacterSound(s32 character, s32 kind) {
    sPreviewSoundDelays[character] = sCharacterSoundDelays[kind][character];
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
            if (sQueuedCharacterSounds[index].delay == 0) {
                PlayCharacterSound(sQueuedCharacterSounds[index].character, index,
                                   sCharacterSoundIndexes[sQueuedCharacterSounds[index].kind],
                                   sQueuedCharacterSounds[index].volume, sQueuedCharacterSounds[index].pan);
            }
            if (sQueuedCharacterSounds[index].delay >= 0) {
                sQueuedCharacterSounds[index].delay--;
            }
        }
        return;
    }
    for (index = 0; index != CHARACTER_COUNT; index++) {
        if (sPreviewSoundDelays[index] == 0) {
            PlayCharacterSound(index, index % QUEUED_CHARACTER_SOUND_COUNT, sCharacterSoundIndexes[sDelayEditorKind],
                               FULL_SCALE, FULL_SCALE);
        }
        if (sPreviewSoundDelays[index] >= 0) {
            sPreviewSoundDelays[index]--;
        }
    }
    if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & L_CBUTTONS) && sDelayEditorKind > 0) {
        sDelayEditorKind--;
    }
    if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & R_CBUTTONS) &&
        sDelayEditorKind < CHARACTER_SOUND_KIND_COUNT - 1) {
        sDelayEditorKind++;
    }
    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & R_TRIG) {
        PreviewCharacterSound(sDelayEditorCharacter % CHARACTER_COUNT, sDelayEditorKind);
    }
    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & A_BUTTON) {
        PreviewCharacterSound(sDelayEditorCharacter % CHARACTER_COUNT, sDelayEditorKind);
        PreviewCharacterSound((sDelayEditorCharacter + 1) % CHARACTER_COUNT, sDelayEditorKind);
    }
    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & B_BUTTON) {
        PreviewCharacterSound(sDelayEditorCharacter % CHARACTER_COUNT, sDelayEditorKind);
        PreviewCharacterSound((sDelayEditorCharacter + 1) % CHARACTER_COUNT, sDelayEditorKind);
        PreviewCharacterSound((sDelayEditorCharacter + 2) % CHARACTER_COUNT, sDelayEditorKind);
        PreviewCharacterSound((sDelayEditorCharacter + 3) % CHARACTER_COUNT, sDelayEditorKind);
    }
    func_8004DA24(" ", DELAY_EDITOR_COLUMN, sDelayEditorCharacter + DELAY_EDITOR_FIRST_ROW);
    if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & U_JPAD) && sDelayEditorCharacter > 0) {
        sDelayEditorCharacter--;
    }
    if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & D_JPAD) &&
        sDelayEditorCharacter < CHARACTER_COUNT - 1) {
        sDelayEditorCharacter++;
    }
    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & R_JPAD) {
        if (sCharacterSoundDelays[sDelayEditorKind][sDelayEditorCharacter] < MAX_CHARACTER_SOUND_DELAY) {
            sCharacterSoundDelays[sDelayEditorKind][sDelayEditorCharacter]++;
        }
    }
    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & L_JPAD) {
        if (sCharacterSoundDelays[sDelayEditorKind][sDelayEditorCharacter] != 0) {
            sCharacterSoundDelays[sDelayEditorKind][sDelayEditorCharacter]--;
        }
    }
    sprintf(g_PrintBuffer, "NO %3d ", sDelayEditorKind);
    func_8004DA24(g_PrintBuffer, DELAY_EDITOR_COLUMN + 1, DELAY_EDITOR_KIND_ROW);
    for (index = 0; index != CHARACTER_COUNT; index++) {
        sprintf(g_PrintBuffer, "%3d ", sCharacterSoundDelays[sDelayEditorKind][index]);
        func_8004DA24(g_PrintBuffer, DELAY_EDITOR_COLUMN + 1, index + DELAY_EDITOR_FIRST_ROW);
    }
    func_8004DA24(">", DELAY_EDITOR_COLUMN, sDelayEditorCharacter + DELAY_EDITOR_FIRST_ROW);
}

void SetSoundFrequencyOffset(s32 channel, f32 offset) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSoundHandles[channel] != NO_HANDLE) {
        MusHandleSetFreqOffset(sSoundHandles[channel], offset);
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
    s32 channel;

    frame = MusGetFrameCount();
    if (frame == sLastAudioFrame) {
        osSyncPrintf("Ouch!%d,%d,%d,%d!!!!!!!!!!!!!!!!\n", sPendingSongChange.state, sPendingSongChange.songId,
                     sPendingSongChange.flags, sPendingSongChange.delay);
        return;
    }
    sLastAudioFrame = frame;
    for (channel = 0; channel != SOUND_CHANNEL_COUNT; channel++) {
        frameCount = sSoundFadeFrameCounts[channel];
        if (frameCount != 0) {
            sSoundFadeFrameCounts[channel] = frameCount - 1;
            sSoundVolumes[channel] = sSoundVolumes[channel] * (f32)(frameCount - 1) / (f32)frameCount;
            SetSoundVolume(channel, sSoundVolumes[channel]);
        }
    }
    UpdateCharacterSounds();
    if (sPendingSongChange.state != SONG_CHANGE_NONE && sPendingSongChange.delay != 0 &&
        --sPendingSongChange.delay == 0) {
        if (sPendingSongChange.state == SONG_CHANGE_SWITCH) {
            if (sSongHandle != NO_HANDLE) {
                s32 mask = osSetIntMask(OS_IM_NONE);
                MusHandleStop(sSongHandle, 0);
                osSetIntMask(mask);
                sSongHandle = NO_HANDLE;
                sPendingSongChange.delay = 1;
                sSongId = NO_SONG;
                return;
            }
            sPendingSongChange.state = SONG_CHANGE_NONE;
            sSongId = NO_SONG;
            PlaySong(sPendingSongChange.songId, sPendingSongChange.flags);
        } else if (sPendingSongChange.state == SONG_CHANGE_STOP) {
            if (sSongHandle != NO_HANDLE) {
                s32 mask = osSetIntMask(OS_IM_NONE);
                MusHandleStop(sSongHandle, 0);
                osSetIntMask(mask);
                sSongHandle = NO_HANDLE;
                sPendingSongChange.delay = 0;
            }
            sPendingSongChange.state = SONG_CHANGE_NONE;
        }
    }
}

void StopSong(s32 fadeFrameCount) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (sSongHandle != NO_HANDLE) {
        MusHandleStop(sSongHandle, fadeFrameCount);
    }
    osSetIntMask(mask);
}

/* Stops the song on UpdateAudio's next frame */
void RequestSongStop(void) {
    sPendingSongChange.state = SONG_CHANGE_STOP;
    sPendingSongChange.delay = 1;
    sSongId = NO_SONG;
}

void SetSongVolume(s32 volume) {
    s32 mask;

    if (sSongHandle != NO_HANDLE) {
        mask = osSetIntMask(OS_IM_NONE);
        MusHandleSetVolume(sSongHandle, volume);
        osSetIntMask(mask);
    }
}

void SetSongReverb(s32 reverb) {
    s32 mask;

    if (sSongHandle != NO_HANDLE) {
        mask = osSetIntMask(OS_IM_NONE);
        MusHandleSetReverb(sSongHandle, reverb);
        osSetIntMask(mask);
    }
}

void SetSongTempo(s32 tempo) {
    s32 mask;

    if (sSongHandle != NO_HANDLE) {
        mask = osSetIntMask(OS_IM_NONE);
        MusHandleSetTempo(sSongHandle, tempo);
        osSetIntMask(mask);
    }
}

void SetSongFrequencyOffset(f32 offset) {
    s32 mask;

    if (sSongHandle != NO_HANDLE) {
        mask = osSetIntMask(OS_IM_NONE);
        MusHandleSetFreqOffset(sSongHandle, offset);
        osSetIntMask(mask);
    }
}

s32 GetSongId(void) {
    return sSongId;
}

/*
 * Starts a song. While another song plays, or this one with
 * SONG_FLAG_RESTART, has UpdateAudio stop it and start this one instead.
 */
void PlaySong(s32 songId, s32 flags) {

    if (!IsGameFlagSet(GAME_FLAG_SILENCE_SONGS)) {
        if (func_8005DE00() && (flags & SONG_FLAG_UNK10)) {
            RequestSongStop();
            return;
        }
        if (sSongHandle != NO_HANDLE) {
            if ((flags & SONG_FLAG_RESTART) || sSongId != songId) {
                sPendingSongChange.state = SONG_CHANGE_SWITCH;
                sPendingSongChange.songId = songId;
                sPendingSongChange.flags = flags;
                sPendingSongChange.delay = SONG_SWITCH_DELAY;
            }
        } else if (sPendingSongChange.state == SONG_CHANGE_SWITCH) {
            sPendingSongChange.songId = songId;
            sPendingSongChange.flags = flags;
        } else {
            osSyncPrintf("SeqGo %d\n", songId);
            if (!func_8005DE00() || !(flags & SONG_FLAG_UNK10)) {
                sSongId = songId;
                if (songId > LAST_SONG_ID) {
                    songId = LAST_SONG_ID;
                }
                LoadSong(sSongRomBounds[songId * 2], sSongRomBounds[songId * 2 + 1] - sSongRomBounds[songId * 2]);
                MusSetMasterVolume(MUSFLAG_SONGS, SONG_MASTER_VOLUME);
                sSongHandleCopy = sSongHandle = StartLoadedSong();
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
    s32 channel;

    func_8005ED8C(0);
    sPendingSongChange.state = SONG_CHANGE_NONE;
    sSongHandle = NO_HANDLE;
    sSongId = NO_SONG;
    LoadSampleBank(g_SoundPointerBankRomStart, g_SoundPointerBankRomEnd - g_SoundPointerBankRomStart,
                   g_SoundWaveBankRomStart);
    LoadSoundEffectBank(g_SoundEffectBankRomStart, g_SoundEffectBankRomEnd - g_SoundEffectBankRomStart);
    for (channel = 0; channel < SOUND_CHANNEL_COUNT; channel++) {
        sSoundHandles[channel] = 0;
        sSoundFadeFrameCounts[channel] = 0;
    }
    ChangeCustomEffect(ECHO_EFFECT_INDEX);
}

/* Stand-ins for libaudio's synthesizer, which libmus replaces */
void alSynNew(ALSynth *synth, ALSynConfig *config) {
    osSyncPrintf("alSynNew\n");
}

void alSynDelete(ALSynth *synth) {
    osSyncPrintf("alSynDelete\n");
}

void __freeParam(struct ALParam_s *param) {
}

void _freePVoice(ALSynth *synth, struct PVoice_s *voice) {
}
