#ifndef CLUB_H
#define CLUB_H

#include <ultra64.h>

#include "asset.h"
#include "compiler.h"
#include "language.h"
#include "surface.h"

enum ShotMode {
    SHOT_MODE_POWER,
    SHOT_MODE_NORMAL,
    SHOT_MODE_APPROACH
};

typedef s32 ShotMode;

#define SHOT_MODE_COUNT 3

#define PUTT_MODE_LONG SHOT_MODE_POWER
#define PUTT_MODE_MIDDLE SHOT_MODE_NORMAL
#define PUTT_MODE_SHORT SHOT_MODE_APPROACH

#define CLUB_SET_SIZE 14
#define CLUB_SET_COUNT 18
#define CLUB_COUNT (CLUB_SET_SIZE * CLUB_SET_COUNT)

enum ClubType {
    CLUB_TYPE_DRIVER,
    CLUB_TYPE_THREE_WOOD,
    CLUB_TYPE_FOUR_WOOD,
    CLUB_TYPE_TWO_IRON,
    CLUB_TYPE_THREE_IRON,
    CLUB_TYPE_FOUR_IRON,
    CLUB_TYPE_FIVE_IRON,
    CLUB_TYPE_SIX_IRON,
    CLUB_TYPE_SEVEN_IRON,
    CLUB_TYPE_EIGHT_IRON,
    CLUB_TYPE_NINE_IRON,
    CLUB_TYPE_PITCHING_WEDGE,
    CLUB_TYPE_SAND_WEDGE,
    CLUB_TYPE_PUTTER
};

typedef s32 ClubType;

enum ClubCategory {
    CLUB_CATEGORY_WOOD,
    CLUB_CATEGORY_IRON,
    CLUB_CATEGORY_WEDGE,
    CLUB_CATEGORY_PUTTER
};

typedef s32 ClubCategory;

#define CLUB_SLOT_COUNT 16
#define NO_CLUB (-1)

#define CLUB_SURFACE_GREEN 0x1
#define CLUB_SURFACE_ROUGH 0x2
#define CLUB_SURFACE_BUNKER 0x4
#define CLUB_SURFACE_TEE_GROUND 0x8
#define CLUB_SURFACE_FAIRWAY 0x10
#define CLUB_SURFACE_SEMI_ROUGH 0x20
#define CLUB_SURFACE_ALL (-1)

#define DRIVER_SURFACES \
    (~(CLUB_SURFACE_GREEN | CLUB_SURFACE_ROUGH | CLUB_SURFACE_BUNKER | CLUB_SURFACE_FAIRWAY | \
        CLUB_SURFACE_SEMI_ROUGH))
#define FAIRWAY_WOOD_SURFACES (~(CLUB_SURFACE_GREEN | CLUB_SURFACE_ROUGH | CLUB_SURFACE_BUNKER))
#define IRON_SURFACES (~CLUB_SURFACE_GREEN)

#define LIE_RATING_MAX 5

typedef struct ClubParameters {
    s32 power[SHOT_MODE_COUNT];
    s32 deflection[SHOT_MODE_COUNT];
} ClubParameters;

typedef struct Club {
    u8 mishitScale;
    char name[4] NONSTRING;
    s32 surfaceMask;
    s16 distance;
    s16 pitch;
    s16 offsetPitch;
    s16 spin;
    s16 lieRatingPercent;
    s16 type;
    s32 power[SHOT_MODE_COUNT];
    s16 category;
    s16 sideSpin;
    s16 deflection[SHOT_MODE_COUNT];
    AssetId headImageAsset;
    s8 curve;
} Club;

extern Club g_Clubs[CLUB_COUNT];

enum ClubTest {
    CLUB_TEST_NONE,
    CLUB_TEST_SHOT,
    CLUB_TEST_PUTT
};

typedef s8 ClubTest;

extern ClubTest g_ClubTestMode;

struct CurrentShot;
struct GameBoyCharacter;

const ClubParameters *GetClubParameters(s32 clubIndex);
s32 GetShotDistance(Club *club, ShotMode shotMode);
s32 GetShotWorldDistance(Club *club, ShotMode shotMode);
void GenerateClub(s32 clubIndex);
void GenerateClubSet(s32 setIndex, struct GameBoyCharacter *gameBoyCharacter);
void InitClubs(void);
s32 GetPuttPower(s32 fraction, ShotMode shotMode);
s32 GetClubPower(Club *club, ShotMode shotMode);
Club *GetClub(u32 clubIndex);
Club *GetSelectedClub(void);
s32 GetSelectedShotDistance(void);
s32 GetSelectedShotWorldDistance(void);
s32 GetSurfaceClubMask(s32 distinguishesKinds, SurfaceTraits *surfaceTraits);
s32 GetCurrentSurfaceClubMask(s32 distinguishesKinds);
s32 GetLieRating(Club *club, Surface surface, ShotMode shotMode);
void GetSurfacePercentRange(SurfaceTraits *surfaceTraits, Club *club, s32 *range);
void GetLieSpan(Club *club, Surface surface, s32 *lieSpan, ShotMode shotMode);
void UpdateClubControls(void);
void UpdateShotModeControls(void);
Boolean StepToClubChoice(struct CurrentShot *targetShot);

#endif
