#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "debug_text.h"
#include "framing.h"
#include "functions.h"
#include "game_flags.h"
#include "heap.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "variables.h"

/*
 * The staff roll, module 0x1F, shown in scene 30. It scrolls the staff
 * text a pixel a frame, has characters from the cast swing on Yoshi's
 * Island's seventh hole, and ends on a closing picture.
 */

/* The cast leaves out one character, the one after the start ShuffleCastOrder is given */
#define CAST_MEMBER_COUNT (CHARACTER_COUNT - 1)
#define FIRST_CAST_OFFSET 2
#define SHUFFLE_ROUND_COUNT 10
#define SHUFFLED_MEMBER_COUNT 16

/* The cast's character buffers, given to the slots from FIRST_CAST_SLOT on */
#define CAST_BUFFER_COUNT 1
#define CAST_BUFFER_CAPACITY 3
#define FIRST_CAST_SLOT 1

#define PAINTING_ASSET 0x959
#define STAFF_FONT_ASSET 0x94C
#define SHADOW_TEXTURE_ASSET 0x4C3
/* Big enough for the painting and for the closing picture read over it later */
#define STAFF_ROLL_IMAGE_SIZE 0x2AC00
#define BILLBOARD_HEAP_SIZE 0x7038
#define CLOSING_PICTURE_ASSET 0x967
#define STAFF_ROLL_FRAMEBUFFER_COUNT 3

/* The staff text's first bytes hold its size, big-endian */
#define STAFF_TEXT_HEADER_SIZE 8
#define STAFF_GLYPH_WIDTH 12
/* MeasureStaffLine gives these characters no width */
#define FIRST_UNMEASURED_CHARACTER 0xDE
#define UNMEASURED_CHARACTER_COUNT 2

#define STAFF_ROLL_HOLE_INDEX 6
#define STAFF_ROLL_WIND_STEP 6000
#define STAFF_ROLL_WIND_STEP_MASK 0x3F
#define STAFF_ROLL_LIGHT_PITCH 0x1000
#define STAFF_ROLL_LIGHT_YAW 0x5800

/* sStaffRollFrame values at which UpdateStaffRoll acts */
#define STAFF_SONG_FRAME 60
#define TIMELINE_START_FRAME 280
#define CLOSING_PICTURE_READ_FRAME 1000
#define STAFF_SONG_END_FRAME 3688
#define CLOSING_FRAME 3798 /* The closing song starts, and the closing picture fades in */
#define FIRST_LEAVING_FRAME 4300
#define MAX_STAFF_ROLL_FRAME 0xFFFF
/* The scroll also stops this many frames after the lines' heights add up to the frame */
#define SCROLL_END_DELAY 376
#define STAFF_LINE_HEIGHT 24
#define FIRST_TOP_LINE_INDEX (-10)
#define DEBUG_SKIPPED_FRAMES 100

#define STAFF_SONG 0x31
#define CLOSING_SONG 0x32
#define EARLIER_SONG_FADE_FRAMES 40
#define STAFF_SONG_FADE_FRAMES 200

/* Where UpdateStaffRoll prints the line count */
#define LINE_COUNT_TEXT_COLUMN 5
#define LINE_COUNT_TEXT_ROW 12

/*
 * The painting is drawn in strips PAINTING_STRIP_STEP rows apart, each
 * PAINTING_STRIP_OVERLAP rows taller so that the next strip overlaps it.
 * The last strip is only the overlap, which ends the painting at the
 * screen's bottom border.
 */
#define PAINTING_WIDTH (SCREEN_WIDTH - 2 * SCREEN_BORDER)
#define PAINTING_STRIP_COUNT 38
#define PAINTING_STRIP_STEP 6
#define PAINTING_STRIP_OVERLAP 2
#define PAINTING_SCROLL_FRAME 60
#define PAINTING_SCROLL_LIMIT 228 /* Rows scrolled when the painting's bottom is shown */
#define PAINTING_OPAQUE_ALPHA 255
#define PAINTING_FADE_RATE 4      /* Alpha per frame */
#define PAINTING_FADE_IN_FRAMES 64
#define PAINTING_CLOUD_ALPHA 180  /* Below this the painting is drawn with G_RM_CLD_SURF */

/* GetGroundHeight's units, 1024 to a world unit */
#define COURSE_UNITS_PER_UNIT 1024.0f
#define QUARTER_TURN_RADIANS 1.5707964f
#define PITCH_STEP_COUNT 10
#define FRAMING_GROUND_CLEARANCE 0xC00

/* Each club kind has three animations in a row: a stance, a swing and an approach swing */
#define WOOD_ANIMATIONS 0
#define IRON_ANIMATIONS 3
#define WEDGE_ANIMATIONS 6
#define PUTTER_ANIMATIONS 9
#define SWING_ANIMATION 1

/* EaseStaffRollView's easedParts bits */
#define EASE_LOOK_AT (1 << 0)
#define EASE_EYE (1 << 1)

typedef struct StaffRollImage {
    /* 0x0 */ ImageHeader header;
    /* 0x8 */ u8 texels[STAFF_ROLL_IMAGE_SIZE];
} StaffRollImage; /* size = 0x2AC08 */

typedef struct StaffLine {
    /* 0x0 */ u16 start; /* In characters, from the end of the lines */
    /* 0x2 */ u16 length;
} StaffLine; /* size = 0x4 */

/* Read from the ROM; the 16-bit characters follow the lines */
typedef struct StaffText {
    /* 0x0 */ u16 size;
    /* 0x2 */ u16 lineCount;
    /* 0x4 */ StaffLine lines[1];
} StaffText;

typedef struct StaffRollEasedView {
    /* 0x00 */ f32 eye[AXIS_COUNT];
    /* 0x0C */ f32 lookAt[AXIS_COUNT];
    /* 0x18 */ s8 isEyeEased;
    /* 0x19 */ s8 isLookAtEased;
} StaffRollEasedView; /* size = 0x1C */

extern u8 g_StaffTextRomStart[];
extern u8 *g_ShadowTexture;
extern s32 g_PlayerCount;
extern s32 D_801B609C;

extern StaffRollImage sStaffRollImage;
extern StaffText *sStaffText;
extern u8 *sStaffFont;
extern void *sBillboardHeap;
extern u8 *sCastBuffers[CAST_BUFFER_CAPACITY];
extern StaffRollEasedView sStaffRollEasedView;
extern s32 sCastOrder[CAST_MEMBER_COUNT];
extern s32 sFadeFramesLeft;

void AssignCharacterBuffers(void);
void SetFramebufferCount(s32 count);
void func_80078910(void);
void func_800789C8(void);

u8 sIsPaintingShown = TRUE;
s32 sStaffRollFrame = 0;
s32 sIsLeavingStaffRoll = FALSE;
s32 sTextScrollOffset = 0;
s32 sTopLineIndex = 0;
s32 D_801F89B4_ovl_14 = -1;
s32 sIsTextScrolling = TRUE;
s32 sDebugRestartCount = 0;
/* Negative while fading out; zero when not fading */
s32 sFadeLength = 0;
f32 D_801F89C4_ovl_14 = 0.0f;
f32 D_801F89C8_ovl_14 = 0.0f;
s32 D_801F89CC_ovl_14 = 0;
s32 D_801F89D0_ovl_14 = 0;
f32 D_801F89D4_ovl_14 = 0.0f;
s32 D_801F89D8_ovl_14[2] = { 0x100000, 0 };
u8 D_801F89E0_ovl_14[256] = {
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 6, 12, 6, 12,
    8, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 10, 10, 12, 12, 12, 12,
    12, 11, 10, 8, 9, 8, 6, 10, 9, 4, 9, 7, 7, 12, 10, 12,
    9, 11, 9, 7, 9, 11, 12, 12, 10, 9, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
    0, 9, 7, 7, 9, 9, 8, 8, 8, 4, 5, 8, 4, 8, 8, 8,
    6, 9, 6, 6, 6, 9, 9, 12, 9, 10, 7, 0, 0, 0, 0, 0,
};

/* The kind of model the cast is loaded as, from the first player's unk7B */
s32 GetCastModelKind(void) {
    return g_Players[0].unk7B & (CHARACTER_MODEL_KIND_COUNT - 1);
}


/*
 * Fills sCastOrder with the characters that follow start, leaving out the
 * one just after it, then shuffles all but the last unless
 * GAME_FLAG_UNK99 is set. SwapCastMembers is a nested function, a GNU C
 * extension.
 */
void ShuffleCastOrder(s32 start) {
    s32 member;
    s32 index;
    s32 round;

    void SwapCastMembers(s32 first, s32 second) {
        s32 member = sCastOrder[first];

        sCastOrder[first] = sCastOrder[second];
        sCastOrder[second] = member;
    }

    for (member = 0; member < CAST_MEMBER_COUNT; member++) {
        sCastOrder[member] = (member + start + FIRST_CAST_OFFSET) % CHARACTER_COUNT;
    }
    if (!IsGameFlagSet(GAME_FLAG_UNK99)) {
        for (round = 0; round < SHUFFLE_ROUND_COUNT; round++) {
            for (index = 0; index < SHUFFLED_MEMBER_COUNT; index++) {
                SwapCastMembers(index, (rand() & 0x7FFF) % SHUFFLED_MEMBER_COUNT);
            }
        }
    }
}

/* Fades the screen to black over frameCount frames */
void StartFadeOut(s32 frameCount) {
    sFadeLength = -frameCount;
    sFadeFramesLeft = frameCount;
}

/* Fades the screen in from black over frameCount frames */
void StartFadeIn(s32 frameCount) {
    sFadeLength = frameCount;
    sFadeFramesLeft = frameCount;
}

void StopFade(void) {
    sFadeLength = 0;
}

/*
 * Module 0x1F's onLoad. It reads the painting, the font and the staff
 * text, and sets up a one-player game on Yoshi's Island's seventh hole
 * for the cast to play on.
 */
void LoadStaffRoll(void) {
    EncodedAsset asset;
    EncodedAsset shadowAsset;
    u8 *header;
    s32 index;
    s32 textSize;

    D_801F89B4_ovl_14 = 11;
    ShuffleCastOrder((s8)g_Players[0].unk1D);
    FreeAllCharacterSlots();
    for (index = 0; index < CAST_BUFFER_COUNT; index++) {
        sCastBuffers[index] = AllocMemory(GetCharacterBufferSize());
        SetCharacterBuffer(index + FIRST_CAST_SLOT, sCastBuffers[index]);
    }
    OpenEncodedAsset(PAINTING_ASSET, &asset);
    ReadEncodedAsset(&sStaffRollImage, &asset);
    sStaffFont = AllocMemory(OpenEncodedAsset(STAFF_FONT_ASSET, &asset));
    ReadEncodedAsset(sStaffFont, &asset);
    header = AllocMemory(STAFF_TEXT_HEADER_SIZE);
    nuPiReadRom((u32)g_StaffTextRomStart, header, STAFF_TEXT_HEADER_SIZE);
    textSize = header[0] << 8 | header[1];
    FreeMemory(&header);
    sStaffText = AllocMemory(textSize);
    nuPiReadRom((u32)g_StaffTextRomStart, sStaffText, textSize);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(SHADOW_TEXTURE_ASSET, &shadowAsset));
    ReadEncodedAsset(g_ShadowTexture, &shadowAsset);
    sBillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
    InitBillboardManager(sBillboardHeap, BILLBOARD_HEAP_SIZE);
    memset(&g_GameState, 0, sizeof(g_GameState));
    g_CourseIndex = COURSE_YOSHIS_ISLAND;
    g_HoleIndex = STAFF_ROLL_HOLE_INDEX;
    D_801B609C = 0;
    g_GameMode = GAME_MODE_STAFF_ROLL;
    g_HoleConditions.isRaining = FALSE;
    g_HoleConditions.windSpeed = (rand() & STAFF_ROLL_WIND_STEP_MASK) * STAFF_ROLL_WIND_STEP;
    g_HoleConditions.windDirection = rand();
    g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_LATE;
    g_LightPitch = STAFF_ROLL_LIGHT_PITCH;
    g_LightYaw = STAFF_ROLL_LIGHT_YAW;
    D_800BA9D8.cupPosition[AXIS_X] = g_HolePoints.cupPositions[0][AXIS_X];
    D_800BA9D8.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[0][AXIS_Y];
    D_800BA9D8.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[0][AXIS_Z];
    D_800BA9D8.unkC[AXIS_X] = g_HolePoints.unk30[0][AXIS_X];
    D_800BA9D8.unkC[AXIS_Y] = g_HolePoints.unk30[0][AXIS_Y];
    D_800BA9D8.unkC[AXIS_Z] = g_HolePoints.unk30[0][AXIS_Z];
    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    g_PlayerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
    SetFramebufferCount(STAFF_ROLL_FRAMEBUFFER_COUNT);
    sIsLeavingStaffRoll = FALSE;
    sTextScrollOffset = 0;
    sTopLineIndex = FIRST_TOP_LINE_INDEX;
    func_800789C8();
}

/* Module 0x1F's onUnload */
void UnloadStaffRoll(void) {
    s32 index;

    func_80078910();
    g_Cameras[0].view.scissorX = SCREEN_BORDER;
    g_Cameras[0].view.scissorY = SCREEN_BORDER;
    g_Cameras[0].view.scissorWidth = SCREEN_WIDTH - 2 * SCREEN_BORDER;
    g_Cameras[0].view.scissorHeight = SCREEN_HEIGHT - 2 * SCREEN_BORDER;
    g_Cameras[0].view.zoom = 1.0f;
    FreeAllCharacterSlots();
    for (index = 0; index < CAST_BUFFER_COUNT; index++) {
        FreeMemory(&sCastBuffers[index]);
    }
    AssignCharacterBuffers();
    SetFillColor(0, 0, 0);
    FreeMemory(&sBillboardHeap);
    FreeMemory(&sStaffText);
    FreeMemory(&sStaffFont);
    FreeMemory(&g_ShadowTexture);
}

/* BlendClamped again */
f32 BlendStaffRollClamped(f32 from, f32 to, f32 weight) {
    if (weight < 0.0f) {
        weight = 0.0f;
    }
    if (weight > 1.0f) {
        weight = 1.0f;
    }
    return from * (1.0f - weight) + to * weight;
}

void ResetStaffRollEase(void) {
    sStaffRollEasedView.isEyeEased = FALSE;
    sStaffRollEasedView.isLookAtEased = FALSE;
}

/* EaseReactionView again, with its own EasedView */
void EaseStaffRollView(f32 *eye, f32 *lookAt, f32 rate, s32 easedParts) {
    if (easedParts & EASE_EYE) {
        if (!sStaffRollEasedView.isEyeEased) {
            sStaffRollEasedView.eye[AXIS_X] = eye[AXIS_X];
            sStaffRollEasedView.eye[AXIS_Y] = eye[AXIS_Y];
            sStaffRollEasedView.eye[AXIS_Z] = eye[AXIS_Z];
            sStaffRollEasedView.isEyeEased = TRUE;
        }
        sStaffRollEasedView.eye[AXIS_X] = eye[AXIS_X] =
            BlendStaffRollClamped(sStaffRollEasedView.eye[AXIS_X], eye[AXIS_X], rate);
        sStaffRollEasedView.eye[AXIS_Y] = eye[AXIS_Y] =
            BlendStaffRollClamped(sStaffRollEasedView.eye[AXIS_Y], eye[AXIS_Y], rate);
        sStaffRollEasedView.eye[AXIS_Z] = eye[AXIS_Z] =
            BlendStaffRollClamped(sStaffRollEasedView.eye[AXIS_Z], eye[AXIS_Z], rate);
    }
    if (easedParts & EASE_LOOK_AT) {
        if (!sStaffRollEasedView.isLookAtEased) {
            sStaffRollEasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
            sStaffRollEasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
            sStaffRollEasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
            sStaffRollEasedView.isLookAtEased = TRUE;
        }
        sStaffRollEasedView.lookAt[AXIS_X] = lookAt[AXIS_X] =
            BlendStaffRollClamped(sStaffRollEasedView.lookAt[AXIS_X], lookAt[AXIS_X], rate);
        sStaffRollEasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y] =
            BlendStaffRollClamped(sStaffRollEasedView.lookAt[AXIS_Y], lookAt[AXIS_Y], rate);
        sStaffRollEasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z] =
            BlendStaffRollClamped(sStaffRollEasedView.lookAt[AXIS_Z], lookAt[AXIS_Z], rate);
    }
}

/* The first of the club kind's animations; shotMode is unused */
s32 GetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
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

/*
 * The second of the club kind's animations, or the third for an approach
 * shot; the timeline skips to just before its ANIMATION_EVENT_UNK6 frame
 */
s32 GetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
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

void SetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    SetCharacterAnimation(slotIndex, GetStanceAnimation(slotIndex, club, shotMode));
}

void SetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    SetCharacterAnimation(slotIndex, GetSwingAnimation(slotIndex, club, shotMode));
}

/*
 * Frames the spheres at yaw, halving the range between pitch and
 * straight down up to ten times, and ends at the last pitch tried that put
 * the eye at least FRAMING_GROUND_CLEARANCE under the ground, or straight
 * down if none did. It stops early if the first pitch does. The final
 * ground height is read, but not used.
 */
void BisectFramingPitch(f32 pitch, f32 yaw, f32 depthOffset) {
    Vec3 eye;
    Vec3 lookAt;
    CoursePoint point;
    u8 unusedBuffer[0x10]; /* Unused; it is there to match */
    f32 buriedPitch = -QUARTER_TURN_RADIANS;
    s32 step = 0;
    f32 clearPitch = pitch;

    for (; step < PITCH_STEP_COUNT; step++) {
        SetFramingAngles(pitch, yaw);
        FrameSpheres(depthOffset);
        GetFramedView(eye, lookAt);
        point[AXIS_X] = eye[AXIS_X] * COURSE_UNITS_PER_UNIT;
        point[AXIS_Y] = eye[AXIS_Y] * COURSE_UNITS_PER_UNIT;
        point[AXIS_Z] = eye[AXIS_Z] * COURSE_UNITS_PER_UNIT;
        if (point[AXIS_Y] - GetGroundHeight(point[AXIS_X], point[AXIS_Z]) <= -FRAMING_GROUND_CLEARANCE) {
            buriedPitch = pitch;
            if (step == 0) {
                break;
            }
        } else {
            clearPitch = pitch;
        }
        pitch = (clearPitch + buriedPitch) * 0.5f;
    }
    SetFramingAngles(buriedPitch, yaw);
    FrameSpheres(depthOffset);
    GetFramedView(eye, lookAt);
    point[AXIS_X] = eye[AXIS_X] * COURSE_UNITS_PER_UNIT;
    point[AXIS_Y] = eye[AXIS_Y] * COURSE_UNITS_PER_UNIT;
    point[AXIS_Z] = eye[AXIS_Z] * COURSE_UNITS_PER_UNIT;
    GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
}

INCLUDE_ASM("asm/nonmatchings/ovl_14/staff_roll", func_801F53F0_ovl_14);

INCLUDE_ASM("asm/nonmatchings/ovl_14/staff_roll", func_801F544C_ovl_14);

/*
 * Runs while the loader thread has finished the hole and a controller is
 * plugged in. It plays the songs, ends the scroll, reads in the closing
 * picture, and runs the cast's timeline. Once the closing picture has
 * been up a while, A or Start goes to the boot logos. With debugging on,
 * B restarts the staff roll with the next character first, and C-Down
 * skips 100 frames.
 */
void UpdateStaffRoll(void) {
    EncodedAsset asset;
    s32 pressedButtons = 0;
    s32 lineCount = sStaffText->lineCount;
    s32 controllerIndex;

    for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
        pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
    }
    if (g_ControllerCount != 0 && g_HasLoaderFinished == TRUE) {
        if (sStaffRollFrame == 0) {
            sIsPaintingShown = TRUE;
            sIsTextScrolling = TRUE;
            StopSong(EARLIER_SONG_FADE_FRAMES);
        }
        if (sStaffRollFrame == STAFF_SONG_FRAME) {
            PlaySong(STAFF_SONG, SONG_FLAG_RESTART);
        }
        if (sStaffRollFrame == STAFF_SONG_END_FRAME) {
            sIsTextScrolling = FALSE;
            StopSong(STAFF_SONG_FADE_FRAMES);
        }
        if (sStaffRollFrame == lineCount * STAFF_LINE_HEIGHT + SCROLL_END_DELAY) {
            sIsTextScrolling = FALSE;
        }
        if (sStaffRollFrame == CLOSING_PICTURE_READ_FRAME) {
            OpenEncodedAsset(CLOSING_PICTURE_ASSET, &asset);
            ReadEncodedAsset(&sStaffRollImage, &asset);
        }
        if (sStaffRollFrame == CLOSING_FRAME) {
            PlaySong(CLOSING_SONG, SONG_FLAG_RESTART);
        }
        if ((pressedButtons & (A_BUTTON | START_BUTTON)) && sStaffRollFrame >= FIRST_LEAVING_FRAME) {
            RequestSongStop();
            func_8009226C(10);
            g_Scene = SCENE_SWAPPING_MODULES;
            g_NextScene = SCENE_BOOT_LOGOS;
            g_SceneModuleToUnload = 0x1F;
            g_SceneModuleToLoad = 0x0C;
            sStaffRollFrame = 0;
            sIsLeavingStaffRoll = TRUE;
            PlaySound(0x5D, 0xF, 0x7F);
        }
        if (g_IsDebugEnabled) {
            if (pressedButtons & B_BUTTON) {
                OpenEncodedAsset(PAINTING_ASSET, &asset);
                ReadEncodedAsset(&sStaffRollImage, &asset);
                g_Players[0].unk1D = ((s8)g_Players[0].unk1D + 1) % CHARACTER_COUNT;
                ShuffleCastOrder((s8)g_Players[0].unk1D);
                sTopLineIndex = FIRST_TOP_LINE_INDEX;
                sStaffRollFrame = -1;
                sIsLeavingStaffRoll = FALSE;
                sTextScrollOffset = 0;
                sIsPaintingShown = TRUE;
                sDebugRestartCount++;
            }
            if (g_IsDebugEnabled && (pressedButtons & D_CBUTTONS)) {
                sStaffRollFrame += DEBUG_SKIPPED_FRAMES;
            }
        }
        if (sStaffRollFrame >= TIMELINE_START_FRAME) {
            func_801F544C_ovl_14(sStaffRollFrame - TIMELINE_START_FRAME);
        }
        if (sStaffRollFrame < MAX_STAFF_ROLL_FRAME) {
            sStaffRollFrame++;
        }
        sprintf(g_PrintBuffer, "MAX:%d    ", lineCount);
        PrintDebugText(g_PrintBuffer, LINE_COUNT_TEXT_COLUMN, LINE_COUNT_TEXT_ROW);
        func_8004CDA0();
        g_ViewedWindAngles[0] = VIEWED_WIND_ANGLE(g_HoleConditions.windDirection, &g_Cameras[0].view);
        if (g_ViewedWindAngles[0] > 2 * M_PI) {
            g_ViewedWindAngles[0] -= 2 * M_PI;
        }
        if (g_ViewedWindAngles[0] < 0.0f) {
            g_ViewedWindAngles[0] += 2 * M_PI;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_14/staff_roll", func_801F7554_ovl_14);

/* Gives the width of the staff text's line; every line is given no height */
#ifdef NON_MATCHING
void MeasureStaffLine(s32 lineIndex, s32 *width, s32 *height) {
    s32 lineWidth = 0;
    s32 index = 0;
    StaffText *text = sStaffText;
    s32 start;
    s32 length;
    s32 character;

    start = text->lineCount * 2 + 2 + text->lines[lineIndex].start;
    length = text->lines[lineIndex].length;
    for (; index != length; index++) {
        character = ((u16 *)text)[start + index];
        if (character < 0x100 && (u32)(character - FIRST_UNMEASURED_CHARACTER) >= UNMEASURED_CHARACTER_COUNT) {
            lineWidth += STAFF_GLYPH_WIDTH;
        }
    }
    *width = lineWidth;
    *height = 0;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_14/staff_roll", MeasureStaffLine);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_14/staff_roll", func_801F80B8_ovl_14);

/*
 * Draws the painting in strips, scrolled down it by a row a frame from
 * PAINTING_SCROLL_FRAME. It fades in at the start, and fades out once the
 * timeline starts; when it has faded out, sIsPaintingShown is cleared.
 */
void DrawStaffRollPainting(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 alpha;
    s32 stripIndex;
    s32 stripHeight;
    s32 scrolledRows;
    s32 texelOffset;

    if (sStaffRollFrame > TIMELINE_START_FRAME) {
        alpha = PAINTING_OPAQUE_ALPHA - (sStaffRollFrame - TIMELINE_START_FRAME) * PAINTING_FADE_RATE;
        if (alpha < 0) {
            sIsPaintingShown = FALSE;
            alpha = 0;
        }
    } else {
        alpha = PAINTING_OPAQUE_ALPHA;
    }
    gDPPipeSync(gfx++);
    if (alpha > 0) {
        if (alpha == PAINTING_OPAQUE_ALPHA) {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            if (sStaffRollFrame < PAINTING_FADE_IN_FRAMES) {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, sStaffRollFrame * PAINTING_FADE_RATE);
                gDPSetCombineLERP(gfx++, TEXEL0, 0, PRIMITIVE_ALPHA, 0, 0, 0, 0, 1, TEXEL0, 0, PRIMITIVE_ALPHA, 0, 0, 0,
                                  0, 1);
            }
        } else {
            if (alpha < PAINTING_CLOUD_ALPHA) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            } else {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            }
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
            gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE);
        }
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(gfx++, (u32)sStaffRollImage.texels & ~7);
        for (stripIndex = 0; stripIndex != PAINTING_STRIP_COUNT; stripIndex++) {
            scrolledRows = sStaffRollFrame - PAINTING_SCROLL_FRAME;
            if (scrolledRows > PAINTING_SCROLL_LIMIT) {
                scrolledRows = PAINTING_SCROLL_LIMIT;
            }
            if (scrolledRows < 0) {
                scrolledRows = 0;
            }
            texelOffset = scrolledRows * PAINTING_WIDTH + LARGE_PALETTED_TEXELS_OFFSET;
            stripHeight = ((stripIndex != PAINTING_STRIP_COUNT - 1) ? PAINTING_STRIP_STEP : 0) | PAINTING_STRIP_OVERLAP;
            gDPLoadTextureTile(gfx++,
                               (u32)&((u8 *)&sStaffRollImage)[stripIndex * PAINTING_STRIP_STEP * PAINTING_WIDTH
                                                              + texelOffset] & ~7,
                               G_IM_FMT_CI, G_IM_SIZ_8b, PAINTING_WIDTH, 0, 0, 0, PAINTING_WIDTH - 1, stripHeight - 1, 0,
                               G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                               G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, SCREEN_BORDER << 2, (SCREEN_BORDER + stripIndex * PAINTING_STRIP_STEP) << 2,
                                    (SCREEN_BORDER + PAINTING_WIDTH) << 2,
                                    (SCREEN_BORDER + stripIndex * PAINTING_STRIP_STEP + stripHeight) << 2,
                                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            gDPPipeSync(gfx++);
        }
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/* Unreferenced */
void DoNothingAfterStaffRoll(void) {
}
