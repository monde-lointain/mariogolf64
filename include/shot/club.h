/*
 * The clubs: their reach and power, their play from each surface, and choosing
 * the club and shot mode.
 */

#ifndef CLUB_H
#define CLUB_H

#include <ultra64.h>

#include "course/surface.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"

// How far the selected club hits: a power shot, a normal shot or an approach
// shot
enum ShotMode {
    SHOT_MODE_POWER,
    SHOT_MODE_NORMAL,
    SHOT_MODE_APPROACH
};

typedef s32 ShotMode;
typedef s8 ShotMode8;
typedef u8 UShotMode8;

#define SHOT_MODE_COUNT 3

// The putter's shot modes are long, middle and short putts
#define PUTT_MODE_LONG SHOT_MODE_POWER
#define PUTT_MODE_MIDDLE SHOT_MODE_NORMAL
#define PUTT_MODE_SHORT SHOT_MODE_APPROACH

// A set has a club of each type, from the 1 wood to the putter
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

// What a club is, from the clubs' initial data
typedef s16 ClubType16;

// What kind of club a club is, from the clubs' initial data
enum ClubCategory {
    CLUB_CATEGORY_NONE = -1,
    CLUB_CATEGORY_WOOD, // 1W, 3W and 4W
    CLUB_CATEGORY_IRON, // 2I to 9I
    CLUB_CATEGORY_WEDGE, // PW and SW
    CLUB_CATEGORY_PUTTER
};

typedef s32 ClubCategory;
typedef s16 ClubCategory16;

// A player's bag has this many slots; empty ones hold NO_CLUB
#define CLUB_SLOT_COUNT 16
typedef s32 ClubSlot;
typedef s8 ClubSlot8;
#define NO_CLUB (-1)
typedef s32 ClubIndex;
typedef u32 UClubIndex;
typedef s16 ClubIndex16;
typedef s32 ClubSurfaceMask;

// Bits of a club's surface mask, one for each group of surfaces it is usable on
#define CLUB_SURFACE_GREEN 0x1
#define CLUB_SURFACE_ROUGH 0x2
#define CLUB_SURFACE_BUNKER 0x4
#define CLUB_SURFACE_TEE_GROUND 0x8
#define CLUB_SURFACE_FAIRWAY 0x10
#define CLUB_SURFACE_SEMI_ROUGH 0x20
#define CLUB_SURFACE_ALL (-1)

// The surfaces each club type can be used on
#define DRIVER_SURFACES \
    (~(CLUB_SURFACE_GREEN | CLUB_SURFACE_ROUGH | CLUB_SURFACE_BUNKER | CLUB_SURFACE_FAIRWAY | \
        CLUB_SURFACE_SEMI_ROUGH))
#define FAIRWAY_WOOD_SURFACES (~(CLUB_SURFACE_GREEN | CLUB_SURFACE_ROUGH | CLUB_SURFACE_BUNKER))
#define IRON_SURFACES (~CLUB_SURFACE_GREEN)

// GetLieRating's ratings run from 1 to this
typedef s32 LieRating;
#define NO_LIE_RATING 0
#define LIE_RATING_MIN 1
#define LIE_RATING_MAX 5

// What GenerateClub finds for a club, for each shot mode
typedef struct ClubParameters {
    s32 power[SHOT_MODE_COUNT];
    s32 deflection[SHOT_MODE_COUNT];
} ClubParameters;

typedef struct Club {
    u8 mishitScale; // Scales a mishit's yaw and side spin, and the impact point's side spin
    char name[4] NONSTRING; // Its short name, such as "1W" or "PT"
    ClubSurfaceMask surfaceMask;
    s16 distance; // In yards
    s16 pitch; // In degrees, from -45 to -7
    s16 offsetPitch; // In degrees per 32 of offsetY
    s16 spin; // Shown as SPIN beside the debug character viewer
    s16 lieRatingPercent; // Scales GetLieRating's rating
    ClubType16 type; // The club's place in its set
    s32 power[SHOT_MODE_COUNT];
    ClubCategory16 category;
    s16 sideSpin; // From -100, a fade, to 100, a draw
    s16 deflection[SHOT_MODE_COUNT]; // 0x1000 less the test shot's landing direction
    // The image of the club's head that the HUD shows
    AssetId headImageAsset;
    s8 curve; // From -4, the strongest fade, to 4, the strongest draw
} Club;

// Every club set, one after another
extern Club g_Clubs[CLUB_COUNT];

// Values of g_ClubTestMode, which tells the ball simulation a test shot is
// running
enum ClubTest {
    CLUB_TEST_NONE,
    CLUB_TEST_SHOT,
    CLUB_TEST_PUTT
};

typedef s8 ClubTest;

// Set while GenerateClub or GetPuttPower test-fires a club
extern ClubTest g_ClubTestMode;

// current_shot.h includes this header, so it can't be included here
struct CurrentShot;
// Defined in transfer_pak.h
struct GameBoyCharacter;

const ClubParameters *GetClubParameters(ClubIndex clubIndex);
s32 GetShotDistance(Club *club, ShotMode shotMode);
s32 GetShotWorldDistance(Club *club, ShotMode shotMode);
void GenerateClub(ClubIndex clubIndex);
void GenerateClubSet(s32 setIndex, struct GameBoyCharacter *gameBoyCharacter);
void InitClubs(void);
s32 GetPuttPower(s32 fraction, ShotMode shotMode);
s32 GetClubPower(Club *club, ShotMode shotMode);
Club *GetClub(UClubIndex clubIndex);
Club *GetSelectedClub(void);
s32 GetSelectedShotDistance(void);
s32 GetSelectedShotWorldDistance(void);
ClubSurfaceMask GetSurfaceClubMask(Boolean distinguishesKinds, SurfaceTraits *surfaceTraits);
ClubSurfaceMask GetCurrentSurfaceClubMask(Boolean distinguishesKinds);
LieRating GetLieRating(Club *club, Surface surface, ShotMode shotMode);
void GetSurfacePercentRange(SurfaceTraits *surfaceTraits, Club *club, s32 *range);
void GetLieSpan(Club *club, Surface surface, s32 *lieSpan, ShotMode shotMode);
void UpdateClubControls(void);
void UpdateShotModeControls(void);
Boolean StepToClubChoice(struct CurrentShot *targetShot);

#endif
