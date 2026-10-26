#include "common.h"
#include "ball_simulation.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "current_shot.h"
#include "debug_text.h"
#include "effects.h"
#include "functions.h"
#include "game_flags.h"
#include "golf_balls.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "math_util.h"
#include "overlay_manager.h"
#include "pin.h"
#include "reaction.h"
#include "round_events.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "shot_simulation.h"
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
/* Where the aim camera eases to, in course units */
#define AIM_CAMERA_PUTT_DISTANCE (-0x1800C)
#define AIM_CAMERA_APPROACH_BACKOFF 0x4B000 /* Back from where the shot lands */
#define AIM_CAMERA_SHOT_BACKOFF_YARDS 50    /* Short of the shot's full distance */
#define AIM_CAMERA_PUTT_HEIGHT (-0xB400)
#define AIM_CAMERA_APPROACH_HEIGHT (-0x12C00)
#define AIM_CAMERA_SHOT_HEIGHT (-0x25800)
/* The aim camera moves this fraction of the way each frame, up to the most */
#define AIM_CAMERA_EASE_FRACTION 0.3f
#define AIM_CAMERA_MAX_EASE_STEP (10 * UNITS_PER_YARD)
/* The free-look camera starts behind the ball, and C-left and C-right turn it */
#define FREE_LOOK_START_DISTANCE (-0x12C00)
#define FREE_LOOK_TURN_STEP 200
#define FREE_LOOK_HUD_FLAGS 0x47
#define LETTERBOX_STATE_UNK1 1
#define FREE_LOOK_FRAMEBUFFER_COUNT 3
/* A shorter pulse than the shots' rumble in effects.c */
#define TURN_RUMBLE_FREQUENCY 0x100
#define TURN_RUMBLE_FRAME_COUNT 8
/* Players in these states may make their characters call out during another player's turn */
#define PLAYER_STATE_UNK11 0x11
#define PLAYER_STATE_UNK14 0x14
/* The character sound that each button plays; the D-pad and C buttons give eight calls */
#define CALL_OUT_SOUND_UP 0
#define CALL_OUT_SOUND_DOWN 2
#define CALL_OUT_SOUND_LEFT 4
#define CALL_OUT_SOUND_RIGHT 6
#define CALL_OUT_SOUND_C_UP 8
#define CALL_OUT_SOUND_C_DOWN 10
#define CALL_OUT_SOUND_C_LEFT 12
#define CALL_OUT_SOUND_C_RIGHT 14
#define CALL_OUT_VOLUME 0x7F
/* Every AMBIENT_SOUND_INTERVAL frames, one of the course's ambient sounds may play */
#define AMBIENT_SOUND_INTERVAL 240
#define AMBIENT_SOUND_CHOICES 7
#define NO_AMBIENT_SOUND (-1)
#define AMBIENT_SOUND_CHANNEL 10
#define AMBIENT_SOUND_VOLUME 0x5F
#define AMBIENT_SOUND_REVERB 15
#define SOUND_PAN_RANGE 255
/* The most shots that each kind of replay record keeps */
#define HOLE_RECORD_MAX_STROKES 5
#define RING_SHOT_RECORD_MAX_STROKES 6
/* Only hole-outs in fewer strokes than this enter the records */
#define MAX_RECORDED_STROKE_COUNT 4
/* The stopwatches stop at an hour */
#define STOPWATCH_MAX_FRAMES 108000
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
/* While putting, the crane's steps are this times a count that grows with the putt's length */
#define AIM_CAMERA_PUTT_STEP_UNIT 426.700810546875
/* The crane swings the camera forward only while it is below this height, and back only above the other */
#define AIM_CAMERA_CRANE_FORWARD_HEIGHT (-0x5A00)
#define AIM_CAMERA_CRANE_BACK_HEIGHT (-0x96000)
/* Shots count towards Course Points on their first flights only */
#define MAX_COUNTED_FLIGHT_COUNT 2

extern s32 D_800C3070;
extern s32 D_801051F0; /* Set unless two players share a controller */
extern s32 D_80223FF8_ovl_00;
extern s32 D_80227154_ovl_00; /* The club slot that the golfer was last posed with */
extern s32 D_8022716C_ovl_00; /* The shot mode that the golfer was last posed with */
extern PlayRecord D_800FEDD8[];
extern RingShotRecord D_80105F40[];
extern s32 D_80223FEC_ovl_00; /* Frames since the last ambient sound */
/* The ambient sounds that each course may play, or NO_AMBIENT_SOUND */
extern s32 D_80223EF0_ovl_00[COURSE_RECORD_COUNT][AMBIENT_SOUND_CHOICES];
extern s16 D_800C307A; /* The free-look camera's heading, in binary angle units */
extern s32 D_800C3080;
extern s32 D_800C3084;
extern s32 D_800C308C; /* The letterbox's state */
extern s32 D_80227194_ovl_00;
extern s32 sTurnTimerStart; /* Nothing reads it */
/* How many times the shot's flight has been shown; StartFlightCamera gets the earlier count */
extern s32 sShotFlightCount;

void SetCharacterTranslucent(u32 slotIndex, s32 value);
void PlaceAimCamera(void);
void SetFramebufferCount(s32 count);
s32 func_8005F30C(s32 x, s32 z);
void func_8006034C(s8 value);
s32 func_80060190(s32 strokes);
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

/* Switches from the pause menu to the free-look camera, facing along the aim line */
void StartFreeLook(void) {
    g_CurrentShot.aimCameraDistance = FREE_LOOK_START_DISTANCE;
    D_800C308C = LETTERBOX_STATE_UNK1;
    D_800C3070 = 0;
    g_IsSettingImpactPoint = FALSE;
    g_RoundHud.flags = FREE_LOOK_HUD_FLAGS;
    D_800C307A = g_CurrentShot.unk0;
    SetMenuBackdrop(MENU_BACKDROP_PATTERN);
    g_PauseScreen = PAUSE_SCREEN_UNK4;
    g_RoundHud.isLetterboxShown = FALSE;
    g_RoundHud.unkC = 0;
    if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
        SetFramebufferCount(FREE_LOOK_FRAMEBUFFER_COUNT);
        SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    }
    StopSound(IMPACT_POINT_SOUND_CHANNEL);
}

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

/* Eases the aim camera towards the place that suits the shot, and stops once it is close */
#ifdef NON_MATCHING
void EaseAimCamera(void) {
    s32 targetDistance;
    s32 targetHeight;

    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        targetDistance = AIM_CAMERA_PUTT_DISTANCE;
    } else if (D_800C3084 & SHOT_UNK3084_APPROACH) {
        targetDistance = g_CurrentShot.unk50 - AIM_CAMERA_APPROACH_BACKOFF;
    } else {
        targetDistance = (GetSelectedShotDistance() - AIM_CAMERA_SHOT_BACKOFF_YARDS) * UNITS_PER_YARD;
    }
    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        targetHeight = AIM_CAMERA_PUTT_HEIGHT;
    } else if (D_800C3084 & SHOT_UNK3084_APPROACH) {
        targetHeight = AIM_CAMERA_APPROACH_HEIGHT;
    } else {
        targetHeight = AIM_CAMERA_SHOT_HEIGHT;
    }
    if (fabsf(g_CurrentShot.aimCameraDistance - targetDistance) > AIM_CAMERA_MAX_EASE_STEP) {
        if (g_CurrentShot.aimCameraDistance > targetDistance) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_MAX_EASE_STEP;
        } else {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_MAX_EASE_STEP;
        }
    } else {
        g_CurrentShot.aimCameraDistance +=
            (targetDistance - g_CurrentShot.aimCameraDistance) * AIM_CAMERA_EASE_FRACTION;
    }
    g_CurrentShot.aimCameraHeight += (targetHeight - g_CurrentShot.aimCameraHeight) * AIM_CAMERA_EASE_FRACTION;
    if (fabsf(g_CurrentShot.aimCameraDistance - targetDistance) < UNITS_PER_YARD) {
        D_800C3070 = 0;
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", EaseAimCamera);
#endif

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

/* Turns the free-look camera with C-left and C-right */
void TurnFreeLook(void) {
    if (g_TurnController.buttons & L_CBUTTONS) {
        D_800C307A -= FREE_LOOK_TURN_STEP;
    } else if (g_TurnController.buttons & R_CBUTTONS) {
        D_800C307A += FREE_LOOK_TURN_STEP;
    }
}

/* Swings the aim camera up and back, or down and forward, with C-right and C-left */
void CraneAimCamera(void) {
    s32 stepCount;

    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        if (g_CurrentShot.shotMode == PUTT_MODE_SHORT) {
            stepCount = 10;
        } else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
            stepCount = 13;
        } else {
            stepCount = 15;
        }
        if (g_TurnController.buttons & L_CBUTTONS) {
            g_CurrentShot.aimCameraHeight += stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        } else if (g_TurnController.buttons & R_CBUTTONS) {
            g_CurrentShot.aimCameraHeight -= stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        }
    } else if (g_TurnController.buttons & L_CBUTTONS) {
        g_CurrentShot.aimCameraHeight += AIM_CAMERA_BUTTON_HEIGHT_STEP;
        if (g_CurrentShot.aimCameraHeight < AIM_CAMERA_CRANE_FORWARD_HEIGHT) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
    } else if (g_TurnController.buttons & R_CBUTTONS) {
        g_CurrentShot.aimCameraHeight -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
        if (g_CurrentShot.aimCameraHeight > AIM_CAMERA_CRANE_BACK_HEIGHT) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_HEIGHT_STEP;
        }
    }
}

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

/* Moves the aim camera along the aim line with C-up and C-down, by steps that suit a putt's length */
void MoveAimCameraDistanceByUpDown(void) {
    s32 stepCount;

    if (D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT) {
        if (g_CurrentShot.shotMode == PUTT_MODE_SHORT) {
            stepCount = 10;
        } else if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
            stepCount = 15;
        } else {
            stepCount = 20;
        }
        if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        } else if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= stepCount * AIM_CAMERA_PUTT_STEP_UNIT;
        }
    } else {
        if (g_TurnController.buttons & U_CBUTTONS) {
            g_CurrentShot.aimCameraDistance += AIM_CAMERA_BUTTON_DISTANCE_STEP;
        } else if (g_TurnController.buttons & D_CBUTTONS) {
            g_CurrentShot.aimCameraDistance -= AIM_CAMERA_BUTTON_DISTANCE_STEP;
        }
    }
}

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

/* Puts the simulated ball at rest on the player's ball, and shows the player's passed rings again */
void PlaceBallAtRest(s32 playerIndex, BallSimulation *simulation) {
    UnkB8 *player = &g_Players[playerIndex];
    s32 i;
    s32 groundHeight;

    StopSound(IMPACT_POINT_SOUND_CHANNEL);
    for (i = 0; i < RING_COUNT; i++) {
        if (player->ringStates[i] != RING_STATE_NONE) {
            player->ringStates[i] = RING_STATE_SHOWN;
        }
        func_8006034C(0);
    }
    player->unkA8 = 0;
    simulation->position[AXIS_X] = player->ballPosition[AXIS_X];
    simulation->position[AXIS_Y] = player->ballPosition[AXIS_Y];
    simulation->position[AXIS_Z] = player->ballPosition[AXIS_Z];
    simulation->playerIndex = playerIndex;
    simulation->unkA0 = 0;
    simulation->isRunning = FALSE;
    groundHeight = GetGroundHeight(simulation->position[AXIS_X], simulation->position[AXIS_Z]);
    if (groundHeight < simulation->position[AXIS_Y]) {
        simulation->position[AXIS_Y] = groundHeight;
    }
    simulation->surface = player->ballSurface;
    simulation->purpose = LAUNCH_PURPOSE_UNK3;
    simulation->unkA1 = 0;
    PrepareShotEffects(simulation);
    if (g_IsModuleAlive[MODULE_UNK1] || ReportModuleNotAlive(MODULE_UNK1)) {
        ResetBallAfterimage();
    }
}

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

/*
 * Returns whether the player's hole-out may enter the records: a short one,
 * by one of the main characters, outside mini-golf, in a mode that keeps them
 */
s32 IsRecordableHoleOut(s32 playerIndex) {
    if (g_GameState.unk31) {
        return FALSE;
    }
    if (!IsGameFlagSet(GAME_FLAG_RECORD_ALL_HOLE_OUTS)) {
        if ((s8)g_Players[playerIndex].unk1E[0] != 0) {
            return FALSE;
        }
        if (g_GameMode == GAME_MODE_TRAINING) {
            return FALSE;
        }
    }
    if (g_Players[playerIndex].strokeCount > MAX_RECORDED_STROKE_COUNT) {
        return FALSE;
    }
    if ((s8)g_Players[playerIndex].characterIndex >= CHARACTER_KID) {
        return FALSE;
    }
    if (g_IsMiniGolf) {
        return FALSE;
    }
    switch (g_GameMode) {
        case GAME_MODE_STROKE:
        case GAME_MODE_UNK1:
        case GAME_MODE_TRAINING:
        case GAME_MODE_GET_CHARACTER:
        case GAME_MODE_TOURNAMENT:
        case GAME_MODE_SPEED_GOLF:
            return TRUE;
        case GAME_MODE_MATCH_GAME:
        case GAME_MODE_UNK6:
        case GAME_MODE_UNK8:
        case GAME_MODE_RING_SHOT:
        case GAME_MODE_SKINS_MATCH:
            return FALSE;
    }
    return FALSE;
}

s32 IsRingShotRound(void) {
    if (!g_GameState.unk31) {
        return g_GameMode == GAME_MODE_RING_SHOT;
    }
    return FALSE;
}

/* Keeps the player's last shot, and the hole and how it was played, in their replay record */
void RecordHoleOut(s32 playerIndex) {
    u32 shotIndex = g_Players[playerIndex].strokeCount - 1;

    if (shotIndex < HOLE_RECORD_MAX_STROKES) {
        PlayRecord *record = &D_800FEDD8[playerIndex];

        /* Read through g_GameState, which keeps GCC from loading the fields early, to match */
        record->gameMode = g_GameState.gameMode;
        record->courseIndex = g_GameState.courseIndex;
        record->holeIndex = g_GameState.holeIndex;
        record->scoreKind = func_80060190(g_Players[playerIndex].strokeCount);
        record->characterIndex = g_Players[playerIndex].characterIndex;
        record->unk3C = g_Players[playerIndex].unk22;
        record->paletteIndex = g_Players[playerIndex].paletteIndex;
        record->strokeCount = g_Players[playerIndex].strokeCount;
        record->unk40 = g_Players[playerIndex].unk88;
        record->hole = D_800BA9D8;
        record->shots[shotIndex].heading = g_CurrentShot.unk0;
        record->shots[shotIndex].unk2 = g_CurrentShot.unk4;
        record->shots[shotIndex].unk3 = g_CurrentShot.unk8;
        record->shots[shotIndex].shotMode = g_CurrentShot.shotMode;
        record->shots[shotIndex].clubSlot = g_CurrentShot.clubSlot;
        record->shots[shotIndex].unk6 = g_CurrentShot.unk14;
        record->shots[shotIndex].unk7 = g_CurrentShot.unk18;
    }
}

/* Keeps the player's last shot, and the hole and how it was played, in their Ring Shot record */
void RecordRingShotHole(s32 playerIndex) {
    u32 shotIndex = g_Players[playerIndex].strokeCount - 1;

    if (shotIndex < RING_SHOT_RECORD_MAX_STROKES) {
        RingShotRecord *record = &D_80105F40[playerIndex];

        record->courseIndex = g_CourseIndex;
        record->holeIndex = g_HoleIndex;
        record->scoreKind = func_80060190(g_Players[playerIndex].strokeCount);
        record->characterIndex = g_Players[playerIndex].characterIndex;
        record->unk38 = g_Players[playerIndex].unk22;
        record->paletteIndex = g_Players[playerIndex].paletteIndex;
        record->strokeCount = g_Players[playerIndex].strokeCount;
        record->unk3C = g_Players[playerIndex].unk88;
        record->hole = D_800BA9D8;
        record->shots[shotIndex].heading = g_CurrentShot.unk0;
        record->shots[shotIndex].unk2 = g_CurrentShot.unk4;
        record->shots[shotIndex].unk3 = g_CurrentShot.unk8;
        record->shots[shotIndex].shotMode = g_CurrentShot.shotMode;
        record->shots[shotIndex].clubSlot = g_CurrentShot.clubSlot;
        record->shots[shotIndex].unk6 = g_CurrentShot.unk14;
        record->shots[shotIndex].unk7 = g_CurrentShot.unk18;
    }
}

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

/* Now and then plays one of the course's ambient sounds, picked at random, from a random side */
void PlayAmbientSound(void) {
    s32 choice;

    if (++D_80223FEC_ovl_00 == AMBIENT_SOUND_INTERVAL) {
        choice = guRandom() % AMBIENT_SOUND_CHOICES;
        D_80223FEC_ovl_00 = 0;
        if (D_80223EF0_ovl_00[GetCourseRecordIndex()][choice] != NO_AMBIENT_SOUND) {
            PlaySoundWithEffects(D_80223EF0_ovl_00[GetCourseRecordIndex()][choice], AMBIENT_SOUND_CHANNEL,
                                 AMBIENT_SOUND_VOLUME, AMBIENT_SOUND_REVERB, guRandom() % SOUND_PAN_RANGE);
        }
    }
}

void FreeCharacterAndReactionLight(void) {
    FreeCharacterSlot(0);
    TurnOffReactionLight();
}

/*
 * Measures a tee shot for the records: the drive counts if it ends on the fairway or the
 * green, and the distance to the pin if it ends on the green.
 */
void MeasureTeeShot(s32 playerIndex) {
    s32 distance;
    SurfaceInfo *info;

    if (g_Players[playerIndex].strokeCount == 1) {
        distance = UNCOUNTED_DISTANCE;
        info = GetSurfaceInfo(g_SimulatedShot.endSurface);
        switch ((u32)info->kind) {
            case SURFACE_KIND_FAIRWAY:
            case SURFACE_KIND_FAIRWAY_FAIR_LIE:
            case SURFACE_KIND_FAIRWAY_POOR_LIE:
            case SURFACE_KIND_GREEN:
            case SURFACE_KIND_GREEN_EDGE:
                distance = IntLength2D(g_SimulatedShot.startPosition[AXIS_X] - g_SimulatedShot.endPosition[AXIS_X],
                                       g_SimulatedShot.startPosition[AXIS_Z] - g_SimulatedShot.endPosition[AXIS_Z]);
                break;
        }
        g_Players[playerIndex].driveDistance = distance;
        distance = UNCOUNTED_DISTANCE;
        if (g_SimulatedShot.unk2E10) {
            distance = 0;
        } else if (info->kind == SURFACE_KIND_GREEN) {
            distance = GetCupDistance(g_CupPositionX - g_SimulatedShot.endPosition[AXIS_X],
                                      g_CupPositionZ - g_SimulatedShot.endPosition[AXIS_Z]);
        }
        g_Players[playerIndex].pinDistance = distance;
    }
}

/* Leaves the distances that the player's contests have not measured uncounted */
void SkipUnmeasuredDistances(s32 playerIndex) {
    if (g_Players[playerIndex].driveDistance == UNMEASURED_DISTANCE) {
        g_Players[playerIndex].driveDistance = UNCOUNTED_DISTANCE;
    }
    if (g_Players[playerIndex].pinDistance == UNMEASURED_DISTANCE) {
        g_Players[playerIndex].pinDistance = UNCOUNTED_DISTANCE;
    }
}

/*
 * Rumbles the player's controller briefly in a game for several players,
 * if every player has a controller of their own
 */
void StartTurnRumble(s32 playerIndex) {
    if (D_801051F0 == TRUE && g_PlayerCount >= 2) {
        s32 controllerIndex = g_Players[playerIndex].controllerIndex;

        EndRumbleForceStop();
        if ((u32)controllerIndex < MAXCONTROLLERS && !g_GameState.unk31) {
            switch (nuContRmbCheck(g_PlayerControllerPorts[controllerIndex])) {
                case 0:
                    nuContRmbStart(g_PlayerControllerPorts[controllerIndex], TURN_RUMBLE_FREQUENCY,
                                   TURN_RUMBLE_FRAME_COUNT);
                    break;
                /* The empty cases are there to match */
                case PFS_ERR_NOPACK:
                    break;
                case PFS_ERR_CONTRFAIL:
                    break;
            }
        }
    }
}

/*
 * Lets the players who are waiting make their characters call out with the
 * D-pad and the C buttons, if every player has a controller of their own
 */
void PlayWaitingPlayersCalls(void) {
    s32 i;
    s32 controllerIndex;
    u16 pressed;

    if (D_801051F0 == TRUE) {
        for (i = 0; i < g_PlayerCount; i++) {
            if ((g_Players[i].unkC == PLAYER_STATE_UNK11) | (g_Players[i].unkC == PLAYER_STATE_UNK14)) {
                controllerIndex = g_Players[i].controllerIndex;
                if (controllerIndex == g_Players[g_GameState.unk20].controllerIndex) {
                    continue;
                }
                if (controllerIndex >= MAXCONTROLLERS) {
                    continue;
                }
                pressed = g_ControllerStates[controllerIndex].pressedButtons;
                if (pressed & U_JPAD) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_UP, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & D_JPAD) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_DOWN, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & L_JPAD) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_LEFT, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & R_JPAD) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_RIGHT, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & U_CBUTTONS) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_C_UP, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & D_CBUTTONS) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_C_DOWN, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & L_CBUTTONS) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_C_LEFT, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                } else if (pressed & R_CBUTTONS) {
                    PlayCharacterSound((s8)g_Players[i].characterIndex, i, CALL_OUT_SOUND_C_RIGHT, CALL_OUT_VOLUME,
                                       g_Players[i].soundPan);
                }
            }
        }
    }
}

/* Plays the jingle for how the player finished the hole */
void PlayHoleOutSong(s32 playerIndex) {
    s32 strokeCount = g_Players[playerIndex].strokeCount;
    s32 scoreToPar = strokeCount - g_HolePar;

    if (g_GameMode == GAME_MODE_SPEED_GOLF) {
        PlaySong(SONG_HOLE_WON, 0);
    } else if (g_GameMode == GAME_MODE_RING_SHOT) {
        if (AreAllRingsPassed(&g_Players[playerIndex])) {
            PlaySong(SONG_HOLE_WON, 0);
        } else {
            PlaySong(SONG_HOLE_LOST, 0);
        }
    } else if (strokeCount == 1) {
        PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
    } else {
        switch (scoreToPar) {
            case -1:
                PlaySong(SONG_HOLE_OUT_BIRDIE, 0);
                break;
            case -2:
                PlaySong(SONG_HOLE_OUT_EAGLE, 0);
                break;
            case -3:
                PlaySong(SONG_HOLE_OUT_EAGLE, 0);
                break;
            case 0:
                PlaySong(SONG_HOLE_OUT_PAR, 0);
                break;
            case 1:
                PlaySong(SONG_HOLE_OUT_BOGEY, 0);
                break;
            case 2:
            case 3:
            default:
                PlaySong(SONG_HOLE_OUT_OVER_BOGEY, 0);
                break;
        }
    }
}

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

/*
 * The functions from here to func_802088B0 are nested in it in the original,
 * a GNU C extension. GCC passes them the parent's frame in $v0, its static
 * chain, and they read the parent's playerIndex through it. As nested
 * functions they match, but they can only be written so once their parent
 * is C.
 */
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802080B4_ovl_00);

/* The turn controller's stick X, which left and right on the control pad tilt fully */
#ifdef NON_MATCHING
s8 GetTurnStickX(void) {
    /* Stands in for the static chain that the original, nested in func_802088B0, saves */
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
    /* Stands in for the static chain that the original, nested in func_802088B0, saves */
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
    /* Stands in for the static chain, through which the original reads its parent's playerIndex */
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

/*
 * Poses the golfer for the picked club and shot mode, when either has changed. An approach
 * shows the swing's first frame, held; the other modes show the stance.
 */
#ifdef NON_MATCHING
void PoseGolferForShot(void) {
    /* Stands in for the static chain, through which the original reads its parent's playerIndex */
    s32 *volatile unused;
    s32 *playerIndex;

    s32 *lastClubSlot = &D_80227154_ovl_00;

    unused = playerIndex;
    if (*lastClubSlot != g_CurrentShot.clubSlot || D_8022716C_ovl_00 != g_CurrentShot.shotMode) {
        *lastClubSlot = g_CurrentShot.clubSlot;
        D_8022716C_ovl_00 = g_CurrentShot.shotMode;
        SetCharacterClub(0, g_Players[*playerIndex].clubs[g_CurrentShot.clubSlot]);
        UpdateGolferTranslucency();
        if (g_CurrentShot.shotMode == SHOT_MODE_APPROACH) {
            SetSwingAnimation(0, g_Players[*playerIndex].clubs[g_CurrentShot.clubSlot], SHOT_MODE_APPROACH);
            SetAnimationSpeed(0, 0);
        } else {
            SetStanceAnimation(0, g_Players[*playerIndex].clubs[g_CurrentShot.clubSlot], g_CurrentShot.shotMode);
            SkipToAnimationEnd(0);
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", PoseGolferForShot);
#endif

/* Advances the player's, the round's and the hole's stopwatches by a frame, up to their limit */
#ifdef NON_MATCHING
void TickStopwatches(void) {
    /* Stands in for the static chain, through which the original reads its parent's playerIndex */
    s32 *volatile unused;
    s32 *playerIndex;

    unused = playerIndex;
    if (D_80223FF8_ovl_00 < 2) {
        g_Players[*playerIndex].stopwatchFrameCount++;
        g_GameState.stopwatchFrameCount++;
        g_HoleTimeFrames[g_HoleIndex]++;
        if (g_Players[*playerIndex].stopwatchFrameCount > STOPWATCH_MAX_FRAMES) {
            g_Players[*playerIndex].stopwatchFrameCount = STOPWATCH_MAX_FRAMES;
        }
        if (g_GameState.stopwatchFrameCount > STOPWATCH_MAX_FRAMES) {
            g_GameState.stopwatchFrameCount = STOPWATCH_MAX_FRAMES;
        }
        if (g_HoleTimeFrames[g_HoleIndex] > STOPWATCH_MAX_FRAMES) {
            g_HoleTimeFrames[g_HoleIndex] = STOPWATCH_MAX_FRAMES;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", TickStopwatches);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020845C_ovl_00);

/* Shows or hides the HUD's parts, apart from the club panel and the letterbox */
void SetRoundHudPartsShown(s8 isShown) {
    /* Stands in for the static chain that the original, nested in func_802088B0, saves */
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
