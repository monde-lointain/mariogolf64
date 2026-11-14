#include "staff_roll.h"

#include <framebuffer_count.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "camera/camera_framing.h"
#include "camera/shot_camera.h"
#include "characters/character.h"
#include "characters/character_model.h"
#include "characters/reaction.h"
#include "controllers/controller.h"
#include "course/billboard.h"
#include "course/course.h"
#include "course/course_space.h"
#include "course/hole_conditions.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/pin.h"
#include "course/terrain.h"
#include "debug_text.h"
#include "game.h"
#include "game_flags.h"
#include "game_state.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "player.h"
#include "scene.h"
#include "screen/display_list.h"
#include "screen/rcp_init.h"
#include "screen/screen.h"
#include "screen/sky_and_hud_animations.h"
#include "shot/club.h"
#include "shot/current_shot.h"
#include "shot/particle_effects.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "text/text.h"

#define CAST_MEMBER_COUNT (CHARACTER_COUNT - 1)
#define FIRST_CAST_OFFSET 2
#define SHUFFLE_ROUND_COUNT 10
#define SHUFFLED_MEMBER_COUNT 16

#define CAST_BUFFER_COUNT 1
#define CAST_BUFFER_CAPACITY 3
#define FIRST_CAST_SLOT 1

#define ASSET_PAINTING 0x959
#define ASSET_STAFF_FONT 0x94C
#define STAFF_ROLL_IMAGE_SIZE 0x2AC00
#define BILLBOARD_HEAP_SIZE 0x7038
#define ASSET_CLOSING_PICTURE 0x967

#define STAFF_TEXT_HEADER_SIZE 8
#define STAFF_GLYPH_WIDTH 12
#define STAFF_GLYPH_HEIGHT 12
#define STAFF_FONT_WIDTH (STAFF_FONT_COLUMN_COUNT * STAFF_GLYPH_WIDTH)
#define STAFF_FONT_COLUMN_COUNT 16
#define STAFF_CHARACTER_LIMIT 0x100
#define FIRST_OVERLAID_CHARACTER 0xDE
#define OVERLAID_CHARACTER_COUNT 2
#define OVERLAID_CHARACTER_SHIFT_X (STAFF_GLYPH_WIDTH / 2)
#define OVERLAID_CHARACTER_SHIFT_Y 4

#define STAFF_ROLL_HOLE_INDEX 6
#define STAFF_ROLL_WIND_STEP 6000
#define STAFF_ROLL_WIND_STEP_MASK 0x3F
#define STAFF_ROLL_LIGHT_PITCH 0x1000
#define STAFF_ROLL_LIGHT_YAW 0x5800

#define STAFF_SONG_FRAME 60
#define TIMELINE_START_FRAME 280
#define CLOSING_PICTURE_READ_FRAME 1000
#define STAFF_SONG_END_FRAME 3688
#define CLOSING_FRAME 3798
#define FIRST_LEAVING_FRAME 4300
#define STAFF_ROLL_FRAME_MAX 0xFFFF
#define SCROLL_END_DELAY 376
#define STAFF_LINE_HEIGHT 24
#define FIRST_TOP_LINE_INDEX (-10)
#define DEBUG_SKIPPED_FRAMES 100

#define EARLIER_SONG_FADE_FRAMES 40
#define STAFF_SONG_FADE_FRAMES 200

#define STAFF_TEXT_LEFT 32
#define STAFF_ROW_COUNT 11
#define TEXT_SHADOW_PASS 0
#define TEXT_PASS_COUNT 2

#define FADE_RIGHT 312
#define FADE_BOTTOM 160

#define ALPHA_TEXT_COLUMN 5
#define ALPHA_TEXT_ROW 13
#define FRAME_TEXT_COLUMN 15
#define FRAME_TEXT_ROW 5

#define LINE_COUNT_TEXT_COLUMN 5
#define LINE_COUNT_TEXT_ROW 12

#define STAFF_ROLL_IMAGE_WIDTH (SCREEN_WIDTH - (2 * SCREEN_BORDER))
#define IMAGE_STRIP_COUNT 38
#define IMAGE_STRIP_STEP 6
#define IMAGE_STRIP_OVERLAP 2
#define PAINTING_SCROLL_FRAME 60
#define PAINTING_SCROLL_LIMIT 228
#define CLOSING_FADE_RATE 2
#define PAINTING_FADE_RATE 4
#define PAINTING_FADE_IN_FRAMES 64
#define CLOUD_ALPHA 180

#define PITCH_STEP_COUNT 10
#define FRAMING_GROUND_CLEARANCE 0xC00

#define FIRST_SWING_FRAME 60
#define VIEW_SHRINK_FRAME 150
#define TEE_REACTIONS_FRAME 300
#define FAIRWAY_FADE_OUT_FRAME 560
#define FAIRWAY_SHOTS_FRAME 600
#define FAIRWAY_SWING_FRAME 650
#define FAIRWAY_REACTIONS_FRAME 940
#define APPROACH_FADE_OUT_FRAME 1200
#define APPROACH_SHOTS_FRAME 1240
#define APPROACH_SWING_FRAME 1300
#define APPROACH_REACTIONS_FRAME 1600
#define PUTT_FADE_OUT_FRAME 1920
#define PUTT_SHOTS_FRAME 1960
#define PUTT_SWING_FRAME 2000
#define PUTT_REACTIONS_FRAME 2200
#define FINALE_FADE_OUT_FRAME 2590
#define FINALE_FRAME 2630
#define FINALE_PAR_FRAME 2740
#define FINALE_FAIRWAY_FRAME 2800
#define FINALE_BOGEY_FRAME 2950
#define FINALE_APPROACH_FRAME 3030
#define FINALE_BIRDIE_FRAME 3100
#define FINALE_PUTT_FRAME 3180
#define FINALE_EAGLE_FRAME 3240
#define LAST_FADE_OUT_FRAME 3360
#define NEVER_FRAME 0x100000

#define IsTeeShotFrame(frame) ((frame) == VIEW_SHRINK_FRAME || (frame) == 190 || (frame) == 240)
#define IsTeeReactionFrame(frame) \
    ((frame) == TEE_REACTIONS_FRAME || (frame) == 360 || (frame) == 420 || (frame) == 480)
#define IsFairwayShotFrame(frame) \
    ((frame) == FAIRWAY_SHOTS_FRAME || (frame) == 770 || (frame) == 810 || (frame) == 850)
#define IsFairwayReactionFrame(frame) \
    ((frame) == FAIRWAY_REACTIONS_FRAME || (frame) == 1000 || (frame) == 1060 || (frame) == 1120)
#define IsApproachShotFrame(frame) \
    ((frame) == APPROACH_SHOTS_FRAME || (frame) == 1420 || (frame) == 1460 || (frame) == 1500)
#define IsApproachReactionFrame(frame) \
    ((frame) == APPROACH_REACTIONS_FRAME || (frame) == 1680 || (frame) == 1760 || (frame) == 1840)
#define IsPuttShotFrame(frame) \
    ((frame) == PUTT_SHOTS_FRAME || (frame) == 2060 || (frame) == 2100 || (frame) == 2150)
#define IsPuttReactionFrame(frame) \
    ((frame) == PUTT_REACTIONS_FRAME || (frame) == 2300 || (frame) == 2400 || (frame) == 2500)

#define CAST_FADE_FRAMES 40
#define LAST_FADE_FRAMES 80
#define SHOTS_PER_STAGE 4
#define TEE_CLUB CLUB_TYPE_DRIVER
#define FAIRWAY_CLUB CLUB_TYPE_TWO_IRON
#define APPROACH_CLUB CLUB_TYPE_SEVEN_IRON
#define CAST_SWING_LEAD_FRAMES 15
#define FINAL_PUTT_LEAD_FRAMES 10
#define CAST_YAW_STEP 30
#define PUTT_YAW_OFFSET 15
#define DEGREES_TO_RADIANS_F ((f32)M_PI / 180)
#define GOLFER_HEADING ((f32)DegreesToRadians(-30))
#define NEAR_CUP_DISTANCE (150 * 1024)

#define RANDOM_PITCH_MASK 0x7F
#define RANDOM_PITCH_BIAS 0x20
#define RANDOM_PITCH_SCALE 70
#define PITCH_UNIT_TO_RADIANS ((f32)DegreesToRadians(1.0 / 127))
#define PITCH_CONTROLLER 1
#define PITCH_STICK_RATE 0.002f

#define STAFF_ROLL_FIELD_OF_VIEW 65.0f
#define CAST_FOCUS_POINT 1
#define SHADOW_SCALE_TO_RADIUS 0.105000004f
#define DEFAULT_FRAMED_RADIUS 15.0f
#define CAST_EASE_RATE 0.1f
#define VIEW_SHRINK_FRAMES 30
#define UNSHRUNK_VIEW_WIDTH 303.0f
#define UNSHRUNK_VIEW_HEIGHT 223.0f
#define SHRINK_LEFT_STEP 5.0000005f
#define SHRINK_TOP_STEP 0.5f
#define SHRINK_WIDTH_STEP 5.3333335f
#define SHRINK_HEIGHT_STEP 4.0f
#define UNIT_ZOOM_HEIGHT 224

#define ClampFrames(frames, count, limit) \
    do { \
        (frames) = (count); \
        if ((frames) < 0) { \
            (frames) = 0; \
        } \
        if ((frames) > (limit)) { \
            (frames) = (limit); \
        } \
    } \
    while (FALSE)

#define ShrinkView(view, frames) \
    do { \
        f32 progress = (frames); \
        s32 left = progress * SHRINK_LEFT_STEP; \
        s32 top = progress * SHRINK_TOP_STEP; \
        s32 height = UNSHRUNK_VIEW_HEIGHT - (progress * SHRINK_HEIGHT_STEP); \
        s32 width = UNSHRUNK_VIEW_WIDTH - (progress * SHRINK_WIDTH_STEP); \
\
        (view).scissorX = left; \
        (view).scissorY = top; \
        (view).scissorWidth = width; \
        (view).scissorHeight = height; \
        (view).viewportCenterX = ViewportCenter(left, width); \
        (view).viewportCenterY = ViewportCenter(top, height); \
        (view).zoom = 2.0f * height * (1.0f / UNIT_ZOOM_HEIGHT); \
    } \
    while (FALSE)

typedef struct StaffRollImage {
    ImageHeader header;
    u8 texels[STAFF_ROLL_IMAGE_SIZE];
} StaffRollImage;

typedef struct StaffText {
    u16 size;
    u16 lineCount;
    TextSpan lines[TRAILING_ARRAY_LENGTH];
} StaffText;

#define STAFF_LINE(text, index) ((text)->lines + (index))
#define STAFF_TEXT_HEADER_CHARACTERS 2
#define CHARACTERS_PER_STAFF_LINE ((s32)(sizeof(TextSpan) / sizeof(u16)))
#define FIRST_STAFF_CHARACTER(text) \
    (((text)->lineCount * CHARACTERS_PER_STAFF_LINE) + STAFF_TEXT_HEADER_CHARACTERS)
#define STAFF_CHARACTER(text, index) (((u16 *)(text))[index])
#define IS_OVERLAID_CHARACTER(character) \
    ((u32)((character) - FIRST_OVERLAID_CHARACTER) < OVERLAID_CHARACTER_COUNT)

extern u8 g_StaffTextRomStart[];

void DrawStaffLine(Gfx **displayListHead, s32 left, s32 top, s32 lineIndex);
void DrawStaffRollPainting(Gfx **displayListHead);

static UBoolean8 f_IsPaintingShown = TRUE;
static s32 f_StaffRollFrame = 0;
static Boolean f_IsLeavingStaffRoll = FALSE;
static s32 f_TextScrollOffset = 0;
static s32 f_TopLineIndex = 0;
#define NO_SCENE_COLOR_INDEX 0xFFFFFFFF
static u32 f_ShownSceneColorIndex = NO_SCENE_COLOR_INDEX;
static Boolean f_IsTextScrolling = TRUE;
static s32 f_DebugRestartCount = 0;
static s32 f_FadeLength = 0;
static Radians f_CastFramingYaw = 0.0f;
static Radians f_CastFramingPitch = 0.0f;
static s32 f_CastIndex = 0;
static Boolean f_IsReactionShown = FALSE;
static f32 f_UnreadValue = 0.0f;
static s32 f_ViewShrinkStartFrame = NEVER_FRAME;
static s32 f_UnusedWord UNUSED = 0;
/* clang-format off */
static u8 f_StaffGlyphAdvances[STAFF_CHARACTER_LIMIT] = {
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,  6, 12,  6, 12,
       8, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 10, 10, 12, 12, 12, 12,
      12, 11, 10,  8,  9,  8,  6, 10,  9,  4,  9,  7,  7, 12, 10, 12,
       9, 11,  9,  7,  9, 11, 12, 12, 10,  9, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
      12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
       0,  9,  7,  7,  9,  9,  8,  8,  8,  4,  5,  8,  4,  8,  8,  8,
       6,  9,  6,  6,  6,  9,  9, 12,  9, 10,  7,  0,  0,  0,  0,  0,
};
/* clang-format on */

static u8 f_UnusedBytesBeforeCastSlot[0x10] UNUSED;
static s32 f_CastSlot;
static CoursePoint f_NearCupPoint;
static u8 f_UnusedBytesAfterNearCupPoint[16] UNUSED;
static StaffRollImage f_StaffRollImage;
static StaffText *f_StaffText;
static u8 *f_StaffFont;
static void *f_BillboardHeap;
static u8 *f_CastBuffers[CAST_BUFFER_CAPACITY];
static EasedView f_EasedView;
static s32 f_CastOrder[CAST_MEMBER_COUNT];
static s32 f_FadeFramesLeft;

s32 GetCastModelKind(void) {
    return g_Players[0].paletteIndex & (CHARACTER_MODEL_KIND_COUNT - 1);
}

void ShuffleCastOrder(s32 start) {
    s32 member;
    s32 index;
    s32 round;

    void SwapCastMembers(s32 first, s32 second) {
        s32 member = f_CastOrder[first];

        f_CastOrder[first] = f_CastOrder[second];
        f_CastOrder[second] = member;
    }

    for (member = 0; member < CAST_MEMBER_COUNT; member++) {
        f_CastOrder[member] = (member + start + FIRST_CAST_OFFSET) % CHARACTER_COUNT;
    }

    if (!IsGameFlagSet(GAME_FLAG_UNSHUFFLED_CAST)) {
        for (round = 0; round < SHUFFLE_ROUND_COUNT; round++) {
            for (index = 0; index < SHUFFLED_MEMBER_COUNT; index++) {
                SwapCastMembers(index, (rand() & 0x7FFF) % SHUFFLED_MEMBER_COUNT);
            }
        }
    }
}

void StartFadeOut(s32 frameCount) {
    f_FadeLength = -frameCount;
    f_FadeFramesLeft = frameCount;
}

void StartFadeIn(s32 frameCount) {
    f_FadeLength = frameCount;
    f_FadeFramesLeft = frameCount;
}

void StopFade(void) {
    f_FadeLength = 0;
}

void LoadStaffRoll(void) {
    EncodedAsset asset;
    EncodedAsset shadowAsset;
    u8 *header;
    s32 index;
    s32 textSize;

    f_ShownSceneColorIndex = SCENE_COLOR_INDEX_SUDDEN_DEATH;
    ShuffleCastOrder(g_Players[0].characterIndex);
    FreeAllCharacterSlots();
    for (index = 0; index < CAST_BUFFER_COUNT; index++) {
        f_CastBuffers[index] = AllocMemory(GetCharacterBufferSize());
        SetCharacterBuffer(index + FIRST_CAST_SLOT, f_CastBuffers[index]);
    }

    OpenEncodedAsset(ASSET_PAINTING, &asset);
    ReadEncodedAsset(&f_StaffRollImage, &asset);
    f_StaffFont = AllocMemory(OpenEncodedAsset(ASSET_STAFF_FONT, &asset));
    ReadEncodedAsset(f_StaffFont, &asset);
    header = AllocMemory(STAFF_TEXT_HEADER_SIZE);
    nuPiReadRom((u32)g_StaffTextRomStart, header, STAFF_TEXT_HEADER_SIZE);
    textSize = header[0] << 8 | header[1];
    FreeMemory(&header);
    f_StaffText = AllocMemory(textSize);
    nuPiReadRom((u32)g_StaffTextRomStart, f_StaffText, textSize);
    g_ShadowTexture = AllocMemory(OpenEncodedAsset(ASSET_SHADOW_TEXTURE, &shadowAsset));
    ReadEncodedAsset(g_ShadowTexture, &shadowAsset);
    f_BillboardHeap = AllocMemory(BILLBOARD_HEAP_SIZE);
    InitBillboardManager(f_BillboardHeap, BILLBOARD_HEAP_SIZE);
    memset(&g_GameState, 0, sizeof(g_GameState));
    g_GameState.courseIndex = COURSE_YOSHIS_ISLAND;
    g_GameState.holeIndex = STAFF_ROLL_HOLE_INDEX;
    g_GameState.unusedHoleValue = 0;
    g_GameState.gameMode = GAME_MODE_STAFF_ROLL;
    g_HoleSetup.conditions.isRaining = FALSE;
    g_HoleSetup.conditions.windSpeed = (rand() & STAFF_ROLL_WIND_STEP_MASK) * STAFF_ROLL_WIND_STEP;
    g_HoleSetup.conditions.windDirection = rand();
    g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_LATE;
    g_HoleSetup.conditions.lightPitch = STAFF_ROLL_LIGHT_PITCH;
    g_HoleSetup.conditions.lightYaw = STAFF_ROLL_LIGHT_YAW;
    g_HoleSetup.cupPosition[AXIS_X] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_X];
    g_HoleSetup.cupPosition[AXIS_Y] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Y];
    g_HoleSetup.cupPosition[AXIS_Z] = g_HolePoints.cupPositions[DEFAULT_CUP][AXIS_Z];
    g_HoleSetup.teePosition[AXIS_X] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_X];
    g_HoleSetup.teePosition[AXIS_Y] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Y];
    g_HoleSetup.teePosition[AXIS_Z] = g_HolePoints.teePositions[PLAYED_TEE][AXIS_Z];
    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    g_GameState.playerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
    SetFramebufferCount(TRIPLE_BUFFERING);
    f_IsLeavingStaffRoll = FALSE;
    f_TextScrollOffset = 0;
    f_TopLineIndex = FIRST_TOP_LINE_INDEX;
    LoadEffectImages();
}

void FreeStaffRoll(void) {
    s32 index;

    FreeEffectImages();
    g_Cameras[MAIN_CAMERA].view.scissorX = SCREEN_BORDER;
    g_Cameras[MAIN_CAMERA].view.scissorY = SCREEN_BORDER;
    g_Cameras[MAIN_CAMERA].view.scissorWidth = SCREEN_WIDTH - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.scissorHeight = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.zoom = 1.0f;
    FreeAllCharacterSlots();
    for (index = 0; index < CAST_BUFFER_COUNT; index++) {
        FreeMemory(&f_CastBuffers[index]);
    }

    AssignCharacterBuffers();
    SetFillColor(0, 0, 0);
    FreeMemory(&f_BillboardHeap);
    FreeMemory(&f_StaffText);
    FreeMemory(&f_StaffFont);
    FreeMemory(&g_ShadowTexture);
}

f32 BlendStaffRollClamped(f32 from, f32 to, f32 weight) {
    if (weight < 0.0f) {
        weight = 0.0f;
    }

    if (weight > 1.0f) {
        weight = 1.0f;
    }

    return (from * (1.0f - weight)) + (to * weight);
}

void ResetStaffRollEase(void) {
    f_EasedView.isEyeEased = FALSE;
    f_EasedView.isLookAtEased = FALSE;
}

void EaseStaffRollView(f32 *eye, f32 *lookAt, f32 rate, s32 easedParts) {
    if (easedParts & EASE_EYE) {
        if (!f_EasedView.isEyeEased) {
            f_EasedView.eye[AXIS_X] = eye[AXIS_X];
            f_EasedView.eye[AXIS_Y] = eye[AXIS_Y];
            f_EasedView.eye[AXIS_Z] = eye[AXIS_Z];
            f_EasedView.isEyeEased = TRUE;
        }

        eye[AXIS_X] = BlendStaffRollClamped(f_EasedView.eye[AXIS_X], eye[AXIS_X], rate);
        f_EasedView.eye[AXIS_X] = eye[AXIS_X];
        eye[AXIS_Y] = BlendStaffRollClamped(f_EasedView.eye[AXIS_Y], eye[AXIS_Y], rate);
        f_EasedView.eye[AXIS_Y] = eye[AXIS_Y];
        eye[AXIS_Z] = BlendStaffRollClamped(f_EasedView.eye[AXIS_Z], eye[AXIS_Z], rate);
        f_EasedView.eye[AXIS_Z] = eye[AXIS_Z];
    }

    if (easedParts & EASE_LOOK_AT) {
        if (!f_EasedView.isLookAtEased) {
            f_EasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
            f_EasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
            f_EasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
            f_EasedView.isLookAtEased = TRUE;
        }

        lookAt[AXIS_X] = BlendStaffRollClamped(f_EasedView.lookAt[AXIS_X], lookAt[AXIS_X], rate);
        f_EasedView.lookAt[AXIS_X] = lookAt[AXIS_X];
        lookAt[AXIS_Y] = BlendStaffRollClamped(f_EasedView.lookAt[AXIS_Y], lookAt[AXIS_Y], rate);
        f_EasedView.lookAt[AXIS_Y] = lookAt[AXIS_Y];
        lookAt[AXIS_Z] = BlendStaffRollClamped(f_EasedView.lookAt[AXIS_Z], lookAt[AXIS_Z], rate);
        f_EasedView.lookAt[AXIS_Z] = lookAt[AXIS_Z];
    }
}

AnimationId GetStanceAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode UNUSED) {
    AnimationId animationId;

    switch (GetCharacterClubCategory(slotIndex, clubIndex)) {
        case CLUB_CATEGORY_WOOD:
            animationId = WOOD_ANIMATIONS;
            break;
        case CLUB_CATEGORY_IRON:
            animationId = IRON_ANIMATIONS;
            break;
        case CLUB_CATEGORY_WEDGE:
            animationId = WEDGE_ANIMATIONS;
            break;
        case CLUB_CATEGORY_PUTTER:
            animationId = PUTTER_ANIMATIONS;
            break;
        default:
            animationId = WOOD_ANIMATIONS;
            break;
    }

    return animationId;
}

AnimationId GetSwingAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode) {
    AnimationId animationId;

    switch (GetCharacterClubCategory(slotIndex, clubIndex)) {
        case CLUB_CATEGORY_WOOD:
            animationId = WOOD_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_CATEGORY_IRON:
            animationId = IRON_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_CATEGORY_WEDGE:
            animationId = WEDGE_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_CATEGORY_PUTTER:
            animationId = PUTTER_ANIMATIONS + SWING_ANIMATION;
            break;
        default:
            animationId = WOOD_ANIMATIONS;
            break;
    }

    return animationId + (shotMode == SHOT_MODE_APPROACH);
}

void SetStanceAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode) {
    SetCharacterAnimation(slotIndex, GetStanceAnimation(slotIndex, clubIndex, shotMode));
}

void SetSwingAnimation(u32 slotIndex, ClubIndex clubIndex, ShotMode shotMode) {
    SetCharacterAnimation(slotIndex, GetSwingAnimation(slotIndex, clubIndex, shotMode));
}

void BisectFramingPitch(Radians pitch, Radians yaw, f32 depthOffset) {
    Vec3 eye;
    Vec3 lookAt;
    CoursePoint point;
    u8 unusedBuffer[0x10] UNUSED;
    Radians buriedPitch = -QUARTER_TURN_RADIANS;
    s32 step = 0;
    Radians clearPitch = pitch;

    for (; step < PITCH_STEP_COUNT; step++) {
        SetFramingAngles(pitch, yaw);
        FrameSpheres(depthOffset);
        GetFramedView(eye, lookAt);
        point[AXIS_X] = eye[AXIS_X] * WORLD_TO_COURSE_SCALE;
        point[AXIS_Y] = eye[AXIS_Y] * WORLD_TO_COURSE_SCALE;
        point[AXIS_Z] = eye[AXIS_Z] * WORLD_TO_COURSE_SCALE;
        if (point[AXIS_Y] - GetGroundHeight(point[AXIS_X], point[AXIS_Z]) <=
            -FRAMING_GROUND_CLEARANCE) {
            buriedPitch = pitch;
            if (step == 0) {
                break;
            }
        }
        else {
            clearPitch = pitch;
        }

        pitch = (clearPitch + buriedPitch) * 0.5f;
    }

    SetFramingAngles(buriedPitch, yaw);
    FrameSpheres(depthOffset);
    GetFramedView(eye, lookAt);
    point[AXIS_X] = eye[AXIS_X] * WORLD_TO_COURSE_SCALE;
    point[AXIS_Y] = eye[AXIS_Y] * WORLD_TO_COURSE_SCALE;
    point[AXIS_Z] = eye[AXIS_Z] * WORLD_TO_COURSE_SCALE;
    GetGroundHeight(point[AXIS_X], point[AXIS_Z]);
}

void RunStaffRollTimeline(s32 frame) {
    CoursePoint point;
    Vec3 focus;
    CharacterSlot *slot;
    s32 slotIndex;
    s32 shrink;
    f32 radius;
    s32 swingAnimation;
    CameraView *view;

    void ChooseFramingPitch(void) {
        f_CastFramingPitch = ((rand() & RANDOM_PITCH_MASK) - RANDOM_PITCH_BIAS) *
            RANDOM_PITCH_SCALE * PITCH_UNIT_TO_RADIANS;
    }

    f_CastFramingPitch += g_ControllerStates[PITCH_CONTROLLER].stickY * PITCH_STICK_RATE;
    SetCharacterLights();
    if (frame == 0) {
        SetGameFlag(GAME_FLAG_STAFF_ROLL_ON_COURSE);
        StopFade();
        f_NearCupPoint[AXIS_X] = g_HoleSetup.cupPosition[AXIS_X] + NEAR_CUP_DISTANCE;
        f_NearCupPoint[AXIS_Z] = g_HoleSetup.cupPosition[AXIS_Z];
        f_NearCupPoint[AXIS_Y] = GetGroundHeight(f_NearCupPoint[AXIS_X], f_NearCupPoint[AXIS_Z]);
        ClearEffects();
        f_ViewShrinkStartFrame = NEVER_FRAME;
        f_UnreadValue = 0.0f;
        f_CastFramingPitch = 0.0f;
        f_IsReactionShown = FALSE;
        f_CastIndex = 0;
        g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
        FreeAllCharacterSlots();
        ResetStaffRollEase();
        f_CastSlot = 0;
        LoadCharacter(0, f_CastOrder[0], GetCastModelKind());
        SetCharacterLighting(0, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(0, TEE_CLUB);
        SetStanceAnimation(0, TEE_CLUB, SHOT_MODE_NORMAL);
        point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
        point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
        point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
        PlaceCharacter(0, point, GOLFER_HEADING);
        f_CastSlot = 0;
    }

    if (frame == FIRST_SWING_FRAME) {
        SetSwingAnimation(f_CastSlot, TEE_CLUB, SHOT_MODE_NORMAL);
        f_CastIndex = 1;
    }

    if (frame == VIEW_SHRINK_FRAME) {
        f_ViewShrinkStartFrame = frame;
    }

    if (IsTeeShotFrame(frame)) {
        ChooseFramingPitch();
        f_CastFramingYaw = (f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F;
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex++], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, TEE_CLUB);
        SetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL);
        swingAnimation =
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL));
        AdvanceAnimation(
            slotIndex,
            GetAnimationEventFrame(slotIndex, swingAnimation, ANIMATION_EVENT_IMPACT) -
                CAST_SWING_LEAD_FRAMES
        );
        point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
        point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
        point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (IsTeeReactionFrame(frame)) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        g_CurrentShot.heading =
            -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F * RADIANS_TO_BINARY_ANGLE;
        if (frame == TEE_REACTIONS_FRAME) {
            f_CastIndex -= SHOTS_PER_STAGE;
        }

        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_PAR);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(g_HoleSetup.teePosition);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_PAR);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
        point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
        point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
        PlaceCharacter(slotIndex, point, DegreesToRadians(-(f_CastIndex * CAST_YAW_STEP)));
        f_CastSlot = slotIndex;
        f_CastIndex++;
    }

    if (frame == FAIRWAY_FADE_OUT_FRAME) {
        StartFadeOut(CAST_FADE_FRAMES);
    }

    if (frame == FAIRWAY_SHOTS_FRAME) {
        StartFadeIn(CAST_FADE_FRAMES);
    }

    if (IsFairwayShotFrame(frame)) {
        ClearEffects();
        ChooseFramingPitch();
        g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_AFTERNOON;
        f_CastFramingYaw = ((f_CastIndex - SHOTS_PER_STAGE) * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F;
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        f_IsReactionShown = FALSE;
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex++], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, FAIRWAY_CLUB);
        if (frame == FAIRWAY_SHOTS_FRAME) {
            SetStanceAnimation(slotIndex, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
        }
        else {
            SetSwingAnimation(slotIndex, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
            swingAnimation = GetAnimationIndex(
                slotIndex,
                GetSwingAnimation(slotIndex, FAIRWAY_CLUB, SHOT_MODE_NORMAL)
            );
            AdvanceAnimation(
                slotIndex,
                GetAnimationEventFrame(slotIndex, swingAnimation, ANIMATION_EVENT_IMPACT) -
                    CAST_SWING_LEAD_FRAMES
            );
        }

        point[AXIS_X] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FAIRWAY_SWING_FRAME) {
        SetSwingAnimation(f_CastSlot, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
    }

    if (IsFairwayReactionFrame(frame)) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        g_CurrentShot.heading =
            -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F * RADIANS_TO_BINARY_ANGLE;
        if (frame == FAIRWAY_REACTIONS_FRAME) {
            f_CastIndex -= SHOTS_PER_STAGE;
        }

        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BOGEY);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT]);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BOGEY);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F);
        f_CastSlot = slotIndex;
        f_CastIndex++;
    }

    if (frame == APPROACH_FADE_OUT_FRAME) {
        StartFadeOut(CAST_FADE_FRAMES);
    }

    if (frame == APPROACH_SHOTS_FRAME) {
        StartFadeIn(CAST_FADE_FRAMES);
    }

    if (IsApproachShotFrame(frame)) {
        ClearEffects();
        ChooseFramingPitch();
        g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_LATE;
        f_CastFramingYaw =
            ((f_CastIndex - (2 * SHOTS_PER_STAGE)) * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F;
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        f_IsReactionShown = FALSE;
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex++], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, APPROACH_CLUB);
        if (frame == APPROACH_SHOTS_FRAME) {
            SetStanceAnimation(slotIndex, APPROACH_CLUB, SHOT_MODE_NORMAL);
        }
        else {
            SetSwingAnimation(slotIndex, APPROACH_CLUB, SHOT_MODE_NORMAL);
            swingAnimation = GetAnimationIndex(
                slotIndex,
                GetSwingAnimation(slotIndex, APPROACH_CLUB, SHOT_MODE_NORMAL)
            );
            AdvanceAnimation(
                slotIndex,
                GetAnimationEventFrame(slotIndex, swingAnimation, ANIMATION_EVENT_IMPACT) -
                    CAST_SWING_LEAD_FRAMES
            );
        }

        point[AXIS_X] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == APPROACH_SWING_FRAME) {
        SetSwingAnimation(f_CastSlot, APPROACH_CLUB, SHOT_MODE_NORMAL);
    }

    if (IsApproachReactionFrame(frame)) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        g_CurrentShot.heading =
            -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F * RADIANS_TO_BINARY_ANGLE;
        if (frame == APPROACH_REACTIONS_FRAME) {
            f_CastIndex -= SHOTS_PER_STAGE;
        }

        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BIRDIE);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT]);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BIRDIE);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F);
        f_CastSlot = slotIndex;
        f_CastIndex++;
    }

    if (frame == PUTT_FADE_OUT_FRAME) {
        StartFadeOut(CAST_FADE_FRAMES);
    }

    if (frame == PUTT_SHOTS_FRAME) {
        StartFadeIn(CAST_FADE_FRAMES);
    }

    if (IsPuttShotFrame(frame)) {
        ClearEffects();
        f_CastFramingYaw = ((f_CastIndex - PUTT_YAW_OFFSET) * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F;
        ChooseFramingPitch();
        g_HoleSetup.conditions.sceneColorIndex = SCENE_COLOR_INDEX_ROUND_END;
        f_UnreadValue = 15.0f;
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        f_IsReactionShown = FALSE;
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex++], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, CLUB_TYPE_PUTTER);
        if (frame == PUTT_SHOTS_FRAME) {
            SetStanceAnimation(slotIndex, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
        }
        else {
            SetSwingAnimation(slotIndex, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
            swingAnimation = GetAnimationIndex(
                slotIndex,
                GetSwingAnimation(slotIndex, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL)
            );
            AdvanceAnimation(
                slotIndex,
                GetAnimationEventFrame(slotIndex, swingAnimation, ANIMATION_EVENT_IMPACT) -
                    CAST_SWING_LEAD_FRAMES
            );
        }

        point[AXIS_X] = f_NearCupPoint[AXIS_X];
        point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
        point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == PUTT_SWING_FRAME) {
        SetSwingAnimation(f_CastSlot, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
    }

    if (IsPuttReactionFrame(frame)) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        g_CurrentShot.heading =
            -(f_CastIndex * CAST_YAW_STEP) * DEGREES_TO_RADIANS_F * RADIANS_TO_BINARY_ANGLE;
        if (frame == PUTT_REACTIONS_FRAME) {
            f_CastIndex -= SHOTS_PER_STAGE;
        }

        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_EAGLE);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(f_NearCupPoint);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_EAGLE);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = f_NearCupPoint[AXIS_X];
        point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
        point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
        PlaceCharacter(slotIndex, point, DegreesToRadians(-(f_CastIndex * CAST_YAW_STEP)));
        f_CastSlot = slotIndex;
        f_CastIndex++;
    }

    if (frame == FINALE_FADE_OUT_FRAME) {
        StartFadeOut(CAST_FADE_FRAMES);
    }

    if (frame == FINALE_FRAME) {
        StartFadeIn(CAST_FADE_FRAMES);
        f_CastFramingYaw = 0.0f;
        ClearEffects();
        ChooseFramingPitch();
        f_IsReactionShown = FALSE;
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, TEE_CLUB);
        SetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
        point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
        point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_PAR_FRAME) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_PAR);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(g_HoleSetup.teePosition);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_PAR);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = g_HoleSetup.teePosition[AXIS_X];
        point[AXIS_Y] = g_HoleSetup.teePosition[AXIS_Y];
        point[AXIS_Z] = g_HoleSetup.teePosition[AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_FAIRWAY_FRAME) {
        ClearEffects();
        f_CastFramingYaw = 0.0f;
        ChooseFramingPitch();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        f_IsReactionShown = FALSE;
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, FAIRWAY_CLUB);
        SetSwingAnimation(slotIndex, FAIRWAY_CLUB, SHOT_MODE_NORMAL);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(
                slotIndex,
                GetSwingAnimation(slotIndex, FAIRWAY_CLUB, SHOT_MODE_NORMAL)
            ),
            ANIMATION_EVENT_IMPACT
        );
        point[AXIS_X] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_BOGEY_FRAME) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BOGEY);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT]);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BOGEY);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[TEE_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_APPROACH_FRAME) {
        f_CastFramingYaw = DEGREES_TO_RADIANS_F * CAST_YAW_STEP;
        ClearEffects();
        ChooseFramingPitch();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        f_IsReactionShown = FALSE;
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, APPROACH_CLUB);
        SetSwingAnimation(slotIndex, APPROACH_CLUB, SHOT_MODE_NORMAL);
        swingAnimation = GetAnimationIndex(
            slotIndex,
            GetSwingAnimation(slotIndex, APPROACH_CLUB, SHOT_MODE_NORMAL)
        );
        AdvanceAnimation(
            slotIndex,
            GetAnimationEventFrame(slotIndex, swingAnimation, ANIMATION_EVENT_IMPACT) -
                (2 * CAST_SWING_LEAD_FRAMES)
        );
        point[AXIS_X] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_BIRDIE_FRAME) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_BIRDIE);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT]);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_BIRDIE);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_X];
        point[AXIS_Y] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Y];
        point[AXIS_Z] = g_HolePoints.aimPoints[SECOND_SHOT_AIM_POINT][AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_PUTT_FRAME) {
        ClearEffects();
        ChooseFramingPitch();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        f_IsReactionShown = FALSE;
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex], GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterClub(slotIndex, CLUB_TYPE_PUTTER);
        SetSwingAnimation(slotIndex, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL);
        swingAnimation = GetAnimationIndex(
            slotIndex,
            GetSwingAnimation(slotIndex, CLUB_TYPE_PUTTER, SHOT_MODE_NORMAL)
        );
        AdvanceAnimation(
            slotIndex,
            GetAnimationEventFrame(slotIndex, swingAnimation, ANIMATION_EVENT_IMPACT) -
                FINAL_PUTT_LEAD_FRAMES
        );
        point[AXIS_X] = f_NearCupPoint[AXIS_X];
        point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
        point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == FINALE_EAGLE_FRAME) {
        ClearEffects();
        FreeCharacterSlot(f_CastSlot);
        ResetStaffRollEase();
        slotIndex = f_CastSlot ^ 1;
        LoadCharacter(slotIndex, f_CastOrder[f_CastIndex] + CHARACTER_COUNT, GetCastModelKind());
        SetCharacterLighting(slotIndex, CHARACTER_LIGHTING_SCENE);
        SetCharacterAnimation(slotIndex, REACTION_ANIMATION_EAGLE);
        GetAnimationEventFrame(
            slotIndex,
            GetAnimationIndex(slotIndex, GetSwingAnimation(slotIndex, TEE_CLUB, SHOT_MODE_NORMAL)),
            ANIMATION_EVENT_IMPACT
        );
        ClearReactionSpot();
        ConsiderReactionSpot(f_NearCupPoint);
        StartReaction(slotIndex, f_CastOrder[f_CastIndex], REACTION_ANIMATION_EAGLE);
        f_IsReactionShown = TRUE;
        point[AXIS_X] = f_NearCupPoint[AXIS_X];
        point[AXIS_Y] = f_NearCupPoint[AXIS_Y];
        point[AXIS_Z] = f_NearCupPoint[AXIS_Z];
        PlaceCharacter(slotIndex, point, GOLFER_HEADING);
        f_CastSlot = slotIndex;
    }

    if (frame == LAST_FADE_OUT_FRAME) {
        StartFadeOut(LAST_FADE_FRAMES);
    }

    TakePinOut();
    UpdateEffects();
    if (f_ShownSceneColorIndex != g_HoleSetup.conditions.sceneColorIndex) {
        InitSkyColors();
        f_ShownSceneColorIndex = g_HoleSetup.conditions.sceneColorIndex;
    }

    g_Cameras[MAIN_CAMERA].view.fieldOfView = STAFF_ROLL_FIELD_OF_VIEW;
    AnimateAllCharacters();
    if (f_IsReactionShown) {
        PlaceAndViewCharacter(f_CastSlot);
        UpdateReaction(f_CastSlot);
    }
    else {
        slot = GetUsedCharacterSlot(f_CastSlot);
        ClearFramedSpheres();
        SetFramingAngles(f_CastFramingPitch, f_CastFramingYaw);
        GetCharacterPointPosition(f_CastSlot, CAST_FOCUS_POINT, focus);
        if (slot != NULL) {
            radius = slot->shadowScale * SHADOW_SCALE_TO_RADIUS;
        }
        else {
            radius = DEFAULT_FRAMED_RADIUS;
        }

        AddFramedSphere(focus, radius);
        FrameSpheres(0.0f);
        BisectFramingPitch(f_CastFramingPitch, f_CastFramingYaw, 0.0f);
        view = &g_Cameras[MAIN_CAMERA].view;
        GetFramedView(view->eye, view->lookAt);
        EaseStaffRollView(view->eye, view->lookAt, CAST_EASE_RATE, EASE_LOOK_AT | EASE_EYE);
    }

    g_Cameras[MAIN_CAMERA].view.fieldOfView = STAFF_ROLL_FIELD_OF_VIEW;
    ClampFrames(shrink, frame - f_ViewShrinkStartFrame, VIEW_SHRINK_FRAMES);
    ShrinkView(g_Cameras[MAIN_CAMERA].view, shrink);
    g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vscale[0] =
        g_Cameras[MAIN_CAMERA].view.scissorWidth * 4;
    g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vscale[1] =
        g_Cameras[MAIN_CAMERA].view.scissorHeight * 4;
    g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vtrans[0] =
        (g_Cameras[MAIN_CAMERA].view.scissorX + (g_Cameras[MAIN_CAMERA].view.scissorWidth / 2)) * 4;
    g_Viewports[VIEWPORT_STAFF_ROLL + g_DrawBufferIndex].vp.vtrans[1] =
        (g_Cameras[MAIN_CAMERA].view.scissorY + (g_Cameras[MAIN_CAMERA].view.scissorHeight / 2)) *
        4;
}

void UpdateStaffRoll(void) {
    EncodedAsset asset;
    s32 pressedButtons = 0;
    s32 lineCount = f_StaffText->lineCount;
    s32 controllerIndex;

    for (controllerIndex = 0; controllerIndex != g_ControllerCount; controllerIndex++) {
        pressedButtons |= g_ControllerStates[controllerIndex].pressedButtons;
    }

    if (g_ControllerCount != 0 && g_HasLoaderFinished == TRUE) {
        if (f_StaffRollFrame == 0) {
            f_IsPaintingShown = TRUE;
            f_IsTextScrolling = TRUE;
            StopSong(EARLIER_SONG_FADE_FRAMES);
        }

        if (f_StaffRollFrame == STAFF_SONG_FRAME) {
            PlaySong(SONG_STAFF_ROLL, SONG_FLAG_RESTART);
        }

        if (f_StaffRollFrame == STAFF_SONG_END_FRAME) {
            f_IsTextScrolling = FALSE;
            StopSong(STAFF_SONG_FADE_FRAMES);
        }

        if (f_StaffRollFrame == (lineCount * STAFF_LINE_HEIGHT) + SCROLL_END_DELAY) {
            f_IsTextScrolling = FALSE;
        }

        if (f_StaffRollFrame == CLOSING_PICTURE_READ_FRAME) {
            OpenEncodedAsset(ASSET_CLOSING_PICTURE, &asset);
            ReadEncodedAsset(&f_StaffRollImage, &asset);
        }

        if (f_StaffRollFrame == CLOSING_FRAME) {
            PlaySong(SONG_CLOSING, SONG_FLAG_RESTART);
        }

        if ((pressedButtons & (A_BUTTON | START_BUTTON)) &&
            f_StaffRollFrame >= FIRST_LEAVING_FRAME) {
            RequestSongStop();
            StartHudAnimation(HUD_ANIMATION_SPIRAL);
            g_Scene = SCENE_SWAPPING_MODULES;
            g_NextScene = SCENE_BOOT_LOGOS;
            g_SceneModuleToUnload = MODULE_STAFF_ROLL;
            g_SceneModuleToLoad = MODULE_BOOT_LOGOS;
            f_StaffRollFrame = 0;
            f_IsLeavingStaffRoll = TRUE;
            PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }

        if (g_IsDebugEnabled) {
            if (pressedButtons & B_BUTTON) {
                OpenEncodedAsset(ASSET_PAINTING, &asset);
                ReadEncodedAsset(&f_StaffRollImage, &asset);
                g_Players[0].characterIndex = (g_Players[0].characterIndex + 1) % CHARACTER_COUNT;
                ShuffleCastOrder(g_Players[0].characterIndex);
                f_TopLineIndex = FIRST_TOP_LINE_INDEX;
                f_StaffRollFrame = -1;
                f_IsLeavingStaffRoll = FALSE;
                f_TextScrollOffset = 0;
                f_IsPaintingShown = TRUE;
                f_DebugRestartCount++;
            }

            if (g_IsDebugEnabled && (pressedButtons & D_CBUTTONS)) {
                f_StaffRollFrame += DEBUG_SKIPPED_FRAMES;
            }
        }

        if (f_StaffRollFrame >= TIMELINE_START_FRAME) {
            RunStaffRollTimeline(f_StaffRollFrame - TIMELINE_START_FRAME);
        }

        if (f_StaffRollFrame < STAFF_ROLL_FRAME_MAX) {
            f_StaffRollFrame++;
        }

        sprintf(g_PrintBuffer, "MAX:%d    ", (int)lineCount);
        PrintDebugText(g_PrintBuffer, LINE_COUNT_TEXT_COLUMN, LINE_COUNT_TEXT_ROW);
        StepNoiseTexture();
        g_SkyState.viewedWindAngles[MAIN_CAMERA] =
            ViewedWindAngle(g_HoleSetup.conditions.windDirection, &g_Cameras[MAIN_CAMERA].view);
        if (g_SkyState.viewedWindAngles[MAIN_CAMERA] > FULL_TURN_RADIANS) {
            g_SkyState.viewedWindAngles[MAIN_CAMERA] -= FULL_TURN_RADIANS;
        }

        if (g_SkyState.viewedWindAngles[MAIN_CAMERA] < 0.0f) {
            g_SkyState.viewedWindAngles[MAIN_CAMERA] += FULL_TURN_RADIANS;
        }
    }
}

void DrawStaffRoll(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 lineCount = f_StaffText->lineCount;
    s32 fadeAlpha;
    s32 closingAlpha;
    s32 pass;
    s32 index;
    s32 lineIndex;
    s32 stripHeight;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetScissor(
        gfx++,
        G_SC_NON_INTERLACE,
        SCREEN_BORDER,
        SCREEN_BORDER,
        SCREEN_WIDTH - SCREEN_BORDER,
        SCREEN_HEIGHT - SCREEN_BORDER
    );
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    if (f_FadeLength != 0) {
        if (f_FadeFramesLeft != 0) {
            f_FadeFramesLeft--;
        }

        fadeAlpha = (f_FadeFramesLeft << 8) / f_FadeLength;
        if (f_FadeLength < 0) {
            fadeAlpha += 0x100;
        }

        if (fadeAlpha > OPAQUE_ALPHA) {
            fadeAlpha = OPAQUE_ALPHA;
        }

        if (fadeAlpha < 0) {
            fadeAlpha = 0;
        }

        if (fadeAlpha == 0) {
            f_FadeLength = 0;
        }

        gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, fadeAlpha);
        gDPSetCombineLERP(gfx++, 0, 0, 0, 0, 0, 0, 0, PRIMITIVE, 0, 0, 0, 0, 0, 0, 0, PRIMITIVE);
        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
        gSPTextureRectangle(
            gfx++,
            0,
            0,
            QuarterPixels(FADE_RIGHT),
            QuarterPixels(FADE_BOTTOM),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gSPLoadGeometryMode(gfx++, 0);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 0, 0, 0, 1);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_DISABLE);
    gDPSetEnvColor(gfx++, 0x80, 0x80, 0xFF, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetCombineLERP(
        gfx++,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        TEXEL0,
        0
    );
    if (f_IsPaintingShown) {
        DrawStaffRollPainting(&gfx);
    }

    if (f_StaffRollFrame < CLOSING_FRAME) {
        closingAlpha = 0;
        if (!f_IsPaintingShown && f_IsTextScrolling) {
            if (--f_TextScrollOffset == -STAFF_LINE_HEIGHT) {
                f_TextScrollOffset = 0;
                f_TopLineIndex++;
            }
        }
    }
    else {
        closingAlpha = (f_StaffRollFrame - CLOSING_FRAME) * CLOSING_FADE_RATE;
        if (closingAlpha > OPAQUE_ALPHA) {
            closingAlpha = OPAQUE_ALPHA;
        }
    }

    sprintf(g_PrintBuffer, "alpha:%d    ", (int)closingAlpha);
    PrintDebugText(g_PrintBuffer, ALPHA_TEXT_COLUMN, ALPHA_TEXT_ROW);
    if (!f_IsPaintingShown) {
        for (pass = 0; pass < TEXT_PASS_COUNT; pass++) {
            if (pass == TEXT_SHADOW_PASS) {
                gDPSetCombineLERP(
                    gfx++,
                    0,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0
                );
            }
            else {
                gDPSetCombineLERP(
                    gfx++,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0,
                    0,
                    0,
                    0,
                    PRIMITIVE,
                    PRIMITIVE,
                    0,
                    TEXEL0,
                    0
                );
            }

            for (index = 0; index != STAFF_ROW_COUNT; index++) {
                lineIndex = index + f_TopLineIndex;
                if (lineIndex >= 0) {
                    if (lineIndex < lineCount) {
                        DrawStaffLine(
                            &gfx,
                            STAFF_TEXT_LEFT - pass,
                            (index * STAFF_LINE_HEIGHT) + f_TextScrollOffset - pass,
                            lineIndex
                        );
                    }
                }
            }
        }
    }

    if (g_IsDebugEnabled) {
        sprintf(g_PrintBuffer, "%5d", (int)f_StaffRollFrame);
        PrintDebugText(g_PrintBuffer, FRAME_TEXT_COLUMN, FRAME_TEXT_ROW);
    }

    gDPPipeSync(gfx++);
    if (closingAlpha > 0) {
        if (closingAlpha == OPAQUE_ALPHA) {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        }
        else {
            if (closingAlpha < CLOUD_ALPHA) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            }
            else {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            }

            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, closingAlpha);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                0,
                0,
                0,
                PRIMITIVE,
                0,
                0,
                0,
                TEXEL0,
                0,
                0,
                0,
                PRIMITIVE
            );
        }

        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
        gDPPipeSync(gfx++);
        for (index = 0; index != IMAGE_STRIP_COUNT; index++) {
            stripHeight =
                ((index != IMAGE_STRIP_COUNT - 1) ? IMAGE_STRIP_STEP : 0) | IMAGE_STRIP_OVERLAP;
            gDPLoadTextureTile(
                gfx++,
                (u32) &
                    ((
                        u16 *
                    )f_StaffRollImage.texels)[index * IMAGE_STRIP_STEP * STAFF_ROLL_IMAGE_WIDTH] &
                    ~7,
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                STAFF_ROLL_IMAGE_WIDTH,
                0,
                0,
                0,
                STAFF_ROLL_IMAGE_WIDTH - 1,
                stripHeight - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(SCREEN_BORDER),
                QuarterPixels(SCREEN_BORDER + (index * IMAGE_STRIP_STEP)),
                QuarterPixels(SCREEN_BORDER + STAFF_ROLL_IMAGE_WIDTH),
                QuarterPixels(SCREEN_BORDER + (index * IMAGE_STRIP_STEP) + stripHeight),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void MeasureStaffLine(s32 lineIndex, s32 *width, s32 *height) {
    s32 lineWidth = 0;
    s32 index;
    s32 lineStart = STAFF_LINE(f_StaffText, lineIndex)->start;
    s32 length = STAFF_LINE(f_StaffText, lineIndex)->length;
    s32 firstCharacter = FIRST_STAFF_CHARACTER(f_StaffText);
    s32 character;

    for (index = 0; index != length; index++) {
        character = STAFF_CHARACTER(f_StaffText, firstCharacter + lineStart + index);
        if (character < STAFF_CHARACTER_LIMIT && !IS_OVERLAID_CHARACTER(character)) {
            lineWidth += STAFF_GLYPH_WIDTH;
        }
    }

    *width = lineWidth;
    *height = 0;
}

void DrawStaffLine(Gfx **displayListHead, s32 left, s32 top, s32 lineIndex) {
    Gfx *gfx = *displayListHead;
    s32 lineStart = STAFF_LINE(f_StaffText, lineIndex)->start;
    s32 length = STAFF_LINE(f_StaffText, lineIndex)->length;
    s32 firstCharacter = FIRST_STAFF_CHARACTER(f_StaffText);
    s32 width;
    s32 height;
    s32 index;
    s32 character;
    s32 row;
    s32 column;
    s32 glyphLeft;
    s32 glyphTop;

    MeasureStaffLine(lineIndex, &width, &height);
    gDPPipeSync(gfx++);
    for (index = 0; index != length; index++) {
        character = STAFF_CHARACTER(f_StaffText, lineStart + index + firstCharacter);
        if (character < STAFF_CHARACTER_LIMIT) {
            row = character / STAFF_FONT_COLUMN_COUNT;
            column = character - (row * STAFF_FONT_COLUMN_COUNT);
            glyphLeft = column * STAFF_GLYPH_WIDTH;
            glyphTop = row * STAFF_GLYPH_HEIGHT;
            gDPPipeSync(gfx++);
            gDPLoadTextureTile_4b(
                gfx++,
                TextureAddress(ImageBody(f_StaffFont)),
                G_IM_FMT_I,
                STAFF_FONT_WIDTH,
                0,
                glyphLeft,
                glyphTop,
                glyphLeft + STAFF_GLYPH_WIDTH - 1,
                glyphTop + STAFF_GLYPH_HEIGHT - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            if (IS_OVERLAID_CHARACTER(character)) {
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(left - OVERLAID_CHARACTER_SHIFT_X),
                    QuarterPixels(top - OVERLAID_CHARACTER_SHIFT_Y),
                    QuarterPixels(left + OVERLAID_CHARACTER_SHIFT_X),
                    QuarterPixels(top - OVERLAID_CHARACTER_SHIFT_Y + STAFF_GLYPH_HEIGHT),
                    G_TX_RENDERTILE,
                    TexelCoordinate(glyphLeft),
                    TexelCoordinate(glyphTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
            else {
                gSPTextureRectangle(
                    gfx++,
                    QuarterPixels(left),
                    QuarterPixels(top),
                    QuarterPixels(left + STAFF_GLYPH_WIDTH),
                    QuarterPixels(top + STAFF_GLYPH_HEIGHT),
                    G_TX_RENDERTILE,
                    TexelCoordinate(glyphLeft),
                    TexelCoordinate(glyphTop),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                left += f_StaffGlyphAdvances[character];
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawStaffRollPainting(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 alpha;
    s32 stripIndex;
    s32 stripHeight;
    s32 scrolledRows;
    s32 texelOffset;

    if (f_StaffRollFrame > TIMELINE_START_FRAME) {
        alpha = OPAQUE_ALPHA - ((f_StaffRollFrame - TIMELINE_START_FRAME) * PAINTING_FADE_RATE);
        if (alpha < 0) {
            f_IsPaintingShown = FALSE;
            alpha = 0;
        }
    }
    else {
        alpha = OPAQUE_ALPHA;
    }

    gDPPipeSync(gfx++);
    if (alpha > 0) {
        if (alpha == OPAQUE_ALPHA) {
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            if (f_StaffRollFrame < PAINTING_FADE_IN_FRAMES) {
                gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, f_StaffRollFrame * PAINTING_FADE_RATE);
                gDPSetCombineLERP(
                    gfx++,
                    TEXEL0,
                    0,
                    PRIMITIVE_ALPHA,
                    0,
                    0,
                    0,
                    0,
                    1,
                    TEXEL0,
                    0,
                    PRIMITIVE_ALPHA,
                    0,
                    0,
                    0,
                    0,
                    1
                );
            }
        }
        else {
            if (alpha < CLOUD_ALPHA) {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            }
            else {
                gDPPipeSync(gfx++);
                gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            }

            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, alpha);
            gDPSetCombineLERP(
                gfx++,
                0,
                0,
                0,
                TEXEL0,
                0,
                0,
                0,
                PRIMITIVE,
                0,
                0,
                0,
                TEXEL0,
                0,
                0,
                0,
                PRIMITIVE
            );
        }

        gDPPipeSync(gfx++);
        gDPSetAlphaDither(gfx++, G_AD_PATTERN);
        gDPPipeSync(gfx++);
        gDPSetColorDither(gfx++, G_CD_MAGICSQ);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPLoadTLUT_pal256(gfx++, TextureAddress(f_StaffRollImage.texels));
        for (stripIndex = 0; stripIndex != IMAGE_STRIP_COUNT; stripIndex++) {
            scrolledRows = f_StaffRollFrame - PAINTING_SCROLL_FRAME;
            if (scrolledRows > PAINTING_SCROLL_LIMIT) {
                scrolledRows = PAINTING_SCROLL_LIMIT;
            }

            if (scrolledRows < 0) {
                scrolledRows = 0;
            }

            texelOffset = (scrolledRows * STAFF_ROLL_IMAGE_WIDTH) + LARGE_PALETTED_TEXELS_OFFSET;
            stripHeight = ((stripIndex != IMAGE_STRIP_COUNT - 1) ? IMAGE_STRIP_STEP : 0) |
                IMAGE_STRIP_OVERLAP;
            gDPLoadTextureTile(
                gfx++,
                (u32) &
                    ((u8 *)&f_StaffRollImage)
                        [(stripIndex * IMAGE_STRIP_STEP * STAFF_ROLL_IMAGE_WIDTH) + texelOffset] &
                    ~7,
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                STAFF_ROLL_IMAGE_WIDTH,
                0,
                0,
                0,
                STAFF_ROLL_IMAGE_WIDTH - 1,
                stripHeight - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(SCREEN_BORDER),
                QuarterPixels(SCREEN_BORDER + (stripIndex * IMAGE_STRIP_STEP)),
                QuarterPixels(SCREEN_BORDER + STAFF_ROLL_IMAGE_WIDTH),
                QuarterPixels(SCREEN_BORDER + (stripIndex * IMAGE_STRIP_STEP) + stripHeight),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DoNothingAfterStaffRoll(void) {
}
