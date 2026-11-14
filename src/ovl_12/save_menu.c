#include "save_menu.h"

#include <framebuffer_count.h>
#include <ultra64.h>

#define OMITS_MEASURE_TEXT_FLAGS

#include "character.h"
#include "controllers/controller.h"
#include "course.h"
#include "display_list.h"
#include "game_state.h"
#include "math/math_util.h"
#include "menu.h"
#include "pause_menu.h"
#include "play.h"
#include "play_record.h"
#include "player.h"
#include "round_hud.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "screen_layers.h"
#include "sky_and_hud_animations.h"
#include "sound.h"
#include "system/asset.h"
#include "system/c_language.h"
#include "system/compiler.h"
#include "system/heap.h"
#include "system/memory_address.h"
#include "text.h"
#include "text_format.h"

enum SaveMenuState {
    SAVE_MENU_STATE_OPENING,
    SAVE_MENU_STATE_CHOOSING,
    SAVE_MENU_STATE_BACKING_OUT,
    SAVE_MENU_STATE_BACKED_OUT,
    SAVE_MENU_STATE_SAVED,
    SAVE_MENU_STATE_LEAVING
};

typedef s32 SaveMenuState;

#define ICON_FRAME_PERIOD 5

#define GAME_MODE_ICON_COUNT 12
#define MODE_NAME_COUNT 7

#define ASSET_CONTROLS_PICTURE 0x92A
#define ASSET_MINI_GOLF_CONTROLS_PICTURE 0x96A
#define CONTROLS_PICTURE_WIDTH 288
#define CONTROLS_PICTURE_LEFT ((SCREEN_WIDTH - CONTROLS_PICTURE_WIDTH) / 2)
#define CONTROLS_STRIP_HEIGHT 7
#define CONTROLS_STRIP_COUNT 32

#define MENU_SOUND_REVERB 0x14

#define SLIDE_FRAMES 14
#define SLIDE_SOUND_FRAME 3
#define SLIDE_OUT_START_OFFSET (-1)
#define SLIDE_OUT_START_STEP 4

#define FACE_IMAGE_COUNT (SAVED_GAME_COUNT * PLAYER_COUNT_MAX)
#define REPLAY_LABEL_COUNT 6

#define SAVE_MENU_SONG_VOLUME 0x60

#define ASSET_SAVE_MENU_TITLE_IMAGE 0x6A5
#define MESSAGE_CENTER_X (SCREEN_WIDTH / 2)
#define MESSAGE_CENTER_Y 114
#define NO_TEXT (-1)

#define END_GAME_ROW SAVED_GAME_COUNT
#define SKIP_REPLAY_ROW RING_SHOT_RECORD_COUNT
#define SLIDE_ROW_COUNT 4
#define SAVED_WAIT_FRAMES 15

#define GAME_ROW_COUNT (SAVED_GAME_COUNT + 1)
#define FIRST_GAME_ROW_TOP 50
#define GAME_ROW_SPACING 47
#define GAME_PANEL_STRIP_HEIGHT 15
#define GAME_PANEL_MIDDLE 72
#define END_GAME_PANEL_MIDDLE 64
#define END_GAME_PANEL_STRIP_HEIGHT 9
#define TITLE_IMAGE_WIDTH 160
#define TITLE_STRIP_COUNT 3
#define TITLE_STRIP_HEIGHT 10
#define TITLE_STRIP_SIZE (TITLE_IMAGE_WIDTH * TITLE_STRIP_HEIGHT)
#define TITLE_STRIP_DRAWN_HEIGHT 12
#define TITLE_LAST_STRIP_HEIGHT 7
#define TITLE_IMAGE_LEFT 70
#define TITLE_SWAY_LEFT 10.0f
#define TITLE_SWAY_TOP 18.0f
#define TITLE_SWAY_DISTANCE 5.0f
#define TEXEL_COORDINATE_ONE 32
#define SWAY_DEGREES_PER_FRAME 10
#define MODE_ICON_SIZE 64
#define MODE_ICON_PART_HEIGHT 32
#define MODE_ICON_PART_COUNT 2
#define MODE_ICON_LEFT 26
#define MODE_ICON_TOP 54
#define HALF_SIZE_TEXEL_STEP (2 * TEXEL_STEP_ONE)
#define GOLFERS_LEFT 240
#define GOLFER_SPACING 26
#define GOLFER_TOP 52
#define MODE_TEXT_LEFT 62
#define MODE_TEXT_TOP 57
#define COURSE_TEXT_TOP 74
#define GAME_HOLE_TEXT_LEFT 240
#define GAME_HOLE_TEXT_TOP 78
#define EMPTY_GAME_TEXT_TOP 64
#define END_GAME_TEXT_LEFT 116
#define END_GAME_TEXT_TOP 199
#define CURSOR_ALPHA_MIDDLE 127
#define CURSOR_ALPHA_SWING 64.0f
#define MINI_GOLF_GREEN 0
#define MINI_GOLF_SLOW 1
#define MINI_GOLF_FAST 2
#define LUIGIS_GARDEN_COURSE_INDEX MINI_GOLF_COURSE_INDEX
#define PEACHS_CASTLE_COURSE_INDEX (MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD)

enum ModeName {
    MODE_NAME_TOURNAMENT,
    MODE_NAME_GET_CHARACTER,
    MODE_NAME_STROKE,
    MODE_NAME_MATCH_GAME,
    MODE_NAME_SKINS_MATCH,
    MODE_NAME_CLUB_SLOTS,
    MODE_NAME_MINI_GOLF
};

#define REPLAY_ROW_COUNT (RING_SHOT_RECORD_COUNT + 1)
#define FIRST_REPLAY_ROW_TOP 36
#define REPLAY_ROW_SPACING 34
#define NAME_PLATE_STRIP_SIZE (NAME_PLATE_WIDTH * NAME_PLATE_STRIP_HEIGHT)
#define WIDE_PANEL_LEFT 24
#define WIDE_PANEL_WIDTH 272
#define RECORD_PANEL_STRIP_HEIGHT 11
#define NARROW_PANEL_LEFT 80
#define NARROW_PANEL_WIDTH 160
#define HOLE_OUT_PANEL_STRIP_HEIGHT 8
#define WIDE_PANEL_HALF_WIDTH (WIDE_PANEL_WIDTH / 2)
#define NARROW_PANEL_HALF_WIDTH (NARROW_PANEL_WIDTH / 2)
#define RECORD_PANEL_HEIGHT (RECORD_PANEL_STRIP_HEIGHT * NAME_PLATE_STRIP_COUNT)
#define HOLE_OUT_PANEL_HEIGHT (HOLE_OUT_PANEL_STRIP_HEIGHT * NAME_PLATE_STRIP_COUNT)
#define TITLE_PANEL_LEFT 96
#define TITLE_PANEL_TOP 13
#define TITLE_PANEL_WIDTH 128
#define TITLE_PANEL_STRIP_HEIGHT 9
#define TITLE_TEXT_LEFT 120
#define TITLE_TEXT_TOP 18
#define TITLE_BOB_HEIGHT 4.0f
#define TITLE_BOB_DEGREES_PER_FRAME 10
#define CHOSEN_PANEL_SHADE 0xEF
#define OTHER_PANEL_SHADE 0xBF
#define CURSOR_FRAME_THICKNESS 2
#define COURSE_DIGIT_COUNT 6
#define COURSE_WORD_TEXEL_LEFT (COURSE_DIGIT_COUNT * COURSE_DIGIT_WIDTH)
#define COURSE_WORD_LEFT 64
#define COURSE_TOP 36
#define LABEL_LEFT 128
#define LABEL_TOP 38
#define CHARACTER_LEFT 32
#define CHARACTER_TOP 40
#define HOLE_TEXT_LEFT 64
#define HOLE_TEXT_TOP 53
#define EMPTY_TEXT_TOP 47
#define HOLE_OUT_TEXT_LEFT 128
#define HOLE_OUT_TEXT_TOP 212
#define FIRST_RECORD_TEXT_SLOT 2

enum ReplayLabel {
    NO_REPLAY_LABEL = -1,
    REPLAY_LABEL_ALBATROSS,
    REPLAY_LABEL_HOLE_IN_ONE,
    REPLAY_LABEL_EAGLE,
    REPLAY_LABEL_BIRDIE,
    REPLAY_LABEL_PAR
};

typedef s32 ReplayLabel;

static s8 f_ModeIcons[GAME_MODE_ICON_COUNT] = {
    MODE_ICON_STROKE,
    0,
    MODE_ICON_MATCH_GAME,
    0,
    MODE_ICON_GET_CHARACTER,
    MODE_ICON_TOURNAMENT,
    MODE_ICON_MINI_GOLF,
    3,
    0,
    0,
    MODE_ICON_SKINS_MATCH,
    MODE_ICON_CLUB_SLOTS,
};
static SaveMenuList f_MenuKind = SAVE_MENU_CLOSED;
static Boolean f_IsControlsShown = FALSE;
static s32 f_CursorIndex = 0;
static s32 f_StateFrameCount = 0;
static SaveMenuState f_MenuState = SAVE_MENU_STATE_OPENING;
static char *f_ModeNames[MODE_NAME_COUNT] = {
    "Tournament",
    "Get Character",
    "Stroke",
    "Match Game",
    "Skins Match",
    "Club Slots",
    "Mini-Golf",
};
static char f_NoSavedGameText[] = "There is no saved data.";
static char f_NoReplayText[] = "There is no replay data.";
static char f_EndGameText[] = "End Game";
static char f_SaveMenuText[] = "Save Menu";
static s32 f_IconImageIndex = 0;
static s32 f_UnusedSaveMenuWord UNUSED = 0;
static SongId f_ResumedSongId = NO_SONG;
static s16 f_ReplayLabelIds[REPLAY_LABEL_COUNT] = {0x697, 0x698, 0x699, 0x69A, 0x69B, 0x69C};
static s32 f_IconFrameTime = 0;
static Radians f_PanelWaveAngle = 0.0f;
static Radians f_TitleSwayAngle = 0.0f;
static Radians f_CursorPulseAngle = 0.0f;
static Radians f_ReplayTitleBobAngle = 0.0f;

static s32 f_SlideOffsets[4];
static u8 f_UnusedBytesAfterSlideOffsets[8] UNUSED;
static PaletteColor *f_FacePalettes[FACE_IMAGE_COUNT];
static ImageAsset *f_FaceImages[FACE_IMAGE_COUNT];
static ImageAsset *f_ModeIconImages[SAVED_GAME_COUNT][2];
static ImageAsset *f_NamePlateImage;
static ImageAsset *f_ReplayLabelImages[REPLAY_LABEL_COUNT];
static ImageAsset *f_CourseNumbersImage;
static ImageAsset *f_SaveMenuTitleImage;
static s32 f_IconFrames[SAVED_GAME_COUNT];

s32 GetModeIconIndex(GameState *state) {
    s32 iconIndex;

    if (state->isMiniGolf) {
        iconIndex = MODE_ICON_MINI_GOLF;
    }
    else {
        switch (state->gameMode) {
            case GAME_MODE_TOURNAMENT:
                iconIndex = MODE_ICON_TOURNAMENT;
                break;
            case GAME_MODE_GET_CHARACTER:
                iconIndex = MODE_ICON_GET_CHARACTER;
                break;
            case GAME_MODE_STROKE:
                iconIndex = MODE_ICON_STROKE;
                break;
            case GAME_MODE_MATCH_GAME:
                iconIndex = MODE_ICON_MATCH_GAME;
                break;
            case GAME_MODE_SKINS_MATCH:
                iconIndex = MODE_ICON_SKINS_MATCH;
                break;
            case GAME_MODE_CLUB_SLOTS:
                iconIndex = MODE_ICON_CLUB_SLOTS;
                break;
            default:
                iconIndex = NO_MODE_ICON;
                break;
        }
    }

    return iconIndex;
}

void OpenSaveMenu(SaveMenuList kind) {
    EncodedAsset encodedAsset;
    AssetFile file;
    SaveData *save;
    s32 slot;
    s32 playerIndex;
    s32 iconIndex;
    RingShotRecord *record;
    Player *player;
    GameState *state;
    s32 fileSize;

    save = GetSaveData();
    if (kind == SAVE_MENU_GAMES) {
        InvalidateBrokenSavedGames();
    }
    else {
        InvalidateBrokenRingShotRecords();
    }

    if (kind == SAVE_MENU_GAMES) {
        SetSongVolume(SAVE_MENU_SONG_VOLUME);
    }

    ReadEncodedAsset(
        f_NamePlateImage = AllocMemory(OpenEncodedAsset(ASSET_NAME_PLATE, &encodedAsset)),
        &encodedAsset
    );
    ReadEncodedAsset(
        f_SaveMenuTitleImage =
            AllocMemory(OpenEncodedAsset(ASSET_SAVE_MENU_TITLE_IMAGE, &encodedAsset)),
        &encodedAsset
    );
    PlaySoundWithEffects(
        SOUND_PANEL_OPEN,
        g_NextSoundChannel,
        MENU_VOLUME_MEDIUM,
        MENU_SOUND_REVERB,
        SOUND_PAN_RIGHT
    );
    for (slot = 0; slot != FACE_IMAGE_COUNT; slot++) {
        ReadEncodedAsset(
            f_FaceImages[slot] = AllocMemory(OpenEncodedAsset(
                g_CharacterFirstFaceAssets[CHARACTER_PLUM] + CHARACTER_FACE_IMAGE_OFFSET,
                &encodedAsset
            )),
            &encodedAsset
        );
        OpenAssetFile(g_CharacterFirstPaletteFiles[CHARACTER_PLUM], &file);
        fileSize = GetAssetFileSize(&file);
        ReadAssetFile(f_FacePalettes[slot] = AllocMemory(fileSize), fileSize, &file);
    }

    if (kind == SAVE_MENU_GAMES) {
        s16 *defaultFrames = g_ModeIconAnimations[0];
        ImageAsset **second = &f_ModeIconImages[0][1];

        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (save->savedGames[slot].isValid &&
                (iconIndex = GetModeIconIndex(&save->savedGames[slot].state)) != NO_MODE_ICON) {
                ReadEncodedAsset(
                    f_ModeIconImages[slot][0] = AllocMemory(
                        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][0], &encodedAsset)
                    ),
                    &encodedAsset
                );
                ReadEncodedAsset(
                        *second = AllocMemory(
                            OpenEncodedAsset(g_ModeIconAnimations[iconIndex][1], &encodedAsset)
                        ),
                    &encodedAsset
                );
            }
            else {
                ReadEncodedAsset(
                    f_ModeIconImages[slot][0] =
                        AllocMemory(OpenEncodedAsset(defaultFrames[0], &encodedAsset)),
                    &encodedAsset
                );
                ReadEncodedAsset(
                        *second = AllocMemory(OpenEncodedAsset(defaultFrames[1], &encodedAsset)),
                    &encodedAsset
                );
            }

            second += 2;
        }
    }
    else {
        for (slot = 0; slot != REPLAY_LABEL_COUNT; slot++) {
            ReadEncodedAsset(
                f_ReplayLabelImages[slot] =
                    AllocMemory(OpenEncodedAsset(f_ReplayLabelIds[slot], &encodedAsset)),
                &encodedAsset
            );
        }

        ReadEncodedAsset(
            f_CourseNumbersImage =
                AllocMemory(OpenEncodedAsset(ASSET_COURSE_NUMBERS, &encodedAsset)),
            &encodedAsset
        );
    }

    if (kind == SAVE_MENU_GAMES) {
        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (save->savedGames[slot].isValid) {
                state = &save->savedGames[slot].state;
                for (playerIndex = 0; playerIndex != state->playerCount; playerIndex++) {
                    player = &save->savedPlayers[slot][playerIndex];
                    OpenEncodedAsset(
                        g_CharacterFirstFaceAssets[player->characterIndex] +
                            CHARACTER_FACE_IMAGE_OFFSET,
                        &encodedAsset
                    );
                    ReadEncodedAsset(
                        f_FaceImages[(slot * PLAYER_COUNT_MAX) + playerIndex],
                        &encodedAsset
                    );
                    OpenAssetFile(
                        g_CharacterFirstPaletteFiles[player->characterIndex] + player->paletteIndex,
                        &file
                    );
                    ReadAssetFile(
                        f_FacePalettes[(slot * PLAYER_COUNT_MAX) + playerIndex],
                        GetAssetFileSize(&file),
                        &file
                    );
                }
            }
        }
    }
    else {
        for (slot = 0; slot != RING_SHOT_RECORD_COUNT; slot++) {
            record = &save->ringShotRecords[slot];
            if (record->scoreKind & SCORE_KIND_RECORDED) {
                OpenEncodedAsset(
                    g_CharacterFirstFaceAssets[record->characterIndex] +
                        CHARACTER_FACE_IMAGE_OFFSET,
                    &encodedAsset
                );
                ReadEncodedAsset(f_FaceImages[slot], &encodedAsset);
                OpenAssetFile(
                    g_CharacterFirstPaletteFiles[record->characterIndex] + record->paletteIndex,
                    &file
                );
                ReadAssetFile(f_FacePalettes[slot], GetAssetFileSize(&file), &file);
            }
        }
    }

    f_IconImageIndex = 0;
    f_IconFrames[0] = 1;
    f_IconFrames[1] = 1;
    f_IconFrames[2] = 1;
    if (kind == SAVE_MENU_GAMES) {
        f_CursorIndex = END_GAME_ROW;
        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (!save->savedGames[slot].isValid) {
                f_CursorIndex = slot;
                break;
            }
        }
    }
    else {
        f_CursorIndex = SKIP_REPLAY_ROW;
        for (slot = 0; slot != RING_SHOT_RECORD_COUNT; slot++) {
            if (!(save->ringShotRecords[slot].scoreKind & SCORE_KIND_RECORDED)) {
                f_CursorIndex = slot;
                break;
            }
        }
    }

    f_StateFrameCount = 0;
    f_MenuState = SAVE_MENU_STATE_OPENING;
    f_SlideOffsets[0] = SCREEN_WIDTH;
    f_SlideOffsets[1] = SCREEN_WIDTH;
    f_SlideOffsets[2] = SCREEN_WIDTH;
    f_SlideOffsets[3] = SCREEN_WIDTH;
    f_MenuKind = kind;
}

void CloseSaveMenu(void) {
    s32 index;

    SetSongVolume(SOUND_FULL_SCALE);
    FreeMemory(&f_NamePlateImage);
    FreeMemory(&f_SaveMenuTitleImage);
    if (f_MenuKind == SAVE_MENU_GAMES) {
        for (index = 0; index != SAVED_GAME_COUNT; index++) {
            FreeMemory(&f_ModeIconImages[index][0]);
            FreeMemory(&f_ModeIconImages[index][1]);
        }
    }
    else {
        for (index = 0; index != REPLAY_LABEL_COUNT; index++) {
            FreeMemory(&f_ReplayLabelImages[index]);
        }

        FreeMemory(&f_CourseNumbersImage);
    }

    for (index = 0; index != FACE_IMAGE_COUNT; index++) {
        FreeMemory(&f_FaceImages[index]);
        FreeMemory(&f_FacePalettes[index]);
    }

    f_MenuKind = SAVE_MENU_CLOSED;
    SetTextSlot(
        MESSAGE_CENTER_X,
        MESSAGE_CENTER_Y,
        NO_TEXT,
        TEXT_SLOT_LARGE_FONT,
        0,
        0,
        0,
        0,
        0,
        0
    );
    SetTextSlot(
        MESSAGE_CENTER_X,
        MESSAGE_CENTER_Y,
        NO_TEXT,
        TEXT_SLOT_LARGE_FONT,
        1,
        0,
        0,
        0,
        0,
        0
    );
    SetTextSlot(
        MESSAGE_CENTER_X,
        MESSAGE_CENTER_Y,
        NO_TEXT,
        TEXT_SLOT_LARGE_FONT,
        2,
        0,
        0,
        0,
        0,
        0
    );
    SetTextSlot(
        MESSAGE_CENTER_X,
        MESSAGE_CENTER_Y,
        NO_TEXT,
        TEXT_SLOT_LARGE_FONT,
        3,
        0,
        0,
        0,
        0,
        0
    );
    SetTextSlot(
        MESSAGE_CENTER_X,
        MESSAGE_CENTER_Y,
        NO_TEXT,
        TEXT_SLOT_LARGE_FONT,
        4,
        0,
        0,
        0,
        0,
        0
    );
}

static inline s32 NextIconFrame(s32 slot) {
    return ++f_IconFrames[slot];
}

void AnimateModeIcons(Boolean isImmediate) {
    EncodedAsset encodedAsset;
    SaveData *save;
    SavedGame *game;
    s32 slot;

    save = GetSaveData();
    if ((++f_IconFrameTime >= ICON_FRAME_PERIOD) | (isImmediate == TRUE)) {
        f_IconFrameTime = 0;
        for (slot = 0; slot != SAVED_GAME_COUNT; slot++) {
            if (!save->savedGames[slot].isValid) {
                continue;
            }

            game = &save->savedGames[slot];

            if (game->state.isMiniGolf) {
                if (f_CursorIndex == slot) {
                    if (g_ModeIconAnimations[MODE_ICON_MINI_GOLF][NextIconFrame(slot)] ==
                        END_OF_ICON_FRAMES) {
                        f_IconFrames[slot] = 0;
                    }

                    OpenEncodedAsset(
                        g_ModeIconAnimations[MODE_ICON_MINI_GOLF][f_IconFrames[slot]],
                        &encodedAsset
                    );
                }
                else {
                    OpenEncodedAsset(g_ModeIconAnimations[MODE_ICON_MINI_GOLF][0], &encodedAsset);
                }
                ReadEncodedAsset(f_ModeIconImages[slot][f_IconImageIndex], &encodedAsset);
            }
            else if (f_CursorIndex == slot) {
                if (g_ModeIconAnimations[f_ModeIcons[game->state.gameMode]][NextIconFrame(slot)] ==
                    END_OF_ICON_FRAMES) {
                    f_IconFrames[slot] = 0;
                }

                OpenEncodedAsset(
                    g_ModeIconAnimations[f_ModeIcons[game->state.gameMode]][f_IconFrames[slot]],
                    &encodedAsset
                );
                ReadEncodedAsset(f_ModeIconImages[slot][f_IconImageIndex], &encodedAsset);
            }
            else {
                OpenEncodedAsset(
                    g_ModeIconAnimations[f_ModeIcons[game->state.gameMode]][0],
                    &encodedAsset
                );
                ReadEncodedAsset(f_ModeIconImages[slot][f_IconImageIndex], &encodedAsset);
            }
        }

        f_IconImageIndex ^= 1;
    }
}

void DrawGameSaveMenu(Gfx **displayListHead) {
    Gfx *gfx;
    SaveData *save;
    s32 row;

    s32 index;
    s32 strip;
    s32 stripHeight;
    f32 titleLeftExact;
    f32 titleTopExact;
    s32 titleLeft;
    s32 titleTop;
    s32 titleTexelLeft;
    s32 titleTexelTop;
    s32 drawnHeight;
    s32 imageLeft;

    save = GetSaveData();
    gfx = *displayListHead;
    if (f_MenuKind == SAVE_MENU_GAMES) {
        AnimateModeIcons(FALSE);

        gDPPipeSync(gfx++);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);

        sinf(f_PanelWaveAngle);
        f_PanelWaveAngle += DegreesToRadians(SWAY_DEGREES_PER_FRAME);
        LoadLargePalette(&gfx, ImagePalette(f_NamePlateImage));
        for (strip = 0; strip != NAME_PLATE_STRIP_COUNT; strip++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NamePlateImage) + (strip * NAME_PLATE_STRIP_SIZE),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_MIRROR,
                G_TX_MIRROR,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            for (index = 0; index != SAVED_GAME_COUNT; index++) {
                if (index == f_CursorIndex) {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE
                    );
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                stripHeight = GAME_PANEL_STRIP_HEIGHT;
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(f_SlideOffsets[index] + WIDE_PANEL_LEFT),
                    QuarterPixels(
                        GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) + (strip * stripHeight) -
                        (stripHeight * NAME_PLATE_STRIP_COUNT / 2)
                    ),
                    QuarterPixels(f_SlideOffsets[index] + WIDE_PANEL_LEFT + WIDE_PANEL_WIDTH),
                    QuarterPixels(
                        GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) + (strip * stripHeight) -
                        (stripHeight * NAME_PLATE_STRIP_COUNT / 2) + stripHeight
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TexelRunStep(NAME_PLATE_WIDTH, WIDE_PANEL_WIDTH),
                    TexelRunStep(NAME_PLATE_HEIGHT, stripHeight * NAME_PLATE_STRIP_COUNT)
                );
            }

            if (f_CursorIndex == index) {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    CHOSEN_PANEL_SHADE,
                    CHOSEN_PANEL_SHADE,
                    CHOSEN_PANEL_SHADE,
                    CHOSEN_PANEL_SHADE
                );
            }
            else {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OPAQUE_ALPHA
                );
            }

            stripHeight = END_GAME_PANEL_STRIP_HEIGHT;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(f_SlideOffsets[index] + NARROW_PANEL_LEFT),
                QuarterPixels(
                    END_GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) + (strip * stripHeight) -
                    (stripHeight * NAME_PLATE_STRIP_COUNT / 2)
                ),
                QuarterPixels(f_SlideOffsets[index] + NARROW_PANEL_LEFT + NARROW_PANEL_WIDTH),
                QuarterPixels(
                    END_GAME_PANEL_MIDDLE + (index * GAME_ROW_SPACING) + (strip * stripHeight) -
                    (stripHeight * NAME_PLATE_STRIP_COUNT / 2) + stripHeight
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NAME_PLATE_WIDTH, NARROW_PANEL_WIDTH),
                TexelRunStep(NAME_PLATE_HEIGHT, stripHeight * NAME_PLATE_STRIP_COUNT)
            );
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                CHOSEN_PANEL_SHADE,
                CHOSEN_PANEL_SHADE,
                CHOSEN_PANEL_SHADE,
                CHOSEN_PANEL_SHADE
            );
        }

        titleLeftExact = (cosf(f_TitleSwayAngle * 0.5f) * TITLE_SWAY_DISTANCE) + TITLE_SWAY_LEFT;
        titleTopExact = (sinf(f_TitleSwayAngle) * TITLE_SWAY_DISTANCE) + TITLE_SWAY_TOP;
        titleLeft = HalfTowardZero(titleLeftExact);
        titleTop = HalfTowardZero(titleTopExact);
        titleTexelLeft = (s32)(HALF_TEXEL - ((titleLeftExact - titleLeft) * HALF_TEXEL));
        titleTexelTop = (s32)(HALF_TEXEL - ((titleTopExact - titleTop) * HALF_TEXEL));
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_AVERAGE);
        f_TitleSwayAngle += DegreesToRadians(SWAY_DEGREES_PER_FRAME);
        LoadLargePalette(&gfx, ImagePalette(f_SaveMenuTitleImage));
        imageLeft = TITLE_IMAGE_LEFT;

        for (row = 0; row != TITLE_STRIP_COUNT; row++) {
            drawnHeight = TITLE_STRIP_DRAWN_HEIGHT;
            if (row == TITLE_STRIP_COUNT - 1) {
                drawnHeight = TITLE_LAST_STRIP_HEIGHT;
            }

            gDPLoadTextureTile(
                gfx++,
                TextureAddress(LargeImageTexels(f_SaveMenuTitleImage) + (row * TITLE_STRIP_SIZE)),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                TITLE_IMAGE_WIDTH,
                TITLE_STRIP_DRAWN_HEIGHT,
                0,
                0,
                TITLE_IMAGE_WIDTH - 1,
                TITLE_STRIP_DRAWN_HEIGHT - 1,
                0,
                0,
                0,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gSPTextureRectangle(
                gfx++,
                QuarterPixels(titleLeft + imageLeft - f_SlideOffsets[1]),
                QuarterPixels(titleTop + (row * TITLE_STRIP_HEIGHT)),
                QuarterPixels(titleLeft + (imageLeft + TITLE_IMAGE_WIDTH) - f_SlideOffsets[1]),
                QuarterPixels((row * TITLE_STRIP_HEIGHT) + drawnHeight + titleTop),
                G_TX_RENDERTILE,
                titleTexelLeft + TEXEL_COORDINATE_ONE,
                titleTexelTop + TEXEL_COORDINATE_ONE,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);

        for (row = 0; row != SAVED_GAME_COUNT; row++) {
            LoadLargePalette(&gfx, ImagePalette(f_ModeIconImages[row][f_IconImageIndex ^ 1]));
            if (save->savedGames[row].isValid) {
                gDPPipeSync(gfx++);
                if (row == f_CursorIndex) {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE
                    );
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                gDPPipeSync(gfx++);
                for (index = 0; index != MODE_ICON_PART_COUNT; index++) {
                    LoadTextureBlock(
                        &gfx,
                        LargeImageTexels(f_ModeIconImages[row][f_IconImageIndex ^ 1]) +
                            (index * MODE_ICON_PART_HEIGHT * MODE_ICON_SIZE),
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        MODE_ICON_SIZE,
                        MODE_ICON_PART_HEIGHT,
                        0,
                        G_TX_MIRROR,
                        G_TX_MIRROR,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels(f_SlideOffsets[row] + MODE_ICON_LEFT),
                        QuarterPixels(
                            MODE_ICON_TOP + (row * GAME_ROW_SPACING) +
                            (index * (MODE_ICON_PART_HEIGHT / 2))
                        ),
                        QuarterPixels(f_SlideOffsets[row] + MODE_ICON_LEFT + (MODE_ICON_SIZE / 2)),
                        QuarterPixels(
                            MODE_ICON_TOP + (row * GAME_ROW_SPACING) +
                            (index * (MODE_ICON_PART_HEIGHT / 2)) + (MODE_ICON_PART_HEIGHT / 2)
                        ),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        HALF_SIZE_TEXEL_STEP,
                        HALF_SIZE_TEXEL_STEP
                    );
                }
            }
        }

        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        for (row = 0; row != SAVED_GAME_COUNT; row++) {
            if (save->savedGames[row].isValid) {
                GameState *state;
                s32 golfersLeft;
                s32 golfersRight;

                gDPPipeSync(gfx++);
                golfersLeft = GOLFERS_LEFT;
                golfersRight = GOLFERS_LEFT + FACE_WIDTH;
                state = &save->savedGames[row].state;
                if (row == f_CursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                gDPPipeSync(gfx++);
                for (index = 0; index != state->playerCount; index++) {
                    LoadLargePalette(&gfx, f_FacePalettes[(row * PLAYER_COUNT_MAX) + index]);
                    LoadTextureBlock(
                        &gfx,
                        f_FaceImages[(row * PLAYER_COUNT_MAX) + index],
                        G_IM_FMT_CI,
                        G_IM_SIZ_8b,
                        FACE_WIDTH,
                        FACE_HEIGHT,
                        0,
                        0,
                        0,
                        G_TX_NOMASK,
                        G_TX_NOMASK,
                        G_TX_NOLOD,
                        G_TX_NOLOD
                    );
                    DrawTextureRectangle(
                        &gfx,
                        QuarterPixels(
                            (index * GOLFER_SPACING) + (f_SlideOffsets[row] + golfersLeft) -
                            (state->playerCount * (GOLFER_SPACING / 2))
                        ),
                        QuarterPixels(GOLFER_TOP + (row * GAME_ROW_SPACING)),
                        QuarterPixels(
                            (index * GOLFER_SPACING) + (f_SlideOffsets[row] + golfersRight) -
                            (state->playerCount * (GOLFER_SPACING / 2))
                        ),
                        QuarterPixels(GOLFER_TOP + (row * GAME_ROW_SPACING) + FACE_HEIGHT),
                        G_TX_RENDERTILE,
                        0,
                        0,
                        TEXEL_STEP_ONE,
                        TEXEL_STEP_ONE
                    );
                }
            }
        }

        {
            s32 rowHeights[GAME_ROW_COUNT] = {42, 42, 42, 25};
            s32 rowHalfWidths[GAME_ROW_COUNT] = {
                WIDE_PANEL_HALF_WIDTH,
                WIDE_PANEL_HALF_WIDTH,
                WIDE_PANEL_HALF_WIDTH,
                NARROW_PANEL_HALF_WIDTH,
            };
            s32 centerX;
            s32 rowTop;

            f_CursorPulseAngle += DegreesToRadians(SWAY_DEGREES_PER_FRAME);
            centerX = SCREEN_WIDTH / 2;
            rowTop = FIRST_GAME_ROW_TOP;
            gDPSetPrimColor(
                gfx++,
                0,
                0,
                0x00,
                0xBF,
                0xFF,
                (s32)(sinf(f_CursorPulseAngle) * CURSOR_ALPHA_SWING) + CURSOR_ALPHA_MIDDLE
            );
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    f_SlideOffsets[f_CursorIndex] + centerX - rowHalfWidths[f_CursorIndex]
                ),
                QuarterPixels(rowTop + (f_CursorIndex * GAME_ROW_SPACING)),
                QuarterPixels(
                    rowHalfWidths[f_CursorIndex] + (f_SlideOffsets[f_CursorIndex] + centerX)
                ),
                QuarterPixels(rowTop + (f_CursorIndex * GAME_ROW_SPACING) + CURSOR_FRAME_THICKNESS),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    f_SlideOffsets[f_CursorIndex] + centerX - rowHalfWidths[f_CursorIndex]
                ),
                QuarterPixels(
                    rowHeights[f_CursorIndex] + (rowTop + (f_CursorIndex * GAME_ROW_SPACING))
                ),
                QuarterPixels(
                    rowHalfWidths[f_CursorIndex] + (f_SlideOffsets[f_CursorIndex] + centerX)
                ),
                QuarterPixels(
                    rowHeights[f_CursorIndex] + (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) +
                    CURSOR_FRAME_THICKNESS
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    f_SlideOffsets[f_CursorIndex] + centerX - rowHalfWidths[f_CursorIndex]
                ),
                QuarterPixels(rowTop + (f_CursorIndex * GAME_ROW_SPACING)),
                QuarterPixels(
                    f_SlideOffsets[f_CursorIndex] + centerX - rowHalfWidths[f_CursorIndex] +
                    CURSOR_FRAME_THICKNESS
                ),
                QuarterPixels(
                    rowHeights[f_CursorIndex] + (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) +
                    CURSOR_FRAME_THICKNESS
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    rowHalfWidths[f_CursorIndex] + (f_SlideOffsets[f_CursorIndex] + centerX) -
                    CURSOR_FRAME_THICKNESS
                ),
                QuarterPixels(rowTop + (f_CursorIndex * GAME_ROW_SPACING)),
                QuarterPixels(
                    rowHalfWidths[f_CursorIndex] + (f_SlideOffsets[f_CursorIndex] + centerX)
                ),
                QuarterPixels(
                    rowHeights[f_CursorIndex] + (rowTop + (f_CursorIndex * GAME_ROW_SPACING)) +
                    CURSOR_FRAME_THICKNESS
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        SetUpGradientTextDrawing(&gfx, 0);
        for (row = 0; row != SAVED_GAME_COUNT; row++) {
            gDPPipeSync(gfx++);
            if (row == f_CursorIndex) {
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            }
            else {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OPAQUE_ALPHA
                );
            }

            gDPPipeSync(gfx++);
            if (save->savedGames[row].isValid) {
                GameState *state = &save->savedGames[row].state;

                if (state->isMiniGolf) {
                    sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_MINI_GOLF]);
                }
                else {
                    switch (state->gameMode) {
                        case GAME_MODE_TOURNAMENT:
                            sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_TOURNAMENT]);
                            break;
                        case GAME_MODE_GET_CHARACTER:
                            sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_GET_CHARACTER]);
                            break;
                        case GAME_MODE_STROKE:
                            sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_STROKE]);
                            break;
                        case GAME_MODE_MATCH_GAME:
                            sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_MATCH_GAME]);
                            break;
                        case GAME_MODE_SKINS_MATCH:
                            sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_SKINS_MATCH]);
                            break;
                        case GAME_MODE_CLUB_SLOTS:
                            sprintf(g_PrintBuffer, "%s", f_ModeNames[MODE_NAME_CLUB_SLOTS]);
                            break;
                    }
                }

                DrawLargeTextWithShadow(
                    &gfx,
                    f_SlideOffsets[row] + MODE_TEXT_LEFT,
                    MODE_TEXT_TOP + (row * GAME_ROW_SPACING),
                    g_PrintBuffer,
                    0xFFFF9300,
                    0x44FF0000,
                    0
                );
                if (state->isMiniGolf) {
                    if (state->courseIndex == LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_GREEN) {
                        sprintf(
                            g_PrintBuffer,
                            "%s GREEN",
                            g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name
                        );
                    }

                    if (state->courseIndex == LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_SLOW) {
                        sprintf(
                            g_PrintBuffer,
                            "%s SLOW",
                            g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name
                        );
                    }

                    if (state->courseIndex == LUIGIS_GARDEN_COURSE_INDEX + MINI_GOLF_FAST) {
                        sprintf(
                            g_PrintBuffer,
                            "%s FAST",
                            g_CourseRecords[COURSE_RECORD_LUIGIS_GARDEN].name
                        );
                    }

                    if (state->courseIndex == PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_GREEN) {
                        sprintf(
                            g_PrintBuffer,
                            "%s GREEN",
                            g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name
                        );
                    }

                    if (state->courseIndex == PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_SLOW) {
                        sprintf(
                            g_PrintBuffer,
                            "%s SLOW",
                            g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name
                        );
                    }

                    if (state->courseIndex == PEACHS_CASTLE_COURSE_INDEX + MINI_GOLF_FAST) {
                        sprintf(
                            g_PrintBuffer,
                            "%s FAST",
                            g_CourseRecords[COURSE_RECORD_PEACHS_CASTLE].name
                        );
                    }
                }
                else if (state->gameMode == GAME_MODE_TOURNAMENT) {
                    sprintf(g_PrintBuffer, "%s", g_TournamentNames[state->courseIndex]);
                }
                else {
                    sprintf(g_PrintBuffer, "%s", g_CourseRecords[state->courseIndex].name);
                }

#ifdef AVOID_UB
                MeasureText(g_PrintBuffer, FONT_SIZE_LARGE, MEASURE_TEXT_FIXED_WIDTH);
#else
                MeasureText(g_PrintBuffer, FONT_SIZE_LARGE);
#endif
                DrawLargeTextWithShadow(
                    &gfx,
                    f_SlideOffsets[row] + MODE_TEXT_LEFT,
                    COURSE_TEXT_TOP + (row * GAME_ROW_SPACING),
                    g_PrintBuffer,
                    0x00FFFF00,
                    0xFFFFFF00,
                    0
                );
                gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
                sprintf(g_PrintBuffer, "Hole%d", (int)(state->holeIndex + 1));
                DrawLargeText(
                    &gfx,
                    f_SlideOffsets[row] + GAME_HOLE_TEXT_LEFT,
                    GAME_HOLE_TEXT_TOP + (row * GAME_ROW_SPACING),
                    g_PrintBuffer
                );
            }
            else {
                gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x2C, OPAQUE_ALPHA);
                gDPSetPrimColor(gfx++, 0, 0, 0x40, 0xFF, 0x00, OPAQUE_ALPHA);
                sprintf(g_PrintBuffer, "%s", f_NoSavedGameText);
                DrawCenteredLargeText(
                    &gfx,
                    f_SlideOffsets[row] + (SCREEN_WIDTH / 2),
                    EMPTY_GAME_TEXT_TOP + (row * GAME_ROW_SPACING),
                    g_PrintBuffer
                );
            }
        }

        gDPPipeSync(gfx++);
        DrawLargeTextWithShadow(
            &gfx,
            f_SlideOffsets[END_GAME_ROW] + END_GAME_TEXT_LEFT,
            END_GAME_TEXT_TOP,
            f_EndGameText,
            0xFF571300,
            0xFFDB4C00,
            0
        );
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0x00, OPAQUE_ALPHA);
        gDPSetEnvColor(gfx++, 0xFF, 0x68, 0x00, OPAQUE_ALPHA);
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

void SaveGameToSlot(void) {
    EncodedAsset encodedAsset;
    AssetFile file;
    SaveData *save;
    SavedGame *game;
    GameState *state;
    Player *player;
    s32 playerIndex;

    save = GetSaveData();
    game = &save->savedGames[f_CursorIndex];
    SaveOrContinueGame(f_CursorIndex, FALSE);
    state = &game->state;
    f_StateFrameCount = 0;
    InvalidateBrokenSavedGames();
    if (save->savedGames[f_CursorIndex].isValid) {
        f_MenuState = SAVE_MENU_STATE_SAVED;
        AnimateModeIcons(TRUE);
        for (playerIndex = 0; playerIndex != state->playerCount; playerIndex++) {
            player = &save->savedPlayers[f_CursorIndex][playerIndex];
            OpenEncodedAsset(
                g_CharacterFirstFaceAssets[player->characterIndex] + CHARACTER_FACE_IMAGE_OFFSET,
                &encodedAsset
            );
            ReadEncodedAsset(
                f_FaceImages[(f_CursorIndex * PLAYER_COUNT_MAX) + playerIndex],
                &encodedAsset
            );
            OpenAssetFile(
                g_CharacterFirstPaletteFiles[player->characterIndex] + player->paletteIndex,
                &file
            );
            ReadAssetFile(
                f_FacePalettes[(f_CursorIndex * PLAYER_COUNT_MAX) + playerIndex],
                GetAssetFileSize(&file),
                &file
            );
        }
    }
    else {
        f_MenuState = SAVE_MENU_STATE_CHOOSING;
    }

    WriteSaveData();
}

void LeaveGameSaveMenu(void) {
    f_StateFrameCount = 0;
    f_MenuState = SAVE_MENU_STATE_LEAVING;
}

void SaveReplayToSlot(void) {
    EncodedAsset encodedAsset;
    AssetFile file;
    RingShotRecord *record;

    record = &GetSaveData()->ringShotRecords[f_CursorIndex];
    SaveRingShotRecord(f_CursorIndex, g_RingShotRecords);
    f_StateFrameCount = 0;
    InvalidateBrokenRingShotRecords();
    if (record->scoreKind & SCORE_KIND_RECORDED) {
        OpenEncodedAsset(
            g_CharacterFirstFaceAssets[record->characterIndex] + CHARACTER_FACE_IMAGE_OFFSET,
            &encodedAsset
        );
        ReadEncodedAsset(f_FaceImages[f_CursorIndex], &encodedAsset);
        OpenAssetFile(
            g_CharacterFirstPaletteFiles[record->characterIndex] + record->paletteIndex,
            &file
        );
        ReadAssetFile(f_FacePalettes[f_CursorIndex], GetAssetFileSize(&file), &file);
        f_MenuState = SAVE_MENU_STATE_SAVED;
    }
    else {
        f_MenuState = SAVE_MENU_STATE_CHOOSING;
    }

    WriteSaveData();
}

void LeaveReplaySaveMenu(void) {
    f_StateFrameCount = 0;
    f_MenuState = SAVE_MENU_STATE_LEAVING;
}

void UpdateGameSaveMenu(ControllerState *controller) {
    SaveData *save;
    s32 row;

    save = GetSaveData();
    f_StateFrameCount++;
    switch (f_MenuState) {
        case SAVE_MENU_STATE_OPENING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < f_StateFrameCount && f_SlideOffsets[row] > 0) {
                    if (f_SlideOffsets[row] == 1) {
                        f_SlideOffsets[row] = 0;
                    }
                    else {
                        f_SlideOffsets[row] += -f_SlideOffsets[row] / 2;
                    }
                }
            }

            if (f_StateFrameCount == SLIDE_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_CHOOSING;
            }

            break;
        case SAVE_MENU_STATE_CHOOSING:
            if (!IsPromptClosed()) {
                break;
            }

            if (controller->pressedButtons & B_BUTTON) {
                PlaySoundWithEffects(
                    SOUND_PANEL_CLOSE,
                    g_NextSoundChannel,
                    MENU_VOLUME_MEDIUM,
                    MENU_SOUND_REVERB,
                    SOUND_PAN_LEFT
                );
                AnimateModeIcons(TRUE);
                f_StateFrameCount = 0;
                f_MenuState = SAVE_MENU_STATE_BACKING_OUT;
                break;
            }

            if (controller->pressedButtons & A_BUTTON) {
                if (f_CursorIndex >= 0 && f_CursorIndex < SAVED_GAME_COUNT) {
                    if (save->savedGames[f_CursorIndex].isValid) {
                        g_PauseScreen = PAUSE_SCREEN_PROMPT;
                        OpenPrompt(PROMPT_OVERWRITE_GAME);
                        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    }
                    else {
                        SaveGameToSlot();
                        PlaySound(SOUND_SAVED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                    }
                }
                else {
                    g_PauseScreen = PAUSE_SCREEN_PROMPT;
                    OpenPrompt(PROMPT_END_GAME);
                }

                break;
            }

            if (controller->repeatedButtons & U_JPAD) {
                f_CursorIndex--;
                AnimateModeIcons(TRUE);
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            }
            else if (controller->repeatedButtons & D_JPAD) {
                f_CursorIndex++;
                AnimateModeIcons(TRUE);
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            }

            if (f_CursorIndex < 0) {
                f_CursorIndex = END_GAME_ROW;
            }

            if (f_CursorIndex > END_GAME_ROW) {
                f_CursorIndex = 0;
            }

            break;
        case SAVE_MENU_STATE_BACKING_OUT:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < f_StateFrameCount) {
                    if (f_SlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
                        f_SlideOffsets[row] -= SLIDE_OUT_START_STEP;
                    }
                    else {
                        f_SlideOffsets[row] += f_SlideOffsets[row] / 2;
                    }
                }
            }

            if (f_StateFrameCount == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(
                    SOUND_PANEL_CLOSE,
                    g_NextSoundChannel,
                    MENU_VOLUME_MEDIUM,
                    MENU_SOUND_REVERB,
                    SOUND_PAN_LEFT
                );
            }

            if (f_StateFrameCount == SLIDE_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_BACKED_OUT;
            }

            break;
        case SAVE_MENU_STATE_BACKED_OUT:
            CloseSaveMenu();
            g_PauseScreen = PAUSE_SCREEN_NONE;
            SetMenuBackdrop(MENU_BACKDROP_NONE);
            SetFramebufferCount(TRIPLE_BUFFERING);
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            g_RoundHud.isLetterboxShown = TRUE;
            g_RoundHud.isClubPanelShown = TRUE;
            g_RoundHud.isWindIndicatorShown = TRUE;
            g_RoundHud.isIgnoredPausePartShown = FALSE;
            g_RoundHud.isHoleInfoShown = TRUE;
            g_RoundHud.isLieIconShown = TRUE;
            g_RoundHud.isClubHeadIconShown = TRUE;
            g_RoundHud.isUnreadItemShown = TRUE;
            g_RoundHud.isProgressShown = TRUE;
            break;
        case SAVE_MENU_STATE_SAVED:
            if (f_StateFrameCount == SAVED_WAIT_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_LEAVING;
                f_StateFrameCount = 0;
            }

            break;
        case SAVE_MENU_STATE_LEAVING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < f_StateFrameCount) {
                    if (f_SlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
                        f_SlideOffsets[row] -= SLIDE_OUT_START_STEP;
                    }
                    else {
                        f_SlideOffsets[row] += f_SlideOffsets[row] / 2;
                    }
                }
            }

            if (f_StateFrameCount == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(
                    SOUND_PANEL_CLOSE,
                    g_NextSoundChannel,
                    MENU_VOLUME_MEDIUM,
                    MENU_SOUND_REVERB,
                    SOUND_PAN_LEFT
                );
            }

            if (f_StateFrameCount == SLIDE_FRAMES) {
                g_PauseScreen = PAUSE_SCREEN_NONE;
                CloseSaveMenu();
                QuitPlayToTitle();
            }

            break;
    }
}

ReplayMenuState UpdateReplaySaveMenu(ControllerState *controller) {
    SaveData *save;
    s32 row;

    save = GetSaveData();
    f_StateFrameCount++;
    switch (f_MenuState) {
        case SAVE_MENU_STATE_OPENING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < f_StateFrameCount && f_SlideOffsets[row] > 0) {
                    if (f_SlideOffsets[row] == 1) {
                        f_SlideOffsets[row] = 0;
                    }
                    else {
                        f_SlideOffsets[row] += -f_SlideOffsets[row] / 2;
                    }
                }
            }

            if (f_StateFrameCount == SLIDE_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_CHOOSING;
            }

            break;
        case SAVE_MENU_STATE_CHOOSING:
            if (!IsPromptClosed()) {
                break;
            }

            if (controller->pressedButtons & A_BUTTON) {
                if (f_CursorIndex >= 0 && f_CursorIndex < RING_SHOT_RECORD_COUNT) {
                    if ((save->ringShotRecords + f_CursorIndex)->scoreKind & SCORE_KIND_RECORDED) {
                        g_PauseScreen = PAUSE_SCREEN_PROMPT;
                        OpenPrompt(PROMPT_OVERWRITE_REPLAY);
                        PlaySound(SOUND_MENU_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                        return REPLAY_MENU_PROMPTING;
                    }

                    SaveReplayToSlot();
                    PlaySound(SOUND_SAVED, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                }
                else {
                    g_PauseScreen = PAUSE_SCREEN_PROMPT;
                    OpenPrompt(PROMPT_SKIP_REPLAY);
                    return REPLAY_MENU_PROMPTING;
                }
            }
            else if (controller->pressedButtons & B_BUTTON) {
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
                f_CursorIndex = SKIP_REPLAY_ROW;
            }
            else if (controller->repeatedButtons & U_JPAD) {
                f_CursorIndex--;
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            }
            else if (controller->repeatedButtons & D_JPAD) {
                f_CursorIndex++;
                PlaySound(SOUND_MENU_MOVE, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            }

            if (f_CursorIndex < 0) {
                f_CursorIndex = SKIP_REPLAY_ROW;
            }

            if (f_CursorIndex > SKIP_REPLAY_ROW) {
                f_CursorIndex = 0;
            }

            break;
        case SAVE_MENU_STATE_SAVED:
            if (f_StateFrameCount == SAVED_WAIT_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_LEAVING;
                f_StateFrameCount = 0;
            }

            break;
        case SAVE_MENU_STATE_LEAVING:
            for (row = 0; row != SLIDE_ROW_COUNT; row++) {
                if (row < f_StateFrameCount) {
                    if (f_SlideOffsets[row] >= SLIDE_OUT_START_OFFSET) {
                        f_SlideOffsets[row] -= SLIDE_OUT_START_STEP;
                    }
                    else {
                        f_SlideOffsets[row] += f_SlideOffsets[row] / 2;
                    }
                }
            }

            if (f_StateFrameCount == SAVED_WAIT_FRAMES) {
                CloseSaveMenu();
                return REPLAY_MENU_CLOSED;
            }

            break;
    }

    return REPLAY_MENU_OPEN;
}

void DrawReplaySaveMenu(Gfx **displayListHead) {
    ReplayLabel rowLabels[REPLAY_ROW_COUNT];
    s32 rowOffsets[REPLAY_ROW_COUNT];
    Gfx *gfx;
    SaveData *save;
    s32 row;

    s32 index;
    s32 stripHeight;
    s32 titleBob;
    s32 titleHeight;
    s32 slideOffset;
    ImageAsset *labelImage;
    s32 labelWidth;
    s32 labelHeight;
    s32 text;

    save = GetSaveData();
    gfx = *displayListHead;

    for (row = 0; row != REPLAY_ROW_COUNT; row++) {
        slideOffset = f_SlideOffsets[row / 2];
        rowOffsets[row] = slideOffset;
        if (!(row & 1)) {
            rowOffsets[row] = -slideOffset;
        }
    }

    if (f_MenuKind == SAVE_MENU_REPLAYS) {
        SetUpTextDrawing(&gfx);

        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
        LoadLargePalette(&gfx, ImagePalette(f_NamePlateImage));
        for (index = 0; index != NAME_PLATE_STRIP_COUNT; index++) {
            stripHeight = RECORD_PANEL_STRIP_HEIGHT;
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NamePlateImage) + (index * NAME_PLATE_STRIP_SIZE),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_MIRROR,
                G_TX_MIRROR,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            gDPPipeSync(gfx++);
            for (row = 0; row != RING_SHOT_RECORD_COUNT; row++) {
                if (row == f_CursorIndex) {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE,
                        CHOSEN_PANEL_SHADE
                    );
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(rowOffsets[row] + WIDE_PANEL_LEFT),
                    QuarterPixels(
                        FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) + (index * stripHeight)
                    ),
                    QuarterPixels(rowOffsets[row] + WIDE_PANEL_LEFT + WIDE_PANEL_WIDTH),
                    QuarterPixels(
                        FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) + (index * stripHeight) +
                        stripHeight
                    ),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TexelRunStep(NAME_PLATE_WIDTH, WIDE_PANEL_WIDTH),
                    TexelRunStep(NAME_PLATE_HEIGHT, stripHeight * NAME_PLATE_STRIP_COUNT)
                );
            }

            stripHeight = HOLE_OUT_PANEL_STRIP_HEIGHT;
            gDPPipeSync(gfx++);
            if (f_CursorIndex == row) {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    CHOSEN_PANEL_SHADE,
                    CHOSEN_PANEL_SHADE,
                    CHOSEN_PANEL_SHADE,
                    CHOSEN_PANEL_SHADE
                );
            }
            else {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OPAQUE_ALPHA
                );
            }

            DrawTextureRectangle(
                &gfx,
                QuarterPixels(rowOffsets[row] + NARROW_PANEL_LEFT),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) + (index * stripHeight)
                ),
                QuarterPixels(rowOffsets[row] + NARROW_PANEL_LEFT + NARROW_PANEL_WIDTH),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (row * REPLAY_ROW_SPACING) + (index * stripHeight) +
                    stripHeight
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NAME_PLATE_WIDTH, NARROW_PANEL_WIDTH),
                TexelRunStep(NAME_PLATE_HEIGHT, stripHeight * NAME_PLATE_STRIP_COUNT)
            );
        }

        LoadSmallPalette(&gfx, 0, ImagePalette(f_CourseNumbersImage));
        gDPLoadTextureTile_4b(
            gfx++,
            TextureAddress(ImageTexels(f_CourseNumbersImage)),
            G_IM_FMT_CI,
            COURSE_NUMBERS_WIDTH,
            COURSE_NUMBERS_HEIGHT,
            0,
            0,
            COURSE_NUMBERS_WIDTH - 1,
            COURSE_NUMBERS_HEIGHT - 1,
            0,
            G_TX_MIRROR,
            G_TX_MIRROR,
            G_TX_NOMASK,
            G_TX_NOMASK,
            G_TX_NOLOD,
            G_TX_NOLOD
        );
        for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
            RingShotRecord *record = &save->ringShotRecords[index];

            if (!(record->scoreKind & SCORE_KIND_RECORDED)) {
                rowLabels[index] = NO_REPLAY_LABEL;
            }
            else {
                gDPPipeSync(gfx++);
                if (index == f_CursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                gDPPipeSync(gfx++);
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(rowOffsets[index] + COURSE_WORD_LEFT),
                    QuarterPixels(COURSE_TOP + (index * REPLAY_ROW_SPACING)),
                    QuarterPixels(rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH),
                    QuarterPixels(
                        COURSE_TOP + (index * REPLAY_ROW_SPACING) + COURSE_NUMBERS_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    TexelCoordinate(COURSE_WORD_TEXEL_LEFT),
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH),
                    QuarterPixels(COURSE_TOP + (index * REPLAY_ROW_SPACING)),
                    QuarterPixels(
                        rowOffsets[index] + COURSE_WORD_LEFT + COURSE_WORD_WIDTH +
                        COURSE_DIGIT_WIDTH
                    ),
                    QuarterPixels(
                        COURSE_TOP + (index * REPLAY_ROW_SPACING) + COURSE_NUMBERS_HEIGHT
                    ),
                    G_TX_RENDERTILE,
                    TexelCoordinate(record->courseIndex * COURSE_DIGIT_WIDTH),
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
                switch (record->scoreKind & SCORE_KIND_MASK) {
                    case SCORE_KIND_ALBATROSS:
                        rowLabels[index] = REPLAY_LABEL_ALBATROSS;
                        break;
                    case SCORE_KIND_HOLE_IN_ONE:
                        rowLabels[index] = REPLAY_LABEL_HOLE_IN_ONE;
                        break;
                    case SCORE_KIND_EAGLE:
                        rowLabels[index] = REPLAY_LABEL_EAGLE;
                        break;
                    case SCORE_KIND_BIRDIE:
                        rowLabels[index] = REPLAY_LABEL_BIRDIE;
                        break;
                    case SCORE_KIND_PAR:
                        rowLabels[index] = REPLAY_LABEL_PAR;
                        break;
                }
            }
        }

        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
        for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
            if (rowLabels[index] != NO_REPLAY_LABEL) {
                labelImage = f_ReplayLabelImages[rowLabels[index]];
                labelWidth = ReadSmallImageWidth(labelImage);
                labelHeight = ReadSmallImageHeight(labelImage);
                gDPPipeSync(gfx++);
                if (index == f_CursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                gDPPipeSync(gfx++);
                LoadSmallPalette(&gfx, 0, ImagePalette(f_ReplayLabelImages[rowLabels[index]]));
                LoadNibbleTextureBlock(
                    &gfx,
                    ImageTexels(f_ReplayLabelImages[rowLabels[index]]),
                    G_IM_FMT_CI,
                    labelWidth,
                    labelHeight,
                    0,
                    G_TX_MIRROR,
                    G_TX_MIRROR,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(rowOffsets[index] + LABEL_LEFT),
                    QuarterPixels(LABEL_TOP + (index * REPLAY_ROW_SPACING)),
                    QuarterPixels(rowOffsets[index] + LABEL_LEFT + labelWidth),
                    QuarterPixels(LABEL_TOP + (index * REPLAY_ROW_SPACING) + labelHeight),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
        }

        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_NONE);
        for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
            RingShotRecord *record = &save->ringShotRecords[index];

            if (record->scoreKind & SCORE_KIND_RECORDED) {
                gDPPipeSync(gfx++);
                if (index == f_CursorIndex) {
                    gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
                }
                else {
                    gDPSetPrimColor(
                        gfx++,
                        0,
                        0,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OTHER_PANEL_SHADE,
                        OPAQUE_ALPHA
                    );
                }

                gDPPipeSync(gfx++);
                gDPPipeSync(gfx++);
                gDPSetTextureLUT(gfx++, G_TT_RGBA16);
                LoadLargePalette(&gfx, f_FacePalettes[index]);
                LoadTextureBlock(
                    &gfx,
                    f_FaceImages[index],
                    G_IM_FMT_CI,
                    G_IM_SIZ_8b,
                    FACE_WIDTH,
                    FACE_HEIGHT,
                    0,
                    0,
                    0,
                    G_TX_NOMASK,
                    G_TX_NOMASK,
                    G_TX_NOLOD,
                    G_TX_NOLOD
                );
                DrawTextureRectangle(
                    &gfx,
                    QuarterPixels(rowOffsets[index] + CHARACTER_LEFT),
                    QuarterPixels(CHARACTER_TOP + (index * REPLAY_ROW_SPACING)),
                    QuarterPixels(rowOffsets[index] + CHARACTER_LEFT + FACE_WIDTH),
                    QuarterPixels(CHARACTER_TOP + (index * REPLAY_ROW_SPACING) + FACE_HEIGHT),
                    G_TX_RENDERTILE,
                    0,
                    0,
                    TEXEL_STEP_ONE,
                    TEXEL_STEP_ONE
                );
            }
        }

        {
            s32 rowHeights[REPLAY_ROW_COUNT] = {
                RECORD_PANEL_HEIGHT,
                RECORD_PANEL_HEIGHT,
                RECORD_PANEL_HEIGHT,
                RECORD_PANEL_HEIGHT,
                RECORD_PANEL_HEIGHT,
                HOLE_OUT_PANEL_HEIGHT,
            };
            s32 rowHalfWidths[REPLAY_ROW_COUNT] = {
                WIDE_PANEL_HALF_WIDTH,
                WIDE_PANEL_HALF_WIDTH,
                WIDE_PANEL_HALF_WIDTH,
                WIDE_PANEL_HALF_WIDTH,
                WIDE_PANEL_HALF_WIDTH,
                NARROW_PANEL_HALF_WIDTH,
            };
            s32 centerX;

            centerX = SCREEN_WIDTH / 2;
            gDPSetPrimColor(gfx++, 0, 0, 0x00, 0xBF, 0xFF, OPAQUE_ALPHA);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
            gDPSetCombineMode(gfx++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX)),
                QuarterPixels(FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING)),
                QuarterPixels(rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX)),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
                    CURSOR_FRAME_THICKNESS
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX)),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
                    rowHeights[f_CursorIndex] - CURSOR_FRAME_THICKNESS
                ),
                QuarterPixels(rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX)),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
                    rowHeights[f_CursorIndex]
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX)),
                QuarterPixels(FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING)),
                QuarterPixels(
                    rowOffsets[f_CursorIndex] - (rowHalfWidths[f_CursorIndex] - centerX) +
                    CURSOR_FRAME_THICKNESS
                ),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
                    rowHeights[f_CursorIndex]
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(
                    rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX) -
                    CURSOR_FRAME_THICKNESS
                ),
                QuarterPixels(FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING)),
                QuarterPixels(rowOffsets[f_CursorIndex] + (rowHalfWidths[f_CursorIndex] + centerX)),
                QuarterPixels(
                    FIRST_REPLAY_ROW_TOP + (f_CursorIndex * REPLAY_ROW_SPACING) +
                    rowHeights[f_CursorIndex]
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        titleBob = sinf(f_ReplayTitleBobAngle) * TITLE_BOB_HEIGHT;
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        f_ReplayTitleBobAngle += DegreesToRadians(TITLE_BOB_DEGREES_PER_FRAME);
        LoadLargePalette(&gfx, ImagePalette(f_NamePlateImage));
        for (index = 0; index != NAME_PLATE_STRIP_COUNT; index++) {
            stripHeight = TITLE_PANEL_STRIP_HEIGHT;
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NamePlateImage) + (index * NAME_PLATE_STRIP_SIZE),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                NAME_PLATE_WIDTH,
                NAME_PLATE_STRIP_HEIGHT,
                0,
                G_TX_MIRROR,
                G_TX_MIRROR,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            titleHeight = stripHeight * NAME_PLATE_STRIP_COUNT;
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(rowOffsets[1] + TITLE_PANEL_LEFT),
                QuarterPixels(titleBob + TITLE_PANEL_TOP + (index * TITLE_PANEL_STRIP_HEIGHT)),
                QuarterPixels(rowOffsets[1] + TITLE_PANEL_LEFT + TITLE_PANEL_WIDTH),
                QuarterPixels(
                    titleBob + TITLE_PANEL_TOP + (index * TITLE_PANEL_STRIP_HEIGHT) + stripHeight
                ),
                G_TX_RENDERTILE,
                0,
                0,
                TexelRunStep(NAME_PLATE_WIDTH, TITLE_PANEL_WIDTH),
                TexelRunStep(NAME_PLATE_HEIGHT, titleHeight)
            );
        }

        SetUpGradientTextDrawing(&gfx, 0);
        for (index = 0; index != RING_SHOT_RECORD_COUNT; index++) {
            RingShotRecord *record;

            gDPPipeSync(gfx++);
            record = &save->ringShotRecords[index];
            if (index == f_CursorIndex) {
                gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, OPAQUE_ALPHA);
            }
            else {
                gDPSetPrimColor(
                    gfx++,
                    0,
                    0,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OTHER_PANEL_SHADE,
                    OPAQUE_ALPHA
                );
            }

            gDPPipeSync(gfx++);
            if (!(record->scoreKind & SCORE_KIND_RECORDED)) {
                SetUpGradientTextDrawing(&gfx, 0);
                gDPSetEnvColor(gfx++, 0xFF, 0xFF, 0x2C, OPAQUE_ALPHA);
                gDPSetPrimColor(gfx++, 0, 0, 0x40, 0xFF, 0x00, OPAQUE_ALPHA);

                sprintf(g_PrintBuffer, "%s", f_NoSavedGameText);
                DrawCenteredLargeText(
                    &gfx,
                    rowOffsets[index] + (SCREEN_WIDTH / 2),
                    EMPTY_TEXT_TOP + (index * REPLAY_ROW_SPACING),
                    f_NoReplayText
                );
            }
            else {
                text = (record->courseIndex * RING_SHOT_HOLE_COUNT) + record->holeIndex;
                SetUpTextDrawing(&gfx);
                SetTextSlot(
                    rowOffsets[index] + HOLE_TEXT_LEFT,
                    HOLE_TEXT_TOP + (index * REPLAY_ROW_SPACING),
                    text + MESSAGE_FIRST_RING_SHOT_HOLE,
                    TEXT_SLOT_LARGE_FONT,
                    FIRST_RECORD_TEXT_SLOT + index,
                    TEXT_FLAG_DRAWN_BY_OWNER,
                    0xFF,
                    0xFF,
                    0xFF,
                    OPAQUE_ALPHA
                );
                DrawLargeTextSlot(&gfx, FIRST_RECORD_TEXT_SLOT + index);
            }
        }

        SetUpGradientTextDrawing(&gfx, 0);
        DrawLargeTextWithShadow(
            &gfx,
            rowOffsets[5] + HOLE_OUT_TEXT_LEFT,
            HOLE_OUT_TEXT_TOP,
            "HoleOut",
            0xFF571300,
            0xFFDB4C00,
            0
        );
        gDPPipeSync(gfx++);
        DrawLargeTextWithShadow(
            &gfx,
            rowOffsets[1] + TITLE_TEXT_LEFT,
            titleBob + TITLE_TEXT_TOP,
            f_SaveMenuText,
            0xFF000000,
            0xFFFFFF00,
            0
        );
        gDPPipeSync(gfx++);
        gDPPipeSync(gfx++);
        gDPSetCycleType(gfx++, G_CYC_1CYCLE);
        gDPPipeSync(gfx++);
        *displayListHead = gfx;
    }
}

void OpenControlsScreen(void) {
    EncodedAsset encodedAsset;
    ImageAsset *picture;

    if (g_GameState.isMiniGolf) {
        picture = AllocMemory(OpenEncodedAsset(ASSET_MINI_GOLF_CONTROLS_PICTURE, &encodedAsset));
        f_NamePlateImage = picture;
    }
    else {
        picture = AllocMemory(OpenEncodedAsset(ASSET_CONTROLS_PICTURE, &encodedAsset));
        f_NamePlateImage = picture;
    }

    ReadEncodedAsset(picture, &encodedAsset);
    f_SlideOffsets[0] = SCREEN_HEIGHT;
    f_IsControlsShown = TRUE;
    f_StateFrameCount = 0;
    f_ResumedSongId = GetSongId();
    PlaySong(SONG_CONTROLS, SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD);
}

void CloseControlsScreen(void) {
    FreeMemory(&f_NamePlateImage);
    f_IsControlsShown = FALSE;
    PlaySong(f_ResumedSongId, SONG_FLAG_FOLLOWS_MUSIC_OPTION | SONG_FLAG_UNREAD);
}

void DrawControlsScreen(Gfx **displayListHead) {
    Gfx *gfx;
    s32 strip;
    s32 topRaise;
    s32 bottomRaise;

    gfx = *displayListHead;
    if (f_IsControlsShown == TRUE) {
        gDPPipeSync(gfx++);
        gSPTexture(gfx++, TEXTURE_SCALE_HALF, TEXTURE_SCALE_HALF, 0, G_TX_RENDERTILE, G_ON);
        gDPPipeSync(gfx++);
        gDPSetAlphaCompare(gfx++, G_AC_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureLUT(gfx++, G_TT_RGBA16);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_NONE);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
        gDPSetCombineMode(gfx++, G_CC_MODULATEIA_PRIM, G_CC_MODULATEIA_PRIM);
        LoadLargePalette(&gfx, ImagePalette(f_NamePlateImage));
        for (strip = 0; strip != CONTROLS_STRIP_COUNT; strip++) {
            LoadTextureBlock(
                &gfx,
                LargeImageTexels(f_NamePlateImage) +
                    (strip * CONTROLS_STRIP_HEIGHT * CONTROLS_PICTURE_WIDTH),
                G_IM_FMT_CI,
                G_IM_SIZ_8b,
                CONTROLS_PICTURE_WIDTH,
                CONTROLS_STRIP_HEIGHT,
                0,
                G_TX_MIRROR,
                G_TX_MIRROR,
                G_TX_NOMASK,
                G_TX_NOMASK,
                G_TX_NOLOD,
                G_TX_NOLOD
            );
            topRaise = f_SlideOffsets[0] - SCREEN_BORDER;
            bottomRaise = f_SlideOffsets[0] - (SCREEN_BORDER + CONTROLS_STRIP_HEIGHT);
            DrawTextureRectangle(
                &gfx,
                QuarterPixels(CONTROLS_PICTURE_LEFT),
                QuarterPixels((strip * CONTROLS_STRIP_HEIGHT) - topRaise),
                QuarterPixels(CONTROLS_PICTURE_LEFT + CONTROLS_PICTURE_WIDTH),
                QuarterPixels((strip * CONTROLS_STRIP_HEIGHT) - bottomRaise),
                G_TX_RENDERTILE,
                0,
                0,
                TEXEL_STEP_ONE,
                TEXEL_STEP_ONE
            );
        }

        *displayListHead = gfx;
    }
}

void UpdateControlsScreen(ControllerState *controller) {
    f_StateFrameCount++;
    switch (f_MenuState) {
        case SAVE_MENU_STATE_OPENING:
            if (f_SlideOffsets[0] > 0) {
                if (f_SlideOffsets[0] == 1) {
                    f_SlideOffsets[0] = 0;
                }
                else {
                    f_SlideOffsets[0] += -f_SlideOffsets[0] / 2;
                }
            }

            if (f_StateFrameCount == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(
                    SOUND_CONTROLS_OPEN,
                    g_NextSoundChannel,
                    MENU_VOLUME_MEDIUM,
                    MENU_SOUND_REVERB,
                    SOUND_FULL_SCALE
                );
            }

            if (f_StateFrameCount == SLIDE_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_CHOOSING;
            }

            break;
        case SAVE_MENU_STATE_CHOOSING:
            if (controller->pressedButtons & (A_BUTTON | B_BUTTON)) {
                PlaySound(SOUND_MENU_CONFIRM, g_NextSoundChannel, MENU_VOLUME_MEDIUM);
                f_StateFrameCount = 0;
                f_MenuState = SAVE_MENU_STATE_BACKING_OUT;
            }

            break;
        case SAVE_MENU_STATE_BACKING_OUT:
            if (f_SlideOffsets[0] >= SLIDE_OUT_START_OFFSET) {
                f_SlideOffsets[0] -= SLIDE_OUT_START_STEP;
            }
            else {
                f_SlideOffsets[0] += f_SlideOffsets[0] / 2;
            }

            if (f_StateFrameCount == SLIDE_SOUND_FRAME) {
                PlaySoundWithEffects(
                    SOUND_CONTROLS_CLOSE,
                    g_NextSoundChannel,
                    MENU_VOLUME_MEDIUM,
                    MENU_SOUND_REVERB,
                    SOUND_FULL_SCALE
                );
            }

            if (f_StateFrameCount == SLIDE_FRAMES) {
                f_MenuState = SAVE_MENU_STATE_BACKED_OUT;
            }

            break;
        case SAVE_MENU_STATE_BACKED_OUT:
            CloseControlsScreen();
            g_PauseScreen = PAUSE_SCREEN_NONE;
            SetMenuBackdrop(MENU_BACKDROP_NONE);
            SetFramebufferCount(TRIPLE_BUFFERING);
            SetScreenEffect(SCREEN_EFFECT_NONE, 0);
            g_RoundHud.isLetterboxShown = TRUE;
            g_RoundHud.isClubPanelShown = TRUE;
            g_RoundHud.isWindIndicatorShown = TRUE;
            g_RoundHud.isIgnoredPausePartShown = FALSE;
            g_RoundHud.isHoleInfoShown = TRUE;
            g_RoundHud.isLieIconShown = TRUE;
            g_RoundHud.isClubHeadIconShown = TRUE;
            g_RoundHud.isUnreadItemShown = TRUE;
            g_RoundHud.isProgressShown = TRUE;
            break;
    }
}
