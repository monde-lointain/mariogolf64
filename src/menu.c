#include "menu.h"

#include <framebuffer_count.h>
#include <nusys.h>
#include <stdlib.h>
#include <string.h>
#include <ultra64.h>

#include "asset.h"
#include "character.h"
#include "club.h"
#include "compiler.h"
#include "course.h"
#include "course_space.h"
#include "course_stats.h"
#include "fault.h"
#include "frame_time.h"
#include "game_flags.h"
#include "game_state.h"
#include "heap.h"
#include "host_file.h"
#include "language.h"
#include "main.h"
#include "main_menu.h"
#include "math_util.h"
#include "menu_memory.h"
#include "password.h"
#include "play_record.h"
#include "player.h"
#include "round_stats.h"
#include "save.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "sram.h"
#include "transfer_pak.h"

#define GAME_FLAG_SAVE_TO_HOST 0x53

#define DMA_ALIGNMENT 0x10

#define ASSET_TROPHY_ICON_IMAGE 0x623
#define ASSET_SPEECH_BUBBLE_IMAGE 0x6F7
#define ASSET_COMPUTER_LEFTY_ICONS_IMAGE 0x709

#define DmaAlignedAddress(address) (((u32)(address) + DMA_ALIGNMENT - 1) & ~(DMA_ALIGNMENT - 1))

#define DONKEY_KONG_STAR_COUNT 30
#define MAPLE_BADGE_COUNT 50

typedef struct ImportedCharacterCell {
    s16 unusedBeforeColumn;
    s16 column;
    s16 unusedBeforeRow;
    s16 row;
} ImportedCharacterCell;

s16 g_LastMenuPlayerCountChoice;
GameState g_BackedUpGameState;
Player g_BackedUpPlayers[PLAYER_COUNT_MAX];
s32 g_ChosenPlayerCount;
Boolean g_IsNintendoClubTournament;
s32 g_ClubTournamentNameIndex;
s32 g_ClubTournamentNumber;
u8 g_TournamentCode[TOURNAMENT_CODE_SIZE];
s16 g_LastMenuScreen;
ImageAsset *g_BrickWallImage;
ImageAsset *g_ModeIconImages[7];
ImageAsset *g_MenuIconImages[7];
ImageAsset *g_MiniGolfDiscImages[2];
ImageAsset *g_MenuBoardImages[2];
ImageAsset *g_NamePlateImage;
ImageAsset *g_MenuLabelImages[2];
ImageAsset *g_MenuTitleImages[2];
ImageAsset *g_CourseTrophyImages[2];
ImageAsset *g_HoleBadgeImages;
ImageAsset *g_SpinningBadgeImage;
ImageAsset *g_MenuWideSpareImage;
PlayRecord g_PlayRecords[PLAYER_COUNT_MAX];
RingShotRecord g_RingShotRecords[PLAYER_COUNT_MAX];
u8 g_UnreadMenuBuffer[0x1218];
UBoolean8 g_IsMenuCharacterLoaded[MAXCONTROLLERS];
ImageAsset *g_AnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT];
BetSet g_BetSet;
ImageAsset *g_CharacterFaceImages[18];
PaletteColor *g_CharacterFacePalettes[18];
s32 g_ChoosingPlayerIndex;
s32 g_ChosenCharacters[PLAYER_COUNT_MAX];
s16 g_ChosenLabelGrowth;
LabelGrowthDirection g_ChosenLabelGrowthDirection;
s8 g_ChosenLefties[4];
ImageAsset *g_ComputerLeftyIcons;
CourseSelectKind g_CourseSelectKind;
CourseStatFigures g_CourseStatFigures[COURSE_COUNT + 1];
GameState g_GameState;
ImageAsset *g_ImageSlots[IMAGE_SLOT_COUNT];
Boolean g_IsChosenHoleLoadStarted;
UBoolean8 g_IsMenuWipeOpening;
UBoolean8 g_IsRingShotWipeOpening;
ImageAsset *g_MenuArrowSizedImages[MENU_ARROW_SIZED_IMAGE_COUNT];
ImageAsset *g_MenuCourseSceneImage;
MenuMemory g_MenuMemory;
MenuScreen g_MenuScreen;
ImageAsset *g_MenuSpareImages[MENU_SPARE_IMAGE_COUNT];
ImageAsset *g_PlayerFaceImages[PLAYER_COUNT_MAX];
PaletteColor *g_PlayerFacePalettes[PLAYER_COUNT_MAX];
Player g_Players[PLAYER_COUNT_MAX];
ImageAsset *g_SecondAnimatedIconImages[ANIMATED_ICON_BUFFER_COUNT];
TakenCharacters g_TakenCharacters;
ImageAsset *g_TrophyFrameBuffers[TROPHY_FRAME_BUFFER_COUNT];
PaletteColor *g_TrophyPalettes[TROPHY_PALETTE_COUNT];

FILE_STATIC SaveData g_SaveData;

#define MARKABLE_CHARACTER_COUNT 10
#define RECORD_CHARACTER_COUNT 13

#define NO_HOST_FILE_NAME g_EmptyString
#define LOWERCASE_LETTER_BIT 0x80
#define MARK_LOWERCASE(letter) ((letter) | LOWERCASE_LETTER_BIT)
#define SAVE_COPY_SRAM_SIZE 0x4000
#define SAVE_COPY_LOCATION(sramOffset) \
    {NO_HOST_FILE_NAME, \
        NO_HOST_FILE_NAME, \
        PI_DOM2_ADDR2 + (sramOffset), \
        PI_DOM2_ADDR2 + (sramOffset) + sizeof(SaveData)}

void PlaceImportedCharacters(void);

typedef struct GridCell {
    s16 column;
    s16 row;
} GridCell;

SaveReadResult ReadSaveCopies(SaveData *save, UBoolean8 isInSram);
Boolean WriteSaveCopies(SaveData *save, UBoolean8 isInSram);

s8 g_LoadedMenuScreen = NO_MENU_SCREEN;
s8 g_PlayerControllerIndices[PLAYER_COUNT_MAX] =
    {UNCHOSEN_CONTROLLER, UNCHOSEN_CONTROLLER, UNCHOSEN_CONTROLLER, UNCHOSEN_CONTROLLER};
s8 g_ChoosingControllerIndices[PLAYER_COUNT_MAX] =
    {UNCHOSEN_CONTROLLER, UNCHOSEN_CONTROLLER, UNCHOSEN_CONTROLLER, UNCHOSEN_CONTROLLER};
u8 g_SoundOptionBits = 0;
s32 g_MenuRepeatButtons = 0;
s32 g_MenuPressedButtons = 0;
static s32 f_UnusedWordAfterPressedButtons UNUSED = 0;
s16 g_ImportedCharacterColumns[PLAYER_COUNT_MAX] =
    {NO_IMPORTED_COLUMN, NO_IMPORTED_COLUMN, NO_IMPORTED_COLUMN, NO_IMPORTED_COLUMN};
static AssetId f_MenuLabelAssets[] =
    {0x0523, 0x0524, 0x0525, 0x0526, 0x0527, 0x0528, 0x0529, 0x052A, 0x052B, 0x052C};
AssetId g_MenuScreenImageAssets[MENU_SCREEN_COUNT] = {
    0x050E,
    0x050F,
    0x0510,
    0x06F9,
    0x0512,
    0x0513,
    0x0514,
    0x0708,
    0x0516,
    0x0517,
    0x0518,
    0x0519,
    0x051A,
    0x06FB,
    0x051C,
    0x06E9,
    0x06FA,
    0x070A,
    0x050E,
    0x050E,
    0x0670,
    0x050E,
    0x066F,
    0x0513,
    0x050F,
    0x070A,
    0x0513,
    0x0940,
    0x0510,
};
AssetId g_CourseIconAssets[] = {0x05F8, 0x05F9, 0x05FA, 0x05FC, 0x05FB, 0x05FD, 0x05FE};
AssetId g_TournamentIconAssets[] = {
    0x05F8,
    0x05F9,
    0x05FA,
    0x05FC,
    0x05FB,
    0x05FD,
    0x094F,
    0x0950,
    0x0951,
    0x0952,
    0x0953,
    0x0954,
};
AssetId g_MiniGolfCourseIconAssets[] = {0x0855, 0x085A, 0x085F, 0x0864, 0x0869, 0x086E};
AssetId g_MiniGolfDiscFrameAssets[MINI_GOLF_COURSES_PER_RECORD * MINI_GOLF_DISC_IMAGE_COUNT]
                                 [MINI_GOLF_TURN_FRAME_COUNT] = {
                                     {0x0853, 0x0854, 0x0855, 0x0856, 0x0857},
                                     {0x0862, 0x0863, 0x0864, 0x0865, 0x0866},
                                     {0x0858, 0x0859, 0x085A, 0x085B, 0x085C},
                                     {0x0867, 0x0868, 0x0869, 0x086A, 0x086B},
                                     {0x085D, 0x085E, 0x085F, 0x0860, 0x0861},
                                     {0x086C, 0x086D, 0x086E, 0x086F, 0x0870},
};
SignedAssetId g_MiniGolfDiscFiles[] = {0x0876, 0x0876, 0x0876, 0x0876, 0x0876, 0x0876};
TrophySize g_TrophySizes[COURSE_COUNT] =
    {{40, 48}, {40, 51}, {40, 51}, {32, 64}, {48, 42}, {36, 51}};
static u8 f_UnusedBytesAfterTrophySizes[12] UNUSED = {0};
AssetId g_PlayerCountIconAssets[][PLAYER_COUNT_ASSET_ROW_LENGTH] = {
    {
        0x0623,
        0x0629,
        0x05B7,
        0x05AF,
        0x0599,
        0x06FC,
        0x05DD,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {
        0x0599,
        0x0631,
        0x065E,
        0x0665,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {
        0x0599,
        0x065E,
        0x0665,
        0x06FC,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {
        0x0599,
        0x065E,
        0x0665,
        0x06FC,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {
        0x0599,
        0x05B7,
        0x05AF,
        0x06FC,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {
        0x0599,
        0x0631,
        0x065E,
        0x0665,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {
        0x0599,
        0x065E,
        0x0665,
        0x06FC,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
    },
    {0x0599,
        0x065E,
        0x0665,
        0x06FC,
        0x06FC,
        0x06FC,
        0x06FC,
        0x0000,
        0x0000,
        0x0000,
        0x0000,
        0x0000},
};
AssetId g_TrophyFrameAssets[][TROPHY_FRAME_LIMIT] = {
    {
        0x088D,
        0x088E,
        0x088F,
        0x0890,
        0x0891,
        0x0892,
        0x0893,
        0x0894,
        0x0895,
        0x0896,
        0x0897,
        0x0898,
        0x0899,
        0x089A,
        0x089B,
        0x089C,
    },
    {
        0x08B2,
        0x08B3,
        0x08B4,
        0x08B5,
        0x08B6,
        0x08B7,
        0x08B8,
        0x08B9,
        0x08BA,
        0x08BB,
        0x08BC,
        0x08BD,
        0x08BE,
        0x08BF,
        0x08C0,
        0x08C1,
    },
    {
        0x087A,
        0x087B,
        0x087C,
        0x087D,
        0x087E,
        0x087F,
        0x0880,
        0x0881,
        0x0882,
        0x0883,
        0x0884,
        0x0885,
        0x0886,
        0x0887,
        0x0888,
        0x0889,
    },
    {
        0x08D8,
        0x08D9,
        0x08DA,
        0x08DB,
        0x08DC,
        0x08DD,
        0x08DE,
        0x08DF,
        0x08E0,
        0x08E1,
        0x08E2,
        0x08E3,
        0x08E4,
        0x08E5,
        0x08E6,
        0x08E7,
    },
    {
        0x08C5,
        0x08C6,
        0x08C7,
        0x08C8,
        0x08C9,
        0x08CA,
        0x08CB,
        0x08CC,
        0x08CD,
        0x08CE,
        0x08CF,
        0x08D0,
        0x08D1,
        0x08D2,
        0x08D3,
        0x08D4,
    },
    {
        0x08AE,
        0x08AD,
        0x08AC,
        0x08AB,
        0x08AA,
        0x08A9,
        0x08A8,
        0x08A7,
        0x08A6,
        0x08A5,
        0x08A4,
        0x08A3,
        0x08A2,
        0x08A1,
        0x08A0,
        TROPHY_FRAME_END,
    },
};
SignedAssetId g_TrophyPaletteFiles[][TROPHY_PALETTE_COUNT] = {
    {0x088C, 0x088B, 0x088A},
    {0x08B1, 0x08B0, 0x08AF},
    {0x0879, 0x0878, 0x0877},
    {0x08D7, 0x08D6, 0x08D5},
    {0x08C4, 0x08C3, 0x08C2},
    {0x089F, 0x089E, 0x089D},
};
s16 g_ModeIconAnimations[MODE_ICON_ANIMATION_COUNT][MODE_ICON_FRAME_COUNT] = {
    {
        0x0623,
        0x0624,
        0x0625,
        0x0626,
        0x0627,
        0x0628,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x0629,
        0x062A,
        0x062B,
        0x062C,
        0x062D,
        0x062E,
        0x062F,
        0x0630,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x0598,
        0x0599,
        0x059A,
        0x059B,
        0x059C,
        0x059D,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x05AF,
        0x05B0,
        0x05B1,
        0x05B2,
        0x05B3,
        0x05B4,
        0x05B5,
        0x05B6,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x05DD,
        0x05DE,
        0x05DF,
        0x05E0,
        0x05E1,
        0x05E2,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x06FC,
        0x06FD,
        0x06FE,
        0x06FF,
        0x0700,
        0x0701,
        0x0700,
        0x06FF,
        0x06FE,
        0x06FD,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x0631,
        0x0632,
        0x0633,
        0x0634,
        0x0635,
        0x0636,
        0x0637,
        0x0638,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x065E,
        0x065F,
        0x0660,
        0x065F,
        0x065E,
        0x0661,
        0x0662,
        0x0661,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x05B7,
        0x05B8,
        0x05B9,
        0x05BA,
        0x05BB,
        0x05BC,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x05A2,
        0x05A1,
        0x05A0,
        0x059F,
        0x05A0,
        0x05A1,
        0x05A2,
        0x05A1,
        0x05A0,
        0x059F,
        0x05A0,
        0x05A1,
        0x05A2,
        0x05F6,
        0x05F5,
        0x05F4,
        0x05A3,
        0x05F4,
        0x05F5,
        0x05F6,
        0x05F5,
        0x05F4,
        0x05A3,
        0x05F4,
        0x05F5,
        0x05F6,
        END_OF_ICON_FRAMES,
    },
    {
        0x0665,
        0x0666,
        0x0667,
        0x0668,
        0x0669,
        0x066A,
        0x066B,
        0x066C,
        0x066D,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x0598,
        0x0599,
        0x059A,
        0x059B,
        0x059C,
        0x059D,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x0598,
        0x0599,
        0x059A,
        0x059B,
        0x059C,
        0x059D,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
    {
        0x06FC,
        0x06FD,
        0x06FE,
        0x06FF,
        0x0700,
        0x0701,
        0x0700,
        0x06FF,
        0x06FE,
        0x06FD,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
        END_OF_ICON_FRAMES,
    },
};
SignedAssetId g_CharacterFacePaletteFiles[][CHARACTER_PALETTE_COUNT] = {
    {0x07B8, 0x07B9, 0x07BA, 0x07BB},
    {0x0748, 0x0749, 0x074A, 0x074B},
    {0x07AA, 0x07AB, 0x07AC, 0x07AD},
    {0x073A, 0x073B, 0x073C, 0x073D},
    {0x0780, 0x0781, 0x0782, 0x0783},
    {0x07E2, 0x07E3, 0x07E4, 0x07E5},
    {0x07C6, 0x07C7, 0x07C8, 0x07C9},
    {0x07D4, 0x07D5, 0x07D6, 0x07D7},
    {0x0764, 0x0765, 0x0766, 0x0767},
    {0x078E, 0x078F, 0x0790, 0x0791},
    {0x079C, 0x079D, 0x079E, 0x079F},
    {0x0756, 0x0757, 0x0758, 0x0759},
    {0x0772, 0x0773, 0x0774, 0x0775},
    {0x0980, 0x0981, 0x0982, 0x0983},
    {0x07F0, 0x07F1, 0x07F2, 0x07F3},
    {0x07FE, 0x07FF, 0x0800, 0x0801},
    {0x080C, 0x080D, 0x080E, 0x080F},
    {0x081A, 0x081B, 0x081C, 0x081D},
    {0x078E, 0x078E, 0x078E, 0x078E},
    {0x078E, 0x078E, 0x078E, 0x078E},
    {0x078E, 0x078E, 0x078E, 0x078E},
    {0x078E, 0x078E, 0x078E, 0x078E},
    {0x078E, 0x078E, 0x078E, 0x078E},
    {0x078E, 0x078E, 0x078E, 0x078E},
};
AssetId g_FaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT] = {
    {0x07BC, 0x07BD, 0x07BE, 0x07BF, 0x07C0, 0x07BF, 0x07BE, 0x07BD},
    {0x074C, 0x074D, 0x074E, 0x074F, 0x0750, 0x074F, 0x074E, 0x074D},
    {0x07AE, 0x07AF, 0x07B0, 0x07B1, 0x07B2, 0x07B1, 0x07B0, 0x07AF},
    {0x073E, 0x073F, 0x0740, 0x0741, 0x0742, 0x0741, 0x0740, 0x073F},
    {0x0784, 0x0785, 0x0786, 0x0787, 0x0788, 0x0787, 0x0786, 0x0785},
    {0x07E6, 0x07E7, 0x07E8, 0x07E9, 0x07EA, 0x07E9, 0x07E8, 0x07E7},
    {0x07CA, 0x07CB, 0x07CC, 0x07CD, 0x07CE, 0x07CD, 0x07CC, 0x07CB},
    {0x07D8, 0x07D9, 0x07DA, 0x07DB, 0x07DC, 0x07DB, 0x07DA, 0x07D9},
    {0x0768, 0x0769, 0x076A, 0x076B, 0x076C, 0x076B, 0x076A, 0x0769},
    {0x0792, 0x0793, 0x0794, 0x0795, 0x0796, 0x0795, 0x0794, 0x0793},
    {0x07A0, 0x07A1, 0x07A2, 0x07A3, 0x07A4, 0x07A3, 0x07A2, 0x07A1},
    {0x075A, 0x075B, 0x075C, 0x075D, 0x075E, 0x075D, 0x075C, 0x075B},
    {0x0776, 0x0777, 0x0778, 0x0779, 0x077A, 0x0779, 0x0778, 0x0777},
    {0x0984, 0x0985, 0x0986, 0x0987, 0x0988, 0x0987, 0x0986, 0x0985},
    {0x07F4, 0x07F5, 0x07F6, 0x07F7, 0x07F8, 0x07F7, 0x07F6, 0x07F5},
    {0x0802, 0x0803, 0x0804, 0x0805, 0x0806, 0x0805, 0x0804, 0x0803},
    {0x0810, 0x0811, 0x0812, 0x0813, 0x0814, 0x0813, 0x0812, 0x0811},
    {0x081E, 0x081F, 0x0820, 0x0821, 0x0822, 0x0821, 0x0820, 0x081F},
    {0x0792, 0x0793, 0x0794, 0x0795, 0x0796, 0x0795, 0x0794, 0x0793},
};
AssetId g_AlternateFaceAnimationAssets[][FACE_ANIMATION_FRAME_COUNT] = {
    {0x07BC, 0x07C1, 0x07C2, 0x07C3, 0x07C4, 0x07C3, 0x07C2, 0x07C1},
    {0x074C, 0x0751, 0x0752, 0x0753, 0x0754, 0x0753, 0x0752, 0x0751},
    {0x07AE, 0x07B3, 0x07B4, 0x07B5, 0x07B6, 0x07B5, 0x07B4, 0x07B3},
    {0x073E, 0x0743, 0x0744, 0x0745, 0x0746, 0x0745, 0x0744, 0x0743},
    {0x0784, 0x0789, 0x078A, 0x078B, 0x078C, 0x078B, 0x078A, 0x0789},
    {0x07E6, 0x07EB, 0x07EC, 0x07ED, 0x07EE, 0x07ED, 0x07EC, 0x07EB},
    {0x07CA, 0x07CF, 0x07D0, 0x07D1, 0x07D2, 0x07D1, 0x07D0, 0x07CF},
    {0x07D8, 0x07DD, 0x07DE, 0x07DF, 0x07E0, 0x07DF, 0x07DE, 0x07DD},
    {0x0768, 0x076D, 0x076E, 0x076F, 0x0770, 0x076F, 0x076E, 0x076D},
    {0x0792, 0x0797, 0x0798, 0x0799, 0x079A, 0x0799, 0x0798, 0x0797},
    {0x07A0, 0x07A5, 0x07A6, 0x07A7, 0x07A8, 0x07A7, 0x07A6, 0x07A5},
    {0x075A, 0x075F, 0x0760, 0x0761, 0x0762, 0x0761, 0x0760, 0x075F},
    {0x0776, 0x077B, 0x077C, 0x077D, 0x077E, 0x077D, 0x077C, 0x077B},
    {0x0984, 0x0989, 0x098A, 0x098B, 0x098C, 0x098B, 0x098A, 0x0989},
    {0x07F4, 0x07F9, 0x07FA, 0x07FB, 0x07FC, 0x07FB, 0x07FA, 0x07F9},
    {0x0802, 0x0807, 0x0808, 0x0809, 0x080A, 0x0809, 0x0808, 0x0807},
    {0x0810, 0x0815, 0x0816, 0x0817, 0x0818, 0x0817, 0x0816, 0x0815},
    {0x081E, 0x0823, 0x0824, 0x0825, 0x0826, 0x0825, 0x0824, 0x0823},
    {0x0792, 0x0797, 0x0798, 0x0799, 0x079A, 0x0799, 0x0798, 0x0797},
};
static AssetId f_ToadImageAssets[] = {
    0x0710,
    0x0711,
    0x0712,
    0x0713,
    0x0714,
    0x0715,
    0x0716,
    0x0717,
    0x0718,
    0x0719,
    0x071A,
    0x071B,
    0x071C,
};
static WideAssetId f_UnusedToadAssetIds[] UNUSED =
    {0x00000710, 0x00000711, 0x00000712, 0x00000713, 0x00000714, 0x00000715};
s16 g_CharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT] = {
    {
        CHARACTER_CHOICE_PLUM,
        CHARACTER_CHOICE_PEACH,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
    {
        CHARACTER_CHOICE_CHARLIE,
        CHARACTER_CHOICE_BABY_MARIO,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
    {
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
};
static s16 f_EveryCharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT] = {
    {
        CHARACTER_CHOICE_PLUM,
        CHARACTER_CHOICE_PEACH,
        CHARACTER_CHOICE_LUIGI,
        CHARACTER_CHOICE_SONNY,
        CHARACTER_CHOICE_HARRY,
        CHARACTER_CHOICE_MAPLE,
        CHARACTER_CHOICE_BOWSER,
    },
    {
        CHARACTER_CHOICE_CHARLIE,
        CHARACTER_CHOICE_BABY_MARIO,
        CHARACTER_CHOICE_YOSHI,
        CHARACTER_CHOICE_WARIO,
        CHARACTER_CHOICE_MARIO,
        CHARACTER_CHOICE_DONKEY_KONG,
        CHARACTER_CHOICE_METAL_MARIO,
    },
    {
        CHARACTER_CHOICE_KID,
        CHARACTER_CHOICE_SHERRY,
        CHARACTER_CHOICE_AZALEA,
        CHARACTER_CHOICE_JOE,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
};
static s16 f_RecordGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT] = {
    {
        CHARACTER_PLUM,
        CHARACTER_PEACH,
        CHARACTER_LUIGI,
        CHARACTER_SONNY,
        CHARACTER_HARRY,
        CHARACTER_MAPLE,
        CHARACTER_BOWSER,
    },
    {
        CHARACTER_CHARLIE,
        CHARACTER_BABY_MARIO,
        CHARACTER_YOSHI,
        CHARACTER_WARIO,
        CHARACTER_MARIO,
        CHARACTER_DONKEY_KONG,
        CHARACTER_METAL_MARIO,
    },
    {
        CHARACTER_KID,
        CHARACTER_SHERRY,
        CHARACTER_AZALEA,
        CHARACTER_JOE,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
};
static s16 f_CharacterChoices[CHARACTER_COUNT] = {
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
#define MULTIPLAYER_GRID_COLUMN_COUNT 5
static s16 f_StartingCharacterGrid[CHARACTER_GRID_ROW_COUNT][CHARACTER_GRID_COLUMN_COUNT] = {
    {
        CHARACTER_CHOICE_PLUM,
        CHARACTER_CHOICE_PEACH,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
    {
        CHARACTER_CHOICE_CHARLIE,
        CHARACTER_CHOICE_BABY_MARIO,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
    {
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
        EMPTY_GRID_SLOT,
    },
};
static GridCell f_SlotCells[] = {
    {0, 0},
    {0, 1},
    {1, 0},
    {1, 1},
    {2, 0},
    {2, 1},
    {3, 0},
    {3, 1},
    {4, 0},
    {4, 1},
    {5, 0},
    {5, 1},
    {6, 0},
    {6, 1},
    {0, 2},
    {1, 2},
    {2, 2},
    {3, 2},
};
char g_CharacterNames[][IMPORTED_CHARACTER_NAME_LENGTH] = {
    "Plum",
    "Charlie",
    "Peach",
    "Baby Mario",
    "Luigi",
    "Yoshi",
    "Sonny",
    "Wario",
    "Harry",
    "Mario",
    "Maple",
    "DK",
    "Bowser",
    "Metal Mario",
};
static s8 f_ImportedCharacterNames[IMPORTED_CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH] =
    {"Kid", "Sherry", "Azalea", "Joe"};
s32 g_CourseNameEnvColors[] = {
    0x18A0E900,
    0x0DFF1E00,
    0xFFFFFF00,
    0xFFFFFF00,
    0x0DFF1E00,
    0x0DFF1E00,
    0xF5FF0100,
    0xF5FF0100,
    0xF5FF0100,
    0xF5FF0100,
};
s32 g_CourseNamePrimColors[] = {
    0x0DFF1E00,
    0xE94CFB00,
    0xF5FF0100,
    0x18A0E900,
    0xFF960000,
    0xEF470300,
    0x0DFF1E00,
    0x18A0E900,
    0xEF470300,
    0xEF470300,
};
static s32 f_CoursePointsNeeded[COURSE_COUNT] = {0, 50, 300, 1000, 2300, 4500};
static s16 f_DebugImportedCharacters[IMPORTED_CHARACTER_COUNT] =
    {CHARACTER_KID, CHARACTER_SHERRY, CHARACTER_AZALEA, CHARACTER_JOE};
static s16 f_DebugImportedDistances[IMPORTED_CHARACTER_COUNT] = {500, 400, 300, 200};
static s16 f_DebugImportedSpins[IMPORTED_CHARACTER_COUNT] = {40, 30, 20, 10};
static s16 f_DebugImportedSideSpins[IMPORTED_CHARACTER_COUNT] = {0, 50, -50, 10};
static char f_DebugImportedNames[IMPORTED_CHARACTER_COUNT][IMPORTED_CHARACTER_NAME_LENGTH] = {
    {'K', MARK_LOWERCASE('i'), MARK_LOWERCASE('d')},
    {
        'S',
        MARK_LOWERCASE('h'),
        MARK_LOWERCASE('e'),
        MARK_LOWERCASE('r'),
        MARK_LOWERCASE('r'),
        MARK_LOWERCASE('y'),
    },
    {
        'A',
        MARK_LOWERCASE('z'),
        MARK_LOWERCASE('a'),
        MARK_LOWERCASE('l'),
        MARK_LOWERCASE('e'),
        MARK_LOWERCASE('a'),
    },
    {'J', MARK_LOWERCASE('o'), MARK_LOWERCASE('e')},
};
static ImportedCharacterCell f_ImportedCharacterCells[IMPORTED_CHARACTER_COUNT] =
    {{0, 0, 0, 2}, {0, 1, 0, 2}, {0, 2, 0, 2}, {0, 3, 0, 2}};
static s16 f_CharacterSlots[] = {0, 1, 10, 3, 4, 2, 6, 7, 8, 9, 5, 11, 12, 13, 0};
s32 g_MenuCourseCount = COURSE_COUNT;
static s8 f_UnreadMenuEntryByte = 0;
UBoolean8 g_AreBetPagesBlocked = FALSE;
static s32 f_UnusedWordsAfterBetPagesBlocked[2] UNUSED = {0};
static SaveCopyLocation f_SaveCopyLocations[SAVE_COPY_COUNT] = {
    SAVE_COPY_LOCATION(0),
    SAVE_COPY_LOCATION(SAVE_COPY_SRAM_SIZE),
};
static s32 f_UnusedWordsAfterSaveCopyLocations[4] UNUSED = {0};

#define MENU_LABEL_BUFFER_SIZE 0x70A8
#define TROPHY_FRAME_BUFFER_SIZE 0xFA0
#define COURSE_TROPHY_BUFFER_SIZE 0x125C

void LoadMenuResources(void) {
    EncodedAsset asset;
    AssetFile file;
    s32 unused[4] UNUSED;
    SaveData *save;
    s32 index;
    s32 size;

    save = GetSaveData();
    if (g_LoadedMenuScreen != -1) {
        osSyncPrintf("menu const error");
        return;
    }

    g_MenuCourseSceneImage = AllocMemory(OpenEncodedAsset(ASSET_COURSE_SCENE, &asset));
    ReadEncodedAsset(g_MenuCourseSceneImage, &asset);
    g_BrickWallImage = AllocMemory(OpenEncodedAsset(ASSET_BRICK_WALL, &asset));
    ReadEncodedAsset(g_BrickWallImage, &asset);
    g_MenuBoardImages[0] = AllocMemory(OpenEncodedAsset(ASSET_WIDE_BOARD, &asset));
    ReadEncodedAsset(g_MenuBoardImages[0], &asset);
    g_MenuBoardImages[1] = AllocMemory(OpenEncodedAsset(ASSET_WIDE_BOARD, &asset));
    ReadEncodedAsset(g_MenuBoardImages[1], &asset);
    g_NamePlateImage = AllocMemory(OpenEncodedAsset(ASSET_NAME_PLATE, &asset));
    ReadEncodedAsset(g_NamePlateImage, &asset);
    g_MenuLabelImages[0] = AllocMemory(MENU_LABEL_BUFFER_SIZE);
    g_ImageSlots[0] = AllocMemory(MENU_LABEL_BUFFER_SIZE);
    g_MenuLabelImages[1] = AllocMemory(MENU_LABEL_BUFFER_SIZE);
    g_ImageSlots[1] = AllocMemory(MENU_LABEL_BUFFER_SIZE);
    OpenEncodedAsset(*f_MenuLabelAssets, &asset);
    ReadEncodedAsset(g_MenuLabelImages[0], &asset);
    OpenEncodedAsset(*f_MenuLabelAssets, &asset);
    ReadEncodedAsset(g_MenuLabelImages[1], &asset);
    if (g_MenuScreen == MENU_SCREEN_MODE_SELECT) {
        OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
        ReadEncodedAsset(g_ImageSlots[0], &asset);
        OpenEncodedAsset(ASSET_MENU_GAME_MODES, &asset);
        ReadEncodedAsset(g_ImageSlots[1], &asset);
    }
    else {
        OpenEncodedAsset(f_MenuLabelAssets[1], &asset);
        ReadEncodedAsset(g_ImageSlots[0], &asset);
        OpenEncodedAsset(f_MenuLabelAssets[1], &asset);
        ReadEncodedAsset(g_ImageSlots[1], &asset);
    }

    g_MenuTitleImages[0] = AllocMemory(OpenEncodedAsset(*g_MenuScreenImageAssets, &asset));
    ReadEncodedAsset(g_MenuTitleImages[0], &asset);
    g_MenuTitleImages[1] = AllocMemory(OpenEncodedAsset(*g_MenuScreenImageAssets, &asset));
    ReadEncodedAsset(g_MenuTitleImages[1], &asset);
    for (index = 0; index != ARRAY_COUNT(g_MenuIconImages); index++) {
        if (g_MenuScreen == MENU_SCREEN_MODE_SELECT) {
            size =
                OpenEncodedAsset(g_PlayerCountIconAssets[g_ChosenPlayerCount - 1][index], &asset);
        }
        else {
            size = OpenEncodedAsset(g_CourseIconAssets[index], &asset);
        }

        g_MenuIconImages[index] = AllocMemory(size);
        ReadEncodedAsset(g_MenuIconImages[index], &asset);
        if (g_MenuScreen == MENU_SCREEN_MODE_SELECT) {
            size =
                OpenEncodedAsset(g_PlayerCountIconAssets[g_ChosenPlayerCount - 1][index], &asset);
        }
        else {
            size = OpenEncodedAsset(g_CourseIconAssets[index], &asset);
        }

        g_ModeIconImages[index] = AllocMemory(size);
        ReadEncodedAsset(g_ModeIconImages[index], &asset);
    }

    for (index = 0; index != ARRAY_COUNT(g_MiniGolfDiscImages); index++) {
        OpenAssetFile(g_MiniGolfDiscFiles[index], &file);
        size = GetAssetFileSize(&file);
        g_MiniGolfDiscImages[index] = AllocMemory(size);
        ReadAssetFile(g_MiniGolfDiscImages[index], size, &file);
    }

    OpenEncodedAsset(g_TrophyFrameAssets[0][0], &asset);
    g_TrophyFrameBuffers[0] = AllocMemory(TROPHY_FRAME_BUFFER_SIZE);
    ReadEncodedAsset(g_TrophyFrameBuffers[0], &asset);
    OpenEncodedAsset(g_TrophyFrameAssets[0][0], &asset);
    g_TrophyFrameBuffers[1] = AllocMemory(TROPHY_FRAME_BUFFER_SIZE);
    ReadEncodedAsset(g_TrophyFrameBuffers[1], &asset);
    for (index = 0; index != ARRAY_COUNT(g_CourseTrophyImages); index++) {
        OpenEncodedAsset(g_TrophyFrameAssets[0][0], &asset);
        g_CourseTrophyImages[index] = AllocMemory(COURSE_TROPHY_BUFFER_SIZE);
        ReadEncodedAsset(g_CourseTrophyImages[index], &asset);
    }

    for (index = 0; index != ARRAY_COUNT(g_MenuArrowSizedImages); index++) {
        g_MenuArrowSizedImages[index] = AllocMemory(OpenEncodedAsset(ASSET_GREEN_ARROWS, &asset));
        ReadEncodedAsset(g_MenuArrowSizedImages[index], &asset);
    }

    size = OpenEncodedAsset(ASSET_TROPHY_ICON_IMAGE, &asset);
    g_AnimatedIconImages[0] = AllocMemory(size);
    g_AnimatedIconImages[1] = AllocMemory(size);
    g_SecondAnimatedIconImages[0] = AllocMemory(size);
    g_SecondAnimatedIconImages[1] = AllocMemory(size);
    ReadEncodedAsset(g_AnimatedIconImages[0], &asset);
    g_HoleBadgeImages = AllocMemory(OpenEncodedAsset(ASSET_HOLES_BADGES, &asset));
    ReadEncodedAsset(g_HoleBadgeImages, &asset);
    g_SpinningBadgeImage = AllocMemory(OpenEncodedAsset(ASSET_SPINNING_BADGE, &asset));
    ReadEncodedAsset(g_SpinningBadgeImage, &asset);
    for (index = 0; index != ARRAY_COUNT(g_CharacterFaceImages); index++) {
        g_CharacterFaceImages[index] =
            AllocMemory(OpenEncodedAsset(g_FaceAnimationAssets[index][0], &asset));
        ReadEncodedAsset(g_CharacterFaceImages[index], &asset);
    }

    for (index = 0; index != ARRAY_COUNT(g_CharacterFacePalettes); index++) {
        OpenAssetFile(g_CharacterFacePaletteFiles[index][0], &file);
        size = GetAssetFileSize(&file);
        g_CharacterFacePalettes[index] = AllocMemory(size);
        ReadAssetFile(g_CharacterFacePalettes[index], size, &file);
    }

    for (index = 0; index != ARRAY_COUNT(g_TrophyPalettes); index++) {
        OpenAssetFile(g_TrophyPaletteFiles[0][index], &file);
        size = GetAssetFileSize(&file);
        g_TrophyPalettes[index] = AllocMemory(size);
        ReadAssetFile(g_TrophyPalettes[index], size, &file);
    }

    g_MenuSpareImages[0] = AllocMemory(OpenEncodedAsset(*f_ToadImageAssets, &asset) * 2);
    ReadEncodedAsset(g_MenuSpareImages[0], &asset);
    g_MenuSpareImages[1] = AllocMemory(OpenEncodedAsset(*f_ToadImageAssets, &asset) * 2);
    ReadEncodedAsset(g_MenuSpareImages[1], &asset);
    g_MenuWideSpareImage = AllocMemory(OpenEncodedAsset(ASSET_SPEECH_BUBBLE_IMAGE, &asset) * 2);
    ReadEncodedAsset(g_MenuWideSpareImage, &asset);
    g_ComputerLeftyIcons = AllocMemory(OpenEncodedAsset(ASSET_COMPUTER_LEFTY_ICONS_IMAGE, &asset));
    ReadEncodedAsset(g_ComputerLeftyIcons, &asset);
    PlaySong(SONG_TITLE, 0);
    SetFramebufferCount(TRIPLE_BUFFERING);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
    g_MenuMemory.statsCourse = COURSE_COUNT;
    InitSram();
    TransferSaveData(save, FALSE);
}

void FreeMenuResources(void) {
    s32 index;

    g_LoadedMenuScreen = NO_MENU_SCREEN;
    FreeMemory(&g_MenuCourseSceneImage);
    FreeMemory(&g_BrickWallImage);
    for (index = 0; index != ARRAY_COUNT(g_MenuIconImages); index++) {
        FreeMemory(&g_MenuIconImages[index]);
        FreeMemory(&g_ModeIconImages[index]);
    }

    FreeMemory(&g_MiniGolfDiscImages[0]);
    FreeMemory(&g_MiniGolfDiscImages[1]);
    FreeMemory(&g_MenuBoardImages[0]);
    FreeMemory(&g_MenuBoardImages[1]);
    FreeMemory(&g_NamePlateImage);
    FreeMemory(&g_MenuLabelImages[0]);
    FreeMemory(&g_ImageSlots[0]);
    FreeMemory(&g_MenuLabelImages[1]);
    FreeMemory(&g_ImageSlots[1]);
    FreeMemory(&g_MenuTitleImages[0]);
    FreeMemory(&g_MenuTitleImages[1]);
    FreeMemory(&g_TrophyFrameBuffers[0]);
    FreeMemory(&g_TrophyFrameBuffers[1]);
    for (index = 0; index != ARRAY_COUNT(g_CourseTrophyImages); index++) {
        FreeMemory(&g_CourseTrophyImages[index]);
    }

    for (index = 0; index != ARRAY_COUNT(g_TrophyPalettes); index++) {
        FreeMemory(&g_TrophyPalettes[index]);
    }

    for (index = 0; index != ARRAY_COUNT(g_MenuArrowSizedImages); index++) {
        FreeMemory(&g_MenuArrowSizedImages[index]);
    }

    FreeMemory(&g_AnimatedIconImages[0]);
    FreeMemory(&g_AnimatedIconImages[1]);
    FreeMemory(&g_SecondAnimatedIconImages[0]);
    FreeMemory(&g_SecondAnimatedIconImages[1]);
    FreeMemory(&g_HoleBadgeImages);
    FreeMemory(&g_SpinningBadgeImage);
    for (index = 0; index != ARRAY_COUNT(g_CharacterFaceImages); index++) {
        FreeMemory(&g_CharacterFaceImages[index]);
    }

    for (index = 0; index != ARRAY_COUNT(g_CharacterFacePalettes); index++) {
        FreeMemory(&g_CharacterFacePalettes[index]);
    }

    FreeMemory(&g_MenuSpareImages[0]);
    FreeMemory(&g_MenuSpareImages[1]);
    FreeMemory(&g_MenuWideSpareImage);
    FreeMemory(&g_ComputerLeftyIcons);
}

SaveData *GetSaveData(void) {
    GetMenuMemory();
    return &g_SaveData;
}

MenuMemory *GetMenuMemory(void) {
    return &g_MenuMemory;
}

void ResetSaveData(void) {
    SaveData *save;
    s8 row;
    s8 column;

    save = GetSaveData();
    memset(save, 0, sizeof(SaveData));
    save->magic = SAVE_MAGIC;
    for (row = 0; row != ARRAY_COUNT(save->records.bestTimes); row++) {
        for (column = 0; column != ARRAY_COUNT(save->records.bestTimes[0]); column++) {
            save->records.bestTimes[row][column] = -1;
        }
    }

    TransferSaveData(save, TRUE);
}

void InitSaveData(void) {
    SaveData *save;

    save = GetSaveData();
    InitSram();
    TransferSaveData(save, FALSE);
}

s32 CountOpenCourses(s32 coursePoints) {
    CourseIndex courseIndex;

    for (courseIndex = COURSE_COUNT - 1; courseIndex != 0; courseIndex--) {
        if (coursePoints >= f_CoursePointsNeeded[courseIndex]) {
            break;
        }
    }

    return courseIndex + 1;
}

s32 GetCoursePointsNeeded(CourseIndex courseIndex) {
    return f_CoursePointsNeeded[courseIndex];
}

#define BIRDIE_PRIORITY 50
#define BONUS_BIRDIE_PRIORITY 60
#define EAGLE_PRIORITY 60
#define BONUS_EAGLE_PRIORITY 70
#define SHORT_HOLE_EAGLE_BONUS 10
#define HOLE_IN_ONE_PRIORITY 90
#define ALBATROSS_PRIORITY 100

s32 GetRecordPriority(PlayRecord *record) {
    s32 priority;
    s32 flaggedScoreKind;
    s32 par;
    ScoreKind kind;
    s32 priorityMax;

    flaggedScoreKind = record->scoreKind;
    kind = flaggedScoreKind & SCORE_KIND_MASK;
    priority = 0;
    par = record->hole.par;
    priorityMax = ALBATROSS_PRIORITY;
    if (flaggedScoreKind & SCORE_KIND_RECORDED) {
        switch (kind) {
            case SCORE_KIND_BIRDIE:
                priority = BIRDIE_PRIORITY;
                if (flaggedScoreKind & SCORE_KIND_UNUSED_BONUS) {
                    priority = BONUS_BIRDIE_PRIORITY;
                }

                break;
            case SCORE_KIND_EAGLE:
                priority = EAGLE_PRIORITY;
                if (flaggedScoreKind & SCORE_KIND_UNUSED_BONUS) {
                    priority = BONUS_EAGLE_PRIORITY;
                }

                if (par < PAR_FIVE) {
                    priority += SHORT_HOLE_EAGLE_BONUS;
                }

                break;
            case SCORE_KIND_HOLE_IN_ONE:
                priority = HOLE_IN_ONE_PRIORITY;
                if (par < PAR_FOUR) {
                    break;
                }

            case SCORE_KIND_ALBATROSS:
                priority = priorityMax;
                break;
        }
    }

    return priority;
}

void SortIndicesDescending(s32 *values, s32 *order, s32 count) {
    s32 sortedValues[count];
    s32 index;
    s32 searchIndex;
    s32 shiftIndex;
    s32 insertedValue;
    s32 valueIndex;

    for (index = 0; index < count; index++) {
        order[index] = index;
    }

    for (index = 0; index < count; index++) {
        sortedValues[index] = values[index];
    }

    for (index = 0; index < count; index++) {
        insertedValue = sortedValues[index];
        valueIndex = order[index];
        for (searchIndex = index - 1; searchIndex >= 0; searchIndex--) {
            if (sortedValues[searchIndex] >= insertedValue) {
                break;
            }
        }

        if (searchIndex < index - 1) {
            for (shiftIndex = index; searchIndex + 1 < shiftIndex; shiftIndex--) {
                sortedValues[shiftIndex] = sortedValues[shiftIndex - 1];
                order[shiftIndex] = order[shiftIndex - 1];
            }

            sortedValues[shiftIndex] = insertedValue;
            order[shiftIndex] = valueIndex;
        }
    }
}

void SaveRingShotRecord(s32 recordIndex, RingShotRecord *record) {
    g_SaveData.ringShotRecords[recordIndex] = *record;
    TransferSaveData(GetSaveData(), TRUE);
}

#define RANKED_GROUP_LENGTH 5

void InsertRankedRecord(PlayRecord *record) {
    s32 priorities[RANKED_RECORD_COUNT];
    s32 order[RANKED_RECORD_COUNT];
    PlayRecord copies[RANKED_RECORD_COUNT];
    s32 newPriority;
    PlayRecord *records;
    s32 firstIndex;
    s32 lowestPriority;
    s32 lowestIndex;
    s32 index;
    s32 groupLength;

    osSyncPrintf("Checking SuperShot");
    if ((s8)record->scoreKind & SCORE_KIND_RECORDED) {
        records = g_SaveData.rankedRecords;
        newPriority = GetRecordPriority(record);
        lowestPriority = 0x40000000;
        lowestIndex = 0;

        switch (record->scoreKind & SCORE_KIND_MASK) {
            case SCORE_KIND_BIRDIE:
                firstIndex = 0 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
            case SCORE_KIND_EAGLE:
                firstIndex = 1 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
            case SCORE_KIND_HOLE_IN_ONE:
                firstIndex = 2 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
            case SCORE_KIND_ALBATROSS:
            default:
                firstIndex = 3 * RANKED_GROUP_LENGTH;
                groupLength = RANKED_GROUP_LENGTH;
                break;
        }

        osSyncPrintf("1:prior=%d:", newPriority);
        for (index = 0; index < groupLength; index++) {
            priorities[index] = GetRecordPriority(&records[firstIndex + index]);
        }

        for (index = 0; index < groupLength; index++) {
            if (lowestPriority >= priorities[index]) {
                lowestIndex = index;
                lowestPriority = priorities[index];
            }
        }

        if (newPriority >= lowestPriority) {
            osSyncPrintf("2");
            priorities[lowestIndex] = 0;
            SortIndicesDescending(priorities, order, groupLength);
            for (index = 0; index < groupLength; index++) {
                copies[index] = records[firstIndex + index];
            }

            for (index = 0; index < groupLength - 1; index++) {
                records[firstIndex + index + 1] = copies[order[index]];
                priorities[index + 1] = GetRecordPriority(&records[firstIndex + index + 1]);
            }

            priorities[0] = newPriority;
            records[firstIndex] = *record;
            SortIndicesDescending(priorities, order, groupLength);
            for (index = 0; index < groupLength; index++) {
                copies[index] = records[firstIndex + index];
            }

            for (index = 0; index < groupLength; index++) {
                records[firstIndex + index] = copies[order[index]];
            }

            TransferSaveData(GetSaveData(), TRUE);
        }
    }

    osSyncPrintf("\n");
}

s32 SaveOrContinueGameByMode(Boolean isContinuing) {
    Player *players;
    PlayRecord *records;
    SavedGame *game;
    s32 playerCount;
    Boolean hasIllegalState;
    s32 index;

    playerCount = 0;
    switch (g_GameState.gameMode) {
        case GAME_MODE_STROKE:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_SKINS_MATCH:
            players = &g_SaveData.modePlayers[MODE0_PLAYER_INDEX];
            records = &g_SaveData.modeRecords[MODE0_RECORD_INDEX];
            game = &g_SaveData.modeGames[MODE0_GAME_INDEX];
            playerCount = MODE0_PLAYER_COUNT;
            break;
        case GAME_MODE_TOURNAMENT:
            players = &g_SaveData.modePlayers[MODE5_PLAYER_INDEX];
            records = &g_SaveData.modeRecords[MODE5_RECORD_INDEX];
            game = &g_SaveData.modeGames[MODE5_GAME_INDEX];
            osSyncPrintf("Record/Play -Tour-/\n");
            playerCount = MODE5_PLAYER_COUNT;
            break;
        case GAME_MODE_GET_CHARACTER:
            players = &g_SaveData.modePlayers[MODE4_PLAYER_INDEX];
            records = &g_SaveData.modeRecords[MODE4_RECORD_INDEX];
            game = &g_SaveData.modeGames[MODE4_GAME_INDEX];
            playerCount = MODE4_PLAYER_COUNT;
            break;
#ifdef AVOID_UB
        default:
            return -1;
#endif
    }

    if (!isContinuing) {
        hasIllegalState = FALSE;
        if (g_GameState.playerCount > playerCount) {
            osSyncPrintf("Too Many Players\n");
            return -1;
        }

        playerCount = g_GameState.playerCount;
        game->isValid = FALSE;
        for (index = 0; index < playerCount; index++) {
            players[index] = g_Players[index];
            switch (players[index].state) {
                case PLAYER_STATE_STARTING_TURN:
                case PLAYER_STATE_AIMING:
                case PLAYER_STATE_UNUSED_AIMING:
                case PLAYER_STATE_PREPARING_AIM:
                    players[index].state = PLAYER_STATE_STARTING_TURN;
                    break;
                default:
                    osSyncPrintf("Illegal PlayerState\n");
                    hasIllegalState = TRUE;
                    break;
                case PLAYER_STATE_WAITING:
                case PLAYER_STATE_HOLE_DONE:
                    break;
            }

            records[index] = g_PlayRecords[index];
        }

        game->state = g_GameState;
        game->hole = g_HoleSetup;
        if (!hasIllegalState) {
            game->isValid = TRUE;
            return 0;
        }

        return -1;
    }

    if (game->isValid == TRUE) {
        if (game->state.playerCount > playerCount) {
            osSyncPrintf("Too Many Players\n");
            return -1;
        }

        playerCount = game->state.playerCount;
        for (index = 0; index < playerCount; index++) {
            g_Players[index] = players[index];
            g_PlayRecords[index] = records[index];
        }

        g_GameState = game->state;
        g_GameState.step = GAME_STATE_LOAD_HOLE;
        SetGameFlag(GAME_FLAG_LOADED_PLAY);
        g_HoleSetup = game->hole;
    }

    g_GameState.isRoundContinued = ROUND_CONTINUED;
    osSyncPrintf("Go Continue\n");
    return 0;
}

#define SAVE_COUNT_MAX 99

s32 SaveOrContinueGame(s32 slotIndex, Boolean isContinuing) {
    Player *players;
    SavedGame *game;
    s32 playerCount;
    Boolean hasIllegalState;
    s32 index;

    playerCount = PLAYER_COUNT_MAX;
    players = g_SaveData.savedPlayers[slotIndex];
    game = &g_SaveData.savedGames[slotIndex];
    if (!isContinuing) {
        hasIllegalState = FALSE;
        if (g_GameState.playerCount > playerCount) {
            osSyncPrintf("Too Many Players\n");
            return -1;
        }

        playerCount = g_GameState.playerCount;
        game->isValid = FALSE;
        for (index = 0; index < playerCount; index++) {
            players[index] = g_BackedUpPlayers[index];
            switch (players[index].state) {
                case PLAYER_STATE_STARTING_TURN:
                case PLAYER_STATE_AIMING:
                case PLAYER_STATE_UNUSED_AIMING:
                case PLAYER_STATE_PREPARING_AIM:
                    players[index].state = PLAYER_STATE_STARTING_TURN;
                    break;
                default:
                    osSyncPrintf("Illegal PlayerState\n");
                    hasIllegalState = TRUE;
                    break;
                case PLAYER_STATE_WAITING:
                case PLAYER_STATE_HOLE_DONE:
                    break;
            }
        }

        game->state = g_BackedUpGameState;
        if (game->state.saveCount < SAVE_COUNT_MAX) {
            game->state.saveCount++;
        }

        game->hole = g_HoleSetup;
        game->roundStats = g_RoundStats;
        if (hasIllegalState) {
            return -1;
        }

        game->isValid = TRUE;
    }
    else {
        if (game->isValid == TRUE) {
            if (game->state.playerCount > playerCount) {
                osSyncPrintf("Too Many Players\n");
                return -1;
            }

            playerCount = game->state.playerCount;
            for (index = 0; index < playerCount; index++) {
                g_Players[index] = players[index];
            }

            g_GameState = game->state;
            g_GameState.step = GAME_STATE_LOAD_HOLE;
            SetGameFlag(GAME_FLAG_LOADED_PLAY);
            g_HoleSetup = game->hole;
            g_RoundStats = game->roundStats;
        }

        g_GameState.isRoundContinued = ROUND_CONTINUED;
        osSyncPrintf("Go Continue\n");
    }

    if (!isContinuing) {
        TransferSaveData(GetSaveData(), TRUE);
    }

    return 0;
}

void RecordDisplayListLength(u32 commandCount) {
    if (g_SaveData.longestDisplayListLength < commandCount) {
        g_SaveData.longestDisplayListLength = commandCount;
    }
}

void InsertBestTime(
    CourseIndex courseIndex,
    s32 time,
    s32 scoreToPar,
    CharacterIndex characterIndex,
    s32 paletteIndex
) {
    SaveData *save;
    s32 count;
    s32 index;
    s32 runIndex;
    s32 scanIndex;
    s32 otherIndex;
    s32 unused[4] UNUSED;

    save = GetSaveData();
    if (courseIndex < 0) {
        courseIndex = 0;
    }

    if (courseIndex >= COURSE_COUNT) {
        courseIndex = COURSE_COUNT - 1;
    }

    if (time >= 0) {
        if (time <= STOPWATCH_FRAMES_MAX) {
            if (paletteIndex < 0) {
                paletteIndex = 0;
            }

            if (paletteIndex > CHARACTER_PALETTE_COUNT - 1) {
                paletteIndex = CHARACTER_PALETTE_COUNT - 1;
            }

            count = 0;
            for (index = 0; index != BEST_TIME_COUNT; index++) {
                count += save->records.bestTimes[courseIndex][index] > 0;
            }

            for (index = 0; index != count; index++) {
                for (scanIndex = index + 1; scanIndex != count; scanIndex++) {
                    if (save->records.bestTimes[courseIndex][index] >
                        save->records.bestTimes[courseIndex][scanIndex]) {
                        save->records.bestTimes[courseIndex][index] =
                            save->records.bestTimes[courseIndex][scanIndex];
                        save->records.bestTimeCharacters[courseIndex][index] =
                            save->records.bestTimeCharacters[courseIndex][scanIndex];
                        save->records.bestTimeScoresToPar[courseIndex][index] =
                            save->records.bestTimeScoresToPar[courseIndex][scanIndex];
                        save->records.bestTimePalettes[courseIndex][index] =
                            save->records.bestTimePalettes[courseIndex][scanIndex];
                    }
                }
            }

            for (index = 0; index != count; index++) {
                for (scanIndex = index + 1; scanIndex != count; scanIndex++) {
                    if (save->records.bestTimeScoresToPar[courseIndex][index] !=
                        save->records.bestTimeScoresToPar[courseIndex][scanIndex]) {
                        break;
                    }
                }

                if (index + 1 < scanIndex) {
                    for (runIndex = index; runIndex != scanIndex; runIndex++) {
                        for (otherIndex = runIndex + 1; otherIndex != scanIndex; otherIndex++) {
                            if (save->records.bestTimeScoresToPar[courseIndex][runIndex] >
                                save->records.bestTimeScoresToPar[courseIndex][otherIndex]) {
                                save->records.bestTimes[courseIndex][runIndex] =
                                    save->records.bestTimes[courseIndex][otherIndex];
                                save->records.bestTimeCharacters[courseIndex][runIndex] =
                                    save->records.bestTimeCharacters[courseIndex][otherIndex];
                                save->records.bestTimeScoresToPar[courseIndex][runIndex] =
                                    save->records.bestTimeScoresToPar[courseIndex][otherIndex];
                                save->records.bestTimePalettes[courseIndex][runIndex] =
                                    save->records.bestTimePalettes[courseIndex][otherIndex];
                            }
                        }
                    }
                }
            }

            if (count > 0) {
                for (index = 0; index != count; index++) {
                    if (time < save->records.bestTimes[courseIndex][index]) {
                        break;
                    }

                    if (time == save->records.bestTimes[courseIndex][index] &&
                        scoreToPar <= save->records.bestTimeScoresToPar[courseIndex][index]) {
                        break;
                    }
                }

                if (index < BEST_TIME_COUNT) {
                    for (scanIndex = BEST_TIME_COUNT - 1; index < scanIndex; scanIndex--) {
                        save->records.bestTimes[courseIndex][scanIndex] =
                            save->records.bestTimes[courseIndex][scanIndex - 1];
                        save->records.bestTimeCharacters[courseIndex][scanIndex] =
                            save->records.bestTimeCharacters[courseIndex][scanIndex - 1];
                        save->records.bestTimeScoresToPar[courseIndex][scanIndex] =
                            save->records.bestTimeScoresToPar[courseIndex][scanIndex - 1];
                        save->records.bestTimePalettes[courseIndex][scanIndex] =
                            save->records.bestTimePalettes[courseIndex][scanIndex - 1];
                    }

                    save->records.bestTimes[courseIndex][index] = time;
                    save->records.bestTimeCharacters[courseIndex][index] = characterIndex;
                    save->records.bestTimeScoresToPar[courseIndex][index] = scoreToPar;
                    save->records.bestTimePalettes[courseIndex][index] = paletteIndex;
                }
            }
            else {
                save->records.bestTimes[courseIndex][0] = time;
                save->records.bestTimeCharacters[courseIndex][0] = characterIndex;
                save->records.bestTimeScoresToPar[courseIndex][0] = scoreToPar;
                save->records.bestTimePalettes[courseIndex][0] = paletteIndex;
            }

            TransferSaveData(save, TRUE);
        }
    }
}

Boolean HasEveryRingShotStar(void) {
    SaveData *save;
    s32 starCount;
    CourseIndex courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->records.hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }

    return starCount == RING_SHOT_COURSE_COUNT * RING_SHOT_HOLE_COUNT;
}

Boolean IsDonkeyKongUnlocked(void) {
    SaveData *save;
    s32 starCount;
    CourseIndex courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->records.hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }

    return starCount >= DONKEY_KONG_STAR_COUNT;
}

s32 GetRingShotCourseOpenedAtStarCount(void) {
    SaveData *save;
    s32 starCount;
    CourseIndex courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    starCount = 0;
    for (courseIndex = 0; courseIndex != RING_SHOT_COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != RING_SHOT_HOLE_COUNT; holeIndex++) {
            starCount += save->records.hasRingShotStar[courseIndex][holeIndex] != FALSE;
        }
    }

    if (starCount == RING_SHOT_STARS_PER_OPENED_COURSE * 1) {
        return 1;
    }
    else if (starCount == RING_SHOT_STARS_PER_OPENED_COURSE * 2) {
        return 2;
    }
    else if (starCount == RING_SHOT_STARS_PER_OPENED_COURSE * 3) {
        return 3;
    }
    else if (starCount == RING_SHOT_STARS_PER_OPENED_COURSE * 4) {
        return 4;
    }
    else if (starCount == RING_SHOT_STARS_PER_OPENED_COURSE * 5) {
        return 5;
    }

    return 0;
}

Boolean IsMapleUnlocked(void) {
    SaveData *save;
    s32 badgeCount;
    CourseIndex courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
            badgeCount += save->records.awards.birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
        }
    }

    return badgeCount >= MAPLE_BADGE_COUNT;
}

Boolean IsMetalMarioUnlocked(void) {
    SaveData *save;
    s32 badgeCount;
    CourseIndex courseIndex;
    s32 holeIndex;

    save = GetSaveData();
    badgeCount = 0;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        for (holeIndex = 0; holeIndex != COURSE_HOLE_COUNT; holeIndex++) {
            badgeCount += save->records.awards.birdieBadgeCounts[courseIndex][holeIndex] != FALSE;
        }
    }

    return badgeCount == COURSE_COUNT * COURSE_HOLE_COUNT;
}

// playerCount is reused to count a character's wins and then to hold an
// imported character's ID; giving each use its own variable breaks the
// match.
void BuildCharacterGrid(s32 playerCount, Boolean skipsImportedFaces) {
    EncodedAsset asset;
    AssetFile file;
    SaveData *save;
    s32 row;
    s32 column;
    s32 index;
    CharacterIndex characterIndex;

    save = GetSaveData();
    for (row = 0; row != CHARACTER_GRID_ROW_COUNT; row++) {
        for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
            g_CharacterGrid[row][column] = f_StartingCharacterGrid[row][column];
        }
    }

    if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                g_CharacterGrid[row][column] = f_EveryCharacterGrid[row][column];
            }
        }

        PlaceImportedCharacters();
        return;
    }

    if (playerCount >= 2) {
        for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
            for (column = 0; column != MULTIPLAYER_GRID_COLUMN_COUNT; column++) {
                g_CharacterGrid[row][column] = f_EveryCharacterGrid[row][column];
            }
        }
    }

    for (index = 0; index != RECORD_CHARACTER_COUNT; index++) {
        for (characterIndex = 0, playerCount = 0; characterIndex != CHARACTER_COUNT;
            characterIndex++) {
            if (save->records.awards.matchups[characterIndex][index].winBits & WIN_COUNT_MASK) {
                playerCount++;
                break;
            }
        }

        if (playerCount > 0) {
            for (row = 0; row != CHARACTER_CHOICE_ROW_COUNT; row++) {
                for (column = 0; column != CHARACTER_GRID_COLUMN_COUNT; column++) {
                    if (f_RecordGrid[row][column] == index) {
                        g_CharacterGrid[row][column] = f_CharacterChoices[index];
                    }
                }
            }
        }
    }

    if (IsMapleUnlocked()) {
        g_CharacterGrid[0][5] = CHARACTER_CHOICE_MAPLE;
    }

    if (IsDonkeyKongUnlocked()) {
        g_CharacterGrid[1][5] = CHARACTER_CHOICE_DONKEY_KONG;
    }

    if (IsMetalMarioUnlocked()) {
        g_CharacterGrid[1][6] = CHARACTER_CHOICE_METAL_MARIO;
    }

    PlaceImportedCharacters();
    for (index = 0; index != MARKABLE_CHARACTER_COUNT; index++) {
        row = f_SlotCells[index].row;
        column = f_SlotCells[index].column;
        if (g_CharacterGrid[row][column] < 0) {
            g_CharacterGrid[row][column] = index - MARKED_CHARACTER_OFFSET;
            break;
        }
    }

    if (index == MARKABLE_CHARACTER_COUNT) {
        if (g_CharacterGrid[0][6] != CHARACTER_CHOICE_BOWSER) {
            g_CharacterGrid[0][6] = CHARACTER_CHOICE_BOWSER - MARKED_CHARACTER_OFFSET;
        }
    }

    if ((u8)skipsImportedFaces != TRUE) {
        for (index = 0; index != ARRAY_COUNT(g_ImportedCharacters); index++) {
            characterIndex = g_CharacterGrid[2][index];
            if (characterIndex >= 0) {
                playerCount = g_ImportedCharacters[index].characterIndex;
                OpenEncodedAsset(g_FaceAnimationAssets[playerCount][0], &asset);
                ReadEncodedAsset(g_CharacterFaceImages[FIRST_IMPORTED_CHARACTER + index], &asset);
                OpenAssetFile(g_CharacterFacePaletteFiles[playerCount][0], &file);
                ReadAssetFile(
                    g_CharacterFacePalettes[FIRST_IMPORTED_CHARACTER + index],
                    GetAssetFileSize(&file),
                    &file
                );
            }
        }
    }
}

void PlaceImportedCharacters(void) {
    s16 index;
    s16 byteIndex;
    s16 count;
    char *name;
    u8 letter;

    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        g_ImportedCharacters[index].characterIndex = 0;
        g_ImportedCharacters[index].clubDistance = 0;
        g_ImportedCharacters[index].clubSpin = 0;
        g_ImportedCharacters[index].clubSideSpin = 0;
        for (count = 0; count != IMPORTED_CHARACTER_NAME_LENGTH; count++) {
            g_ImportedCharacters[index].name[count] = 0;
        }
    }

    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        for (count = 0; count != IMPORTED_CHARACTER_NAME_LENGTH; count++) {
            f_ImportedCharacterNames[index][count] = 0;
        }
    }

    count = 0;
    for (index = 0; index != IMPORTED_CHARACTER_COUNT; index++) {
        if (g_GameBoyCharacters[index].status == GAME_BOY_CHARACTER_STATUS_PRESENT) {
            count++;
        }
    }

    if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
        count = IMPORTED_CHARACTER_COUNT;
    }

    if (count > 0) {
        if (count > IMPORTED_CHARACTER_COUNT) {
            count = IMPORTED_CHARACTER_COUNT;
        }

        for (index = 0; index != count; index++) {
            if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
                name = f_DebugImportedNames[index];
            }
            else {
                name = g_GameBoyCharacters[index].name;
            }

            for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH; byteIndex++) {
                letter = *name++;
                f_ImportedCharacterNames[index][byteIndex] = letter;
                g_ImportedCharacters[index].name[byteIndex] = letter;
            }

            g_CharacterGrid[f_ImportedCharacterCells[index].row]
                           [f_ImportedCharacterCells[index].column] =
                               FIRST_IMPORTED_CHARACTER + index;
            if (IsGameFlagSet(GAME_FLAG_ALL_CHARACTERS)) {
                g_ImportedCharacters[index].characterIndex = f_DebugImportedCharacters[index];
                g_ImportedCharacters[index].clubDistance = f_DebugImportedDistances[index];
                g_ImportedCharacters[index].clubSpin = f_DebugImportedSpins[index];
                g_ImportedCharacters[index].clubSideSpin = f_DebugImportedSideSpins[index];
                g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].distance =
                    f_DebugImportedDistances[index];
                g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].spin =
                    f_DebugImportedSpins[index];
                g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].sideSpin =
                    f_DebugImportedSideSpins[index];
            }
            else {
                g_ImportedCharacters[index].characterIndex =
                    g_GameBoyCharacters[index].golferIndex + FIRST_IMPORTED_CHARACTER;
                g_ImportedCharacters[index].clubDistance =
                    g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].distance;
                g_ImportedCharacters[index].clubSpin =
                    g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].spin;
                g_ImportedCharacters[index].clubSideSpin =
                    g_Clubs[(FIRST_IMPORTED_CHARACTER + index) * CLUB_SET_SIZE].sideSpin;
            }
        }
    }
}

GridSearchResult FindCharacterOnGrid(CharacterIndex characterIndex) {
    s32 slot;
    s32 row;
    s32 column;

    if (characterIndex < 0) {
        return CHARACTER_OFF_GRID;
    }

    if (characterIndex >= ARRAY_COUNT(f_CharacterSlots)) {
        return CHARACTER_OFF_GRID;
    }

    BuildCharacterGrid(1, TRUE);
    slot = f_CharacterSlots[characterIndex];
    row = f_SlotCells[slot].row;
    column = f_SlotCells[slot].column;
    return g_CharacterGrid[row][column] < 0 ? CHARACTER_OFF_GRID : CHARACTER_ON_GRID;
}

void InvalidateBrokenSavedGames(void) {
    SaveData *save;
    Boolean isInvalid[SAVED_GAME_COUNT];
    s32 index;
    s32 entryIndex;
    SavedGame *game;
    GameState *state;
    Player *player;

    save = GetSaveData();
    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        isInvalid[index] = FALSE;
    }

    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        if (!save->savedGames[index].isValid) {
            continue;
        }

        game = &save->savedGames[index];
        state = &game->state;
        if (state->isMiniGolf == FALSE) {
            switch (state->gameMode) {
                case GAME_MODE_STROKE:
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                case GAME_MODE_TOURNAMENT:
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    break;
                default:
                    isInvalid[index] = TRUE;
                    break;
            }
        }

        if (state->playerCount < 1 || state->playerCount > PLAYER_COUNT_MAX) {
            isInvalid[index] = TRUE;
        }

        if (state->courseIndex < 0 || state->courseIndex >= DRIVING_RANGE_COURSE_INDEX) {
            isInvalid[index] = TRUE;
        }

        if (state->holeIndex < 0 || state->holeIndex >= COURSE_HOLE_COUNT) {
            isInvalid[index] = TRUE;
        }

        for (entryIndex = 0; entryIndex != state->playerCount; entryIndex++) {
            player = &save->savedPlayers[index][entryIndex];
            if ((u8)player->characterIndex >= CHARACTER_COUNT) {
                isInvalid[index] = TRUE;
            }

            if ((u8)player->paletteIndex >= CHARACTER_PALETTE_COUNT) {
                isInvalid[index] = TRUE;
            }
        }
    }

    for (index = 0; index != SAVED_GAME_COUNT; index++) {
        if (isInvalid[index] == TRUE) {
            save->savedGames[index].isValid = FALSE;
        }
    }
}

void InvalidateBrokenRingShotRecords(void) {
    SaveData *save;
    Boolean isInvalid[RING_SHOT_RECORD_COUNT];
    s32 recordIndex;
    s32 scoreKind;
    s32 flaggedScoreKind;
    s32 unused[2] UNUSED;

    save = GetSaveData();
    for (recordIndex = 0; recordIndex != RING_SHOT_RECORD_COUNT; recordIndex++) {
        isInvalid[recordIndex] = FALSE;
    }

    for (recordIndex = 0; recordIndex != RING_SHOT_RECORD_COUNT; recordIndex++) {
        RingShotRecord *record = &save->ringShotRecords[recordIndex];

        flaggedScoreKind = record->scoreKind;
        scoreKind = flaggedScoreKind;
        flaggedScoreKind &= SCORE_KIND_RECORDED;
        if (flaggedScoreKind != 0) {
            if ((scoreKind &= SCORE_KIND_MASK) >= SCORE_KIND_COUNT || scoreKind < 0) {
                isInvalid[recordIndex] = TRUE;
            }
        }

        if ((u8)record->characterIndex >= CHARACTER_COUNT) {
            isInvalid[recordIndex] = TRUE;
        }

        if ((u8)record->paletteIndex >= CHARACTER_PALETTE_COUNT) {
            isInvalid[recordIndex] = TRUE;
        }

        if ((u8)record->courseIndex >= RING_SHOT_COURSE_COUNT) {
            isInvalid[recordIndex] = TRUE;
        }

        if ((u8)record->holeIndex >= RING_SHOT_HOLE_COUNT) {
            isInvalid[recordIndex] = TRUE;
        }
    }

    for (recordIndex = 0; recordIndex != RING_SHOT_RECORD_COUNT; recordIndex++) {
        RingShotRecord *record = &save->ringShotRecords[recordIndex];

        if (isInvalid[recordIndex] == TRUE) {
            record->scoreKind ^= SCORE_KIND_RECORDED;
        }
    }
}

void RememberPlayedHole(void) {
    MenuMemory *memory;

    memory = GetMenuMemory();
    memory->holeIndex = g_GameState.holeIndex;
    memory->courseIndex = g_GameState.courseIndex;
}

GameMode GetClubTournamentGameMode(void) {
    GameMode gameMode;

    switch (g_ClubTournamentRules.isSpeedGolf) {
        case FALSE:
        default:
            gameMode = GAME_MODE_STROKE;
            break;
        case TRUE:
            gameMode = GAME_MODE_SPEED_GOLF;
            break;
    }

    return gameMode;
}

s32 GetClubTournamentCourse(void) {
    return g_ClubTournamentRules.courseIndex;
}

Boolean IsNintendoClubTournament(void) {
    return g_IsNintendoClubTournament != FALSE;
}

#define MESSAGE_FIRST_COURSE_NAME 0x1EA
// The original names a club tournament's course with the message after its
// own: Koopa Park for Toad Highlands, and DUMMY for Mario's Star.
#define CLUB_COURSE_NAME_MESSAGE_SHIFT 1
#define MESSAGE_STROKE_PLAY 0x9E
#define MESSAGE_SPEED_GOLF 0x9D
#define MESSAGE_FIRST_MINI_GOLF_TITLE 0xC7
#define MESSAGE_SECOND_MINI_GOLF_TITLE 0xC8

MessageId GetClubTournamentNameMessage(void) {
    s32 nameIndex;

    nameIndex = g_ClubTournamentNameIndex;
    if (nameIndex < 0) {
        nameIndex = 0;
    }

    return nameIndex + MESSAGE_FIRST_CLUB_TOURNAMENT_NAME;
}

MessageId GetClubTournamentCourseMessage(void) {
    CourseIndex courseIndex;

    courseIndex = GetClubTournamentCourse();
    if (courseIndex < 0) {
        courseIndex = 0;
    }

    return courseIndex + (MESSAGE_FIRST_COURSE_NAME + CLUB_COURSE_NAME_MESSAGE_SHIFT);
}

s32 GetClubTournamentNumber(void) {
    s32 tournamentNumber;

    tournamentNumber = g_ClubTournamentNumber;
    if (tournamentNumber <= 0) {
        tournamentNumber = 1;
    }

    return tournamentNumber;
}

void CopyTournamentCode(u8 *destination) {
    s32 index;

    for (index = 0; index != ARRAY_COUNT(g_TournamentCode); index++) {
        *destination++ = g_TournamentCode[index];
    }
}

MessageId GetClubTournamentModeMessage(void) {
    MessageId messageId;

    switch (g_ClubTournamentRules.isSpeedGolf) {
        case FALSE:
        default:
            messageId = MESSAGE_STROKE_PLAY;
            break;
        case TRUE:
            messageId = MESSAGE_SPEED_GOLF;
            break;
    }

    return messageId;
}

MessageId GetClubTournamentCourseTitleMessage(void) {
    MessageId messageId;

    if (g_ClubTournamentRules.courseIndex >= MINI_GOLF_COURSE_INDEX) {
        messageId = MESSAGE_FIRST_MINI_GOLF_TITLE;
        if (g_ClubTournamentRules.courseIndex >=
            MINI_GOLF_COURSE_INDEX + MINI_GOLF_COURSES_PER_RECORD) {
            messageId = MESSAGE_SECOND_MINI_GOLF_TITLE;
        }
    }
    else {
        messageId = g_ClubTournamentRules.courseIndex +
            (MESSAGE_FIRST_COURSE_NAME + CLUB_COURSE_NAME_MESSAGE_SHIFT);
    }

    return messageId;
}

void ClearImportedCharacters(void) {
    s32 index;
    s32 byteIndex;

    for (index = 0; index != ARRAY_COUNT(g_ImportedCharacters); index++) {
        g_ImportedCharacters[index].characterIndex = 0;
        g_ImportedCharacters[index].clubDistance = 0;
        g_ImportedCharacters[index].clubSpin = 0;
        g_ImportedCharacters[index].clubSideSpin = 0;
        for (byteIndex = 0; byteIndex != IMPORTED_CHARACTER_NAME_LENGTH; byteIndex++) {
            g_ImportedCharacters[index].name[byteIndex] = 0;
        }

        g_ImportedCharacters[index].isChosen = FALSE;
    }
}

void ComputeRoundStatistics(void) {
    CourseStats *allCourseStats;
    CourseStats *courseStats;
    f32 reciprocal;
    s32 sandSaveAttemptCount;
    CourseIndex courseIndex;
    Boolean hasPlayedCourse;
    f32 strokeSum;
    f32 measuredDriveSum;
    f32 puttSum;
    f32 parOrBetterSum;
    f32 parOnSum;
    f32 sandSaveSum;
    f32 sandSaveChanceSum;
    f32 driveYardSum;
    s32 driveCountSum;
    f32 unplayedRoundCount;
    f32 emptyCount;
    f32 holeCount;

    allCourseStats = GetSaveData()->records.courseStats;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        courseStats = &allCourseStats[courseIndex];
        g_CourseStatFigures[courseIndex].roundCount = courseStats->roundCount;
        if (g_CourseStatFigures[courseIndex].roundCount == 0.0f) {
            g_CourseStatFigures[courseIndex].bestScore = 0.0f;
            g_CourseStatFigures[courseIndex].scoreAverage = 0.0f;
            g_CourseStatFigures[courseIndex].bestDrive = 0.0f;
            g_CourseStatFigures[courseIndex].driveAverage = 0.0f;
            g_CourseStatFigures[courseIndex].unusedDriveFigure = 0.0f;
            g_CourseStatFigures[courseIndex].fairwayPercent = 0.0f;
            g_CourseStatFigures[courseIndex].puttAverage = 0.0f;
            g_CourseStatFigures[courseIndex].parOnPercent = 0.0f;
            g_CourseStatFigures[courseIndex].parSavePercent = 0.0f;
            g_CourseStatFigures[courseIndex].hasPlace[THIRD_PLACE_INDEX] = FALSE;
            g_CourseStatFigures[courseIndex].hasPlace[SECOND_PLACE_INDEX] = FALSE;
            g_CourseStatFigures[courseIndex].hasPlace[FIRST_PLACE_INDEX] = FALSE;
        }
        else {
            reciprocal = 1.0f / courseStats->roundCount;
            g_CourseStatFigures[courseIndex].bestScore = courseStats->bestScore;
            g_CourseStatFigures[courseIndex].scoreAverage = courseStats->strokeTotal * reciprocal;
            g_CourseStatFigures[courseIndex].bestDrive = courseStats->longestDrive * YARDS_PER_UNIT;
            g_CourseStatFigures[courseIndex].driveAverage =
                courseStats->driveAverage * YARDS_PER_UNIT;
            g_CourseStatFigures[courseIndex].fairwayPercent = courseStats->measuredDriveTotal *
                reciprocal * ((f32)PERCENT_PER_WHOLE / HOLES_PER_ROUND);
            g_CourseStatFigures[courseIndex].puttAverage =
                courseStats->puttTotal * reciprocal * (1.0f / HOLES_PER_ROUND);
            g_CourseStatFigures[courseIndex].parSavePercent = courseStats->parOrBetterCount *
                reciprocal * ((f32)PERCENT_PER_WHOLE / HOLES_PER_ROUND);
            g_CourseStatFigures[courseIndex].parOnPercent =
                courseStats->parOnCount * reciprocal * ((f32)PERCENT_PER_WHOLE / HOLES_PER_ROUND);
            sandSaveAttemptCount = courseStats->sandSaveCount + courseStats->missedSandSaveCount;
            if (sandSaveAttemptCount <= 0) {
                g_CourseStatFigures[courseIndex].sandSavePercent = 0.0f;
            }
            else {
                g_CourseStatFigures[courseIndex].sandSavePercent =
                    ((f32)courseStats->sandSaveCount / (f32)sandSaveAttemptCount) *
                    (f32)PERCENT_PER_WHOLE;
            }

            g_CourseStatFigures[courseIndex].hasPlace[THIRD_PLACE_INDEX] =
                courseStats->hasPlace[THIRD_PLACE_INDEX];
            g_CourseStatFigures[courseIndex].hasPlace[SECOND_PLACE_INDEX] =
                courseStats->hasPlace[SECOND_PLACE_INDEX];
            g_CourseStatFigures[courseIndex].hasPlace[FIRST_PLACE_INDEX] =
                courseStats->hasPlace[FIRST_PLACE_INDEX];
        }
    }

    courseIndex = 0;
    hasPlayedCourse = FALSE;
    g_CourseStatFigures[COURSE_COUNT].roundCount = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].bestScore = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].scoreAverage = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].bestDrive = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].driveAverage = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].fairwayPercent = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].puttAverage = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].parSavePercent = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].parOnPercent = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].sandSavePercent = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].hasPlace[THIRD_PLACE_INDEX] = FALSE;
    g_CourseStatFigures[COURSE_COUNT].hasPlace[SECOND_PLACE_INDEX] = FALSE;
    g_CourseStatFigures[COURSE_COUNT].hasPlace[FIRST_PLACE_INDEX] = FALSE;
    g_CourseStatFigures[COURSE_COUNT].bestScore = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].bestDrive = 0.0f;
    g_CourseStatFigures[COURSE_COUNT].bestScore = g_CourseStatFigures[0].bestScore;
    g_CourseStatFigures[COURSE_COUNT].bestDrive = g_CourseStatFigures[0].bestDrive;
    unplayedRoundCount = g_CourseStatFigures[COURSE_COUNT].roundCount;
    for (; courseIndex != COURSE_COUNT; courseIndex++) {
        if (unplayedRoundCount < g_CourseStatFigures[courseIndex].roundCount) {
            if (!hasPlayedCourse) {
                g_CourseStatFigures[COURSE_COUNT].bestScore =
                    g_CourseStatFigures[courseIndex].bestScore;
                hasPlayedCourse = TRUE;
                g_CourseStatFigures[COURSE_COUNT].bestDrive =
                    g_CourseStatFigures[courseIndex].bestDrive;
            }

            if (g_CourseStatFigures[COURSE_COUNT].bestScore >
                g_CourseStatFigures[courseIndex].bestScore) {
                g_CourseStatFigures[COURSE_COUNT].bestScore =
                    g_CourseStatFigures[courseIndex].bestScore;
            }

            if (g_CourseStatFigures[COURSE_COUNT].bestDrive <
                g_CourseStatFigures[courseIndex].bestDrive) {
                g_CourseStatFigures[COURSE_COUNT].bestDrive =
                    g_CourseStatFigures[courseIndex].bestDrive;
            }

            g_CourseStatFigures[COURSE_COUNT].roundCount +=
                g_CourseStatFigures[courseIndex].roundCount;
        }
    }

    driveCountSum = 0;
    sandSaveSum = 0.0f;
    sandSaveChanceSum = 0.0f;
    parOrBetterSum = 0.0f;
    measuredDriveSum = 0.0f;
    parOnSum = 0.0f;
    puttSum = 0.0f;
    driveYardSum = 0.0f;
    strokeSum = 0.0f;
    emptyCount = 0.0f;
    for (courseIndex = 0; courseIndex != COURSE_COUNT; courseIndex++) {
        courseStats = &allCourseStats[courseIndex];
        if (emptyCount < g_CourseStatFigures[courseIndex].roundCount) {
            measuredDriveSum += courseStats->measuredDriveTotal;
            puttSum += courseStats->puttTotal;
            parOrBetterSum += courseStats->parOrBetterCount;
            parOnSum += courseStats->parOnCount;
            strokeSum += courseStats->strokeTotal;
            sandSaveSum += courseStats->sandSaveCount;
            sandSaveChanceSum += courseStats->sandSaveCount + courseStats->missedSandSaveCount;
            driveCountSum += courseStats->averagedDriveCount;
            driveYardSum +=
                courseStats->driveAverage * YARDS_PER_UNIT * courseStats->averagedDriveCount;
        }
    }

    if (g_CourseStatFigures[COURSE_COUNT].roundCount > 0.0f) {
        if (sandSaveChanceSum <= 0.0f) {
            g_CourseStatFigures[COURSE_COUNT].sandSavePercent = 0.0f;
        }
        else {
            g_CourseStatFigures[COURSE_COUNT].sandSavePercent =
                (sandSaveSum / sandSaveChanceSum) * (f32)PERCENT_PER_WHOLE;
        }

        holeCount = g_CourseStatFigures[COURSE_COUNT].roundCount * HOLES_PER_ROUND;
        g_CourseStatFigures[COURSE_COUNT].puttAverage = puttSum / holeCount;
        g_CourseStatFigures[COURSE_COUNT].fairwayPercent =
            (measuredDriveSum / holeCount) * (f32)PERCENT_PER_WHOLE;
        g_CourseStatFigures[COURSE_COUNT].parSavePercent =
            (parOrBetterSum / holeCount) * (f32)PERCENT_PER_WHOLE;
        g_CourseStatFigures[COURSE_COUNT].parOnPercent =
            (parOnSum / holeCount) * (f32)PERCENT_PER_WHOLE;
        g_CourseStatFigures[COURSE_COUNT].scoreAverage =
            strokeSum / g_CourseStatFigures[COURSE_COUNT].roundCount;
        if (driveYardSum > 0.0f && driveCountSum > 0) {
            g_CourseStatFigures[COURSE_COUNT].driveAverage = driveYardSum / driveCountSum;
        }
        else {
            g_CourseStatFigures[COURSE_COUNT].driveAverage = 0.0f;
        }
    }
}

void ResetMenuChoices(void) {
    s8 index;

    g_MenuCourseCount = COURSE_COUNT;
    g_IsChosenHoleLoadStarted = FALSE;
    g_MenuScreen = MENU_SCREEN_TITLE;
    g_ChosenPlayerCount = 0;
    g_ChoosingPlayerIndex = 0;
    g_ChosenCharacters[0] = NO_CHOSEN_CHARACTER;
    g_ChosenCharacters[1] = NO_CHOSEN_CHARACTER;
    g_ChosenCharacters[2] = NO_CHOSEN_CHARACTER;
    g_ChosenCharacters[3] = NO_CHOSEN_CHARACTER;
    g_IsMenuCharacterLoaded[0] = FALSE;
    g_IsMenuCharacterLoaded[1] = FALSE;
    g_IsMenuCharacterLoaded[2] = FALSE;
    g_IsMenuCharacterLoaded[3] = FALSE;
    memset(&g_MenuMemory, 0, sizeof(g_MenuMemory));
    g_TakenCharacters.unusedMenuValue = 0;
    g_LastMenuScreen = 0;
    g_LastMenuPlayerCountChoice = 0;
    g_ChosenLabelGrowthDirection = LABEL_GROWING;
    g_ChosenLabelGrowth = 0;
    g_CourseSelectKind = COURSE_SELECT_KIND_NONE;
    COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_AWARDS;
    for (index = 0; index != ARRAY_COUNT(g_ChosenLefties); index++) {
        g_ChosenLefties[index] = 0;
    }

    g_MenuMemory.holesChoice = HOLES_CHOICE_ALL;
    g_GameState.holeSelection = HOLE_SELECTION_ALL;
    g_MenuMemory.statsCourse = COURSE_COUNT;
    g_IsRingShotWipeOpening = FALSE;
    f_UnreadMenuEntryByte = 0;
}

void ResetMenuEntryFlags(void) {
    g_IsChosenHoleLoadStarted = FALSE;
    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = FALSE;
    f_UnreadMenuEntryByte = 0;
}

void EnterMenuAtCourseSelect(void) {
    switch (g_GameState.gameMode) {
        case GAME_MODE_TOURNAMENT:
            g_CourseSelectKind = COURSE_SELECT_KIND_TOURNAMENTS;
            COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_AWARDS;
            g_BetSet = BET_SET_NONE;
            break;
        case GAME_MODE_STROKE:
            g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
            COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
            if (g_ChosenPlayerCount == 4) {
                g_BetSet = BET_SET_STROKE_FOUR_PLAYER;
            }
            else {
                g_BetSet = BET_SET_STROKE;
            }

            g_AreBetPagesBlocked = TRUE;
            break;
        case GAME_MODE_MATCH_GAME:
            g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
            COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
            g_BetSet = BET_SET_MATCH_GAME;
            break;
        case GAME_MODE_SKINS_MATCH:
            g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
            COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
            g_BetSet = BET_SET_SKINS_MATCH;
            break;
        case GAME_MODE_TRAINING:
        case GAME_MODE_GET_CHARACTER:
        case GAME_MODE_UNRECORDED_STROKE:
        case GAME_MODE_SPEED_GOLF:
        case GAME_MODE_RING_SHOT:
            g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
            COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_PLAIN;
            g_BetSet = BET_SET_NONE;
            break;
        case GAME_MODE_CLUB_SLOTS:
            g_CourseSelectKind = COURSE_SELECT_KIND_COURSES;
            COURSE_SELECT_LAYOUT = COURSE_SELECT_LAYOUT_HOLES;
            g_BetSet = BET_SET_CLUB_SLOTS;
            break;
    }

    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = TRUE;
    g_IsChosenHoleLoadStarted = FALSE;
    f_UnreadMenuEntryByte = 0;
    g_MenuScreen = MENU_SCREEN_COURSE_SELECT;
}

void EnterMenuAtTitle(void) {
    ResetMenuChoices();
    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = TRUE;
    g_IsChosenHoleLoadStarted = FALSE;
    f_UnreadMenuEntryByte = 0;
    g_ImportedCharacters[0].isChosen = FALSE;
    g_ImportedCharacters[1].isChosen = FALSE;
    g_ImportedCharacters[2].isChosen = FALSE;
    g_ImportedCharacters[3].isChosen = FALSE;
    g_PlayerControllerIndices[0] = UNCHOSEN_CONTROLLER;
    g_PlayerControllerIndices[1] = UNCHOSEN_CONTROLLER;
    g_PlayerControllerIndices[2] = UNCHOSEN_CONTROLLER;
    g_PlayerControllerIndices[3] = UNCHOSEN_CONTROLLER;
    g_MenuScreen = MENU_SCREEN_TITLE;
}

void EnterMenuAtRingShotExit(void) {
    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = TRUE;
    g_IsChosenHoleLoadStarted = FALSE;
    f_UnreadMenuEntryByte = 0;
    g_MenuScreen = MENU_SCREEN_RING_SHOT_EXIT;
}

void EnterMenuAtModeSelect(void) {
    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = TRUE;
    g_IsChosenHoleLoadStarted = FALSE;
    f_UnreadMenuEntryByte = 0;
    g_MenuScreen = MENU_SCREEN_MODE_SELECT;
}

void EnterMenuAtBlankScreen(void) {
    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = TRUE;
    g_IsChosenHoleLoadStarted = FALSE;
    f_UnreadMenuEntryByte = 0;
    g_MenuScreen = MENU_SCREEN_BLANK;
}

void EnterMenuAtRingShotRecords(void) {
    g_IsRingShotWipeOpening = TRUE;
    g_IsMenuWipeOpening = TRUE;
    g_IsChosenHoleLoadStarted = FALSE;
    f_UnreadMenuEntryByte = 0;
    g_MenuScreen = MENU_SCREEN_RING_SHOT_RECORDS;
}

void EnterMenuAtBestShots(void) {
    g_IsChosenHoleLoadStarted = FALSE;
    g_IsRingShotWipeOpening = TRUE;
    f_UnreadMenuEntryByte = 0;
    g_IsMenuWipeOpening = TRUE;
    g_GameState.replayKind = REPLAY_KIND_NONE;
    ClearGameFlag(GAME_FLAG_LOADED_PLAY);
    g_MenuScreen = MENU_SCREEN_BEST_SHOTS;
}

Boolean8 IsMusicOffChosen(void) {
    return GetSaveData()->options[SAVE_OPTION_MUSIC_OFF];
}

Boolean8 IsMonoChosen(void) {
    return GetSaveData()->options[SAVE_OPTION_MONO];
}

Boolean8 IsSimpleCameraChosen(void) {
    return GetSaveData()->options[SAVE_OPTION_SIMPLE_CAMERA];
}

void WriteSaveData(void) {
    TransferSaveData(GetSaveData(), TRUE);
}

void SyncSaveOptions(SaveData *save, Boolean isWrite, SaveReadResult readResult) {
    u8 *optionBits;
    s8 *options;

    options = save->options;
    if (!isWrite) {
        optionBits = &g_MenuMemory.optionBits;
        *optionBits = 0;
        if (readResult >= SAVE_READ_FIRST_COPY) {
            if (options[SAVE_OPTION_MUSIC_OFF]) {
                *optionBits = OPTION_BIT_MUSIC_OFF;
            }

            if (options[SAVE_OPTION_MONO]) {
                *optionBits |= OPTION_BIT_MONO;
                g_SoundOptionBits = *optionBits;
            }

            if (options[SAVE_OPTION_SIMPLE_CAMERA]) {
                *optionBits |= OPTION_BIT_SIMPLE_CAMERA;
            }
        }
    }
    else {
        if (g_MenuMemory.optionBits & OPTION_BIT_MUSIC_OFF) {
            options[SAVE_OPTION_MUSIC_OFF] = TRUE;
        }
        else {
            options[SAVE_OPTION_MUSIC_OFF] = FALSE;
        }

        if (g_MenuMemory.optionBits & OPTION_BIT_MONO) {
            options[SAVE_OPTION_MONO] = TRUE;
        }
        else {
            options[SAVE_OPTION_MONO] = FALSE;
        }

        if (g_MenuMemory.optionBits & OPTION_BIT_SIMPLE_CAMERA) {
            options[SAVE_OPTION_SIMPLE_CAMERA] = TRUE;
        }
        else {
            options[SAVE_OPTION_SIMPLE_CAMERA] = FALSE;
        }
    }
}

void TransferSaveData(SaveData *save, UBoolean8 isWrite) {
    UBoolean8 isSramSelected;
    UBoolean8 isInSram;
    SaveReadResult readResult;

    isSramSelected = !IsGameFlagSet(GAME_FLAG_SAVE_TO_HOST);
    isInSram = isSramSelected;
    if (!isWrite) {
        readResult = ReadSaveCopies(save, isInSram);
        SyncSaveOptions(save, FALSE, readResult);
    }
    else {
#ifdef AVOID_UB
        readResult = SAVE_READ_FAILED;
#endif
        SyncSaveOptions(save, TRUE, readResult);
        WriteSaveCopies(save, isInSram);
    }
}

SaveReadResult ReadSaveCopies(SaveData *save, UBoolean8 isInSram) {
    u8 unusedBuffer[0x90] UNUSED;
    u8 checkBuffer[0x40];
    SaveCheck *check;
    u32 size;
    s32 unmarkedCount;
    s32 copyIndex;
    u16 crc;

    check = (SaveCheck *)DmaAlignedAddress(checkBuffer);
    unmarkedCount = 0;
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        if (!isInSram) {
            if (ReadHostFile(f_SaveCopyLocations[copyIndex].dataFileName, &size) != HOST_FILE_OK ||
                size != sizeof(SaveData)) {
                continue;
            }

            if (ReadHostFile(f_SaveCopyLocations[copyIndex].checkFileName, &size) != HOST_FILE_OK ||
                size != sizeof(SaveCheck)) {
                continue;
            }
        }
        else {
            nuPiReadSram(f_SaveCopyLocations[copyIndex].dataSramAddress, save, sizeof(SaveData));
            nuPiReadSram(f_SaveCopyLocations[copyIndex].checkSramAddress, check, sizeof(SaveCheck));
        }

        if (save->magic != SAVE_MAGIC) {
            unmarkedCount++;
            continue;
        }

        crc = check->keyedCrc ^ check->key;
        if (crc == ComputeCrc16((u8 *)save, sizeof(SaveData))) {
            break;
        }
    }

    if (copyIndex == 0) {
        return SAVE_READ_FIRST_COPY;
    }

    if (copyIndex == 1) {
        return SAVE_READ_SECOND_COPY;
    }

    ResetSaveData();
    return (unmarkedCount == SAVE_COPY_COUNT) ? SAVE_READ_NEVER_WRITTEN : SAVE_READ_FAILED;
}

Boolean WriteSaveCopies(SaveData *save, UBoolean8 isInSram) {
    u8 checkBuffer[0xC0];
    SaveCheck *check;
    u16 crc;
    s32 dateIndex;
    s32 copyIndex;

    check = (SaveCheck *)DmaAlignedAddress(checkBuffer);
    for (dateIndex = 0; dateIndex < ARRAY_COUNT(save->invertedBuildDate); dateIndex++) {
        save->invertedBuildDate[dateIndex] = ~g_RomBuildDate[dateIndex];
    }

    crc = ComputeCrc16((u8 *)save, sizeof(SaveData));
    for (copyIndex = 0; copyIndex < SAVE_COPY_COUNT; copyIndex++) {
        check->key = rand();
        check->keyedCrc = check->key ^ crc;
        if (!isInSram) {
            if (WriteHostFile(f_SaveCopyLocations[copyIndex].dataFileName, sizeof(SaveData)) ==
                HOST_FILE_OK) {
                WriteHostFile(f_SaveCopyLocations[copyIndex].checkFileName, sizeof(SaveCheck));
            }
        }
        else {
            nuPiWriteSram(f_SaveCopyLocations[copyIndex].dataSramAddress, save, sizeof(SaveData));
            nuPiWriteSram(
                f_SaveCopyLocations[copyIndex].checkSramAddress,
                check,
                sizeof(SaveCheck)
            );
        }
    }

    return TRUE;
}
