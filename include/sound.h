#ifndef SOUND_H
#define SOUND_H

#include "common.h"

/* Sound effects play on channels, each holding one effect at a time */
#define SOUND_CHANNEL_COUNT 16

/*
 * The largest volume or pan scale: libmus keeps a sound's own value at 0x80,
 * and nearly all of it at this
 */
#define SOUND_FULL_SCALE 0x7F

/* The menus' sound channel, and how loud each of their sounds plays */
#define MENU_SOUND_CHANNEL 15
#define MENU_VOLUME_SOFT 80
#define MENU_VOLUME_MEDIUM 100

/*
 * The sound effects that PlaySound and PlaySoundWithEffects play. Many play
 * in more than one place; each is named for one use, and the comments name
 * the others.
 */
enum Sound {
  SOUND_FLAG_SWISH = 0x1F,
  SOUND_TICK, /* A tally's tick and each click of the club slots' reels */
  SOUND_REEL_STOP,
  /* Also the club slots' turn and a sound of the scorecard */
  SOUND_TALLY_DONE,
  SOUND_START_LIGHT_RED,
  SOUND_START_LIGHT_GO,
  SOUND_WIND_FLY_GUY = 0x28,
  SOUND_GOOMBA = 0x2A, /* Also the Monty Mole's */
  SOUND_CONTACT_MARIO = 0x2E, /* Also Bowser's */
  SOUND_CONTACT_HEAVY_HITTER,
  SOUND_CONTACT_WARIO, /* Also played when the game halts on a fault */
  SOUND_YOSHI_EAGLE_SECOND_EFFECT = 0x35, /* Also played in the opening */
  SOUND_METAL_MARIO_EAGLE_EFFECT = 0x38,
  SOUND_WARIO_BOGEY_EFFECT,
  /* With each spreading circle of Bowser's bogey reaction */
  SOUND_SPREADING_CIRCLE,
  /* Also played by the save prompt and late in some eagle reactions */
  SOUND_SAVED,
  /* Also in Metal Mario's, Sherry's and Wario's eagles */
  SOUND_PEACH_BIRDIE_EFFECT,
  SOUND_MAPLE_EAGLE_EFFECT,
  SOUND_YOSHI_EAGLE_EFFECT,
  SOUND_CHARLIE_BIRDIE_EFFECT,
  /* Over and over in his reactions and the opening */
  SOUND_DONKEY_KONG_REPEATED = 0x41,
  SOUND_DONKEY_KONG_EAGLE_EFFECT,
  SOUND_SHRINKING_IMAGE,
  SOUND_BANNER,
  SOUND_SLICE, /* Also played when the holes of a round are chosen */
  /* The scorecard and the save menu opening and closing */
  SOUND_PANEL_OPEN = 0x47,
  SOUND_PANEL_CLOSE,
  /*
   * Also played when a contest's winner appears, an erasure is declined and
   * a tournament is declined
   */
  SOUND_READY,
  /*
   * Also played when a contest's winner vanishes, the erase window opens and
   * a club tournament code is accepted
   */
  SOUND_GO,
  SOUND_CAPTION_MEET, /* Also played when the save data is erased */
  /* Over and over in her reactions and the opening */
  SOUND_YOSHI_REPEATED = 0x4F,
  SOUND_MENU_START,
  /* A ball being struck by a plain club or bouncing on hard ground */
  SOUND_CONTACT_PLAIN,
  SOUND_ENTER_TOURNAMENT = 0x54,
  SOUND_UNK90 = 0x5A,
  SOUND_LOOK_DOWN_RANGE, /* Also played when a scorecard row flips */
  SOUND_MENU_MOVE, /* Also the prompts' cursor and the scorecard's pages */
  SOUND_MENU_CONFIRM, /* The sound of confirming a choice */
  SOUND_MENU_CANCEL = 0x5F,
  SOUND_LOOK_AT_CUP,
  SOUND_LOOK_BACK, /* Also a sound of the scorecard */
  /* The swing meter's click for an impact on the mark */
  SOUND_IMPACT_ON_MARK,
  /*
   * A menu choice that cannot be made, or a cursor that cannot move that way.
   * Also a mishit impact and an incorrect club tournament code.
   */
  SOUND_MENU_BLOCKED,
  /*
   * Also the inset view opening, a wipe closing, the aim camera lowering and
   * the club slots' reels starting
   */
  SOUND_PROMPT_OPEN,
  /* Also the inset view closing, a wipe opening and the aim camera rising */
  SOUND_PROMPT_CLOSE,
  SOUND_THREE_STARS = 0x67, /* Also a sound of the scorecard */
  SOUND_UNK106 = 0x6A,
  SOUND_SKIN_COUNTED,
  SOUND_TEXT_TYPING,
  /* The swing meter's click for an impact off the mark */
  SOUND_IMPACT_OFF_MARK = 0x6E,
  SOUND_OPTION_TOGGLED,
  SOUND_LONG_SHOT,
  SOUND_LONG_SHOT_HEAVY,
  SOUND_CONTROLS_OPEN,
  /* Also played when the main menu shows the transmit option */
  SOUND_CONTROLS_CLOSE,
  SOUND_OUT_OF_BOUNDS = 0x77,
  SOUND_RING_SHOT_RETRY = 0x8E,
  SOUND_HOLE_INTRO,
  SOUND_UNK154 = 0x9A,
  SOUND_NEW_BEST_SCORE = 0x9C,
  SOUND_AZALEA_EAGLE = 0xA7,
  SOUND_AZALEA_PAR = 0xAE,
  SOUND_AZALEA_BOGEY = 0xB1,
  SOUND_AZALEA_BIRDIE,
  SOUND_BABY_MARIO_EAGLE = 0xC6,
  SOUND_BABY_MARIO_PAR = 0xC9, /* Also the start of his birdie */
  SOUND_BABY_MARIO_BIRDIE,
  SOUND_BABY_MARIO_BOGEY,
  SOUND_CHARLIE_EAGLE,
  SOUND_CHARLIE_PAR = 0xD6,
  SOUND_CHARLIE_BOGEY = 0xE0,
  SOUND_CHARLIE_BIRDIE,
  SOUND_DONKEY_KONG_EAGLE,
  SOUND_DONKEY_KONG_BOGEY = 0xE8,
  /* Also his par, the end of his eagle and the opening */
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
  SOUND_KID_BIRDIE = 0x12E, /* Also the end of his eagle */
  SOUND_BOWSER_EAGLE,
  SOUND_BOWSER_BOGEY = 0x135,
  SOUND_BOWSER_BIRDIE,
  SOUND_BOWSER_PAR = 0x138,
  SOUND_LUIGI_BIRDIE = 0x13F,
  SOUND_LUIGI_BOGEY,
  SOUND_LUIGI_PAR = 0x14E,
  SOUND_LUIGI_EAGLE = 0x150,
  SOUND_MAPLE_BOGEY = 0x159,
  SOUND_MAPLE_EAGLE = 0x15D,
  SOUND_MAPLE_BIRDIE = 0x168,
  SOUND_MAPLE_PAR,
  SOUND_METAL_MARIO_BIRDIE = 0x173,
  SOUND_METAL_MARIO_BOGEY = 0x17F,
  SOUND_METAL_MARIO_EAGLE,
  SOUND_METAL_MARIO_PAR = 0x182,
  /* The opening's ambient sounds */
  SOUND_AMBIENT_ALTERNATING_FIRST = 0x184,
  SOUND_AMBIENT_ALTERNATING_SECOND,
  SOUND_AMBIENT_REPEATING = 0x18B,
  SOUND_PEACH_PAR = 0x18E,
  SOUND_PEACH_BIRDIE = 0x194,
  SOUND_PEACH_BOGEY = 0x19F,
  SOUND_PEACH_EAGLE,
  SOUND_AMBIENT_RANDOM_FIRST = 0x1A3, /* Also Plum's par reaction */
  SOUND_PLUM_EAGLE = 0x1AC,
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
  SOUND_YOSHI_PAR
};

/*
 * The songs that PlaySong plays. SONG_HOLE_* are a hole's result: Stroke play
 * picks one by the score against par, Get Character plays the one for how the
 * first player did, and match games and skins matches play SONG_HOLE_WON for
 * any hole that somebody won. SONG_ROUND_* play through a hole. When a shot
 * comes to rest, UpdateTurns plays the song of the course, the mode or the
 * tournament's course, or SONG_CUP_IN_* by the score for a shot that holes
 * out.
 */
enum Song {
  SONG_HOLE_OUT_EAGLE, /* Also for an albatross */
  /* Also for a hole-in-one, and on choosing a scene from the debug menu */
  SONG_HOLE_OUT_BIRDIE,
  SONG_HOLE_OUT_PAR,
  SONG_RESULTS, /* The round results' song, outside tournaments */
  SONG_HOLE_OUT_BOGEY,
  SONG_HOLE_OUT_OVER_BOGEY, /* Also the no-controller screen's song */
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
  SONG_ROUND_FINAL, /* A hole that can decide the round */
  SONG_TRAINING,
  SONG_TRANSMIT_MENU,
  /* The controls screen's song; the main menu also plays it as it enters
   * MENU_SCREEN_UNK22 */
  SONG_CONTROLS,
  SONG_ROUND_UNK24,
  SONG_ROUND_MATCH,
  SONG_ROUND_UNK26,
  SONG_ROUND_COURSE,
  SONG_ROUND_RING_SHOT = 0x1D,
  SONG_RING_SHOT,
  SONG_ROUND_UNK31,
  SONG_TOURNAMENT_TOAD_HIGHLANDS,
  SONG_TOURNAMENT_KOOPA_PARK,
  SONG_TOURNAMENT_SHY_GUY_DESERT,
  SONG_TOURNAMENT_BOO_VALLEY,
  SONG_TOURNAMENT_MARIOS_STAR,
  SONG_TOURNAMENT_RESULTS,
  SONG_SPEED_GOLF,
  SONG_TITLE, /* The menus' title screen's song */
  /* For a round of four players who are all Yoshi or Donkey Kong */
  SONG_YOSHI_AND_DONKEY_KONG,
  SONG_COURSE_MARIOS_STAR,
  /*
   * The title screen that alternates with the boot logos, before the menus;
   * the menus' own title screen plays SONG_TITLE
   */
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

/* PlaySong's flags */
/* Passed by some callers, but PlaySong never tests it */
#define SONG_FLAG_UNREAD 0x01
/* Stops the song instead when SAVE_OPTION_MUSIC_OFF is set */
#define SONG_FLAG_UNK10 0x10
/* Restarts the song if it is already playing */
#define SONG_FLAG_RESTART 0x80

/* The song id when no song is playing */
#define NO_SONG -1
/* libmus scales a song's own tempo by SetSongTempo's value over this */
#define NORMAL_SONG_TEMPO 0x80

/*
 * Where each sound starts in a character's table of sound ID and volume
 * pairs. The call-outs are what other players' buttons make their characters
 * say during a shot; five of them are also the onlookers' reactions that
 * QueueCharacterSound plays.
 */
enum CharacterSound {
  CHARACTER_SOUND_CALL_OUT_UP = 0,
  CHARACTER_SOUND_CALL_OUT_DOWN = 2,
  CHARACTER_SOUND_CALL_OUT_LEFT = 4,
  /* Also CHARACTER_SOUND_KIND_GREAT's sound */
  CHARACTER_SOUND_CALL_OUT_RIGHT = 6,
  /* Also CHARACTER_SOUND_KIND_UNK3's sound */
  CHARACTER_SOUND_CALL_OUT_C_UP = 8,
  /* Also CHARACTER_SOUND_KIND_OUT_OF_COURSE's sound */
  CHARACTER_SOUND_CALL_OUT_C_DOWN = 10,
  /* Also CHARACTER_SOUND_KIND_TROUBLE's sound */
  CHARACTER_SOUND_CALL_OUT_C_LEFT = 12,
  /* Also CHARACTER_SOUND_KIND_BUNKER's sound */
  CHARACTER_SOUND_CALL_OUT_C_RIGHT = 14,
  CHARACTER_SOUND_LONGEST_DRIVE = 16,
  CHARACTER_SOUND_NEAREST_PIN = 18,
  CHARACTER_SOUND_NOTICE = 20, /* As an unlock notice's portrait appears */
  CHARACTER_SOUND_CHOSEN = 24,
  /* The character select screen plays this or CHARACTER_SOUND_CHOSEN */
  CHARACTER_SOUND_OTHER_CHOSEN = 26,
  CHARACTER_SOUND_WIN = 28,
  CHARACTER_SOUND_EXCELLENT = 30,
  CHARACTER_SOUND_FULL_SHOT = 32
};

/*
 * The other players' reactions to a shot that QueueCharacterSound plays, each
 * after a delay set for the kind and character
 */
enum CharacterSoundKind {
  CHARACTER_SOUND_KIND_FULL_SHOT,
  CHARACTER_SOUND_KIND_EXCELLENT, /* Also a flag shot */
  CHARACTER_SOUND_KIND_GREAT, /* Also a chip-in */
  CHARACTER_SOUND_KIND_UNK3,
  CHARACTER_SOUND_KIND_OUT_OF_COURSE,
  /*
   * A landing in a water hazard, out of bounds or out of course, and a step
   * of the flight when unk2E42 is set
   */
  CHARACTER_SOUND_KIND_TROUBLE,
  CHARACTER_SOUND_KIND_BUNKER, /* Also a landing in deep rough */
  CHARACTER_SOUND_KIND_COUNT
};

/* The audio channel for the next sound effect, alternating between two */
extern s32 g_NextSoundChannel;

void PlaySound(s32 soundId, s32 channel, s32 volume);
void PlaySoundWithEffects(
    s32 soundId, s32 channel, s32 volume, s32 reverb, s32 pan);
void StopSound(s32 channel);
void SetSoundPan(s32 channel, s32 pan);
void SetSoundVolume(s32 channel, s32 volume);
void SetSoundReverb(s32 channel, s32 reverb);
void FadeOutSound(s32 channel, u32 frameCount);
void SetSoundTempo(s32 channel, s32 tempo);
void PlayCharacterSound(
    s32 character, s32 slot, s32 soundIndex, s32 volume, s32 pan);
void QueueCharacterSound(
    s32 character, s32 slot, s32 kind, s32 volume, s32 pan);
void SetSoundFrequencyOffset(s32 channel, f32 offset);
s32 UpdateAudio(void);
void StopSong(s32 fadeFrameCount);
void RequestSongStop(void);
void SetSongVolume(s32 volume);
void SetSongReverb(s32 reverb);
void SetSongTempo(s32 tempo);
void SetSongFrequencyOffset(f32 offset);
s32 GetSongId(void);
void PlaySong(s32 songId, s32 flags);
void InitAudio(void);

#endif
