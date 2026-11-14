/*
 * The clubhouse's Best Times screen. Left and right turn it between the open
 * courses, and it shows each course's best times with their places and holders.
 */

#include "menus/best_times.h"

#include <ultra64.h>

#include "audio/sound.h"
#include "camera/view_projection.h"
#include "characters/character.h"
#include "course/course.h"
#include "course/work_area.h"
#include "game/game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "menus/course_stats_screen.h"
#include "menus/main_menu.h"
#include "menus/menu.h"
#include "menus/menu_arrows.h"
#include "menus/menu_memory.h"
#include "menus/menu_number.h"
#include "menus/player_stats.h"
#include "menus/save_screens.h"
#include "save/save.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/sky_and_hud_animations.h"
#include "system/asset.h"
#include "system/compiler.h"
#include "system/frame_time.h"
#include "system/memory_address.h"
#include "text/text.h"

// Each character's face, 32 by 24, one above another
#define ASSET_CHARACTER_FACE_STRIP 0x8E9

// The images of g_CourseIconAssets that LoadBestTimesCourseIcons reads
#define BEST_TIMES_IMAGE_COUNT 6

// The best times' courses turn this far in all between one course and the next,
// and this fast
#define BEST_TIMES_TURN 90
#define BEST_TIMES_SPIN_SPEED 18

// ASSET_PLACES holds 32 by 32 frames of the digits 1 to 4, then the
// suffixes ST, ND, RD and TH, each drawn at 24 by 24
#define PLACE_LABEL_SIZE 24
#define PLACE_LABEL_SUFFIX_X 16
#define PLACE_LABEL_LAST_PLACE 3

// DrawStopwatchTime writes a best time as minutes, seconds and hundredths, two
// digits each, from the 16 by 16 CI4 glyphs of f_StopwatchGlyphs: the digits
// follow a first glyph, and a separator follows each group but the last
#define TIME_GROUP_COUNT 3
#define TIME_GROUP_DIGIT_COUNT 2
#define HUNDREDTHS_PER_MINUTE (SECONDS_PER_MINUTE * HUNDREDTHS_PER_SECOND)
#define TIME_GLYPH_SIZE 16
#define TIME_GLYPH_BYTES (TIME_GLYPH_SIZE * TIME_GLYPH_SIZE / 2)
#define TIME_FIRST_DIGIT_GLYPH 1
#define TIME_SEPARATOR_GLYPH 11
#define TIME_DIGIT_SPACING 14
#define TIME_SEPARATOR_SHIFT_LEFT 4
#define TIME_SEPARATOR_SPACING 6

void DrawPlaceLabel(Gfx **displayListHead, s32 left, s32 top, s32 place);
void DrawStopwatchTime(Gfx **displayListHead, s32 left, s32 top, s32 frameCount);

// The side arrows' blinking frame and its time shown; the third word is cleared
// with them and read by no routine
static s32 f_BestTimesArrowFrame = 0;
static s32 f_BestTimesArrowTimer = 0;
static s32 f_UnusedBestTimesArrowWord = 0;
// Where each character's face is in f_CharacterFaceStrip's image, and its name
// in g_MenuCharacterNames
static s32 f_CharacterChoices[CHARACTER_COUNT] = {
    CHARACTER_CHOICE_PLUM,
    CHARACTER_CHOICE_CHARLIE,
    CHARACTER_CHOICE_MAPLE,
    CHARACTER_CHOICE_BABY_MARIO,
    CHARACTER_CHOICE_LUIGI,
    CHARACTER_CHOICE_PEACH,
    CHARACTER_CHOICE_SONNY,
    CHARACTER_CHOICE_WARIO,
    CHARACTER_CHOICE_HARRY,
    CHARACTER_CHOICE_MARIO,
    CHARACTER_CHOICE_YOSHI,
    CHARACTER_CHOICE_DONKEY_KONG,
    CHARACTER_CHOICE_BOWSER,
    CHARACTER_CHOICE_METAL_MARIO,
    CHARACTER_CHOICE_KID,
    CHARACTER_CHOICE_SHERRY,
    CHARACTER_CHOICE_AZALEA,
    CHARACTER_CHOICE_JOE,
};

// The camera that DrawBestTimes views the course wheel with
static Mat4 f_BestTimesWheelProjection;
static Mat4 f_BestTimesWheelView;
// The images that the Load routines read: the course icons, the face strip,
// the stopwatch's glyphs, the place labels and the side arrows
static ImageAsset **f_BestTimesCourseIcons;
static ImageAsset *f_CharacterFaceStrip;
static ImageAsset *f_StopwatchGlyphs;
static ImageAsset *f_PlaceLabels;
static ImageAsset *f_BestTimesGreenArrows;

// Read by no routine; 2 objects, as one would be aligned past its start
static u8 f_UnusedBytesAfterGreenArrows[0x4] UNUSED;
static u8 f_UnusedBufferAfterGreenArrows[0x88] UNUSED;

// Reads every course's icon into the Best Times screen's buffers
void LoadBestTimesCourseIcons(ImageAsset **buffers) {
    EncodedAsset asset;

    s16 imageIndex;

    f_BestTimesCourseIcons = buffers;
    for (imageIndex = 0; imageIndex != BEST_TIMES_IMAGE_COUNT; imageIndex++) {
        OpenEncodedAsset(g_CourseIconAssets[imageIndex], &asset);
        ReadEncodedAsset(f_BestTimesCourseIcons[imageIndex], &asset);
    }
}

// Reads the character face strip into buffer, for Best Shots and Ring Shot
void LoadCharacterFaceStrip(void *buffer) {
    EncodedAsset asset;

    f_CharacterFaceStrip = buffer;
    OpenEncodedAsset(ASSET_CHARACTER_FACE_STRIP, &asset);
    ReadEncodedAsset(f_CharacterFaceStrip, &asset);
}

void LoadBestTimesBuffer(void *buffer, BestTimesBuffer bufferKind) {
    EncodedAsset asset;

    switch (bufferKind) {
        case BEST_TIMES_BUFFER_FACE_STRIP:
            f_CharacterFaceStrip = &g_WorkArea;
            OpenEncodedAsset(ASSET_CHARACTER_FACE_STRIP, &asset);
            ReadEncodedAsset(f_CharacterFaceStrip, &asset);
            break;
        case BEST_TIMES_BUFFER_UNREAD:
            f_StopwatchGlyphs = buffer;
            break;
        case BEST_TIMES_BUFFER_PLACES:
            f_PlaceLabels = buffer;
            OpenEncodedAsset(ASSET_PLACES, &asset);
            ReadEncodedAsset(f_PlaceLabels, &asset);
            break;
        case BEST_TIMES_BUFFER_GREEN_ARROWS:
            f_BestTimesGreenArrows = buffer;
            OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset);
            ReadEncodedAsset(f_BestTimesGreenArrows, &asset);
            f_BestTimesArrowFrame = 0;
            f_BestTimesArrowTimer = 0;
            f_UnusedBestTimesArrowWord = 0;
            break;
    }
}

void UpdateBestTimes(void) {
    MenuMemory *memory = GetMenuMemory();
    s32 courseCount = GetOpenCourseCount();
    s32 direction;
    s32 step;

    if (IsHudAnimationDone() == TRUE) {
        if (g_MenuPressedButtons & B_BUTTON) {
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, SOUND_FULL_SCALE);
            StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
            g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
            return;
        }

        if (memory->bestTimesSpin == 0) {
            if (g_MenuRepeatButtons & R_JPAD) {
                if (courseCount >= 2) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->bestTimesCourse++;
                    if (memory->bestTimesCourse > courseCount - 1) {
                        memory->bestTimesCourse = 0;
                    }

                    memory->bestTimesSpin = -BEST_TIMES_TURN / courseCount;
                }
            }
            else if (g_MenuRepeatButtons & L_JPAD) {
                if (courseCount >= 2) {
                    PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    memory->bestTimesCourse--;
                    if (memory->bestTimesCourse < 0) {
                        memory->bestTimesCourse = courseCount - 1;
                    }

                    memory->bestTimesSpin = BEST_TIMES_TURN / courseCount;
                }
            }
        }

        if (Abs(memory->bestTimesSpin) != 0) {
            direction = memory->bestTimesSpin < 0 ? -1 : 1;
            step = BEST_TIMES_SPIN_SPEED / courseCount;
            if (step == 0) {
                step = 1;
            }

            memory->bestTimesSpin -= direction * step;

            // Stop on reaching the course rather than turning past it
            if ((memory->bestTimesSpin < 0 ? -1 : 1) != direction) {
                memory->bestTimesSpin = 0;
            }
        }
    }
}

// DrawBestTimes turns the open courses' images on the wheel of menu.h, with
// the chosen course's name on the banner below, then lists that course's best
// times on a shaded panel: each with its place, its bestTimeScoresToPar as a
// signed number and its holder's face. A row of dashes stands in for each unset
// time.
#define BEST_TIME_PLACE_X 22
#define BEST_TIME_FIRST_PLACE_Y 144
#define BEST_TIME_PLACE_SPACING 27
#define BEST_TIME_X 72
#define BEST_TIME_FIRST_Y 148
#define BEST_TIME_SPACING 28
#define BEST_TIME_SCORE_X 190
#define BEST_TIME_FACE_X 260
#define BEST_TIME_FIRST_FACE_Y 142
#define UNSET_TIME_DASH_COUNT 12
#define UNSET_TIME_DASH_SPACING 18
#define UNSET_TIME_FIRST_DASH_Y 152
#define BEST_TIMES_PANEL_LEFT 24
#define BEST_TIMES_PANEL_TOP 138
#define BEST_TIMES_PANEL_RIGHT 302
#define BEST_TIMES_PANEL_BOTTOM 230

void DrawBestTimes(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    u16 perspectiveNormalization;
    f32 iconX[COURSE_COUNT];
    f32 iconY[COURSE_COUNT];
    f32 iconScale[COURSE_COUNT];
    // Pads the frame to match; without it 238 words differ
    s32 unusedSlots[COURSE_COUNT] UNUSED;
    s32 drawOrder[COURSE_COUNT];
    // Pads the frame to match; without it 228 words differ
    s32 unused[2] UNUSED;
    SaveData *save = GetSaveData();
    MenuMemory *memory = GetMenuMemory();
    s32 courseCount;
    // Every loop but the dashes' counts with one of these two, as the original's
    // registers show
    s16 index;
    s16 otherIndex;
    s32 key;
    CourseIndex bestTimesCourse;
    s32 spin;
    s32 angleStep;
    Radians angle;
    f32 wheelX;
    s32 drawnIndex;
    s32 halfSize;
    s32 texelStep;
    s32 time;
    CharacterIndex16 characterIndex;
    s16 dashIndex;
    s16 holderCourse;

    DrawMenuBackdropLayers(&gfx, PLAIN_BACKDROP_LAYERS);
    guPerspectiveF(
        f_BestTimesWheelProjection,
        &perspectiveNormalization,
        WHEEL_FIELD_OF_VIEW,
        SCREEN_ASPECT_RATIO,
        WHEEL_NEAR_PLANE,
        WHEEL_FAR_PLANE,
        1.0f
    );
    guLookAtF(
        f_BestTimesWheelView,
        0.0f,
        WHEEL_EYE_HEIGHT,
        WHEEL_EYE_DISTANCE,
        0.0f,
        0.0f,
        0.0f,
        0.0f,
        1.0f,
        0.0f
    );
    SetCpuProjectionF(f_BestTimesWheelProjection, f_BestTimesWheelView, g_Viewports);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    courseCount = GetOpenCourseCount();
    if (courseCount <= 0) {
        courseCount = 1;
    }

    if (courseCount > COURSE_COUNT) {
        courseCount = COURSE_COUNT;
    }

    bestTimesCourse = memory->bestTimesCourse;
    spin = memory->bestTimesSpin;
    angleStep = DEGREES_PER_TURN / courseCount;
    for (index = 0; index != courseCount; index++) {
        ScreenPoint point;

        angle = -DegreesToRadians(
            (((bestTimesCourse - index) % courseCount) * angleStep) + (spin * DEGREES_PER_SPIN_STEP)
        );
        wheelX = sinf(angle) * WHEEL_RADIUS_X;
        if (ProjectPoint(wheelX, 0.0f, cosf(angle) * WHEEL_RADIUS_Z, &point) < 0) {
            iconX[index] = point.x;
            iconY[index] = point.y - WHEEL_RAISE;
            iconScale[index] = point.scaleX;
        }
    }

    for (otherIndex = 0; otherIndex != courseCount; otherIndex++) {
        drawOrder[otherIndex] = otherIndex;
    }

    // Draw the farthest images first, so the nearer ones cover them
    for (otherIndex = 1; otherIndex != courseCount; otherIndex++) {
        key = drawOrder[otherIndex];
        for (index = otherIndex - 1; index >= 0 && iconY[drawOrder[index]] > iconY[key]; index--) {
            drawOrder[index + 1] = drawOrder[index];
        }

        drawOrder[index + 1] = key;
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    for (index = 0; index != courseCount; index++) {
        drawnIndex = drawOrder[index];
        halfSize = iconScale[drawnIndex] * COURSE_ICON_HALF_SIZE;
        texelStep = (f32)(COURSE_ICON_STRIP_HEIGHT << TEXEL_STEP_SHIFT) / halfSize;
        gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_BestTimesCourseIcons[drawnIndex])));
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        for (otherIndex = 0; otherIndex != COURSE_ICON_STRIP_COUNT; otherIndex++) {
            gDPLoadTextureBlock(
                gfx++,
                TextureAddress(
                    LargeImageTexels(f_BestTimesCourseIcons[drawnIndex]) +
                    otherIndex * COURSE_ICON_WIDTH * COURSE_ICON_STRIP_HEIGHT
                ),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                COURSE_ICON_WIDTH,
                COURSE_ICON_STRIP_HEIGHT,
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
                QuarterPixels((s32)(iconX[drawnIndex] - halfSize)),
                QuarterPixels(
                    (s32)(iconY[drawnIndex] - halfSize + (otherIndex * halfSize)) - WHEEL_ICON_RAISE
                ),
                QuarterPixels((s32)(iconX[drawnIndex] + halfSize)),
                QuarterPixels(
                    (s32)(iconY[drawnIndex] + (otherIndex * halfSize)) - WHEEL_ICON_RAISE
                ),
                G_TX_RENDERTILE,
                0,
                0,
                texelStep,
                texelStep
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    if (courseCount >= 2) {
        DrawBlinkingSideArrows(
            &gfx,
            f_BestTimesGreenArrows,
            &f_BestTimesArrowFrame,
            &f_BestTimesArrowTimer,
            WHEEL_ARROW_WIDTH,
            WHEEL_ARROW_X,
            WHEEL_ARROW_Y
        );
    }

    DrawCourseBanner(&gfx, TRUE, memory->bestTimesCourse);
    gDPSetPrimColor(gfx++, 0, 0, 64, 64, 64, 191);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPPipeSync(gfx++);
    gSPTextureRectangle(
        gfx++,
        QuarterPixels(BEST_TIMES_PANEL_LEFT),
        QuarterPixels(BEST_TIMES_PANEL_TOP),
        QuarterPixels(BEST_TIMES_PANEL_RIGHT),
        QuarterPixels(BEST_TIMES_PANEL_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    for (otherIndex = 0; otherIndex != BEST_TIME_COUNT; otherIndex++) {
        DrawPlaceLabel(
            &gfx,
            BEST_TIME_PLACE_X,
            BEST_TIME_FIRST_PLACE_Y + (otherIndex * BEST_TIME_PLACE_SPACING),
            otherIndex + 1
        );
        time = save->records.bestTimes[memory->bestTimesCourse][otherIndex];
        if (time > 0) {
            DrawStopwatchTime(
                &gfx,
                BEST_TIME_X,
                BEST_TIME_FIRST_Y + (otherIndex * BEST_TIME_SPACING),
                time
            );
            DrawNarrowTwoDigitMenuNumber(
                &gfx,
                BEST_TIME_SCORE_X,
                BEST_TIME_FIRST_Y + (otherIndex * BEST_TIME_SPACING),
                save->records.bestTimeScoresToPar[memory->bestTimesCourse][otherIndex],
                MENU_NUMBER_SIGNED
            );
            gDPPipeSync(gfx++);
            gDPPipeSync(gfx++);
            gDPSetCycleType(gfx++, G_CYC_1CYCLE);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
            gDPPipeSync(gfx++);
            gDPSetTextureFilter(gfx++, G_TF_BILERP);
            // Read into a local of its own to match: indexing with
            // memory->bestTimesCourse gives 37 words differing (13 in shape), and
            // with the bestTimesCourse read at the top 217 (27)
            holderCourse = memory->bestTimesCourse;
            characterIndex = save->records.bestTimeCharacters[holderCourse][otherIndex];
            DrawCharacterFace(
                &gfx,
                g_CharacterFaceImages[characterIndex],
                g_MenuCharacterPalettes[(memory->bestTimesCourse * BEST_TIME_COUNT) + otherIndex],
                BEST_TIME_FACE_X,
                BEST_TIME_FIRST_FACE_Y + (otherIndex * BEST_TIME_SPACING),
                characterIndex,
                FACE_PLAIN
            );
        }
        else {
            for (dashIndex = 0; dashIndex != UNSET_TIME_DASH_COUNT; dashIndex++) {
                DrawMenuSign(
                    &gfx,
                    BEST_TIME_X + (dashIndex * UNSET_TIME_DASH_SPACING),
                    UNSET_TIME_FIRST_DASH_Y + (otherIndex * BEST_TIME_PLACE_SPACING),
                    MENU_SIGN_DASH
                );
            }
        }
    }

    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawBestTimeFace(
    Gfx **displayListHead,
    s32 left,
    s32 top,
    CharacterIndex characterIndex,
    u8 drawMode
) {
    Gfx *gfx = *displayListHead;

    if (characterIndex < 0) {
        characterIndex = 0;
    }

    if (characterIndex >= CHARACTER_COUNT) {
        characterIndex = CHARACTER_COUNT - 1;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (drawMode == FACE_DARKENED) {
        gDPSetCombineMode(gfx++, G_CC_MODULATERGBA_PRIM, G_CC_MODULATERGBA_PRIM);
        gDPSetPrimColor(
            gfx++,
            0,
            0,
            FACE_DARKENED_SHADE,
            FACE_DARKENED_SHADE,
            FACE_DARKENED_SHADE,
            OPAQUE_ALPHA
        );
    }
    else {
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    }

    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_CharacterFaceStrip)));
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(
            LargeImageTexels(f_CharacterFaceStrip) +
            (f_CharacterChoices[characterIndex] * FACE_WIDTH * FACE_HEIGHT)
        ),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        FACE_WIDTH,
        FACE_HEIGHT,
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
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + FACE_WIDTH),
        QuarterPixels(top + FACE_HEIGHT),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    if (drawMode == FACE_WITH_NAME) {
        SetUpTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        sprintf(g_PrintBuffer, "%s", g_MenuCharacterNames[f_CharacterChoices[characterIndex]]);
        DrawLargeText(&gfx, left + FACE_NAME_OFFSET_X, top + FACE_NAME_OFFSET_Y, g_PrintBuffer);
        gDPPipeSync(gfx++);
    }

    *displayListHead = gfx;
}

// Draws a place's label, such as 1st, at (left, top): its number and then its
// suffix, from the places image, with the place kept within the label's range.
// The suffixes' frames follow the digits'.
void DrawPlaceLabel(Gfx **displayListHead, s32 left, s32 top, s32 place) {
    Gfx *gfx = *displayListHead;

    if (place <= 0) {
        place = 1;
    }

    if (place > PLACE_LABEL_LAST_PLACE) {
        place = PLACE_LABEL_LAST_PLACE;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_PlaceLabels)));
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(LargeImageTexelsAt(f_PlaceLabels, (place - 1) * PLACE_PART_IMAGE_SIZE)),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PLACE_PART_SIZE,
        PLACE_PART_SIZE,
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
        QuarterPixels(left),
        QuarterPixels(top),
        QuarterPixels(left + PLACE_LABEL_SIZE),
        QuarterPixels(top + PLACE_LABEL_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(PLACE_PART_SIZE, PLACE_LABEL_SIZE),
        TexelRunStep(PLACE_PART_SIZE, PLACE_LABEL_SIZE)
    );
    gDPPipeSync(gfx++);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(LargeImageTexelsAt(
            f_PlaceLabels,
            ((place - 1) * PLACE_PART_IMAGE_SIZE) +
                (PLACE_NUMBER_PART_COUNT * PLACE_PART_IMAGE_SIZE)
        )),
        G_IM_FMT_CI,
        G_IM_SIZ_8b,
        PLACE_PART_SIZE,
        PLACE_PART_SIZE,
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
        QuarterPixels(left + PLACE_LABEL_SUFFIX_X),
        QuarterPixels(top),
        QuarterPixels(left + PLACE_LABEL_SUFFIX_X + PLACE_LABEL_SIZE),
        QuarterPixels(top + PLACE_LABEL_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TexelRunStep(PLACE_PART_SIZE, PLACE_LABEL_SIZE),
        TexelRunStep(PLACE_PART_SIZE, PLACE_LABEL_SIZE)
    );
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void DrawStopwatchTime(Gfx **displayListHead, s32 left, s32 top, s32 frameCount) {
    s32 digits[TIME_GROUP_COUNT][TIME_GROUP_DIGIT_COUNT];
    Gfx *gfx = *displayListHead;
    s32 hundredths = frameCount * HUNDREDTHS_PER_SECOND / FRAMES_PER_SECOND;
    s32 minutes = hundredths / HUNDREDTHS_PER_MINUTE;
    s32 seconds;
    u16 separatorShift;
    s16 group;
    s16 digit;

    hundredths %= HUNDREDTHS_PER_MINUTE;
    seconds = hundredths / HUNDREDTHS_PER_SECOND;
    hundredths %= HUNDREDTHS_PER_SECOND;
    digits[0][0] = minutes / DECIMAL_BASE;
    digits[0][1] = minutes % DECIMAL_BASE;
    digits[1][0] = seconds / DECIMAL_BASE;
    digits[1][1] = seconds % DECIMAL_BASE;
    digits[2][0] = hundredths / DECIMAL_BASE;
    digits[2][1] = hundredths % DECIMAL_BASE;
    separatorShift = 0;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal16(gfx++, 0, TextureAddress(ImagePalette(f_StopwatchGlyphs)));
    for (group = 0; group != TIME_GROUP_COUNT; group++) {
        for (digit = 0; digit != TIME_GROUP_DIGIT_COUNT; digit++) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    ImageTexels(f_StopwatchGlyphs) +
                    ((digits[group][digit] + TIME_FIRST_DIGIT_GLYPH) * TIME_GLYPH_BYTES)
                ),
                G_IM_FMT_CI,
                TIME_GLYPH_SIZE,
                TIME_GLYPH_SIZE,
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
                QuarterPixels(
                    separatorShift + left +
                    (((group * TIME_GROUP_DIGIT_COUNT) + digit) * TIME_DIGIT_SPACING)
                ),
                QuarterPixels(top),
                QuarterPixels(
                    separatorShift + left +
                    (((group * TIME_GROUP_DIGIT_COUNT) + digit) * TIME_DIGIT_SPACING) +
                    TIME_GLYPH_SIZE
                ),
                QuarterPixels(top + TIME_GLYPH_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }

        if (group != TIME_GROUP_COUNT - 1) {
            gDPLoadTextureBlock_4b(
                gfx++,
                TextureAddress(
                    (u32)f_StopwatchGlyphs + (TIME_SEPARATOR_GLYPH * TIME_GLYPH_BYTES) +
                    PALETTED_TEXELS_OFFSET
                ),
                G_IM_FMT_CI,
                TIME_GLYPH_SIZE,
                TIME_GLYPH_SIZE,
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
                QuarterPixels(
                    separatorShift + left +
                    (((group * TIME_GROUP_DIGIT_COUNT) + TIME_GROUP_DIGIT_COUNT) *
                        TIME_DIGIT_SPACING) -
                    TIME_SEPARATOR_SHIFT_LEFT
                ),
                QuarterPixels(top),
                QuarterPixels(
                    separatorShift + left +
                    (((group * TIME_GROUP_DIGIT_COUNT) + TIME_GROUP_DIGIT_COUNT) *
                        TIME_DIGIT_SPACING) -
                    TIME_SEPARATOR_SHIFT_LEFT + TIME_GLYPH_SIZE
                ),
                QuarterPixels(top + TIME_GLYPH_SIZE),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
            separatorShift += TIME_SEPARATOR_SPACING;
        }
    }

    *displayListHead = gfx;
}

// Reads every character's face, and the palette of the character and color
// that set each course's best times, or Plum's first for an unset time
void LoadBestTimesFaces(void) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save = GetSaveData();
    CharacterIndex characterIndex;
    CourseIndex courseIndex;
    s32 rank;
    CharacterIndex holderCharacterIndex;
    CButtonPalette paletteIndex;

    for (characterIndex = 0; characterIndex != CHARACTER_COUNT; characterIndex++) {
        OpenEncodedAsset(g_CharacterFaceAssets[characterIndex], &asset);
        ReadEncodedAsset(g_CharacterFaceImages[characterIndex], &asset);
    }

    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (rank = 0; rank != BEST_TIME_COUNT; rank++) {
            holderCharacterIndex = save->records.bestTimeCharacters[courseIndex][rank];
            if (holderCharacterIndex != NO_BEST_TIME_HOLDER) {
                if (holderCharacterIndex < 0) {
                    holderCharacterIndex = 0;
                }

                if (holderCharacterIndex >= CHARACTER_COUNT) {
                    holderCharacterIndex = CHARACTER_COUNT - 1;
                }

                paletteIndex = save->records.bestTimePalettes[courseIndex][rank];
            }
            else {
                holderCharacterIndex = 0;
                paletteIndex = UP_C_PALETTE;
            }

            OpenAssetFile(g_CharacterPaletteFiles[holderCharacterIndex][paletteIndex], &file);
            ReadAssetFile(
                g_MenuCharacterPalettes[(courseIndex * BEST_TIME_COUNT) + rank],
                GetAssetFileSize(&file),
                &file
            );
        }
    }
}
