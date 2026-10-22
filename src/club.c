#include "common.h"
#include "ball_simulation.h"
#include "club.h"
#include "controller.h"
#include "current_shot.h"
#include "game_flags.h"
#include "heap.h"
#include "host_file.h"
#include "libc.h"
#include "main.h"
#include "math_util.h"
#include "save.h"
#include "sound.h"
#include "surface.h"

/* World units in a yard, and in a foot */
#define UNITS_PER_YARD 14045.184f
#define UNITS_PER_FOOT 4681.728f
#define YARDS_PER_UNIT 0.00007119878f
/* Converts radians to an angle where 0x10000 is a full turn */
#define RADIANS_TO_ANGLE 10430.378060218739
/* The direction GenerateClub measures each test shot's landing against */
#define STRAIGHT_ANGLE 0x1000

/* A power shot reaches this much further than a normal one */
#define POWER_SHOT_SCALE 1.1f
/* The reach of an approach shot, in yards */
#define APPROACH_SHOT_DISTANCE 60
/* The reach of each putt mode with a club usable on the green, in feet */
#define LONG_PUTT_DISTANCE 200
#define MIDDLE_PUTT_DISTANCE 100
#define SHORT_PUTT_DISTANCE 30

/* Tries for GenerateClub and GetPuttPower to bring a test shot within 1% of its target */
#define GENERATION_TOLERANCE 0.01f
#define MAX_CLUB_TRY_COUNT 128
#define MAX_PUTT_TRY_COUNT 64
/* A test shot is stopped after this many steps */
#define MAX_TEST_SHOT_STEP_COUNT 900

/* The stock club sets, whose parameters are in sClubParameters */
#define STOCK_CLUB_SET_COUNT 14
/* The sets after the stock ones are generated, with GenerateClubSet */

/* sPutterPowers has a power for each 1/30 of each shot mode's reach */
#define PUTT_STEP_COUNT 30
/* GetPuttPower's fraction is out of this */
#define PUTT_FRACTION_ONE 256
/* A generated putt is aimed this many world units past its target */
#define PUTT_OVERSHOOT 0x600
#define PUTT_TEST_SURFACE 4

/* The size of WriteClubParameters' text */
#define CLUB_SOURCE_SIZE 0x8000
#define CLUB_SOURCE_LINE_SIZE 256
#define END_OF_FILE_CHARACTER 0x1A

#define CLUB_ASSERT_LINE 543

/* Values of g_ClubTestMode, which tells the ball simulation a test shot is running */
#define CLUB_TEST_NONE 0
#define CLUB_TEST_SHOT 1
#define CLUB_TEST_PUTT 2

/* Frames between the steps StepToClubChoice takes */
#define CLUB_CHOICE_STEP_DELAY 3

#define CLUB_CHANGE_SOUND 0x5B
/* Played when B can't switch to a power shot */
#define POWER_SHOT_REFUSED_SOUND 0x5E
#define CLUB_SOUND_CHANNEL 0xF
#define CLUB_SOUND_VOLUME 0x7F

/* A club's stats, as GenerateClubSet takes them */
typedef struct ClubStats {
    /* 0x0 */ s16 distance; /* In yards */
    /* 0x2 */ u16 pitch;    /* 0x10000 is a full turn */
    /* 0x4 */ s8 unk4;
    /* 0x5 */ s8 unk5;
} ClubStats; /* size = 0x6 */

typedef struct ClubSetStats {
    /* 0x00 */ u8 unk0[0x1C];
    /* 0x1C */ s8 pitch; /* In degrees, added to each club's */
    /* 0x1D */ s8 unk1D;
    /* 0x1E */ s8 unk1E;
    /* 0x1F */ s8 unk1F;
    /* 0x20 */ u8 unk20[0x3C - 0x20];
    /* 0x3C */ ClubStats clubs[CLUB_SET_SIZE]; /* The putter's run 4 bytes into the next set's stats */
} ClubSetStats; /* The sets' stats are 0x8C bytes apart */

/* A selection for StepToClubChoice to move to */
typedef struct ClubChoice {
    /* 0x00 */ u8 unk0[0xC];
    /* 0x0C */ s32 shotMode;
    /* 0x10 */ s32 clubSlot;
} ClubChoice;

extern s32 sPutterPowers[SHOT_MODE_COUNT][PUTT_STEP_COUNT];
/* GetClub returns this for an index past the last club */
extern Club sNoClub;
extern Unk20 sClubTestShot;
extern u8 sUnk14Values[9];
extern s8 sStockUnk30Values[CLUB_SET_COUNT];
extern Unk20 sPutterTestShot;
extern s32 sClubChoiceDelay;
extern s8 g_ShowsBillboardsAtEdge;

s32 func_8003DFD0(s32 x, s32 z);
s32 func_8021EA48(void);

/*
 * Each set's clubs' parameters for each shot mode. WriteClubParameters
 * writes generated parameters as source with this table's layout.
 */
static const ClubParameters sClubParameters[CLUB_COUNT] = {
    { { 91273, 81148, 20488 }, { 10, 10, 15 } },
    { { 80352, 71517, 20154 }, { 11, 10, 13 } },
    { { 71104, 63580, 19613 }, { 12, 16, 12 } },
    { { 62157, 54639, 19140 }, { 20, 18, 15 } },
    { { 56746, 50842, 18471 }, { 19, 20, 18 } },
    { { 51925, 46196, 17780 }, { 20, 23, 17 } },
    { { 47897, 42264, 16917 }, { 25, 28, 20 } },
    { { 43660, 38865, 16568 }, { 31, 26, 23 } },
    { { 39516, 34958, 16296 }, { 36, 36, 27 } },
    { { 35857, 32197, 15960 }, { 34, 34, 33 } },
    { { 31958, 28817, 15952 }, { 37, 34, 30 } },
    { { 27458, 23975, 15605 }, { 41, 41, 51 } },
    { { 19465, 17681, 16037 }, { 51, 48, 49 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 96714, 84400, 20688 }, { 235, 128, 15 } },
    { { 85097, 76077, 20095 }, { 137, 72, 13 } },
    { { 73983, 66540, 19689 }, { 47, 21, 15 } },
    { { 66715, 59309, 19254 }, { 12, 12, 14 } },
    { { 60287, 53032, 18746 }, { 17, 15, 16 } },
    { { 54993, 48482, 18151 }, { 17, 15, 20 } },
    { { 49343, 42696, 17670 }, { 20, 21, 21 } },
    { { 44804, 39405, 16991 }, { 23, 23, 23 } },
    { { 40348, 35122, 16598 }, { 26, 29, 23 } },
    { { 36481, 31597, 16106 }, { 25, 31, 28 } },
    { { 32265, 28208, 15932 }, { 30, 32, 32 } },
    { { 26016, 23939, 15605 }, { 40, 35, 40 } },
    { { 18989, 17165, 15829 }, { 56, 44, 42 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 114263, 101092, 19138 }, { 10, 10, 18 } },
    { { 99750, 88951, 18974 }, { 12, 13, 15 } },
    { { 89648, 79116, 18452 }, { 16, 14, 18 } },
    { { 78202, 68737, 17922 }, { 19, 19, 18 } },
    { { 72335, 63335, 17575 }, { 20, 21, 20 } },
    { { 66544, 57720, 17007 }, { 23, 24, 21 } },
    { { 60072, 52686, 16348 }, { 25, 27, 22 } },
    { { 54785, 47896, 15986 }, { 28, 28, 24 } },
    { { 48998, 43660, 15901 }, { 31, 33, 29 } },
    { { 44483, 38902, 15915 }, { 34, 37, 30 } },
    { { 39609, 35302, 15702 }, { 38, 40, 30 } },
    { { 33415, 29836, 15605 }, { 49, 41, 43 } },
    { { 24048, 21680, 15605 }, { 48, 56, 49 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 95098, 83880, 18851 }, { -419, -279, 19 } },
    { { 85831, 74761, 18588 }, { -376, -221, 18 } },
    { { 74269, 65097, 18079 }, { -313, -169, 18 } },
    { { 65773, 56912, 17745 }, { -73, 10, 20 } },
    { { 59707, 52724, 17431 }, { 18, 19, 20 } },
    { { 54193, 47529, 16773 }, { 21, 20, 21 } },
    { { 50198, 43964, 16293 }, { 22, 26, 25 } },
    { { 46328, 40533, 15935 }, { 27, 29, 25 } },
    { { 41442, 35617, 15647 }, { 31, 33, 28 } },
    { { 36973, 32889, 15605 }, { 35, 33, 30 } },
    { { 33248, 29716, 15605 }, { 33, 40, 31 } },
    { { 28155, 25063, 15605 }, { 39, 39, 47 } },
    { { 20518, 17990, 15575 }, { 46, 46, 55 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 100035, 85831, 21093 }, { 966, 675, 13 } },
    { { 87781, 77621, 20658 }, { 939, 662, 15 } },
    { { 77101, 68679, 20157 }, { 825, 567, 16 } },
    { { 67068, 58727, 19251 }, { 688, 468, 16 } },
    { { 62084, 54708, 18623 }, { 525, 319, 16 } },
    { { 56982, 49273, 17914 }, { 294, 135, 18 } },
    { { 52371, 46246, 16934 }, { 194, 71, 22 } },
    { { 48218, 42430, 16637 }, { 50, 33, 24 } },
    { { 43646, 39144, 16270 }, { 28, 29, 30 } },
    { { 39576, 34986, 16254 }, { 35, 37, 28 } },
    { { 35435, 31265, 16210 }, { 34, 38, 31 } },
    { { 29640, 26184, 15911 }, { 50, 50, 44 } },
    { { 21945, 19729, 16449 }, { 50, 47, 48 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 94443, 82710, 19874 }, { 9, 10, 17 } },
    { { 83190, 74213, 19200 }, { 11, 11, 16 } },
    { { 73150, 65592, 18972 }, { 11, 12, 16 } },
    { { 64381, 55984, 18412 }, { 20, 15, 17 } },
    { { 57743, 51321, 17870 }, { 21, 17, 19 } },
    { { 53499, 47029, 17361 }, { 20, 18, 20 } },
    { { 50317, 43111, 16773 }, { 23, 25, 21 } },
    { { 44267, 38892, 16348 }, { 26, 29, 22 } },
    { { 40306, 35478, 16119 }, { 26, 29, 24 } },
    { { 36351, 31958, 15605 }, { 28, 34, 27 } },
    { { 32605, 28790, 15669 }, { 40, 30, 35 } },
    { { 27473, 24321, 15423 }, { 45, 43, 47 } },
    { { 20172, 17904, 15952 }, { 39, 47, 46 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 118021, 101263, 18852 }, { -937, -800, 17 } },
    { { 103732, 89966, 18326 }, { -896, -761, 18 } },
    { { 90848, 79048, 18357 }, { -830, -660, 19 } },
    { { 78528, 69967, 17904 }, { -914, -678, 17 } },
    { { 71062, 63018, 17754 }, { -741, -484, 18 } },
    { { 65085, 58534, 17397 }, { -500, -330, 21 } },
    { { 57737, 50643, 16628 }, { -421, -227, 22 } },
    { { 52306, 46109, 16250 }, { -77, 11, 24 } },
    { { 46873, 41469, 16140 }, { 24, 24, 23 } },
    { { 41732, 36836, 15605 }, { 26, 30, 23 } },
    { { 37666, 33185, 15605 }, { 33, 29, 26 } },
    { { 31611, 27877, 15163 }, { 36, 35, 35 } },
    { { 23120, 20574, 15605 }, { 40, 41, 45 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 121081, 105182, 20973 }, { 1449, 1180, 13 } },
    { { 106352, 90903, 20181 }, { 1501, 1191, 14 } },
    { { 92463, 81484, 20070 }, { 1417, 1146, 16 } },
    { { 83271, 71385, 18909 }, { 1326, 1056, 15 } },
    { { 74388, 64653, 18584 }, { 1124, 794, 16 } },
    { { 67978, 59497, 17840 }, { 857, 607, 19 } },
    { { 61936, 55532, 17222 }, { 789, 532, 21 } },
    { { 55865, 49324, 16755 }, { 479, 276, 25 } },
    { { 51446, 45760, 16300 }, { 261, 112, 27 } },
    { { 46642, 40924, 16001 }, { 67, 31, 31 } },
    { { 41325, 36423, 16032 }, { 33, 36, 35 } },
    { { 34746, 30536, 15605 }, { 39, 42, 56 } },
    { { 25550, 22727, 15917 }, { 47, 51, 51 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 132818, 115066, 19067 }, { -895, -748, 18 } },
    { { 117959, 103455, 18852 }, { -876, -735, 17 } },
    { { 103257, 91635, 18610 }, { -789, -632, 17 } },
    { { 88561, 76963, 18203 }, { -839, -565, 19 } },
    { { 80117, 70523, 17696 }, { -812, -542, 23 } },
    { { 72626, 63924, 17639 }, { -546, -334, 22 } },
    { { 64521, 56681, 16653 }, { -436, -229, 26 } },
    { { 58355, 50935, 16529 }, { -337, -161, 21 } },
    { { 53410, 45814, 16107 }, { -150, -12, 21 } },
    { { 46813, 41444, 16007 }, { -52, 19, 27 } },
    { { 42090, 37189, 15605 }, { 26, 30, 25 } },
    { { 35419, 31159, 15605 }, { 34, 41, 38 } },
    { { 25727, 22877, 15605 }, { 40, 36, 45 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 138873, 119412, 19440 }, { -1283, -1064, 17 } },
    { { 120879, 104818, 18981 }, { -1237, -981, 15 } },
    { { 105947, 92559, 18699 }, { -1134, -896, 17 } },
    { { 92073, 79255, 18362 }, { -1293, -1015, 21 } },
    { { 82806, 72787, 17745 }, { -1093, -812, 20 } },
    { { 76534, 66734, 17247 }, { -1043, -688, 21 } },
    { { 69011, 61440, 16872 }, { -908, -624, 19 } },
    { { 62652, 54565, 16433 }, { -575, -332, 24 } },
    { { 57092, 48882, 15806 }, { -325, -118, 27 } },
    { { 50643, 44828, 15819 }, { -72, 17, 28 } },
    { { 44729, 39398, 15605 }, { 32, 30, 34 } },
    { { 37792, 32706, 15226 }, { 39, 43, 37 } },
    { { 27322, 24595, 15605 }, { 48, 39, 47 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 106457, 93361, 20055 }, { 9, 10, 16 } },
    { { 91683, 80929, 19468 }, { 14, 11, 18 } },
    { { 81084, 71005, 19159 }, { 15, 15, 15 } },
    { { 72429, 62782, 18686 }, { 17, 19, 14 } },
    { { 65473, 57108, 17951 }, { 21, 24, 19 } },
    { { 59098, 51660, 17501 }, { 22, 25, 18 } },
    { { 55682, 48543, 16845 }, { 25, 23, 22 } },
    { { 50610, 43592, 16307 }, { 24, 27, 20 } },
    { { 46579, 40899, 15886 }, { 31, 29, 29 } },
    { { 41017, 36656, 15892 }, { 37, 33, 31 } },
    { { 36783, 33214, 15773 }, { 34, 34, 31 } },
    { { 30683, 27220, 15837 }, { 35, 42, 46 } },
    { { 22681, 20205, 15782 }, { 44, 53, 45 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 148735, 130352, 20979 }, { 1485, 1337, 17 } },
    { { 131882, 115746, 20254 }, { 1370, 1232, 12 } },
    { { 116071, 99926, 19870 }, { 1339, 1205, 16 } },
    { { 98224, 86372, 19422 }, { 1353, 1136, 15 } },
    { { 88171, 77618, 19175 }, { 1105, 860, 15 } },
    { { 80209, 70554, 18598 }, { 884, 676, 17 } },
    { { 71315, 61835, 17880 }, { 698, 447, 18 } },
    { { 63281, 56566, 17377 }, { 328, 190, 19 } },
    { { 56618, 49898, 17238 }, { 55, 27, 16 } },
    { { 51058, 44343, 17030 }, { 21, 19, 15 } },
    { { 44835, 39214, 16390 }, { 23, 27, 24 } },
    { { 38250, 33360, 15190 }, { 31, 31, 31 } },
    { { 26962, 24182, 15059 }, { 39, 37, 38 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 149091, 129065, 19482 }, { -1610, -1373, 13 } },
    { { 130311, 113687, 19219 }, { -1729, -1485, 13 } },
    { { 115455, 99598, 18907 }, { -1779, -1497, 15 } },
    { { 99530, 87001, 18644 }, { -1998, -1734, 17 } },
    { { 90123, 78456, 17872 }, { -1953, -1717, 19 } },
    { { 82740, 72384, 17777 }, { -1984, -1720, 18 } },
    { { 74839, 64119, 16929 }, { -2076, -1913, 20 } },
    { { 67913, 58363, 16811 }, { -2101, -1865, 22 } },
    { { 60497, 52426, 16141 }, { -2157, -1945, 25 } },
    { { 52373, 47025, 15974 }, { -2000, -1746, 24 } },
    { { 46742, 41727, 15605 }, { -1968, -1702, 27 } },
    { { 39487, 34757, 15605 }, { -1995, -1776, -67 } },
    { { 27948, 25105, 15135 }, { -1456, -1088, -109 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 148190, 129885, 19440 }, { -1352, -1166, 17 } },
    { { 131682, 113169, 18981 }, { -1336, -1105, 15 } },
    { { 113538, 99917, 18699 }, { -1268, -1046, 17 } },
    { { 99616, 88561, 18362 }, { -1410, -1184, 21 } },
    { { 91683, 80702, 17745 }, { -1287, -1025, 20 } },
    { { 83554, 72420, 17247 }, { -1209, -909, 21 } },
    { { 74705, 65861, 16872 }, { -1066, -762, 19 } },
    { { 67008, 59143, 16433 }, { -709, -466, 24 } },
    { { 61458, 54110, 15806 }, { -450, -239, 27 } },
    { { 54114, 47577, 15819 }, { -134, -22, 28 } },
    { { 48362, 42749, 15605 }, { 19, 34, 34 } },
    { { 40533, 36332, 15226 }, { 45, 38, 37 } },
    { { 29418, 26030, 15605 }, { 48, 50, 47 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 92100, 79978, 20597 }, { -662, -468, 14 } },
    { { 79131, 70724, 20291 }, { -596, -395, 15 } },
    { { 69479, 61514, 19166 }, { -450, -257, 14 } },
    { { 61521, 55369, 18861 }, { -412, -238, 17 } },
    { { 57100, 49707, 18424 }, { -264, -107, 15 } },
    { { 50410, 45748, 18033 }, { -78, -4, 18 } },
    { { 47692, 41862, 16996 }, { -16, 27, 20 } },
    { { 42779, 37352, 16678 }, { 27, 27, 22 } },
    { { 38736, 33833, 16224 }, { 33, 31, 28 } },
    { { 34119, 30733, 15868 }, { 33, 30, 33 } },
    { { 30279, 27237, 15963 }, { 37, 33, 31 } },
    { { 25758, 23141, 15605 }, { 38, 46, 47 } },
    { { 18448, 16271, 15605 }, { 48, 49, 50 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 90042, 80758, 20055 }, { 11, 11, 16 } },
    { { 79420, 70511, 19468 }, { 11, 12, 18 } },
    { { 69466, 62649, 19159 }, { 14, 13, 15 } },
    { { 62782, 54843, 18686 }, { 19, 18, 14 } },
    { { 57108, 50296, 17951 }, { 24, 20, 19 } },
    { { 52612, 46696, 17501 }, { 20, 19, 18 } },
    { { 47885, 42688, 16845 }, { 25, 23, 22 } },
    { { 43202, 37914, 16307 }, { 29, 24, 20 } },
    { { 38633, 34730, 15886 }, { 31, 31, 29 } },
    { { 34779, 31194, 15892 }, { 36, 32, 31 } },
    { { 31364, 27761, 15773 }, { 34, 36, 31 } },
    { { 26671, 23211, 15837 }, { 40, 42, 46 } },
    { { 18770, 17291, 15782 }, { 45, 45, 45 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 95440, 83898, 21153 }, { 9, 8, 14 } },
    { { 83490, 73625, 20595 }, { 11, 10, 11 } },
    { { 72377, 64695, 20020 }, { 14, 12, 14 } },
    { { 63633, 57154, 19213 }, { 16, 17, 16 } },
    { { 57321, 50644, 18795 }, { 16, 19, 17 } },
    { { 53646, 47180, 18016 }, { 22, 18, 17 } },
    { { 49005, 42787, 17418 }, { 23, 24, 18 } },
    { { 44020, 39930, 16572 }, { 28, 27, 22 } },
    { { 39495, 35539, 16434 }, { 30, 30, 26 } },
    { { 35954, 31954, 16296 }, { 36, 36, 27 } },
    { { 31958, 28409, 15893 }, { 34, 33, 27 } },
    { { 26163, 23428, 15605 }, { 45, 42, 55 } },
    { { 19181, 16981, 15770 }, { 48, 42, 49 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
    { { 92443, 81930, 19138 }, { 398, 242, 18 } },
    { { 80847, 71977, 18974 }, { 347, 208, 15 } },
    { { 70960, 63147, 18452 }, { 309, 164, 18 } },
    { { 65421, 57076, 17922 }, { 224, 100, 18 } },
    { { 59238, 51176, 17575 }, { 68, 21, 20 } },
    { { 53110, 47266, 17007 }, { 34, 23, 21 } },
    { { 49637, 44148, 16348 }, { 28, 27, 22 } },
    { { 45031, 39325, 15986 }, { 28, 29, 24 } },
    { { 40712, 35783, 15901 }, { 33, 37, 29 } },
    { { 36865, 32410, 15915 }, { 34, 31, 30 } },
    { { 32560, 28633, 15702 }, { 41, 41, 30 } },
    { { 27043, 23760, 15605 }, { 40, 41, 43 } },
    { { 19456, 17496, 15605 }, { 48, 45, 49 } },
    { { 87366, 43749, 13217 }, { 6, 12, 32 } },
};

const ClubParameters *GetClubParameters(s32 clubIndex) {
    return &sClubParameters[clubIndex];
}

/* Has no callers */
void DoNothingWithClubs(void) {
}

/* Returns the club's reach in the shot mode: in feet for a club usable on the green, or else in yards */
s32 GetShotDistance(Club *club, u32 shotMode) {
    s32 distance = 0;

    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        switch (shotMode) {
            case PUTT_MODE_LONG:
                distance = LONG_PUTT_DISTANCE;
                break;
            case PUTT_MODE_MIDDLE:
                distance = MIDDLE_PUTT_DISTANCE;
                break;
            case PUTT_MODE_SHORT:
                distance = SHORT_PUTT_DISTANCE;
                break;
        }
    } else {
        switch (shotMode) {
            case SHOT_MODE_POWER:
                distance = club->distance * POWER_SHOT_SCALE;
                break;
            case SHOT_MODE_NORMAL:
                distance = club->distance;
                break;
            case SHOT_MODE_APPROACH:
                distance = APPROACH_SHOT_DISTANCE;
                break;
        }
    }
    return distance;
}

/* Returns the club's reach in the shot mode, in world units */
#ifdef NON_MATCHING
s32 GetShotWorldDistance(Club *club, s32 shotMode) {
    s32 distance = GetShotDistance(club, shotMode);
    f32 unitsPerStep = UNITS_PER_YARD;

    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        unitsPerStep = UNITS_PER_FOOT;
    }
    return distance * unitsPerStep;
}
#else
INCLUDE_ASM("asm/nonmatchings/club", GetShotWorldDistance);
#endif

/*
 * Builds the source of sClubParameters and sPutterPowers from the clubs,
 * then hands WriteHostFile the name clubpara.c and the text's size.
 * AppendText is a nested function, a GNU C extension.
 */
#ifdef NON_MATCHING
void WriteClubParameters(void) {
    char line[CLUB_SOURCE_LINE_SIZE];
    char *cursor;
    char *end;
    char *source;
    s32 index;
    s32 step;

    void AppendText(char *text) {
        char character;

        while (TRUE) {
            character = *text++;
            if (character == '\0') {
                break;
            }
            if (cursor < end) {
                *cursor++ = character;
            }
        }
    }

    cursor = source = AllocMemory(CLUB_SOURCE_SIZE);
    end = source + CLUB_SOURCE_SIZE;
    if (source == NULL) {
        ReportFailedAssertion("EX", "c:/usgolf64/club.c", CLUB_ASSERT_LINE);
    }
    sprintf(line, "/* Caution! This file is automatically generated by club.c*/\n"
                  "static const CPARA cp[MAXCLUB] = {\n");
    AppendText(line);
    for (index = 0; index < CLUB_COUNT; index++) {
        sprintf(line, "{{%d, %d, %d}, {%d, %d, %d}},\n", g_Clubs[index].power[0], g_Clubs[index].power[1],
                g_Clubs[index].power[2], g_Clubs[index].deflection[0], g_Clubs[index].deflection[1],
                g_Clubs[index].deflection[2]);
        AppendText(line);
    }
    sprintf(line, "};\nextern CPARA *kGetCparaInfo (LONG clubno) { return &cp[clubno]; }\n");
    AppendText(line);
    AppendText("LONG lkPutterPower[3][30] = {");
    for (index = 0; index < SHOT_MODE_COUNT; index++) {
        AppendText("{");
        for (step = 1; step <= PUTT_STEP_COUNT; step++) {
            sprintf(line, "%d,\n", GetPuttPower((step * PUTT_FRACTION_ONE) / PUTT_STEP_COUNT, index));
            AppendText(line);
        }
        AppendText("},\n");
    }
    AppendText("};\n");
    line[0] = END_OF_FILE_CHARACTER;
    line[1] = '\0';
    AppendText(line);
    sprintf(line, "clubpara.c");
    WriteHostFile(line, cursor - source);
    FreeMemory(&source);
}
#else
INCLUDE_ASM("asm/nonmatchings/club", AppendText);
INCLUDE_ASM("asm/nonmatchings/club", WriteClubParameters);
#endif

/*
 * Test-fires the club in each shot mode until its power carries it the
 * mode's reach, closing in by interpolating between the shortest and longest
 * results, then sets the deflection from where the last test shot landed.
 * InterpolatePower is a nested function, a GNU C extension.
 */
#ifdef NON_MATCHING
void GenerateClub(s32 clubIndex) {
    BallSimulation simulation;
    Club *club;
    s32 shotMode;
    s32 targetDistance;
    s32 distance;
    s32 shortDistance;
    s32 shortPower;
    s32 longDistance;
    s32 longPower;
    s32 tryIndex;
    s32 step;
    f32 error;
    s32 power;

    s32 InterpolatePower(s32 from, s32 to, f32 fraction) {
        return from + (to - from) * (f64)fraction;
    }

    club = &g_Clubs[clubIndex];
    sClubTestShot.clubIndex = clubIndex;
    for (shotMode = 0; shotMode < SHOT_MODE_COUNT; shotMode++) {
        sClubTestShot.shotMode = shotMode;
        /* The target distance is in yards until it is scaled below; the reuse matches */
        targetDistance = GetShotDistance(club, shotMode);
        osSyncPrintf("Generation Club%d PowerMode%d TargetDist %d\n", clubIndex, shotMode, targetDistance);
        club->power[shotMode] = targetDistance * 260.096f;
        shortDistance = 0;
        shortPower = 0;
        longDistance = 0;
        longPower = 0;
        if (club->surfaceMask & CLUB_SURFACE_GREEN) {
            targetDistance = targetDistance * 0x3C00;
            g_ClubTestMode = CLUB_TEST_PUTT;
        } else {
            g_ClubTestMode = CLUB_TEST_SHOT;
            targetDistance = targetDistance * UNITS_PER_YARD;
        }
        osSyncPrintf("Generation Club%d PowerMode%d\n", clubIndex, shotMode);
        for (tryIndex = 0; tryIndex < MAX_CLUB_TRY_COUNT; tryIndex++) {
            simulation.position[0] = 0;
            simulation.position[1] = 0;
            simulation.position[2] = 0;
            func_80037F94(&simulation, 1, &sClubTestShot);
            for (step = 0; step < MAX_TEST_SHOT_STEP_COUNT; step++) {
                if (!simulation.isRunning) {
                    break;
                }
                func_8003327C(&simulation);
            }
            distance = IntLength2D(simulation.position[0], simulation.position[2]);
            error = distance - targetDistance;
            osSyncPrintf("Try %d: dist %fYd:  Error %f%\n", tryIndex + 1, distance * YARDS_PER_UNIT,
                         error / targetDistance * 100.0f);
            if (fabsf(error) < targetDistance * GENERATION_TOLERANCE) {
                break;
            }
            if (distance > targetDistance) {
                longPower = club->power[shotMode];
                longDistance = distance;
                if (longDistance == shortDistance) {
                    break;
                }
                if (shortPower == 0) {
                    power = longPower * 3 / 4;
                } else {
                    power = InterpolatePower(shortPower, longPower,
                                             (f32)(targetDistance - shortDistance) /
                                                 (f32)(longDistance - shortDistance));
                }
            } else {
                shortDistance = distance;
                shortPower = club->power[shotMode];
                if (longDistance == shortDistance) {
                    break;
                }
                if (longPower == 0) {
                    power = shortPower * 3 / 2;
                } else {
                    power = InterpolatePower(shortPower, longPower,
                                             (f32)(targetDistance - shortDistance) /
                                                 (f32)(longDistance - shortDistance));
                }
            }
            club->power[shotMode] = power;
        }
        club->deflection[shotMode] =
            STRAIGHT_ANGLE - (s32)(atan2(simulation.position[2] - simulation.startPosition[2],
                                         simulation.position[0] - simulation.startPosition[0]) *
                                   RADIANS_TO_ANGLE);
    }
    g_ClubTestMode = CLUB_TEST_NONE;
}
#else
INCLUDE_ASM("asm/nonmatchings/club", InterpolatePower);
INCLUDE_ASM("asm/nonmatchings/club", GenerateClub);
#endif

/* Builds the set's clubs from the stats, then generates their parameters */
void GenerateClubSet(s32 setIndex, ClubSetStats *stats) {
    s32 type;
    Club *club;

    for (type = 0; type < CLUB_SET_SIZE; type++) {
        club = GetClub(setIndex * CLUB_SET_SIZE + type);
        club->distance = stats->clubs[type].distance;
        if (club->distance > 400) {
            club->distance = 400;
        }
        osSyncPrintf("TargetDist %d\n", club->distance);
        club->pitch = -(stats->clubs[type].pitch * 360 / 0x10000 + stats->pitch);
        osSyncPrintf("Xang %d\n", club->pitch);
        if (club->pitch > -7) {
            club->pitch = -7;
        }
        if (club->pitch < -45) {
            club->pitch = -45;
        }
        club->unk26 = stats->clubs[type].unk5;
        if (club->unk26 > 100) {
            club->unk26 = 100;
        }
        if (club->unk26 < -100) {
            club->unk26 = -100;
        }
        if ((u8)(stats->unk1E + 4) >= ARRAY_COUNT(sUnk14Values)) {
            club->unk14 = 100;
        } else {
            club->unk14 = sUnk14Values[stats->unk1E + 4];
        }
        club->unk0 = 110 - stats->unk1F * 9;
        club->unk30 = stats->unk1D;
        if (club->unk30 < -4) {
            club->unk30 = -4;
        }
        if (club->unk30 > 4) {
            club->unk30 = 4;
        }
        if (club->unk0 > 145) {
            club->unk0 = 145;
        }
        if (club->unk0 < 70) {
            club->unk0 = 70;
        }
        g_ShowsBillboardsAtEdge = FALSE;
        GenerateClub(setIndex * CLUB_SET_SIZE + type);
    }
}

/*
 * Fills in the stock sets' parameters from sClubParameters; with
 * GAME_FLAG_GENERATE_CLUBS set, generates every club's instead and passes
 * the results to WriteClubParameters. GetStockUnk30 is a nested function, a
 * GNU C extension.
 */
void InitClubs(void) {
    s32 clubIndex;
    ClubParameters *parameters;
    Club *club;

    s8 GetStockUnk30(s32 clubIndex) {
        u32 setIndex = clubIndex / CLUB_SET_SIZE;

        if (setIndex >= CLUB_SET_COUNT) {
            setIndex = 0;
        }
        return sStockUnk30Values[setIndex];
    }

    if (!IsGameFlagSet(GAME_FLAG_GENERATE_CLUBS)) {
        for (clubIndex = 0; clubIndex < STOCK_CLUB_SET_COUNT * CLUB_SET_SIZE; clubIndex++) {
            club = &g_Clubs[clubIndex];
            /* Casting away const matches: GCC moves loads through const pointers ahead of stores */
            parameters = (ClubParameters *)GetClubParameters(clubIndex);
            club->power[0] = parameters->power[0];
            club->power[1] = parameters->power[1];
            club->power[2] = parameters->power[2];
            club->deflection[0] = parameters->deflection[0];
            club->deflection[1] = parameters->deflection[1];
            club->deflection[2] = parameters->deflection[2];
            club->unk30 = GetStockUnk30(clubIndex);
        }
    } else if (IsGameFlagSet(GAME_FLAG_GENERATE_CLUBS)) {
        for (clubIndex = 0; clubIndex < CLUB_COUNT; clubIndex++) {
            GenerateClub(clubIndex);
        }
        WriteClubParameters();
    }
}

/*
 * Returns the putter power for a putt of the fraction, out of
 * PUTT_FRACTION_ONE, of the shot mode's reach. With GAME_FLAG_GENERATE_CLUBS
 * set, it is found by test-firing the first set's putter instead.
 * InterpolatePuttPower is a nested function, a GNU C extension.
 */
#ifdef NON_MATCHING
s32 GetPuttPower(s32 fraction, s32 shotMode) {
    BallSimulation simulation;
    Club *putter;
    s32 targetDistance;
    s32 distance;
    s32 shortDistance;
    s32 shortPower;
    s32 longDistance;
    s32 longPower;
    s32 tryIndex;
    s32 step;
    s32 index;
    f32 reach;
    f32 error;
    s32 power;

    s32 InterpolatePuttPower(s32 from, s32 to, f32 fraction) {
        return from + (to - from) * (f64)fraction;
    }

    if (!IsGameFlagSet(GAME_FLAG_GENERATE_CLUBS)) {
        index = fraction * PUTT_STEP_COUNT / PUTT_FRACTION_ONE;
        if (index > PUTT_STEP_COUNT) {
            index = PUTT_STEP_COUNT;
        }
        if (index <= 0) {
            index = 1;
        }
        index--;
        return sPutterPowers[shotMode][index];
    }
    sPutterTestShot.clubIndex = CLUB_TYPE_PUTTER;
    sPutterTestShot.shotMode = shotMode;
    reach = GetShotDistance(&g_Clubs[CLUB_TYPE_PUTTER], shotMode);
    putter = &g_Clubs[CLUB_TYPE_PUTTER];
    shortDistance = 0;
    shortPower = 0;
    longDistance = 0;
    longPower = 0;
    tryIndex = 0;
    g_ClubTestMode = CLUB_TEST_PUTT;
    targetDistance = (s32)(reach * (UNITS_PER_FOOT / PUTT_FRACTION_ONE) * fraction) + PUTT_OVERSHOOT;
    putter->power[shotMode] = (s32)(reach * 130.048f) / 3;
    for (; tryIndex < MAX_PUTT_TRY_COUNT; tryIndex++) {
        simulation.position[0] = 0;
        simulation.position[1] = 0;
        simulation.position[2] = 0;
        simulation.surface = PUTT_TEST_SURFACE;
        func_80037F94(&simulation, 1, &sPutterTestShot);
        for (step = 0; step < MAX_TEST_SHOT_STEP_COUNT; step++) {
            if (!simulation.isRunning) {
                break;
            }
            func_8003327C(&simulation);
        }
        distance = IntLength2D(simulation.position[0], simulation.position[2]);
        error = fabsf(distance - targetDistance);
        if (error < targetDistance * GENERATION_TOLERANCE) {
            break;
        }
        if (distance > targetDistance) {
            longPower = putter->power[shotMode];
            longDistance = distance;
            if (longDistance == shortDistance) {
                break;
            }
            if (shortPower == 0) {
                power = longPower * 3 / 4;
            } else {
                power = InterpolatePuttPower(shortPower, longPower,
                                             (f32)(targetDistance - shortDistance) /
                                                 (f32)(longDistance - shortDistance));
            }
        } else {
            shortDistance = distance;
            shortPower = putter->power[shotMode];
            if (longDistance == shortDistance) {
                break;
            }
            if (longPower == 0) {
                power = shortPower * 3 / 2;
            } else {
                power = InterpolatePuttPower(shortPower, longPower,
                                             (f32)(targetDistance - shortDistance) /
                                                 (f32)(longDistance - shortDistance));
            }
        }
        putter->power[shotMode] = power;
    }
    g_ClubTestMode = CLUB_TEST_NONE;
    return putter->power[shotMode];
}
#else
INCLUDE_ASM("asm/nonmatchings/club", InterpolatePuttPower);
INCLUDE_ASM("asm/nonmatchings/club", GetPuttPower);
#endif

s32 GetClubPower(Club *club, s32 shotMode) {
    return club->power[shotMode];
}

Club *GetClub(u32 clubIndex) {
    return (clubIndex < CLUB_COUNT) ? &g_Clubs[clubIndex] : &sNoClub;
}

/* Returns the club in the current player's selected slot */
Club *GetSelectedClub(void) {
    return &g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]];
}

s32 GetSelectedShotDistance(void) {
    return GetShotDistance(GetSelectedClub(), g_CurrentShot.shotMode);
}

s32 GetSelectedShotWorldDistance(void) {
    return GetShotWorldDistance(GetSelectedClub(), g_CurrentShot.shotMode);
}

/*
 * Returns the surface mask bits of the clubs usable on the surface. Unless
 * distinguishesKinds is set, the green is the only kind told apart.
 */
#ifdef NON_MATCHING
s32 GetSurfaceClubMask(s32 distinguishesKinds, SurfaceInfo *info) {
    s32 kind = info->kind;

    if (IsGameFlagSet(GAME_FLAG_ANY_CLUB_ANYWHERE)) {
        return CLUB_SURFACE_ALL;
    }
    if (g_ShowsBillboardsAtEdge) {
        return CLUB_SURFACE_GREEN;
    }
    if (!distinguishesKinds) {
        return (kind != SURFACE_KIND_GREEN) ? CLUB_SURFACE_ALL : CLUB_SURFACE_GREEN;
    }
    switch (kind) {
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_WASTE_AREA:
            return CLUB_SURFACE_ROUGH;
        case SURFACE_KIND_GREEN:
            return CLUB_SURFACE_GREEN;
        case SURFACE_KIND_ROUGH_POOR_LIE:
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
            return CLUB_SURFACE_BUNKER;
        case SURFACE_KIND_TEE_GROUND:
            return CLUB_SURFACE_TEE_GROUND;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
            return CLUB_SURFACE_FAIRWAY;
        case SURFACE_KIND_SEMI_ROUGH:
            return CLUB_SURFACE_SEMI_ROUGH;
    }
    return CLUB_SURFACE_ALL;
}
#else
INCLUDE_ASM("asm/nonmatchings/club", GetSurfaceClubMask);
#endif

s32 GetCurrentSurfaceClubMask(s32 distinguishesKinds) {
    return GetSurfaceClubMask(distinguishesKinds, GetSurfaceInfo(g_CurrentShot.surface));
}

/*
 * Lie ratings, as one digit for each club type from the 1W to the SW; the
 * putter is not rated. Several surface kinds share each group's ratings.
 */
#define LIE_RATING_LENGTH CLUB_TYPE_PUTTER
static const char sTeeUpLieRatings[LIE_RATING_LENGTH + 1] = "3444444555555";
static const char sTeeUpApproachLieRatings[LIE_RATING_LENGTH + 1] = "5555555555555";
static const char sFairwayLieRatings[LIE_RATING_LENGTH + 1] = "3334444445555";
static const char sFairwayApproachLieRatings[LIE_RATING_LENGTH + 1] = "4444555555555";
static const char sFairwayFairLieLieRatings[LIE_RATING_LENGTH + 1] = "2333344444455";
static const char sFairwayFairLieApproachLieRatings[LIE_RATING_LENGTH + 1] = "3344445555555";
static const char sFairwayPoorLieLieRatings[LIE_RATING_LENGTH + 1] = "1222333344445";
static const char sFairwayPoorLieApproachLieRatings[LIE_RATING_LENGTH + 1] = "2333334444455";
static const char sRoughLieRatings[LIE_RATING_LENGTH + 1] = "1223333444455";
static const char sRoughApproachLieRatings[LIE_RATING_LENGTH + 1] = "2233344445555";
static const char sRoughPoorLieLieRatings[LIE_RATING_LENGTH + 1] = "1112223334445";
static const char sRoughPoorLieApproachLieRatings[LIE_RATING_LENGTH + 1] = "2222333444455";
static const char sDeepRoughLieRatings[LIE_RATING_LENGTH + 1] = "1111222233334";
static const char sDeepRoughApproachLieRatings[LIE_RATING_LENGTH + 1] = "1112222333344";
static const char sBunkerLieRatings[LIE_RATING_LENGTH + 1] = "1112223333445";
static const char sBunkerApproachLieRatings[LIE_RATING_LENGTH + 1] = "2223333444455";
static const char sBunkerFairLieLieRatings[LIE_RATING_LENGTH + 1] = "1111122222334";
static const char sBunkerFairLieApproachLieRatings[LIE_RATING_LENGTH + 1] = "1112223333444";
static const char sBunkerPoorLieLieRatings[LIE_RATING_LENGTH + 1] = "1111111222223";
static const char sBunkerPoorLieApproachLieRatings[LIE_RATING_LENGTH + 1] = "1111112223334";
static const char sCartWayLieRatings[LIE_RATING_LENGTH + 1] = "1112223333344";
static const char sCartWayApproachLieRatings[LIE_RATING_LENGTH + 1] = "2223333333444";
static const char sDefaultLieRatings[LIE_RATING_LENGTH + 1] = "3333333333333";

/*
 * Rates how well the club plays from the surface, from 1 to MAX_LIE_RATING,
 * scaled by the club's unk14 as a percentage; the putter rates 0. Each kind
 * of surface has a digit for each club type, with its own digits for an
 * approach shot.
 */
s32 GetLieRating(Club *club, s32 surface, s32 shotMode) {
    const char *ratings;
    const char *approachRatings;
    s32 rating;

    switch (GetSurfaceInfo(surface)->kind) {
        case SURFACE_KIND_TEE_UP:
            ratings = sTeeUpLieRatings;
            approachRatings = sTeeUpApproachLieRatings;
            break;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_GREEN_EDGE:
        case SURFACE_KIND_TEE_GROUND:
            ratings = sFairwayLieRatings;
            approachRatings = sFairwayApproachLieRatings;
            break;
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
            ratings = sFairwayFairLieLieRatings;
            approachRatings = sFairwayFairLieApproachLieRatings;
            break;
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
            ratings = sFairwayPoorLieLieRatings;
            approachRatings = sFairwayPoorLieApproachLieRatings;
            break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_BARE_GROUND:
            ratings = sRoughLieRatings;
            approachRatings = sRoughApproachLieRatings;
            break;
        case SURFACE_KIND_ROUGH_POOR_LIE:
            ratings = sRoughPoorLieLieRatings;
            approachRatings = sRoughPoorLieApproachLieRatings;
            break;
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_WASTE_AREA:
            ratings = sDeepRoughLieRatings;
            approachRatings = sDeepRoughApproachLieRatings;
            break;
        case SURFACE_KIND_BUNKER:
            ratings = sBunkerLieRatings;
            approachRatings = sBunkerApproachLieRatings;
            break;
        case SURFACE_KIND_BUNKER_FAIR_LIE:
            ratings = sBunkerFairLieLieRatings;
            approachRatings = sBunkerFairLieApproachLieRatings;
            break;
        case SURFACE_KIND_BUNKER_POOR_LIE:
            ratings = sBunkerPoorLieLieRatings;
            approachRatings = sBunkerPoorLieApproachLieRatings;
            break;
        case SURFACE_KIND_CART_WAY:
            ratings = sCartWayLieRatings;
            approachRatings = sCartWayApproachLieRatings;
            break;
        default:
            ratings = sDefaultLieRatings;
            approachRatings = ratings;
            break;
    }
    if (club->type != CLUB_TYPE_PUTTER) {
        if (shotMode == SHOT_MODE_APPROACH) {
            rating = approachRatings[club->type] - '0';
        } else {
            rating = ratings[club->type] - '0';
        }
        rating = (rating * club->unk14 + 50) / 100;
        if (rating <= 0) {
            rating = 1;
        }
        if (rating > MAX_LIE_RATING) {
            rating = MAX_LIE_RATING;
        }
    } else {
        rating = 0;
    }
    return rating;
}

/* Sets span[0] to 2 * rating - 1 and span[1] to rating - 1; both are 0 for the putter */
void GetLieSpan(Club *club, s32 surface, s32 *span, s32 shotMode) {
    s32 rating = GetLieRating(club, surface, shotMode);

    if (rating > 0) {
        span[0] = rating * 2 - 1;
        span[1] = rating - 1;
    } else {
        span[0] = 0;
        span[1] = 0;
    }
}

/*
 * Moves the selected shot mode, then the selected club, a step towards the
 * choice every few frames, with a sound for each step. Returns TRUE once the
 * choice is selected.
 */
#ifdef NON_MATCHING
s32 StepToClubChoice(ClubChoice *choice) {
    s32 hasStepped = FALSE;
    s32 slot;

    if (g_CurrentShot.shotMode == choice->shotMode && g_CurrentShot.clubSlot == choice->clubSlot) {
        return TRUE;
    }
    if (sClubChoiceDelay > CLUB_CHOICE_STEP_DELAY) {
        sClubChoiceDelay = CLUB_CHOICE_STEP_DELAY;
    }
    if (sClubChoiceDelay != 0) {
        sClubChoiceDelay--;
        return FALSE;
    }
    sClubChoiceDelay = CLUB_CHOICE_STEP_DELAY;
    if (hasStepped) {
        return !hasStepped;
    }
    if (g_CurrentShot.shotMode != choice->shotMode) {
        PlaySound(CLUB_CHANGE_SOUND, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
        hasStepped = TRUE;
        g_CurrentShot.shotMode = choice->shotMode;
    }
    if (hasStepped) {
        return !hasStepped;
    }
    if (g_CurrentShot.clubSlot < choice->clubSlot) {
        PlaySound(CLUB_CHANGE_SOUND, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
        for (slot = g_CurrentShot.clubSlot + 1; slot < CLUB_SLOT_COUNT; slot++) {
            if (g_Players[g_CurrentShot.playerIndex].clubs[slot] != NO_CLUB &&
                (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[slot]].surfaceMask &
                 GetCurrentSurfaceClubMask(FALSE))) {
                g_CurrentShot.clubSlot = slot;
                hasStepped = TRUE;
                break;
            }
        }
        if (!hasStepped || choice->clubSlot < g_CurrentShot.clubSlot) {
            g_CurrentShot.clubSlot = choice->clubSlot;
        }
    } else if (choice->clubSlot < g_CurrentShot.clubSlot) {
        PlaySound(CLUB_CHANGE_SOUND, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
        for (slot = g_CurrentShot.clubSlot - 1; slot >= 0; slot--) {
            if (g_Players[g_CurrentShot.playerIndex].clubs[slot] != NO_CLUB &&
                (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[slot]].surfaceMask &
                 GetCurrentSurfaceClubMask(FALSE))) {
                g_CurrentShot.clubSlot = slot;
                hasStepped = TRUE;
                break;
            }
        }
        if (!hasStepped || g_CurrentShot.clubSlot < choice->clubSlot) {
            g_CurrentShot.clubSlot = choice->clubSlot;
        }
    }
    return !hasStepped;
}
#else
INCLUDE_ASM("asm/nonmatchings/club", StepToClubChoice);
#endif

/* With the control pad, down selects the next club in the bag usable on the surface, and up the one before */
void HandleClubInput(void) {
    s32 slot;
    u16 buttons;

    if (func_8021EA48()) {
        buttons = g_MenuControllerData[g_CurrentShot.controllerIndex].button;
        if (buttons & D_JPAD) {
            for (slot = g_CurrentShot.clubSlot + 1; slot < CLUB_SLOT_COUNT; slot++) {
                if (g_Players[g_CurrentShot.playerIndex].clubs[slot] != NO_CLUB &&
                    (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[slot]].surfaceMask &
                     GetCurrentSurfaceClubMask(FALSE))) {
                    g_CurrentShot.clubSlot = slot;
                    PlaySound(CLUB_CHANGE_SOUND, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
                    return;
                }
            }
        } else if (buttons & U_JPAD) {
            for (slot = g_CurrentShot.clubSlot - 1; slot >= 0; slot--) {
                if (g_Players[g_CurrentShot.playerIndex].clubs[slot] != NO_CLUB &&
                    (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[slot]].surfaceMask &
                     GetCurrentSurfaceClubMask(FALSE))) {
                    g_CurrentShot.clubSlot = slot;
                    PlaySound(CLUB_CHANGE_SOUND, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
                    return;
                }
            }
        }
    }
}

/*
 * B changes the shot mode. With a club usable on the green it goes from a
 * middle putt to a long putt to a short putt and back. With other clubs,
 * further than APPROACH_SHOT_DISTANCE yards from the cup, it switches between
 * normal and power, though not to power while unk54 is clear; closer, between
 * normal and approach. Modes that don't fit the distance are then put back to
 * normal.
 */
#ifdef NON_MATCHING
void HandleShotModeInput(void) {
    Club *club = GetSelectedClub();
    s32 cupDistance = func_8003DFD0(g_CurrentShot.ballPosition[0] - D_800BA9D8.cupPosition[0],
                                    g_CurrentShot.ballPosition[2] - D_800BA9D8.cupPosition[2]) *
                      YARDS_PER_UNIT;
    s32 sound;

    if ((g_MenuControllerData[g_CurrentShot.controllerIndex].button & B_BUTTON) && func_8021EA48()) {
        if (club->surfaceMask & CLUB_SURFACE_GREEN) {
            if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
                g_CurrentShot.shotMode = PUTT_MODE_LONG;
            } else if (g_CurrentShot.shotMode == PUTT_MODE_LONG) {
                g_CurrentShot.shotMode = PUTT_MODE_SHORT;
            } else {
                g_CurrentShot.shotMode = PUTT_MODE_MIDDLE;
            }
            sound = CLUB_CHANGE_SOUND;
            PlaySound(sound, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
        } else if (cupDistance > APPROACH_SHOT_DISTANCE) {
            if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                sound = POWER_SHOT_REFUSED_SOUND;
                if (g_CurrentShot.unk54) {
                    g_CurrentShot.shotMode = SHOT_MODE_POWER;
                    sound = CLUB_CHANGE_SOUND;
                }
            } else {
                g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
                sound = CLUB_CHANGE_SOUND;
            }
            PlaySound(sound, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
        } else {
            PlaySound(CLUB_CHANGE_SOUND, CLUB_SOUND_CHANNEL, CLUB_SOUND_VOLUME);
            if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                g_CurrentShot.shotMode = SHOT_MODE_APPROACH;
            } else {
                g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
            }
        }
    }
    if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
        if ((g_CurrentShot.shotMode == SHOT_MODE_POWER) & (cupDistance <= APPROACH_SHOT_DISTANCE)) {
            g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
        }
        if ((g_CurrentShot.shotMode == SHOT_MODE_APPROACH) & !(cupDistance <= APPROACH_SHOT_DISTANCE)) {
            g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
        }
        if (g_CurrentShot.shotMode == SHOT_MODE_POWER && !g_CurrentShot.unk54) {
            g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
        }
    }
}
#else
INCLUDE_ASM("asm/nonmatchings/club", HandleShotModeInput);
#endif
