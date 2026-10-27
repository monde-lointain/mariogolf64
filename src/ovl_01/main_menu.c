#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "camera.h"
#include "character.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "course.h"
#include "cpu_projection.h"
#include "game_flags.h"
#include "hole_display.h"
#include "hole_loader.h"
#include "libc.h"
#include "math_util.h"
#include "menu.h"
#include "menu_memory.h"
#include "overlay_manager.h"
#include "round_hud.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "scoring.h"
#include "screen.h"
#include "screen_layers.h"
#include "sound.h"
#include "text.h"
#include "transfer_pak.h"
#include "variables.h"

/* The wind and light of the hole that loads behind the menus */
#define CHOSEN_HOLE_WIND_SPEED_MASK 0x3F
#define CHOSEN_HOLE_WIND_SPEED_STEP 6000
#define CHOSEN_HOLE_LIGHT_PITCH 0x1000
#define CHOSEN_HOLE_LIGHT_YAW 0x2000

/* The module that the menus draw as */
#define MODULE_MENU 2

/* The title screen's rows, which func_801F70F0 moves between */
#define TITLE_CHOICE_NEW_GAME 0
#define TITLE_CHOICE_SAVED_GAMES 1
#define TITLE_CHOICE_MENUS 2
#define TITLE_CHOICE_COUNT 3
/* Holding these as A is pressed on the menus row shows the Transmit choice in the main menu */
#define TRANSMIT_BUTTONS (A_BUTTON | L_TRIG | R_TRIG)
#define TRANSMIT_OTHER_BUTTONS (A_BUTTON | Z_TRIG | R_TRIG)
#define SOUND_TRANSMIT_SHOWN 0x73
#define TRANSMIT_SOUND_CHANNEL 14
/* The wipe that leaves for the Transmit scene from the saved games row */
#define TRANSMIT_WIPE_SHAPE 12
/* Where the title scene's text goes; pressing B clears it before going back there */
#define TITLE_SCREEN_TEXT_Y 114

/* How the menus' bobbing items move: the phase step per frame, and the height in pixels */
#define MENU_BOB_PHASE_STEP 0.13f
#define MENU_BOB_HEIGHT 5.0f

/* Labels in the shadowed font */
extern char g_ContinueLabelText[];
extern char g_BirdieBadgeLabelText[];

/* The bobbing of the menus' items: its phase, and the offset that it gives */
extern f32 sMenuBobOffset;
extern f32 sMenuBobPhase;

/* Where func_801FD7B0 and func_801FCEF8 draw the banners of D_801052BC and D_801B60D0 */
#define MENU_BANNER_TOP 18
#define UNUSED_BANNER_TOP 16

/* func_801FD1B4 squeezes D_801B60D0 to this width at a given position; nothing calls it */
#define SMALL_BANNER_WIDTH 96

/* func_801FD4F0 stretches D_801B60D0; its strips overlap, as they are taller than their spacing */
#define LARGE_BANNER_LEFT 26
#define LARGE_BANNER_TOP 174
#define LARGE_BANNER_WIDTH 270
#define LARGE_BANNER_STRIP_HEIGHT 20
#define LARGE_BANNER_STRIP_SPACING 16

/*
 * func_801FCB74 reads D_800FF420's buffer as a 160 texel wide image, though
 * menu.c loads the 256 texel wide board into it; its strips overlap, as each
 * samples its rows at 8/9 of a texel
 */
#define NARROW_BOARD_WIDTH 160
#define NARROW_BOARD_STRIP_HEIGHT 8
#define NARROW_BOARD_STRIP_COUNT 20
#define NARROW_BOARD_STRIP_SPACING 7
#define NARROW_BOARD_ROW_SCALE 9
#define NARROW_BOARD_LEFT 15
#define NARROW_BOARD_TOP 36
#define NARROW_BOARD_SHOWN_WIDTH 170

/*
 * func_801FC754 draws the course backdrop of D_801F4A20 at double size. Each
 * strip loads a row either side of the rows it shows, so that the filtering
 * blends across the seams.
 */
#define BACKDROP_WIDTH 152
#define BACKDROP_SCALE 2
#define BACKDROP_SEAM_ROW_COUNT 2
#define BACKDROP_SECOND_ROW_START (1 << 5)
#define BACKDROP_STRIP_ROW_COUNT 11
#define BACKDROP_STRIP_COUNT 11
#define BACKDROP_LEFT 8
#define BACKDROP_FIRST_TOP 8
#define BACKDROP_TOP 10

extern s8 D_800C1FF4[MAX_PLAYER_COUNT];
extern void *D_800FF420[2];
extern s32 D_801051F0;
extern void *D_801052BC[2];
extern void *D_801B60D0;
extern void *D_801F4A20;
extern s16 D_8022BE8C_ovl_01;
extern s16 D_8022BE92_ovl_01;
extern s32 D_8022F8C8_ovl_01;
extern s8 D_8022F8CD_ovl_01;
extern u8 D_8022F8CC_ovl_01; /* Which of D_801B93E0's buffers holds the star frame shown */
extern void *D_801B93E0[];
extern s32 D_8022F848_ovl_01[]; /* The star's frames */
extern u8 D_8022F8CE_ovl_01;
extern s32 D_80105B68;
extern s32 D_8022F8C0_ovl_01;
extern u8 D_8022F8C4_ovl_01;
extern s8 D_8022F8C5_ovl_01;
extern void *D_8010526C[2];
extern void *D_800FBE30[2];
extern u8 D_8022F8E0_ovl_01[MAX_PLAYER_COUNT + 1][MAX_PLAYER_COUNT];
/* The 24x24 icon that func_80207E28 animates, such as the birdie badge; each frame shows for two calls */
#define ANIMATED_ICON_SIZE 24
#define ANIMATED_ICON_IMAGE_SIZE (ANIMATED_ICON_SIZE * ANIMATED_ICON_SIZE / 2)
#define ANIMATED_ICON_FRAME_COUNT 6
#define ANIMATED_ICON_FRAME_DURATION 2
/* func_802081E8 draws a row of spinning stars, one per hole, from D_801B93E0, and enlarges the chosen hole's */
#define STAR_WIDTH 32
#define STAR_HEIGHT 24
#define STAR_FRAME_COUNT 15
#define MAX_STAR_COUNT 6
#define CHOSEN_STAR_GROWTH 8
/* func_80200A40 draws the chosen mode's icon, 64x64 CI8, in two strips */
#define MODE_ICON_WIDTH 64
#define MODE_ICON_STRIP_HEIGHT 32
#define MODE_ICON_STRIP_COUNT 2
#define MODE_ICON_FRAME_DURATION 4
/* Where func_802004A0 draws the mode icon, unscaled */
#define MODE_ICON_LEFT 168
#define MODE_ICON_TOP 96
/* The most modes the mode select offers for a player count */
#define MODE_CHOICE_LIMIT 12

extern u8 D_8022BEBC_ovl_01[MAX_PLAYER_COUNT][MODE_CHOICE_LIMIT];

/* func_80200EF0 draws the players' characters, and in Get Character the rival as a spinning silhouette */
#define NO_RIVAL -1
#define RIVAL_SLOT 1
#define RIVAL_SPIN_STEP 10.0f
#define MENU_CHARACTERS_NEAR 1.0f
#define MENU_CHARACTERS_FAR 3000.0f
#define MENU_CHARACTERS_ORTHO_SCALE 10.0f
#define MENU_CHARACTER_LAYOUT_COUNT 4
#define MENU_CHARACTER_SLOT_COUNT 5

extern f32 D_8022FA54_ovl_01;
extern f32 D_8022FA58_ovl_01;
extern f32 D_8022FA5C_ovl_01;
extern f32 D_8022FA68_ovl_01[MENU_CHARACTER_LAYOUT_COUNT][MENU_CHARACTER_SLOT_COUNT][3];
extern s32 D_8022FB58_ovl_01[CHARACTER_COUNT]; /* The character for each grid mark */
extern Mtx D_80231D60_ovl_01[MAX_PLAYER_COUNT][2];
extern Mtx D_80231F60_ovl_01;
extern f32 D_80231FA0_ovl_01[MAX_PLAYER_COUNT];
extern s32 D_8023BFC0_ovl_01[MAX_PLAYER_COUNT];
extern u8 D_801B70F0[MAXCONTROLLERS];

/* func_80208F8C draws a frame of the waving Toad (assets 0x710 onward), 48x51 CI8, in two strips */
#define TOAD_WIDTH 48
#define TOAD_STRIP_HEIGHT 30
#define TOAD_LAST_STRIP_HEIGHT 21
#define TOAD_STRIP_COUNT 2
/* The parts of the menu backdrop that func_801FC60C draws */
#define MENU_LAYER_UNK1 (1 << 0)
#define MENU_LAYER_UNK4 (1 << 2)
#define MENU_LAYER_UNK8 (1 << 3)
#define MENU_LAYER_UNK10 (1 << 4)

/* The choices of both main menus; the one without Transmit stops at its options */
#define MAIN_CHOICE_CLUBHOUSE 0
#define MAIN_CHOICE_HELP 1
#define MAIN_CHOICE_OPTIONS 2
#define MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT 3
#define MAIN_CHOICE_CLUB_TOURNAMENT 2
#define MAIN_CHOICE_FULL_OPTIONS 3
#define MAIN_CHOICE_COUNT 4

/* The clubhouse's rows, and the rooms that D_8022BF1C gives each of them */
#define CLUBHOUSE_CHOICE_COUNT 5
#define CLUBHOUSE_ROOM_COURSE_STATS 0
#define CLUBHOUSE_ROOM_UNK12 1
#define CLUBHOUSE_ROOM_UNK13 2
#define CLUBHOUSE_ROOM_BIRDIE_BADGES 3
#define CLUBHOUSE_ROOM_CHARACTER_TABLE 4
#define CLUBHOUSE_ROOM_BEST_TIMES 5

/* Past func_80217B88's presets, so that it clears the values instead */
#define MENU_UNKE_PRESET_NONE 10

/*
 * The menus set the top byte of g_MapCamera's view mode. Reading it as a
 * byte of the field rather than of the whole camera lets the compiler move
 * the read past display list words, as the original does.
 */
#define MAP_CAMERA_MODE_TOP_BYTE (*(u8 *)&g_MapCamera.view.mode)
/* Values of D_801B60B9, the replay that leaving the menus starts */
#define REPLAY_KIND_NONE 0
#define REPLAY_KIND_RING_SHOT 2
#define GAME_STATE_UNK13 13
/* The character select row of the Game Boy characters, whose column is the character's index */
#define GAME_BOY_CHARACTER_ROW 2
#define PLAYER_UNK28_TRAINING (-1)
#define PLAYER_UNK28_NINE_HOLES 3
#define PLAYER_UNK28_EIGHTEEN_HOLES 6

/* The choices of the list that func_801FAFEC runs, which no menu screen dispatches to */
#define UNUSED_LIST_CHOICE_COUNT 21

/* The debug caddy select: its two caddies, and where it draws its title and names */
#define CADDY_COUNT 2
#define CADDY_SELECT_TITLE_X 64
#define CADDY_SELECT_TITLE_Y 32
#define CADDY_SELECT_NAME_X 96
#define CADDY_SELECT_FIRST_Y 64
#define CADDY_SELECT_LINE_HEIGHT 8

extern char *D_8022C0D0_ovl_01[CADDY_COUNT];
extern const char D_8023176C_ovl_01[];
extern s8 D_801052E8;
extern s32 D_800FE334;
extern s8 D_801B60B9;
extern u8 D_801323E6;
extern s16 D_8022BF1C_ovl_01[CLUBHOUSE_CHOICE_COUNT];
extern s32 D_8022BE18_ovl_01;
extern s32 D_801B609C;
extern s32 D_800FBD40[];
extern MenuMemory D_801323A0;
extern s32 D_8022BE0C_ovl_01;
extern s32 D_8022BE28_ovl_01;
extern s32 D_8022BE2C_ovl_01;
extern s32 D_801B7070;
extern s32 D_8022BE84_ovl_01;
extern u8 D_8022BE88_ovl_01;
extern u8 D_8022BE89_ovl_01;
extern s32 D_8023BFD0_ovl_01;
extern s32 D_802322F0_ovl_01;
extern s32 D_802322F4_ovl_01;
extern s8 D_801B60B8;
extern s32 D_801B60A0;
extern s32 D_801B60A8;
extern s32 D_800C42D0;
extern s8 D_800FBE08[MAX_PLAYER_COUNT];
extern s16 D_801B71CC;
extern s16 D_800FBE2C;
extern u8 D_801B9410;
extern u8 D_800FF4C0;
extern s8 D_801B5634;
extern s8 D_801B60BA[MENU_UNKE_COUNT];
extern s16 D_8023BFDC_ovl_01[MAX_PLAYER_COUNT];
extern s32 D_8012F480[MAX_PLAYER_COUNT];
extern s32 D_8022BE1C_ovl_01;
extern u16 D_8022BE50_ovl_01[MAX_PLAYER_COUNT]; /* Each player's clubs, a bit per slot */
extern s32 D_8022F8F4_ovl_01[];
extern char D_8022BF70_ovl_01[][IMPORTED_CHARACTER_NAME_LENGTH];

/*
 * The mini-golf course select. A record's three courses lie on a disc that
 * turns from one to the next through five frames, each two images, read
 * into the buffers of D_801049A0 not shown; the courses face the screen at
 * the middle frame. Changing record slides the screen across instead.
 */
#define MINI_GOLF_RECORD_COUNT 2
#define MINI_GOLF_TURN_FRAME_COUNT 5
#define MINI_GOLF_FACING_FRAME 2
#define MINI_GOLF_TURN_FRAME_DURATION 2
#define MINI_GOLF_DISC_IMAGE_COUNT 2
/* D_8022F8DC values */
#define MINI_GOLF_TURN_NONE 0
#define MINI_GOLF_TURN_TO_PREVIOUS 1
#define MINI_GOLF_TURN_TO_NEXT 2
#define MINI_GOLF_RECORD_SLIDE 45
#define MINI_GOLF_RECORD_SLIDE_STEP 9
/* What g_MenuChoices.unk0 holds once the C buttons choose the holes to play */
#define HOLES_CHOICE_FRONT_NINE 1
#define HOLES_CHOICE_BACK_NINE 2
#define HOLES_CHOICE_ALL 3
/* D_8022BE88's value once the popup opened with C-up has finished */
#define MENU_PHASE_UNK2 2
/* In this phase, the course select's banners slide the other way */
#define MENU_PHASE_UNK7 7
#define SONG_UNK17 0x17
#define SOUND_UNK6A 0x6A
#define SOUND_HOLES_CHOSEN 0x45

extern u16 D_800C2098[MINI_GOLF_COURSES_PER_RECORD * MINI_GOLF_DISC_IMAGE_COUNT][MINI_GOLF_TURN_FRAME_COUNT];
extern void *D_801049A0[][MINI_GOLF_DISC_IMAGE_COUNT];
extern s32 D_800C2BD0;
extern u8 D_800C2BD5;
extern s32 D_8022F8D4_ovl_01; /* The disc's frame */
extern s32 D_8022F8D8_ovl_01; /* The calls that it has shown */
extern u8 D_8022F8DC_ovl_01;  /* Which way it turns */
extern u8 D_8022F8DD_ovl_01;  /* Which buffers it shows */

/*
 * The course select. Its wheel shows the open courses and the next one
 * still shut, and turns from one to the next; the chosen course's image
 * is read into the one of D_800FBDFC's buffers not shown, and its trophy
 * files into D_800FBE38.
 */
#define COURSE_SELECT_TURN 90
#define COURSE_SELECT_SPIN_SPEED 18
#define COURSE_SELECT_TEXT_SLOT_COUNT 4

extern void *D_800FBDFC[2];
extern u8 D_8022BE80_ovl_01; /* Which of D_800FBDFC's buffers shows the course */

s32 GetClubTournamentCourse(void);
s32 GetClubTournamentGameMode(void);
s32 GetOpenCourseCount(void);
s32 func_8022BDC4_ovl_01(void);
s32 func_8022BDD0_ovl_01(void);
s32 func_8021E468_ovl_01(void);
void LoadRingShotRecord(RingShotRecord *record);
void LoadSuperShotReplay(PlayRecord *record);
s32 SaveOrContinueGameByMode(s32 isContinuing);
s32 func_801F7660_ovl_01(s32 arg0, s32 playerNumber);
void func_80217B88_ovl_01(s32 requestedPreset);
void func_80228B60_ovl_01(void);
void func_801FB54C_ovl_01(Gfx **displayListHead, s32);
void func_801FC754_ovl_01(Gfx **displayListHead);
void func_801FCB74_ovl_01(Gfx **displayListHead);
void func_801FD4F0_ovl_01(Gfx **displayListHead);
void func_801FD7B0_ovl_01(Gfx **displayListHead);
void func_801FDA40_ovl_01(Gfx **displayListHead);
void func_801FE4D0_ovl_01(Gfx **displayListHead);
void func_80201574_ovl_01(Gfx **displayListHead);
void func_80202000_ovl_01(Gfx **displayListHead);
void func_80205038_ovl_01(Gfx **displayListHead);
void func_802058B8_ovl_01(Gfx **displayListHead);
void func_80206180_ovl_01(Gfx **displayListHead);
void func_80206A68_ovl_01(Gfx **displayListHead);
void func_80207D08_ovl_01(Gfx **displayListHead);
void func_80207E18_ovl_01(Gfx **displayListHead);
void DrawBirdieBadgeLabel(Gfx **displayListHead);
void func_8020B590_ovl_01(Gfx **displayListHead, u8 isSinglePlayer);
void func_8020E898_ovl_01(Gfx **displayListHead);
void func_80213748_ovl_01(Gfx **displayListHead);
void func_80215DC4_ovl_01(Gfx **displayListHead);
void func_8021D3AC_ovl_01(Gfx **displayListHead);
void func_8021FC5C_ovl_01(Gfx **displayListHead);
void func_802230C0_ovl_01(Gfx **displayListHead);
void func_80229CB0_ovl_01(Gfx **displayListHead);
void DrawHelpBoard(Gfx **displayListHead);

void *GetMenuImageBuffer(void) {
    return g_MenuImageBuffer;
}

void StartLoadingChosenHole(void) {
    u32 slotIndex;

    GetSaveData();
    slotIndex = 0;
    func_8005AF74();
    do {
        if (IsCharacterSlotBusy(slotIndex)) {
            FreeCharacterSlot(slotIndex);
        }
        slotIndex++;
    } while (slotIndex != CHARACTER_SLOT_COUNT);
    D_800FE334 = 1;
    if (g_MenuScreen == MENU_SCREEN_UNK23 || g_MenuScreen == MENU_SCREEN_RING_SHOT_RECORDS) {
        g_CourseIndex = g_MenuChoices.courseIndex;
        g_HoleIndex = g_MenuChoices.holeIndex;
    } else if (g_MenuScreen == MENU_SCREEN_CLUB_TOURNAMENT) {
        g_CourseIndex = GetClubTournamentCourse();
        g_IsMiniGolf = g_CourseIndex >= MINI_GOLF_COURSE_INDEX;
        g_HoleIndex = 0;
    } else if (g_GameMode == GAME_MODE_TRAINING) {
        g_CourseIndex = 0;
        g_HoleIndex = 0;
    } else {
        if (g_IsMiniGolf) {
            g_CourseIndex =
                g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX + g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD;
        } else {
            g_CourseIndex = D_801323A0.courseChoice;
        }
        g_HoleIndex = GetFirstHoleIndex();
    }
    D_801B609C = 0;
    g_HoleConditions.isRaining = FALSE;
    g_HoleConditions.windSpeed = (rand() & CHOSEN_HOLE_WIND_SPEED_MASK) * CHOSEN_HOLE_WIND_SPEED_STEP;
    g_HoleConditions.windDirection = rand();
    g_HoleConditions.sceneColorIndex = SCENE_COLOR_INDEX_DRIVING_RANGE;
    g_LightPitch = CHOSEN_HOLE_LIGHT_PITCH;
    g_LightYaw = CHOSEN_HOLE_LIGHT_YAW;
    SetGameFlag(GAME_FLAG_HOLE_LOADED);
    g_PlayerCount = 1;
    StopLoaderThread();
    StartLoaderThread();
}

s32 GetChosenCourseIndex(void) {
    if (g_IsMiniGolf) {
        return g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX + g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD;
    }
    return D_801323A0.courseChoice;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F4C78_ovl_01);

/*
 * Doesn't match: the original reloads g_PlayerCount for each pass of the
 * outer loop, where this keeps it in a register.
 */
#ifdef NON_MATCHING
void func_801F6A38_ovl_01(void) {
    s32 playerIndex;
    s32 otherIndex;

    D_801051F0 = TRUE;
    for (playerIndex = 0; playerIndex != g_PlayerCount; playerIndex++) {
        for (otherIndex = 0; otherIndex != g_PlayerCount; otherIndex++) {
            if (playerIndex != otherIndex && D_800C1FF4[playerIndex] != NO_CONTROLLER_INDEX &&
                D_800C1FF4[otherIndex] != NO_CONTROLLER_INDEX && D_800C1FF4[playerIndex] == D_800C1FF4[otherIndex]) {
                D_801051F0 = FALSE;
            }
        }
    }
    for (playerIndex = 0; playerIndex != g_PlayerCount; playerIndex++) {
        g_Players[playerIndex].soundPan = D_8022F8E0_ovl_01[g_PlayerCount][playerIndex];
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F6A38_ovl_01);
#endif

void func_801F6B20_ovl_01(void) {
}

const char D_80231514_ovl_01[] = "%s";

void func_801F6B28_ovl_01(void) {
    SaveData *save = GetSaveData();
    MenuMemory *memory = func_8005AF74();
    UnkB8 *player;
    s32 index;
    s32 slot;
    s32 clubSet;
    s32 *skippedNines;
    s32 row;
    s32 column;

    memset(&g_RoundHud, 0, sizeof(g_RoundHud));
    if (D_801B60B9 != REPLAY_KIND_NONE) {
        if (D_801B60B9 == REPLAY_KIND_RING_SHOT) {
            LoadRingShotRecord((RingShotRecord *)&save->unk1DF0[memory->ringShotReplaySlot]);
        } else {
            LoadSuperShotReplay((PlayRecord *)&save->rankedRecords[func_8021E468_ovl_01()]);
        }
        D_801B609C = 0;
        g_PlayerCount = 1;
        g_GameState.unk0 = 0;
        return;
    }
    StartBets();
    if (func_8022BDD0_ovl_01()) {
        g_SkippedNines = 0;
        g_CourseIndex = GetClubTournamentCourse();
        g_GameMode = GetClubTournamentGameMode();
        g_IsMiniGolf = g_CourseIndex >= MINI_GOLF_COURSE_INDEX;
        for (index = 0; index != MENU_UNKE_COUNT; index++) {
            D_801B60BA[index] = -1;
        }
        for (index = 0; index != MAX_PLAYER_COUNT; index++) {
            D_8023BFDC_ovl_01[index] = 0;
        }
        if (func_8022BDC4_ovl_01() != -1) {
            D_8012F480[0] = func_8022BDC4_ovl_01();
            D_800FBE08[0] = 0;
        }
    } else {
        if (g_GameMode == GAME_MODE_RING_SHOT) {
            g_CourseIndex = g_MenuChoices.courseIndex;
        } else if (g_GameMode == GAME_MODE_TRAINING) {
            g_CourseIndex = 0;
        } else {
            g_CourseIndex = GetChosenCourseIndex();
        }
        if (g_GameMode == GAME_MODE_STROKE || g_GameMode == GAME_MODE_MATCH_GAME || g_GameMode == GAME_MODE_RING_SHOT ||
            g_GameMode == GAME_MODE_SKINS_MATCH || g_GameMode == GAME_MODE_CLUB_SLOTS) {
            /* Through a pointer, as the original keeps the address in a register for every store */
            skippedNines = &g_SkippedNines;
            *skippedNines = D_8022BE1C_ovl_01;
            if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
                switch (g_MenuChoices.unk0) {
                    case 0:
                        *skippedNines = 0;
                        break;
                    case 1:
                        *skippedNines = BACK_NINE_SKIPPED;
                        break;
                    case 2:
                        *skippedNines = FRONT_NINE_SKIPPED;
                        break;
                    case 3:
                        *skippedNines = 0;
                        break;
                }
            }
        } else {
            g_SkippedNines = 0;
        }
    }
    D_801B609C = 0;
    g_PlayerCount = D_80105B68;
    if (g_GameMode == GAME_MODE_GET_CHARACTER) {
        g_PlayerCount = 2;
    }
    g_GameState.unk0 = GAME_STATE_UNK13;
    func_801F6A38_ovl_01();
    if (g_GameMode == GAME_MODE_TOURNAMENT) {
        StartRoundStats();
    }
    for (index = 0; index != g_PlayerCount; index++) {
        player = &g_Players[index];
        player->handicap = D_8023BFDC_ovl_01[index];
        player->unk22 = D_800FBE08[index];
        player->characterIndex = D_8012F480[index];
        player->controllerIndex = D_800C1FF4[index];
        sprintf(player->name, D_80231514_ovl_01, D_8022BF70_ovl_01[D_8022F8F4_ovl_01[D_8012F480[index]]]);
        if (D_8012F480[index] >= CHARACTER_KID) {
            row = D_801323A0.characterRows[index];
            column = D_801323A0.characterColumns[index];
            if (row == GAME_BOY_CHARACTER_ROW) {
                player->gameBoyCharacterIndex = column;
            }
            sprintf(player->name, D_80231514_ovl_01, g_GameBoyCharacters[player->gameBoyCharacterIndex].name);
        } else {
            player->gameBoyCharacterIndex = -1;
            sprintf(player->name, D_80231514_ovl_01, D_8022BF70_ovl_01[D_8022F8F4_ovl_01[D_8012F480[index]]]);
        }
        if (player->controllerIndex == MAXCONTROLLERS) {
            g_Players[index].unk1E[0] = 1;
        } else {
            g_Players[index].unk1E[0] = 0;
        }
        for (slot = 0; slot != CLUB_SET_SIZE; slot++) {
            if (!((D_8022BE50_ovl_01[index] >> slot) & 1)) {
                player->clubs[slot] = NO_CLUB;
            } else {
                clubSet = D_8012F480[index];
                if (clubSet >= CHARACTER_KID) {
                    clubSet = player->gameBoyCharacterIndex;
                    clubSet += CHARACTER_KID;
                }
                player->clubs[slot] = slot + clubSet * CLUB_SET_SIZE;
            }
        }
        for (; slot != CLUB_SLOT_COUNT; slot++) {
            player->clubs[slot] = NO_CLUB;
        }
        if (g_GameMode == GAME_MODE_TRAINING) {
            player->unk28 = PLAYER_UNK28_TRAINING;
        } else if (g_SkippedNines == BACK_NINE_SKIPPED || g_SkippedNines == FRONT_NINE_SKIPPED) {
            player->unk28 = PLAYER_UNK28_NINE_HOLES;
        } else {
            player->unk28 = PLAYER_UNK28_EIGHTEEN_HOLES;
        }
        LoadPlayerFace(index, (s8)player->characterIndex, (s8)player->paletteIndex);
    }
    if (g_GameMode == GAME_MODE_GET_CHARACTER) {
        g_Players[1].unk1E[0] = 1;
        g_Players[1].controllerIndex = MAXCONTROLLERS;
    }
}

void func_801F70F0_ovl_01(void) {
    MenuMemory *memory = func_8005AF74();
    s32 index;
    s8 titleChoice;

    if (IsHudAnimationDone() != TRUE) {
        return;
    }
    D_8022BE84_ovl_01 = 0;
    D_8022BE88_ovl_01 = 0;
    D_8022BE89_ovl_01 = 0;
    D_8023BFD0_ovl_01 = 0;
    D_802322F0_ovl_01 = 0;
    D_802322F4_ovl_01 = 0;
    D_801B60B8 = 0;
    D_801B60A0 = 0;
    g_IsMiniGolf = FALSE;
    D_801B60B9 = 0;
    D_801B60A8 = 0;
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_Scene = SCENE_SWAPPING_MODULES;
            g_SceneModuleToUnload = SCENE_MODULE_MENU;
            g_NextScene = SCENE_TRANSMIT;
            g_SceneModuleToLoad = SCENE_MODULE_TRANSMIT;
            ResetGameBoyTransfer(TRANSFER_MODE_FROM_MENU);
            StartWipe(WIPE_OPENING);
        }
        return;
    }
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 1) {
        return;
    }
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        titleChoice = D_801323A0.titleChoice;
        if (titleChoice == TITLE_CHOICE_NEW_GAME) {
            for (index = 0; index != MAX_PLAYER_COUNT; index++) {
                *((s8 *)memory + index + 3) = 0;
                *((s8 *)memory + index + 7) = 0;
            }
            memory->modeChoice = 0;
            memory->unkC = 0;
            memory->unkD = 0;
            for (index = 0; index != MENU_UNKE_COUNT; index++) {
                memory->unkE[index] = 0;
            }
            memory->courseChoice = 0;
            memory->courseSpin = 0;
            memory->holeIndex = 0;
            memory->courseIndex = 0;
            StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
            g_MenuScreen = MENU_SCREEN_CHARACTER_SELECT;
            D_80105B68 = D_801323A0.playerCountChoice + 1;
            for (index = 0; index != MAX_PLAYER_COUNT; index++) {
                D_800FBE08[index] = 0;
            }
            D_801B7070 = 0;
            for (index = 0; index != D_8022BE0C_ovl_01; index++) {
                D_800FBD40[index + 2] = 0;
            }
        } else if (titleChoice == TITLE_CHOICE_SAVED_GAMES) {
            if (D_800C42D0) {
                D_801B9410 = 0;
                LoadWipeShape(TRANSMIT_WIPE_SHAPE);
                StartWipe(WIPE_CLOSING);
                D_801052E8 = 0;
            } else {
                D_80105B68 = titleChoice;
                StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                g_MenuScreen = MENU_SCREEN_SAVED_GAMES;
            }
        } else if (titleChoice == TITLE_CHOICE_MENUS) {
            D_80105B68 = 1;
            if ((g_MenuRepeatButtons == TRANSMIT_BUTTONS) | (g_MenuRepeatButtons == TRANSMIT_OTHER_BUTTONS)) {
                PlaySound(SOUND_TRANSMIT_SHOWN, TRANSMIT_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                D_801323E6 = 1;
                g_MenuScreen = titleChoice;
            } else {
                D_801323E6 = 0;
                g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
            }
        }
        if (D_801B71CC != g_MenuScreen || D_800FBE2C != D_801323A0.playerCountChoice) {
            D_801323A0.modeChoice = 0;
            *(s16 *)D_800FBD40 = 0;
        }
        D_801052E8 = 0;
        D_801B9410 = 0;
        D_801B71CC = g_MenuScreen;
        D_800FBE2C = D_801323A0.playerCountChoice;
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_SPIRAL);
        g_Scene = SCENE_SWAPPING_MODULES;
        g_NextScene = SCENE_TITLE_SCREEN;
        g_SceneModuleToUnload = SCENE_MODULE_MENU;
        g_SceneModuleToLoad = SCENE_MODULE_TITLE_SCREEN;
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        SetTextSlot(SCREEN_WIDTH / 2, TITLE_SCREEN_TEXT_Y, NO_MESSAGE, 1, 0, 0, 0, 0, 0, 0);
        return;
    }
    /* titleChoice is moved through the array's bytes, which keeps its address in a register as the original does */
    if (g_MenuRepeatButtons & U_JPAD) {
        D_801323A0.titleChoice--;
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    } else if (g_MenuRepeatButtons & D_JPAD) {
        D_801323A0.titleChoice++;
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }
    if (D_801323A0.titleChoice < 0) {
        D_801323A0.titleChoice = TITLE_CHOICE_COUNT - 1;
    }
    if (D_801323A0.titleChoice >= TITLE_CHOICE_COUNT) {
        D_801323A0.titleChoice = 0;
    }
    if (D_801323A0.titleChoice == TITLE_CHOICE_NEW_GAME) {
        if (g_MenuRepeatButtons & L_JPAD) {
            D_801323A0.playerCountChoice--;
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        } else if (g_MenuRepeatButtons & R_JPAD) {
            D_801323A0.playerCountChoice++;
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        }
    }
    if (D_801323A0.playerCountChoice < 0) {
        D_801323A0.playerCountChoice = MAX_PLAYER_COUNT - 1;
    }
    if (D_801323A0.playerCountChoice >= MAX_PLAYER_COUNT) {
        D_801323A0.playerCountChoice = 0;
    }
}

void func_801F75EC_ovl_01(void) {
    D_801323A0.unk2 = 0;
    if (D_800FBD40[2] == 1) {
        do {
            D_801323A0.unk2++;
            if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
                D_801323A0.unk2 = 0;
            }
        } while (D_800FBD40[D_801323A0.unk2 + 2] == 1);
    }
}

/*
 * func_801F7660 moves the character cursor past the characters marked as taken, and plays the voice of the one
 * chosen. Nothing reads arg0.
 */
#define CHOSEN_CHARACTER_VOICE 24
#define CHOSEN_CHARACTER_PAN 127

s32 func_801F7660_ovl_01(s32 arg0, s32 playerNumber) {
    if (D_8022BE28_ovl_01 != 0) {
        while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
            D_801323A0.unk2++;
            if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
                D_801323A0.unk2 = 0;
            }
        }
        if (g_ControllerStates[0].pressedButtons & U_JPAD) {
            D_801323A0.unk2--;
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2--;
                if (D_801323A0.unk2 < 0) {
                    D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
                }
            }
        } else if (g_ControllerStates[0].pressedButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            D_801323A0.unk2++;
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2++;
                if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
                    D_801323A0.unk2 = 0;
                }
            }
        }
        if (D_801323A0.unk2 < 0) {
            D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2--;
            }
        }
        if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
            D_801323A0.unk2 = 0;
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2++;
            }
        }
    } else if (D_8022BE2C_ovl_01 == 1) {
        if (GetCurrentAnimationEndFrame(0) == GetAnimationFrame(0)) {
            D_8022BE2C_ovl_01 = 0;
            if (D_801B7070 == playerNumber) {
                return 1;
            }
        }
    } else if (!(g_ControllerStates[0].buttons & Z_TRIG)) {
        if ((g_MenuPressedButtons & (A_BUTTON | R_TRIG)) && D_800FBD40[D_801323A0.unk2 + 2] == 0) {
            PlayCharacterSound(D_801323A0.unk2, 0, CHOSEN_CHARACTER_VOICE, MENU_VOLUME_SOFT, CHOSEN_CHARACTER_PAN);
            D_8022BE2C_ovl_01 = 1;
            SetAnimationSpeed(0, 1.0f);
            return 0;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            return -1;
        }
        while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
            D_801323A0.unk2++;
            if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
                D_801323A0.unk2 = 0;
            }
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            D_801323A0.unk2--;
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2--;
                if (D_801323A0.unk2 < 0) {
                    D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
                }
            }
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_801323A0.unk2++;
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2++;
                if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
                    D_801323A0.unk2 = 0;
                }
            }
        }
        if (D_801323A0.unk2 < 0) {
            D_801323A0.unk2 = D_8022BE0C_ovl_01 - 1;
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2--;
            }
        }
        if (D_801323A0.unk2 > D_8022BE0C_ovl_01 - 1) {
            D_801323A0.unk2 = 0;
            while (D_800FBD40[D_801323A0.unk2 + 2] == 1) {
                D_801323A0.unk2++;
            }
        }
    }
    return 0;
}

void func_801F7CD0_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE && func_801F7660_ovl_01(1, 2) == -1) {
        D_801B7070 = isDone;
        g_MenuScreen = MENU_SCREEN_TITLE;
        FreeAllCharacterSlots();
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F7D28_ovl_01);

void func_801F8BAC_ovl_01(void) {
    EncodedAsset asset;
    AssetFile file;
    /* The open courses, and then those on the wheel */
    s32 courseCount = GetOpenCourseCount();
    s32 openCourseCount;
    s32 isDone;
    s32 index;
    s32 direction;
    s32 step;

    courseCount = courseCount < COURSE_COUNT ? courseCount + 1 : COURSE_COUNT;
    if (IsHudAnimationDone() != TRUE) {
        return;
    }
    if (D_8022BE88_ovl_01 == MENU_PHASE_UNK2) {
        PlaySong(SONG_UNK17, 0);
        g_MenuScreen = MENU_SCREEN_UNK22;
        D_8022BE88_ovl_01 = 1;
        D_8022BE89_ovl_01 = 0;
        return;
    }
    if (D_8022BE89_ovl_01 != 0 || D_8022BE88_ovl_01 != 0) {
        return;
    }
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
        return;
    }
    isDone = IsHudAnimationDone();
    if (isDone != TRUE) {
        return;
    }
    if (g_MenuPressedButtons & A_BUTTON) {
        if (D_801323A0.courseSpin != 0) {
            D_801323A0.courseSpin = 0;
            OpenEncodedAsset(D_800C21B8[D_801323A0.courseChoice][0], &asset);
            ReadEncodedAsset(D_800FBDFC[D_8022BE80_ovl_01 ^ 1], &asset);
            for (index = 0; index != COURSE_FILE_COUNT; index++) {
                OpenAssetFile(D_800C2278[D_801323A0.courseChoice][index], &file);
                ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
            }
            D_8022BE80_ovl_01 ^= 1;
            return;
        }
        if (D_801323A0.courseChoice < D_800C2BD0) {
            openCourseCount = GetOpenCourseCount();
            if (openCourseCount < COURSE_COUNT && openCourseCount == D_801323A0.courseChoice) {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                return;
            }
            for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            }
            D_801B9410 = 0;
            FreeAllCharacterSlots();
            LoadWipeShape(D_801323A0.courseChoice);
            StartWipe(WIPE_CLOSING);
            D_801052E8 = 0;
        }
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        D_801323A0.courseSpin = 0;
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        if (D_8022BEBC_ovl_01[D_80105B68 - 1][D_801323A0.modeChoice] == MODE_ICON_GET_CHARACTER) {
            g_MenuScreen = MENU_SCREEN_UNK16;
            D_801B7070--;
            D_800FBD40[D_8012F480[D_801B7070] + 2] = 0;
        } else {
            g_MenuScreen = MENU_SCREEN_UNK5;
        }
        MAP_CAMERA_MODE_TOP_BYTE = 0;
        D_800FE334 = 0;
        StopLoaderThread();
        FreeAllCharacterSlots();
        for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
            SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
        }
        return;
    }
    /* isDone is still TRUE, and the C-up test compares with it */
    if (D_801323A0.courseSpin == 0) {
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (--D_801323A0.courseChoice < 0) {
                D_801323A0.courseChoice = courseCount - 1;
            }
            D_801323A0.courseSpin = COURSE_SELECT_TURN / courseCount;
            for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            }
        } else if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (++D_801323A0.courseChoice > courseCount - 1) {
                D_801323A0.courseChoice = 0;
            }
            D_801323A0.courseSpin = -COURSE_SELECT_TURN / courseCount;
            for (index = 0; index != COURSE_SELECT_TEXT_SLOT_COUNT; index++) {
                SetTextSlot(0, 0, NO_MESSAGE, 1, index, 0, 0xFF, 0xFF, 0xFF, 0xFF);
            }
        } else if ((g_MenuRepeatButtons & U_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == isDone) {
            if (D_800C2BD5 != MAP_CAMERA_MODE_TOP_BYTE) {
                PlaySound(SOUND_UNK6A, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                D_8022BE88_ovl_01 = 1;
                D_8022BE89_ovl_01 = 1;
            } else {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            }
        } else if ((g_MenuRepeatButtons & D_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == 1) {
            PlaySound(SOUND_HOLES_CHOSEN, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
            g_SkippedNines = 0;
            StartLoadingChosenHole();
        } else if ((g_MenuRepeatButtons & L_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == 1) {
            PlaySound(SOUND_HOLES_CHOSEN, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
            g_SkippedNines = BACK_NINE_SKIPPED;
            StartLoadingChosenHole();
        } else if ((g_MenuRepeatButtons & R_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == 1) {
            PlaySound(SOUND_HOLES_CHOSEN, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuChoices.unk0 = HOLES_CHOICE_BACK_NINE;
            g_SkippedNines = FRONT_NINE_SKIPPED;
            StartLoadingChosenHole();
        }
    }
    if (ABS(D_801323A0.courseSpin) != 0) {
        step = COURSE_SELECT_SPIN_SPEED / courseCount;
        direction = D_801323A0.courseSpin < 0 ? -1 : 1;
        if (step == 0) {
            step = 1;
        }
        D_801323A0.courseSpin -= direction * step;
        /* Stop on reaching the course rather than turning past it */
        if ((D_801323A0.courseSpin < 0 ? -1 : 1) != direction) {
            D_801323A0.courseSpin = 0;
        }
        if (D_801323A0.courseSpin == 0) {
            OpenEncodedAsset(D_800C21B8[D_801323A0.courseChoice][0], &asset);
            ReadEncodedAsset(D_800FBDFC[D_8022BE80_ovl_01 ^ 1], &asset);
            for (index = 0; index != COURSE_FILE_COUNT; index++) {
                OpenAssetFile(D_800C2278[D_801323A0.courseChoice][index], &file);
                ReadAssetFile(D_800FBE38[index], GetAssetFileSize(&file), &file);
            }
            D_8022BE80_ovl_01 ^= 1;
        }
    }
}

/*
 * Doesn't match: the original loads the -1 and 1 that the last sign test
 * compares with before the test of the slide's sign, leaving 8 diff lines
 * (4 opcodes).
 */
#ifdef NON_MATCHING
void func_801F93D8_ovl_01(void) {
    EncodedAsset asset;
    s32 unused[4];
    s32 isDone;
    s32 course;
    s32 direction;
    s32 step;
    void *(*buffers)[MINI_GOLF_DISC_IMAGE_COUNT];

    isDone = IsHudAnimationDone();
    if (isDone != TRUE) {
        return;
    }
    if (D_8022F8DC_ovl_01 != MINI_GOLF_TURN_NONE) {
        switch (D_8022F8DC_ovl_01) {
            case MINI_GOLF_TURN_TO_PREVIOUS:
                if (++D_8022F8D8_ovl_01 == MINI_GOLF_TURN_FRAME_DURATION) {
                    D_8022F8D8_ovl_01 = 0;
                    if (++D_8022F8D4_ovl_01 >= MINI_GOLF_TURN_FRAME_COUNT) {
                        D_8022F8D4_ovl_01 = 0;
                    }
                    if (D_8022F8D4_ovl_01 >= 0 && D_8022F8D4_ovl_01 <= MINI_GOLF_FACING_FRAME) {
                        course = g_MenuChoices.unk14 - 1;
                        if (course < 0) {
                            course = MINI_GOLF_COURSES_PER_RECORD - 1;
                        }
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01], &asset);
                        ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][0], &asset);
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT + 1][D_8022F8D4_ovl_01],
                                         &asset);
                        ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][1], &asset);
                    } else {
                        course = g_MenuChoices.unk14;
                        buffers = D_801049A0;
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01], &asset);
                        ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][0], &asset);
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT + 1][D_8022F8D4_ovl_01],
                                         &asset);
                        ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][1], &asset);
                    }
                    D_8022F8DD_ovl_01 ^= 1;
                    if (D_8022F8D4_ovl_01 == MINI_GOLF_FACING_FRAME) {
                        if (--g_MenuChoices.unk14 < 0) {
                            g_MenuChoices.unk14 = MINI_GOLF_COURSES_PER_RECORD - 1;
                        }
                        D_8022F8DC_ovl_01 = MINI_GOLF_TURN_NONE;
                    }
                }
                break;
            case MINI_GOLF_TURN_TO_NEXT:
                if (++D_8022F8D8_ovl_01 == MINI_GOLF_TURN_FRAME_DURATION) {
                    D_8022F8D8_ovl_01 = 0;
                    if (--D_8022F8D4_ovl_01 < 0) {
                        D_8022F8D4_ovl_01 = MINI_GOLF_TURN_FRAME_COUNT - 1;
                    }
                    if (D_8022F8D4_ovl_01 >= MINI_GOLF_FACING_FRAME && D_8022F8D4_ovl_01 < MINI_GOLF_TURN_FRAME_COUNT) {
                        course = g_MenuChoices.unk14 + 1;
                        if (course >= MINI_GOLF_COURSES_PER_RECORD) {
                            course = 0;
                        }
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01], &asset);
                        ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][0], &asset);
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT + 1][D_8022F8D4_ovl_01],
                                         &asset);
                        ReadEncodedAsset(D_801049A0[D_8022F8DD_ovl_01 ^ 1][1], &asset);
                    } else {
                        course = g_MenuChoices.unk14;
                        buffers = D_801049A0;
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT][D_8022F8D4_ovl_01], &asset);
                        ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][0], &asset);
                        OpenEncodedAsset(D_800C2098[course * MINI_GOLF_DISC_IMAGE_COUNT + 1][D_8022F8D4_ovl_01],
                                         &asset);
                        ReadEncodedAsset(buffers[D_8022F8DD_ovl_01 ^ 1][1], &asset);
                    }
                    D_8022F8DD_ovl_01 ^= 1;
                    if (D_8022F8D4_ovl_01 == MINI_GOLF_FACING_FRAME) {
                        if (++g_MenuChoices.unk14 >= MINI_GOLF_COURSES_PER_RECORD) {
                            g_MenuChoices.unk14 = 0;
                        }
                        D_8022F8DC_ovl_01 = MINI_GOLF_TURN_NONE;
                    }
                }
                break;
        }
        return;
    }
    if (D_8022BE88_ovl_01 == MENU_PHASE_UNK2) {
        PlaySong(SONG_UNK17, 0);
        g_MenuScreen = MENU_SCREEN_UNK22;
        D_8022BE88_ovl_01 = 1;
        D_8022BE89_ovl_01 = 0;
        return;
    }
    if (D_8022BE89_ovl_01 != 0 || D_8022BE88_ovl_01 != 0) {
        return;
    }
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
        return;
    }
    isDone = IsHudAnimationDone();
    if (isDone != TRUE) {
        return;
    }
    if (g_MenuPressedButtons & A_BUTTON) {
        if (g_MenuChoices.unk15 != 0) {
            g_MenuChoices.unk15 = 0;
            return;
        }
        if (g_MenuChoices.unk13 < D_800C2BD0) {
            D_801B9410 = 0;
            FreeAllCharacterSlots();
            LoadWipeShape(g_MenuChoices.unk14 + g_MenuChoices.unk13 * MINI_GOLF_COURSES_PER_RECORD +
                          MINI_GOLF_COURSE_INDEX);
            StartWipe(WIPE_CLOSING);
            D_801052E8 = 0;
        }
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        StartHudAnimation(HUD_ANIMATION_FAST_FADE_OUT);
        g_MenuChoices.unk15 = 0;
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        g_IsMiniGolf = FALSE;
        g_MenuScreen = MENU_SCREEN_UNK5;
        MAP_CAMERA_MODE_TOP_BYTE = 0;
        D_800FE334 = 0;
        StopLoaderThread();
        FreeAllCharacterSlots();
        return;
    }
    /* isDone is still TRUE, and the C-up test compares with it */
    if (g_MenuChoices.unk15 == 0) {
        if (g_MenuRepeatButtons & L_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (--g_MenuChoices.unk13 < 0) {
                g_MenuChoices.unk13 = MINI_GOLF_RECORD_COUNT - 1;
            }
            g_MenuChoices.unk15 = MINI_GOLF_RECORD_SLIDE;
        } else if (g_MenuRepeatButtons & R_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            if (++g_MenuChoices.unk13 >= MINI_GOLF_RECORD_COUNT) {
                g_MenuChoices.unk13 = 0;
            }
            g_MenuChoices.unk15 = -MINI_GOLF_RECORD_SLIDE;
        } else if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_8022F8DC_ovl_01 = MINI_GOLF_TURN_TO_PREVIOUS;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            D_8022F8DC_ovl_01 = MINI_GOLF_TURN_TO_NEXT;
        } else if ((g_MenuRepeatButtons & U_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == isDone) {
            if (D_800C2BD5 != MAP_CAMERA_MODE_TOP_BYTE) {
                PlaySound(SOUND_UNK6A, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
                D_8022BE88_ovl_01 = 1;
                D_8022BE89_ovl_01 = 1;
            } else {
                PlaySound(SOUND_MENU_BLOCKED, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            }
        } else if ((g_MenuRepeatButtons & D_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == 1) {
            PlaySound(SOUND_HOLES_CHOSEN, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuChoices.unk0 = HOLES_CHOICE_ALL;
            g_SkippedNines = 0;
            StartLoadingChosenHole();
        } else if ((g_MenuRepeatButtons & L_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == 1) {
            PlaySound(SOUND_HOLES_CHOSEN, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuChoices.unk0 = HOLES_CHOICE_FRONT_NINE;
            g_SkippedNines = BACK_NINE_SKIPPED;
            StartLoadingChosenHole();
        } else if ((g_MenuRepeatButtons & R_CBUTTONS) && MAP_CAMERA_MODE_TOP_BYTE == 1) {
            PlaySound(SOUND_HOLES_CHOSEN, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuChoices.unk0 = HOLES_CHOICE_BACK_NINE;
            g_SkippedNines = FRONT_NINE_SKIPPED;
            StartLoadingChosenHole();
        }
    }
    if (ABS(g_MenuChoices.unk15) != 0) {
        direction = g_MenuChoices.unk15 >= 0 ? 1 : -1;
        step = MINI_GOLF_RECORD_SLIDE_STEP;
        g_MenuChoices.unk15 -= direction * step;
        /* Stop on reaching the record rather than sliding past it */
        if ((g_MenuChoices.unk15 >= 0 ? 1 : -1) != direction) {
            g_MenuChoices.unk15 = 0;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801F93D8_ovl_01);
#endif

void func_801F9BCC_ovl_01(void) {
    if (IsHudAnimationDone() == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            g_MenuScreen = MENU_SCREEN_EXIT;
            g_GameMode = GAME_MODE_STROKE;
            SaveOrContinueGameByMode(TRUE);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_TITLE;
        }
    }
}

void func_801F9C58_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (g_MenuChoices.mainChoice) {
                case MAIN_CHOICE_CLUBHOUSE:
                    g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                    break;
                case MAIN_CHOICE_CLUB_TOURNAMENT:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    func_80228B60_ovl_01();
                    g_MenuScreen = MENU_SCREEN_CLUB_TOURNAMENT;
                    break;
                case MAIN_CHOICE_HELP:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_HELP;
                    if (IsGameFlagSet(GAME_FLAG_UNK56)) {
                        g_GameMode = GAME_MODE_GET_CHARACTER;
                        g_PlayerCount = 2;
                        D_800FF4C0 = 1;
                        MAP_CAMERA_MODE_TOP_BYTE = 2;
                        D_801B5634 = 0;
                        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
                    }
                    break;
                case MAIN_CHOICE_FULL_OPTIONS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_OPTIONS;
                    break;
            }
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_TITLE;
            PlaySong(SONG_TITLE, 0);
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoice--;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoice++;
        }
        if (g_MenuChoices.mainChoice >= MAIN_CHOICE_COUNT) {
            g_MenuChoices.mainChoice = 0;
        }
        if (g_MenuChoices.mainChoice < 0) {
            g_MenuChoices.mainChoice = MAIN_CHOICE_COUNT - 1;
        }
    }
}

void func_801F9E38_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (g_MenuChoices.mainChoiceWithoutTransmit) {
                case MAIN_CHOICE_CLUBHOUSE:
                    g_MenuScreen = MENU_SCREEN_CLUBHOUSE;
                    break;
                case MAIN_CHOICE_HELP:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_HELP;
                    if (IsGameFlagSet(GAME_FLAG_UNK56)) {
                        g_GameMode = GAME_MODE_GET_CHARACTER;
                        g_PlayerCount = 2;
                        D_800FF4C0 = 1;
                        MAP_CAMERA_MODE_TOP_BYTE = 2;
                        D_801B5634 = 0;
                        func_80217B88_ovl_01(MENU_UNKE_PRESET_NONE);
                    }
                    break;
                case MAIN_CHOICE_OPTIONS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_OPTIONS;
                    break;
            }
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            g_MenuScreen = MENU_SCREEN_TITLE;
            PlaySong(SONG_TITLE, 0);
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoiceWithoutTransmit--;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.mainChoiceWithoutTransmit++;
        }
        if (g_MenuChoices.mainChoiceWithoutTransmit >= MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT) {
            g_MenuChoices.mainChoiceWithoutTransmit = 0;
        }
        if (g_MenuChoices.mainChoiceWithoutTransmit < 0) {
            g_MenuChoices.mainChoiceWithoutTransmit = MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT - 1;
        }
    }
}

void func_801F9FF8_ovl_01(void) {
    s32 isDone = IsHudAnimationDone();

    if (isDone == TRUE) {
        if (g_MenuPressedButtons & A_BUTTON) {
            switch (D_8022BF1C_ovl_01[g_MenuChoices.clubhouseChoice]) {
                case CLUBHOUSE_ROOM_COURSE_STATS:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_COURSE_STATS;
                    break;
                case CLUBHOUSE_ROOM_UNK12:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_UNK12;
                    break;
                case CLUBHOUSE_ROOM_UNK13:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_RING_SHOT_RECORDS;
                    break;
                case CLUBHOUSE_ROOM_BIRDIE_BADGES:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BIRDIE_BADGES;
                    break;
                case CLUBHOUSE_ROOM_CHARACTER_TABLE:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_CHARACTER_TABLE;
                    break;
                case CLUBHOUSE_ROOM_BEST_TIMES:
                    StartHudAnimation(HUD_ANIMATION_SLIDE_APART);
                    g_MenuScreen = MENU_SCREEN_BEST_TIMES;
                    break;
            }
            PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
            return;
        }
        if (g_MenuPressedButtons & B_BUTTON) {
            PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
            if (D_801323E6 == isDone) {
                g_MenuScreen = MENU_SCREEN_MAIN;
            } else {
                g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
            }
            PlaySong(SONG_TITLE, 0);
            return;
        }
        if (g_MenuRepeatButtons & U_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.clubhouseChoice--;
        } else if (g_MenuRepeatButtons & D_JPAD) {
            PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
            g_MenuChoices.clubhouseChoice++;
        }
        if (g_MenuChoices.clubhouseChoice >= CLUBHOUSE_CHOICE_COUNT) {
            g_MenuChoices.clubhouseChoice = 0;
        }
        if (g_MenuChoices.clubhouseChoice < 0) {
            g_MenuChoices.clubhouseChoice = CLUBHOUSE_CHOICE_COUNT - 1;
        }
    }
}

void func_801FA1B8_ovl_01(void) {
    if (IsHudAnimationDone() == TRUE && (g_MenuPressedButtons & B_BUTTON)) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FA20C_ovl_01);

void func_801FADBC_ovl_01(void) {
    if (IsHudAnimationDone() != TRUE) {
        return;
    }
    if (GetWipeKind() != WIPE_NONE) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
        return;
    }
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_MENU_START, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        StartWipe(WIPE_CLOSING);
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        if (D_801323A0.modeChoice == MENU_SCREEN_UNK5) {
            g_MenuScreen = D_801323A0.modeChoice;
        } else {
            g_MenuScreen = MENU_SCREEN_UNK6;
        }
        return;
    }
    if ((g_MenuRepeatButtons & U_JPAD) && g_MenuChoices.unk1 > 0) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        g_MenuChoices.unk1--;
    } else if ((g_MenuRepeatButtons & D_JPAD) && g_MenuChoices.unk1 < CADDY_COUNT - 1) {
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
        g_MenuChoices.unk1++;
    }
}

void LeaveMenuWithWipe(void) {
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
    } else {
        LoadWipeShape(0);
        StartWipe(WIPE_CLOSING);
        D_801052E8 = 0;
        D_801B9410 = 0;
    }
}

void LeaveMenuWithCourseWipe(void) {
    if (GetWipeKind() != WIPE_NONE && D_801B9410 == 0) {
        if (GetWipeProgress() == 0) {
            g_MenuScreen = MENU_SCREEN_EXIT;
        }
    } else {
        LoadWipeShape(g_MenuChoices.courseIndex);
        StartWipe(WIPE_CLOSING);
        D_801052E8 = 0;
        D_801B9410 = 0;
    }
}

void func_801FAFEC_ovl_01(void) {
    GetSaveData();
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_EXIT;
        D_800FE334 = 0;
        D_801B60B9 = 2;
        return;
    }
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        if (D_801323E6) {
            g_MenuScreen = MENU_SCREEN_MAIN;
        } else {
            g_MenuScreen = MENU_SCREEN_MAIN_WITHOUT_TRANSMIT;
        }
        return;
    }
    if (g_MenuRepeatButtons & U_JPAD) {
        D_8022BE18_ovl_01--;
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    } else if (g_MenuRepeatButtons & D_JPAD) {
        D_8022BE18_ovl_01++;
        PlaySound(SOUND_MENU_CURSOR, MENU_SOUND_CHANNEL, MENU_VOLUME_MEDIUM);
    }
    if (D_8022BE18_ovl_01 < 0) {
        D_8022BE18_ovl_01 = UNUSED_LIST_CHOICE_COUNT - 1;
    }
    if (D_8022BE18_ovl_01 >= UNUSED_LIST_CHOICE_COUNT) {
        D_8022BE18_ovl_01 = 0;
    }
}

void func_801FB11C_ovl_01(void) {
    if (g_MenuPressedButtons & B_BUTTON) {
        PlaySound(SOUND_MENU_CANCEL, MENU_SOUND_CHANNEL, MENU_VOLUME_FULL);
        g_MenuScreen = MENU_SCREEN_MAIN;
    }
}

void func_801FB15C_ovl_01(void) {
    if (g_MenuPressedButtons & A_BUTTON) {
        PlaySound(SOUND_CONFIRM, MENU_SOUND_CHANNEL, MENU_VOLUME_SOFT);
        g_MenuScreen = MENU_SCREEN_TITLE;
    }
}

void DrawMenuScreen(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    g_ModuleRdpRequestFrames[MODULE_MENU] = g_FrameCount;
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetFillColor(gfx++, GPACK_RGBA5551(0, 0, 0, 1) << 16 | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(gfx++, SCREEN_BORDER, SCREEN_BORDER, SCREEN_WIDTH - SCREEN_BORDER - 1,
                     SCREEN_HEIGHT - SCREEN_BORDER - 1);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    sMenuBobPhase += MENU_BOB_PHASE_STEP;
    sMenuBobOffset = sinf(sMenuBobPhase) * MENU_BOB_HEIGHT;
    switch (g_MenuScreen) {
        case MENU_SCREEN_TITLE:
            SetTextSlot(0, 0, NO_MESSAGE, 1, 0, 1, 255, 255, 255, 255);
            func_801FDA40_ovl_01(&gfx);
            break;
        case 1:
        case 24:
            func_8020B590_ovl_01(&gfx, 0);
            break;
        case 16:
            func_8020B590_ovl_01(&gfx, 1);
            break;
        case 3:
            func_802230C0_ovl_01(&gfx);
            break;
        case MENU_SCREEN_MAIN:
            func_80205038_ovl_01(&gfx);
            break;
        case MENU_SCREEN_MAIN_WITHOUT_TRANSMIT:
            func_802058B8_ovl_01(&gfx);
            break;
        case MENU_SCREEN_CLUB_TOURNAMENT:
            func_80229CB0_ovl_01(&gfx);
            break;
        case MENU_SCREEN_CLUBHOUSE:
            func_80206180_ovl_01(&gfx);
            break;
        case MENU_SCREEN_OPTIONS:
            func_80206A68_ovl_01(&gfx);
            break;
        case MENU_SCREEN_HELP:
            if (IsGameFlagSet(GAME_FLAG_UNK56)) {
                func_80201574_ovl_01(&gfx);
            } else {
                DrawHelpBoard(&gfx);
            }
            break;
        case 8:
            func_80207D08_ovl_01(&gfx);
            break;
        case 6:
        case 20:
        case 22:
            func_80202000_ovl_01(&gfx);
            break;
        case 18:
            func_80207E18_ovl_01(&gfx);
            break;
        case MENU_SCREEN_UNK12:
            func_8021D3AC_ovl_01(&gfx);
            break;
        case MENU_SCREEN_RING_SHOT_RECORDS:
            func_8021FC5C_ovl_01(&gfx);
            break;
        case MENU_SCREEN_COURSE_STATS:
            func_8020E898_ovl_01(&gfx);
            break;
        case MENU_SCREEN_BEST_TIMES:
            func_80213748_ovl_01(&gfx);
            break;
        case MENU_SCREEN_BIRDIE_BADGES:
            DrawBirdieBadgeLabel(&gfx);
            break;
        case MENU_SCREEN_CHARACTER_TABLE:
            func_80215DC4_ovl_01(&gfx);
            break;
        case 5:
        case MENU_SCREEN_UNK23:
        case 26:
            func_801FE4D0_ovl_01(&gfx);
            break;
    }
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FB54C_ovl_01);

void func_801FC60C_ovl_01(Gfx **displayListHead, s32 layers) {
    Gfx *gfx = *displayListHead;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    if (layers & MENU_LAYER_UNK1) {
        func_801FC754_ovl_01(&gfx);
    }
    if (layers & MENU_LAYER_UNK4) {
        func_801FCB74_ovl_01(&gfx);
    }
    if (layers & MENU_LAYER_UNK10) {
        func_801FD4F0_ovl_01(&gfx);
    }
    if (layers & MENU_LAYER_UNK8) {
        func_801FD7B0_ovl_01(&gfx);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_801FC754_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 shownRowCount;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801F4A20 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != BACKDROP_STRIP_COUNT; stripIndex++) {
        /* The image ends in a strip of one row */
        shownRowCount = (stripIndex != BACKDROP_STRIP_COUNT - 1) ? BACKDROP_STRIP_ROW_COUNT : 1;
        if (stripIndex == 0) {
            /* The first strip has no row above it, so it shows its first row too */
            gDPLoadTextureBlock(gfx++, ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
                                G_IM_SIZ_8b, BACKDROP_WIDTH, BACKDROP_STRIP_ROW_COUNT + BACKDROP_SEAM_ROW_COUNT, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, BACKDROP_FIRST_TOP << 2,
                                    (BACKDROP_LEFT + BACKDROP_WIDTH * BACKDROP_SCALE) << 2,
                                    (BACKDROP_FIRST_TOP + (BACKDROP_STRIP_ROW_COUNT + 1) * BACKDROP_SCALE) << 2,
                                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE / BACKDROP_SCALE,
                                    TEXEL_STEP_ONE / BACKDROP_SCALE);
        } else {
            gDPPipeSync(gfx++);
            gDPLoadTextureBlock(gfx++,
                                ((u32)D_801F4A20 + LARGE_PALETTED_TEXELS_OFFSET +
                                 stripIndex * BACKDROP_WIDTH * BACKDROP_STRIP_ROW_COUNT) & ~7,
                                G_IM_FMT_CI, G_IM_SIZ_8b, BACKDROP_WIDTH, shownRowCount + BACKDROP_SEAM_ROW_COUNT, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            stripTop = BACKDROP_TOP + stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE;
            gSPScisTextureRectangle(gfx++, BACKDROP_LEFT << 2, stripTop << 2,
                                    (BACKDROP_LEFT + BACKDROP_WIDTH * BACKDROP_SCALE) << 2,
                                    (BACKDROP_TOP + stripIndex * BACKDROP_STRIP_ROW_COUNT * BACKDROP_SCALE +
                                     shownRowCount * BACKDROP_SCALE)
                                        << 2,
                                    G_TX_RENDERTILE, 0, BACKDROP_SECOND_ROW_START, TEXEL_STEP_ONE / BACKDROP_SCALE,
                                    TEXEL_STEP_ONE / BACKDROP_SCALE);
        }
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

void func_801FCB74_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_800FF420[D_8022BE92_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != NARROW_BOARD_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_800FF420[D_8022BE92_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * NARROW_BOARD_WIDTH * NARROW_BOARD_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, NARROW_BOARD_WIDTH, NARROW_BOARD_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = NARROW_BOARD_TOP + stripIndex * NARROW_BOARD_STRIP_SPACING;
        gSPScisTextureRectangle(gfx++, NARROW_BOARD_LEFT << 2, stripTop << 2,
                                (NARROW_BOARD_LEFT + NARROW_BOARD_SHOWN_WIDTH) << 2,
                                (stripTop + NARROW_BOARD_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (NARROW_BOARD_WIDTH << 10) / NARROW_BOARD_SHOWN_WIDTH,
                                (NARROW_BOARD_STRIP_HEIGHT << 10) / NARROW_BOARD_ROW_SCALE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/* Nothing in the menu calls this; menu.c loads its image, asset 0x521 */
void func_801FCEF8_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = UNUSED_BANNER_TOP + stripIndex * MENU_BANNER_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
                                (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
                                (stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/* Doesn't match: the original tests the s32 top << 2 for the texture's row offset, not the s16 top */
#ifdef NON_MATCHING
void func_801FD1B4_ovl_01(Gfx **displayListHead, s16 left, s16 top) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, top << 2, (left + SMALL_BANNER_WIDTH) << 2,
                                (top + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (MENU_BANNER_WIDTH << 10) / SMALL_BANNER_WIDTH, TEXEL_STEP_ONE);
        top += MENU_BANNER_STRIP_HEIGHT;
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FD1B4_ovl_01);
#endif

/* Nothing in the menu calls this either */
void func_801FD4F0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    s32 stripTop;

    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = LARGE_BANNER_TOP + stripIndex * LARGE_BANNER_STRIP_SPACING;
        gSPScisTextureRectangle(gfx++, LARGE_BANNER_LEFT << 2, stripTop << 2,
                                (LARGE_BANNER_LEFT + LARGE_BANNER_WIDTH) << 2,
                                (stripTop + LARGE_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (MENU_BANNER_WIDTH << 10) / LARGE_BANNER_WIDTH,
                                (MENU_BANNER_STRIP_HEIGHT << 10) / LARGE_BANNER_STRIP_HEIGHT);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

void func_801FD7B0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 stripIndex;
    u8 *image = D_801052BC[D_8022BE8C_ovl_01];
    s32 stripTop;

    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = MENU_BANNER_TOP + stripIndex * MENU_BANNER_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, MENU_BANNER_LEFT << 2, stripTop << 2,
                                (MENU_BANNER_LEFT + MENU_BANNER_WIDTH) << 2,
                                (stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

/* The chosen label grows two pixels a frame until it passes the limit, then shrinks until it passes 0 */
#define CHOSEN_LABEL_GROWTH_LIMIT 6
#define CHOSEN_LABEL_GROWTH_STEP 2
#define LABEL_GROWING 0
#define LABEL_SHRINKING 1

extern s16 D_8022BE8E_ovl_01;
extern s16 D_8022BE90_ovl_01;
extern void *D_8012D420[2];
extern void *D_801B5CD0[2];
extern s16 D_800FE33C;
extern u8 D_800FBDB9;

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FDA40_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_801FE4D0_ovl_01);

/* Nothing calls this */
void func_802004A0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 stripIndex;
    void *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPPipeSync(gfx++);
    gSPTexture(gfx++, 0x8000, 0x8000, 0, G_TX_RENDERTILE, G_ON);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    image = D_8010526C[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, MODE_ICON_LEFT << 2,
                                (MODE_ICON_TOP + stripIndex * MODE_ICON_STRIP_HEIGHT) << 2,
                                (MODE_ICON_LEFT + MODE_ICON_WIDTH) << 2,
                                (MODE_ICON_TOP + MODE_ICON_STRIP_HEIGHT + stripIndex * MODE_ICON_STRIP_HEIGHT) << 2,
                                G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        gDPPipeSync(gfx++);
    }
    D_8022F8C5_ovl_01++;
    if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
        iconIndex = D_8022BEBC_ovl_01[D_80105B68 - 1][D_801323A0.modeChoice];
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01++;
        if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
            D_8022F8C0_ovl_01 = 0;
        }
        if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] == END_OF_ICON_FRAMES) {
            D_8022F8C0_ovl_01 = 0;
        }
        D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
    }
    *displayListHead = gfx;
}

void func_80200A40_ovl_01(Gfx **displayListHead, s32 left, s32 top, s32 halfSize, s32 scale) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    s32 stripIndex;
    void *image;
    u8 iconIndex;

    gDPPipeSync(gfx++);
    image = D_8010526C[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal256(gfx++, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != MODE_ICON_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)image + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MODE_ICON_WIDTH * MODE_ICON_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MODE_ICON_WIDTH, MODE_ICON_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, (top + stripIndex * halfSize) << 2, (left + halfSize + halfSize) << 2,
                                (top + halfSize + stripIndex * halfSize) << 2, G_TX_RENDERTILE, 0, 0, scale, scale);
        gDPPipeSync(gfx++);
    }
    D_8022F8C5_ovl_01++;
    if (D_8022F8C5_ovl_01 == MODE_ICON_FRAME_DURATION) {
        iconIndex = D_8022BEBC_ovl_01[D_80105B68 - 1][D_801323A0.modeChoice];
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01++;
        if (D_8022F8C0_ovl_01 >= MODE_ICON_FRAME_COUNT) {
            D_8022F8C0_ovl_01 = 0;
        }
        if (g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01] == END_OF_ICON_FRAMES) {
            D_8022F8C0_ovl_01 = 0;
        }
        D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
        OpenEncodedAsset(g_ModeIconAnimations[iconIndex][D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
    }
    *displayListHead = gfx;
}

/*
 * Doesn't match: the original fills the delay slot of the rival test before
 * SetCharacterLighting with its first argument, and this leaves it empty.
 */
#ifdef NON_MATCHING
void func_80200EF0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 rival = NO_RIVAL;
    MenuMemory *memory;
    s32 characterCount;
    s32 row;
    s32 column;
    s32 layout;
    s32 character;
    s32 palette;
    s32 raise;
    u16 slot;
    Vec3 translation;
    Vec3 rotation;
    Vec3 scale;
    s32 unused[12];
    Mat4 transform;

    memory = func_8005AF74();
    characterCount = D_80105B68;
    if (D_8022BEBC_ovl_01[characterCount - 1][memory->modeChoice] == MODE_ICON_GET_CHARACTER) {
        if (characterCount == 1) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                if (g_CharacterGrid[row][column] < MARKED_CHARACTER_LIMIT) {
                    rival = D_8022FB58_ovl_01[g_CharacterGrid[row][column] + MARKED_CHARACTER_OFFSET];
                }
            }
        }
    }
    }
    characterCount += (rival >= 0);
    switch (characterCount) {
        case 1:
            D_80231FA0_ovl_01[0] = 5.0f;
            break;
        case 2:
            D_80231FA0_ovl_01[0] = (rival >= 0) ? 5.0f : 4.0f;
            D_80231FA0_ovl_01[1] = 4.0f;
            break;
        case 3:
        case 4:
            D_80231FA0_ovl_01[0] = 3.5f;
            D_80231FA0_ovl_01[1] = 3.4f;
            D_80231FA0_ovl_01[2] = 3.4f;
            D_80231FA0_ovl_01[3] = 3.5f;
            break;
        default:
            D_80231FA0_ovl_01[0] = 4.0f;
            D_80231FA0_ovl_01[1] = 4.0f;
            D_80231FA0_ovl_01[2] = 4.0f;
            D_80231FA0_ovl_01[3] = 4.0f;
            break;
    }
    guOrtho(&D_80231F60_ovl_01, -(SCREEN_WIDTH / 2), SCREEN_WIDTH / 2, -(SCREEN_HEIGHT / 2), SCREEN_HEIGHT / 2,
            MENU_CHARACTERS_NEAR, MENU_CHARACTERS_FAR, MENU_CHARACTERS_ORTHO_SCALE);
    for (slot = 0; slot != characterCount; slot++) {
        if (D_8023BFC0_ovl_01[slot] > 0) {
            D_8023BFC0_ovl_01[slot]--;
        }
        if (D_8023BFC0_ovl_01[slot] == 0) {
            D_8023BFC0_ovl_01[slot] = -1;
            SetAnimationLooping(slot, FALSE);
            SetNextAnimation(slot, MENU_ANIMATION_FIDGET);
        }
        if (D_8023BFC0_ovl_01[slot] == -1) {
            if (GetAnimationEndFrame(slot, MENU_ANIMATION_FIDGET) == GetAnimationFrame(slot)) {
                SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
                SetAnimationLooping(slot, TRUE);
                D_8023BFC0_ovl_01[slot] = guRandom() % FIDGET_DELAY_RANGE;
            }
        }
        if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
            character = rival;
            palette = 1;
        } else {
            character = (s8)g_Players[slot].characterIndex;
            palette = (s8)g_Players[slot].paletteIndex;
        }
        if (!D_801B70F0[slot]) {
            if (IsCharacterSlotBusy(slot)) {
                FreeCharacterSlot(slot);
            } else {
                LoadCharacter(slot, character + CHARACTER_COUNT, palette);
                D_801B70F0[slot] = TRUE;
                SetCharacterAnimation(slot, MENU_ANIMATION_STANDING);
                SetAnimationSpeed(slot, 1.0f);
                if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
                    SetCharacterLighting(RIVAL_SLOT, CHARACTER_LIGHTING_BLACK);
                }
            }
        }
        layout = 0;
        if (rival < 0) {
            layout = characterCount - 1;
        }
        if ((characterCount == 2) & (rival >= 0) && slot == RIVAL_SLOT) {
            D_8022FA5C_ovl_01 -= RIVAL_SPIN_STEP;
            D_8022FA58_ovl_01 = D_8022FA5C_ovl_01;
        } else {
            D_8022FA58_ovl_01 = 0.0f;
        }
        rotation[0] = DEGREES_TO_RADIANS(D_8022FA54_ovl_01);
        rotation[1] = DEGREES_TO_RADIANS(D_8022FA58_ovl_01);
        rotation[2] = 0.0f;
        translation[0] = D_8022FA68_ovl_01[layout][slot][0];
        raise = (character == CHARACTER_BABY_MARIO) ? BABY_MARIO_RAISE : 0;
        translation[1] = D_8022FA68_ovl_01[layout][slot][1] + raise;
        translation[2] = D_8022FA68_ovl_01[layout][slot][2];
        scale[0] = D_80231FA0_ovl_01[slot];
        scale[1] = D_80231FA0_ovl_01[slot];
        scale[2] = D_80231FA0_ovl_01[slot];
        BuildTransformMatrix(transform, rotation, translation, scale);
        ConvertMatrixToFixed(transform, &D_80231D60_ovl_01[slot][g_DrawBufferIndex]);
        gDPPipeSync(gfx++);
        gDPSetTexturePersp(gfx++, G_TP_PERSP);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        gDPPipeSync(gfx++);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231F60_ovl_01), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
        gSPMatrix(gfx++, OS_K0_TO_PHYSICAL(&D_80231D60_ovl_01[slot][g_DrawBufferIndex]),
                  G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        g_DisplayListHead = gfx;
        DrawCharacter(slot);
        *displayListHead = g_DisplayListHead;
        gfx = *displayListHead;
        AnimateCharacter(slot);
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80200EF0_ovl_01);
#endif

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80201574_ovl_01);

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80202000_ovl_01);

/*
 * func_802047F0 draws the course select's banners of D_801B60D0: one behind
 * the course's name, and while the holes can be chosen, a wider one below
 * it behind the choice made, which squeezes the first.
 */
#define COURSE_NAME_BANNER_LEFT 67
#define COURSE_NAME_BANNER_TOP 168
#define COURSE_NAME_BANNER_WIDTH 190
#define COURSE_NAME_BANNER_SQUEEZE 48
#define HOLES_BANNER_LEFT 47
#define HOLES_BANNER_TOP 190
#define HOLES_BANNER_WIDTH 230
#define HOLES_BANNER_STRIP_HEIGHT 11
#define COURSE_NAME_Y 176
#define HOLES_CHOICE_Y 202
/* A HOLES_CHOICE_* value plus this is the message that explains it */
#define HOLES_CHOICE_MESSAGE_BASE 0xC3
#define HOLES_CHOICE_TEXT_SLOT 1

extern const char D_80231660_ovl_01[];
extern char *D_8022FDEC_ovl_01[MINI_GOLF_RECORD_COUNT];
extern s32 g_CourseNameEnvColors[];
extern s32 g_CourseNamePrimColors[];

/*
 * Doesn't match: the original sets the text drawing's argument before the
 * mini-golf test that picks the name's colours, and keeps the course name
 * banner's strip top in one register instead of copying it.
 */
#ifdef NON_MATCHING
void func_802047F0_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 slide = D_8023BFD0_ovl_01;
    /* Also the course's index into the name colours, to match the original's registers */
    s32 stripIndex;
    s32 stripTop;
    s32 squeeze;
    s32 cameraModeTopByte;

    if (D_8022BE88_ovl_01 == MENU_PHASE_UNK7) {
        slide = -slide;
    }
    gDPPipeSync(gfx++);
    gDPSetTextureLUT(gfx++, G_TT_RGBA16);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_801B60D0 + IMAGE_HEADER_SIZE) & ~7);
    if (MAP_CAMERA_MODE_TOP_BYTE == 1) {
        for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
            gDPLoadTextureBlock(gfx++,
                                ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                                 stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                                G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                                G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            stripTop = HOLES_BANNER_TOP + stripIndex * HOLES_BANNER_STRIP_HEIGHT;
            gSPScisTextureRectangle(gfx++, HOLES_BANNER_LEFT << 2, (slide + stripTop) << 2,
                                    (HOLES_BANNER_LEFT + HOLES_BANNER_WIDTH) << 2,
                                    (slide + stripTop + HOLES_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                    (MENU_BANNER_WIDTH << 10) / HOLES_BANNER_WIDTH,
                                    (MENU_BANNER_STRIP_HEIGHT << 10) / HOLES_BANNER_STRIP_HEIGHT);
            gDPPipeSync(gfx++);
        }
    }
    squeeze = MAP_CAMERA_MODE_TOP_BYTE != 0 ? COURSE_NAME_BANNER_SQUEEZE : 0;
    for (stripIndex = 0; stripIndex != MENU_BANNER_STRIP_COUNT; stripIndex++) {
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_801B60D0 + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * MENU_BANNER_WIDTH * MENU_BANNER_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, MENU_BANNER_WIDTH, MENU_BANNER_STRIP_HEIGHT, 0,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        stripTop = COURSE_NAME_BANNER_TOP + stripIndex * MENU_BANNER_STRIP_HEIGHT;
        gSPScisTextureRectangle(gfx++, (COURSE_NAME_BANNER_LEFT + squeeze / 2) << 2, (slide + stripTop) << 2,
                                (COURSE_NAME_BANNER_LEFT + COURSE_NAME_BANNER_WIDTH - squeeze / 2) << 2,
                                (slide + stripTop + MENU_BANNER_STRIP_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (MENU_BANNER_WIDTH << 10) / (COURSE_NAME_BANNER_WIDTH - squeeze), TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    SetUpTextDrawing(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    if (!g_IsMiniGolf) {
        if (D_800FF4C0 == 1) {
            sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
                    g_CourseRecords[D_801323A0.courseChoice].name);
        } else {
            sprintf(g_PrintBuffer, D_80231660_ovl_01, D_801323A0.courseChoice + 1,
                    g_TournamentNames[D_801323A0.courseChoice]);
        }
    } else {
        sprintf(g_PrintBuffer, D_80231660_ovl_01, g_MenuChoices.unk13 + 1, D_8022FDEC_ovl_01[g_MenuChoices.unk13]);
    }
    SetUpGradientTextDrawing(&gfx, 0);
    if (g_IsMiniGolf == TRUE) {
        stripIndex = g_MenuChoices.unk14 + MINI_GOLF_COURSE_INDEX;
    } else {
        stripIndex = D_801323A0.courseChoice;
    }
    DrawLargeTextWithShadow(&gfx, SCREEN_WIDTH / 2, slide + COURSE_NAME_Y, g_PrintBuffer,
                            g_CourseNameEnvColors[stripIndex], g_CourseNamePrimColors[stripIndex],
                            TEXT_FLAG_CENTERED);
    cameraModeTopByte = MAP_CAMERA_MODE_TOP_BYTE;
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    if (cameraModeTopByte == 1) {
        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        SetTextColor(&gfx, 255, 255, 255);
        SetTextSlot(SCREEN_WIDTH / 2, slide + HOLES_CHOICE_Y, g_MenuChoices.unk0 + HOLES_CHOICE_MESSAGE_BASE, 1,
                    HOLES_CHOICE_TEXT_SLOT, TEXT_FLAG_CENTERED | TEXT_FLAG_UNK20, 255, 255, 255, 255);
        DrawLargeTextSlot(&gfx, HOLES_CHOICE_TEXT_SLOT);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802047F0_ovl_01);
#endif

void DrawContinueLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 96, 64, g_ContinueLabelText);
    *displayListHead = gfx;
}

/* The STATUS, HOW TO PLAY, CODE ENTRY and OPTIONS labels, 128 by 20 each, plain in D_801B5CD0 and lit in D_8012D420 */
#define MAIN_CHOICE_LABEL_WIDTH 128
#define MAIN_CHOICE_LABEL_HEIGHT 20
#define MAIN_CHOICE_LABEL_IMAGE_SIZE (MAIN_CHOICE_LABEL_WIDTH * MAIN_CHOICE_LABEL_HEIGHT / 2)

extern s32 D_8022FDF4_ovl_01[MAIN_CHOICE_COUNT][2];

void func_80205038_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 choiceIndex;

    func_801FB54C_ovl_01(&gfx, 61);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_COUNT; choiceIndex++) {
        if (g_MenuChoices.mainChoice != choiceIndex) {
            gDPLoadTextureBlock_4b(gfx++,
                                   ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
                                    choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE) & ~7,
                                   G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, D_8022FDF4_ovl_01[choiceIndex][0] << 2,
                                    D_8022FDF4_ovl_01[choiceIndex][1] << 2,
                                    (D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH) << 2,
                                    (D_8022FDF4_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT) << 2,
                                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            gDPPipeSync(gfx++);
        }
    }
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    choiceIndex = g_MenuChoices.mainChoice;
    gDPLoadTextureBlock_4b(gfx++,
                           ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
                            choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE) & ~7,
                           G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (D_8022FDF4_ovl_01[choiceIndex][0] - D_800FE33C / 2) << 2,
                            (D_8022FDF4_ovl_01[choiceIndex][1] - D_800FE33C / 2) << 2,
                            (D_8022FDF4_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH + D_800FE33C / 2)
                                << 2,
                            (D_8022FDF4_ovl_01[choiceIndex][1] + (MAIN_CHOICE_LABEL_HEIGHT + D_800FE33C / 2))
                                << 2,
                            G_TX_RENDERTILE, 0, 0,
                            (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
                            (MAIN_CHOICE_LABEL_HEIGHT << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
    gDPPipeSync(gfx++);
    if (D_800FBDB9 == LABEL_GROWING) {
        D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
    }
    if (D_800FBDB9 == LABEL_SHRINKING) {
        D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
    }
    if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
        D_800FBDB9 = LABEL_SHRINKING;
    }
    if (D_800FE33C < 0) {
        D_800FBDB9 = LABEL_GROWING;
    }
    *displayListHead = gfx;
}

/* The menu without Transmit skips the CODE ENTRY label */
extern s32 D_8022FE14_ovl_01[MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT][2];

void func_802058B8_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 choiceIndex;
    s32 labelOffset;

    func_801FB54C_ovl_01(&gfx, 61);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (choiceIndex = 0; choiceIndex != MAIN_CHOICE_WITHOUT_TRANSMIT_COUNT; choiceIndex++) {
        if (g_MenuChoices.mainChoiceWithoutTransmit != choiceIndex) {
            labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
            if (choiceIndex < MAIN_CHOICE_OPTIONS) {
                labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
            }
            gDPLoadTextureBlock_4b(gfx++,
                                   ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET +
                                    labelOffset) & ~7,
                                   G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, D_8022FE14_ovl_01[choiceIndex][0] << 2,
                                    D_8022FE14_ovl_01[choiceIndex][1] << 2,
                                    (D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH) << 2,
                                    (D_8022FE14_ovl_01[choiceIndex][1] + MAIN_CHOICE_LABEL_HEIGHT) << 2,
                                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            gDPPipeSync(gfx++);
        }
    }
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    choiceIndex = g_MenuChoices.mainChoiceWithoutTransmit;
    if (choiceIndex == MAIN_CHOICE_OPTIONS) {
        labelOffset = MAIN_CHOICE_FULL_OPTIONS * MAIN_CHOICE_LABEL_IMAGE_SIZE;
    } else {
        labelOffset = choiceIndex * MAIN_CHOICE_LABEL_IMAGE_SIZE;
    }
    gDPLoadTextureBlock_4b(gfx++,
                           ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
                            labelOffset) & ~7,
                           G_IM_FMT_CI, MAIN_CHOICE_LABEL_WIDTH, MAIN_CHOICE_LABEL_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (D_8022FE14_ovl_01[choiceIndex][0] - D_800FE33C / 2) << 2,
                            (D_8022FE14_ovl_01[choiceIndex][1] - D_800FE33C / 2) << 2,
                            (D_8022FE14_ovl_01[choiceIndex][0] + MAIN_CHOICE_LABEL_WIDTH + D_800FE33C / 2)
                                << 2,
                            (D_8022FE14_ovl_01[choiceIndex][1] + (MAIN_CHOICE_LABEL_HEIGHT + D_800FE33C / 2))
                                << 2,
                            G_TX_RENDERTILE, 0, 0,
                            (MAIN_CHOICE_LABEL_WIDTH << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_WIDTH),
                            (MAIN_CHOICE_LABEL_HEIGHT << 10) / (D_800FE33C + MAIN_CHOICE_LABEL_HEIGHT));
    gDPPipeSync(gfx++);
    if (D_800FBDB9 == LABEL_GROWING) {
        D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
    }
    if (D_800FBDB9 == LABEL_SHRINKING) {
        D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
    }
    if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
        D_800FBDB9 = LABEL_SHRINKING;
    }
    if (D_800FE33C < 0) {
        D_800FBDB9 = LABEL_GROWING;
    }
    *displayListHead = gfx;
}

/* The clubhouse's room labels, 192 by 20 each, plain in D_801B5CD0 and lit in D_8012D420 */
#define ROOM_LABEL_WIDTH 192
#define ROOM_LABEL_HEIGHT 20
#define ROOM_LABEL_IMAGE_SIZE (ROOM_LABEL_WIDTH * ROOM_LABEL_HEIGHT / 2)

/* Places for the clubhouse's rows, three more than it shows */
#define CLUBHOUSE_ROW_PLACE_COUNT 8

extern s32 D_8022FE2C_ovl_01[CLUBHOUSE_ROW_PLACE_COUNT][2];

void func_80206180_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 choiceIndex;
    s32 labelOffset;

    func_801FB54C_ovl_01(&gfx, 61);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_801B5CD0[D_8022BE90_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (choiceIndex = 0; choiceIndex != CLUBHOUSE_CHOICE_COUNT; choiceIndex++) {
        if (choiceIndex != g_MenuChoices.clubhouseChoice) {
            labelOffset = D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE;
            gDPLoadTextureBlock_4b(gfx++,
                                   ((u32)D_801B5CD0[D_8022BE90_ovl_01] + PALETTED_TEXELS_OFFSET + labelOffset) & ~7,
                                   G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
                                   G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                                   G_TX_NOLOD, G_TX_NOLOD);
            gDPPipeSync(gfx++);
            gSPScisTextureRectangle(gfx++, D_8022FE2C_ovl_01[choiceIndex][0] << 2,
                                    D_8022FE2C_ovl_01[choiceIndex][1] << 2,
                                    (D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH) << 2,
                                    (D_8022FE2C_ovl_01[choiceIndex][1] + ROOM_LABEL_HEIGHT) << 2,
                                    G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
            gDPPipeSync(gfx++);
        }
    }
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)D_8012D420[D_8022BE8E_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    choiceIndex = g_MenuChoices.clubhouseChoice;
    gDPLoadTextureBlock_4b(gfx++,
                           ((u32)D_8012D420[D_8022BE8E_ovl_01] + PALETTED_TEXELS_OFFSET +
                            D_8022BF1C_ovl_01[choiceIndex] * ROOM_LABEL_IMAGE_SIZE) & ~7,
                           G_IM_FMT_CI, ROOM_LABEL_WIDTH, ROOM_LABEL_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK,
                           G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, (D_8022FE2C_ovl_01[choiceIndex][0] - D_800FE33C / 2) << 2,
                            (D_8022FE2C_ovl_01[choiceIndex][1] - D_800FE33C / 2) << 2,
                            (D_8022FE2C_ovl_01[choiceIndex][0] + ROOM_LABEL_WIDTH + D_800FE33C / 2)
                                << 2,
                            (D_8022FE2C_ovl_01[choiceIndex][1] + (ROOM_LABEL_HEIGHT + D_800FE33C / 2))
                                << 2,
                            G_TX_RENDERTILE, 0, 0,
                            (ROOM_LABEL_WIDTH << 10) / (D_800FE33C + ROOM_LABEL_WIDTH),
                            (ROOM_LABEL_HEIGHT << 10) / (D_800FE33C + ROOM_LABEL_HEIGHT));
    gDPPipeSync(gfx++);
    if (D_800FBDB9 == LABEL_GROWING) {
        D_800FE33C += CHOSEN_LABEL_GROWTH_STEP;
    }
    if (D_800FBDB9 == LABEL_SHRINKING) {
        D_800FE33C -= CHOSEN_LABEL_GROWTH_STEP;
    }
    if (D_800FE33C > CHOSEN_LABEL_GROWTH_LIMIT) {
        D_800FBDB9 = LABEL_SHRINKING;
    }
    if (D_800FE33C < 0) {
        D_800FBDB9 = LABEL_GROWING;
    }
    *displayListHead = gfx;
}

void func_80206A2C_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    func_801FB54C_ovl_01(&gfx, 61);
    *displayListHead = gfx;
}

const char D_802316EC_ovl_01[] = "Peach's Castle";
const char D_802316FC_ovl_01[] = "Luigi's Garden";

INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_80206A68_ovl_01);

void func_80207D08_ovl_01(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    s32 caddyIndex;

    func_801FB54C_ovl_01(&gfx, 61);
    BeginShadowedText(&gfx);
    SetTextColor(&gfx, 255, 255, 255);
    DrawShadowedText(&gfx, CADDY_SELECT_TITLE_X, CADDY_SELECT_TITLE_Y, D_8023176C_ovl_01);
    for (caddyIndex = 0; caddyIndex != CADDY_COUNT; caddyIndex++) {
        if (g_MenuChoices.unk1 == caddyIndex) {
            SetTextColor(&gfx, 255, 255, 0);
        } else {
            SetTextColor(&gfx, 255, 255, 255);
        }
        DrawShadowedText(&gfx, CADDY_SELECT_NAME_X, caddyIndex * CADDY_SELECT_LINE_HEIGHT + CADDY_SELECT_FIRST_Y,
                         D_8022C0D0_ovl_01[caddyIndex]);
    }
    gDPPipeSync(gfx++);
    *displayListHead = gfx;
}

/* Defined after the function, as the INCLUDE_ASM before it leaves the assembler in .text */
const char D_8023176C_ovl_01[] = "CADDY SELECT";

void func_80207E18_ovl_01(Gfx **displayListHead) {
}

void func_80207E20_ovl_01(void) {
}

void func_80207E28_ovl_01(Gfx **displayListHead, void *image, s32 x, s32 y) {
    Gfx *gfx = *displayListHead;
    s32 unused[8];

    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPPipeSync(gfx++);
    gDPLoadTextureTile_4b(gfx++,
                          ((u32)image + PALETTED_TEXELS_OFFSET + D_8022F8C8_ovl_01 * ANIMATED_ICON_IMAGE_SIZE) & ~7,
                          G_IM_FMT_CI, ANIMATED_ICON_SIZE, ANIMATED_ICON_SIZE, 0, 0, ANIMATED_ICON_SIZE - 1,
                          ANIMATED_ICON_SIZE - 1, 0, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                          G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    gSPScisTextureRectangle(gfx++, x << 2, y << 2, (x + ANIMATED_ICON_SIZE) << 2, (y + ANIMATED_ICON_SIZE) << 2,
                            G_TX_RENDERTILE, 0, 0, TEXEL_STEP_ONE, TEXEL_STEP_ONE);
    gDPPipeSync(gfx++);
    D_8022F8CD_ovl_01++;
    if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01++;
        if (D_8022F8C8_ovl_01 >= ANIMATED_ICON_FRAME_COUNT) {
            D_8022F8C8_ovl_01 = 0;
        }
    }
    *displayListHead = gfx;
}

/*
 * Doesn't match: the two growths swap registers ($t1 and $t8), and the
 * right and bottom edges add their constants last.
 */
#ifdef NON_MATCHING
void func_802081E8_ovl_01(Gfx **displayListHead, s32 left, s32 top, u8 starCount, u8 enlargesChosenHole) {
    Gfx *gfx = *displayListHead;
    EncodedAsset asset;
    void *image = D_801B93E0[D_8022F8CC_ovl_01];
    u8 starIndex = 0;
    s32 growWidth = 0;
    s32 growHeight;

    gDPLoadTLUT_pal16(gfx++, 0, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI, STAR_WIDTH, STAR_HEIGHT, 0,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD,
                           G_TX_NOLOD);
    gDPPipeSync(gfx++);
    if (starCount > MAX_STAR_COUNT) {
        starCount = MAX_STAR_COUNT;
    }
    if (starCount == 0) {
        starCount = 1;
    }
    for (; starIndex < starCount; starIndex++) {
        growWidth = 0;
        if (enlargesChosenHole == TRUE) {
            growHeight = 0;
            if (starIndex == g_MenuChoices.holeIndex) {
                growWidth = CHOSEN_STAR_GROWTH;
                growHeight = CHOSEN_STAR_GROWTH;
            }
        } else {
            growHeight = 0;
        }
        gSPScisTextureRectangle(gfx++, (left + starIndex * STAR_WIDTH - growWidth / 2) << 2,
                                (top - growHeight / 2) << 2,
                                (left + starIndex * STAR_WIDTH + growWidth / 2 + STAR_WIDTH) << 2,
                                (top + growHeight / 2 + STAR_HEIGHT) << 2, G_TX_RENDERTILE, 0, 0,
                                (STAR_WIDTH << 10) / (growWidth + STAR_WIDTH),
                                (STAR_HEIGHT << 10) / (growHeight + STAR_HEIGHT));
        gDPPipeSync(gfx++);
    }
    D_8022F8CD_ovl_01++;
    if (D_8022F8CD_ovl_01 == ANIMATED_ICON_FRAME_DURATION) {
        D_8022F8CD_ovl_01 = 0;
        D_8022F8C8_ovl_01++;
        if (D_8022F8C8_ovl_01 >= STAR_FRAME_COUNT) {
            D_8022F8C8_ovl_01 = 0;
        }
        D_8022F8CC_ovl_01 = D_8022F8CC_ovl_01 == 0;
        OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C8_ovl_01], &asset);
        ReadEncodedAsset(D_801B93E0[D_8022F8CC_ovl_01], &asset);
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_01/main_menu", func_802081E8_ovl_01);
#endif

/*
 * func_8020879C draws the course's Ring Shot holes as stars on a hexagon
 * around a centre, gold where the hole's star was won and silver where not,
 * with the chosen hole larger. Nothing calls it.
 */
#define RING_SHOT_STAR_RADIUS 56
#define RING_SHOT_STAR_WIDTH 32
#define RING_SHOT_STAR_HEIGHT 24
#define RING_SHOT_STAR_GROWTH 16
#define RING_SHOT_STAR_FRAME_DURATION 2
#define GOLD_STAR_PALETTE 0
#define SILVER_STAR_PALETTE 1
/* The silver star's texels follow the gold star's in texture memory */
#define SILVER_STAR_TMEM 128
#define SILVER_STAR_TILE 1

extern s32 D_8022F884_ovl_01[STAR_FRAME_COUNT]; /* The silver star's frames */
extern void *D_800FF180[2];

void func_8020879C_ovl_01(Gfx **displayListHead, s32 centerX, s32 centerY) {
    EncodedAsset asset;
    SaveData *save = GetSaveData();
    Gfx *gfx = *displayListHead;
    f32 starPositions[RING_SHOT_HOLE_COUNT][2] = {
        { 0.0f, -1.0f }, { 0.89f, -0.45f }, { 0.89f, 0.45f }, { 0.0f, 1.0f }, { -0.89f, 0.45f }, { -0.89f, -0.45f },
    };
    void *image;
    s32 tile;
    u8 holeIndex;
    s32 growWidth;
    s32 growHeight;
    s32 left;
    s32 top;

    image = D_8010526C[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal16(gfx++, GOLD_STAR_PALETTE, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadTextureBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7, G_IM_FMT_CI,
                           RING_SHOT_STAR_WIDTH, RING_SHOT_STAR_HEIGHT, GOLD_STAR_PALETTE, G_TX_NOMIRROR | G_TX_WRAP,
                           G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    image = D_800FF180[D_8022F8C4_ovl_01];
    gDPLoadTLUT_pal16(gfx++, SILVER_STAR_PALETTE, ((u32)image + IMAGE_HEADER_SIZE) & ~7);
    gDPLoadMultiBlock_4b(gfx++, ((u32)image + PALETTED_TEXELS_OFFSET) & ~7, SILVER_STAR_TMEM,
                         SILVER_STAR_TILE, G_IM_FMT_CI, RING_SHOT_STAR_WIDTH, RING_SHOT_STAR_HEIGHT,
                         SILVER_STAR_PALETTE, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK,
                         G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
    gDPPipeSync(gfx++);
    for (holeIndex = 0; holeIndex < RING_SHOT_HOLE_COUNT; holeIndex++) {
        growWidth = 0;
        if (holeIndex == g_MenuChoices.holeIndex) {
            growWidth = RING_SHOT_STAR_GROWTH;
            growHeight = RING_SHOT_STAR_GROWTH;
        } else {
            growHeight = 0;
        }
        left = starPositions[holeIndex][0] * RING_SHOT_STAR_RADIUS;
        top = starPositions[holeIndex][1] * RING_SHOT_STAR_RADIUS;
        left += centerX - RING_SHOT_STAR_WIDTH / 2;
        top += centerY - RING_SHOT_STAR_HEIGHT / 2;
        /* A hole without its star draws from the silver star's tile */
        tile = save->hasRingShotStar[g_MenuChoices.courseIndex][holeIndex] == FALSE;
        gSPScisTextureRectangle(gfx++, (left - growWidth / 2) << 2, (top - growHeight / 2) << 2,
                                (left + RING_SHOT_STAR_WIDTH + growWidth / 2) << 2,
                                (top + RING_SHOT_STAR_HEIGHT + growHeight / 2) << 2, tile, 0, 0,
                                (RING_SHOT_STAR_WIDTH << 10) / (growWidth + RING_SHOT_STAR_WIDTH),
                                (RING_SHOT_STAR_HEIGHT << 10) / (growHeight + RING_SHOT_STAR_HEIGHT));
        gDPPipeSync(gfx++);
    }
    D_8022F8C5_ovl_01++;
    if (D_8022F8C5_ovl_01 == RING_SHOT_STAR_FRAME_DURATION) {
        D_8022F8C5_ovl_01 = 0;
        D_8022F8C0_ovl_01++;
        if (D_8022F8C0_ovl_01 >= STAR_FRAME_COUNT) {
            D_8022F8C0_ovl_01 = 0;
        }
        D_8022F8C4_ovl_01 = D_8022F8C4_ovl_01 == 0;
        OpenEncodedAsset(D_8022F848_ovl_01[D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_8010526C[D_8022F8C4_ovl_01], &asset);
        OpenEncodedAsset(D_8022F884_ovl_01[D_8022F8C0_ovl_01], &asset);
        ReadEncodedAsset(D_800FF180[D_8022F8C4_ovl_01], &asset);
    }
    *displayListHead = gfx;
}

void func_80208F8C_ovl_01(Gfx **displayListHead, s32 left, s32 top) {
    Gfx *gfx = *displayListHead;
    s16 stripIndex;
    s32 rowCount;
    s32 unused[8];

    gDPPipeSync(gfx++);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
    gDPPipeSync(gfx++);
    gDPSetRenderMode(gfx++, G_RM_AA_TEX_EDGE, G_RM_AA_TEX_EDGE2);
    gDPPipeSync(gfx++);
    gDPSetTextureFilter(gfx++, G_TF_BILERP);
    gDPLoadTLUT_pal256(gfx++, ((u32)D_800FBE30[D_8022F8CE_ovl_01] + IMAGE_HEADER_SIZE) & ~7);
    for (stripIndex = 0; stripIndex != TOAD_STRIP_COUNT; stripIndex++) {
        rowCount = TOAD_LAST_STRIP_HEIGHT;
        if (stripIndex == 0) {
            rowCount = TOAD_STRIP_HEIGHT;
        }
        gDPLoadTextureBlock(gfx++,
                            ((u32)D_800FBE30[D_8022F8CE_ovl_01] + LARGE_PALETTED_TEXELS_OFFSET +
                             stripIndex * TOAD_WIDTH * TOAD_STRIP_HEIGHT) & ~7,
                            G_IM_FMT_CI, G_IM_SIZ_8b, TOAD_WIDTH, rowCount, 0, G_TX_NOMIRROR | G_TX_WRAP,
                            G_TX_NOMIRROR | G_TX_WRAP, G_TX_NOMASK, G_TX_NOMASK, G_TX_NOLOD, G_TX_NOLOD);
        gDPPipeSync(gfx++);
        gSPScisTextureRectangle(gfx++, left << 2, (top + stripIndex * TOAD_STRIP_HEIGHT) << 2, (left + TOAD_WIDTH) << 2,
                                (top + stripIndex * TOAD_STRIP_HEIGHT + rowCount) << 2, G_TX_RENDERTILE, 0, 0,
                                TEXEL_STEP_ONE, TEXEL_STEP_ONE);
        gDPPipeSync(gfx++);
    }
    *displayListHead = gfx;
}

void DrawBirdieBadgeLabel(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    DrawShadowedText(&gfx, 64, 32, g_BirdieBadgeLabelText);
    *displayListHead = gfx;
}

s32 GetOpenCourseCount(void) {
    s32 coursePoints = GetSaveData()->coursePoints;

    if (IsGameFlagSet(GAME_FLAG_OPEN_SIX_COURSES)) {
        return 6;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_FIVE_COURSES)) {
        return 5;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_FOUR_COURSES)) {
        return 4;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_THREE_COURSES)) {
        return 3;
    }
    if (IsGameFlagSet(GAME_FLAG_OPEN_TWO_COURSES)) {
        return 2;
    }
    return CountOpenCourses(coursePoints);
}
