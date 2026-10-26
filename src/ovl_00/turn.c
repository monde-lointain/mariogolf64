#include "common.h"
#include "billboard.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "functions.h"
#include "math_util.h"
#include "reaction.h"
#include "round_events.h"
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
#define SHOT_KIND_MAX_COUNT 0xFF
#define MAX_RING_SHOT_CLEAR_COUNT 99

/* The voice that reads out the hole-out banners */
#define HOLE_OUT_CALL_HOLE_IN_ONE 0x84
#define HOLE_OUT_CALL_ALBATROSS 0x85
#define HOLE_OUT_CALL_EAGLE 0x86
#define HOLE_OUT_CALL_BIRDIE 0x87
#define HOLE_OUT_CALL_PAR 0x88
#define HOLE_OUT_CALL_FINISH 0x90
#define HOLE_OUT_CALL_COMPLETE 0x96
#define HOLE_OUT_CALL_BOGEY 0x98
#define HOLE_OUT_CALL_CHANNEL 3
#define HOLE_OUT_CALL_VOLUME 0x7F
/* How far the control pad tilts the stick, as GetTurnStickX and GetTurnStickY read it */
#define DPAD_STICK_TILT 40
#define LETTERBOX_STATE_UNK4 4
/* The aim camera stays inside the course's ground, in course units */
#define AIM_CAMERA_MAX_X 4347904.0f
#define AIM_CAMERA_MAX_Z 8542208.0f
/* How far above the ground under the ball the aim camera looks, in world units */
#define AIM_LOOK_AT_RISE 17.25f
/* Where the aim camera starts, in course units */
#define AIM_CAMERA_START_DISTANCE (-0xF000)
#define AIM_CAMERA_START_HEIGHT (-0x4E00)
/* The stick moves the aim camera's height by this much for every AIM_STICK_STEP of tilt */
#define AIM_CAMERA_HEIGHT_STEP 0x600
#define AIM_STICK_STEP 16
#define AIM_STICK_DEAD_ZONE 8
/* The stick moves the aim camera's distance for every AIM_DISTANCE_STICK_STEP of tilt */
#define AIM_DISTANCE_STICK_STEP 4
#define AIM_CAMERA_DISTANCE_STEP 0xF00
#define AIM_CAMERA_PUTT_DISTANCE_STEP 0x180
/* How far the C buttons move the aim camera each frame */
#define AIM_CAMERA_BUTTON_DISTANCE_STEP 42670.0810546875
#define AIM_CAMERA_BUTTON_HEIGHT_STEP 28446.720703125
#define AIM_CAMERA_BUTTON_PUTT_STEP 4267.00810546875
/* Shots count towards Course Points on their first flights only */
#define MAX_COUNTED_FLIGHT_COUNT 2

extern s32 D_800C3070;
extern s32 D_800C3080;
extern s32 D_800C3084;
extern s32 D_800C308C; /* The letterbox's state */
extern s32 D_80227194_ovl_00;
extern s32 sTurnTimerStart; /* Nothing reads it */
/* How many times the shot's flight has been shown; StartFlightCamera gets the earlier count */
extern s32 sShotFlightCount;

void SetCharacterTranslucent(u32 slotIndex, s32 value);
void PlaceAimCamera(void);
s32 func_8005F30C(s32 x, s32 z);
s32 HasNoRingsLeft(UnkB8 *player);
void RecordRingShotClear(s32 playerIndex);

/* Lets the Rumble Paks run again after nuContRmbForceStop */
void EndRumbleForceStop(void) {
    nuSiSendMesg(NU_CONT_RMB_FORCESTOPEND_MSG, NULL);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202A20_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202D04_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802038D0_ovl_00);

/* Places the aim camera on the aim line, looking at the ball */
#ifdef NON_MATCHING
void PlaceAimCamera(void) {
    Vec3 eye;
    f32 heading = (u16)g_CurrentShot.unk0 * BINARY_ANGLE_TO_RADIANS;

    eye[AXIS_X] = cosf(heading) * g_CurrentShot.aimCameraDistance + g_CurrentShot.ballPosition[AXIS_X];
    if (eye[AXIS_X] < 0.0f) {
        eye[AXIS_X] = 0.0f;
    } else if (eye[AXIS_X] >= AIM_CAMERA_MAX_X) {
        eye[AXIS_X] = AIM_CAMERA_MAX_X - 1.0f;
    }
    eye[AXIS_Z] = sinf(heading) * g_CurrentShot.aimCameraDistance + g_CurrentShot.ballPosition[AXIS_Z];
    if (eye[AXIS_Z] < 0.0f) {
        eye[AXIS_Z] = 0.0f;
    } else if (eye[AXIS_Z] >= AIM_CAMERA_MAX_Z) {
        eye[AXIS_Z] = AIM_CAMERA_MAX_Z - 1.0f;
    }
    eye[AXIS_Y] = GetGroundHeight(eye[AXIS_X], eye[AXIS_Z]);
    if (g_CurrentShot.ballPosition[AXIS_Y] < eye[AXIS_Y]) {
        eye[AXIS_Y] = g_CurrentShot.ballPosition[AXIS_Y];
    }
    g_Cameras[0].view.lookAt[AXIS_X] = g_CurrentShot.ballPosition[AXIS_X];
    g_Cameras[0].view.lookAt[AXIS_Z] = g_CurrentShot.ballPosition[AXIS_Z];
    eye[AXIS_X] *= COURSE_TO_WORLD_SCALE;
    eye[AXIS_Y] = (eye[AXIS_Y] + g_CurrentShot.aimCameraHeight) * COURSE_TO_WORLD_SCALE;
    eye[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.eye[AXIS_X] = eye[AXIS_X];
    g_Cameras[0].view.eye[AXIS_Y] = eye[AXIS_Y];
    g_Cameras[0].view.eye[AXIS_Z] = eye[AXIS_Z];
    g_Cameras[0].view.lookAt[AXIS_Y] =
        func_8005F30C(g_CurrentShot.ballPosition[AXIS_X], g_CurrentShot.ballPosition[AXIS_Z]) * COURSE_TO_WORLD_SCALE
        - AIM_LOOK_AT_RISE;
    g_Cameras[0].view.lookAt[AXIS_X] *= COURSE_TO_WORLD_SCALE;
    g_Cameras[0].view.lookAt[AXIS_Z] *= COURSE_TO_WORLD_SCALE;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PlaceAimCamera);
#endif

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

void ResetAimCamera(void) {
    s32 unused[4];

    g_CurrentShot.aimCameraDistance = AIM_CAMERA_START_DISTANCE;
    g_CurrentShot.aimCameraHeight = AIM_CAMERA_START_HEIGHT;
    D_800C3070 = 0;
    D_800C3080 = 0;
    PlaceAimCamera();
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203F30_ovl_00);

/* Raises or lowers the aim camera with the C buttons, in smaller steps for a putt */
void MoveAimCameraHeightByButtons(void) {
    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_PUTT_STEP;
        } else if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_PUTT_STEP;
        }
    } else {
        if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_HEIGHT_STEP;
        } else if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802041C8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204210_ovl_00);

/* Moves the aim camera along the aim line with the C buttons, in smaller steps for a putt */
void MoveAimCameraDistanceByButtons(void) {
    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        if (g_TurnController.buttons & R_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_PUTT_STEP;
        } else if (g_TurnController.buttons & L_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_PUTT_STEP;
        }
    } else {
        if (g_TurnController.buttons & R_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_DISTANCE_STEP;
        } else if (g_TurnController.buttons & L_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_DISTANCE_STEP;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204460_ovl_00);

/* Raises or lowers the aim camera with the stick's Y */
void MoveAimCameraHeight(void) {
    if (ABS(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
        g_CurrentShot.aimCameraHeight -= g_TurnController.stickY / AIM_STICK_STEP * AIM_CAMERA_HEIGHT_STEP;
    }
}

/* Moves the aim camera along the aim line with the stick's Y, in smaller steps for a putt */
void MoveAimCameraDistance(void) {
    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        if (ABS(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
            g_CurrentShot.aimCameraDistance +=
                g_TurnController.stickY / AIM_DISTANCE_STICK_STEP * AIM_CAMERA_PUTT_DISTANCE_STEP;
        }
    } else {
        if (ABS(g_TurnController.stickY) > AIM_STICK_DEAD_ZONE) {
            g_CurrentShot.aimCameraDistance += g_TurnController.stickY / AIM_DISTANCE_STICK_STEP * AIM_CAMERA_DISTANCE_STEP;
        }
    }
}

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

    if ((s8)g_Players[playerIndex].unk1E[0] != 0 || g_GameState.unk31 || g_GameMode == GAME_MODE_TRAINING) {
        return;
    }
    if (g_GameMode == GAME_MODE_RING_SHOT || sShotFlightCount >= MAX_COUNTED_FLIGHT_COUNT) {
        return;
    }
    if (g_IsMiniGolf && shotKind != SHOT_KIND_HOLE_IN_ONE) {
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
        case SHOT_KIND_GREAT:
        case SHOT_KIND_EXCELLENT:
            points = 2;
            break;
        case SHOT_KIND_LONG_CHIP_IN:
            points = 8;
            break;
        case SHOT_KIND_PIN_SHOT:
        case SHOT_KIND_UNK5:
            points = 4;
            break;
        case SHOT_KIND_HOLE_IN_ONE:
            if (g_IsMiniGolf) {
                points = 10;
            } else {
                points = 20;
            }
            break;
        case SHOT_KIND_FLAG_SHOT:
        case SHOT_KIND_EAGLE:
        case SHOT_KIND_WATER_CUT:
            points = 10;
            break;
        case SHOT_KIND_ALBATROSS:
            points = 16;
            break;
        case SHOT_KIND_CHIP_IN:
        case SHOT_KIND_BIRDIE:
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

s32 HasNoRingsLeft(UnkB8 *player) {
    s32 i;

    for (i = 0; i < RING_COUNT; i++) {
        if (player->ringStates[i] != RING_STATE_NONE) {
            return FALSE;
        }
    }
    return TRUE;
}

s32 AreAllRingsPassed(UnkB8 *player) {
    s32 i;

    for (i = 0; i < RING_COUNT; i++) {
        if (player->ringStates[i] == RING_STATE_SHOWN) {
            return FALSE;
        }
    }
    return TRUE;
}

void RecordRingShotClear(s32 playerIndex) {
    s8 *count = &GetSaveData()->hasRingShotStar[g_CourseIndex][g_HoleIndex];

    (*count)++;
    if (*count < 0) {
        *count = 0;
    } else if (*count > MAX_RING_SHOT_CLEAR_COUNT) {
        *count = MAX_RING_SHOT_CLEAR_COUNT;
    }
    if ((s8)g_Players[playerIndex].characterIndex < SAVE_UNK1344_ENTRY_COUNT) {
        count = &GetSaveData()->unk1344[g_CourseIndex][g_HoleIndex][(s8)g_Players[playerIndex].characterIndex];
        (*count)++;
        if (*count < 0) {
            *count = 0;
        } else if (*count > MAX_RING_SHOT_CLEAR_COUNT) {
            *count = MAX_RING_SHOT_CLEAR_COUNT;
        }
    }
    WriteSaveData();
}

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

/* Leaves the distances that the player's contests have not measured uncounted */
void SkipUnmeasuredDistances(s32 playerIndex) {
    if (g_Players[playerIndex].driveDistance == UNMEASURED_DISTANCE) {
        g_Players[playerIndex].driveDistance = UNCOUNTED_DISTANCE;
    }
    if (g_Players[playerIndex].pinDistance == UNMEASURED_DISTANCE) {
        g_Players[playerIndex].pinDistance = UNCOUNTED_DISTANCE;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207BEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207CAC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207E88_ovl_00);

void ShowHoleOutBanner(s32 playerIndex) {
    s8 strokeCount = g_Players[playerIndex].strokeCount;
    s32 scoreToPar = strokeCount - g_HolePar;
    s32 banner = BANNER_NONE;

    if (g_GameMode == GAME_MODE_SPEED_GOLF) {
        StartBanner(BANNER_FINISH);
        PlaySound(HOLE_OUT_CALL_FINISH, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
    } else if (g_GameMode == GAME_MODE_RING_SHOT) {
        if (AreAllRingsPassed(&g_Players[playerIndex])) {
            StartBanner(BANNER_COMPLETE);
            PlaySound(HOLE_OUT_CALL_COMPLETE, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
        }
    } else if (strokeCount == 1) {
        banner = BANNER_HOLE_IN_ONE;
        PlaySound(HOLE_OUT_CALL_HOLE_IN_ONE, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
    } else {
        switch (scoreToPar) {
            case -1:
                banner = BANNER_BIRDIE;
                PlaySound(HOLE_OUT_CALL_BIRDIE, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
                break;
            case -2:
                banner = BANNER_EAGLE;
                PlaySound(HOLE_OUT_CALL_EAGLE, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
                break;
            case -3:
                banner = BANNER_ALBATROSS;
                PlaySound(HOLE_OUT_CALL_ALBATROSS, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
                break;
            case 0:
                banner = BANNER_PAR;
                PlaySound(HOLE_OUT_CALL_PAR, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
                break;
            case 1:
                banner = BANNER_BOGEY;
                PlaySound(HOLE_OUT_CALL_BOGEY, HOLE_OUT_CALL_CHANNEL, HOLE_OUT_CALL_VOLUME);
                break;
            case 2:
                banner = BANNER_STROKES_OVER_PAR + 2;
                break;
            case 3:
                banner = BANNER_STROKES_OVER_PAR + 3;
                break;
            default:
                banner = BANNER_STROKES_OVER_PAR + scoreToPar;
                break;
        }
    }
    if (banner >= 0) {
        StartBanner(banner);
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802080B4_ovl_00);

/* The turn controller's stick X, which left and right on the control pad tilt fully */
#ifdef NON_MATCHING
s8 GetTurnStickX(void) {
    /* The original stores a never-set register to the stack */
    volatile s32 unused;
    s32 garbage;
    s32 x;

    unused = garbage;
    x = (g_TurnController.buttons & L_JPAD) ? -DPAD_STICK_TILT : 0;
    if (g_TurnController.buttons & R_JPAD) {
        x = DPAD_STICK_TILT;
    }
    if (x == 0) {
        x = g_TurnController.stickX;
    }
    return x;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", GetTurnStickX);
#endif

/* The turn controller's stick Y, which down and up on the control pad tilt fully */
#ifdef NON_MATCHING
s8 GetTurnStickY(void) {
    /* The original stores a never-set register to the stack */
    volatile s32 unused;
    s32 garbage;
    s32 y;

    unused = garbage;
    y = (g_TurnController.buttons & D_JPAD) ? -DPAD_STICK_TILT : 0;
    if (g_TurnController.buttons & U_JPAD) {
        y = DPAD_STICK_TILT;
    }
    if (y == 0) {
        y = g_TurnController.stickY;
    }
    return y;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", GetTurnStickY);
#endif

/* Makes the golfer translucent while a club for the green is held */
#ifdef NON_MATCHING
void UpdateGolferTranslucency(void) {
    /* The original reads the player index through a never-set pointer */
    s32 *volatile unused;
    s32 *playerIndex;

    unused = playerIndex;
    if ((GetClub(g_Players[*playerIndex].clubs[g_CurrentShot.clubSlot])->surfaceMask & CLUB_SURFACE_GREEN)
        && D_800C308C != LETTERBOX_STATE_UNK4) {
        SetCharacterTranslucent(0, TRUE);
    } else {
        SetCharacterTranslucent(0, FALSE);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", UpdateGolferTranslucency);
#endif

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
