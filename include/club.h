#ifndef CLUB_H
#define CLUB_H

#include <ultra64.h>

/* How far the selected club hits: a power shot, a normal shot or an approach shot */
#define SHOT_MODE_POWER 0
#define SHOT_MODE_NORMAL 1
#define SHOT_MODE_APPROACH 2
#define SHOT_MODE_COUNT 3

/* The putter's shot modes are long, middle and short putts */
#define PUTT_MODE_LONG SHOT_MODE_POWER
#define PUTT_MODE_MIDDLE SHOT_MODE_NORMAL
#define PUTT_MODE_SHORT SHOT_MODE_APPROACH

/* A set has a club of each type, from the 1 wood to the putter */
#define CLUB_SET_SIZE 14
#define CLUB_SET_COUNT 18
#define CLUB_COUNT (CLUB_SET_SIZE * CLUB_SET_COUNT)
#define CLUB_TYPE_PUTTER 13

/* A player's bag has this many slots; empty ones hold NO_CLUB */
#define CLUB_SLOT_COUNT 16
#define NO_CLUB (-1)

/* Bits of a club's surface mask, one for each group of surfaces it is usable on */
#define CLUB_SURFACE_GREEN 0x1
#define CLUB_SURFACE_ROUGH 0x2
#define CLUB_SURFACE_BUNKER 0x4
#define CLUB_SURFACE_TEE_GROUND 0x8
#define CLUB_SURFACE_FAIRWAY 0x10
#define CLUB_SURFACE_SEMI_ROUGH 0x20
#define CLUB_SURFACE_ALL (-1)

/* GetLieRating's ratings run from 1 to this */
#define MAX_LIE_RATING 5

/* What GenerateClub finds for a club, for each shot mode */
typedef struct ClubParameters {
    /* 0x00 */ s32 power[SHOT_MODE_COUNT];
    /* 0x0C */ s32 deflection[SHOT_MODE_COUNT];
} ClubParameters; /* size = 0x18 */

typedef struct Club {
    /* 0x00 */ u8 unk0;
    /* 0x01 */ u8 unk1[4];
    /* 0x05 */ u8 unk5;
    /* 0x06 */ u8 unk6[2];
    /* 0x08 */ s32 surfaceMask;
    /* 0x0C */ s16 distance; /* In yards */
    /* 0x0E */ s16 pitch;    /* In degrees, from -45 to -7 */
    /* 0x10 */ s16 unk10;
    /* 0x12 */ s16 unk12;
    /* 0x14 */ s16 unk14;
    /* 0x16 */ s16 type; /* The club's place in its set */
    /* 0x18 */ s32 power[SHOT_MODE_COUNT];
    /* 0x24 */ u8 unk24[2];
    /* 0x26 */ s16 unk26;
    /* 0x28 */ s16 deflection[SHOT_MODE_COUNT]; /* 0x1000 less the test shot's landing direction */
    /* 0x2E */ u8 unk2E[2];
    /* 0x30 */ s8 unk30;
    /* 0x31 */ u8 unk31[3];
} Club; /* size = 0x34 */

/* Every club set, one after another */
extern Club g_Clubs[CLUB_COUNT];

/* Set while GenerateClub or GetPuttPower test-fires a club */
extern s8 g_ClubTestMode;

const ClubParameters *GetClubParameters(s32 clubIndex);
s32 GetShotDistance(Club *club, u32 shotMode);
s32 GetShotWorldDistance(Club *club, s32 shotMode);
void GenerateClub(s32 clubIndex);
void InitClubs(void);
s32 GetPuttPower(s32 fraction, s32 shotMode);
s32 GetClubPower(Club *club, s32 shotMode);
Club *GetClub(u32 clubIndex);
Club *GetSelectedClub(void);
s32 GetSelectedShotDistance(void);
s32 GetSelectedShotWorldDistance(void);
s32 GetCurrentSurfaceClubMask(s32 distinguishesKinds);
s32 GetLieRating(Club *club, s32 surface, s32 shotMode);
void GetLieSpan(Club *club, s32 surface, s32 *span, s32 shotMode);
void HandleClubInput(void);
void HandleShotModeInput(void);

#endif
