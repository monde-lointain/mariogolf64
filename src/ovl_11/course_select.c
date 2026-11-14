#include "course_select.h"

#include <ultra64.h>

#include "asset.h"
#include "c_language.h"
#include "compiler.h"
#include "course.h"
#include "game_flags.h"
#include "game_state.h"
#include "gbi_helpers.h"
#include "heap.h"
#include "memory_address.h"
#include "menu.h"
#include "menu_memory.h"
#include "save.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"

enum CourseSelectState {
    COURSE_SELECT_UNLOADED,
    COURSE_SELECT_RING_SHOT,
    COURSE_SELECT_HOLES
};

typedef s32 CourseSelectState;

#define DIRECTION_UP 1
#define DIRECTION_DOWN 2
#define DIRECTION_LEFT 4
#define DIRECTION_RIGHT 8

enum PlanksState {
    PLANKS_SHUT = -1,
    PLANKS_CLOSING,
    PLANKS_OPENING
};

typedef s32 PlanksState;

#define StarsToOpen(courseCount) (((courseCount) - 1) * RING_SHOT_STARS_PER_OPENED_COURSE)

#define ASSET_HOLE_SIGN 0x739
#define ASSET_PLANKS 0x8ED
#define ASSET_COURSE_IMAGE_PALETTE 0x876
#define ASSET_BLANK_COURSE_IMAGE 0x834
#define ASSET_SPINNING_EMPTY_STAR 0x5E5

#define STAR_FRAME_DURATION 2
#define STAR_BUFFER_COUNT 2
#define STILL_STAR_IMAGE STAR_BUFFER_COUNT
#define STAR_IMAGE_COUNT (STAR_BUFFER_COUNT + 1)
#define STAR_GROWTH 16
#define STARS_LEFT 46
#define STAR_SPACING 38
#define STARS_TOP 88
#define TILE_SPINNING_STAR 0
#define TILE_SPINNING_EMPTY_STAR 1
#define TILE_STILL_STAR 2
#define TILE_STILL_EMPTY_STAR 3
#define STAR_TMEM_STEP 0x40

#define COURSE_IMAGE_STRIP_SIZE (COURSE_IMAGE_WIDTH * COURSE_IMAGE_STRIP_HEIGHT)
#define COURSE_IMAGE_LEFT 128
#define RING_SHOT_COURSE_IMAGE_TOP 144
#define HOLE_COURSE_IMAGE_TOP 152
#define COURSE_IMAGE_FRONT 2
#define COURSE_IMAGE_BLANK (-1)
#define COURSE_TURN_FRAME_COUNT 7
#define COURSE_TURN_MIDDLE 4

#define ARROW_FRAME_DURATION 4
#define ARROW_LEFT 152
#define ARROW_PUSH_UP (-6)
#define ARROW_PUSH_DOWN 6
#define ARROW_STEP 4
#define RING_SHOT_UP_ARROW_TOP 132
#define RING_SHOT_DOWN_ARROW_TOP 204
#define HOLE_UP_ARROW_TOP 140
#define HOLE_DOWN_ARROW_TOP 212
#define SIDE_ARROW_TOP 44
#define LEFT_ARROW_LEFT 104
#define RIGHT_ARROW_LEFT 200

#define NAME_PLATE_HALF_WIDTH 104
#define NAME_PLATE_TEXEL_STEP 984

#define HOLE_SIGN_WIDTH 176
#define HOLE_SIGN_STRIP_HEIGHT 8
#define HOLE_SIGN_STRIP_COUNT 6
#define HOLE_SIGN_LEFT 72
#define HOLE_SIGN_TOP 16
#define HOLE_SELECT_SLIDE_START 64
#define HOLE_SELECT_SLIDE_STEP 4

#define PLANKS_WIDTH 304
#define PLANK_STRIP_HEIGHT 6
#define PLANK_STRIP_COUNT 38
#define PLANK_LAST_STRIP_HEIGHT 2
#define PLANKS_MIDDLE (SCREEN_WIDTH / 2)
#define PLANKS_SLIDE_MIN SCREEN_BORDER
#define PLANK_GROUP_STRIP_COUNT 7
#define PLANK_LAST_GROUP 4
#define PLANK_GROUP_LAG 64
#define PLANK_SLIDE_MAX 480
#define PLANK_SLIDE_STEP 24

void DrawNamePlate(Gfx **displayListHead, s32 centerX, s32 centerY);
void DrawRingShotStars(Gfx **displayListHead, s32 centerX, s32 centerY);
void DrawPlanks(Gfx **displayListHead);

static AssetId f_StarFrameAssets[STAR_FRAME_COUNT] = {
    0x5BE,
    0x5BF,
    0x5C0,
    0x5C1,
    0x5C2,
    0x5C3,
    0x5C4,
    0x5C5,
    0x5C6,
    0x5C7,
    0x5C8,
    0x5C9,
    0x5CA,
    0x5CB,
    0x5CC,
};
static AssetId f_EmptyStarFrameAssets[STAR_FRAME_COUNT] = {
    0x5E5,
    0x5E6,
    0x5E7,
    0x5E8,
    0x5E9,
    0x5EA,
    0x5EB,
    0x5EC,
    0x5ED,
    0x5EE,
    0x5EF,
    0x5F0,
    0x5F1,
    0x5F2,
    0x5F3,
};
static AssetId f_CourseBadgeAssets[COURSE_INDEX_COUNT] = {
    0x733,
    0x734,
    0x735,
    0x736,
    0x737,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
    0x738,
};
static AssetId f_CourseImageAssets[COURSE_INDEX_COUNT] = {
    0x835,
    0x83A,
    0x83F,
    0x844,
    0x849,
    0x84E,
    0x853,
    0x858,
    0x85D,
    0x862,
    0x867,
    0x86C,
    0x871,
};
static s32 f_StarFrame = 0;
static u8 f_StarBufferIndex = 0;
static s8 f_StarFrameTime = 0;
static s32 f_CourseArrowFrameTime = 0;
static s32 f_CourseSelectUnusedWord UNUSED = 0;
static CourseSelectState f_LoadedScreen = COURSE_SELECT_UNLOADED;
static s32 f_SlideOffset = 0;
static CourseIndex f_ShownCourse = 0;
static CourseIndex f_NextCourse = 0;
static s32 f_CourseTurnIndex = 0;
static s32 f_CourseTurnStep = 0;
static u8 f_PressedDirection = 0;
static s32 f_CourseTurnFrames[COURSE_TURN_FRAME_COUNT] = {2, 3, 4, COURSE_IMAGE_BLANK, 0, 1, 2};
static s32 f_PlankSlide = 0;
static PlanksState f_PlankMotion = PLANKS_SHUT;

static ImageAsset *f_BoardImage;
static u8 *f_CourseBadges[COURSE_INDEX_COUNT];
static ImageAsset *f_CourseImage;
static PaletteColor *f_CourseImagePalette;
static ImageAsset *f_ArrowsImage;
static ImageAsset *f_StarImages[STAR_IMAGE_COUNT];
static ImageAsset *f_EmptyStarImages[STAR_IMAGE_COUNT];
static ImageAsset *f_PlanksImage;
static s32 f_PressedButtons;
static u32 f_RepeatedButtons;
static u8 f_UnusedBufferAfterRepeatedButtons[0x40] UNUSED;

s32 CountOpenRingShotCourses(void) {
    SaveData *save = GetSaveData();
    s32 starCount = 0;
    CourseIndex courseIndex;
    s32 holeIndex;

    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->records.hasRingShotStar[courseIndex][holeIndex] > 0;
        }
    }

    if (IsGameFlagSet(GAME_FLAG_ALL_COURSES)) {
        return RING_SHOT_COURSE_COUNT;
    }

    if (starCount >= StarsToOpen(6)) {
        return 6;
    }

    if (starCount >= StarsToOpen(5)) {
        return 5;
    }

    if (starCount >= StarsToOpen(4)) {
        return 4;
    }

    if (starCount >= StarsToOpen(3)) {
        return 3;
    }

    if (starCount >= StarsToOpen(2)) {
        return 2;
    }

    return 1;
}

void LoadCourseSelect(Boolean isHoleSelect) {
    EncodedAsset asset;
    AssetFile file;
    CourseIndex courseIndex;
    s32 size;

    if (f_LoadedScreen != COURSE_SELECT_UNLOADED) {
        osSyncPrintf("ring const err !");
        return;
    }

    f_PressedDirection = 0;
    if (!isHoleSelect) {
        f_LoadedScreen = COURSE_SELECT_RING_SHOT;
        f_BoardImage = AllocMemory(OpenEncodedAsset(ASSET_NAME_PLATE, &asset));
        ReadEncodedAsset(f_BoardImage, &asset);
        f_StarImages[0] = AllocMemory(OpenEncodedAsset(ASSET_SPINNING_STAR, &asset));
        ReadEncodedAsset(f_StarImages[0], &asset);
        f_StarImages[1] = AllocMemory(OpenEncodedAsset(ASSET_SPINNING_STAR, &asset));
        ReadEncodedAsset(f_StarImages[1], &asset);
        f_StarImages[STILL_STAR_IMAGE] = AllocMemory(OpenEncodedAsset(ASSET_SPINNING_STAR, &asset));
        ReadEncodedAsset(f_StarImages[STILL_STAR_IMAGE], &asset);
        f_EmptyStarImages[0] = AllocMemory(OpenEncodedAsset(ASSET_SPINNING_EMPTY_STAR, &asset));
        ReadEncodedAsset(f_EmptyStarImages[0], &asset);
        f_EmptyStarImages[1] = AllocMemory(OpenEncodedAsset(ASSET_SPINNING_EMPTY_STAR, &asset));
        ReadEncodedAsset(f_EmptyStarImages[1], &asset);
        f_EmptyStarImages[STILL_STAR_IMAGE] =
            AllocMemory(OpenEncodedAsset(ASSET_SPINNING_EMPTY_STAR, &asset));
        ReadEncodedAsset(f_EmptyStarImages[STILL_STAR_IMAGE], &asset);
        f_PlanksImage = AllocMemory(OpenEncodedAsset(ASSET_PLANKS, &asset));
        ReadEncodedAsset(f_PlanksImage, &asset);
    }
    else {
        f_LoadedScreen = COURSE_SELECT_HOLES;
        f_SlideOffset = HOLE_SELECT_SLIDE_START;
        f_BoardImage = AllocMemory(OpenEncodedAsset(ASSET_HOLE_SIGN, &asset));
        ReadEncodedAsset(f_BoardImage, &asset);
    }

    f_CourseTurnIndex = 0;
    f_CourseTurnStep = 0;
    f_ShownCourse = g_GameState.courseIndex;
    f_NextCourse = g_GameState.courseIndex;
    for (courseIndex = 0; courseIndex != COURSE_INDEX_COUNT; courseIndex++) {
        f_CourseBadges[courseIndex] =
            AllocMemory(OpenEncodedAsset(f_CourseBadgeAssets[courseIndex], &asset));
        ReadEncodedAsset(f_CourseBadges[courseIndex], &asset);
    }

    f_CourseImage = AllocMemory(
        OpenEncodedAsset(f_CourseImageAssets[g_GameState.courseIndex] + COURSE_IMAGE_FRONT, &asset)
    );
    ReadEncodedAsset(f_CourseImage, &asset);
    OpenAssetFile(ASSET_COURSE_IMAGE_PALETTE, &file);
    size = GetAssetFileSize(&file);
    f_CourseImagePalette = AllocMemory(size);
    ReadAssetFile(f_CourseImagePalette, size, &file);
    f_ArrowsImage = AllocMemory(OpenEncodedAsset(ASSET_GREEN_ARROWS_COPY, &asset));
    ReadEncodedAsset(f_ArrowsImage, &asset);
    f_StarFrame = 0;
    f_StarBufferIndex = 0;
    f_StarFrameTime = 0;
}

void TurnCourseImage(void) {
    EncodedAsset asset;
    s32 frame;

    if (f_CourseTurnStep != 0) {
        frame = f_CourseTurnFrames[f_CourseTurnIndex];
        if (frame == COURSE_IMAGE_BLANK) {
            OpenEncodedAsset(ASSET_BLANK_COURSE_IMAGE, &asset);
        }
        else {
            if (f_CourseTurnIndex < COURSE_TURN_MIDDLE) {
                if (f_CourseTurnStep != -1) {
                    OpenEncodedAsset(f_CourseImageAssets[f_ShownCourse] + frame, &asset);
                }
                else {
                    OpenEncodedAsset(f_CourseImageAssets[f_NextCourse] + frame, &asset);
                }
            }
            else if (f_CourseTurnStep == -1) {
                OpenEncodedAsset(f_CourseImageAssets[f_ShownCourse] + frame, &asset);
            }
            else {
                OpenEncodedAsset(f_CourseImageAssets[f_NextCourse] + frame, &asset);
            }
        }

        ReadEncodedAsset(f_CourseImage, &asset);
        f_CourseTurnIndex += f_CourseTurnStep;
        if (f_CourseTurnIndex != ((f_CourseTurnStep == 1) ? COURSE_TURN_FRAME_COUNT : -1)) {
            return;
        }

        f_CourseTurnStep = 0;
        f_ShownCourse = f_NextCourse;
    }
}

void FreeCourseSelect(void) {
    CourseIndex courseIndex;

    if (f_LoadedScreen == COURSE_SELECT_UNLOADED) {
        osSyncPrintf("ring dest err");
        return;
    }

    FreeMemory(&f_BoardImage);
    for (courseIndex = 0; courseIndex != COURSE_INDEX_COUNT; courseIndex++) {
        FreeMemory(&f_CourseBadges[courseIndex]);
    }

    FreeMemory(&f_CourseImage);
    FreeMemory(&f_CourseImagePalette);
    FreeMemory(&f_ArrowsImage);
    if (f_LoadedScreen == COURSE_SELECT_RING_SHOT) {
        FreeMemory(&f_StarImages[0]);
        FreeMemory(&f_StarImages[1]);
        FreeMemory(&f_StarImages[STILL_STAR_IMAGE]);
        FreeMemory(&f_EmptyStarImages[0]);
        FreeMemory(&f_EmptyStarImages[1]);
        FreeMemory(&f_EmptyStarImages[STILL_STAR_IMAGE]);
        FreeMemory(&f_PlanksImage);
    }

    f_LoadedScreen = COURSE_SELECT_UNLOADED;
}

SelectResult UpdateRingShotSelect(s32 pressedButtons, u32 repeatedButtons) {
    MenuMemory *memory = GetMenuMemory();
    s32 openCourseCount = CountOpenRingShotCourses();
    SelectResult result = SELECT_IDLE;

    f_PressedButtons = pressedButtons;
    f_RepeatedButtons = repeatedButtons;
    f_PressedDirection = 0;
    if (f_CourseTurnStep == 0 && !(pressedButtons & A_BUTTON)) {
        if (pressedButtons & B_BUTTON) {
            result = SELECT_BACK;
        }
        else if ((repeatedButtons & U_JPAD) && openCourseCount >= 2) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            result = SELECT_MOVED;
            memory->holeIndex = 0;
            memory->courseIndex--;
            f_CourseTurnStep = 1;
            f_CourseTurnIndex = 1;
            f_PressedDirection = DIRECTION_UP;
        }
        else if ((repeatedButtons & D_JPAD) && openCourseCount >= 2) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            result = SELECT_MOVED;
            memory->holeIndex = 0;
            memory->courseIndex++;
            f_CourseTurnStep = -1;
            f_CourseTurnIndex = COURSE_TURN_FRAME_COUNT - 2;
            f_PressedDirection = DIRECTION_DOWN;
        }
        else if (repeatedButtons & L_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            result = SELECT_MOVED;
            memory->holeIndex--;
            f_PressedDirection = DIRECTION_LEFT;
        }
        else if (repeatedButtons & R_JPAD) {
            PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            result = SELECT_MOVED;
            memory->holeIndex++;
            f_PressedDirection = DIRECTION_RIGHT;
        }
    }

    if (memory->courseIndex < 0) {
        memory->courseIndex = openCourseCount - 1;
    }

    if (memory->courseIndex > openCourseCount - 1) {
        memory->courseIndex = 0;
    }

    if (memory->holeIndex < 0) {
        memory->holeIndex = RING_SHOT_HOLE_COUNT - 1;
    }

    if (memory->holeIndex >= RING_SHOT_HOLE_COUNT) {
        memory->holeIndex = 0;
    }

    g_GameState.holeIndex = memory->holeIndex;
    g_GameState.courseIndex = memory->courseIndex;
    f_NextCourse = g_GameState.courseIndex;
    return result;
}

SelectResult UpdateHoleSelect(s32 pressedButtons, s32 repeatedButtons) {
    SelectResult result = SELECT_IDLE;
    s32 openCourseCount;

    GetMenuMemory();
    openCourseCount = CountOpenCourses(GetSaveData()->coursePoints);
    if (IsGameFlagSet(GAME_FLAG_ALL_COURSES)) {
        openCourseCount = COURSE_COUNT;
    }

    f_PressedDirection = 0;
    if (!(pressedButtons & A_BUTTON)) {
        if (pressedButtons & B_BUTTON) {
            return SELECT_BACK;
        }

        if (f_CourseTurnStep == 0) {
            if (repeatedButtons & L_JPAD) {
                if (!IsOnDrivingRange()) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    result = SELECT_MOVED;
                    f_PressedDirection = DIRECTION_LEFT;
                    g_GameState.holeIndex--;
                }
                else {
                    PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                }
            }
            else if (repeatedButtons & R_JPAD) {
                if (IsOnDrivingRange()) {
                    PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                }
                else {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    result = SELECT_MOVED;
                    f_PressedDirection = DIRECTION_RIGHT;
                    g_GameState.holeIndex++;
                }
            }
            else if (repeatedButtons & U_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                g_GameState.courseIndex =
                    (g_GameState.courseIndex + COURSE_INDEX_COUNT - 1) % COURSE_INDEX_COUNT;
                if (g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX &&
                    g_GameState.courseIndex < DRIVING_RANGE_COURSE_INDEX) {
                    g_GameState.isMiniGolf = TRUE;
                }
                else {
                    g_GameState.isMiniGolf = FALSE;
                }

                if (g_GameState.courseIndex > openCourseCount - 1 &&
                    g_GameState.courseIndex < MINI_GOLF_COURSE_INDEX) {
                    g_GameState.courseIndex = openCourseCount - 1;
                }

                result = SELECT_MOVED;
                f_PressedDirection = DIRECTION_UP;
                f_CourseTurnStep = 1;
                f_CourseTurnIndex = 1;
            }
            else if (repeatedButtons & D_JPAD) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
                g_GameState.courseIndex =
                    (g_GameState.courseIndex + COURSE_INDEX_COUNT + 1) % COURSE_INDEX_COUNT;
                if (g_GameState.courseIndex > openCourseCount - 1 &&
                    g_GameState.courseIndex < MINI_GOLF_COURSE_INDEX) {
                    g_GameState.courseIndex = MINI_GOLF_COURSE_INDEX;
                }

                if (g_GameState.courseIndex >= MINI_GOLF_COURSE_INDEX &&
                    g_GameState.courseIndex < DRIVING_RANGE_COURSE_INDEX) {
                    g_GameState.isMiniGolf = TRUE;
                }
                else {
                    g_GameState.isMiniGolf = FALSE;
                }

                result = SELECT_MOVED;
                f_PressedDirection = DIRECTION_DOWN;
                f_CourseTurnStep = -1;
                f_CourseTurnIndex = COURSE_TURN_FRAME_COUNT - 2;
            }
        }

        g_GameState.courseIndex =
            (g_GameState.courseIndex + COURSE_INDEX_COUNT) % COURSE_INDEX_COUNT;
        f_NextCourse = g_GameState.courseIndex;
        if (IsOnDrivingRange()) {
            g_GameState.holeIndex = 0;
        }
        else {
            g_GameState.holeIndex = (g_GameState.holeIndex + COURSE_HOLE_COUNT) % COURSE_HOLE_COUNT;
        }
    }

    return result;
}

Boolean ArePlanksOpen(void) {
    return f_PlankSlide == 0;
}

void SetPlankMotion(s32 motion) {
    if (f_PlankMotion != motion) {
        f_PlankMotion = motion;
    }
}

void DrawPlanks(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripHeight;
    s32 slide;
    s32 top;
    s32 bottom;
    s32 plankGroup;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImagePalette(f_PlanksImage));
    switch (f_PlankMotion) {
        case PLANKS_SHUT:
            break;
        case PLANKS_OPENING:
            if (f_PlankSlide > 0) {
                f_PlankSlide -= PLANK_SLIDE_STEP;
            }

            if (f_PlankSlide < 0) {
                f_PlankSlide = 0;
            }

            break;
        case PLANKS_CLOSING:
            if (f_PlankSlide < PLANK_SLIDE_MAX) {
                f_PlankSlide += PLANK_SLIDE_STEP;
            }

            if (f_PlankSlide > PLANK_SLIDE_MAX) {
                f_PlankSlide = PLANK_SLIDE_MAX;
            }

            break;
    }

    for (stripIndex = 0; stripIndex != PLANK_STRIP_COUNT; stripIndex++) {
        stripHeight =
            (stripIndex != PLANK_STRIP_COUNT - 1) ? PLANK_STRIP_HEIGHT : PLANK_LAST_STRIP_HEIGHT;
        if (f_PlankMotion == PLANKS_SHUT) {
            slide = PLANKS_MIDDLE;
        }
        else {
            if (f_PlankMotion == PLANKS_OPENING) {
                plankGroup = stripIndex / PLANK_GROUP_STRIP_COUNT;
            }
            else {
                plankGroup = stripIndex / PLANK_GROUP_STRIP_COUNT;
            }

            if (plankGroup > PLANK_LAST_GROUP) {
                plankGroup = PLANK_LAST_GROUP;
            }

            slide = f_PlankSlide - (plankGroup * PLANK_GROUP_LAG);
            if (slide > PLANKS_MIDDLE) {
                slide = PLANKS_MIDDLE;
            }

            if (slide < PLANKS_SLIDE_MIN) {
                slide = PLANKS_SLIDE_MIN;
            }
        }

        gDPPipeSync(gfx++);
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_PlanksImage) + (stripIndex * PLANK_STRIP_HEIGHT * PLANKS_WIDTH),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            PLANKS_WIDTH,
            stripHeight,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        top = QuarterPixels(SCREEN_BORDER + (stripIndex * PLANK_STRIP_HEIGHT));
        bottom = QuarterPixels(SCREEN_BORDER + (stripIndex * PLANK_STRIP_HEIGHT) + stripHeight);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(SCREEN_BORDER),
            top,
            QuarterPixels(slide),
            bottom,
            G_TX_RENDERTILE,
            TexelCoordinate(PLANKS_MIDDLE - slide),
            TexelCoordinate(1),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(SCREEN_WIDTH - slide),
            top,
            QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER),
            bottom,
            G_TX_RENDERTILE,
            TexelCoordinate(PLANKS_WIDTH / 2),
            TexelCoordinate(1),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    *displayListHead = gfx;
}

void DrawRingShotSelect(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    MenuMemory *memory = GetMenuMemory();
    s32 stripIndex;
    s16 arrowFrame;
    s32 push;
    u8 unusedBuffer[0x1C] UNUSED;

    if (f_LoadedScreen == COURSE_SELECT_RING_SHOT) {
        DrawPlanks(&gfx);
        DrawNamePlate(&gfx, SCREEN_WIDTH / 2, 48);
        TurnCourseImage();
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_POINT);
        stripIndex = 0;
        LoadLargePalette(&gfx, f_CourseImagePalette);
        for (; stripIndex != COURSE_IMAGE_STRIP_COUNT; stripIndex++) {
            LoadTextureBlock(
                &gfx,
                f_CourseImage + (stripIndex * COURSE_IMAGE_STRIP_SIZE),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_IMAGE_WIDTH,
                COURSE_IMAGE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(COURSE_IMAGE_LEFT),
                QuarterPixels(
                    (stripIndex * COURSE_IMAGE_STRIP_HEIGHT) + RING_SHOT_COURSE_IMAGE_TOP +
                    f_SlideOffset
                ),
                QuarterPixels(COURSE_IMAGE_LEFT + COURSE_IMAGE_WIDTH),
                QuarterPixels(
                    (stripIndex * COURSE_IMAGE_STRIP_HEIGHT) +
                    (COURSE_IMAGE_STRIP_HEIGHT + RING_SHOT_COURSE_IMAGE_TOP) + f_SlideOffset
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        LoadSmallPalette(&gfx, 0, ImagePalette(f_ArrowsImage));
        if (g_GameState.replayKind == REPLAY_KIND_NONE && CountOpenRingShotCourses() >= 2) {
            arrowFrame =
                f_CourseArrowFrameTime++ % (2 * ARROW_FRAME_DURATION) >= ARROW_FRAME_DURATION;
            LoadNibbleTextureBlock(
                &gfx,
                ImageTexels(f_ArrowsImage) + (arrowFrame * GREEN_ARROW_FRAME_SIZE),
                G_IM_FMT_CI,
                GREEN_ARROW_SIZE,
                GREEN_ARROW_FRAME_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            push = (f_RepeatedButtons & U_JPAD) ? ARROW_PUSH_UP : 0;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(ARROW_LEFT),
                QuarterPixels(
                    push + RING_SHOT_UP_ARROW_TOP -
                    (f_PressedDirection == DIRECTION_UP ? ARROW_STEP : 0)
                ),
                QuarterPixels(ARROW_LEFT + GREEN_ARROW_SIZE),
                QuarterPixels(
                    push + RING_SHOT_UP_ARROW_TOP + GREEN_ARROW_SIZE -
                    (f_PressedDirection == DIRECTION_UP ? ARROW_STEP : 0)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            push = (f_RepeatedButtons & D_JPAD) ? ARROW_PUSH_DOWN : 0;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(ARROW_LEFT),
                QuarterPixels(
                    push + RING_SHOT_DOWN_ARROW_TOP +
                    (f_PressedDirection == DIRECTION_DOWN ? ARROW_STEP : 0)
                ),
                QuarterPixels(ARROW_LEFT + GREEN_ARROW_SIZE),
                QuarterPixels(
                    push + RING_SHOT_DOWN_ARROW_TOP + GREEN_ARROW_SIZE +
                    (f_PressedDirection == DIRECTION_DOWN ? ARROW_STEP : 0)
                ),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_DOWN)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        DrawRingShotStars(&gfx, SCREEN_WIDTH / 2, 150);
        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        SetTextSlot(
            SCREEN_WIDTH / 2,
            42,
            (memory->courseIndex * RING_SHOT_HOLE_COUNT) + memory->holeIndex +
                MESSAGE_FIRST_RING_SHOT_HOLE,
            TEXT_SLOT_LARGE_FONT,
            stripIndex,
            TEXT_FLAG_CENTERED | TEXT_FLAG_DRAWN_BY_OWNER,
            255,
            255,
            255,
            OPAQUE_ALPHA
        );
        DrawLargeTextSlot(&gfx, stripIndex);
        *displayListHead = gfx;
    }
}

void DrawHoleSelect(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 arrowFrame;
    s32 push;
    s32 colorIndex;

    GetMenuMemory();
    f_SlideOffset -= HOLE_SELECT_SLIDE_STEP;
    if (f_SlideOffset < 0) {
        f_SlideOffset = 0;
    }

    if (f_LoadedScreen == COURSE_SELECT_HOLES) {
        gDPPipeSync(gfx++);
        gSPDisplayList(gfx++, g_DecalTextureSetup);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        LoadLargePalette(&gfx, ImagePalette(f_BoardImage));
        for (stripIndex = 0; stripIndex != HOLE_SIGN_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(
                    LargeImageTexels(f_BoardImage) +
                    stripIndex * HOLE_SIGN_STRIP_HEIGHT * HOLE_SIGN_WIDTH
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                HOLE_SIGN_WIDTH,
                0,
                0,
                0,
                HOLE_SIGN_WIDTH - 1,
                HOLE_SIGN_STRIP_HEIGHT - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(HOLE_SIGN_LEFT),
                QuarterPixels(
                    (stripIndex * HOLE_SIGN_STRIP_HEIGHT) + HOLE_SIGN_TOP - f_SlideOffset
                ),
                QuarterPixels(HOLE_SIGN_LEFT + HOLE_SIGN_WIDTH),
                QuarterPixels(
                    (stripIndex * HOLE_SIGN_STRIP_HEIGHT) +
                    (HOLE_SIGN_TOP + HOLE_SIGN_STRIP_HEIGHT) - f_SlideOffset
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        TurnCourseImage();
        LoadLargePalette(&gfx, f_CourseImagePalette);
        for (stripIndex = 0; stripIndex != COURSE_IMAGE_STRIP_COUNT; stripIndex++) {
            LoadTextureBlock(
                &gfx,
                f_CourseImage + (stripIndex * COURSE_IMAGE_STRIP_SIZE),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_IMAGE_WIDTH,
                COURSE_IMAGE_STRIP_HEIGHT,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(COURSE_IMAGE_LEFT),
                QuarterPixels(
                    (stripIndex * COURSE_IMAGE_STRIP_HEIGHT) + HOLE_COURSE_IMAGE_TOP + f_SlideOffset
                ),
                QuarterPixels(COURSE_IMAGE_LEFT + COURSE_IMAGE_WIDTH),
                QuarterPixels(
                    (stripIndex * COURSE_IMAGE_STRIP_HEIGHT) +
                    (COURSE_IMAGE_STRIP_HEIGHT + HOLE_COURSE_IMAGE_TOP) + f_SlideOffset
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        arrowFrame =
            (f_CourseArrowFrameTime++ % (2 * ARROW_FRAME_DURATION) >= ARROW_FRAME_DURATION) *
            GREEN_ARROW_FRAME_SIZE;
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        LoadSmallPalette(&gfx, 0, ImagePalette(f_ArrowsImage));
        LoadNibbleTextureBlock(
            &gfx,
            ImageTexelsAt(f_ArrowsImage, arrowFrame),
            G_IM_FMT_CI,
            GREEN_ARROW_SIZE,
            GREEN_ARROW_FRAME_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        push = (f_RepeatedButtons & U_JPAD) ? ARROW_PUSH_UP : 0;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(ARROW_LEFT),
            QuarterPixels(
                push + HOLE_UP_ARROW_TOP + f_SlideOffset -
                (f_PressedDirection == DIRECTION_UP ? ARROW_STEP : 0)
            ),
            QuarterPixels(ARROW_LEFT + GREEN_ARROW_SIZE),
            QuarterPixels(
                push + HOLE_UP_ARROW_TOP + GREEN_ARROW_SIZE + f_SlideOffset -
                (f_PressedDirection == DIRECTION_UP ? ARROW_STEP : 0)
            ),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        push = (f_RepeatedButtons & D_JPAD) ? ARROW_PUSH_DOWN : 0;
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(ARROW_LEFT),
            QuarterPixels(
                push + HOLE_DOWN_ARROW_TOP + f_SlideOffset +
                (f_PressedDirection == DIRECTION_DOWN ? ARROW_STEP : 0)
            ),
            QuarterPixels(ARROW_LEFT + GREEN_ARROW_SIZE),
            QuarterPixels(
                push + HOLE_DOWN_ARROW_TOP + GREEN_ARROW_SIZE + f_SlideOffset +
                (f_PressedDirection == DIRECTION_DOWN ? ARROW_STEP : 0)
            ),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(GreenArrowRow(MENU_ARROW_DOWN)),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
        if (!IsOnDrivingRange()) {
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    f_PressedDirection == DIRECTION_LEFT ? LEFT_ARROW_LEFT - ARROW_STEP :
                                                           LEFT_ARROW_LEFT
                ),
                QuarterPixels(SIDE_ARROW_TOP - f_SlideOffset),
                QuarterPixels(
                    f_PressedDirection == DIRECTION_LEFT ?
                        LEFT_ARROW_LEFT - ARROW_STEP + GREEN_ARROW_SIZE :
                        LEFT_ARROW_LEFT + GREEN_ARROW_SIZE
                ),
                QuarterPixels(SIDE_ARROW_TOP + GREEN_ARROW_SIZE - f_SlideOffset),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_LEFT)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    f_PressedDirection == DIRECTION_RIGHT ? RIGHT_ARROW_LEFT + ARROW_STEP :
                                                            RIGHT_ARROW_LEFT
                ),
                QuarterPixels(SIDE_ARROW_TOP - f_SlideOffset),
                QuarterPixels(
                    f_PressedDirection == DIRECTION_RIGHT ?
                        RIGHT_ARROW_LEFT + ARROW_STEP + GREEN_ARROW_SIZE :
                        RIGHT_ARROW_LEFT + GREEN_ARROW_SIZE
                ),
                QuarterPixels(SIDE_ARROW_TOP + GREEN_ARROW_SIZE - f_SlideOffset),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(GreenArrowRow(MENU_ARROW_RIGHT)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        gDPPipeSync(gfx++);
        SetUpGradientTextDrawing(&gfx, 0);
        SetTextColor(&gfx, 255, 255, 255);
        colorIndex = g_GameState.courseIndex;
        if (colorIndex >= MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD) {
            colorIndex -= MINI_GOLF_COURSES_PER_RECORD;
        }

        sprintf(g_PrintBuffer, "%s", g_CourseRecords[GetCourseRecordIndex()].name);
        DrawLargeTextWithShadow(
            &gfx,
            SCREEN_WIDTH / 2,
            24 - f_SlideOffset,
            g_PrintBuffer,
            g_CourseNameEnvColors[colorIndex],
            g_CourseNamePrimColors[colorIndex],
            TEXT_FLAG_CENTERED
        );
        if (!IsOnDrivingRange()) {
            sprintf(g_PrintBuffer, "Hole %d", g_GameState.holeIndex + 1);
            DrawLargeTextWithShadow(
                &gfx,
                SCREEN_WIDTH / 2,
                44 - f_SlideOffset,
                g_PrintBuffer,
                g_CourseNameEnvColors[colorIndex],
                g_CourseNamePrimColors[colorIndex],
                TEXT_FLAG_CENTERED
            );
        }

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

#define NamePlateStripTop(centerY, strip) ((centerY) + ((strip) * NAME_PLATE_STRIP_HEIGHT))

void DrawNamePlate(Gfx **displayListHead, s32 centerX, s32 centerY) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImagePalette(f_BoardImage));
    for (stripIndex = 0; stripIndex != NAME_PLATE_STRIP_COUNT; stripIndex++) {
        LoadTextureBlock(
            &gfx,
            LargeImageTexels(f_BoardImage) +
                (stripIndex * NAME_PLATE_STRIP_HEIGHT * NAME_PLATE_WIDTH),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            NAME_PLATE_WIDTH,
            NAME_PLATE_STRIP_HEIGHT,
            0,
            0,
            0,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(centerX - NAME_PLATE_HALF_WIDTH),
            QuarterPixels(NamePlateStripTop(centerY, stripIndex) - 15),
            QuarterPixels(centerX + NAME_PLATE_HALF_WIDTH),
            QuarterPixels(NamePlateStripTop(centerY, stripIndex) - 5),
            G_TX_RENDERTILE,
            0,
            0,
            NAME_PLATE_TEXEL_STEP,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

void DrawRingShotStars(Gfx **displayListHead, s32 centerX UNUSED, s32 centerY UNUSED) {
    EncodedAsset asset;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    Gfx *gfx = *displayListHead;

    f32 hexagon[6][2] UNUSED = {
        {0.0f, -1.0f},
        {0.89f, -0.45f},
        {0.89f, 0.45f},
        {0.0f, 1.0f},
        {-0.89f, 0.45f},
        {-0.89f, -0.45f},
    };
    u8 holeIndex;
    s32 width;
    s32 height;
    s32 left;
    s32 top;
    s32 tile;
    CourseIndex courseIndex;
    ImageAsset *spinningImage = f_StarImages[f_StarBufferIndex];

    LoadSmallPalette(&gfx, TILE_SPINNING_STAR, ImagePalette(spinningImage));
    gDPLoadTextureBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(spinningImage)),
        G_IM_FMT_CI,
        STAR_WIDTH,
        STAR_HEIGHT,
        TILE_SPINNING_STAR,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    spinningImage = f_EmptyStarImages[f_StarBufferIndex];
    LoadSmallPalette(&gfx, TILE_SPINNING_EMPTY_STAR, ImagePalette(spinningImage));
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(spinningImage)),
        STAR_TMEM_STEP * TILE_SPINNING_EMPTY_STAR,
        TILE_SPINNING_EMPTY_STAR,
        G_IM_FMT_CI,
        STAR_WIDTH,
        STAR_HEIGHT,
        TILE_SPINNING_EMPTY_STAR,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    LoadSmallPalette(&gfx, TILE_STILL_STAR, ImagePalette(f_StarImages[STILL_STAR_IMAGE]));
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_StarImages[STILL_STAR_IMAGE])),
        STAR_TMEM_STEP * TILE_STILL_STAR,
        TILE_STILL_STAR,
        G_IM_FMT_CI,
        STAR_WIDTH,
        STAR_HEIGHT,
        TILE_STILL_STAR,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    LoadSmallPalette(
        &gfx,
        TILE_STILL_EMPTY_STAR,
        ImagePalette(f_EmptyStarImages[STILL_STAR_IMAGE])
    );
    gDPLoadMultiBlock_4b(
        gfx++,
        TextureAddress(ImageTexels(f_EmptyStarImages[STILL_STAR_IMAGE])),
        STAR_TMEM_STEP * TILE_STILL_EMPTY_STAR,
        TILE_STILL_EMPTY_STAR,
        G_IM_FMT_CI,
        STAR_WIDTH,
        STAR_HEIGHT,
        TILE_STILL_EMPTY_STAR,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    for (holeIndex = 0; holeIndex < RING_SHOT_HOLE_COUNT; holeIndex++) {
        if (holeIndex == memory->holeIndex) {
            width = STAR_GROWTH;
            height = STAR_GROWTH;
        }
        else {
            width = 0;
            height = 0;
        }

        left = (holeIndex * STAR_SPACING) + STARS_LEFT;
        top = STARS_TOP;

        courseIndex = memory->courseIndex;
        if (holeIndex == memory->holeIndex) {
            tile = !save->records.hasRingShotStar[courseIndex][holeIndex] ?
                TILE_SPINNING_EMPTY_STAR :
                TILE_SPINNING_STAR;
        }
        else {
            tile = !save->records.hasRingShotStar[courseIndex][holeIndex] ? TILE_STILL_EMPTY_STAR :
                                                                            TILE_STILL_STAR;
        }

        DrawTextureRectangle(
            &gfx,
            QuarterPixels(left - (width / 2)),
            QuarterPixels(top - (height / 2)),
            QuarterPixels(left + STAR_WIDTH + (width / 2)),
            QuarterPixels(top + STAR_HEIGHT + (height / 2)),
            tile,
            0,
            0,
            (STAR_WIDTH * TEXEL_STEP_ONE) / (width + STAR_WIDTH),
            (STAR_HEIGHT * TEXEL_STEP_ONE) / (height + STAR_HEIGHT)
        );
        gDPPipeSync(gfx++);
    }

    f_StarFrameTime++;
    if (f_StarFrameTime == STAR_FRAME_DURATION) {
        f_StarFrameTime = 0;
        f_StarFrame++;
        if (f_StarFrame >= STAR_FRAME_COUNT) {
            f_StarFrame = 0;
        }

        f_StarBufferIndex = f_StarBufferIndex == 0;
        OpenEncodedAsset(f_StarFrameAssets[f_StarFrame], &asset);
        ReadEncodedAsset(f_StarImages[f_StarBufferIndex], &asset);
        OpenEncodedAsset(f_EmptyStarFrameAssets[f_StarFrame], &asset);
        ReadEncodedAsset(f_EmptyStarImages[f_StarBufferIndex], &asset);
    }

    *displayListHead = gfx;
}
