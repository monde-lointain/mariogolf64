/*
 * The game's frame: updating the current scene and drawing it, with the screen
 * effects, the dynamic textures and the inset view, and loading and unloading
 * the overlays' modules.
 */

#include "game/game.h"

// The loader flag and the current shot are defined volatile, but KMC builds
// declare them plain for most files; see hole_loader.h and current_shot.h.
// This file reads both with the qualifier. DrawGame's loads of the flag match
// the original that way, and BuildHorizonDynamicTextures keeps its unused load
// of the current shot's player index, which flow would delete if it weren't
// volatile.
#define READS_LOADER_FLAG_VOLATILE
#define READS_CURRENT_SHOT_VOLATILE

#include <PR/libmus.h>
#include <nusys.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/camera.h"
#include "camera/shot_camera.h"
#include "camera/view_projection.h"
#include "characters/character_model.h"
#include "cinematics/boot_logos.h"
#include "cinematics/opening.h"
#include "cinematics/staff_roll.h"
#include "controllers/controller.h"
#include "course/billboard.h"
#include "course/hole_conditions.h"
#include "course/hole_creatures.h"
#include "course/hole_loader.h"
#include "course/hole_setup.h"
#include "course/hole_triangles.h"
#include "course/pin.h"
#include "course/terrain.h"
#include "course/work_area.h"
#include "debug/debug_text.h"
#include "debug/game_flags.h"
#include "debug/hole_editor.h"
#include "debug/performance_meter.h"
#include "game/scene.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "menus/confirm_wait.h"
#include "menus/main_menu.h"
#include "menus/title_screen.h"
#include "menus/transmit_menu.h"
#include "round/game_state.h"
#include "round/movie.h"
#include "round/pause_menu.h"
#include "round/play.h"
#include "round/round_events.h"
#include "round/round_hud.h"
#include "round/round_results.h"
#include "round/save_menu.h"
#include "round/scorecard.h"
#include "save/save.h"
#include "screen/animated_box.h"
#include "screen/display_list.h"
#include "screen/rcp_init.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "shot/current_shot.h"
#include "shot/golf_balls.h"
#include "shot/particle_effects.h"
#include "shot/shot_effects.h"
#include "shot/shot_labels.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/main.h"
#include "system/memory_address.h"
#include "system/overlay_manager.h"
#include "system/rom_arrays.h"
#include "text/text.h"

#define INSET_VIEW_SOUND_VOLUME 0x3F
#define INSET_VIEW_SOUND_REVERB 0x1E

// The audio test plays three sounds at once on these channels
#define AUDIO_TEST_FIRST_CHANNEL 3
#define AUDIO_TEST_SECOND_CHANNEL 4
#define AUDIO_TEST_THIRD_CHANNEL 11
#define AUDIO_TEST_VOLUME 0x60
#define SECOND_INSET_TEST_CONTROLLER 3
#define DEBUG_MENU_CONTROLLER 0
#define SCREEN_TEST_FADE_STEP 8

// Steps that the inset view takes to open or close fully
#define INSET_VIEW_STEP_COUNT 8

// Where SetInsetViewRect puts the inset view, which opens out from its
// center
#define INSET_VIEW_OPEN_SOUND_STEP 1
#define INSET_VIEW_CLOSE_SOUND_STEP (INSET_VIEW_STEP_COUNT - 1)

typedef struct InsetViewRectangle {
    s32 centerX;
    s32 centerY;
    s32 width;
    s32 height;
} InsetViewRectangle;

// Values of f_FadeColor
enum FadeColor {
    FADE_COLOR_NONE,
    FADE_COLOR_WHITE,
    FADE_COLOR_NOISE,
    FADE_COLOR_BLACK
};

typedef s32 FadeColor;

#define FADE_NOISE_GRAY 0xBF

// Values a debug menu edits; nothing reads them
#define DEBUG_MENU_VALUE_COUNT 3
#define DEBUG_MENU_VALUE_MAX 128
#define DEBUG_MENU_VALUE_START 127

// A viewport over the whole screen, in the RSP's quarter pixels
#define FULL_SCREEN_VIEWPORT \
    { \
        { \
            {SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, G_MAXZ / 2, 0}, \
                {SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, G_MAXZ / 2, 0} \
        } \
    }

// A viewport over the screen's bottom-right quarter
#define QUARTER_SCREEN_VIEWPORT \
    { \
        { \
            {SCREEN_WIDTH, SCREEN_HEIGHT, G_MAXZ / 2, 0}, \
                {SCREEN_WIDTH * 3, SCREEN_HEIGHT * 3, G_MAXZ / 2, 0} \
        } \
    }

// Palette loads that each read from an address inside the list itself
#define LOAD_LIST_PALETTE(list, index) gsDPLoadTLUT_pal256(&(list)[index])

PacingMode g_FramePacingMode = PACING_MODE_NORMAL;
static s32 f_FadeAlpha = 0;
TitleScreenEntrance g_TitleScreenEntrance = TITLE_ENTRANCE_NONE;
ScreenEffect g_ScreenEffect = SCREEN_EFFECT_NONE;
// The effect's length in frames; the tiled image counts it down
static s32 f_ScreenEffectFrameCount = 0;
// Holds a frame for the saved-frame effect, past the general pool
static u16 *f_SavedFrameBuffer = (u16 *)FixedAddress(MAIN_REGION_END);
// Set when the effects stop, never read
static s32 f_UnreadEffectSetting = 0;
static Radians f_SpinAngle = 0.0f; // In radians

// Read by no routine
static Gfx f_UnreadPaletteLoads[] = {
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 0),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 1),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 2),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 3),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 4),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 5),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 6),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 7),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 8),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 9),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 10),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 11),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 12),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 13),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 14),
    LOAD_LIST_PALETTE(f_UnreadPaletteLoads, 15),
    gsSPEndDisplayList(),
};

// The sources of g_FlameTexture
static u8 f_NoiseTexture[] RCP_ALIGNED = {
#include "assets/game/game/noise_texture.i8.inc.c"
};
DEFINE_ROM_ARRAY(noise_texture, f_NoiseTexture);
static u16 f_FlameGradientTexture[] RCP_ALIGNED = {
#include "assets/game/game/flame_gradient_texture.rgba16.inc.c"
};
DEFINE_ROM_ARRAY(flame_gradient_texture, f_FlameGradientTexture);

s32 g_HorizonRow = 0;
// How far BuildHorizonDynamicTextures has scrolled its textures
static s32 f_HorizonTextureScroll = 0;
// How far BuildDynamicTextures has scrolled its textures
static s32 f_DynamicTextureScroll = 0;
// When DrawGame last started, in microseconds
static u32 f_DrawStartTime = 0;
// When the last graphics task started, in microseconds
static u32 f_GfxTaskStartTime = 0;
// When UpdateGame last started and finished, in microseconds
static u32 f_UpdateStartTime = 0;
static u32 f_UpdateEndTime = 0;
static f32 f_MapFieldOfView = 60.0f; // In degrees
static Boolean8 f_NeedsIdentityMatrix = TRUE;
static FadeColor f_FadeColor = FADE_COLOR_WHITE;
// The frame CopyPreviousFrame copies, and the one shown last
static u16 *f_CopySourceFrame = NULL;
static u16 *f_PreviousFrame = NULL;
static s32 f_DebugMenuValues[DEBUG_MENU_VALUE_COUNT] = {
    DEBUG_MENU_VALUE_START,
    DEBUG_MENU_VALUE_START,
    DEBUG_MENU_VALUE_START,
};
static s32 f_DebugMenuValueIndex = 0;
static Boolean f_HasUnloadedSceneModule = FALSE;
static Boolean f_HasUnloadedSubModule = FALSE;
// The inset view's rectangle when fully open, in screen pixels
static InsetViewRectangle f_InsetViewRectangle = {0, 0, 0, 0};
s32 g_DrawBufferIndex = 0;

// Read by no routine
static Gfx f_UnreadEmptyList[] UNUSED = {
    gsSPEndDisplayList(),
};

Vp g_Viewports[VIEWPORT_COUNT] = {
    FULL_SCREEN_VIEWPORT,
    FULL_SCREEN_VIEWPORT,
    QUARTER_SCREEN_VIEWPORT,
    QUARTER_SCREEN_VIEWPORT,
    QUARTER_SCREEN_VIEWPORT,
    QUARTER_SCREEN_VIEWPORT,
};

// Resets the model view before each view's menu backdrop and scene
static Mtx f_IdentityMatrix;

// The spin effect's mesh and its rotation, one per draw buffer. Each draw
// buffer's vertices start one vertex after the other's, so most are shared,
// and f_SpinVertices runs on over f_SpinMatrices, so a frame's vertex updates
// overwrite parts of the matrices.
static Vtx f_SpinVertices[1];
static Mtx f_SpinMatrices[DRAW_BUFFER_COUNT];

static u8 f_UnusedBytesAfterSpinMatrices[16] UNUSED;

// How the saved-frame effect splits the screen into strips, in rows
#define SAVED_FRAME_STRIP_HEIGHT 6
#define SAVED_FRAME_STRIP_COUNT 38
#define SAVED_FRAME_LAST_STRIP_HEIGHT 2

// The spin effect's mesh holds two vertices per row, loaded in batches of
// up to SPIN_BATCH_VERTEX_COUNT
#define SPIN_BATCH_COUNT 3
#define SPIN_BATCH_VERTEX_COUNT 32
#define SPIN_LAST_BATCH_VERTEX_COUNT 18
#define SPIN_LAST_BATCH_STRIP_COUNT 8
// The mesh's model is scaled down by SPIN_MESH_SCALE, so it spans the screen
// inside its border at SPIN_MESH_DISTANCE from the camera
#define SPIN_MESH_ROW_COUNT 42
#define SPIN_MESH_HALF_WIDTH (((SCREEN_WIDTH / 2) - SCREEN_BORDER) * 10)
#define SPIN_MESH_SCALE 0.1f
#define SPIN_MESH_DISTANCE (-290.0f)
#define IsLastSpinStrip(batchIndex, stripIndex) \
    (((batchIndex) == SPIN_BATCH_COUNT - 1) & ((stripIndex) == SPIN_LAST_BATCH_STRIP_COUNT - 1))

#define RESET_FADE_ALPHA 0x40

// Over the fly-over's last frames the fade rises, and the scene spins away
// if no fade color is set; once the fly-over has ended, the fade lifts
#define FLYOVER_FADE_FRAME_COUNT 15
#define FLYOVER_FADE_START_FRAME (HOLE_FLYOVER_FRAME_COUNT - FLYOVER_FADE_FRAME_COUNT)
#define FLYOVER_FADE_ALPHA_STEP 16
#define FLYOVER_FADE_LIFT_ALPHA_STEP 32
#define FLYOVER_SPIN_FRAME_COUNT 30
// How long the screen test's effects last, from the second controller
#define SCREEN_TEST_SPIN_FRAME_COUNT 16
#define SCREEN_TEST_FRAME_COUNT 64

// In SCENE_GROUND_CAMERA, the C buttons move the map camera, the stick turns
// and raises it, and it looks ahead along the ground
#define MAP_CAMERA_STEP 10.0f
#define MAP_CAMERA_HEIGHT 120.0f
#define MAP_CAMERA_LIFT_PER_STICK 32
#define MAP_CAMERA_TURN_PER_STICK 1024.0
#define MAP_CAMERA_LOOK_DISTANCE 1000.0f

// GetGroundHeight's units per world unit
#define GROUND_UNITS_PER_UNIT 1024

// SCENE_MENU opens a larger, square inset view, in more steps and without
// sounds. Each step adds this many pixels to its sides.
#define LARGE_INSET_VIEW_STEP_COUNT 32
#define LARGE_INSET_VIEW_GROWTH_PER_STEP 4
#define LARGE_INSET_VIEW_CENTER_X (SCREEN_WIDTH / 2)
#define LARGE_INSET_VIEW_CENTER_Y 140

// Where the debug menu draws its values, in text cells
#define DEBUG_MENU_COLUMN 34
#define DEBUG_MENU_CURSOR_COLUMN 33
#define DEBUG_MENU_ROW 3

// BuildDynamicTextures shrinks a strip of the frame into g_MirrorTexture
// by copying blocks of MIRROR_BLOCK_WIDTH pixels, spaced out across it,
// from every MIRROR_ROW_SPACING-th row
#define MIRROR_BLOCK_COUNT 4
#define MIRROR_BLOCK_WIDTH (DYNAMIC_TEXTURE_SIZE / MIRROR_BLOCK_COUNT)
#define MIRROR_SOURCE_LEFT 34
#define MIRROR_SOURCE_TOP 48
#define MIRROR_BLOCK_SPACING 76
#define MIRROR_ROW_SPACING 3

// How strongly each layer covers what is under it
#define MIRROR_TINT_ALPHA 0xC8
#define MIRROR_NOISE_ALPHA 0x9F
#define FLAME_NOISE_ALPHA 0x30
#define BLENDED_TEXTURE_ALPHA 0x7F

// BuildHorizonDynamicTextures copies every HORIZON_ROW_SPACING-th row of
// a strip of the frame that starts HORIZON_STRIP_RISE rows above the
// horizon and ends at HORIZON_STRIP_BOTTOM
#define HORIZON_STRIP_RISE 70
#define HORIZON_FIRST_ROW 8
#define HORIZON_ROW_SPACING 7
#define HORIZON_STRIP_BOTTOM 224

// The window of the saved image that the tiled-image effect repeats
#define TILED_IMAGE_LEFT 64
#define TILED_IMAGE_RIGHT 191
#define TILED_IMAGE_TOP 104
#define TILED_IMAGE_STRIP_HEIGHT 16
#define TILED_IMAGE_STRIP_COUNT 6
// Wraps s every 2^7 = 128 texels, the window's width
#define TILED_IMAGE_MASK_S 7

// How the tiled-image effect lays the window out on the screen
#define TILED_IMAGE_STRIP_SCREEN_HEIGHT 13
#define TILED_IMAGE_COPY_COUNT 4
#define TILED_IMAGE_COPY_SPACING 72
// Texels per pixel, s5.10: about 1.23
#define TILED_IMAGE_SCALE 0x04EC

// Turns gamma correction off and gamma dithering and the dither filter on
#define VI_FEATURES (OS_VI_GAMMA_OFF | OS_VI_GAMMA_DITHER_ON | OS_VI_DITHER_FILTER_ON)

// Indexed by view, then by draw buffer
FILE_STATIC CameraMatrices f_CameraMatrices[CAMERA_COUNT * DRAW_BUFFER_COUNT];
Camera g_MapCamera;
u16 g_BlendedTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] RCP_ALIGNED;
u8 *g_BlendedTextureScrollingLayer;
u8 *g_BlendedTextureStillLayer;
Camera g_Cameras[CAMERA_COUNT];
Gfx *g_DisplayListHead;
u16 g_FlameTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] RCP_ALIGNED;
ScreenRectangle g_HudScissor;
InsetView g_InsetView;
u16 g_MirrorTexture[DYNAMIC_TEXTURE_SIZE * DYNAMIC_TEXTURE_SIZE] RCP_ALIGNED;
u16 g_ScreenEffectPerspectiveNormalization;
Mtx g_ScreenEffectProjection;
Mtx g_ScreenOrthoMatrix;

FILE_STATIC s32 f_PerspectiveNormalization;
// Set up by StartGame; no code in the main segment draws with it. A view
// alone rather than a Camera: nothing reaches the four bytes before it, and
// libc's errno follows it where a Camera's mode would be.
FILE_STATIC CameraView f_SpareCameraView;
FILE_STATIC Gfx f_DisplayLists[2][DISPLAY_LIST_LENGTH];

// Where StartGame places the spare view and the inset camera's view
#define SPARE_VIEW_LEFT 128
#define SPARE_VIEW_TOP 64
#define SPARE_VIEW_WIDTH (SCREEN_WIDTH / 2)
#define SPARE_VIEW_HEIGHT (SCREEN_HEIGHT / 2)
#define START_INSET_LEFT 64
#define START_INSET_TOP 96
#define START_INSET_WIDTH 128
#define START_INSET_HEIGHT 64

void StartGame(void) {
    g_MapCamera.view.eye[AXIS_X] = 0.0f;
    g_MapCamera.view.eye[AXIS_Y] = 0.0f;
    g_MapCamera.view.eye[AXIS_Z] = 0.0f;
    g_MapCamera.view.lookAt[AXIS_X] = 0.0f;
    g_MapCamera.view.lookAt[AXIS_Y] = 0.0f;
    g_MapCamera.view.lookAt[AXIS_Z] = 1.0f;
    g_MapCamera.view.heading = 0.0f;
    g_MapCamera.view.scissorX = SCREEN_BORDER;
    g_MapCamera.view.scissorY = SCREEN_BORDER;
    g_MapCamera.view.scissorWidth = SCREEN_WIDTH - (2 * SCREEN_BORDER);
    g_MapCamera.view.scissorHeight = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_MapCamera.view.unreadSettings[0] = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_MapCamera.view.unreadSettings[1] = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_MapCamera.view.viewportCenterX = SCREEN_WIDTH;
    g_MapCamera.view.viewportCenterY = SCREEN_HEIGHT;
    g_MapCamera.view.zoom = 1.0f;
    f_SpareCameraView.eye[AXIS_X] = 0.0f;
    f_SpareCameraView.eye[AXIS_Y] = 0.0f;
    f_SpareCameraView.eye[AXIS_Z] = 0.0f;
    f_SpareCameraView.lookAt[AXIS_X] = 0.0f;
    f_SpareCameraView.lookAt[AXIS_Y] = 0.0f;
    f_SpareCameraView.lookAt[AXIS_Z] = 1.0f;
    f_SpareCameraView.heading = 0.0f;
    f_SpareCameraView.scissorX = SPARE_VIEW_LEFT;
    f_SpareCameraView.scissorY = SPARE_VIEW_TOP;
    f_SpareCameraView.scissorWidth = SPARE_VIEW_WIDTH;
    f_SpareCameraView.scissorHeight = SPARE_VIEW_HEIGHT;
    f_SpareCameraView.unreadSettings[0] = 0;
    f_SpareCameraView.unreadSettings[1] = 30;
    f_SpareCameraView.viewportCenterX = ViewportCenter(SPARE_VIEW_LEFT, SPARE_VIEW_WIDTH);
    f_SpareCameraView.viewportCenterY = ViewportCenter(SPARE_VIEW_TOP, SPARE_VIEW_HEIGHT);
    f_SpareCameraView.zoom = 0.5f;
    g_Cameras[MAIN_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = 0.0f;
    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = -100.0f;
    g_Cameras[MAIN_CAMERA].view.scissorX = SCREEN_BORDER;
    g_Cameras[MAIN_CAMERA].view.scissorY = SCREEN_BORDER;
    g_Cameras[MAIN_CAMERA].view.scissorWidth = SCREEN_WIDTH - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.scissorHeight = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.unreadSettings[0] = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.unreadSettings[1] = SCREEN_HEIGHT - (2 * SCREEN_BORDER);
    g_Cameras[MAIN_CAMERA].view.zoom = 1.0f;
    g_Cameras[MAIN_CAMERA].view.viewportCenterX = SCREEN_WIDTH;
    g_Cameras[MAIN_CAMERA].view.viewportCenterY = SCREEN_HEIGHT;
    g_Cameras[MAIN_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
    g_Cameras[MAIN_CAMERA].view.fieldOfView = 45.0f;
    g_Cameras[INSET_CAMERA].mode = CAMERA_MODE_BEHIND_GOLFER;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_X] = 0.0f;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Y] = 0.0f;
    g_Cameras[INSET_CAMERA].view.eye[AXIS_Z] = 0.0f;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_X] = 0.0f;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Y] = 0.0f;
    g_Cameras[INSET_CAMERA].view.lookAt[AXIS_Z] = -100.0f;
    g_Cameras[INSET_CAMERA].view.scissorX = START_INSET_LEFT;
    g_Cameras[INSET_CAMERA].view.scissorY = START_INSET_TOP;
    g_Cameras[INSET_CAMERA].view.scissorWidth = START_INSET_WIDTH;
    g_Cameras[INSET_CAMERA].view.scissorHeight = START_INSET_HEIGHT;
    g_Cameras[INSET_CAMERA].view.unreadSettings[0] = 0;
    g_Cameras[INSET_CAMERA].view.unreadSettings[1] = 0;
    g_Cameras[INSET_CAMERA].view.zoom = 0.9f;
    g_Cameras[INSET_CAMERA].view.viewportCenterX =
        ViewportCenter(START_INSET_LEFT, START_INSET_WIDTH);
    g_Cameras[INSET_CAMERA].view.viewportCenterY =
        ViewportCenter(START_INSET_TOP, START_INSET_HEIGHT);
    g_Cameras[INSET_CAMERA].view.drawFlags = 0;
    g_Cameras[INSET_CAMERA].view.fieldOfView = 60.0f;
    osViSetSpecialFeatures(VI_FEATURES);
    ClearDebugText();
    LoadTextAssets();
    LoadRoundHud();
    LoadPinTextures();
}

void TranslateMatrix(Mtx *matrix, f32 x, f32 y, f32 z) {
    Mtx translation;

    guTranslate(&translation, x, y, z);
    guMtxCatL(matrix, &translation, matrix);
}

void ScaleMatrix(Mtx *matrix, f32 x, f32 y, f32 z) {
    Mtx scale;

    guScale(&scale, x, y, z);
    guMtxCatL(matrix, &scale, matrix);
}

void SetScreenEffect(ScreenEffect effect, s32 frameCount) {
    g_ScreenEffect = effect;
    f_ScreenEffectFrameCount = frameCount;
    if (effect == SCREEN_EFFECT_NONE) {
        g_Cameras[MAIN_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
        f_UnreadEffectSetting = -640;
    }
    else {
        g_Cameras[MAIN_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT;
    }
}

// Draws a 128 by 96 window of the image saved in the depth buffer, shrunk
// and repeated across the screen, for a number of frames
void DrawTiledImageEffect(Gfx **displayListHead, CameraView *view) {
    Gfx *gfx = *displayListHead;
    s32 stripTop;
    s32 texelPairOffset;
    s32 copyIndex;

    f_ScreenEffectFrameCount--;
    if (f_ScreenEffectFrameCount == 0) {
        view->drawFlags = CAMERA_DRAW_SCENE;
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
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
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    texelPairOffset = TILED_IMAGE_TOP * SCREEN_WIDTH / 2;
    for (stripTop = 0; stripTop != TILED_IMAGE_STRIP_COUNT * TILED_IMAGE_STRIP_SCREEN_HEIGHT;
        stripTop += TILED_IMAGE_STRIP_SCREEN_HEIGHT) {
        // Each 32-bit step of texelPairOffset covers two 16-bit texels
        gDPLoadTextureTile(
            gfx++,
            TextureAddress((u32 *)g_DepthBuffer + texelPairOffset),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            SCREEN_WIDTH,
            0,
            TILED_IMAGE_LEFT,
            0,
            TILED_IMAGE_RIGHT,
            TILED_IMAGE_STRIP_HEIGHT - 1,
            0,
            G_TX_NOMIRROR | G_TX_WRAP,
            G_TX_NOMIRROR | G_TX_WRAP,
            TILED_IMAGE_MASK_S,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        texelPairOffset += TILED_IMAGE_STRIP_HEIGHT * SCREEN_WIDTH / 2;
        for (copyIndex = 0; copyIndex != TILED_IMAGE_COPY_COUNT; copyIndex++) {
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(SCREEN_BORDER),
                QuarterPixels(SCREEN_BORDER + (copyIndex * TILED_IMAGE_COPY_SPACING) + stripTop),
                QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER),
                QuarterPixels(
                    SCREEN_BORDER + (copyIndex * TILED_IMAGE_COPY_SPACING) + stripTop +
                    TILED_IMAGE_STRIP_SCREEN_HEIGHT
                ),
                G_TX_RENDERTILE,
                TexelCoordinate(TILED_IMAGE_LEFT),
                0,
                TILED_IMAGE_SCALE,
                TILED_IMAGE_SCALE
            );
        }
    }

    *displayListHead = gfx;
}

// Draws the frame saved in the depth buffer on the spin effect's mesh: one
// strip of the frame between each pair of rows of vertices
void DrawSpinMesh(Gfx **displayListHead) {
    Boolean isTileSetUp = FALSE;
    Gfx *gfx = *displayListHead;
    s32 texelPairOffset = SCREEN_BORDER * SCREEN_WIDTH / 2;
    s32 batchIndex;
    s32 stripIndex;
    s32 stripHeight;
    UBoolean8 isLastStrip;

    gSPMatrix(gfx++, &g_ScreenEffectProjection, G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPMatrix(gfx++, &f_SpinMatrices[g_DrawBufferIndex], G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_PUSH);
    for (batchIndex = 0; batchIndex != SPIN_BATCH_COUNT; batchIndex++) {
        if (batchIndex == SPIN_BATCH_COUNT - 1) {
            gSPVertex(
                gfx++,
                &f_SpinVertices[g_DrawBufferIndex] + (batchIndex * SPIN_BATCH_VERTEX_COUNT),
                SPIN_LAST_BATCH_VERTEX_COUNT,
                0
            );
        }
        else {
            gSPVertex(
                gfx++,
                &f_SpinVertices[g_DrawBufferIndex] + (batchIndex * SPIN_BATCH_VERTEX_COUNT),
                SPIN_BATCH_VERTEX_COUNT,
                0
            );
        }

        for (stripIndex = 0; stripIndex != (SPIN_BATCH_VERTEX_COUNT / 2) - 1; stripIndex++) {
            isLastStrip = IsLastSpinStrip(batchIndex, stripIndex);
            stripHeight = isLastStrip ? SAVED_FRAME_LAST_STRIP_HEIGHT : SAVED_FRAME_STRIP_HEIGHT;
            if (!isTileSetUp) {
                gDPLoadTextureBlock(
                    gfx++,
                    TextureAddress((u32 *)g_DepthBuffer + texelPairOffset),
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    SCREEN_WIDTH,
                    stripHeight,
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                isTileSetUp = TRUE;
            }
            else {
                gDPSetTextureImage(
                    gfx++,
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    1,
                    TextureAddress((u32 *)g_DepthBuffer + texelPairOffset)
                );
                gDPLoadSync(gfx++);
                gDPLoadBlock(
                    gfx++,
                    G_TX_LOADTILE,
                    0,
                    0,
                    (SCREEN_WIDTH * stripHeight) - 1,
                    CALC_DXT(SCREEN_WIDTH, G_IM_SIZ_16b_BYTES)
                );
                gDPPipeSync(gfx++);
            }

            gDPLoadTextureTile(
                gfx++,
                TextureAddress((u32 *)g_DepthBuffer + texelPairOffset),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SCREEN_WIDTH,
                0,
                SCREEN_BORDER,
                0,
                SCREEN_WIDTH - SCREEN_BORDER - 1,
                stripHeight - 1,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            // The original tests again here; the build depends on it
            isLastStrip = IsLastSpinStrip(batchIndex, stripIndex);
            stripHeight = isLastStrip ? SAVED_FRAME_LAST_STRIP_HEIGHT : SAVED_FRAME_STRIP_HEIGHT;
            gSPModifyVertex(
                gfx++,
                stripIndex * 2,
                G_MWO_POINT_ST,
                TextureCoordinateWord(TexelCoordinate(SCREEN_BORDER), 0)
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 1,
                G_MWO_POINT_ST,
                TextureCoordinateWord(TexelCoordinate(SCREEN_WIDTH - SCREEN_BORDER), 0)
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 2,
                G_MWO_POINT_ST,
                TextureCoordinateWord(TexelCoordinate(SCREEN_BORDER), TexelCoordinate(stripHeight))
            );
            gSPModifyVertex(
                gfx++,
                (stripIndex * 2) + 3,
                G_MWO_POINT_ST,
                TextureCoordinateWord(
                    TexelCoordinate(SCREEN_WIDTH - SCREEN_BORDER),
                    TexelCoordinate(stripHeight)
                )
            );
            gSP2Triangles(
                gfx++,
                stripIndex * 2,
                (stripIndex * 2) + 2,
                (stripIndex * 2) + 1,
                0,
                (stripIndex * 2) + 1,
                (stripIndex * 2) + 2,
                (stripIndex * 2) + 3,
                0
            );
            gDPPipeSync(gfx++);
            if (isLastStrip) {
                break;
            }

            texelPairOffset += SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH / 2;
        }
    }

    *displayListHead = gfx;
}

// Copies the frame kept in f_SavedFrameBuffer to the screen, through the
// depth buffer's memory, a strip at a time in copy mode
void DrawSavedFrameEffect(Gfx **displayListHead, CameraView *view UNUSED) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s16 stripHeight;

    f_ScreenEffectFrameCount = SAVED_FRAME_STEP_SHOWING;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        TextureAddress(nuGfxZBuffer)
    );
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != SAVED_FRAME_STRIP_COUNT; stripIndex++) {
        stripHeight = (stripIndex == SAVED_FRAME_STRIP_COUNT - 1) ? SAVED_FRAME_LAST_STRIP_HEIGHT :
                                                                    SAVED_FRAME_STRIP_HEIGHT;
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(
                f_SavedFrameBuffer + (stripIndex * SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH)
            ),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            SCREEN_WIDTH,
            0,
            SCREEN_BORDER,
            SCREEN_BORDER,
            SCREEN_WIDTH - SCREEN_BORDER - 1,
            SCREEN_BORDER + stripHeight - 1,
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
            QuarterPixels(SCREEN_BORDER + (stripIndex * SAVED_FRAME_STRIP_HEIGHT)),
            QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER - 1),
            QuarterPixels(
                (stripIndex * SAVED_FRAME_STRIP_HEIGHT) + SCREEN_BORDER + stripHeight - 1
            ),
            G_TX_RENDERTILE,
            TexelCoordinate(SCREEN_BORDER),
            TexelCoordinate(SCREEN_BORDER),
            COPY_MODE_TEXEL_STEP,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != SAVED_FRAME_STRIP_COUNT; stripIndex++) {
        stripHeight = (stripIndex == SAVED_FRAME_STRIP_COUNT - 1) ? SAVED_FRAME_LAST_STRIP_HEIGHT :
                                                                    SAVED_FRAME_STRIP_HEIGHT;
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(g_DepthBuffer + (stripIndex * SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH)),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            SCREEN_WIDTH,
            0,
            SCREEN_BORDER,
            SCREEN_BORDER,
            SCREEN_WIDTH - SCREEN_BORDER - 1,
            SCREEN_BORDER + stripHeight - 1,
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
            QuarterPixels(SCREEN_BORDER + (stripIndex * SAVED_FRAME_STRIP_HEIGHT)),
            QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER - 1),
            QuarterPixels(
                (stripIndex * SAVED_FRAME_STRIP_HEIGHT) + SCREEN_BORDER + stripHeight - 1
            ),
            G_TX_RENDERTILE,
            TexelCoordinate(SCREEN_BORDER),
            TexelCoordinate(SCREEN_BORDER),
            COPY_MODE_TEXEL_STEP,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Turns the frame saved in the depth buffer about the vertical axis, a half
// turn over f_ScreenEffectFrameCount frames, then shows the scene again
void DrawSpinEffect(Gfx **displayListHead, CameraView *view) {
    Mat4 transform;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    Gfx *gfx = *displayListHead;
    Radians angleStep;
    Vtx *vertex;
    s32 row;

    rotation[AXIS_X] = 0.0f;
    rotation[AXIS_Y] = f_SpinAngle;
    rotation[AXIS_Z] = 0.0f;
    translation[AXIS_X] = 0.0f;
    translation[AXIS_Y] = 0.0f;
    translation[AXIS_Z] = SPIN_MESH_DISTANCE;
    scale[AXIS_X] = SPIN_MESH_SCALE;
    scale[AXIS_Y] = SPIN_MESH_SCALE;
    scale[AXIS_Z] = SPIN_MESH_SCALE;
    BuildTransformMatrix(transform, rotation, translation, scale);
    ConvertMatrixToFixed(transform, &f_SpinMatrices[g_DrawBufferIndex]);

    angleStep = M_PI / f_ScreenEffectFrameCount;
    f_SpinAngle += angleStep;

    // Skips the half turn in which the frame's back would face the camera
    if (f_SpinAngle > M_PI / 2 && f_SpinAngle < angleStep + (M_PI / 2)) {
        f_SpinAngle += M_PI;
    }

    if (f_SpinAngle > FULL_TURN_RADIANS) {
        f_SpinAngle = 0.0f;
        view->drawFlags = CAMERA_DRAW_SCENE;
    }

    vertex = &f_SpinVertices[g_DrawBufferIndex];
    for (row = 0; row != SPIN_MESH_ROW_COUNT; row++) {
        vertex[0].v.ob[AXIS_X] = -SPIN_MESH_HALF_WIDTH;
        vertex[1].v.ob[AXIS_X] = SPIN_MESH_HALF_WIDTH;
        vertex += 2;
    }

    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, FillColor16(0, 0, 0, 1));
    gDPFillRectangle(gfx++, 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetScissor(
        gfx++,
        G_SC_NON_INTERLACE,
        view->scissorX,
        view->scissorY,
        view->scissorX + view->scissorWidth,
        view->scissorY + view->scissorHeight
    );
    gSPClearGeometryMode(gfx++, ALL_GEOMETRY_MODES);
    gSPSetGeometryMode(gfx++, G_SHADE | G_CULL_BACK | G_SHADING_SMOOTH);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_OPA_SURF, G_RM_AA_OPA_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    gDPSetCombineMode(gfx++, G_CC_MODULATEI_PRIM, G_CC_MODULATEI_PRIM);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPPipeSync(gfx++);
    DrawSpinMesh(&gfx);
    *displayListHead = gfx;
}

void DrawEmptyEffect(Gfx **displayListHead UNUSED, CameraView *view UNUSED) {
}

void DrawScreenEffect(CameraMatrices *matrices, CameraView *view) {
    // The spin effect pushes a modelview matrix and changes the projection
    if (g_ScreenEffect == SCREEN_EFFECT_SPIN) {
        DrawSpinEffect(&g_DisplayListHead, view);
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gSPMatrix(
            g_DisplayListHead++,
            &matrices->projection,
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
        );
    }
    else if (g_ScreenEffect == SCREEN_EFFECT_TILED_IMAGE) {
        DrawTiledImageEffect(&g_DisplayListHead, view);
    }
    else if (g_ScreenEffect == SCREEN_EFFECT_EMPTY) {
        DrawEmptyEffect(&g_DisplayListHead, view);

        // Pops although DrawEmptyEffect pushed nothing, as if it once spun too
        gSPPopMatrix(g_DisplayListHead++, G_MTX_MODELVIEW);
        gSPMatrix(
            g_DisplayListHead++,
            &matrices->projection,
            G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
        );
    }
    else if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
        DrawSavedFrameEffect(&g_DisplayListHead, view);
    }
}

// Draws the dynamic textures, filling g_MirrorTexture from a strip of the
// frame above the horizon. Only SCENE_PLAY_ROUND and SCENE_MENU
// use it.
void BuildHorizonDynamicTextures(Gfx **displayListHead) {
    s32 stripTop;
    s32 sourceTop;
    s32 destinationOffset;
    s32 rowCount;
    Gfx *gfx = *displayListHead;
    UnsignedAddress frame = osVirtualToPhysical(nuGfxCfb_ptr);
    GameStateStep gameStep;
    s32 row;
    s32 blockIndex;

    // Keeps the strip's rows on the screen
    void FitStripToScreen(void) {
        if (stripTop < 0) {
            sourceTop = 0;
        }
        else {
            sourceTop = stripTop;
        }

        if (stripTop > 0) {
            destinationOffset = 0;
        }
        else {
            destinationOffset = -stripTop / HORIZON_ROW_SPACING;
        }

        if (sourceTop > 0) {
            rowCount = (HORIZON_STRIP_BOTTOM - sourceTop) / HORIZON_ROW_SPACING;
        }
        else {
            rowCount = DYNAMIC_TEXTURE_SIZE - destinationOffset;
        }
    }

    if ((g_Scene != SCENE_PLAY_ROUND) & (g_Scene != SCENE_MENU)) {
        return;
    }

    stripTop = g_HorizonRow - HORIZON_STRIP_RISE;
    gameStep = g_GameState.step;
    if (IsHoleOverviewStep(gameStep)) {
        sourceTop = 0;
        destinationOffset = 0;
        rowCount = DYNAMIC_TEXTURE_SIZE;
    }
    else {
        // The value read here is never used, but the original loads it, as
        // g_CurrentShot's volatile declaration makes it. A plain read without
        // that gives 7 words differing, 5 in shape, and no read or an empty if
        // 11 (7)
        if (gameStep == GAME_STATE_TAKING_TURNS) {
            gameStep = g_CurrentShot.playerIndex;
        }

        FitStripToScreen();
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(gfx++, TEXEL0, 0, TEXEL0, 0, 0, 0, 0, 1, TEXEL0, 0, TEXEL0, 0, 0, 0, 0, 1);
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_TINT_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        RenderTargetAddress(osVirtualToPhysical(g_MirrorTexture))
    );
    gDPPipeSync(gfx++);
    if (g_Cameras[INSET_CAMERA].view.drawFlags != CAMERA_DRAW_SCENE &&
        g_PauseScreen != PAUSE_SCREEN_FREE_LOOK &&
        sourceTop <= HORIZON_STRIP_BOTTOM &&
        destinationOffset < DYNAMIC_TEXTURE_SIZE) {
        for (row = 0; row != rowCount; row++) {
            for (blockIndex = 0; blockIndex != MIRROR_BLOCK_COUNT; blockIndex++) {
                gDPLoadTextureTile(
                    gfx++,
                    TextureAddress(frame),
                    G_IM_FMT_RGBA,
                    G_IM_SIZ_16b,
                    SCREEN_WIDTH,
                    0,
                    MIRROR_SOURCE_LEFT + (blockIndex * MIRROR_BLOCK_SPACING),
                    sourceTop + HORIZON_FIRST_ROW + (row * HORIZON_ROW_SPACING),
                    MIRROR_SOURCE_LEFT + (blockIndex * MIRROR_BLOCK_SPACING) + MIRROR_BLOCK_WIDTH -
                        1,
                    sourceTop + HORIZON_FIRST_ROW + (row * HORIZON_ROW_SPACING),
                    0,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMIRROR | G_TX_WRAP,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                gSPScisTextureRectangle(
                    gfx++,
                    QuarterPixels(blockIndex * MIRROR_BLOCK_WIDTH),
                    QuarterPixels(DYNAMIC_TEXTURE_SIZE - 1 - row - destinationOffset),
                    QuarterPixels((blockIndex * MIRROR_BLOCK_WIDTH) + MIRROR_BLOCK_WIDTH),
                    QuarterPixels(DYNAMIC_TEXTURE_SIZE - 1 - row - destinationOffset + 1),
                    G_TX_RENDERTILE,
                    TexelCoordinate(MIRROR_SOURCE_LEFT + (blockIndex * MIRROR_BLOCK_SPACING)),
                    TexelCoordinate(sourceTop + HORIZON_FIRST_ROW + (row * HORIZON_ROW_SPACING)),
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
        }
    }

    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        TEXEL1,
        PRIMITIVE_ALPHA,
        TEXEL1,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        COMBINED,
        0,
        0,
        0,
        0,
        COMBINED
    );
    f_HorizonTextureScroll++;
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(f_NoiseTexture),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(g_MirrorTexture),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_NOISE_ALPHA);
    if (rowCount > 0) {
        gSPScisTextureRectangle(
            gfx++,
            0,
            QuarterPixels(DYNAMIC_TEXTURE_SIZE - destinationOffset - rowCount),
            QuarterPixels(DYNAMIC_TEXTURE_SIZE),
            QuarterPixels(DYNAMIC_TEXTURE_SIZE - destinationOffset),
            G_TX_RENDERTILE,
            0,
            TexelCoordinate(DYNAMIC_TEXTURE_SIZE - destinationOffset - rowCount),
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
    }

    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        TEXEL1,
        PRIMITIVE_ALPHA,
        TEXEL1,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        COMBINED,
        0,
        0,
        0,
        0,
        COMBINED
    );
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        RenderTargetAddress(osVirtualToPhysical(g_FlameTexture))
    );
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(f_NoiseTexture),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(f_FlameGradientTexture),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, FLAME_NOISE_ALPHA);
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        f_HorizonTextureScroll,
        32,
        f_HorizonTextureScroll + 32
    );
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(
        gfx++,
        0,
        0,
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        RenderTargetAddress(osVirtualToPhysical(g_BlendedTexture))
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, BLENDED_TEXTURE_ALPHA);
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureScrollingLayer)),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureStillLayer)),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        f_HorizonTextureScroll,
        f_HorizonTextureScroll,
        f_HorizonTextureScroll + 32,
        f_HorizonTextureScroll + 32
    );
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(
        gfx++,
        0,
        0,
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );
    gDPPipeSync(gfx++);
    gDPSetScissor(
        gfx++,
        G_SC_NON_INTERLACE,
        SCREEN_BORDER,
        SCREEN_BORDER,
        SCREEN_WIDTH - SCREEN_BORDER,
        SCREEN_HEIGHT - SCREEN_BORDER
    );
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Draws the dynamic textures from a fixed strip of the frame. Restores
// the frame as the color image when done.
void BuildDynamicTextures(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    UnsignedAddress frame = osVirtualToPhysical(nuGfxCfb_ptr);
    s32 row;
    s32 blockIndex;

    f_DynamicTextureScroll++;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        PRIMITIVE,
        PRIMITIVE_ALPHA,
        PRIMITIVE,
        0,
        0,
        0,
        1,
        TEXEL0,
        PRIMITIVE,
        PRIMITIVE_ALPHA,
        PRIMITIVE,
        0,
        0,
        0,
        1
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_TINT_ALPHA);
    gDPPipeSync(gfx++);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        RenderTargetAddress(osVirtualToPhysical(g_MirrorTexture))
    );
    gDPPipeSync(gfx++);
    for (row = 0; row != DYNAMIC_TEXTURE_SIZE; row++) {
        for (blockIndex = 0; blockIndex != MIRROR_BLOCK_COUNT; blockIndex++) {
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(frame),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SCREEN_WIDTH,
                0,
                MIRROR_SOURCE_LEFT + (blockIndex * MIRROR_BLOCK_SPACING),
                MIRROR_SOURCE_TOP + (row * MIRROR_ROW_SPACING),
                MIRROR_SOURCE_LEFT + (blockIndex * MIRROR_BLOCK_SPACING) + MIRROR_BLOCK_WIDTH - 1,
                MIRROR_SOURCE_TOP + (row * MIRROR_ROW_SPACING) + MIRROR_ROW_SPACING,
                0,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMIRROR | G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels(blockIndex * MIRROR_BLOCK_WIDTH),
                QuarterPixels(DYNAMIC_TEXTURE_SIZE - 1 - row),
                QuarterPixels((blockIndex * MIRROR_BLOCK_WIDTH) + MIRROR_BLOCK_WIDTH),
                QuarterPixels(DYNAMIC_TEXTURE_SIZE - row),
                G_TX_RENDERTILE,
                TexelCoordinate(MIRROR_SOURCE_LEFT + (blockIndex * MIRROR_BLOCK_SPACING)),
                TexelCoordinate(MIRROR_SOURCE_TOP + (row * MIRROR_ROW_SPACING)),
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(f_NoiseTexture),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
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
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, MIRROR_NOISE_ALPHA);
    gSPTextureRectangle(
        gfx++,
        0,
        0,
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_2CYCLE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineLERP(
        gfx++,
        TEXEL0,
        TEXEL1,
        PRIMITIVE_ALPHA,
        TEXEL1,
        0,
        0,
        0,
        PRIMITIVE,
        PRIMITIVE,
        0,
        COMBINED,
        0,
        0,
        0,
        0,
        COMBINED
    );
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        RenderTargetAddress(osVirtualToPhysical(g_FlameTexture))
    );
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(f_NoiseTexture),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(f_FlameGradientTexture),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, FLAME_NOISE_ALPHA);
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        0,
        f_DynamicTextureScroll,
        32,
        f_DynamicTextureScroll + 32
    );
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(
        gfx++,
        0,
        0,
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        RenderTargetAddress(osVirtualToPhysical(g_BlendedTexture))
    );
    gDPLoadTextureBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureScrollingLayer)),
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPLoadMultiBlock(
        gfx++,
        TextureAddress(ImageBody(g_BlendedTextureStillLayer)),
        SECOND_TEXTURE_TMEM,
        1,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        DYNAMIC_TEXTURE_SIZE,
        DYNAMIC_TEXTURE_SIZE,
        0,
        G_TX_NOMIRROR | G_TX_WRAP,
        G_TX_NOMIRROR | G_TX_WRAP,
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        TextureMask(DYNAMIC_TEXTURE_SIZE),
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, BLENDED_TEXTURE_ALPHA);
    gDPSetTileSize(
        gfx++,
        G_TX_RENDERTILE,
        f_DynamicTextureScroll,
        f_DynamicTextureScroll,
        f_DynamicTextureScroll + 32,
        f_DynamicTextureScroll + 32
    );
    gDPSetTileSize(gfx++, 1, 0, 0, 32, 32);
    gSPTextureRectangle(
        gfx++,
        0,
        0,
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        QuarterPixels(DYNAMIC_TEXTURE_SIZE),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );
    gDPPipeSync(gfx++);
    gDPSetScissor(
        gfx++,
        G_SC_NON_INTERLACE,
        SCREEN_BORDER,
        SCREEN_BORDER,
        SCREEN_WIDTH - SCREEN_BORDER,
        SCREEN_HEIGHT - SCREEN_BORDER
    );
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_MAGICSQ);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// How DrawGame draws a frame, chosen by scene
enum DrawMode {
    DRAW_MODE_INSET_SCREEN, // A module's screen, with the course in the inset view
    DRAW_MODE_COURSE,
    // DRAW_MODE_UNUSED_CLEAR and DRAW_MODE_OPENING are never chosen. The first
    // only clears the screen, as DRAW_MODE_CONFIRM_WAIT does.
    DRAW_MODE_UNUSED_CLEAR,
    DRAW_MODE_MAP_PERSPECTIVE,
    DRAW_MODE_MAP_ISOMETRIC,
    DRAW_MODE_MAP_MINIATURE,
    DRAW_MODE_MAP_TOP_DOWN,
    DRAW_MODE_OPENING,
    DRAW_MODE_CONFIRM_WAIT,
    DRAW_MODE_BOOT_LOGOS,
    DRAW_MODE_TITLE_SCREEN,
    DRAW_MODE_STAFF_ROLL,
    DRAW_MODE_TRANSMIT,
    DRAW_MODE_NOTHING,
    DRAW_MODE_BLANK
};

typedef u32 DrawMode;

// The map cameras' views, in world units
#define MAP_NEAR_CLIP 10.0f
#define MAP_FAR_CLIP 8000.0f
#define MAP_ORTHO_HALF_WIDTH 160.0f
#define MAP_ORTHO_HALF_HEIGHT 120.0f
#define MAP_ORTHO_FAR_CLIP 20000.0f
// How far the isometric and top-down maps push the scene into their view
#define MAP_ORTHO_PLACEMENT_DEPTH 1000.0f
#define MAP_ISOMETRIC_OFFSET_X 5000.0f
#define MAP_ISOMETRIC_OFFSET_Y 4000.0f
#define MAP_TOP_DOWN_HEIGHT 4000.0f
#define MAP_TOP_DOWN_TILT 50.0f
#define MAP_MINIATURE_SCALE 0.07f

// The course camera's clip planes, in world units
#define COURSE_NEAR_CLIP 10.0f
// A double, so that DrawGame's upper clamp compares in double precision
#define COURSE_FAR_CLIP 32000.0
#define CLOSE_UP_NEAR_CLIP 200.0f
#define SHORT_FAR_CLIP 500.0f

// Vertices that the course drawers may add to a frame
#define VERTEX_BUFFER_LENGTH 512
#define EXPANSION_VERTEX_BUFFER_LENGTH 5000
// Where undefined_syms.txt places g_ExpansionVertexBuffers for the N64
#define EXPANSION_VERTEX_BUFFERS_ADDRESS 0x8049C860
#define WORK_AREA_VERTEX_OFFSET 0x6A920

#define GFX_TASK_FLAGS (NU_SC_TASK_DP_WAIT | NU_SC_SWAPBUFFER)

// With the Expansion Pak, the vertex buffers of the frames, one for each draw
// buffer, at a fixed place in its memory
#ifdef TARGET_PC
#    define g_ExpansionVertexBuffers \
        (*(Vtx(*)[2][EXPANSION_VERTEX_BUFFER_LENGTH])FixedAddress(EXPANSION_VERTEX_BUFFERS_ADDRESS))
#else
extern Vtx g_ExpansionVertexBuffers[2][EXPANSION_VERTEX_BUFFER_LENGTH];
#endif
extern unsigned long _mus_cpu_last;

extern unsigned long _mus_cpu_worst;
int __MusIntMemRemaining(void);

#define SetCameraScissor(view) \
    gDPSetScissor( \
        g_DisplayListHead++, \
        G_SC_NON_INTERLACE, \
        (view).scissorX, \
        (view).scissorY, \
        (view).scissorX + (view).scissorWidth, \
        (view).scissorY + (view).scissorHeight \
    )
#define FRAME_BUFFER_TEXT_COLUMN 3
#define FRAME_BUFFER_TEXT_ROW 4
#define FRAME_RATE_TEXT_COLUMN 3
#define FRAME_RATE_TEXT_ROW 5

// Returns the frame count, which the caller ignores
//
// The views are counted by viewIndex alone, and each camera is read as
// g_Cameras[viewIndex]. Only the menu inset's hole keeps its view's address
// in a variable, insetView, set in that arm. The loop pass takes the first
// view's address out of the loop and makes insetView that address plus
// viewIndex times a camera's size, which it reduces to a register stepped
// by the size ($s4), beside the one it steps for the cameras' byte offset
// ($s5). It zeroes the offset before it sets the pointer, as the original
// does. The hoisted address gets no register of its own, so each other
// &g_Cameras[viewIndex].view rebuilds it in $t1 and adds the offset, as in
// the original. A camera pointer stepped in the loop's header ends up as
// the same two registers, but set in the other order before the loop.
//
// The inset's backdrop takes the inset camera's view by its index, as the
// course's inset view does, and the two calls share a tail that loads the
// address as a constant.
//
// The isometric map's eye is worked out inside guLookAt's arguments. The
// matrices pointer is then reloaded from the stack before the eye's
// subtractions, as in the original; with the eye set in statements before
// the call, it is reloaded after them.
//
// The far plane is clamped between the short far plane and the course's,
// then set to the course's anyway, so both clamps are dead. The upper
// clamp keeps the lower clamp's move, as the original does, whose bc1tl
// branches to the very next instruction with the move in its delay slot:
// flow deletes the upper clamp's move as dead but keeps its branch, which
// still reads the clamped value, and jump2 then deletes the empty branch
// with its compare.
//
// COURSE_FAR_CLIP is a double, unlike the other planes, so the upper
// clamp compares in double precision. As a float, the clamped value takes
// $f0 rather than the $f6 that the original gives it and the course's far
// plane. The target keeps no trace of the upper clamp's bound: other
// bounds, such as 0.0f or 1000.0f, match as well.
//
// Measured against the match, as words differing and how many of them in
// shape:
//
//   The inset's view read as g_Cameras[viewIndex].view at each
//     use, with no variable                                    478 (106)
//   The inset's view read through courseView                   390 (54),
//                                                              frame -8
//   A camera pointer set in the inset's arm, for insetView     129 (33)
//   insetView set at the loop's top                            356 (66)
//   insetView set after matrices                               346 (58)
//   The inset's field of view read through insetView           19 (9)
//   The inset's scissor read through insetView                 154 (6)
//   The inset's scissor read from g_Cameras[viewIndex].view    118 (6)
//   The inset's backdrop given insetView, or
//     &g_Cameras[viewIndex].view                               341 (51)
//   The inset's pin given insetView                            93 (11),
//                                                              frame -8
//   The inset's pin given the inset camera's view by its index 3 (3)
//   The course's pin given courseView                          216 (0)
//   The course's view read as g_Cameras[viewIndex].view at
//     each use, with no variable                               427 (83)
//   A camera pointer stepped in the loop's header, the inset's
//     view read through it or insetView set from it            81 (21)
//   The same, with the pins' views through it as well          32 (14)
//   The same, with the inset's backdrop through it too         27 (7)
//   The same, with the eye set in statements (the old draft)   31 (9)
//   insetView set from a camera pointer set at the loop's top  356 (66)
//   The eye set in statements before guLookAt                  4 (2)
//   The eye's x alone set in the arguments                     56 (16)
//   The eye's y alone set in the arguments                     8 (6)
//   The eye's x and y set in the arguments                     12 (6)
//   The eye's z, or its y and z, set in the arguments          4 (2)
//   The eye set in statements before the buffers are cleared   88 (22)
//   Without the upper clamp, or without the lower              10 (8)
//   Without either clamp                                       26 (18)
//   The clamps joined by else                                  272 (12)
//   The upper clamp first                                      13 (13)
//   COURSE_FAR_CLIP as a float                                 12 (4)
//   Without LoadProjection's unusedMatrix                      16 (0), and
//                                                              16 (0),
//                                                              frame +64,
//                                                              there
//   The view loop testing != 2                                 4 (4)
//
// Also matching: insetView set anywhere in its arm before guLookAtF, or
// at the top of the inset screen's case; insetView declared in its arm
// with its value, or after courseView; and the eye set in statements
// after a variable is set to the viewing matrix's address, for guLookAt's
// first argument.
s32 DrawGame(void) {
    Boolean usesFloatMatrices;
    CameraMatrices *matrices;
    u16 mapPerspNorm;
    u16 insetPerspNorm;
    u16 perspectiveNormalization;
    Boolean drawsScene;
    TerrainStyle terrainStyle;
    DrawMode drawMode;
    s32 cameraIndex;
    s32 slotIndex;
    s32 commandCount;
    f32 nearZ;
    f32 farClip;
    f32 billboardRise;
    f32 eyeX;
    f32 eyeY;
    f32 eyeZ;
    f32 topDownEyeX;
    f32 topDownEyeY;
    f32 topDownEyeZ;
    f32 lookAtX;
    f32 lookAtY;
    f32 lookAtZ;
    CameraMode cameraMode;
    CameraView *insetView;
    CameraView *courseView;
    f32 *lookAt;
    u16 *pressedButtons;

    // Loads the projection of the camera being drawn
    void LoadProjection(void) {
        Mat4 projection;
        Mat4 translation;
        // Pads the frame to match
        Mat4 unusedMatrix UNUSED;

        if (usesFloatMatrices) {
            guTranslateF(translation, 0.0f, 0.0f, 0.0f);
            guMtxCatF(matrices->viewingF, translation, matrices->viewingF);
            MultiplyMatrices(matrices->viewingF, matrices->perspectiveF, projection);
            ConvertMatrixToFixed(projection, &matrices->projection);
            gSPMatrix(
                g_DisplayListHead++,
                &matrices->projection,
                G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
            );
            SetCpuProjectionF(matrices->perspectiveF, matrices->viewingF, NULL);
        }
        else {
            gSPMatrix(
                g_DisplayListHead++,
                &matrices->perspective,
                G_MTX_PROJECTION | G_MTX_LOAD | G_MTX_NOPUSH
            );
            gSPMatrix(
                g_DisplayListHead++,
                &matrices->placement,
                G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH
            );
            gSPMatrix(
                g_DisplayListHead++,
                &matrices->viewing,
                G_MTX_PROJECTION | G_MTX_MUL | G_MTX_NOPUSH
            );
            SetCpuProjection(&matrices->perspective, &matrices->viewing, NULL);
        }
    }

    f_DrawStartTime = OS_CYCLES_TO_USEC(osGetTime());
    if (IsGameFlagSet(GAME_FLAG_SHOW_AUDIO_CPU_USE)) {
        PrintDebugPaddedHex(_mus_cpu_last, 20, 10);
        PrintDebugPaddedHex(_mus_cpu_worst, 20, 11);
        MusPrintDmaUsage();
    }

    if (IsGameFlagSet(GAME_FLAG_AUDIO_TEST)) {
        PrintDebugPaddedHex(__MusIntMemRemaining(), 20, 10);
        pressedButtons = &g_ControllerData[1].trigger;
        if (*pressedButtons & B_BUTTON) {
            RequestSongStop();
            _mus_cpu_worst = 0;
        }

        if (*pressedButtons & A_BUTTON) {
            PlaySound(SOUND_CALL_BIRDIE, AUDIO_TEST_FIRST_CHANNEL, AUDIO_TEST_VOLUME);
            PlaySound(SOUND_PEACH_CALL_OUT_C_LEFT, AUDIO_TEST_SECOND_CHANNEL, AUDIO_TEST_VOLUME);
            PlaySound(SOUND_EAGLE_CUP_IN, AUDIO_TEST_THIRD_CHANNEL, AUDIO_TEST_VOLUME);
        }
    }

    switch (g_Scene) {
        case SCENE_HEIGHTS:
        case SCENE_TREES:
        case SCENE_PIN:
        case SCENE_TRIANGLES:
        case SCENE_RINGS:
        case SCENE_WALLS:
            drawMode = DRAW_MODE_MAP_PERSPECTIVE;
            break;
        case SCENE_CELL_EDITOR:
        case SCENE_POINT_EDITOR:
            drawMode = DRAW_MODE_MAP_TOP_DOWN;
            break;
        case SCENE_MAP_OVERVIEW:
        case SCENE_GROUND_CAMERA:
            drawMode = DRAW_MODE_MAP_MINIATURE;
            break;
        case SCENE_MENU:
            drawMode = DRAW_MODE_INSET_SCREEN;
            break;
        case SCENE_BOOT_LOGOS:
            drawMode = DRAW_MODE_BOOT_LOGOS;
            break;
        case SCENE_CONFIRM_WAIT:
            drawMode = DRAW_MODE_CONFIRM_WAIT;
            break;
        case SCENE_TITLE_SCREEN:
            drawMode = DRAW_MODE_TITLE_SCREEN;
            break;
        case SCENE_STAFF_ROLL:
            drawMode = IsGameFlagSet(GAME_FLAG_STAFF_ROLL_ON_COURSE) ? DRAW_MODE_COURSE :
                                                                       DRAW_MODE_STAFF_ROLL;
            break;
        case SCENE_TRANSMIT:
            drawMode = DRAW_MODE_TRANSMIT;
            break;
        case SCENE_UNUSED_UNDRAWN_A:
        case SCENE_UNUSED_UNDRAWN_B:
            drawMode = DRAW_MODE_NOTHING;
            break;
        case SCENE_FLAG_PANEL:
        case SCENE_UNUSED_BLANK_A:
        case SCENE_UNUSED_BLANK_B:
        case SCENE_SWAPPING_MODULES:
            drawMode = DRAW_MODE_BLANK;
            break;
        default:
            drawMode = DRAW_MODE_COURSE;
            break;
    }

    g_DisplayListHead = f_DisplayLists[g_DrawBufferIndex];
    gDPPipeSync(g_DisplayListHead++);
    gDPPipelineMode(g_DisplayListHead++, G_PM_1PRIMITIVE);
    if (g_HasExpansionPak) {
        g_VertexListHead = g_ExpansionVertexBuffers[g_DrawBufferIndex];
    }
    else {
        g_VertexListHead = (Vtx *)&g_WorkArea.bytes[WORK_AREA_VERTEX_OFFSET] +
            (g_DrawBufferIndex * VERTEX_BUFFER_LENGTH);
    }

    for (cameraIndex = 0; cameraIndex < CAMERA_COUNT; cameraIndex++) {
        drawsScene = TRUE;
        terrainStyle = TERRAIN_STYLE_NORMAL;
        usesFloatMatrices = FALSE;
        if (g_Scene == SCENE_STAFF_ROLL) {
            InitRcp(g_DrawBufferIndex + VIEWPORT_STAFF_ROLL);
            if (cameraIndex == INSET_CAMERA) {
                break;
            }

            gDPPipeSync(g_DisplayListHead++);
            gDPSetScissor(
                g_DisplayListHead++,
                G_SC_NON_INTERLACE,
                SCREEN_BORDER,
                SCREEN_BORDER,
                SCREEN_WIDTH - SCREEN_BORDER,
                SCREEN_HEIGHT - SCREEN_BORDER
            );
        }
        else if (drawMode == DRAW_MODE_UNUSED_CLEAR) {
            InitRcp((cameraIndex * DRAW_BUFFER_COUNT) + g_DrawBufferIndex);
        }
        else if (drawMode < DRAW_MODE_UNUSED_CLEAR) {
            InitRcp((cameraIndex * DRAW_BUFFER_COUNT) + g_DrawBufferIndex);
        }
        else {
            if (cameraIndex == INSET_CAMERA) {
                break;
            }

            InitRcp(g_DrawBufferIndex);
        }

        matrices = &f_CameraMatrices[(cameraIndex * DRAW_BUFFER_COUNT) + g_DrawBufferIndex];

        switch (drawMode) {
            case DRAW_MODE_BLANK:
                ClearBuffers(BUFFER_CLEAR_WHOLE);
                FillScreen();
                break;
            case DRAW_MODE_MAP_PERSPECTIVE:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                }

                guLookAt(
                    &matrices->viewing,
                    g_MapCamera.view.eye[AXIS_X],
                    g_MapCamera.view.eye[AXIS_Y],
                    g_MapCamera.view.eye[AXIS_Z],
                    g_MapCamera.view.lookAt[AXIS_X],
                    g_MapCamera.view.lookAt[AXIS_Y],
                    g_MapCamera.view.lookAt[AXIS_Z],
                    0.0f,
                    -1.0f,
                    0.0f
                );
                guAlign(
                    &matrices->billboard,
                    0.0f,
                    g_MapCamera.view.lookAt[AXIS_X] - g_MapCamera.view.eye[AXIS_X],
                    g_MapCamera.view.lookAt[AXIS_Y] - g_MapCamera.view.eye[AXIS_Y],
                    g_MapCamera.view.lookAt[AXIS_Z] - g_MapCamera.view.eye[AXIS_Z]
                );
                guAlign(
                    &matrices->antiBillboard,
                    M_PI,
                    -(g_MapCamera.view.lookAt[AXIS_X] - g_MapCamera.view.eye[AXIS_X]),
                    -(g_MapCamera.view.lookAt[AXIS_Y] - g_MapCamera.view.eye[AXIS_Y]),
                    -(g_MapCamera.view.lookAt[AXIS_Z] - g_MapCamera.view.eye[AXIS_Z])
                );
                terrainStyle = TERRAIN_STYLE_MAP;
                guPerspective(
                    &matrices->perspective,
                    &mapPerspNorm,
                    f_MapFieldOfView,
                    SCREEN_ASPECT_RATIO,
                    MAP_NEAR_CLIP,
                    MAP_FAR_CLIP,
                    1.0f
                );
                gSPPerspNormalize(g_DisplayListHead++, mapPerspNorm);
                guTranslate(&matrices->placement, 0.0f, 0.0f, 0.0f);
                LoadProjection();
                break;
            case DRAW_MODE_MAP_ISOMETRIC:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                }

                // The eye is set in the arguments, to match
                guLookAt(
                    &matrices->viewing,
                    eyeX = g_MapCamera.view.eye[AXIS_X] - MAP_ISOMETRIC_OFFSET_X,
                    eyeY = g_MapCamera.view.eye[AXIS_Y] - MAP_ISOMETRIC_OFFSET_Y,
                    eyeZ = g_MapCamera.view.eye[AXIS_Z] - MAP_ISOMETRIC_OFFSET_X,
                    g_MapCamera.view.eye[AXIS_X],
                    g_MapCamera.view.eye[AXIS_Y],
                    g_MapCamera.view.eye[AXIS_Z],
                    0.0f,
                    -1.0f,
                    0.0f
                );
                guAlign(
                    &matrices->billboard,
                    0.0f,
                    g_MapCamera.view.eye[AXIS_X] - eyeX,
                    g_MapCamera.view.eye[AXIS_Y] - eyeY,
                    g_MapCamera.view.eye[AXIS_Z] - eyeZ
                );
                guAlign(
                    &matrices->antiBillboard,
                    M_PI,
                    eyeX - g_MapCamera.view.eye[AXIS_X],
                    eyeY - g_MapCamera.view.eye[AXIS_Y],
                    eyeZ - g_MapCamera.view.eye[AXIS_Z]
                );
                guOrtho(
                    &matrices->perspective,
                    -MAP_ORTHO_HALF_WIDTH,
                    MAP_ORTHO_HALF_WIDTH,
                    -MAP_ORTHO_HALF_HEIGHT,
                    MAP_ORTHO_HALF_HEIGHT,
                    MAP_NEAR_CLIP,
                    MAP_ORTHO_FAR_CLIP,
                    8.0f
                );
                guTranslate(&matrices->placement, 0.0f, 0.0f, -MAP_ORTHO_PLACEMENT_DEPTH);
                terrainStyle = TERRAIN_STYLE_MAP;
                ScaleMatrix(&matrices->viewing, g_MapCamera.view.zoom, g_MapCamera.view.zoom, 1.0f);
                LoadProjection();
                break;
            case DRAW_MODE_MAP_TOP_DOWN:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                }

                topDownEyeX = g_MapCamera.view.eye[AXIS_X];
                terrainStyle =
                    (g_Scene == SCENE_CELL_EDITOR) ? TERRAIN_STYLE_TOP_DOWN_MAP : TERRAIN_STYLE_MAP;
                topDownEyeY = g_MapCamera.view.eye[AXIS_Y] - MAP_TOP_DOWN_HEIGHT;
                topDownEyeZ = g_MapCamera.view.eye[AXIS_Z] - MAP_TOP_DOWN_TILT;
                guLookAt(
                    &matrices->viewing,
                    topDownEyeX,
                    topDownEyeY,
                    topDownEyeZ,
                    topDownEyeX,
                    g_MapCamera.view.eye[AXIS_Y],
                    g_MapCamera.view.eye[AXIS_Z],
                    0.0f,
                    -1.0f,
                    0.0f
                );
                guAlign(
                    &matrices->billboard,
                    0.0f,
                    g_MapCamera.view.eye[AXIS_X] - topDownEyeX,
                    g_MapCamera.view.eye[AXIS_Y] - topDownEyeY,
                    g_MapCamera.view.eye[AXIS_Z] - topDownEyeZ
                );
                guAlign(
                    &matrices->antiBillboard,
                    M_PI,
                    topDownEyeX - g_MapCamera.view.eye[AXIS_X],
                    topDownEyeY - g_MapCamera.view.eye[AXIS_Y],
                    topDownEyeZ - g_MapCamera.view.eye[AXIS_Z]
                );
                guOrtho(
                    &matrices->perspective,
                    -MAP_ORTHO_HALF_WIDTH,
                    MAP_ORTHO_HALF_WIDTH,
                    -MAP_ORTHO_HALF_HEIGHT,
                    MAP_ORTHO_HALF_HEIGHT,
                    MAP_NEAR_CLIP,
                    MAP_ORTHO_FAR_CLIP,
                    8.0f
                );
                guTranslate(&matrices->placement, 0.0f, 0.0f, -MAP_ORTHO_PLACEMENT_DEPTH);
                ScaleMatrix(&matrices->viewing, g_MapCamera.view.zoom, g_MapCamera.view.zoom, 1.0f);
                LoadProjection();
                break;
            case DRAW_MODE_MAP_MINIATURE:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_BY_LINE);
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    FillScreen();
                }

                // The look-at is set within the call, to match: setting it before
                // the call gives 59 words differing (21 in shape) against 41, or 63
                // set in z, y, x order
                guLookAt(
                    &matrices->viewing,
                    g_MapCamera.view.eye[AXIS_X],
                    g_MapCamera.view.eye[AXIS_Y],
                    g_MapCamera.view.eye[AXIS_Z],
                    lookAtX = g_MapCamera.view.eye[AXIS_X] + 10.0f,
                    lookAtY = g_MapCamera.view.eye[AXIS_Y] + 1000.0f,
                    lookAtZ = g_MapCamera.view.eye[AXIS_Z],
                    0.0f,
                    -1.0f,
                    0.0f
                );
                guAlign(
                    &matrices->billboard,
                    0.0f,
                    lookAtX - g_MapCamera.view.eye[AXIS_X],
                    lookAtY - g_MapCamera.view.eye[AXIS_Y],
                    lookAtZ - g_MapCamera.view.eye[AXIS_Z]
                );
                guAlign(
                    &matrices->antiBillboard,
                    M_PI,
                    g_MapCamera.view.eye[AXIS_X] - lookAtX,
                    g_MapCamera.view.eye[AXIS_Y] - lookAtY,
                    g_MapCamera.view.eye[AXIS_Z] - lookAtZ
                );
                guOrtho(
                    &matrices->perspective,
                    -MAP_ORTHO_HALF_WIDTH,
                    MAP_ORTHO_HALF_WIDTH,
                    -MAP_ORTHO_HALF_HEIGHT,
                    MAP_ORTHO_HALF_HEIGHT,
                    1.0f,
                    30000.0f,
                    8.0f
                );
                guTranslate(&matrices->placement, 0.0f, 0.0f, -100.0f);
                ScaleMatrix(
                    &matrices->viewing,
                    MAP_MINIATURE_SCALE,
                    MAP_MINIATURE_SCALE,
                    MAP_MINIATURE_SCALE
                );
                LoadProjection();
                break;
            case DRAW_MODE_UNUSED_CLEAR:
            case DRAW_MODE_CONFIRM_WAIT:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DoNothingWithDisplayList(&g_DisplayListHead);
                }

                break;
            case DRAW_MODE_BOOT_LOGOS:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DrawBootLogos(&g_DisplayListHead);
                }

                break;
            case DRAW_MODE_OPENING:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DrawOpening(matrices, &g_DisplayListHead);
                }

                break;
            case DRAW_MODE_TITLE_SCREEN:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    FillScreen();
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DrawTitleScreen(&g_DisplayListHead);
                }

                break;
            case DRAW_MODE_STAFF_ROLL:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    FillScreen();
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DrawStaffRoll(&g_DisplayListHead);
                }

                break;
            case DRAW_MODE_TRANSMIT:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DrawTransmitMenu(&g_DisplayListHead);
                }

                break;
            case DRAW_MODE_INSET_SCREEN:
                if (cameraIndex == MAIN_CAMERA) {
                    ClearBuffers(BUFFER_CLEAR_WHOLE);
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                    DrawMenuScreen(&g_DisplayListHead);
                    g_Cameras[MAIN_CAMERA].view.eye[AXIS_X] = 0.0f;
                    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_X] = 0.0f;
                    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Y] = 0.0f;
                    g_Cameras[MAIN_CAMERA].view.lookAt[AXIS_Z] = 0.0f;
                    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Y] = 10.0f;
                    g_Cameras[MAIN_CAMERA].view.eye[AXIS_Z] = 60.0f;
                    SetCharacterHighlight(matrices);
                }
                else if (g_IsHoleShownInMenuInset && g_HasLoaderFinished == TRUE) {
                    usesFloatMatrices = TRUE;
                    guPerspectiveF(
                        matrices->perspectiveF,
                        &insetPerspNorm,
                        g_Cameras[cameraIndex].view.fieldOfView,
                        SCREEN_ASPECT_RATIO,
                        MAP_NEAR_CLIP,
                        MAP_FAR_CLIP,
                        1.0f
                    );
                    ConvertMatrixToFixed(matrices->perspectiveF, &matrices->perspective);
                    guTranslate(&matrices->placement, 0.0f, 0.0f, 0.0f);
                    // Only this arm keeps its view's address in a variable, to match
                    insetView = &g_Cameras[cameraIndex].view;
                    guLookAtF(
                        matrices->viewingF,
                        insetView->eye[AXIS_X],
                        insetView->eye[AXIS_Y],
                        insetView->eye[AXIS_Z],
                        insetView->lookAt[AXIS_X],
                        insetView->lookAt[AXIS_Y],
                        insetView->lookAt[AXIS_Z],
                        0.0f,
                        -1.0f,
                        0.0f
                    );
                    ConvertMatrixToFixed(matrices->viewingF, &matrices->viewing);
                    guAlign(
                        &matrices->billboard,
                        0.0f,
                        insetView->lookAt[AXIS_X] - insetView->eye[AXIS_X],
                        insetView->lookAt[AXIS_Y] - insetView->eye[AXIS_Y],
                        insetView->lookAt[AXIS_Z] - insetView->eye[AXIS_Z]
                    );
                    guAlign(
                        &matrices->antiBillboard,
                        0.0f,
                        -(insetView->lookAt[AXIS_X] - insetView->eye[AXIS_X]),
                        -(insetView->lookAt[AXIS_Y] - insetView->eye[AXIS_Y]),
                        -(insetView->lookAt[AXIS_Z] - insetView->eye[AXIS_Z])
                    );
                    LoadProjection();
                    gSPPerspNormalize(g_DisplayListHead++, insetPerspNorm);
                    gDPPipeSync(g_DisplayListHead++);
                    SetCameraScissor(g_Cameras[INSET_CAMERA].view);
                    gDPPipeSync(g_DisplayListHead++);
                    gDPPipeSync(g_DisplayListHead++);
                    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
                    UsePrimitiveDepth(1);
                    DrawBackdrop(&g_DisplayListHead, &g_Cameras[INSET_CAMERA].view, INSET_CAMERA);
                    LoadProjection();
                }

                break;
            case DRAW_MODE_COURSE:
                if (g_MenuBackdropState != MENU_BACKDROP_STATE_NONE &&
                    g_PauseScreen != PAUSE_SCREEN_FREE_LOOK) {
                    ClearBuffers(BUFFER_CLEAR_DEPTH_ONLY);
                }
                else if (!g_HasLoaderFinished) {
                    if (cameraIndex == MAIN_CAMERA) {
                        ClearBuffers(BUFFER_CLEAR_WHOLE);
                    }
                }
                else {
                    if (g_Scene == SCENE_STAFF_ROLL) {
                        ClearBuffers(BUFFER_CLEAR_WHOLE);
                    }
                    else if (cameraIndex == MAIN_CAMERA) {
                        ClearBuffers(BUFFER_CLEAR_DEPTH_ONLY);
                    }

                    usesFloatMatrices = TRUE;
                    if (g_Cameras[cameraIndex].view.drawFlags != 0) {
                        nearZ = -COURSE_NEAR_CLIP;
                        farClip = -g_GroundFarthestDepth * 1.2f;
                        if (g_Cameras[cameraIndex].mode == CAMERA_MODE_HOLE_INTRO ||
                            g_Cameras[cameraIndex].mode == CAMERA_MODE_FREE_LOOK) {
                            nearZ = -CLOSE_UP_NEAR_CLIP;
                        }

                        // Both clamps are dead: the far plane is always the course's
                        if (farClip < SHORT_FAR_CLIP) {
                            farClip = SHORT_FAR_CLIP;
                        }

                        if (farClip > COURSE_FAR_CLIP) {
                            farClip = COURSE_FAR_CLIP;
                        }

                        farClip = COURSE_FAR_CLIP;

                        guPerspectiveF(
                            matrices->perspectiveF,
                            &perspectiveNormalization,
                            g_Cameras[cameraIndex].view.fieldOfView,
                            SCREEN_ASPECT_RATIO,
                            -nearZ,
                            farClip,
                            1.0f
                        );
                        ConvertMatrixToFixed(matrices->perspectiveF, &matrices->perspective);
                        f_PerspectiveNormalization = perspectiveNormalization;
                        gSPPerspNormalize(g_DisplayListHead++, perspectiveNormalization);
                        guTranslate(&matrices->placement, 0.0f, 0.0f, 0.0f);
                        courseView = &g_Cameras[cameraIndex].view;
                        guLookAtReflectF(
                            matrices->viewingF,
                            &matrices->lookAt,
                            courseView->eye[AXIS_X],
                            courseView->eye[AXIS_Y],
                            courseView->eye[AXIS_Z],
                            courseView->lookAt[AXIS_X],
                            courseView->lookAt[AXIS_Y],
                            courseView->lookAt[AXIS_Z],
                            0.0f,
                            -1.0f,
                            0.0f
                        );
                        gSPLookAt(g_DisplayListHead++, &matrices->lookAt);
                        SetCharacterHighlight(matrices);
                        ConvertMatrixToFixed(matrices->viewingF, &matrices->viewing);
                        lookAt = courseView->lookAt;
                        if (g_GameState.isMiniGolf) {
                            billboardRise =
                                (courseView->lookAt[AXIS_Y] - courseView->eye[AXIS_Y]) * 0.125f;
                            guAlign(
                                &matrices->billboard,
                                0.0f,
                                courseView->lookAt[AXIS_X] - courseView->eye[AXIS_X],
                                billboardRise,
                                courseView->lookAt[AXIS_Z] - courseView->eye[AXIS_Z]
                            );
                        }
                        else {
                            billboardRise =
                                (courseView->lookAt[AXIS_Y] - courseView->eye[AXIS_Y]) * 0.5f;
                            guAlign(
                                &matrices->billboard,
                                0.0f,
                                courseView->lookAt[AXIS_X] - courseView->eye[AXIS_X],
                                billboardRise,
                                courseView->lookAt[AXIS_Z] - courseView->eye[AXIS_Z]
                            );
                        }

                        guAlign(
                            &matrices->antiBillboard,
                            0.0f,
                            -(lookAt[AXIS_X] - courseView->eye[AXIS_X]),
                            -(lookAt[AXIS_Y] - courseView->eye[AXIS_Y]),
                            -(lookAt[AXIS_Z] - courseView->eye[AXIS_Z])
                        );
                        LoadProjection();
                    }

                    if (cameraIndex == MAIN_CAMERA) {
                        if (g_Cameras[MAIN_CAMERA].view.drawFlags ==
                            (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT)) {
                            if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME &&
                                f_ScreenEffectFrameCount == SAVED_FRAME_STEP_SAVING) {
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                                gDPSetColorImage(
                                    g_DisplayListHead++,
                                    G_IM_FMT_RGBA,
                                    G_IM_SIZ_16b,
                                    SCREEN_WIDTH,
                                    TextureAddress(f_SavedFrameBuffer)
                                );
                                gDPSetFillColor(
                                    g_DisplayListHead++,
                                    GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)
                                );
                                gDPFillRectangle(
                                    g_DisplayListHead++,
                                    0,
                                    0,
                                    SCREEN_WIDTH - 1,
                                    SCREEN_HEIGHT - 1
                                );
                                gDPPipeSync(g_DisplayListHead++);
                            }

                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                RenderTargetAddress(osVirtualToPhysical(g_DepthBuffer))
                            );
                            gDPPipeSync(g_DisplayListHead++);
                            if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                                gDPSetDepthImage(
                                    g_DisplayListHead++,
                                    TextureAddress(f_SavedFrameBuffer)
                                );
                            }
                        }
                        else {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                            gDPSetDepthImage(g_DisplayListHead++, TextureAddress(nuGfxZBuffer));
                            gDPPipeSync(g_DisplayListHead++);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                TextureAddress(nuGfxZBuffer)
                            );
                            gDPSetFillColor(
                                g_DisplayListHead++,
                                GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)
                            );
                            gDPFillRectangle(
                                g_DisplayListHead++,
                                0,
                                0,
                                SCREEN_WIDTH - 1,
                                SCREEN_HEIGHT - 1
                            );
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                            );
                        }

                        gDPPipeSync(g_DisplayListHead++);
                        SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
                        if ((g_Cameras[MAIN_CAMERA].view.drawFlags !=
                                    (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT) ||
                                g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME ||
                                f_ScreenEffectFrameCount != SAVED_FRAME_STEP_SHOWING) &&
                            g_PauseScreen != PAUSE_SCREEN_FREE_LOOK) {
                            UsePrimitiveDepth(0);
                            if (!IsGameFlagSet(GAME_FLAG_HIDE_BACKDROP)) {
                                DrawBackdrop(&g_DisplayListHead, &g_Cameras[MAIN_CAMERA].view, 0);
                            }

                            LoadProjection();

                            // Keeps the main view out of the inset view, which clears it
                            // again
                            if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                                gDPSetColorImage(
                                    g_DisplayListHead++,
                                    G_IM_FMT_RGBA,
                                    G_IM_SIZ_16b,
                                    SCREEN_WIDTH,
                                    TextureAddress(nuGfxZBuffer)
                                );
                                gDPSetFillColor(
                                    g_DisplayListHead++,
                                    GPACK_ZDZ(0, 0) << 16 | GPACK_ZDZ(0, 0)
                                );
                                gDPFillRectangle(
                                    g_DisplayListHead++,
                                    g_Cameras[INSET_CAMERA].view.scissorX,
                                    g_Cameras[INSET_CAMERA].view.scissorY,
                                    g_Cameras[INSET_CAMERA].view.scissorX +
                                        g_Cameras[INSET_CAMERA].view.scissorWidth - 1,
                                    g_Cameras[INSET_CAMERA].view.scissorY +
                                        g_Cameras[INSET_CAMERA].view.scissorHeight - 1
                                );
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetColorImage(
                                    g_DisplayListHead++,
                                    G_IM_FMT_RGBA,
                                    G_IM_SIZ_16b,
                                    SCREEN_WIDTH,
                                    RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                                );
                                gDPSetFillColor(g_DisplayListHead++, 0xFFFFFFFF);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPPipeSync(g_DisplayListHead++);
                                gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
                                gDPPipeSync(g_DisplayListHead++);
                            }
                        }
                    }
                    else {
                        if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetRenderMode(g_DisplayListHead++, G_RM_NOOP, G_RM_NOOP2);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetCycleType(g_DisplayListHead++, G_CYC_FILL);
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                TextureAddress(nuGfxZBuffer)
                            );
                            gDPSetFillColor(
                                g_DisplayListHead++,
                                GPACK_ZDZ(G_MAXFBZ, 0) << 16 | GPACK_ZDZ(G_MAXFBZ, 0)
                            );
                            gDPFillRectangle(
                                g_DisplayListHead++,
                                g_Cameras[INSET_CAMERA].view.scissorX,
                                g_Cameras[INSET_CAMERA].view.scissorY,
                                g_Cameras[INSET_CAMERA].view.scissorX +
                                    g_Cameras[INSET_CAMERA].view.scissorWidth - 1,
                                g_Cameras[INSET_CAMERA].view.scissorY +
                                    g_Cameras[INSET_CAMERA].view.scissorHeight - 1
                            );
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetFillColor(g_DisplayListHead++, 0xFFFFFFFF);
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                            );
                        }
                        else if (g_Cameras[INSET_CAMERA].view.drawFlags ==
                            CAMERA_DRAW_SCREEN_EFFECT) {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                RenderTargetAddress(osVirtualToPhysical(g_DepthBuffer))
                            );
                        }

                        if (g_Cameras[INSET_CAMERA].view.drawFlags != 0) {
                            gDPPipeSync(g_DisplayListHead++);
                            gDPPipeSync(g_DisplayListHead++);
                            gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
                            gDPPipeSync(g_DisplayListHead++);
                            SetCameraScissor(g_Cameras[INSET_CAMERA].view);
                            UsePrimitiveDepth(0);
                            DrawBackdrop(
                                &g_DisplayListHead,
                                &g_Cameras[INSET_CAMERA].view,
                                INSET_CAMERA
                            );
                            LoadProjection();
                        }
                    }
                }

                break;
            case DRAW_MODE_NOTHING:
                break;
            default:
                drawsScene = FALSE;
                break;
        }

        if (f_NeedsIdentityMatrix) {
            f_NeedsIdentityMatrix = FALSE;
            guTranslate(&f_IdentityMatrix, 0.0f, 0.0f, 0.0f);
        }

        gSPMatrix(
            g_DisplayListHead++,
            &f_IdentityMatrix,
            G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH
        );
        if (g_MenuBackdropState != MENU_BACKDROP_STATE_NONE) {
            if (cameraIndex == MAIN_CAMERA) {
                CaptureMenuBackdrop(&g_DisplayListHead, g_MenuBackdropState);
                DrawMenuBackdrop(&g_DisplayListHead);
            }
        }

        if (drawMode == DRAW_MODE_COURSE) {
            if (g_HasLoaderFinished == TRUE &&
                (g_MenuBackdropState == MENU_BACKDROP_STATE_NONE ||
                    g_PauseScreen == PAUSE_SCREEN_FREE_LOOK)) {
                if (g_Cameras[cameraIndex].view.drawFlags != 0) {
                    if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME &&
                        f_ScreenEffectFrameCount == SAVED_FRAME_STEP_SHOWING) {
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetColorImage(
                            g_DisplayListHead++,
                            G_IM_FMT_RGBA,
                            G_IM_SIZ_16b,
                            SCREEN_WIDTH,
                            RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                        );
                        gDPSetDepthImage(g_DisplayListHead++, TextureAddress(nuGfxZBuffer));
                        gDPPipeSync(g_DisplayListHead++);
                    }
                    else {
                        SetSceneFog(&g_DisplayListHead);
                        if (!IsGameFlagSet(GAME_FLAG_SKIP_GROUND_DRAWING)) {
                            DrawGround(matrices, terrainStyle);
                        }

                        DrawBillboards(matrices, cameraIndex);
                        if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
                            DrawRings(&g_DisplayListHead, matrices, &g_Cameras[cameraIndex]);
                            if (cameraIndex == MAIN_CAMERA) {
                                BuildHorizonDynamicTextures(&g_DisplayListHead);
                                DrawGreenGrid(&g_DisplayListHead, matrices);
                            }

                            DrawHoleTriangles(
                                &g_DisplayListHead,
                                matrices,
                                &g_Cameras[cameraIndex]
                            );
                            SetSceneFog(&g_DisplayListHead);
                            DrawFadingBillboards(matrices, cameraIndex);
                        }
                    }

                    if (g_Cameras[cameraIndex].view.drawFlags ==
                            (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT) &&
                        g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME) {
                        gDPPipeSync(g_DisplayListHead++);
                        gDPSetColorImage(
                            g_DisplayListHead++,
                            G_IM_FMT_RGBA,
                            G_IM_SIZ_16b,
                            SCREEN_WIDTH,
                            RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                        );
                        gDPSetDepthImage(g_DisplayListHead++, TextureAddress(nuGfxZBuffer));
                        gDPPipeSync(g_DisplayListHead++);
                        DrawScreenEffect(matrices, &g_Cameras[cameraIndex].view);
                    }

                    SetSceneFog(&g_DisplayListHead);
                    if (g_Scene == SCENE_OPENING) {
                        DrawOpening(matrices, &g_DisplayListHead);
                        DrawEffects(matrices, &g_DisplayListHead);
                    }
                    else {
                        if (!IsGameFlagSet(GAME_FLAG_HIDE_PIN)) {
                            DrawPin(matrices, &g_Cameras[cameraIndex].view, cameraIndex);
                        }

                        if (g_ScreenEffect == SCREEN_EFFECT_SAVED_FRAME &&
                            f_ScreenEffectFrameCount == SAVED_FRAME_STEP_SHOWING) {
                            DrawRings(&g_DisplayListHead, matrices, &g_Cameras[cameraIndex]);
                            if (cameraIndex == MAIN_CAMERA) {
                                BuildHorizonDynamicTextures(&g_DisplayListHead);
                                DrawGreenGrid(&g_DisplayListHead, matrices);
                            }

                            DrawHoleTriangles(
                                &g_DisplayListHead,
                                matrices,
                                &g_Cameras[cameraIndex]
                            );
                            SetSceneFog(&g_DisplayListHead);
                            DrawFadingBillboards(matrices, cameraIndex);
                        }

                        if (!IsHoleOverviewStep(g_GameState.step)) {
                            cameraMode = g_Cameras[cameraIndex].mode;
                            if (cameraMode != CAMERA_MODE_HOLE_INTRO &&
                                cameraMode != CAMERA_MODE_FREE_LOOK) {
                                if (!IsGameFlagSet(GAME_FLAG_HIDE_GOLF_BALLS) &&
                                    g_IsModuleAlive[MODULE_GOLF_BALLS]) {
                                    DrawGolfBalls(matrices);
                                }

                                if (!IsGameFlagSet(GAME_FLAG_HIDE_CHARACTERS)) {
                                    for (slotIndex = 0; slotIndex < CHARACTER_SLOT_COUNT;
                                        slotIndex++) {
                                        DrawCharacterAndShadow(matrices, cameraIndex, slotIndex);
                                        if (g_Scene != SCENE_STAFF_ROLL) {
                                            break;
                                        }
                                    }
                                }

                                if (g_IsModuleAlive[MODULE_HOLE_OBJECTS]) {
                                    if (cameraIndex == MAIN_CAMERA) {
                                        DrawHoleCreatures(&g_DisplayListHead);
                                    }
                                }
                            }
                        }

                        if (!IsHoleOverviewStep(g_GameState.step)) {
                            if (g_IsModuleAlive[MODULE_SHOT_LABELS] &&
                                !IsGameFlagSet(GAME_FLAG_SKY_EDITOR)) {
                                DrawAimGuides(matrices);
                            }

                            if (g_IsModuleAlive[MODULE_ROUND_EVENTS]) {
                                DrawContactEffect(&g_DisplayListHead, matrices);
                            }

                            DrawEffects(matrices, &g_DisplayListHead);
                            if (cameraIndex == MAIN_CAMERA &&
                                g_IsModuleAlive[MODULE_SHOT_EFFECTS]) {
                                DrawSwingTrail(&g_DisplayListHead, matrices);
                            }
                        }

                        DrawRain(&g_DisplayListHead, matrices);
                        if (!IsHoleOverviewStep(g_GameState.step)) {
                            if (cameraIndex == MAIN_CAMERA) {
                                DrawBallTrail(&g_DisplayListHead, matrices);
                                DrawShotEffect(&g_DisplayListHead);
                                if (g_IsModuleAlive[MODULE_SHOT_EFFECTS]) {
                                    DrawSparks(&g_DisplayListHead);
                                }
                            }

                            if (g_PauseScreen != PAUSE_SCREEN_FREE_LOOK) {
                                if (cameraIndex == MAIN_CAMERA) {
                                    DrawLensFlare(&g_DisplayListHead);
                                }
                            }
                        }

                        if (g_Cameras[cameraIndex].view.drawFlags ==
                                (CAMERA_DRAW_SCENE | CAMERA_DRAW_SCREEN_EFFECT) &&
                            (g_ScreenEffect == SCREEN_EFFECT_SPIN ||
                                g_ScreenEffect == SCREEN_EFFECT_TILED_IMAGE ||
                                g_ScreenEffect == SCREEN_EFFECT_EMPTY)) {
                            gDPSetColorImage(
                                g_DisplayListHead++,
                                G_IM_FMT_RGBA,
                                G_IM_SIZ_16b,
                                SCREEN_WIDTH,
                                RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                            );
                            gDPSetDepthImage(g_DisplayListHead++, TextureAddress(nuGfxZBuffer));
                            gDPPipeSync(g_DisplayListHead++);
                            DrawScreenEffect(matrices, &g_Cameras[cameraIndex].view);
                        }
                    }
                }

                if (g_Cameras[cameraIndex].view.drawFlags == CAMERA_DRAW_SCREEN_EFFECT) {
                    if (cameraIndex == INSET_CAMERA) {
                        gDPSetColorImage(
                            g_DisplayListHead++,
                            G_IM_FMT_RGBA,
                            G_IM_SIZ_16b,
                            SCREEN_WIDTH,
                            RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
                        );
                        gDPPipeSync(g_DisplayListHead++);
                    }
                }
            }
        }
        else if (drawMode == DRAW_MODE_INSET_SCREEN) {
            if (g_IsHoleShownInMenuInset &&
                cameraIndex == INSET_CAMERA &&
                g_HasLoaderFinished == TRUE) {
                SetSceneFog(&g_DisplayListHead);
                DrawGround(matrices, terrainStyle);
                DrawBillboards(matrices, cameraIndex);
                if (!IsGameFlagSet(GAME_FLAG_HIDE_PIN)) {
                    DrawPin(matrices, &g_Cameras[cameraIndex].view, cameraIndex);
                }

                DrawHoleTriangles(&g_DisplayListHead, matrices, &g_Cameras[cameraIndex]);
                gDPPipeSync(g_DisplayListHead++);
                SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
            }
        }
        else if (drawsScene && IsOverlayInExpansionMemory()) {
            if (!IsGameFlagSet(GAME_FLAG_SKIP_GROUND_DRAWING)) {
                DrawGround(matrices, terrainStyle);
            }

            DrawHoleTriangles(&g_DisplayListHead, matrices, &g_Cameras[cameraIndex]);
            DrawPin(matrices, &g_MapCamera.view, cameraIndex);
            if (drawMode != DRAW_MODE_MAP_MINIATURE && drawMode != DRAW_MODE_MAP_TOP_DOWN) {
                DrawBillboards(matrices, cameraIndex);
            }

            if (g_IsModuleAlive[MODULE_GOLF_BALLS]) {
                DrawGolfBalls(matrices);
            }

            if (IsGameFlagSet(GAME_FLAG_READ_HOST_HOLE) && g_Scene != SCENE_FLAG_PANEL) {
                DrawTargetPole(matrices, &g_DisplayListHead);
            }
        }
    }

    UpdateDebugScenes();
    if (drawMode == DRAW_MODE_COURSE) {
        gDPPipeSync(g_DisplayListHead++);
        gDPPipeSync(g_DisplayListHead++);
        SetCameraScissor(g_Cameras[MAIN_CAMERA].view);
        gSPViewport(g_DisplayListHead++, &g_Viewports[g_DrawBufferIndex]);
        if (g_Cameras[INSET_CAMERA].view.drawFlags == CAMERA_DRAW_SCENE) {
            DrawInsetViewFrame();
        }

        if (g_IsModuleAlive[MODULE_PAUSE_MENU]) {
            DrawPauseMenu(&g_DisplayListHead);
        }

        if (g_HasLoaderFinished == TRUE &&
            !IsGameFlagSet(GAME_FLAG_HIDE_ROUND_HUD) &&
            !IsGameFlagSet(GAME_FLAG_SKY_EDITOR)) {
            DrawRoundHud(&g_Cameras[MAIN_CAMERA].view);
        }

        if (g_IsModuleAlive[MODULE_ROUND_EVENTS]) {
            DrawMovie(&g_DisplayListHead);
        }

        if (g_IsModuleAlive[MODULE_ROUND_EVENTS]) {
            DrawBanner(&g_DisplayListHead);
        }

        if (g_IsModuleAlive[MODULE_ROUND_EVENTS]) {
            DrawWindFlyGuy(&g_DisplayListHead);
        }

        DrawHoleIntroImages(&g_DisplayListHead);
        if (g_IsModuleAlive[MODULE_SCORECARD]) {
            DrawScorecard(&g_DisplayListHead);
        }

        if (g_IsModuleAlive[MODULE_ROUND_RESULTS]) {
            DrawRoundResults(&g_DisplayListHead);
            DrawResultsNotice(&g_DisplayListHead);
        }

        DrawHalfTurn(&g_DisplayListHead);
        if (g_IsModuleAlive[MODULE_SAVE_MENU]) {
            DrawGameSaveMenu(&g_DisplayListHead);
            DrawReplaySaveMenu(&g_DisplayListHead);
            DrawControlsScreen(&g_DisplayListHead);
        }
    }

    SwapMatrixBuffers();
    DrawAnimatedBoxes();
    gDPPipeSync(g_DisplayListHead++);
    gDPPipeSync(g_DisplayListHead++);
    gDPSetCycleType(g_DisplayListHead++, G_CYC_1CYCLE);
    gDPPipeSync(g_DisplayListHead++);

    if (g_HoleFlyoverFrame > FLYOVER_FADE_START_FRAME) {
        f_FadeAlpha = (g_HoleFlyoverFrame - FLYOVER_FADE_START_FRAME) * FLYOVER_FADE_ALPHA_STEP;
        if (f_FadeColor == FADE_COLOR_NONE) {
            SetScreenEffect(SCREEN_EFFECT_SPIN, FLYOVER_SPIN_FRAME_COUNT);
        }
    }
    else if (g_HoleFlyoverFrame == HOLE_FLYOVER_DONE) {
        f_FadeAlpha -= FLYOVER_FADE_LIFT_ALPHA_STEP;
        if (f_FadeAlpha < 0) {
            f_FadeAlpha = 0;
            g_HoleFlyoverFrame = 0;
        }
    }
    else {
        f_FadeAlpha = 0;
    }

    if (f_FadeAlpha != 0) {
        if (f_FadeColor == FADE_COLOR_BLACK) {
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0, 0, 0, f_FadeAlpha);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        }
        else if (f_FadeColor == FADE_COLOR_WHITE) {
            gDPSetPrimColor(g_DisplayListHead++, 0, 0, 0xFF, 0xFF, 0xFF, f_FadeAlpha);
            gDPSetCombineMode(g_DisplayListHead++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
        }
        else if (f_FadeColor == FADE_COLOR_NOISE) {
            gDPSetPrimColor(
                g_DisplayListHead++,
                0,
                0,
                FADE_NOISE_GRAY,
                FADE_NOISE_GRAY,
                FADE_NOISE_GRAY,
                f_FadeAlpha
            );
            gDPSetCombineLERP(
                g_DisplayListHead++,
                NOISE,
                0,
                PRIMITIVE,
                0,
                0,
                0,
                0,
                PRIMITIVE,
                NOISE,
                0,
                PRIMITIVE,
                0,
                0,
                0,
                0,
                PRIMITIVE
            );
        }

        if (f_FadeColor != FADE_COLOR_NONE) {
            gDPPipeSync(g_DisplayListHead++);
            gDPSetRenderMode(g_DisplayListHead++, G_RM_CLD_SURF, G_RM_CLD_SURF2);
            gSPTextureRectangle(
                g_DisplayListHead++,
                QuarterPixels(g_Cameras[MAIN_CAMERA].view.scissorX),
                QuarterPixels(g_Cameras[MAIN_CAMERA].view.scissorY),
                QuarterPixels(
                    g_Cameras[MAIN_CAMERA].view.scissorX + g_Cameras[MAIN_CAMERA].view.scissorWidth
                ),
                QuarterPixels(
                    g_Cameras[MAIN_CAMERA].view.scissorY + g_Cameras[MAIN_CAMERA].view.scissorHeight
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }
    }

    if (g_IsDebugEnabled && (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & L_TRIG)) {
        g_FrameStepState = FRAME_STEP_PAUSED;
    }

    if (IsGameFlagSet(GAME_FLAG_SCREEN_TEST_BUTTONS)) {
        if (g_ControllerStates[DEBUG_CONTROLLER].buttons & D_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_SAVED_FRAME, SAVED_FRAME_STEP_SAVING);
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].buttons & U_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_SPIN, SCREEN_TEST_SPIN_FRAME_COUNT);
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].buttons & L_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_TILED_IMAGE, SCREEN_TEST_FRAME_COUNT);
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].buttons & R_CBUTTONS) {
            SetScreenEffect(SCREEN_EFFECT_EMPTY, SCREEN_TEST_FRAME_COUNT);
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].buttons & D_CBUTTONS) {
            f_FadeAlpha -= SCREEN_TEST_FADE_STEP;
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].buttons & U_CBUTTONS) {
            f_FadeAlpha += SCREEN_TEST_FADE_STEP;
        }

        if (f_FadeAlpha > OPAQUE_ALPHA) {
            f_FadeAlpha = OPAQUE_ALPHA;
        }

        if (f_FadeAlpha < 0) {
            f_FadeAlpha = 0;
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & B_BUTTON) {
            g_FrameStepState = FRAME_STEP_PAUSED;
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & D_JPAD) {
            g_UnreadDebugSwitch ^= TRUE;
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & U_JPAD) {
            g_Cameras[INSET_CAMERA].view.drawFlags ^= CAMERA_DRAW_SCENE;
        }
    }

    DrawPrompt(&g_DisplayListHead);
    if (g_IsModuleAlive[MODULE_PAUSE_MENU]) {
        DrawChoiceBox(&g_DisplayListHead);
    }

    DrawTextSlots(&g_DisplayListHead);
    if (g_IsModuleAlive[MODULE_ROUND_EVENTS]) {
        DrawFlipMessage(&g_DisplayListHead);
    }

    DrawWipe(&g_DisplayListHead);
    DrawScreenFade(&g_DisplayListHead);
    DrawHoleIntroText(&g_DisplayListHead);
    if (IsGameFlagSet(GAME_FLAG_COLOR_TEST)) {
        DrawColorTest(&g_DisplayListHead);
    }

    DrawHudAnimation();
    if (g_IsModuleAlive[MODULE_OPENING]) {
        DrawOpeningLogo(&g_DisplayListHead, 1);
    }

    if (g_Scene == SCENE_STAFF_ROLL && g_IsModuleAlive[MODULE_STAFF_ROLL]) {
        DrawStaffRoll(&g_DisplayListHead);
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_FRAME_RATE)) {
        PrintDebugText(
            !g_HasThreeFrameBuffers ? "2 Frame Buffer" : "3 Frame Buffer",
            FRAME_BUFFER_TEXT_COLUMN,
            FRAME_BUFFER_TEXT_ROW
        );
        sprintf(g_PrintBuffer, "%2d draw/sec", g_FramesPerSecond);
        PrintDebugText(g_PrintBuffer, FRAME_RATE_TEXT_COLUMN, FRAME_RATE_TEXT_ROW);
    }

    if (IsGameFlagSet(GAME_FLAG_SHOW_DEBUG_LOG)) {
        ShowDebugLog();
    }

    DrawDebugText(&g_DisplayListHead);
    if (IsGameFlagSet(GAME_FLAG_DEBUG_TEXT_BUTTON) && (g_ControllerData[1].trigger & Z_TRIG)) {
        if (IsGameFlagSet(GAME_FLAG_HIDE_DEBUG_TEXT)) {
            ClearGameFlag(GAME_FLAG_HIDE_DEBUG_TEXT);
        }
        else {
            SetGameFlag(GAME_FLAG_HIDE_DEBUG_TEXT);
        }
    }

    if (IsGameFlagSet(GAME_FLAG_SCREEN_TEST_BUTTONS)) {
        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & Z_TRIG) {
            CaptureFrame();
        }

        if (g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & R_JPAD) {
            CaptureMapImage();
        }
    }

    gDPFullSync(g_DisplayListHead++);
    gSPEndDisplayList(g_DisplayListHead++);
    commandCount = g_DisplayListHead - f_DisplayLists[g_DrawBufferIndex];
    if ((u32)commandCount >= DISPLAY_LIST_LENGTH) {
        ReportFailedAssertion("EX", "c:/usgolf64/stage00.c", 2636);
    }

    if (!g_HasExpansionPak &&
        g_VertexListHead -
                ((Vtx *)&g_WorkArea.bytes[WORK_AREA_VERTEX_OFFSET] +
                    (g_DrawBufferIndex * VERTEX_BUFFER_LENGTH)) >=
            VERTEX_BUFFER_LENGTH) {
        ReportFailedAssertion("EX", "c:/usgolf64/stage00.c", 2637);
    }

    RecordDisplayListLength(commandCount);

    if (g_IsDebugEnabled) {
        if ((g_ControllerData[1].button & START_BUTTON) &&
            !(g_PreviousControllerData[1].button & START_BUTTON)) {
            g_FramePacingMode = (g_FramePacingMode + 1) % PACING_MODE_COUNT;
        }

        // Reads the second controller's last state for the third's
        if (g_IsDebugEnabled &&
            (g_ControllerData[2].button & START_BUTTON) &&
            !(g_PreviousControllerData[1].button & START_BUTTON)) {
            g_FramePacingMode = (g_FramePacingMode + 2) % PACING_MODE_COUNT;
        }
    }

    f_GfxTaskStartTime = OS_CYCLES_TO_USEC(osGetTime());
#ifdef AVOID_UB
    RecordDrawnSunPosition();
#endif
    if (g_FramePacingMode == PACING_MODE_NORMAL) {
        nuGfxTaskStart(
            f_DisplayLists[g_DrawBufferIndex],
            (s32)(g_DisplayListHead - f_DisplayLists[g_DrawBufferIndex]) * sizeof(Gfx),
            NU_GFX_UCODE_F3DEX,
            GFX_TASK_FLAGS
        );
    }
    else if (g_FramePacingMode == PACING_MODE_PERFORMANCE_METER) {
        nuGfxTaskStart(
            f_DisplayLists[g_DrawBufferIndex],
            (s32)(g_DisplayListHead - f_DisplayLists[g_DrawBufferIndex]) * sizeof(Gfx),
            NU_GFX_UCODE_F3DEX,
            NU_SC_TASK_DP_WAIT | NU_SC_NOSWAPBUFFER
        );
        DrawPerformanceMeter(
            2,
            200,
            f_DrawStartTime,
            f_GfxTaskStartTime + (f_UpdateEndTime - f_UpdateStartTime),
            GFX_TASK_FLAGS,
            METER_READOUT_FREE_MEMORY
        );
    }
    else {
        nuGfxTaskStart(
            f_DisplayLists[g_DrawBufferIndex],
            (s32)(g_DisplayListHead - f_DisplayLists[g_DrawBufferIndex]) * sizeof(Gfx),
            NU_GFX_UCODE_F3DEX,
            NU_SC_TASK_DP_WAIT | NU_SC_NOSWAPBUFFER
        );
        DrawMemoryMeter(192, GFX_TASK_FLAGS);
    }

    g_DrawBufferIndex ^= 1;
    nuGfxDisplayOn();
    return g_FrameCount;
}

// Copies the frame shown last onto the frame being drawn, so a frame can be
// built over the previous one without redrawing the scene
void CopyPreviousFrame(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s16 stripHeight;

    if (f_CopySourceFrame == NULL) {
        f_CopySourceFrame = osViGetNextFramebuffer();
    }
    else {
        f_CopySourceFrame = f_PreviousFrame;
    }

    f_PreviousFrame = nuGfxCfb_ptr;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetColorDither(gfx++, G_CD_DISABLE);
    gDPSetColorImage(
        gfx++,
        G_IM_FMT_RGBA,
        G_IM_SIZ_16b,
        SCREEN_WIDTH,
        RenderTargetAddress(osVirtualToPhysical(nuGfxCfb_ptr))
    );
    gDPPipeSync(gfx++);
    for (stripIndex = 0; stripIndex != SAVED_FRAME_STRIP_COUNT; stripIndex++) {
        stripHeight = (stripIndex == SAVED_FRAME_STRIP_COUNT - 1) ? SAVED_FRAME_LAST_STRIP_HEIGHT :
                                                                    SAVED_FRAME_STRIP_HEIGHT;
        gDPLoadTextureTile(
            gfx++,
            TextureAddress(
                f_CopySourceFrame + (stripIndex * SAVED_FRAME_STRIP_HEIGHT * SCREEN_WIDTH)
            ),
            G_IM_FMT_RGBA,
            G_IM_SIZ_16b,
            SCREEN_WIDTH,
            0,
            SCREEN_BORDER,
            SCREEN_BORDER,
            SCREEN_WIDTH - SCREEN_BORDER - 1,
            SCREEN_BORDER + stripHeight - 1,
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
            QuarterPixels(SCREEN_BORDER + (stripIndex * SAVED_FRAME_STRIP_HEIGHT)),
            QuarterPixels(SCREEN_WIDTH - SCREEN_BORDER - 1),
            QuarterPixels(
                (stripIndex * SAVED_FRAME_STRIP_HEIGHT) + SCREEN_BORDER + stripHeight - 1
            ),
            G_TX_RENDERTILE,
            TexelCoordinate(SCREEN_BORDER),
            TexelCoordinate(SCREEN_BORDER),
            COPY_MODE_TEXEL_STEP,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

// Darkens the last frame while the reset button is held. Each frame lays a
// translucent dark blue over the one before, so the screen fades out.
void DrawResetFade(void) {
    Gfx *gfx;
    Gfx *displayList;
    s32 passIndex;

    gfx = f_DisplayLists[g_DrawBufferIndex];
    displayList = gfx;

    // The original builds the frame in a loop that runs once
    for (passIndex = 0; passIndex != 1; passIndex++) {
        AppendRcpInit(g_DrawBufferIndex, &gfx);
        AppendBufferClear(BUFFER_CLEAR_DEPTH_ONLY, &gfx);
        gDPPipeSync(gfx++);
        gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        CopyPreviousFrame(&gfx);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0x00, 0x00, 0x10, RESET_FADE_ALPHA);
        gDPSetCombineLERP(
            gfx++,
            1,
            0,
            PRIMITIVE,
            0,
            0,
            0,
            0,
            PRIMITIVE,
            1,
            0,
            PRIMITIVE,
            0,
            0,
            0,
            0,
            PRIMITIVE
        );
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gSPTextureRectangle(
            gfx++,
            0,
            0,
            QuarterPixels(SCREEN_WIDTH),
            QuarterPixels(SCREEN_HEIGHT),
            G_TX_RENDERTILE,
            0,
            0,
            TEXEL_STEP_ONE,
            TEXEL_STEP_ONE
        );
        gDPFullSync(gfx++);
        gSPEndDisplayList(gfx++);
#ifdef AVOID_UB
        RecordDrawnSunPosition();
#endif
        nuGfxTaskStart(
            displayList,
            (s32)(gfx - displayList) * sizeof(Gfx),
            NU_GFX_UCODE_F3DEX,
            NU_SC_SWAPBUFFER
        );
        f_GfxTaskStartTime = OS_CYCLES_TO_USEC(osGetTime());
    }

    g_DrawBufferIndex ^= 1;
}

// Runs the parts of a frame's update that depend on the scene, then fits
// the inset view's viewport to its window
void UpdateGame(void) {
    InsetViewState *animation;
    u16 *windDirection;

    f_UpdateStartTime = OS_CYCLES_TO_USEC(osGetTime());
    if (g_Scene == SCENE_GROUND_CAMERA) {
        if (g_ControllerData[0].button & U_CBUTTONS) {
            g_MapCamera.view.eye[AXIS_Z] += MAP_CAMERA_STEP;
        }

        if (g_ControllerData[0].button & D_CBUTTONS) {
            g_MapCamera.view.eye[AXIS_Z] -= MAP_CAMERA_STEP;
        }

        if (g_ControllerData[0].button & L_CBUTTONS) {
            g_MapCamera.view.eye[AXIS_X] -= MAP_CAMERA_STEP;
        }

        if (g_ControllerData[0].button & R_CBUTTONS) {
            g_MapCamera.view.eye[AXIS_X] += MAP_CAMERA_STEP;
        }

        g_MapCamera.view.heading -= g_ControllerData[0].stick_x / MAP_CAMERA_TURN_PER_STICK;
        g_MapCamera.view.eye[AXIS_Y] =
            (f32)(-g_ControllerData[0].stick_y * MAP_CAMERA_LIFT_PER_STICK) - MAP_CAMERA_HEIGHT +
            ((f32)GetGroundHeight(
                 g_MapCamera.view.eye[AXIS_X] * GROUND_UNITS_PER_UNIT,
                 g_MapCamera.view.eye[AXIS_Z] * GROUND_UNITS_PER_UNIT
             ) / GROUND_UNITS_PER_UNIT);
        g_MapCamera.view.lookAt[AXIS_X] = g_MapCamera.view.eye[AXIS_X] +
            (cosf(g_MapCamera.view.heading) * MAP_CAMERA_LOOK_DISTANCE);
        g_MapCamera.view.lookAt[AXIS_Y] = 0.0f;
        g_MapCamera.view.lookAt[AXIS_Z] = g_MapCamera.view.eye[AXIS_Z] +
            (sinf(g_MapCamera.view.heading) * MAP_CAMERA_LOOK_DISTANCE);
    }

    if (g_Scene == SCENE_OPENING) {
        StepNoiseTexture();
        g_SkyState.viewedWindAngles[MAIN_CAMERA] =
            ViewedWindAngle(g_HoleSetup.conditions.windDirection, &g_Cameras[MAIN_CAMERA].view);
        if (g_SkyState.viewedWindAngles[MAIN_CAMERA] > FULL_TURN_RADIANS) {
            g_SkyState.viewedWindAngles[MAIN_CAMERA] -= FULL_TURN_RADIANS;
        }

        if (g_SkyState.viewedWindAngles[MAIN_CAMERA] < 0.0f) {
            g_SkyState.viewedWindAngles[MAIN_CAMERA] += FULL_TURN_RADIANS;
        }

        UpdateOpening();
        UpdateEffects();
    }
    else if (g_Scene == SCENE_PLAY_HOLE ||
        g_Scene == SCENE_PLAY_ROUND ||
        g_Scene == SCENE_PLAY_BEST_SHOT_REPLAY) {
        if (g_ScreenEffect != SCREEN_EFFECT_SAVED_FRAME) {
            StepNoiseTexture();
        }

        if (IsGameFlagSet(GAME_FLAG_SCREEN_TEST_BUTTONS)) {
            if ((g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & L_JPAD) &&
                g_InsetView.step < INSET_VIEW_STEP_COUNT) {
                g_InsetView.animation = INSET_VIEW_OPENING;
            }

            if ((g_ControllerStates[DEBUG_CONTROLLER].pressedButtons & R_JPAD) &&
                g_InsetView.step > 0) {
                g_InsetView.animation = INSET_VIEW_CLOSING;
            }

            if ((g_ControllerStates[SECOND_INSET_TEST_CONTROLLER].pressedButtons & L_JPAD) &&
                g_InsetView.step < INSET_VIEW_STEP_COUNT) {
                g_InsetView.animation = INSET_VIEW_OPENING;
            }

            if ((g_ControllerStates[SECOND_INSET_TEST_CONTROLLER].pressedButtons & R_JPAD) &&
                g_InsetView.step > 0) {
                g_InsetView.animation = INSET_VIEW_CLOSING;
            }
        }

        UpdateInsetView();
        UpdateEffects();
        UpdatePlay();
        windDirection = &g_HoleSetup.conditions.windDirection;
        g_SkyState.viewedWindAngles[MAIN_CAMERA] =
            ViewedWindAngle(*windDirection, &g_Cameras[MAIN_CAMERA].view);
        g_SkyState.viewedWindAngles[INSET_CAMERA] =
            ViewedWindAngle(*windDirection, &g_Cameras[INSET_CAMERA].view);
        if (g_SkyState.viewedWindAngles[MAIN_CAMERA] > FULL_TURN_RADIANS) {
            g_SkyState.viewedWindAngles[MAIN_CAMERA] -= FULL_TURN_RADIANS;
        }

        if (g_SkyState.viewedWindAngles[MAIN_CAMERA] < 0.0f) {
            g_SkyState.viewedWindAngles[MAIN_CAMERA] += FULL_TURN_RADIANS;
        }

        if (g_SkyState.viewedWindAngles[INSET_CAMERA] > FULL_TURN_RADIANS) {
            g_SkyState.viewedWindAngles[INSET_CAMERA] -= FULL_TURN_RADIANS;
        }

        if (g_SkyState.viewedWindAngles[INSET_CAMERA] < 0.0f) {
            g_SkyState.viewedWindAngles[INSET_CAMERA] += FULL_TURN_RADIANS;
        }
    }
    else if (g_Scene == SCENE_MENU) {
        StepNoiseTexture();
        animation = &g_InsetView.animation;
        if (*animation == INSET_VIEW_OPENING) {
            g_Cameras[INSET_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
            g_InsetView.step++;
            if (g_InsetView.step >= LARGE_INSET_VIEW_STEP_COUNT) {
                g_InsetView.step = LARGE_INSET_VIEW_STEP_COUNT;
                *animation = INSET_VIEW_STILL;
            }
        }
        else if (*animation == INSET_VIEW_CLOSING) {
            g_InsetView.step--;
            if (g_InsetView.step <= 0) {
                g_InsetView.step = 0;
                *animation = INSET_VIEW_STILL;
                g_Cameras[INSET_CAMERA].view.drawFlags = 0;
            }
        }

        g_Cameras[INSET_CAMERA].view.scissorWidth =
            g_InsetView.step * LARGE_INSET_VIEW_GROWTH_PER_STEP;
        g_Cameras[INSET_CAMERA].view.scissorHeight =
            g_InsetView.step * LARGE_INSET_VIEW_GROWTH_PER_STEP;
        g_Cameras[INSET_CAMERA].view.scissorX =
            LARGE_INSET_VIEW_CENTER_X - (g_InsetView.step * (LARGE_INSET_VIEW_GROWTH_PER_STEP / 2));
        g_Cameras[INSET_CAMERA].view.scissorY =
            LARGE_INSET_VIEW_CENTER_Y - (g_InsetView.step * (LARGE_INSET_VIEW_GROWTH_PER_STEP / 2));
        g_Cameras[INSET_CAMERA].view.zoom = 0.9f;
        UpdateMenus();
        if (IsGameFlagSet(GAME_FLAG_DEBUG_MENU_VALUES)) {
            sprintf(g_PrintBuffer, "%3d", (int)f_DebugMenuValues[0]);
            PrintDebugText(g_PrintBuffer, DEBUG_MENU_COLUMN, DEBUG_MENU_ROW);
            sprintf(g_PrintBuffer, "%3d", (int)f_DebugMenuValues[1]);
            PrintDebugText(g_PrintBuffer, DEBUG_MENU_COLUMN, DEBUG_MENU_ROW + 1);
            sprintf(g_PrintBuffer, "%3d", (int)f_DebugMenuValues[2]);
            PrintDebugText(g_PrintBuffer, DEBUG_MENU_COLUMN, DEBUG_MENU_ROW + 2);
            PrintDebugText(" ", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW);
            PrintDebugText(" ", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW + 1);
            PrintDebugText(" ", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW + 2);
            PrintDebugText(">", DEBUG_MENU_CURSOR_COLUMN, DEBUG_MENU_ROW + f_DebugMenuValueIndex);
            if ((g_ControllerStates[DEBUG_MENU_CONTROLLER].repeatedButtons & U_CBUTTONS) &&
                f_DebugMenuValueIndex > 0) {
                f_DebugMenuValueIndex--;
            }

            if ((g_ControllerStates[DEBUG_MENU_CONTROLLER].repeatedButtons & D_CBUTTONS) &&
                f_DebugMenuValueIndex < DEBUG_MENU_VALUE_COUNT - 1) {
                f_DebugMenuValueIndex++;
            }

            if (g_ControllerStates[DEBUG_MENU_CONTROLLER].repeatedButtons & L_CBUTTONS) {
                if (f_DebugMenuValues[f_DebugMenuValueIndex] > 0) {
                    f_DebugMenuValues[f_DebugMenuValueIndex]--;
                }
            }

            if (g_ControllerStates[DEBUG_MENU_CONTROLLER].repeatedButtons & R_CBUTTONS) {
                if (f_DebugMenuValues[f_DebugMenuValueIndex] < DEBUG_MENU_VALUE_MAX) {
                    f_DebugMenuValues[f_DebugMenuValueIndex]++;
                }
            }
        }
    }
    else if (g_Scene == SCENE_GROUND_CAMERA) {
        g_Cameras[INSET_CAMERA].view.scissorX = SCREEN_WIDTH / 2;
        g_Cameras[INSET_CAMERA].view.scissorY = SCREEN_HEIGHT / 2;
        g_Cameras[INSET_CAMERA].view.scissorWidth = SCREEN_WIDTH / 2;
        g_Cameras[INSET_CAMERA].view.scissorHeight = SCREEN_HEIGHT / 2;
    }
    else if (g_Scene == SCENE_BOOT_LOGOS) {
        UpdateBootLogos();
    }
    else if (g_Scene == SCENE_CONFIRM_WAIT) {
        WaitForConfirmButton();
    }
    else if (g_Scene == SCENE_TITLE_SCREEN) {
        UpdateTitleScreen();
    }
    else if (g_Scene == SCENE_STAFF_ROLL) {
        UpdateStaffRoll();
    }
    else if (g_Scene == SCENE_TRANSMIT) {
        UpdateTransmitMenu();
    }

    g_Cameras[INSET_CAMERA].view.viewportCenterX = ViewportCenter(
        g_Cameras[INSET_CAMERA].view.scissorX,
        g_Cameras[INSET_CAMERA].view.scissorWidth
    );
    g_Cameras[INSET_CAMERA].view.viewportCenterY = ViewportCenter(
        g_Cameras[INSET_CAMERA].view.scissorY,
        g_Cameras[INSET_CAMERA].view.scissorHeight
    );
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vscale[0] =
        2.0f * (g_Cameras[INSET_CAMERA].view.zoom * SCREEN_WIDTH);
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vscale[1] =
        2.0f * (g_Cameras[INSET_CAMERA].view.zoom * SCREEN_HEIGHT);
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vtrans[0] =
        g_Cameras[INSET_CAMERA].view.viewportCenterX * 2;
    g_Viewports[VIEWPORT_INSET + g_DrawBufferIndex].vp.vtrans[1] =
        g_Cameras[INSET_CAMERA].view.viewportCenterY * 2;
    f_UpdateEndTime = OS_CYCLES_TO_USEC(osGetTime());
}

void UpdateOverlays(void) {
    if (g_Scene == SCENE_SWAPPING_MODULES) {
        if (!f_HasUnloadedSceneModule) {
            f_HasUnloadedSceneModule = TRUE;
            UnloadOverlay(GetOverlayOfModule(g_SceneModuleToUnload));
        }
        else if (CanLoadOverlay(GetOverlayOfModule(g_SceneModuleToLoad))) {
            LoadOverlay(GetOverlayOfModule(g_SceneModuleToLoad));
            f_HasUnloadedSceneModule = FALSE;
            g_Scene = g_NextScene;
        }
    }

    if (g_SubModuleState == SUB_MODULE_SWAP_PENDING) {
        if (!f_HasUnloadedSubModule) {
            f_HasUnloadedSubModule = g_SubModuleState;
            UnloadOverlay(GetOverlayOfModule(g_SubModuleId));
            return;
        }

        if (CanLoadOverlay(GetOverlayOfModule(g_PendingSubModuleId))) {
            LoadOverlay(GetOverlayOfModule(g_PendingSubModuleId));
            g_SubModuleState = SUB_MODULE_LOADED;
            f_HasUnloadedSubModule = FALSE;
            g_SubModuleId = g_PendingSubModuleId;
        }
    }
}

Boolean IsSubModuleLoaded(void) {
    return g_SubModuleState == SUB_MODULE_LOADED;
}

// The game modes are still known only by number
void RequestSubModule(void) {
    g_SubModuleState = SUB_MODULE_SWAP_PENDING;
    switch (g_GameState.gameMode) {
        case GAME_MODE_TRAINING:
        case GAME_MODE_RING_SHOT:
            g_PendingSubModuleId = MODULE_COURSE_SELECT;
            break;
        case GAME_MODE_CLUB_SLOTS:
            g_PendingSubModuleId = MODULE_CLUB_SLOTS;
            break;
        default:
            g_PendingSubModuleId = MODULE_SCORECARD;
            break;
    }
}

void UnloadSubModule(void) {
    UnloadOverlay(GetOverlayOfModule(g_SubModuleId));
    g_SubModuleState = SUB_MODULE_UNLOADED;
}

void SetInsetViewRectangle(s32 centerX, s32 centerY, s32 width, s32 height) {
    f_InsetViewRectangle.width = width;
    f_InsetViewRectangle.height = height;
    f_InsetViewRectangle.centerX = centerX;
    f_InsetViewRectangle.centerY = centerY;
}

// Grows or shrinks the inset view's window by a step and centers it
void UpdateInsetView(void) {
    InsetViewState *animation = &g_InsetView.animation;

    if (*animation == INSET_VIEW_OPENING) {
        if (g_InsetView.step == INSET_VIEW_OPEN_SOUND_STEP) {
            PlaySoundWithEffects(
                SOUND_PROMPT_OPEN,
                g_NextSoundChannel,
                INSET_VIEW_SOUND_VOLUME,
                INSET_VIEW_SOUND_REVERB,
                SOUND_FULL_SCALE
            );
        }

        g_Cameras[INSET_CAMERA].view.drawFlags = CAMERA_DRAW_SCENE;
        g_InsetView.step++;
        if (g_InsetView.step >= INSET_VIEW_STEP_COUNT) {
            g_InsetView.step = INSET_VIEW_STEP_COUNT;
            *animation = INSET_VIEW_STILL;
        }
    }
    else if (*animation == INSET_VIEW_CLOSING) {
        if (g_InsetView.step == INSET_VIEW_CLOSE_SOUND_STEP) {
            PlaySoundWithEffects(
                SOUND_PROMPT_CLOSE,
                g_NextSoundChannel,
                INSET_VIEW_SOUND_VOLUME,
                INSET_VIEW_SOUND_REVERB,
                SOUND_FULL_SCALE
            );
        }

        g_InsetView.step--;
        if (g_InsetView.step <= 0) {
            g_InsetView.step = 0;
            *animation = INSET_VIEW_STILL;
            g_Cameras[INSET_CAMERA].view.drawFlags = 0;
        }
    }

    g_Cameras[INSET_CAMERA].view.scissorWidth =
        g_InsetView.step * f_InsetViewRectangle.width / INSET_VIEW_STEP_COUNT;
    g_Cameras[INSET_CAMERA].view.scissorHeight =
        g_InsetView.step * f_InsetViewRectangle.height / INSET_VIEW_STEP_COUNT;
    g_Cameras[INSET_CAMERA].view.scissorX = f_InsetViewRectangle.centerX -
        (g_InsetView.step * f_InsetViewRectangle.width / (2 * INSET_VIEW_STEP_COUNT));
    g_Cameras[INSET_CAMERA].view.scissorY = f_InsetViewRectangle.centerY -
        (g_InsetView.step * f_InsetViewRectangle.height / (2 * INSET_VIEW_STEP_COUNT));
    g_Cameras[INSET_CAMERA].view.zoom = 0.9f;
}
