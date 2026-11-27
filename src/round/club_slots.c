/*
 * Club Slots: before play, each player in turn spins a slot machine of
 * three reels. The first reel holds woods, the second long irons and the
 * third short irons and wedges, among stars. Each reel the player stops
 * on a club adds that club to the bag beside the putter; three stars give
 * every club.
 */

#include "round/club_slots.h"

#include <framebuffer_count.h>
#include <ultra64.h>

#include "audio/sound.h"
#include "camera/view_projection.h"
#include "characters/character.h"
#include "controllers/controller.h"
#include "debug/game_flags.h"
#include "game/game.h"
#include "math/geometry.h"
#include "math/math_util.h"
#include "menus/menu.h"
#include "round/game_state.h"
#include "round/player.h"
#include "round/round_hud.h"
#include "screen/display_list.h"
#include "screen/screen.h"
#include "screen/screen_layers.h"
#include "screen/sky_and_hud_animations.h"
#include "shot/club.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"

// What the club slots are doing
enum ClubSlotsState {
    CLUB_SLOTS_DONE,
    // Before the reels speed up
    CLUB_SLOTS_STARTING,
    // The reels speed up while START! grows
    CLUB_SLOTS_SPEEDING,
    // The player stops the reels one by one
    CLUB_SLOTS_STOPPING,
    // All three reels show their clubs
    CLUB_SLOTS_STOPPED
};

typedef s32 ClubSlotsState;

// Each of the first two states lasts this many frames
#define CLUB_SLOTS_STATE_FRAME_COUNT 20
// The clubs are given this many frames after the reels stop
#define CLUB_SLOTS_RESULT_DELAY 30
// A reel stops by itself after this many frames
#define REEL_STOP_WAIT_TIME 150

#define REEL_COUNT 3
#define LAST_REEL (REEL_COUNT - 1)
#define REEL_SYMBOL_COUNT 20
// Symbols 0 to 12 are the clubs of those types; the star gives every club
#define CLUB_SYMBOL_COUNT CLUB_TYPE_PUTTER
#define SYMBOL_STAR CLUB_SYMBOL_COUNT
#define SYMBOL_COUNT (CLUB_SYMBOL_COUNT + 1)
#define NO_SYMBOL (-1)
// The reel shows the symbol this many places before its position in the middle
#define SHOWN_SYMBOL_OFFSET 3

// A reel moves on a symbol each time it turns this far
#define SYMBOL_ANGLE DegreesToRadians(35)
// A stopping reel turns this far each frame until its next symbol
#define STOPPING_REEL_SPEED DegreesToRadians(2)
// A spinning reel speeds up by this many degrees a frame, times the reels from
// its right
#define REEL_SPEED_STEP 0.5f
#define REEL_SPEED_MAX DegreesToRadians(18)
// A reel clicks each time f_TickTime reaches this
#define REEL_TICK_TIME 8

// A reel is a strip of quads around its axis, with an edge every
// REEL_EDGE_SPACING degrees. Each quad shows one symbol's image.
#define REEL_EDGE_COUNT 7
#define REEL_QUAD_COUNT (REEL_EDGE_COUNT - 1)
#define REEL_VERTEX_COUNT (REEL_EDGE_COUNT * 2)
#define REEL_EDGE_SPACING 36
#define REEL_HALF_WIDTH 160
#define REEL_RADIUS 512.0f
#define REEL_VERTEX_BUFFER_SIZE 0x180
#define REEL_SPACING 34
#define FIRST_REEL_X (-34.0f)
#define REEL_SCALE 0.1f
#define SLOTS_Y 8.0f
#define SLOTS_Z (-100.0f)

// The reels' window, in screen pixels, and the line across its middle
#define REEL_WINDOW_LEFT 96
#define REEL_WINDOW_TOP 56
#define REEL_WINDOW_RIGHT 224
#define REEL_WINDOW_BOTTOM 168
#define REEL_CENTER_LINE_TOP ((REEL_WINDOW_TOP + REEL_WINDOW_BOTTOM) / 2)

// Two quads shade the top and bottom of the reels' window
#define SHADE_VERTEX_COUNT 8
#define SHADE_HALF_WIDTH 50
// Each quad runs from SHADE_ALPHA at the window's edge to clear inside it
#define SHADE_EDGE_Y 48
#define SHADE_INNER_Y 16
#define SHADE_Z (-10)
#define SHADE_ALPHA 230

// The symbols' images are 32 by 32 RGBA16 texels
#define SYMBOL_SIZE 32
#define SYMBOL_IMAGE_SIZE (IMAGE_HEADER_SIZE + (SYMBOL_SIZE * SYMBOL_SIZE * 2))
// The texture coordinate of a symbol's last texel
#define SYMBOL_ST_END (TexelCoordinate(SYMBOL_SIZE) - 1)

// The slot machine, 304 by 224 texels drawn in strips 6 high, the last one 2
#define MACHINE_WIDTH 304
#define MACHINE_STRIP_HEIGHT 6
#define MACHINE_LAST_STRIP_HEIGHT 2
#define MACHINE_BOTTOM (SCREEN_HEIGHT - SCREEN_BORDER)

// The reels' stop buttons, each pushed in once its reel is told to stop
#define BUTTON_WIDTH 64
#define BUTTON_HEIGHT 64
#define BUTTON_STRIP_HEIGHT 32
#define BUTTON_STRIP_COUNT 2
#define BUTTON_LEFT 64
#define BUTTON_SHOWN_WIDTH 60
#define BUTTON_TOP 168

// START! is 160 texels wide, drawn in strips 10 apart, which overlap
#define START_WIDTH 160
#define START_STRIP_COUNT 6
#define START_STRIP_SPACING 10
#define START_STRIP_HEIGHT 12
#define START_LAST_STRIP_HEIGHT 4
#define START_SHOWN_HEIGHT 50
// START! grows as f_StartAngle turns this far each frame, up to START_MAX_ANGLE
#define START_ANGLE_STEP DegreesToRadians(20)

#define START_ANGLE_MAX DegreesToRadians(120)
// The faces of the players, the one spinning brighter and larger
#define FACE_BUFFER_COUNT 2
#define FACE_LEFT 256
#define FACE_TOP 72
#define FACE_SPACING 34
#define OTHER_FACE_LEFT 264
#define OTHER_FACE_TOP 76
// The spinning player's face changes every FACE_FRAME_TIME frames to the next
// of its frames
#define FACE_FRAME_DURATION 3
#define FACE_LAST_FRAME 4
// ...then stays on the last one for this many changes before the next player
// spins
#define FACE_HOLD_COUNT 9

// The logo, 256 by 72 texels drawn in strips 8 high
#define LOGO_WIDTH 256
#define LOGO_HEIGHT 72
#define LOGO_STRIP_HEIGHT 8
#define LOGO_STRIP_COUNT 9
#define LOGO_X (SCREEN_WIDTH / 2)
#define LOGO_Y 42
#define LOGO_BOB_STEP 0.2f
#define LOGO_BOB_HEIGHT 5.0f
// The shine, 64 by 64 texels, crosses the logo from SHINE_DELAY frames on
#define SHINE_DELAY 5
#define SHINE_START_TIME 16
#define SHINE_SPEED 18
// The primitive depths that the logo and its shine are drawn at
#define LOGO_PRIM_DEPTH 0x3FFF
#define SHINE_PRIM_DEPTH 0x4027
// The buffer the logo image is read into
#define LOGO_IMAGE_BUFFER_SIZE 0x4A20

// The first fourteen characters have their own club sets; the others use the
// sets after them
#define CHARACTER_CLUB_SET_COUNT 14

#define REEL_CLICK_CHANNEL 13
// Each stopping reel's sound pans this much further right
#define REEL_STOP_PAN_STEP 127

// The asset IDs of the images
#define ASSET_MACHINE 0x95B
#define ASSET_BUTTONS 0x971
#define ASSET_START 0x590
#define ASSET_LOGO 0x95A

typedef struct ClubSlots {
    s32 reelPositions[REEL_COUNT]; // Indices into f_ReelStrips
    s32 stoppedSymbols[REEL_COUNT]; // NO_SYMBOL until the reel stops
    s32 stoppedReelCount;
    Radians reelAngles[REEL_COUNT]; // How far each reel has turned toward its next symbol
    f32 reelSpeeds[REEL_COUNT];
    s32 pressCount; // The reels told to stop
    s32 playerIndex; // The player spinning
    s32 resultTime;
    Vtx *reelVertices;
    Vtx *shadeVertices;
    ImageAsset *machineImage;
    ImageAsset *buttonsImage;
    ImageAsset *startImage;
    ImageAsset *symbolImages;
    ImageAsset *faceImages[PLAYER_COUNT_MAX][FACE_BUFFER_COUNT];
} ClubSlots;

void DrawClubSlotsLogo(Gfx **displayListHead, s32 centerX, s32 centerY, f32 scale);

static ClubSlotsState f_ClubSlotsState = CLUB_SLOTS_DONE;
static u8 f_FaceBufferIndex = 0;
static u8 f_FaceFrame = 0;
static u8 f_FaceFrameTime = 0;
static u8 f_FaceHoldCount = 0;
static UBoolean8 f_IsShowingResult = FALSE;
static s16 f_StarCount = 0;
// The rows of g_FaceAnimationAssets and g_AlternateFaceAnimationAssets by
// character
static s16 f_CharacterFaceRows[CLUB_SET_COUNT] = {
    0,
    1,
    10,
    3,
    4,
    2,
    6,
    7,
    8,
    9,
    5,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
};
static u8 f_ReelStrips[REEL_COUNT][REEL_SYMBOL_COUNT] = {
    {0, 2, 1, 13, 0, 2, 1, 13, 0, 1, 0, 2, 13, 1, 13, 0, 2, 0, 1, 13},
    {5, 7, 4, 6, 3, 13, 5, 7, 4, 6, 5, 7, 13, 3, 4, 3, 13, 5, 3, 4},
    {10, 12, 13, 9, 11, 8, 13, 10, 13, 12, 9, 11, 13, 9, 13, 13, 12, 11, 8, 13},
};
static AssetId f_SymbolAssets[SYMBOL_COUNT] = {
    0x583,
    0x584,
    0x585,
    0x586,
    0x587,
    0x588,
    0x589,
    0x58A,
    0x58B,
    0x58C,
    0x58D,
    0x58E,
    0x58F,
    0x582,
};
static s32 f_StateFrameCount = 0;
static s32 f_StopWaitTime = 0;
static s32 f_TickTime = 0;
static Radians f_StartAngle = 0.0f;
static s32 f_ClubSlotsUnusedWord UNUSED = 0; // Nothing reads or writes it
static s32 f_ShineTime = 0;
static Radians f_LogoBobAngle = 0.0f;
static f32 f_LogoBobOffset = 0.0f;
static s32 f_LogoUnusedWord = 0; // Only ever cleared

static Mtx f_ReelMatrices[REEL_COUNT][DRAW_BUFFER_COUNT];
static Mtx f_ShadeMatrix;
static ClubSlots *f_Slots;
static u8 f_LogoImage[LOGO_IMAGE_BUFFER_SIZE] RCP_ALIGNED;
static u8 f_ShineImage[SHINE_IMAGE_BUFFER_SIZE] RCP_ALIGNED;

void LoadClubSlots(void) {
    EncodedAsset asset;
    s32 index;
    s32 row;
    Radians angle;

    f_Slots = AllocMemory(sizeof(ClubSlots));
    SetMenuBackdrop(MENU_BACKDROP_STILL);
    f_Slots->reelVertices = AllocMemory(REEL_VERTEX_BUFFER_SIZE);
    f_Slots->shadeVertices = AllocMemory(SHADE_VERTEX_COUNT * sizeof(Vtx));
    f_Slots->symbolImages = AllocMemory(SYMBOL_COUNT * SYMBOL_IMAGE_SIZE);
    for (index = 0; index != SYMBOL_COUNT; index++) {
        ReadEncodedAsset(
            f_Slots->symbolImages + (OpenEncodedAsset(f_SymbolAssets[index], &asset) * index),
            &asset
        );
    }

    f_Slots->machineImage = AllocMemory(OpenEncodedAsset(ASSET_MACHINE, &asset));
    ReadEncodedAsset(f_Slots->machineImage, &asset);
    f_Slots->buttonsImage = AllocMemory(OpenEncodedAsset(ASSET_BUTTONS, &asset));
    ReadEncodedAsset(f_Slots->buttonsImage, &asset);
    f_Slots->startImage = AllocMemory(OpenEncodedAsset(ASSET_START, &asset));
    ReadEncodedAsset(f_Slots->startImage, &asset);
    for (index = 0; index != g_GameState.playerCount; index++) {
        row = f_CharacterFaceRows[g_Players[index].characterIndex];
        f_Slots->faceImages[index][0] =
            AllocMemory(OpenEncodedAsset(g_FaceAnimationAssets[row][0], &asset));
        ReadEncodedAsset(f_Slots->faceImages[index][0], &asset);
        f_Slots->faceImages[index][1] =
            AllocMemory(OpenEncodedAsset(g_FaceAnimationAssets[row][0], &asset));
        ReadEncodedAsset(f_Slots->faceImages[index][1], &asset);
    }

    f_FaceBufferIndex = 0;
    f_FaceFrame = 0;
    f_FaceFrameTime = 0;
    f_FaceHoldCount = 0;
    f_IsShowingResult = FALSE;
    f_StarCount = 0;
    OpenEncodedAsset(ASSET_LOGO, &asset);
    ReadEncodedAsset(f_LogoImage, &asset);
    OpenEncodedAsset(ASSET_SHINE_IMAGE, &asset);
    ReadEncodedAsset(f_ShineImage, &asset);
    f_Slots->reelPositions[0] = guRandom() % REEL_SYMBOL_COUNT;
    f_Slots->reelPositions[1] = guRandom() % REEL_SYMBOL_COUNT;
    f_Slots->reelPositions[2] = guRandom() % REEL_SYMBOL_COUNT;
    f_ClubSlotsState = CLUB_SLOTS_STARTING;
    f_Slots->stoppedSymbols[0] = NO_SYMBOL;
    f_Slots->stoppedSymbols[1] = NO_SYMBOL;
    f_Slots->stoppedSymbols[2] = NO_SYMBOL;
    f_Slots->stoppedReelCount = 0;
    f_Slots->reelAngles[0] = DegreesToRadians(10);
    f_Slots->reelAngles[1] = DegreesToRadians(20);
    f_Slots->reelAngles[2] = DegreesToRadians(30);
    f_Slots->reelSpeeds[0] = DegreesToRadians(4);
    f_Slots->reelSpeeds[1] = DegreesToRadians(2);
    f_Slots->reelSpeeds[2] = 0;
    f_Slots->pressCount = 0;
    f_Slots->playerIndex = 0;
    for (index = 0; index != REEL_EDGE_COUNT; index++) {
        f_Slots->reelVertices[index * 2].v.ob[AXIS_X] = -REEL_HALF_WIDTH;
        f_Slots->reelVertices[(index * 2) + 1].v.ob[AXIS_X] = REEL_HALF_WIDTH;
        angle = DegreesToRadians((f32)((index - 1) * REEL_EDGE_SPACING));
        f_Slots->reelVertices[index * 2].v.ob[AXIS_Y] = cosf(angle) * REEL_RADIUS;
        f_Slots->reelVertices[(index * 2) + 1].v.ob[AXIS_Y] = cosf(angle) * REEL_RADIUS;
        f_Slots->reelVertices[index * 2].v.ob[AXIS_Z] = sinf(angle) * REEL_RADIUS;
        f_Slots->reelVertices[(index * 2) + 1].v.ob[AXIS_Z] = sinf(angle) * REEL_RADIUS;
        f_Slots->reelVertices[index * 2].v.cn[RED_CHANNEL] = COLOR_CHANNEL_MAX;
        f_Slots->reelVertices[(index * 2) + 1].v.cn[RED_CHANNEL] = COLOR_CHANNEL_MAX;
        f_Slots->reelVertices[index * 2].v.cn[GREEN_CHANNEL] = COLOR_CHANNEL_MAX;
        f_Slots->reelVertices[(index * 2) + 1].v.cn[GREEN_CHANNEL] = COLOR_CHANNEL_MAX;
        f_Slots->reelVertices[index * 2].v.cn[BLUE_CHANNEL] = COLOR_CHANNEL_MAX;
        f_Slots->reelVertices[(index * 2) + 1].v.cn[BLUE_CHANNEL] = COLOR_CHANNEL_MAX;
        f_Slots->reelVertices[index * 2].v.cn[ALPHA_CHANNEL] = OPAQUE_ALPHA;
        f_Slots->reelVertices[(index * 2) + 1].v.cn[ALPHA_CHANNEL] = OPAQUE_ALPHA;
    }

    for (index = 0; index != SHADE_VERTEX_COUNT; index++) {
        if (index & 2) {
            f_Slots->shadeVertices[index].v.ob[AXIS_X] = SHADE_HALF_WIDTH;
        }
        else {
            f_Slots->shadeVertices[index].v.ob[AXIS_X] = -SHADE_HALF_WIDTH;
        }

        f_Slots->shadeVertices[index].v.ob[AXIS_Z] = SHADE_Z;
        f_Slots->shadeVertices[index].v.cn[RED_CHANNEL] = 0;
        f_Slots->shadeVertices[index].v.cn[GREEN_CHANNEL] = 0;
        f_Slots->shadeVertices[index].v.cn[BLUE_CHANNEL] = 0;
        f_Slots->shadeVertices[index].v.cn[ALPHA_CHANNEL] = 0;
    }

    f_Slots->shadeVertices[0].v.ob[AXIS_Y] = SHADE_EDGE_Y;
    f_Slots->shadeVertices[1].v.ob[AXIS_Y] = SHADE_INNER_Y;
    f_Slots->shadeVertices[2].v.ob[AXIS_Y] = SHADE_INNER_Y;
    f_Slots->shadeVertices[3].v.ob[AXIS_Y] = SHADE_EDGE_Y;
    f_Slots->shadeVertices[4].v.ob[AXIS_Y] = -SHADE_INNER_Y;
    f_Slots->shadeVertices[5].v.ob[AXIS_Y] = -SHADE_EDGE_Y;
    f_Slots->shadeVertices[6].v.ob[AXIS_Y] = -SHADE_EDGE_Y;
    f_Slots->shadeVertices[7].v.ob[AXIS_Y] = -SHADE_INNER_Y;
    f_Slots->shadeVertices[0].v.cn[ALPHA_CHANNEL] = SHADE_ALPHA;
    f_Slots->shadeVertices[3].v.cn[ALPHA_CHANNEL] = SHADE_ALPHA;
    f_Slots->shadeVertices[5].v.cn[ALPHA_CHANNEL] = SHADE_ALPHA;
    f_Slots->shadeVertices[6].v.cn[ALPHA_CHANNEL] = SHADE_ALPHA;
    PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
}

void FreeClubSlots(void) {
    s32 playerIndex;

    SetMenuBackdrop(MENU_BACKDROP_NONE);
    SetFramebufferCount(TRIPLE_BUFFERING);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    FreeMemory(&f_Slots->symbolImages);
    FreeMemory(&f_Slots->machineImage);
    FreeMemory(&f_Slots->buttonsImage);
    FreeMemory(&f_Slots->startImage);
    FreeMemory(&f_Slots->reelVertices);
    FreeMemory(&f_Slots->shadeVertices);
    for (playerIndex = 0; playerIndex != g_GameState.playerCount; playerIndex++) {
        FreeMemory(&f_Slots->faceImages[playerIndex][0]);
        FreeMemory(&f_Slots->faceImages[playerIndex][1]);
    }

    FreeMemory(&f_Slots);
}

// Fills the player's bag from the stopped reels
void GiveSlotClubs(s32 playerIndex) {
    Player *player;
    s32 starCount;
    // Counts the slots, then the reels
    s32 index;
    ClubSlot clubSlot;
    s32 symbolIndex;
    s32 symbol;

    starCount = 0;
    player = &g_Players[playerIndex];
    for (index = 0; index != CLUB_SLOT_COUNT; index++) {
        player->clubs[index] = NO_CLUB;
    }

    player->clubs[CLUB_TYPE_PUTTER] = (player->characterIndex * CLUB_SET_SIZE) + CLUB_TYPE_PUTTER;
    f_StarCount = 0;
    for (index = 0; index != REEL_COUNT; index++) {
        symbolIndex = f_Slots->reelPositions[index] - SHOWN_SYMBOL_OFFSET;
        if (symbolIndex < 0) {
            symbolIndex += REEL_SYMBOL_COUNT;
        }

        symbol = f_ReelStrips[index][symbolIndex];
        if (symbol != SYMBOL_STAR) {
            if (player->characterIndex >= CHARACTER_CLUB_SET_COUNT) {
                player->clubs[symbol] =
                    ((player->gameBoyCharacterIndex + CHARACTER_CLUB_SET_COUNT) * CLUB_SET_SIZE) +
                    symbol;
            }
            else {
                player->clubs[symbol] = (player->characterIndex * CLUB_SET_SIZE) + symbol;
            }
        }
        else {
            starCount++;
            f_StarCount++;
        }
    }

    if (starCount == REEL_COUNT) {
        for (clubSlot = 0; clubSlot != CLUB_SYMBOL_COUNT; clubSlot++) {
            if (player->characterIndex >= CHARACTER_CLUB_SET_COUNT) {
                player->clubs[clubSlot] =
                    ((player->gameBoyCharacterIndex + CHARACTER_CLUB_SET_COUNT) * CLUB_SET_SIZE) +
                    clubSlot;
            }
            else {
                player->clubs[clubSlot] = (player->characterIndex * CLUB_SET_SIZE) + clubSlot;
            }
        }

        PlaySound(SOUND_THREE_STARS, g_NextSoundChannel, SOUND_FULL_SCALE);
    }
}

// Whether the next reel stops: on A, once the wait is too long, when the player
// is the computer, or while GAME_FLAG_FAST_AUTOPLAY is
#define IsReelStopping(controllerIndex, player) \
    ((g_ControllerStates[controllerIndex].pressedButtons & A_BUTTON) || \
        (f_StopWaitTime > REEL_STOP_WAIT_TIME) || \
        ((player)->isComputer) || \
        IsGameFlagSet(GAME_FLAG_FAST_AUTOPLAY))

// Returns TRUE once every player has spun
Boolean UpdateClubSlots(void) {
    Player *player;
    s32 controllerIndex;

    player = &g_Players[f_Slots->playerIndex];
    if (f_IsShowingResult != FALSE) {
        return FALSE;
    }

    if (f_Slots->playerIndex == g_GameState.playerCount) {
        f_ClubSlotsState = CLUB_SLOTS_DONE;
        return TRUE;
    }

    switch (f_ClubSlotsState) {
        case CLUB_SLOTS_STARTING:
            if (++f_StateFrameCount == CLUB_SLOTS_STATE_FRAME_COUNT) {
                f_StateFrameCount = 0;
                f_ClubSlotsState = CLUB_SLOTS_SPEEDING;
                PlaySound(SOUND_PROMPT_OPEN, g_NextSoundChannel, SOUND_FULL_SCALE);
            }

            break;
        case CLUB_SLOTS_SPEEDING:
            if (++f_StateFrameCount == CLUB_SLOTS_STATE_FRAME_COUNT) {
                f_StateFrameCount = 0;
                f_ClubSlotsState = CLUB_SLOTS_STOPPING;
                f_StopWaitTime = 0;
            }

            break;
        case CLUB_SLOTS_STOPPING:
            // A player with no controller of their own uses the first
            if (player->controllerIndex == NO_CONTROLLER_INDEX) {
                controllerIndex = 0;
            }
            else {
                controllerIndex = player->controllerIndex;
            }

            f_StopWaitTime++;
            if (f_Slots->pressCount < REEL_COUNT) {
                if (IsReelStopping(controllerIndex, player)) {
                    f_StopWaitTime = 0;
                    PlaySoundWithEffects(
                        SOUND_REEL_STOP,
                        g_NextSoundChannel,
                        SOUND_FULL_SCALE,
                        NO_REVERB,
                        f_Slots->pressCount++ * REEL_STOP_PAN_STEP
                    );
                }
            }
            else if (f_Slots->stoppedReelCount == REEL_COUNT) {
                f_ClubSlotsState = CLUB_SLOTS_STOPPED;
                f_Slots->resultTime = 0;
            }

            break;
        case CLUB_SLOTS_STOPPED:
            if (++f_Slots->resultTime == CLUB_SLOTS_RESULT_DELAY) {
                GiveSlotClubs(f_Slots->playerIndex);
                f_IsShowingResult = TRUE;
                if (f_Slots->playerIndex + 1 != g_GameState.playerCount) {
                    f_ClubSlotsState = CLUB_SLOTS_STARTING;
                    f_Slots->pressCount = 0;
                    f_Slots->stoppedReelCount = 0;
                    f_Slots->stoppedSymbols[0] = NO_SYMBOL;
                    f_Slots->stoppedSymbols[1] = NO_SYMBOL;
                    f_Slots->stoppedSymbols[2] = NO_SYMBOL;
                    PlaySound(SOUND_TALLY_DONE, g_NextSoundChannel, SOUND_FULL_SCALE);
                }
            }

            break;
    }

    return FALSE;
}

s32 GetClubSlotsState(void) {
    return f_ClubSlotsState;
}

// Draws the club slots: the reels and their shading inside the slot machine,
// the stop buttons pushed so far, START! while the reels speed up, the
// players' faces and the logo
void DrawClubSlots(Gfx **displayListHead) {
    Mat4 matrix;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    EncodedAsset asset;
    Gfx *gfx;
    Boolean isStripLoaded;
    s32 reelIndex;
    s32 quadIndex;
    s32 topOrPlayerIndex;
    s32 stripHeight;
    s32 loadedStripHeight;
    s32 texelOffset;
    s32 buttonOrStartStripIndex;
    s32 stripIndex;
    s32 row;
    f32 halfWidth;
    f32 dsdx;
    f32 startStripHeight;
    f32 halfHeight;
    f32 dtdy;

    gfx = *displayListHead;
    isStripLoaded = FALSE;
    for (reelIndex = 0; reelIndex != REEL_COUNT; reelIndex++) {
        if (reelIndex < f_Slots->pressCount) {
            if (f_Slots->stoppedSymbols[reelIndex] == NO_SYMBOL) {
                if (reelIndex == LAST_REEL) {
                    f_TickTime++;
                }

                if (f_Slots->reelAngles[reelIndex] < SYMBOL_ANGLE) {
                    f_Slots->reelAngles[reelIndex] += STOPPING_REEL_SPEED;
                    if (f_Slots->reelAngles[reelIndex] > SYMBOL_ANGLE) {
                        s32 symbolIndex;
                        f_Slots->reelAngles[reelIndex] -= SYMBOL_ANGLE;
                        f_Slots->reelPositions[reelIndex]++;
                        if (f_Slots->reelPositions[reelIndex] >= REEL_SYMBOL_COUNT) {
                            f_Slots->reelPositions[reelIndex] = 0;
                        }

                        symbolIndex = f_Slots->reelPositions[reelIndex] - SHOWN_SYMBOL_OFFSET;
                        if (symbolIndex < 0) {
                            symbolIndex += REEL_SYMBOL_COUNT;
                        }

                        // The last reel stops on a star only after two others
                        if (reelIndex == LAST_REEL) {
                            if (f_Slots->stoppedReelCount == reelIndex) {
                                if (f_Slots->stoppedSymbols[0] == SYMBOL_STAR &&
                                    f_Slots->stoppedSymbols[1] == SYMBOL_STAR) {
                                    if (f_ReelStrips[reelIndex][symbolIndex] == SYMBOL_STAR) {
                                        f_Slots->stoppedSymbols[LAST_REEL] = SYMBOL_STAR;
                                        f_Slots->stoppedReelCount++;
                                    }
                                }
                                else if (f_ReelStrips[reelIndex][symbolIndex] != SYMBOL_STAR) {
                                    f_Slots->stoppedSymbols[reelIndex] =
                                        f_ReelStrips[reelIndex][symbolIndex];
                                    f_Slots->stoppedReelCount++;
                                }
                            }
                        }
                        else {
                            f_Slots->stoppedSymbols[reelIndex] =
                                f_ReelStrips[reelIndex][symbolIndex];
                            f_Slots->stoppedReelCount++;
                        }

                        f_TickTime = 0;
                    }
                }
            }
        }
        else {
            if (reelIndex == LAST_REEL) {
                f_TickTime += 2;
            }

            f_Slots->reelAngles[reelIndex] += f_Slots->reelSpeeds[reelIndex];
            f_Slots->reelSpeeds[reelIndex] +=
                DegreesToRadians((REEL_COUNT - reelIndex) * REEL_SPEED_STEP);
            if (f_Slots->reelSpeeds[reelIndex] > REEL_SPEED_MAX) {
                f_Slots->reelSpeeds[reelIndex] = REEL_SPEED_MAX;
            }

            if (f_Slots->reelAngles[reelIndex] > SYMBOL_ANGLE) {
                f_Slots->reelAngles[reelIndex] -= SYMBOL_ANGLE;
                f_Slots->reelPositions[reelIndex]++;
                if (f_Slots->reelPositions[reelIndex] >= REEL_SYMBOL_COUNT) {
                    f_Slots->reelPositions[reelIndex] = 0;
                }
            }
        }

        rotation[AXIS_X] = f_Slots->reelAngles[reelIndex];
        rotation[AXIS_Y] = 0;
        rotation[AXIS_Z] = 0;
        translation[AXIS_X] = (reelIndex * REEL_SPACING) + FIRST_REEL_X;
        translation[AXIS_Y] = SLOTS_Y;
        translation[AXIS_Z] = SLOTS_Z;
        scale[AXIS_X] = REEL_SCALE;
        scale[AXIS_Y] = REEL_SCALE;
        scale[AXIS_Z] = REEL_SCALE;
        BuildTransformMatrix(matrix, rotation, translation, scale);
        ConvertMatrixToFixed(matrix, &f_ReelMatrices[reelIndex][g_MatrixBufferIndex]);
    }

    gDPPipeSync(gfx++);
    f_TickTime %= REEL_TICK_TIME;
    if (f_Slots->stoppedReelCount < REEL_COUNT && f_TickTime == 0) {
        PlaySound(SOUND_TICK, REEL_CLICK_CHANNEL, SOUND_FULL_SCALE);
    }

    rotation[AXIS_X] = 0;
    rotation[AXIS_Y] = 0;
    rotation[AXIS_Z] = 0;
    translation[AXIS_X] = 0.0f;
    translation[AXIS_Y] = SLOTS_Y;
    translation[AXIS_Z] = SLOTS_Z;
    scale[AXIS_X] = 1.0f;
    scale[AXIS_Y] = 1.0f;
    scale[AXIS_Z] = 1.0f;
    BuildTransformMatrix(matrix, rotation, translation, scale);
    ConvertMatrixToFixed(matrix, &f_ShadeMatrix);

    // The black behind the reels
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gSPLoadGeometryMode(gfx++, 0);
    gSPSetGeometryMode(gfx++, G_SHADE | G_SHADING_SMOOTH);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(REEL_WINDOW_LEFT),
        QuarterPixels(REEL_WINDOW_TOP),
        QuarterPixels(REEL_WINDOW_RIGHT),
        QuarterPixels(REEL_WINDOW_BOTTOM),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );

    // The reels
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_PERSP);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gSPMatrix(
        gfx++,
        PhysicalAddress(&g_ScreenOrthoMatrix),
        G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION
    );
    gDPPipeSync(gfx++);
    for (reelIndex = 0; reelIndex != REEL_COUNT; reelIndex++) {
        gSPMatrix(
            gfx++,
            PhysicalAddress(&f_ReelMatrices[reelIndex][g_MatrixBufferIndex]),
            G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
        );
        gSPVertex(gfx++, f_Slots->reelVertices, REEL_VERTEX_COUNT, 0);
        for (quadIndex = 0; quadIndex != REEL_QUAD_COUNT; quadIndex++) {
            s32 symbolIndex;
            symbolIndex = f_Slots->reelPositions[reelIndex] - quadIndex;
            if (symbolIndex >= REEL_SYMBOL_COUNT / 2) {
                symbolIndex -= REEL_SYMBOL_COUNT;
            }

            if (symbolIndex < 0) {
                symbolIndex += REEL_SYMBOL_COUNT;
            }

            LoadTextureBlock(
                &gfx,
                ImageBodyAt(
                    f_Slots->symbolImages,
                    f_ReelStrips[reelIndex][symbolIndex] * SYMBOL_IMAGE_SIZE
                ),
                G_IM_FMT_RGBA,
                G_IM_SIZ_16b,
                SYMBOL_SIZE,
                SYMBOL_SIZE,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPModifyVertex(gfx++, quadIndex * 2, G_MWO_POINT_ST, 0);
            gSPModifyVertex(
                gfx++,
                (quadIndex * 2) + 1,
                G_MWO_POINT_ST,
                TextureCoordinateWord(SYMBOL_ST_END, 0)
            );
            gSPModifyVertex(gfx++, (quadIndex * 2) + 2, G_MWO_POINT_ST, SYMBOL_ST_END);
            gSPModifyVertex(
                gfx++,
                (quadIndex * 2) + 3,
                G_MWO_POINT_ST,
                TextureCoordinateWord(SYMBOL_ST_END, SYMBOL_ST_END)
            );
            gSP2Triangles(
                gfx++,
                (quadIndex * 2) + 1,
                quadIndex * 2,
                (quadIndex * 2) + 2,
                0,
                (quadIndex * 2) + 1,
                (quadIndex * 2) + 2,
                (quadIndex * 2) + 3,
                0
            );
            gDPPipeSync(gfx++);
        }
    }

    // The shading over the reels' edges and the line across their middle
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_SHADE, G_CC_SHADE);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPPipeSync(gfx++);
    gSPMatrix(gfx++, PhysicalAddress(&f_ShadeMatrix), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPVertex(gfx++, f_Slots->shadeVertices, SHADE_VERTEX_COUNT, 0);
    gSP2Triangles(gfx++, 0, 1, 2, 0, 0, 2, 3, 0);
    gSP2Triangles(gfx++, 4, 5, 6, 0, 4, 6, 7, 0);
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    gDPSetPrimColor(gfx++, 0, 0, 0, 0, 0, OPAQUE_ALPHA);
    gDPPipeSync(gfx++);
    DrawTextureRectangle(
        &gfx,
        QuarterPixels(REEL_WINDOW_LEFT),
        QuarterPixels(REEL_CENTER_LINE_TOP),
        QuarterPixels(REEL_WINDOW_RIGHT),
        QuarterPixels(REEL_CENTER_LINE_TOP + 1),
        G_TX_RENDERTILE,
        0,
        0,
        TEXEL_STEP_ONE,
        TEXEL_STEP_ONE
    );

    // The slot machine
    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_COPY);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_THRESHOLD);
    gDPSetBlendColor(gfx++, 255, 255, 255, 1);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    LoadLargePalette(&gfx, ImagePalette(f_Slots->machineImage));
    texelOffset = LARGE_PALETTED_TEXELS_OFFSET;
    for (topOrPlayerIndex = SCREEN_BORDER; topOrPlayerIndex < MACHINE_BOTTOM;
        topOrPlayerIndex += MACHINE_STRIP_HEIGHT) {
        stripHeight = (topOrPlayerIndex + MACHINE_STRIP_HEIGHT <= MACHINE_BOTTOM) ?
            MACHINE_STRIP_HEIGHT :
            MACHINE_LAST_STRIP_HEIGHT;
        if (!isStripLoaded) {
            LoadTextureBlock(
                &gfx,
                f_Slots->machineImage + texelOffset,
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                MACHINE_WIDTH,
                stripHeight,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            isStripLoaded = TRUE;
        }
        else {
            // The tiles stay as the first strip set them
            gDPSetTextureImage(
                gfx++,
                G_IM_FMT_CI,
                G_IM_SIZ_16b,
                1,
                TextureAddress(f_Slots->machineImage + texelOffset)
            );
            gDPLoadSync(gfx++);
            gDPLoadBlock(
                gfx++,
                G_TX_LOADTILE,
                0,
                0,
                ((stripHeight * MACHINE_WIDTH + G_IM_SIZ_8b_INCR) >> G_IM_SIZ_8b_SHIFT) - 1,
                CALC_DXT(MACHINE_WIDTH, G_IM_SIZ_8b_BYTES)
            );
        }

        gDPPipeSync(gfx++);
        DrawTextureRectangle(
            &gfx,
            QuarterPixels(SCREEN_BORDER),
            QuarterPixels(topOrPlayerIndex),
            QuarterPixels(SCREEN_BORDER + MACHINE_WIDTH - 1),
            QuarterPixels(topOrPlayerIndex + stripHeight - 1),
            G_TX_RENDERTILE,
            0,
            0,
            COPY_MODE_TEXEL_STEP,
            TEXEL_STEP_ONE
        );
        gDPPipeSync(gfx++);
        texelOffset += MACHINE_STRIP_HEIGHT * MACHINE_WIDTH;
    }

    // The stop buttons pushed so far
    LoadLargePalette(&gfx, ImagePalette(f_Slots->buttonsImage));
    for (buttonOrStartStripIndex = 0; buttonOrStartStripIndex != REEL_COUNT;
        buttonOrStartStripIndex++) {
        if (buttonOrStartStripIndex > f_Slots->pressCount - 1) {
            break;
        }

        for (stripIndex = 0; stripIndex != BUTTON_STRIP_COUNT; stripIndex++) {
            LoadTextureBlock(
                &gfx,
                f_Slots->buttonsImage +
                    (LARGE_PALETTED_TEXELS_OFFSET +
                        (stripIndex * BUTTON_STRIP_HEIGHT * BUTTON_WIDTH) +
                        (buttonOrStartStripIndex * BUTTON_HEIGHT * BUTTON_WIDTH)),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                BUTTON_WIDTH,
                BUTTON_STRIP_HEIGHT,
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
                QuarterPixels((buttonOrStartStripIndex * BUTTON_WIDTH) + BUTTON_LEFT),
                QuarterPixels(BUTTON_TOP + (stripIndex * BUTTON_STRIP_HEIGHT)),
                QuarterPixels(
                    BUTTON_LEFT + BUTTON_SHOWN_WIDTH - 1 + (buttonOrStartStripIndex * BUTTON_WIDTH)
                ),
                QuarterPixels(
                    BUTTON_TOP + BUTTON_STRIP_HEIGHT - 1 + (stripIndex * BUTTON_STRIP_HEIGHT)
                ),
                G_TX_RENDERTILE,
                0,
                0,
                COPY_MODE_TEXEL_STEP,
                TEXEL_STEP_ONE
            );
            gDPPipeSync(gfx++);
        }
    }

    gDPPipeSync(gfx++);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);

    // START!, growing while the reels speed up
    halfWidth = (sinf(f_StartAngle) * 64.0f) + 20.0f;
    startStripHeight = ((sinf(f_StartAngle) * 50.0f) + 13.75f) * 0.2f;
    dsdx = (START_WIDTH * TEXEL_STEP_ONE / 2) / halfWidth;
    halfHeight = (startStripHeight * 2.5f) + 2.0f;
    dtdy = (START_SHOWN_HEIGHT * TEXEL_STEP_ONE / 2) / halfHeight;
    if (f_ClubSlotsState == CLUB_SLOTS_STARTING) {
        f_StartAngle = 0.0f;
    }

    if (f_ClubSlotsState == CLUB_SLOTS_SPEEDING) {
        f_StartAngle += START_ANGLE_STEP;
        if (f_StartAngle > START_ANGLE_MAX) {
            f_StartAngle = START_ANGLE_MAX;
        }

        gDPPipeSync(gfx++);
        LoadLargePalette(&gfx, ImagePalette(f_Slots->startImage));
        texelOffset = LARGE_PALETTED_TEXELS_OFFSET;
        for (buttonOrStartStripIndex = 0; buttonOrStartStripIndex != START_STRIP_COUNT;
            buttonOrStartStripIndex++) {
            loadedStripHeight = (buttonOrStartStripIndex != START_STRIP_COUNT - 1) ?
                START_STRIP_HEIGHT :
                START_LAST_STRIP_HEIGHT;
            gDPLoadTextureTile(
                gfx++,
                TextureAddress(f_Slots->startImage + texelOffset),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                START_WIDTH,
                loadedStripHeight,
                0,
                0,
                START_WIDTH - 1,
                loadedStripHeight - 1,
                0,
                G_TX_WRAP,
                G_TX_WRAP,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(
                gfx++,
                QuarterPixels((s32)((SCREEN_WIDTH / 2) - halfWidth)),
                QuarterPixels((s32)(((SCREEN_HEIGHT / 2) - halfHeight) +
                    (buttonOrStartStripIndex * startStripHeight))),
                QuarterPixels((s32)(halfWidth + (SCREEN_WIDTH / 2))),
                QuarterPixels((s32)(((SCREEN_HEIGHT / 2) - halfHeight) +
                    (buttonOrStartStripIndex * startStripHeight) + startStripHeight)),
                G_TX_RENDERTILE,
                0,
                TexelCoordinate(1),
                dsdx,
                dtdy
            );
            texelOffset += START_STRIP_SPACING * START_WIDTH;
        }
    }

    // The players' faces
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    for (topOrPlayerIndex = 0; topOrPlayerIndex != g_GameState.playerCount; topOrPlayerIndex++) {
        LoadLargePalette(&gfx, g_PlayerFacePalettes[topOrPlayerIndex]);
        if (topOrPlayerIndex == f_Slots->playerIndex) {
            ImageAsset *faceImage = f_Slots->faceImages[topOrPlayerIndex][f_FaceBufferIndex];

            LoadTextureBlock(
                &gfx,
                faceImage,
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FACE_FRAME_SIZE,
                FACE_FRAME_SIZE,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(FACE_LEFT),
                QuarterPixels(FACE_TOP + (topOrPlayerIndex * FACE_SPACING)),
                QuarterPixels(FACE_LEFT + FACE_FRAME_SIZE),
                QuarterPixels(FACE_TOP + FACE_FRAME_SIZE + (topOrPlayerIndex * FACE_SPACING)),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            if (f_IsShowingResult == TRUE && ++f_FaceFrameTime >= FACE_FRAME_DURATION) {
                f_FaceFrameTime = 0;
                if (++f_FaceFrame >= FACE_LAST_FRAME + 1) {
                    if (++f_FaceHoldCount >= FACE_HOLD_COUNT) {
                        row = f_CharacterFaceRows[g_Players[topOrPlayerIndex].characterIndex];
                        if (f_StarCount == REEL_COUNT) {
                            OpenEncodedAsset(g_FaceAnimationAssets[row][FACE_LAST_FRAME], &asset);
                            ReadEncodedAsset(f_Slots->faceImages[topOrPlayerIndex][0], &asset);
                            OpenEncodedAsset(g_FaceAnimationAssets[row][FACE_LAST_FRAME], &asset);
                            ReadEncodedAsset(f_Slots->faceImages[topOrPlayerIndex][1], &asset);
                        }
                        else {
                            OpenEncodedAsset(
                                g_AlternateFaceAnimationAssets[row][FACE_LAST_FRAME],
                                &asset
                            );
                            ReadEncodedAsset(f_Slots->faceImages[topOrPlayerIndex][0], &asset);
                            OpenEncodedAsset(
                                g_AlternateFaceAnimationAssets[row][FACE_LAST_FRAME],
                                &asset
                            );
                            ReadEncodedAsset(f_Slots->faceImages[topOrPlayerIndex][1], &asset);
                        }

                        // f_FaceBufferIndex is cleared twice
                        f_FaceBufferIndex = 0;
                        f_StarCount = 0;
                        f_IsShowingResult = FALSE;
                        f_FaceFrameTime = 0;
                        f_FaceFrame = 0;
                        f_FaceBufferIndex = 0;
                        f_FaceHoldCount = 0;
                        f_Slots->playerIndex++;
                    }
                }
                else {
                    row = f_CharacterFaceRows[g_Players[topOrPlayerIndex].characterIndex];
                    if (f_StarCount == REEL_COUNT) {
                        OpenEncodedAsset(g_FaceAnimationAssets[row][f_FaceFrame], &asset);
                        ReadEncodedAsset(
                            f_Slots->faceImages[topOrPlayerIndex][f_FaceBufferIndex ^ 1],
                            &asset
                        );
                    }
                    else {
                        OpenEncodedAsset(g_AlternateFaceAnimationAssets[row][f_FaceFrame], &asset);
                        ReadEncodedAsset(
                            f_Slots->faceImages[topOrPlayerIndex][f_FaceBufferIndex ^ 1],
                            &asset
                        );
                    }

                    f_FaceBufferIndex ^= 1;
                }
            }
        }
        else {
            ImageAsset *faceImage = g_PlayerFaceImages[topOrPlayerIndex];

            LoadTextureBlock(
                &gfx,
                faceImage,
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                FACE_WIDTH,
                FACE_FRAME_SIZE,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPSetPrimColor(gfx++, 0, 0, 127, 127, 127, 127);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(OTHER_FACE_LEFT),
                QuarterPixels(OTHER_FACE_TOP + (topOrPlayerIndex * FACE_SPACING)),
                QuarterPixels(OTHER_FACE_LEFT + FACE_WIDTH),
                QuarterPixels(OTHER_FACE_TOP + FACE_HEIGHT + (topOrPlayerIndex * FACE_SPACING)),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    DrawClubSlotsLogo(&gfx, LOGO_X, LOGO_Y, 1.0f);
    *displayListHead = gfx;
}

// Draws the logo centered there, bobbing until the result, and its shine
void DrawClubSlotsLogo(Gfx **displayListHead, s32 centerX, s32 centerY, f32 scale) {
    Gfx *gfx;
    s32 starCount;
    s32 symbolIndex;
    // Counts the reels, then the strips
    s32 index;
    s32 shineTime;
    f32 width;
    f32 stripHeight;
    f32 halfWidth;
    f32 halfHeight;
    f32 dsdx;
    f32 dtdy;
    f32 shineWidth;
    f32 shineHeight;
    f32 shineHalfWidth;
    f32 shineHalfHeight;
    f32 shineDsdx;
    f32 shineDtdy;

    gfx = *displayListHead;
    starCount = 0;
    if (f_Slots->pressCount == LAST_REEL) {
        for (index = 0; index != LAST_REEL; index++) {
            symbolIndex = f_Slots->reelPositions[index] - SHOWN_SYMBOL_OFFSET;
            if (symbolIndex < 0) {
                symbolIndex += REEL_SYMBOL_COUNT;
            }

            starCount += f_ReelStrips[index][symbolIndex] == SYMBOL_STAR;
        }

        if (IsGameFlagSet(GAME_FLAG_OPTIONS_SOUND_TEST)) {
            starCount = LAST_REEL;
        }
    }

    if (f_IsShowingResult == FALSE) {
        if (starCount == LAST_REEL) {
            f_LogoBobAngle += LOGO_BOB_STEP;
            f_LogoBobOffset = sinf(f_LogoBobAngle) * LOGO_BOB_HEIGHT;
        }
        else {
            f_LogoBobAngle += LOGO_BOB_STEP;
            f_LogoBobOffset = sinf(f_LogoBobAngle) * LOGO_BOB_HEIGHT;
            f_LogoUnusedWord = 0;
        }
    }
    else {
        f_LogoUnusedWord = 0;
        f_LogoBobAngle = 0.0f;
        f_LogoBobOffset = 0.0f;
    }

    width = scale * LOGO_WIDTH;
    stripHeight = scale * LOGO_STRIP_HEIGHT;
    halfWidth = scale * (LOGO_WIDTH / 2);
    halfHeight = scale * (LOGO_HEIGHT / 2);
    if (width != 0.0f) {
        dsdx = (LOGO_WIDTH * TEXEL_STEP_ONE) / width;
    }
    else {
        dsdx = 0.0f;
    }

    if (stripHeight != 0.0f) {
        dtdy = (LOGO_STRIP_HEIGHT * TEXEL_STEP_ONE) / stripHeight;
    }
    else {
        dtdy = 0.0f;
    }

    shineWidth = scale * LOGO_WIDTH;
    shineHeight = scale * LOGO_HEIGHT;
    shineHalfWidth = scale * (LOGO_WIDTH / 2);
    shineHalfHeight = scale * (LOGO_HEIGHT / 2);
    if (shineWidth != 0.0f) {
        shineDsdx = (SHINE_SIZE * TEXEL_STEP_ONE) / shineWidth;
    }
    else {
        shineDsdx = 0.0f;
    }

    if (shineHeight != 0.0f) {
        shineDtdy = (SHINE_SIZE * TEXEL_STEP_ONE) / shineHeight;
    }
    else {
        shineDtdy = 0.0f;
    }

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoImage)));
    for (index = 0; index != LOGO_STRIP_COUNT; index++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_LogoImage) + (index * LOGO_STRIP_HEIGHT * LOGO_WIDTH)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            LOGO_WIDTH,
            LOGO_STRIP_HEIGHT,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels((s32)(centerX - halfWidth)),
            QuarterPixels((s32)((centerY - halfHeight) + (index * stripHeight) + f_LogoBobOffset)),
            QuarterPixels((s32)(centerX + halfWidth)),
            QuarterPixels((s32)((centerY - halfHeight) + (index * stripHeight) + stripHeight +
                f_LogoBobOffset)),
            G_TX_RENDERTILE,
            0,
            0,
            (s32)dsdx,
            (s32)dtdy
        );
        gDPPipeSync(gfx++);
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PRIM);
    gDPSetPrimDepth(gfx++, LOGO_PRIM_DEPTH, 0);
    gDPSetCombineLERP(gfx++, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1, 0, 0, 0, TEXEL0, 0, 1, TEXEL0, 1);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(
        gfx++,
        GBL_c1(G_BL_CLR_MEM, G_BL_A_IN, G_BL_CLR_MEM, G_BL_A_MEM),
        G_RM_AA_ZB_TEX_EDGE2
    );
    gDPLoadTLUT_pal256(gfx++, TextureAddress(ImagePalette(f_LogoImage)));
    for (index = 0; index != LOGO_STRIP_COUNT; index++) {
        gDPLoadTextureBlock(
            gfx++,
            TextureAddress(
                LargeImageTexels(f_LogoImage) + (index * LOGO_STRIP_HEIGHT * LOGO_WIDTH)
            ),
            G_IM_FMT_CI,
            G_IM_SIZ_8b,
            LOGO_WIDTH,
            LOGO_STRIP_HEIGHT,
            0,
            G_TX_WRAP,
            G_TX_WRAP,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels((s32)(centerX - halfWidth)),
            QuarterPixels((s32)((centerY - halfHeight) + (index * stripHeight) + f_LogoBobOffset)),
            QuarterPixels((s32)(centerX + halfWidth)),
            QuarterPixels((s32)((centerY - halfHeight) + (index * stripHeight) + stripHeight +
                f_LogoBobOffset)),
            G_TX_RENDERTILE,
            0,
            0,
            (s32)dsdx,
            (s32)dtdy
        );
        gDPPipeSync(gfx++);
    }

    gDPSetPrimDepth(gfx++, SHINE_PRIM_DEPTH, 0);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_ZB_XLU_SURF, G_RM_ZB_XLU_SURF2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPPipeSync(gfx++);
    gDPSetAlphaDither(gfx++, G_AD_PATTERN);
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
    gDPPipeSync(gfx++);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);
    gDPSetPrimColor(gfx++, 0, 0, 255, 255, 255, OPAQUE_ALPHA);
    LoadTextureBlock(
        &gfx,
        ImageBody(f_ShineImage),
        G_IM_FMT_I,
        G_IM_SIZ_8b,
        SHINE_SIZE,
        SHINE_SIZE,
        0,
        0,
        0,
        G_TX_NOMASK,
        G_TX_NOMASK,
        G_TX_NOLOD,
        G_TX_NOLOD
    );
    gDPPipeSync(gfx++);
    shineTime = f_ShineTime++;
    if (f_ShineTime >= SHINE_DELAY) {
        gSPScisTextureRectangle(
            gfx++,
            QuarterPixels((s32)(centerX - shineHalfWidth)),
            QuarterPixels((s32)((centerY - shineHalfHeight) + f_LogoBobOffset)),
            QuarterPixels((s32)(centerX + shineHalfWidth)),
            QuarterPixels((s32)(centerY + shineHalfHeight + f_LogoBobOffset)),
            G_TX_RENDERTILE,
            TexelCoordinate(SHINE_SIZE - ((shineTime + 1 - SHINE_START_TIME) * SHINE_SPEED)),
            0,
            (s32)shineDsdx,
            (s32)shineDtdy
        );
    }

    gDPPipeSync(gfx++);
    gDPSetDepthSource(gfx++, G_ZS_PIXEL);
    *displayListHead = gfx;
}

void DoNothingInClubSlots(void) {
}
