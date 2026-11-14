#include "sound.h"

#include <PR/libmus.h>
#include <ultra64.h>

#define WIDENS_CHOSEN_OPTIONS

#include "character.h"
#include "controllers/controller.h"
#include "debug_text.h"
#include "game_flags.h"
#include "music.h"
#include "save.h"
#include "system/asset.h"
#include "system/compiler.h"
#include "system/memory_address.h"
#include "text/text.h"

#define NO_HANDLE (-1)
#define SOUND_COUNT 0x215
#define LAST_SONG_ID 50
#define ECHO_EFFECT_INDEX 5
#define SONG_MASTER_VOLUME 12000

#define ECHO_REVERB 6
#define DEFAULT_REVERB 2
#define ECHOING_SOUND_COUNT 34

#define QUEUED_CHARACTER_SOUND_COUNT 4
#define CHARACTER_SOUND_DELAY_MAX 60

#define DELAY_EDITOR_CONTROLLER 1
#define DELAY_EDITOR_COLUMN 19
#define DELAY_EDITOR_FIRST_ROW 5
#define DELAY_EDITOR_KIND_ROW 4

enum SongChangeKind {
    SONG_CHANGE_NONE,
    SONG_CHANGE_SWITCH,
    SONG_CHANGE_STOP
};

typedef s32 SongChangeKind;
#define SONG_SWITCH_DELAY 2
#define PLAYED_DELAY (-1)

typedef struct QueuedCharacterSound {
    CharacterIndex characterIndex;
    s32 delay;
    CharacterSoundKind kind;
    SoundVolume volume;
    SoundPan pan;
} QueuedCharacterSound;

typedef struct SongChange {
    SongChangeKind kind;
    SongId songId;
    SongFlags flags;
    s32 delay;
} SongChange;

extern u8 g_SoundPointerBankRomStart[];
extern u8 g_SoundPointerBankRomEnd[];
extern u8 g_SoundWaveBankRomStart[];
extern u8 g_SoundEffectBankRomStart[];
extern u8 g_SoundEffectBankRomEnd[];

SoundChannel g_NextSoundChannel = 0;
static s32 f_SongHandle = NO_HANDLE;
static s32 f_UnusedWordAfterSongHandle UNUSED = -1;
static s32 f_SoundHandles[SOUND_CHANNEL_COUNT] = {NO_HANDLE};
static u32 f_SoundFadeFramesLeft[SOUND_CHANNEL_COUNT] = {0};
static f32 f_SoundVolumes[SOUND_CHANNEL_COUNT] = {0.0f};
static s32 f_SongHandleCopy = NO_HANDLE;
static s32 f_SoundHandleCopies[SOUND_CHANNEL_COUNT] = {NO_HANDLE};
static SoundId f_EchoingSoundIds[ECHOING_SOUND_COUNT] = {
    0x7B,
    0x7C,
    0x7D,
    0x7E,
    0x7F,
    0x80,
    0x81,
    0x82,
    0x83,
    0x84,
    0x85,
    0x86,
    0x87,
    0x88,
    0x89,
    0x8A,
    0x8B,
    0x8C,
    0x8D,
    0x8E,
    0x90,
    0x91,
    0x92,
    0x93,
    0x94,
    0x95,
    0x96,
    0x97,
    0x98,
    0x99,
    0x9A,
    0x9B,
    0x9C,
    0x9D,
};
static u16 f_CharacterSoundDelays[CHARACTER_SOUND_KIND_COUNT][CHARACTER_COUNT] = {
    {4, 3, 4, 4, 4, 6, 4, 3, 4, 4, 7, 6, 6, 4, 4, 4, 1, 4},
};
static s32 f_PreviewSoundDelays[CHARACTER_COUNT] = {
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
    PLAYED_DELAY,
};
static QueuedCharacterSound f_QueuedCharacterSounds[QUEUED_CHARACTER_SOUND_COUNT] = {
    {0, PLAYED_DELAY, 0, 0, 0},
    {0, PLAYED_DELAY, 0, 0, 0},
    {0, PLAYED_DELAY, 0, 0, 0},
    {0, PLAYED_DELAY, 0, 0, 0},
};
static u8 f_CharacterSoundIndexes[CHARACTER_SOUND_KIND_COUNT] = {
    CHARACTER_SOUND_FULL_SHOT,
    CHARACTER_SOUND_EXCELLENT,
    CHARACTER_SOUND_CALL_OUT_RIGHT,
    CHARACTER_SOUND_CALL_OUT_C_UP,
    CHARACTER_SOUND_CALL_OUT_C_DOWN,
    CHARACTER_SOUND_CALL_OUT_C_LEFT,
    CHARACTER_SOUND_CALL_OUT_C_RIGHT,
};
static CharacterIndex f_DelayEditorCharacter = 0;
static CharacterSoundKind f_DelayEditorKind = CHARACTER_SOUND_KIND_FULL_SHOT;
static SongChange f_PendingSongChange = {SONG_CHANGE_NONE, 0, 0, 0};
static SongId f_SongId = NO_SONG;
#define DeclareSongFile(name) extern u8 songs_##name##_ROM_START[], songs_##name##_ROM_END[]
#define SongRomBounds(name) (u32)(songs_##name##_ROM_START), (u32)(songs_##name##_ROM_END)

DeclareSongFile(hole_out_eagle);
DeclareSongFile(hole_out_birdie);
DeclareSongFile(hole_out_par);
DeclareSongFile(results);
DeclareSongFile(hole_out_bogey);
DeclareSongFile(hole_out_over_bogey);
DeclareSongFile(hole_won);
DeclareSongFile(hole_lost);
DeclareSongFile(hole_halved);
DeclareSongFile(song_9);
DeclareSongFile(match_result);
DeclareSongFile(round_tournament);
DeclareSongFile(course_toad_highlands);
DeclareSongFile(course_koopa_park);
DeclareSongFile(cup_in_par);
DeclareSongFile(cup_in_over_par);
DeclareSongFile(cup_in_under_par);
DeclareSongFile(course_shy_guy_desert);
DeclareSongFile(course_boo_valley);
DeclareSongFile(course_yoshis_island);
DeclareSongFile(round_final);
DeclareSongFile(training);
DeclareSongFile(transmit_menu);
DeclareSongFile(controls);
DeclareSongFile(round_speed_golf);
DeclareSongFile(round_match);
DeclareSongFile(round_training);
DeclareSongFile(round_course);
DeclareSongFile(song_28);
DeclareSongFile(round_ring_shot);
DeclareSongFile(ring_shot);
DeclareSongFile(round_replay);
DeclareSongFile(tournament_toad_highlands);
DeclareSongFile(tournament_koopa_park);
DeclareSongFile(tournament_shy_guy_desert);
DeclareSongFile(tournament_boo_valley);
DeclareSongFile(tournament_marios_star);
DeclareSongFile(tournament_results);
DeclareSongFile(speed_golf);
DeclareSongFile(title);
DeclareSongFile(yoshi_and_donkey_kong);
DeclareSongFile(course_marios_star);
DeclareSongFile(attract_title);
DeclareSongFile(round_club_slots);
DeclareSongFile(mini_golf);
DeclareSongFile(round_mini_golf);
DeclareSongFile(round_skins_match);
DeclareSongFile(opening);
DeclareSongFile(tournament_yoshis_island);
DeclareSongFile(staff_roll);
DeclareSongFile(closing);

static u32 f_SongRomBounds[(LAST_SONG_ID + 1) * 2] = {
    SongRomBounds(hole_out_eagle),
    SongRomBounds(hole_out_birdie),
    SongRomBounds(hole_out_par),
    SongRomBounds(results),
    SongRomBounds(hole_out_bogey),
    SongRomBounds(hole_out_over_bogey),
    SongRomBounds(hole_won),
    SongRomBounds(hole_lost),
    SongRomBounds(hole_halved),
    SongRomBounds(song_9),
    SongRomBounds(match_result),
    SongRomBounds(round_tournament),
    SongRomBounds(course_toad_highlands),
    SongRomBounds(course_koopa_park),
    SongRomBounds(cup_in_par),
    SongRomBounds(cup_in_over_par),
    SongRomBounds(cup_in_under_par),
    SongRomBounds(course_shy_guy_desert),
    SongRomBounds(course_boo_valley),
    SongRomBounds(course_yoshis_island),
    SongRomBounds(round_final),
    SongRomBounds(training),
    SongRomBounds(transmit_menu),
    SongRomBounds(controls),
    SongRomBounds(round_speed_golf),
    SongRomBounds(round_match),
    SongRomBounds(round_training),
    SongRomBounds(round_course),
    SongRomBounds(song_28),
    SongRomBounds(round_ring_shot),
    SongRomBounds(ring_shot),
    SongRomBounds(round_replay),
    SongRomBounds(tournament_toad_highlands),
    SongRomBounds(tournament_koopa_park),
    SongRomBounds(tournament_shy_guy_desert),
    SongRomBounds(tournament_boo_valley),
    SongRomBounds(tournament_marios_star),
    SongRomBounds(tournament_results),
    SongRomBounds(speed_golf),
    SongRomBounds(title),
    SongRomBounds(yoshi_and_donkey_kong),
    SongRomBounds(course_marios_star),
    SongRomBounds(attract_title),
    SongRomBounds(round_club_slots),
    SongRomBounds(mini_golf),
    SongRomBounds(round_mini_golf),
    SongRomBounds(round_skins_match),
    SongRomBounds(opening),
    SongRomBounds(tournament_yoshis_island),
    SongRomBounds(staff_roll),
    SongRomBounds(closing),
};
static s32 f_UnusedWordsAfterSongRomBounds[10] UNUSED = {5, 0, 5, 5, 0, 5, 5, 5, 0, 0};
static u32 f_LastAudioFrame;

s32 ChangeCustomEffect(s32 type);

void PlaySound(SoundId soundId, SoundChannel channel, SoundVolume volume) {
    s32 *handle;
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
                }
                else {
                    SetSoundReverb(channel, ECHO_REVERB);
                }

                SetSoundFrequencyOffset(channel, 0.0f);
            }
            else {
                if (soundId == SOUND_ECHOING_SHOT) {
                    SetSoundReverb(channel, ECHO_REVERB);
                }
                else {
                    SetSoundReverb(channel, DEFAULT_REVERB);
                }
            }
        }
    }
}

void PlaySoundWithEffects(
    SoundId soundId,
    SoundChannel channel,
    SoundVolume volume,
    SoundReverb reverb,
    SoundPan pan
) {
    s32 *handle;
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
                }
                else {
                    SetSoundReverb(channel, ECHO_REVERB);
                }

                SetSoundFrequencyOffset(channel, 0.0f);
            }
            else {
                if (soundId == SOUND_ECHOING_SHOT) {
                    SetSoundReverb(channel, ECHO_REVERB);
                }
                else {
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

void FadeOutSound(SoundChannel channel, u32 frameCount) {
    f_SoundFadeFramesLeft[channel] = frameCount;
}

void SetSoundTempo(SoundChannel channel, s32 tempo) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (f_SoundHandles[channel] != NO_HANDLE) {
        MusHandleSetTempo(f_SoundHandles[channel], tempo);
    }

    osSetIntMask(mask);
}

void QueueCharacterSound(
    CharacterIndex characterIndex,
    s32 slot,
    CharacterSoundKind kind,
    SoundVolume volume,
    SoundPan pan
) {
    f_QueuedCharacterSounds[slot].delay = f_CharacterSoundDelays[kind][characterIndex];
    f_QueuedCharacterSounds[slot].characterIndex = characterIndex;
    f_QueuedCharacterSounds[slot].kind = kind;
    f_QueuedCharacterSounds[slot].volume = volume;
    f_QueuedCharacterSounds[slot].pan = pan;
}

static void PreviewCharacterSound(CharacterIndex characterIndex, CharacterSoundKind kind) {
    f_PreviewSoundDelays[characterIndex] = f_CharacterSoundDelays[kind][characterIndex];
}

static void UpdateCharacterSounds(void) {
    s32 index;

    if (!IsGameFlagSet(GAME_FLAG_TIMING_DEBUG)) {
        for (index = 0; index != QUEUED_CHARACTER_SOUND_COUNT; index++) {
            if (f_QueuedCharacterSounds[index].delay == 0) {
                PlayCharacterSound(
                    f_QueuedCharacterSounds[index].characterIndex,
                    index,
                    f_CharacterSoundIndexes[f_QueuedCharacterSounds[index].kind],
                    f_QueuedCharacterSounds[index].volume,
                    f_QueuedCharacterSounds[index].pan
                );
            }

            if (f_QueuedCharacterSounds[index].delay >= 0) {
                f_QueuedCharacterSounds[index].delay--;
            }
        }

        return;
    }

    for (index = 0; index != CHARACTER_COUNT; index++) {
        if (f_PreviewSoundDelays[index] == 0) {
            PlayCharacterSound(
                index,
                index % QUEUED_CHARACTER_SOUND_COUNT,
                f_CharacterSoundIndexes[f_DelayEditorKind],
                SOUND_FULL_SCALE,
                SOUND_FULL_SCALE
            );
        }

        if (f_PreviewSoundDelays[index] >= 0) {
            f_PreviewSoundDelays[index]--;
        }
    }

    if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & L_CBUTTONS) &&
        f_DelayEditorKind > CHARACTER_SOUND_KIND_FULL_SHOT) {
        f_DelayEditorKind--;
    }

    if ((g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & R_CBUTTONS) &&
        f_DelayEditorKind < CHARACTER_SOUND_KIND_COUNT - 1) {
        f_DelayEditorKind++;
    }

    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & R_TRIG) {
        PreviewCharacterSound(f_DelayEditorCharacter % CHARACTER_COUNT, f_DelayEditorKind);
    }

    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & A_BUTTON) {
        PreviewCharacterSound(f_DelayEditorCharacter % CHARACTER_COUNT, f_DelayEditorKind);
        PreviewCharacterSound((f_DelayEditorCharacter + 1) % CHARACTER_COUNT, f_DelayEditorKind);
    }

    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].pressedButtons & B_BUTTON) {
        PreviewCharacterSound(f_DelayEditorCharacter % CHARACTER_COUNT, f_DelayEditorKind);
        PreviewCharacterSound((f_DelayEditorCharacter + 1) % CHARACTER_COUNT, f_DelayEditorKind);
        PreviewCharacterSound((f_DelayEditorCharacter + 2) % CHARACTER_COUNT, f_DelayEditorKind);
        PreviewCharacterSound((f_DelayEditorCharacter + 3) % CHARACTER_COUNT, f_DelayEditorKind);
    }

    PrintDebugText(" ", DELAY_EDITOR_COLUMN, f_DelayEditorCharacter + DELAY_EDITOR_FIRST_ROW);
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
            CHARACTER_SOUND_DELAY_MAX) {
            f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter]++;
        }
    }

    if (g_ControllerStates[DELAY_EDITOR_CONTROLLER].repeatedButtons & L_JPAD) {
        if (f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter] != 0) {
            f_CharacterSoundDelays[f_DelayEditorKind][f_DelayEditorCharacter]--;
        }
    }

    sprintf(g_PrintBuffer, "NO %3d ", (int)f_DelayEditorKind);
    PrintDebugText(g_PrintBuffer, DELAY_EDITOR_COLUMN + 1, DELAY_EDITOR_KIND_ROW);
    for (index = 0; index != CHARACTER_COUNT; index++) {
        sprintf(g_PrintBuffer, "%3d ", f_CharacterSoundDelays[f_DelayEditorKind][index]);
        PrintDebugText(g_PrintBuffer, DELAY_EDITOR_COLUMN + 1, index + DELAY_EDITOR_FIRST_ROW);
    }

    PrintDebugText(">", DELAY_EDITOR_COLUMN, f_DelayEditorCharacter + DELAY_EDITOR_FIRST_ROW);
}

void SetSoundFrequencyOffset(SoundChannel channel, f32 offset) {
    s32 mask = osSetIntMask(OS_IM_NONE);

    if (f_SoundHandles[channel] != NO_HANDLE) {
        MusHandleSetFreqOffset(f_SoundHandles[channel], offset);
    }

    osSetIntMask(mask);
}

NO_RESULT UpdateAudio(void) {
    u32 audioFrame;
    u32 fadeFramesLeft;
    SoundChannel channel;

    audioFrame = MusGetFrameCount();
    if (audioFrame == f_LastAudioFrame) {
        osSyncPrintf(
            "Ouch!%d,%d,%d,%d!!!!!!!!!!!!!!!!\n",
            f_PendingSongChange.kind,
            f_PendingSongChange.songId,
            f_PendingSongChange.flags,
            f_PendingSongChange.delay
        );
        return;
    }

    f_LastAudioFrame = audioFrame;
    for (channel = 0; channel != SOUND_CHANNEL_COUNT; channel++) {
        fadeFramesLeft = f_SoundFadeFramesLeft[channel];
        if (fadeFramesLeft != 0) {
            f_SoundFadeFramesLeft[channel] = fadeFramesLeft - 1;
            f_SoundVolumes[channel] =
                f_SoundVolumes[channel] * (f32)(fadeFramesLeft - 1) / (f32)fadeFramesLeft;
            SetSoundVolume(channel, f_SoundVolumes[channel]);
        }
    }

    UpdateCharacterSounds();
    if (f_PendingSongChange.kind != SONG_CHANGE_NONE &&
        f_PendingSongChange.delay != 0 &&
        --f_PendingSongChange.delay == 0) {
        if (f_PendingSongChange.kind == SONG_CHANGE_SWITCH) {
            if (f_SongHandle != NO_HANDLE) {
                s32 mask = osSetIntMask(OS_IM_NONE);
                MusHandleStop(f_SongHandle, 0);
                osSetIntMask(mask);
                f_SongHandle = NO_HANDLE;
                f_PendingSongChange.delay = 1;
                f_SongId = NO_SONG;
                return;
            }

            f_PendingSongChange.kind = SONG_CHANGE_NONE;
            f_SongId = NO_SONG;
            PlaySong(f_PendingSongChange.songId, f_PendingSongChange.flags);
        }
        else if (f_PendingSongChange.kind == SONG_CHANGE_STOP) {
            if (f_SongHandle != NO_HANDLE) {
                s32 mask = osSetIntMask(OS_IM_NONE);
                MusHandleStop(f_SongHandle, 0);
                osSetIntMask(mask);
                f_SongHandle = NO_HANDLE;
                f_PendingSongChange.delay = 0;
            }

            f_PendingSongChange.kind = SONG_CHANGE_NONE;
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

void RequestSongStop(void) {
    f_PendingSongChange.kind = SONG_CHANGE_STOP;
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

void PlaySong(SongId songId, SongFlags songFlags) {
    if (!IsGameFlagSet(GAME_FLAG_SILENCE_SONGS)) {
        if (IsMusicOffChosen() && (songFlags & SONG_FLAG_FOLLOWS_MUSIC_OPTION)) {
            RequestSongStop();
            return;
        }

        if (f_SongHandle != NO_HANDLE) {
            if ((songFlags & SONG_FLAG_RESTART) || f_SongId != songId) {
                f_PendingSongChange.kind = SONG_CHANGE_SWITCH;
                f_PendingSongChange.songId = songId;
                f_PendingSongChange.flags = songFlags;
                f_PendingSongChange.delay = SONG_SWITCH_DELAY;
            }
        }
        else if (f_PendingSongChange.kind == SONG_CHANGE_SWITCH) {
            f_PendingSongChange.songId = songId;
            f_PendingSongChange.flags = songFlags;
        }
        else {
            osSyncPrintf("SeqGo %d\n", songId);
            if (!IsMusicOffChosen() || !(songFlags & SONG_FLAG_FOLLOWS_MUSIC_OPTION)) {
                f_SongId = songId;
                if (songId > LAST_SONG_ID) {
                    songId = LAST_SONG_ID;
                }

                LoadSong(
                    f_SongRomBounds[songId * 2],
                    f_SongRomBounds[(songId * 2) + 1] - f_SongRomBounds[songId * 2]
                );
                MusSetMasterVolume(MUSFLAG_SONGS, SONG_MASTER_VOLUME);
                f_SongHandle = StartLoadedSong();
                f_SongHandleCopy = f_SongHandle;
                if (g_SoundOptionBits & OPTION_BIT_MONO) {
                    MusSetStereo(FALSE);
                }
                else {
                    MusSetStereo(TRUE);
                }
            }
        }
    }
}

void InitAudio(void) {
    SoundChannel channel;

    InitMusic(NULL);
    f_PendingSongChange.kind = SONG_CHANGE_NONE;
    f_SongHandle = NO_HANDLE;
    f_SongId = NO_SONG;
    LoadSampleBank(
        g_SoundPointerBankRomStart,
        BytesBetween(g_SoundPointerBankRomStart, g_SoundPointerBankRomEnd),
        g_SoundWaveBankRomStart
    );
    LoadSoundEffectBank(
        g_SoundEffectBankRomStart,
        BytesBetween(g_SoundEffectBankRomStart, g_SoundEffectBankRomEnd)
    );
    for (channel = 0; channel < SOUND_CHANNEL_COUNT; channel++) {
        f_SoundHandles[channel] = 0;
        f_SoundFadeFramesLeft[channel] = 0;
    }

    ChangeCustomEffect(ECHO_EFFECT_INDEX);
}

void alSynNew(ALSynth *synth UNUSED, ALSynConfig *config UNUSED) {
    osSyncPrintf("alSynNew\n");
}

void alSynDelete(ALSynth *synth UNUSED) {
    osSyncPrintf("alSynDelete\n");
}

void __freeParam(struct ALParam_s *param UNUSED) {
}

void _freePVoice(ALSynth *synth UNUSED, struct PVoice_s *voice UNUSED) {
}
