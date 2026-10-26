#include "common.h"
#include "billboard.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "reaction.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "sound.h"
#include "surface.h"
#include "turn.h"
#include "variables.h"

/* The loop played while the stick sets the impact point */
#define IMPACT_POINT_SOUND 0x26
#define IMPACT_POINT_SOUND_CHANNEL 8
#define IMPACT_POINT_SOUND_VOLUME 60
#define IMPACT_POINT_SOUND_REVERB 20
#define IMPACT_POINT_SOUND_PAN 230

/* The buttons that hold the impact point mode */
#define IMPACT_POINT_BUTTONS (Z_TRIG | L_TRIG)

/* D_800C3084's bits, which UpdateShotFlags sets for the shot being set up */
#define SHOT_UNK3084_ON_GREEN 1
#define SHOT_UNK3084_NO_IMPACT_POINT 2 /* For a putter, or in mini golf */
#define SHOT_UNK3084_APPROACH 4
#define SHOT_UNK3084_UNK8 8 /* Set while g_CurrentShot.unk50 is at most SHOT_UNK8_MAX_UNK50 */
#define SHOT_UNK50_SCALE (1.0f / 1536)
#define SHOT_UNK8_MAX_UNK50 600.0f

/* The kinds of notable shot that earn Course Points, as counted in unkAA */
#define SHOT_KIND_COUNT PLAYER_UNKAA_COUNT
#define SHOT_KIND_UNK7 7      /* The only kind that mini golf counts */
#define SHOT_KIND_MAX_COUNT 0xFF
/* Shots count towards Course Points on their first flights only */
#define MAX_COUNTED_FLIGHT_COUNT 2

extern s32 D_800C3084;
extern s32 D_80227194_ovl_00;
extern s32 sTurnTimerStart; /* Nothing reads it */
/* How many times the shot's flight has been shown; StartFlightCamera gets the earlier count */
extern s32 sShotFlightCount;

s32 HasNoRingsLeft(UnkB8 *player);
void RecordRingShotClear(s32 playerIndex);

/* Lets the Rumble Paks run again after nuContRmbForceStop */
void EndRumbleForceStop(void) {
    nuSiSendMesg(NU_CONT_RMB_FORCESTOPEND_MSG, NULL);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202A20_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202D04_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802038D0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203B4C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203DAC_ovl_00);

void UpdateImpactPointMode(void) {
    if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) && !(D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT)) {
        if (!g_IsSettingImpactPoint && D_80227194_ovl_00 == 0) {
            PlaySoundWithEffects(IMPACT_POINT_SOUND, IMPACT_POINT_SOUND_CHANNEL, IMPACT_POINT_SOUND_VOLUME,
                                 IMPACT_POINT_SOUND_REVERB, IMPACT_POINT_SOUND_PAN);
        }
        g_IsSettingImpactPoint = TRUE;
    } else {
        StopSound(IMPACT_POINT_SOUND_CHANNEL);
        g_IsSettingImpactPoint = FALSE;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203EEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203F30_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204104_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802041C8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204210_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020439C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204460_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204568_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802045BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020466C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204910_ovl_00);

void UpdateShotFlags(void) {
    s32 surfaceMask = GetSelectedClub()->surfaceMask;
    s8 *isMiniGolf;

    D_800C3084 = 0;
    if (surfaceMask & CLUB_SURFACE_GREEN) {
        D_800C3084 = SHOT_UNK3084_NO_IMPACT_POINT;
    }
    isMiniGolf = &g_IsMiniGolf;

    if (*isMiniGolf) {

        D_800C3084 |= SHOT_UNK3084_NO_IMPACT_POINT;
    }
    if (GetSurfaceInfo(g_CurrentShot.surface)->kind == SURFACE_KIND_GREEN) {
        D_800C3084 |= SHOT_UNK3084_ON_GREEN;
    }
    if (*isMiniGolf) {
        D_800C3084 |= SHOT_UNK3084_ON_GREEN | SHOT_UNK3084_NO_IMPACT_POINT;
    }
    if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
        D_800C3084 |= SHOT_UNK3084_APPROACH;
    }
    if (g_CurrentShot.unk50 * SHOT_UNK50_SCALE <= SHOT_UNK8_MAX_UNK50) {
        D_800C3084 |= SHOT_UNK3084_UNK8;
    }
}

void StartTurnTimer(void) {
    sTurnTimerStart = osGetCount();
}

/* Always FALSE; maybe once a debug check */
s32 func_8020561C_ovl_00(s32 arg0) {
    return FALSE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205624_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802059C0_ovl_00);

/* Counts a notable shot towards the player's Course Points */
void CountNotableShot(s32 playerIndex, s32 shotKind) {
    u8 *count;

    if ((s8)g_Players[playerIndex].unk1E[0] != 0 || g_GameState.unk31 || g_GameMode == GAME_MODE_UNK3) {
        return;
    }
    if (g_GameMode == GAME_MODE_RING_SHOT || sShotFlightCount >= MAX_COUNTED_FLIGHT_COUNT) {
        return;
    }
    if (g_IsMiniGolf && shotKind != SHOT_KIND_UNK7) {
        return;
    }
    count = (u8 *)&g_Players[playerIndex].unkAA[shotKind];
    if (*count < SHOT_KIND_MAX_COUNT) {
        osSyncPrintf("EXP GET type %d\n", shotKind);
        (*count)++;
    }
}

/* The course points that the player has earned with the shots of one kind */
s32 GetShotKindPoints(s32 playerIndex, s32 shotKind) {
    s32 count = (u8)g_Players[playerIndex].unkAA[shotKind];
    s32 points;

    switch (shotKind) {
        case 0:
        case 1:
            points = 2;
            break;
        case 4:
            points = 8;
            break;
        case 2:
        case 5:
            points = 4;
            break;
        case 7:
            if (g_IsMiniGolf) {
                points = 10;
            } else {
                points = 20;
            }
            break;
        case 6:
        case 9:
        case 11:
            points = 10;
            break;
        case 8:
            points = 16;
            break;
        case 3:
        case 10:
            points = 6;
            break;
        default:
            osSyncPrintf("Unknown Shot Type%d\n", shotKind);
            points = 0;
            break;
    }
    return count * points;
}

/* Turns each player's counts of notable shots into points, and returns everyone's total */
s32 TallyCoursePoints(void) {
    s32 playerIndex;
    s32 shotKind;
    s32 points;
    s32 total = 0;

    for (playerIndex = 0; playerIndex < g_PlayerCount; playerIndex++) {
        points = 0;
        for (shotKind = 0; shotKind < SHOT_KIND_COUNT; shotKind++) {
            points += GetShotKindPoints(playerIndex, shotKind);
            g_Players[playerIndex].unkAA[shotKind] = 0;
        }
        total += points;
        g_Players[playerIndex].points = points;
    }
    return total;
}

void PrintCoursePoints(void) {
    char text[128];
    s32 playerIndex;
    s32 shotKind;
    s32 points;

    for (playerIndex = 0; playerIndex < g_PlayerCount; playerIndex++) {
        points = 0;
        for (shotKind = 0; shotKind < SHOT_KIND_COUNT; shotKind++) {
            points += GetShotKindPoints(playerIndex, shotKind);
        }
        sprintf(text, "Player%d EXP %d\n", playerIndex, points);
        PrintDebugText(text, 2, playerIndex + 3);
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205DE8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206354_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206BD0_ovl_00);

s32 IsRingShotRound(void) {
    if (!g_GameState.unk31) {
        return g_GameMode == GAME_MODE_RING_SHOT;
    }
    return FALSE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206CF0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206E88_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", HasNoRingsLeft);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207048_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", RecordRingShotClear);

/* Saves a Ring Shot hole as cleared once it is holed within par with every ring passed */
void CheckRingShotClear(s32 playerIndex) {
    if (g_GameMode == GAME_MODE_RING_SHOT && g_Players[playerIndex].strokeCount <= g_HolePar &&
        (s8)g_Players[playerIndex].unk1E[0] == 0 && HasNoRingsLeft(&g_Players[playerIndex])) {
        RecordRingShotClear(playerIndex);
    }
}

/*
 * The first of the club kind's animations; shotMode is unused. The staff
 * roll overlay has its own copies of these four routines, with the same
 * names, so these are static to link beside them.
 */
static s32 GetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    s32 animation;

    switch (GetCharacterClubKind(slotIndex, club)) {
        case CLUB_KIND_WOOD:
            animation = WOOD_ANIMATIONS;
            break;
        case CLUB_KIND_IRON:
            animation = IRON_ANIMATIONS;
            break;
        case CLUB_KIND_WEDGE:
            animation = WEDGE_ANIMATIONS;
            break;
        case CLUB_KIND_PUTTER:
            animation = PUTTER_ANIMATIONS;
            break;
        default:
            animation = WOOD_ANIMATIONS;
            break;
    }
    return animation;
}

/* The second of the club kind's animations, or the third for an approach shot */
static s32 GetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    s32 animation;

    switch (GetCharacterClubKind(slotIndex, club)) {
        case CLUB_KIND_WOOD:
            animation = WOOD_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_KIND_IRON:
            animation = IRON_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_KIND_WEDGE:
            animation = WEDGE_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_KIND_PUTTER:
            animation = PUTTER_ANIMATIONS + SWING_ANIMATION;
            break;
        default:
            animation = 0;
            break;
    }
    return animation + (shotMode == SHOT_MODE_APPROACH);
}

static void SetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    SetCharacterAnimation(slotIndex, GetStanceAnimation(slotIndex, club, shotMode));
}

static void SetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    SetCharacterAnimation(slotIndex, GetSwingAnimation(slotIndex, club, shotMode));
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207388_ovl_00);

void func_80207660_ovl_00(UnkB8 *player) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207668_ovl_00);

/* Hides the rings the ball passed, so that a new shot can pass them again */
void ClearPassedRings(UnkB8 *player) {
    s32 i;

    for (i = 0; i < RING_COUNT; i++) {
        if (player->ringStates[i] >= RING_STATE_PASSED) {
            player->ringStates[i] = RING_STATE_NONE;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802078F4_ovl_00);

void FreeCharacterAndReactionLight(void) {
    FreeCharacterSlot(0);
    TurnOffReactionLight();
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207A34_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207B8C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207BEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207CAC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207E88_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207F70_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802080B4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802080D4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208120_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020816C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802081F8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208354_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020845C_ovl_00);

/* Shows or hides the HUD's parts, apart from the club panel and the letterbox */
void SetRoundHudPartsShown(s8 isShown) {
    /* The original stores a never-set register to the stack */
    volatile s32 unused;
    s32 garbage;

    unused = garbage;
    g_RoundHud.unk7 = isShown;
    g_RoundHud.unk8 = isShown;
    g_RoundHud.unk9 = isShown;
    g_RoundHud.unkA = isShown;
    g_RoundHud.unkB = isShown;
    g_RoundHud.unkD = isShown;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208810_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208850_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802088B0_ovl_00);
