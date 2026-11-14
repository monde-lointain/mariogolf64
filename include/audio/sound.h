/*
 * Sound effects, songs and characters' voices: their IDs, the channels they
 * play on, and the routines of sound.c that play and change them.
 */

#ifndef SOUND_H
#define SOUND_H

#include <ultra64.h>

#include "characters/character.h"
#include "system/compiler.h"

// Sound effects play on channels, each holding one effect at a time
#define SOUND_CHANNEL_COUNT 16

// The largest volume or pan scale: libmus keeps a sound's own value at 0x80,
// and nearly all of it at this
#define SOUND_FULL_SCALE 0x7F
#define NO_REVERB 0
#define SOUND_PAN_LEFT 0
#define SOUND_PAN_RIGHT 0xFF

// An enum Sound value
typedef s32 SoundId;
// An enum Song value, or NO_SONG
typedef s32 SongId;
// From 0 to SOUND_CHANNEL_COUNT - 1
typedef s32 SoundChannel;
// How loud a sound plays, from 0 to SOUND_FULL_SCALE
typedef s32 SoundVolume;
// Where a sound plays, from 0 at the left to SOUND_FULL_SCALE at the right
typedef s32 SoundPan;
// How much echo a sound gets, from 0 to SOUND_FULL_SCALE
typedef s32 SoundReverb;

// The menus' sound channel, and how loud each of their sounds plays
#define MENU_SOUND_CHANNEL 15
#define MENU_VOLUME_SOFT 80
#define MENU_VOLUME_MEDIUM 100

// Character sounds play on channels from this one, one for each slot
#define FIRST_CHARACTER_SOUND_CHANNEL 4
// The channel of the club meeting the ball
#define CONTACT_SOUND_CHANNEL 2
// The channel of the hole editor and the opening camera editor
#define EDITOR_SOUND_CHANNEL 15

// The sound effects that PlaySound and PlaySoundWithEffects play. Many play
// in more than one place; each is named for one use, and the comments name
// the others.
enum Sound {
    SOUND_LAND_FAIRWAY = 2, // Also for the semi-rough and in the opening
    SOUND_LAND_ROUGH,
    SOUND_LAND_SAND, // Bunkers and bare ground
    SOUND_LAND_GREEN, // Also played in the opening
    // Hole objects' splash; also the opening camera editor's choice sound
    SOUND_SPLASH = 8,
    SOUND_LAND_WATER,
    // The ball's hits on billboards, by their BillboardCell
    SOUND_HIT_TRUNK = 0xA,
    // Also played four times in the opening, each lower in pitch
    SOUND_HIT_FOLIAGE,
    SOUND_HIT_SOLID,
    SOUND_HIT_FLAGSTICK = 0xE,
    SOUND_HIT_FLAG,
    SOUND_HIT_CUP_WALL,
    // For a ball that drops fast onto the bottom of the cup
    SOUND_HIT_CUP_BOTTOM,
    // For a ball that comes into the cup slowly enough to stay
    SOUND_CUP_IN,
    SOUND_ROLL = 0x14, // Also played in the opening
    SOUND_HIT_WALL,
    // After an excellent shot, a near approach or a chip-in
    SOUND_GOOD_SHOT = 0x17,
    SOUND_FLAG_SWISH = 0x1F, // Also played in the opening
    SOUND_TICK, // A tally's tick and each click of the club slots' reels
    SOUND_REEL_STOP,
    // Also the club slots' turn and a sound of the scorecard
    SOUND_TALLY_DONE,
    SOUND_START_LIGHT_RED,
    SOUND_START_LIGHT_GO,
    // The loop played while the stick sets the impact point
    SOUND_IMPACT_POINT = 0x26,
    // SOUND_FALLING_WHISTLE and SOUND_BOING play in bursts as the opening's
    // Lakitu flies; SOUND_BOING also with the opening's Yoshi or Donkey Kong
    SOUND_FALLING_WHISTLE,
    SOUND_WIND_FLY_GUY, // Also played in the opening
    SOUND_HIT_SHELL,
    SOUND_GOOMBA = 0x2A, // Also the Monty Mole's and in the opening
    SOUND_MONKEY = 0x2C,
    SOUND_CONTACT_MARIO = 0x2E, // Also Bowser's
    SOUND_CONTACT_RAINBOW,
    SOUND_CONTACT_WARIO, // Also played when the game halts on a fault
    // Played a second before a firework launches
    SOUND_FIREWORK_LEAD_IN,
    SOUND_FIREWORK_LAUNCH,
    // As the opening's burst appears
    SOUND_BURST = 0x34,
    SOUND_YOSHI_EAGLE_SECOND_EFFECT, // Also played in the opening
    // In the opening, before SOUND_LUIGI_OH_NO or SOUND_SHORT_OH
    SOUND_CHIME,
    SOUND_METAL_MARIO_EAGLE_EFFECT = 0x38,
    SOUND_WARIO_BOGEY_EFFECT,
    // With each spreading circle of Bowser's bogey reaction
    SOUND_SPREADING_CIRCLE,
    // Also played by the save prompt and late in some eagle reactions
    SOUND_SAVED,
    // Also in Metal Mario's, Sherry's and Wario's eagles
    SOUND_PEACH_BIRDIE_EFFECT,
    SOUND_MAPLE_EAGLE_EFFECT,
    SOUND_YOSHI_EAGLE_EFFECT,
    SOUND_CHARLIE_BIRDIE_EFFECT,
    // Over and over in his reactions and the opening
    SOUND_DONKEY_KONG_REPEATED = 0x41,
    SOUND_DONKEY_KONG_EAGLE_EFFECT,
    // Also the shine across a course notice's title or a trophy
    SOUND_SHRINKING_IMAGE,
    SOUND_BANNER,
    SOUND_SLICE, // Also played when the holes of a round are chosen
    // The opening camera editor's, on loading or deleting a snapshot
    SOUND_SNAPSHOT_LOAD,
    // The scorecard and the save menu opening and closing
    SOUND_PANEL_OPEN,
    SOUND_PANEL_CLOSE,
    // Also played when a contest's winner appears, an erasure is declined and
    // a tournament is declined
    SOUND_READY,
    // Also played when a contest's winner vanishes, the erase window opens and
    // a club tournament code is accepted
    SOUND_GO,
    SOUND_CAPTION_MEET, // Also played when the save data is erased
    SOUND_BOING = 0x4D,
    // A tone that falls about two and a half octaves as it fades. The opening
    // plays it in steps across the pan
    SOUND_BLOOP,
    // Over and over in her reactions and the opening
    SOUND_YOSHI_REPEATED,
    // Also played in the opening, and as a wood meets the ball off a tee or
    // on some power shots
    SOUND_MENU_START,
    // A ball being struck by a plain club or bouncing on hard ground
    SOUND_CONTACT_PLAIN,
    // The rest of the sounds of a club meeting the ball
    SOUND_CONTACT_WOOD,
    SOUND_CONTACT_IRON_TEE,
    SOUND_ENTER_TOURNAMENT, // Also an iron meeting the ball
    SOUND_CONTACT_IRON_BUNKER = 0x56,
    SOUND_CONTACT_PUTTER,
    // As each of the opening's shots launches its sparks; also early in a
    // long shot's flight, under some of the flight cameras
    SOUND_SHOT_SPARKS = 0x59,
    // A little later in a long shot's flight, under the flight cameras that
    // don't play SOUND_SHOT_SPARKS; always played with an echo
    SOUND_ECHOING_SHOT,
    // Also played when a scorecard row flips, the club changes, and the opening
    // camera editor's snapshot cursor moves
    SOUND_LOOK_DOWN_RANGE,
    SOUND_MENU_MOVE, // Also the prompts' cursor and the scorecard's pages
    SOUND_MENU_CONFIRM, // The sound of confirming a choice
    // Played when B can't switch to a power shot
    SOUND_POWER_SHOT_REFUSED,
    SOUND_MENU_CANCEL,
    SOUND_LOOK_AT_CUP, // Also played in the opening
    SOUND_LOOK_BACK, // Also a sound of the scorecard
    // The swing meter's click for an impact on the mark; also a ring passed
    // through, the Nintendo logo's sound and the opening camera editor's
    // snapshot save
    SOUND_IMPACT_ON_MARK,
    // A menu choice that cannot be made, or a cursor that cannot move that way.
    // Also a mishit impact and an incorrect club tournament code.
    SOUND_MENU_BLOCKED,
    // Also the inset view opening, a wipe closing, the aim camera lowering and
    // the club slots' reels starting
    SOUND_PROMPT_OPEN,
    // Also the inset view closing, a wipe opening and the aim camera rising
    SOUND_PROMPT_CLOSE,
    SOUND_PAUSE, // Also the debug scene menu's opening
    SOUND_THREE_STARS, // Also a sound of the scorecard and of a shot's result
    SOUND_UNLOCK_EVERYTHING = 0x69, // Also as a course notice swings in
    // As the course select's brick wall slides out for the bet settings
    SOUND_WALL_SLIDE_OUT,
    SOUND_SKIN_COUNTED,
    SOUND_TEXT_TYPING,
    // The swing meter's click for an impact off the mark
    SOUND_IMPACT_OFF_MARK = 0x6E,
    SOUND_OPTION_TOGGLED,
    SOUND_LONG_SHOT,
    SOUND_SPARK_TRAIL,
    SOUND_CONTROLS_OPEN,
    // Also played when the main menu shows the transmit option
    SOUND_CONTROLS_CLOSE,
    // As a shot drops for a birdie
    SOUND_BIRDIE_CUP_IN,
    // As a ball lands where holing the next stroke beats par: on the green for
    // a birdie or better, or on the fairway after a full-length shot toward the
    // cup for an eagle or better
    SOUND_UNDER_PAR_CHANCE,
    // As a shot drops for an eagle or better; SOUND_EAGLE_CUP_IN,
    // SOUND_CALL_BIRDIE and SOUND_PEACH_CALL_OUT_C_LEFT also play in the audio
    // test
    SOUND_EAGLE_CUP_IN,
    SOUND_OUT_OF_BOUNDS, // Also played in the opening
    SOUND_FULL_SHOT = 0x7B,
    SOUND_TROUBLE_LANDING, // In a bunker, water or out of play
    // The SOUND_CALL_ sounds are the voice that reads out the banner of their name
    SOUND_CALL_EXCELLENT = 0x7E,
    SOUND_CALL_GREAT,
    SOUND_CALL_NICE_ON,
    SOUND_CALL_NICE_APPROACH,
    SOUND_CALL_NICE_TOUCH,
    SOUND_OUT_OF_COURSE,
    SOUND_CALL_HOLE_IN_ONE,
    SOUND_CALL_ALBATROSS,
    SOUND_CALL_EAGLE,
    SOUND_CALL_BIRDIE,
    SOUND_CALL_PAR,
    SOUND_RING_SHOT_RETRY = 0x8E,
    SOUND_HOLE_INTRO,
    SOUND_CALL_FINISH,
    SOUND_CALL_FLAG_SHOT,
    SOUND_CALL_CHIP_IN,
    SOUND_CALL_PIN_SHOT,
    SOUND_CALL_COMPLETE = 0x96,
    // Early in the flight of a mis-hit that rumbles
    SOUND_RUMBLING_MISHIT,
    SOUND_CALL_BOGEY,
    SOUND_DEEP_ROUGH_LANDING,
    SOUND_WASTE_AREA_LANDING,
    // As a long putt drops, or any putt under GAME_FLAG_FORCE_FLOURISHES
    SOUND_LONG_PUTT_HOLED,
    SOUND_NEW_BEST_SCORE,
    // As a course notice swings in
    SOUND_COURSE_NOTICE_CHEER,
    SOUND_AZALEA_EAGLE = 0xA7,
    SOUND_AZALEA_PAR = 0xAE,
    SOUND_AZALEA_BOGEY = 0xB1,
    SOUND_AZALEA_BIRDIE,
    // Also with the opening's Yoshi
    SOUND_BABY_MARIO_CALL_OUT_DOWN = 0xC3,
    SOUND_BABY_MARIO_CALL_OUT_C_DOWN = 0xC5,
    SOUND_BABY_MARIO_EAGLE,
    SOUND_BABY_MARIO_PAR = 0xC9, // Also the start of his birdie
    SOUND_BABY_MARIO_BIRDIE, // Also played in the opening
    SOUND_BABY_MARIO_BOGEY,
    SOUND_CHARLIE_EAGLE,
    SOUND_CHARLIE_PAR = 0xD6,
    SOUND_CHARLIE_BOGEY = 0xE0,
    SOUND_CHARLIE_BIRDIE,
    SOUND_DONKEY_KONG_EAGLE,
    SOUND_DONKEY_KONG_BOGEY = 0xE8,
    // Also his par, the end of his eagle and the opening
    SOUND_DONKEY_KONG_BIRDIE = 0xEA,
    SOUND_HARRY_BOGEY = 0xF5,
    SOUND_HARRY_BIRDIE = 0xFF,
    SOUND_HARRY_PAR = 0x103,
    SOUND_HARRY_EAGLE = 0x105,
    SOUND_JOE_PAR = 0x109,
    SOUND_JOE_BOGEY = 0x10C,
    SOUND_JOE_BIRDIE = 0x11A,
    SOUND_JOE_EAGLE,
    SOUND_KID_PAR = 0x11E,
    SOUND_KID_BOGEY = 0x120,
    SOUND_KID_EAGLE = 0x123,
    SOUND_KID_BIRDIE = 0x12E, // Also the end of his eagle
    SOUND_BOWSER_EAGLE,
    SOUND_BOWSER_BOGEY = 0x135,
    SOUND_BOWSER_BIRDIE,
    SOUND_BOWSER_PAR = 0x138,
    SOUND_LUIGI_BIRDIE = 0x13F,
    SOUND_LUIGI_BOGEY,
    // With the opening's Donkey Kong where SOUND_SHORT_OH plays with Yoshi,
    // and as the opening's burst appears
    SOUND_LUIGI_OH_NO,
    // Luigi's, Mario's, Peach's and Plum's OPENING_VOICE sounds lie in their
    // voice banks, each just before the character's CALL_OUT_UP sound, but no row
    // of f_CharacterSounds lists them; only the opening plays them, in threes
    SOUND_LUIGI_OPENING_VOICE = 0x146,
    // Also Luigi's full-shot sound
    SOUND_LUIGI_CALL_OUT_UP,
    SOUND_LUIGI_CALL_OUT_RIGHT,
    SOUND_LUIGI_PAR = 0x14E,
    SOUND_LUIGI_EAGLE = 0x150,
    SOUND_SHORT_OH = 0x152,
    SOUND_MAPLE_BOGEY = 0x159,
    SOUND_MAPLE_EAGLE = 0x15D,
    SOUND_MAPLE_BIRDIE = 0x168,
    SOUND_MAPLE_PAR,
    SOUND_METAL_MARIO_BIRDIE = 0x173,
    SOUND_MARIO_OPENING_VOICE = 0x176,
    // Also Mario's and Metal Mario's full-shot sound
    SOUND_MARIO_CALL_OUT_UP,
    SOUND_METAL_MARIO_BOGEY = 0x17F,
    SOUND_METAL_MARIO_EAGLE,
    SOUND_METAL_MARIO_PAR = 0x182,
    // The opening's ambient sounds
    SOUND_AMBIENT_ALTERNATING_FIRST = 0x184,
    SOUND_AMBIENT_ALTERNATING_SECOND,
    // SOUND_MARIO_ITS_A_ME and SOUND_PEACH_HEY_MARIO play with the opening's
    // Yoshi, where SOUND_HELLO_BALL and SOUND_PEACH_LONG_HEY_MARIO play with its
    // Donkey Kong
    SOUND_MARIO_ITS_A_ME = 0x187,
    SOUND_HELLO_BALL,
    // SOUND_UH_OH and SOUND_LONG_OH each play once in the opening
    SOUND_UH_OH,
    SOUND_LONG_OH,
    SOUND_AMBIENT_REPEATING,
    SOUND_PEACH_PAR = 0x18E,
    // Also Peach's trouble reaction
    SOUND_PEACH_CALL_OUT_C_LEFT = 0x190,
    SOUND_PEACH_BIRDIE = 0x194,
    SOUND_PEACH_OPENING_VOICE = 0x196,
    // Also Peach's full-shot sound
    SOUND_PEACH_CALL_OUT_UP,
    SOUND_PEACH_BOGEY = 0x19F,
    SOUND_PEACH_EAGLE,
    SOUND_PEACH_HEY_MARIO,
    SOUND_PEACH_LONG_HEY_MARIO,
    SOUND_AMBIENT_RANDOM_FIRST, // Also Plum's par reaction
    SOUND_PLUM_EAGLE = 0x1AC,
    SOUND_PLUM_OPENING_VOICE = 0x1AE,
    // Also Plum's full-shot sound
    SOUND_PLUM_CALL_OUT_UP,
    SOUND_PLUM_BOGEY = 0x1B7,
    SOUND_PLUM_BIRDIE = 0x1B9,
    SOUND_AMBIENT_RANDOM_SECOND,
    SOUND_SONNY_BOGEY = 0x1C1,
    SOUND_SONNY_EAGLE = 0x1C4,
    SOUND_SONNY_BIRDIE = 0x1D0,
    SOUND_SONNY_PAR,
    SOUND_SHERRY_PAR = 0x1D4,
    SOUND_SHERRY_BIRDIE = 0x1D7,
    SOUND_SHERRY_EAGLE = 0x1DE,
    SOUND_SHERRY_BOGEY = 0x1E8,
    SOUND_WARIO_BOGEY = 0x1EE,
    SOUND_WARIO_BIRDIE = 0x1F2,
    SOUND_WARIO_PAR,
    SOUND_WARIO_EAGLE = 0x1FF,
    SOUND_YOSHI_EAGLE,
    SOUND_YOSHI_BOGEY = 0x206,
    SOUND_YOSHI_BIRDIE = 0x208,
    SOUND_YOSHI_PAR,
    // Also Yoshi's excellent-shot sound
    SOUND_YOSHI_CALL_OUT_LEFT = 0x20B,
    // As the opening's Lakitu flies off at its close
    SOUND_OPENING_CLOSE = 0x211
};

// The songs that PlaySong plays. The SONG_HOLE_ songs are a hole's result: Stroke play
// picks one by the score against par, Get Character plays the one for how the
// first player did, and match games and skins matches play SONG_HOLE_WON for
// any hole that somebody won. The SONG_ROUND_ songs play through a hole. When a shot
// comes to rest, UpdateTurns plays the song of the course, the mode or the
// tournament's course, or a SONG_CUP_IN_ song by the score for a shot that holes
// out.
enum Song {
    SONG_HOLE_OUT_EAGLE, // Also for an albatross
    // Also for a hole-in-one, and on choosing a scene from the debug menu
    SONG_HOLE_OUT_BIRDIE,
    SONG_HOLE_OUT_PAR,
    SONG_RESULTS, // The round results' song, outside tournaments
    SONG_HOLE_OUT_BOGEY,
    SONG_HOLE_OUT_OVER_BOGEY, // Also the no-controller screen's song
    SONG_HOLE_WON,
    SONG_HOLE_LOST,
    SONG_HOLE_HALVED,
    SONG_MATCH_RESULT = 0xA,
    SONG_ROUND_TOURNAMENT,
    SONG_COURSE_TOAD_HIGHLANDS,
    SONG_COURSE_KOOPA_PARK,
    SONG_CUP_IN_PAR,
    SONG_CUP_IN_OVER_PAR,
    SONG_CUP_IN_UNDER_PAR,
    SONG_COURSE_SHY_GUY_DESERT,
    SONG_COURSE_BOO_VALLEY,
    SONG_COURSE_YOSHIS_ISLAND,
    SONG_ROUND_FINAL, // A hole that can decide the round
    SONG_TRAINING,
    SONG_TRANSMIT_MENU,
    // The controls screen's song; the main menu also plays it as it enters
    // MENU_SCREEN_BET_SETTINGS
    SONG_CONTROLS,
    SONG_ROUND_SPEED_GOLF,
    SONG_ROUND_MATCH,
    SONG_ROUND_TRAINING,
    SONG_ROUND_COURSE,
    SONG_ROUND_RING_SHOT = 0x1D,
    SONG_RING_SHOT,
    SONG_ROUND_REPLAY,
    SONG_TOURNAMENT_TOAD_HIGHLANDS,
    SONG_TOURNAMENT_KOOPA_PARK,
    SONG_TOURNAMENT_SHY_GUY_DESERT,
    SONG_TOURNAMENT_BOO_VALLEY,
    SONG_TOURNAMENT_MARIOS_STAR,
    SONG_TOURNAMENT_RESULTS,
    SONG_SPEED_GOLF,
    SONG_TITLE, // The menus' title screen's song
    // For a round of four players who are all Yoshi or Donkey Kong
    SONG_YOSHI_AND_DONKEY_KONG,
    SONG_COURSE_MARIOS_STAR,
    // The title screen that alternates with the boot logos, before the menus;
    // the menus' own title screen plays SONG_TITLE
    SONG_ATTRACT_TITLE,
    SONG_ROUND_CLUB_SLOTS,
    SONG_MINI_GOLF,
    SONG_ROUND_MINI_GOLF,
    SONG_ROUND_SKINS_MATCH,
    SONG_OPENING,
    SONG_TOURNAMENT_YOSHIS_ISLAND,
    SONG_STAFF_ROLL,
    SONG_CLOSING
};

// PlaySong's flags
// Passed by some callers, but PlaySong never tests it
#define SONG_FLAG_UNREAD 0x01
// Stops the song instead when SAVE_OPTION_MUSIC_OFF is set
#define SONG_FLAG_FOLLOWS_MUSIC_OPTION 0x10
// Restarts the song if it is already playing
#define SONG_FLAG_RESTART 0x80
typedef s32 SongFlags;

// The song id when no song is playing
#define NO_SONG (-1)
// libmus scales a song's own tempo by SetSongTempo's value over this
#define NORMAL_SONG_TEMPO 0x80

// Where each sound starts in a character's table of sound ID and volume
// pairs. The call-outs are what other players' buttons make their characters
// say during a shot; five of them are also the onlookers' reactions that
// QueueCharacterSound plays.
enum CharacterSound {
    CHARACTER_SOUND_CALL_OUT_UP = 0,
    CHARACTER_SOUND_CALL_OUT_DOWN = 2,
    CHARACTER_SOUND_CALL_OUT_LEFT = 4,
    // Also CHARACTER_SOUND_KIND_GREAT's sound
    CHARACTER_SOUND_CALL_OUT_RIGHT = 6,
    // Also CHARACTER_SOUND_KIND_UNUSED's sound
    CHARACTER_SOUND_CALL_OUT_C_UP = 8,
    // Also CHARACTER_SOUND_KIND_OUT_OF_COURSE's sound
    CHARACTER_SOUND_CALL_OUT_C_DOWN = 10,
    // Also CHARACTER_SOUND_KIND_TROUBLE's sound
    CHARACTER_SOUND_CALL_OUT_C_LEFT = 12,
    // Also CHARACTER_SOUND_KIND_BUNKER's sound
    CHARACTER_SOUND_CALL_OUT_C_RIGHT = 14,
    CHARACTER_SOUND_LONGEST_DRIVE = 16,
    CHARACTER_SOUND_NEAREST_PIN = 18,
    CHARACTER_SOUND_NOTICE = 20, // As an unlock notice's portrait appears
    CHARACTER_SOUND_CHOSEN = 24,
    // The character select screen plays this or CHARACTER_SOUND_CHOSEN
    CHARACTER_SOUND_OTHER_CHOSEN = 26,
    CHARACTER_SOUND_WIN = 28,
    CHARACTER_SOUND_EXCELLENT = 30,
    CHARACTER_SOUND_FULL_SHOT = 32
};

typedef s32 CharacterSound;

// The other players' reactions to a shot that QueueCharacterSound plays, each
// after a delay set for the kind and character
enum CharacterSoundKind {
    CHARACTER_SOUND_KIND_FULL_SHOT,
    CHARACTER_SOUND_KIND_EXCELLENT, // Also a flag shot
    CHARACTER_SOUND_KIND_GREAT, // Also a chip-in
    // Never queued; only the debug delay editor previews it
    CHARACTER_SOUND_KIND_UNUSED,
    CHARACTER_SOUND_KIND_OUT_OF_COURSE,
    // A landing in a water hazard, out of bounds or out of course, and a step
    // of the flight of a shot whose impact rumbles
    CHARACTER_SOUND_KIND_TROUBLE,
    CHARACTER_SOUND_KIND_BUNKER, // Also a landing in deep rough
    CHARACTER_SOUND_KIND_COUNT
};

typedef s32 CharacterSoundKind;

// The audio channel for the next sound effect, alternating between two
extern SoundChannel g_NextSoundChannel;

void PlaySound(SoundId soundId, SoundChannel channel, SoundVolume volume);
void PlaySoundWithEffects(
    SoundId soundId,
    SoundChannel channel,
    SoundVolume volume,
    SoundReverb reverb,
    SoundPan pan
);
void StopSound(SoundChannel channel);
void SetSoundPan(SoundChannel channel, SoundPan pan);
void SetSoundVolume(SoundChannel channel, SoundVolume volume);
void SetSoundReverb(SoundChannel channel, SoundReverb reverb);
void FadeOutSound(SoundChannel channel, u32 frameCount);
void SetSoundTempo(SoundChannel channel, s32 tempo);
void PlayCharacterSound(
    CharacterIndex characterIndex,
    s32 voiceSlot,
    CharacterSound sound,
    SoundVolume volume,
    SoundPan pan
);
void QueueCharacterSound(
    CharacterIndex characterIndex,
    s32 slot,
    CharacterSoundKind kind,
    SoundVolume volume,
    SoundPan pan
);
void SetSoundFrequencyOffset(SoundChannel channel, f32 offset);
NO_RESULT UpdateAudio(void);
void StopSong(s32 fadeFrameCount);
void RequestSongStop(void);
void SetSongVolume(SoundVolume volume);
void SetSongReverb(SoundReverb reverb);
void SetSongTempo(s32 tempo);
void SetSongFrequencyOffset(f32 offset);
SongId GetSongId(void);
void PlaySong(SongId songId, SongFlags songFlags);
void InitAudio(void);

#endif
