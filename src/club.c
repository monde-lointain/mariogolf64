#include "club.h"

#include <math.h>
#include <ultra64.h>

#include "asset.h"
#include "ball_simulation.h"
#include "c_language.h"
#include "club.h"
#include "club_panel.h"
#include "controller.h"
#include "course_space.h"
#include "current_shot.h"
#include "game_flags.h"
#include "game_state.h"
#include "geometry.h"
#include "heap.h"
#include "hole_setup.h"
#include "host_file.h"
#include "main.h"
#include "math_util.h"
#include "pin.h"
#include "player.h"
#include "sound.h"
#include "surface.h"
#include "transfer_pak.h"

#define STRAIGHT_ANGLE 0x1000

#define POWER_SHOT_SCALE 1.1f
#define APPROACH_SHOT_DISTANCE 60
#define LONG_PUTT_DISTANCE 200
#define MIDDLE_PUTT_DISTANCE 100
#define SHORT_PUTT_DISTANCE 30

#define GENERATION_TOLERANCE 0.01f
#define CLUB_TRY_COUNT_MAX 128
#define PUTT_TRY_COUNT_MAX 64
#define TEST_SHOT_STEP_COUNT_MAX 900

#define STOCK_CLUB_SET_COUNT 14

#define PUTT_STEP_COUNT 30
#define PUTT_OVERSHOOT 0x600
#define PUTT_POWER_ESTIMATE_SCALE 130.048f
#define PUTT_POWER_ESTIMATE_DIVISOR 3
#define PUTT_TEST_SURFACE 4

#define CLUB_SOURCE_SIZE 0x8000
#define CLUB_SOURCE_LINE_SIZE 256

#define CLUB_ASSERT_LINE 543

#define CLUB_CHOICE_STEP_DELAY 3

#define CLUB_SOUND_CHANNEL 0xF

#define LIE_RATING_LENGTH CLUB_TYPE_PUTTER

#define CLUB_ENTRY( \
    type, \
    name, \
    category, \
    surfaceMask, \
    mishitScale, \
    distance, \
    pitch, \
    offsetPitch, \
    spin, \
    lieRatingPercent, \
    sideSpin \
) \
    {(mishitScale), \
        (name), \
        (surfaceMask), \
        (distance), \
        (pitch), \
        (offsetPitch), \
        (spin), \
        (lieRatingPercent), \
        (type), \
        {0, 0, 0}, \
        (category), \
        (sideSpin), \
        {0, 0, 0}, \
        ASSET_DRIVER_HEAD_IMAGE + (type), \
        0}

/* clang-format off */
static s32 f_PutterPowers[SHOT_MODE_COUNT][PUTT_STEP_COUNT] = {
    {
          966,  1914,  2742,  3656,  4520,  5457,  6288,  7224,  8056,  8992,
         9823, 10758, 11590, 12525, 13460, 14293, 15230, 16062, 16999, 17832,
        18768, 19504, 20535, 21366, 22300, 23131, 24065, 24896, 25830, 26765,
    },
    {
          540,  1027,  1442,  1915,  2334,  2804,  3250,  3689,  4105,  4573,
         4988,  5456,  5872,  6340,  6808,  7224,  7692,  8108,  8576,  8992,
         9460,  9876, 10344, 10761, 11229, 11645, 12114, 12530, 12998, 13466,
    },
    {
          231,   382,   513,   659,   787,   929,  1055,  1197,  1323,  1465,
         1592,  1734,  1861,  2003,  2144,  2269,  2410,  2535,  2676,  2802,
         2925,  3068,  3208,  3333,  3474,  3599,  3739,  3864,  4005,  4145,
    },
};
/* clang-format on */
Club g_Clubs[CLUB_COUNT] = {
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        80,
        208,
        -11,
        1,
        11,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        80,
        194,
        -12,
        1,
        14,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        80,
        180,
        -13,
        1,
        17,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        165,
        -14,
        1,
        24,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        156,
        -16,
        1,
        26,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        147,
        -18,
        2,
        28,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        137,
        -19,
        2,
        40,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        128,
        -20,
        3,
        46,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        118,
        -21,
        3,
        57,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        109,
        -22,
        3,
        62,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        99,
        -23,
        4,
        73,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        80,
        85,
        -34,
        4,
        94,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        80,
        66,
        -38,
        5,
        106,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        80,
        30,
        0,
        10,
        10,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        85,
        210,
        -11,
        1,
        8,
        150,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        85,
        195,
        -12,
        1,
        10,
        150,
        -9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        85,
        180,
        -13,
        1,
        12,
        150,
        -8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        170,
        -14,
        3,
        17,
        150,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        160,
        -16,
        3,
        19,
        150,
        -6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        150,
        -18,
        4,
        20,
        150,
        -5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        140,
        -19,
        4,
        29,
        150,
        -4
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        130,
        -20,
        5,
        33,
        150,
        -3
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        120,
        -21,
        5,
        41,
        150,
        -3
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        110,
        -22,
        7,
        45,
        150,
        -2
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        100,
        -23,
        9,
        53,
        150,
        -2
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        85,
        85,
        -34,
        11,
        68,
        150,
        -1
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        85,
        65,
        -38,
        12,
        77,
        150,
        -1
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        85,
        30,
        0,
        10,
        10,
        150,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        115,
        245,
        -15,
        1,
        10,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        115,
        228,
        -16,
        1,
        13,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        115,
        212,
        -17,
        1,
        16,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        195,
        -18,
        1,
        22,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        184,
        -20,
        1,
        24,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        173,
        -22,
        2,
        26,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        161,
        -23,
        2,
        37,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        150,
        -24,
        3,
        42,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        139,
        -25,
        3,
        52,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        128,
        -26,
        3,
        57,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        115,
        117,
        -27,
        4,
        67,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        115,
        100,
        -38,
        4,
        86,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        115,
        78,
        -42,
        5,
        97,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        115,
        30,
        0,
        10,
        10,
        70,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        95,
        215,
        -17,
        1,
        9,
        120,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        95,
        200,
        -18,
        1,
        11,
        120,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        95,
        186,
        -19,
        1,
        14,
        120,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        171,
        -20,
        1,
        19,
        120,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        161,
        -22,
        1,
        21,
        120,
        7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        151,
        -24,
        2,
        23,
        120,
        6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        142,
        -25,
        2,
        33,
        120,
        5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        132,
        -26,
        3,
        37,
        120,
        4
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        122,
        -27,
        3,
        46,
        120,
        4
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        112,
        -28,
        3,
        51,
        120,
        3
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        103,
        -29,
        4,
        60,
        120,
        2
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        95,
        88,
        -40,
        4,
        77,
        120,
        1
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        95,
        68,
        -44,
        5,
        87,
        120,
        1
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        95,
        30,
        0,
        10,
        10,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        100,
        220,
        -9,
        1,
        13,
        100,
        -15
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        100,
        205,
        -10,
        1,
        16,
        100,
        -14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        100,
        190,
        -11,
        1,
        20,
        100,
        -13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        175,
        -12,
        3,
        28,
        100,
        -12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        165,
        -14,
        3,
        31,
        100,
        -11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        155,
        -16,
        4,
        33,
        100,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        145,
        -17,
        4,
        48,
        100,
        -9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        135,
        -18,
        5,
        54,
        100,
        -8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        125,
        -19,
        5,
        67,
        100,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        115,
        -20,
        7,
        74,
        100,
        -6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        100,
        105,
        -21,
        7,
        87,
        100,
        -5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        100,
        90,
        -32,
        7,
        111,
        100,
        -5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        100,
        70,
        -36,
        7,
        126,
        100,
        -5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        100,
        30,
        0,
        10,
        10,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        90,
        212,
        -14,
        1,
        9,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        90,
        198,
        -15,
        1,
        11,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        90,
        183,
        -16,
        1,
        14,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        169,
        -17,
        1,
        19,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        159,
        -19,
        1,
        21,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        149,
        -21,
        2,
        23,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        140,
        -22,
        2,
        33,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        130,
        -23,
        3,
        37,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        120,
        -24,
        3,
        46,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        111,
        -25,
        3,
        51,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        101,
        -26,
        4,
        60,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        90,
        87,
        -37,
        4,
        77,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        90,
        67,
        -41,
        5,
        87,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        90,
        30,
        0,
        10,
        10,
        120,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        110,
        240,
        -17,
        1,
        6,
        100,
        15
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        110,
        223,
        -18,
        1,
        7,
        100,
        15
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        110,
        207,
        -19,
        1,
        9,
        100,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        191,
        -20,
        1,
        13,
        100,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        180,
        -22,
        1,
        14,
        100,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        169,
        -24,
        2,
        15,
        100,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        158,
        -25,
        2,
        22,
        100,
        11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        147,
        -26,
        3,
        25,
        100,
        9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        136,
        -27,
        3,
        31,
        100,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        125,
        -28,
        3,
        34,
        100,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        110,
        114,
        -29,
        4,
        40,
        100,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        110,
        98,
        -40,
        4,
        51,
        100,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        110,
        76,
        -44,
        5,
        58,
        100,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        110,
        30,
        0,
        10,
        10,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        120,
        250,
        -10,
        1,
        11,
        80,
        -18
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        120,
        233,
        -11,
        1,
        14,
        80,
        -17
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        120,
        216,
        -12,
        1,
        17,
        80,
        -16
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        199,
        -13,
        1,
        24,
        80,
        -14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        187,
        -15,
        1,
        26,
        80,
        -13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        176,
        -17,
        2,
        28,
        80,
        -12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        165,
        -18,
        2,
        40,
        80,
        -11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        153,
        -19,
        3,
        46,
        80,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        142,
        -20,
        3,
        57,
        80,
        -9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        131,
        -21,
        3,
        62,
        80,
        -8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        120,
        119,
        -22,
        4,
        73,
        80,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        120,
        102,
        -33,
        4,
        94,
        80,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        120,
        80,
        -37,
        5,
        106,
        80,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        120,
        30,
        0,
        10,
        10,
        80,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        125,
        260,
        -16,
        1,
        5,
        80,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        125,
        242,
        -17,
        1,
        6,
        80,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        125,
        224,
        -18,
        1,
        8,
        80,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        207,
        -19,
        1,
        13,
        80,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        195,
        -21,
        1,
        14,
        80,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        183,
        -23,
        2,
        15,
        80,
        11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        171,
        -24,
        2,
        22,
        80,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        159,
        -25,
        3,
        25,
        80,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        148,
        -26,
        3,
        31,
        80,
        9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        136,
        -27,
        3,
        34,
        80,
        9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        125,
        124,
        -28,
        4,
        40,
        80,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        125,
        106,
        -39,
        4,
        51,
        80,
        7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        125,
        83,
        -43,
        5,
        58,
        80,
        7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        125,
        30,
        0,
        10,
        10,
        80,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        130,
        270,
        -15,
        1,
        8,
        50,
        15
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        130,
        252,
        -16,
        1,
        10,
        50,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        130,
        233,
        -17,
        1,
        12,
        50,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        215,
        -18,
        1,
        17,
        50,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        202,
        -20,
        1,
        19,
        50,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        190,
        -22,
        2,
        20,
        50,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        178,
        -23,
        2,
        29,
        50,
        11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        166,
        -24,
        3,
        33,
        50,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        153,
        -25,
        3,
        41,
        50,
        9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        141,
        -26,
        3,
        45,
        50,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        129,
        -27,
        4,
        53,
        50,
        7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        130,
        110,
        -38,
        4,
        68,
        50,
        6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        130,
        86,
        -42,
        5,
        77,
        50,
        5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        130,
        30,
        0,
        10,
        10,
        50,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        105,
        230,
        -13,
        1,
        10,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        105,
        214,
        -14,
        1,
        13,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        105,
        199,
        -15,
        1,
        16,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        183,
        -16,
        1,
        22,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        172,
        -18,
        1,
        24,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        162,
        -20,
        2,
        26,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        152,
        -21,
        2,
        37,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        141,
        -22,
        3,
        42,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        131,
        -23,
        3,
        52,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        120,
        -24,
        3,
        57,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        105,
        110,
        -25,
        4,
        67,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        105,
        94,
        -36,
        4,
        86,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        105,
        73,
        -40,
        5,
        97,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        105,
        30,
        0,
        10,
        10,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        135,
        275,
        -11,
        1,
        5,
        50,
        -25
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        135,
        256,
        -12,
        1,
        6,
        50,
        -23
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        135,
        238,
        -13,
        1,
        8,
        50,
        -21
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        219,
        -14,
        1,
        11,
        50,
        -19
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        206,
        -16,
        1,
        12,
        50,
        -16
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        194,
        -18,
        2,
        13,
        50,
        -14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        181,
        -19,
        2,
        18,
        50,
        -12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        169,
        -20,
        3,
        21,
        50,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        156,
        -21,
        3,
        26,
        50,
        -8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        144,
        -22,
        3,
        28,
        50,
        -6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        135,
        131,
        -23,
        4,
        33,
        50,
        -4
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        135,
        113,
        -34,
        4,
        43,
        50,
        -2
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        135,
        88,
        -38,
        5,
        48,
        50,
        -1
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        135,
        30,
        0,
        10,
        10,
        50,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        140,
        280,
        -14,
        1,
        7,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        140,
        261,
        -15,
        1,
        9,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        140,
        242,
        -16,
        1,
        11,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        223,
        -17,
        1,
        15,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        210,
        -19,
        1,
        16,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        197,
        -21,
        2,
        18,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        184,
        -22,
        2,
        25,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        172,
        -23,
        3,
        29,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        159,
        -24,
        3,
        36,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        146,
        -25,
        3,
        39,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        140,
        134,
        -26,
        4,
        46,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        140,
        114,
        -37,
        4,
        60,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        140,
        89,
        -41,
        5,
        67,
        50,
        20
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        140,
        30,
        0,
        10,
        10,
        50,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        130,
        285,
        -15,
        6,
        8,
        50,
        15
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        130,
        265,
        -16,
        6,
        10,
        50,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        130,
        246,
        -17,
        6,
        12,
        50,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        227,
        -18,
        6,
        17,
        50,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        214,
        -20,
        6,
        19,
        50,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        201,
        -22,
        7,
        20,
        50,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        188,
        -23,
        7,
        29,
        50,
        11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        175,
        -24,
        8,
        33,
        50,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        162,
        -25,
        8,
        41,
        50,
        9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        149,
        -26,
        8,
        45,
        50,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        130,
        136,
        -27,
        9,
        53,
        50,
        7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        130,
        117,
        -38,
        9,
        68,
        50,
        6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        130,
        91,
        -42,
        10,
        77,
        50,
        5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        130,
        30,
        0,
        10,
        10,
        50,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        85,
        205,
        -11,
        1,
        10,
        110,
        15
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        85,
        190,
        -12,
        1,
        13,
        110,
        14
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        85,
        175,
        -13,
        1,
        16,
        110,
        13
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        165,
        -14,
        1,
        22,
        110,
        12
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        155,
        -16,
        1,
        24,
        110,
        11
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        145,
        -18,
        2,
        26,
        110,
        10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        135,
        -19,
        2,
        37,
        110,
        9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        125,
        -20,
        3,
        42,
        110,
        8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        115,
        -21,
        3,
        52,
        110,
        7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        105,
        -22,
        3,
        57,
        110,
        6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        85,
        95,
        -23,
        4,
        67,
        110,
        4
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        85,
        82,
        -34,
        4,
        86,
        110,
        2
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        85,
        62,
        -38,
        5,
        97,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        85,
        30,
        0,
        10,
        10,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        80,
        207,
        -13,
        1,
        10,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        80,
        192,
        -14,
        1,
        13,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        80,
        177,
        -15,
        1,
        16,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        167,
        -16,
        1,
        22,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        157,
        -18,
        1,
        24,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        147,
        -20,
        2,
        26,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        137,
        -21,
        2,
        37,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        127,
        -22,
        3,
        42,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        117,
        -23,
        3,
        52,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        107,
        -24,
        3,
        57,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        80,
        97,
        -25,
        4,
        67,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        80,
        83,
        -36,
        4,
        86,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        80,
        63,
        -40,
        5,
        97,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        80,
        30,
        0,
        10,
        10,
        110,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        90,
        210,
        -10,
        1,
        10,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        90,
        195,
        -11,
        1,
        13,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        90,
        180,
        -12,
        1,
        16,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        168,
        -13,
        1,
        22,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        158,
        -15,
        1,
        24,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        149,
        -17,
        2,
        26,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        139,
        -18,
        2,
        37,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        129,
        -19,
        3,
        42,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        119,
        -20,
        3,
        52,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        109,
        -21,
        3,
        57,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        90,
        99,
        -22,
        4,
        67,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        90,
        84,
        -33,
        4,
        86,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        90,
        64,
        -37,
        5,
        97,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        90,
        30,
        0,
        10,
        10,
        100,
        0
    ),
    CLUB_ENTRY(
        CLUB_TYPE_DRIVER,
        "1W",
        CLUB_CATEGORY_WOOD,
        DRIVER_SURFACES,
        95,
        210,
        -15,
        1,
        10,
        100,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_WOOD,
        "3W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        95,
        195,
        -16,
        1,
        13,
        100,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_WOOD,
        "4W",
        CLUB_CATEGORY_WOOD,
        FAIRWAY_WOOD_SURFACES,
        95,
        180,
        -17,
        1,
        16,
        100,
        -10
    ),
    CLUB_ENTRY(
        CLUB_TYPE_TWO_IRON,
        "2I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        170,
        -18,
        1,
        22,
        100,
        -9
    ),
    CLUB_ENTRY(
        CLUB_TYPE_THREE_IRON,
        "3I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        160,
        -20,
        1,
        24,
        100,
        -8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FOUR_IRON,
        "4I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        150,
        -22,
        2,
        26,
        100,
        -8
    ),
    CLUB_ENTRY(
        CLUB_TYPE_FIVE_IRON,
        "5I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        140,
        -23,
        2,
        37,
        100,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SIX_IRON,
        "6I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        130,
        -24,
        3,
        42,
        100,
        -7
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SEVEN_IRON,
        "7I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        120,
        -25,
        3,
        52,
        100,
        -6
    ),
    CLUB_ENTRY(
        CLUB_TYPE_EIGHT_IRON,
        "8I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        110,
        -26,
        3,
        57,
        100,
        -5
    ),
    CLUB_ENTRY(
        CLUB_TYPE_NINE_IRON,
        "9I",
        CLUB_CATEGORY_IRON,
        IRON_SURFACES,
        95,
        100,
        -27,
        4,
        67,
        100,
        -4
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PITCHING_WEDGE,
        "PW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        95,
        85,
        -38,
        4,
        86,
        100,
        -3
    ),
    CLUB_ENTRY(
        CLUB_TYPE_SAND_WEDGE,
        "SW",
        CLUB_CATEGORY_WEDGE,
        IRON_SURFACES,
        95,
        65,
        -42,
        5,
        97,
        100,
        -2
    ),
    CLUB_ENTRY(
        CLUB_TYPE_PUTTER,
        "PT",
        CLUB_CATEGORY_PUTTER,
        CLUB_SURFACE_ALL,
        95,
        30,
        0,
        10,
        10,
        100,
        0
    ),
};
static Club f_NoClub = {
    80,
    "0000",
    0,
    230,
    -11,
    0,
    14,
    100,
    CLUB_TYPE_DRIVER,
    {0, 0, 0},
    CLUB_CATEGORY_WOOD,
    0,
    {0, 0, 0},
    ASSET_DRIVER_HEAD_IMAGE,
    0,
};
static ShotLaunch f_ClubTestShot =
    {STRAIGHT_ANGLE, NO_CLUB, LAUNCH_POWER_MAX, 0, 0, 0, 0, SHOT_MODE_NORMAL};
#define LIE_RATING_LEVEL_MAX 4
#define DEFAULT_LIE_RATING_PERCENT PERCENT_PER_WHOLE
static u8 f_LieRatingPercents[(LIE_RATING_LEVEL_MAX * 2) + 1] =
    {30, 50, 70, 80, 100, 115, 120, 145, 170};
static s8 f_StockCurves[CLUB_SET_COUNT] =
    {0, -1, 0, 1, -2, 0, 2, -3, 2, 3, 0, -3, 3, 3, 0, 0, 0, 0};
static ShotLaunch f_PutterTestShot =
    {STRAIGHT_ANGLE, NO_CLUB, LAUNCH_POWER_MAX, 0, 0, 0, 0, SHOT_MODE_NORMAL};
static s32 f_ClubChoiceDelay = CLUB_CHOICE_STEP_DELAY;

static const ClubParameters f_ClubParameters[CLUB_COUNT] = {
    {{91273, 81148, 20488}, {10, 10, 15}},
    {{80352, 71517, 20154}, {11, 10, 13}},
    {{71104, 63580, 19613}, {12, 16, 12}},
    {{62157, 54639, 19140}, {20, 18, 15}},
    {{56746, 50842, 18471}, {19, 20, 18}},
    {{51925, 46196, 17780}, {20, 23, 17}},
    {{47897, 42264, 16917}, {25, 28, 20}},
    {{43660, 38865, 16568}, {31, 26, 23}},
    {{39516, 34958, 16296}, {36, 36, 27}},
    {{35857, 32197, 15960}, {34, 34, 33}},
    {{31958, 28817, 15952}, {37, 34, 30}},
    {{27458, 23975, 15605}, {41, 41, 51}},
    {{19465, 17681, 16037}, {51, 48, 49}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{96714, 84400, 20688}, {235, 128, 15}},
    {{85097, 76077, 20095}, {137, 72, 13}},
    {{73983, 66540, 19689}, {47, 21, 15}},
    {{66715, 59309, 19254}, {12, 12, 14}},
    {{60287, 53032, 18746}, {17, 15, 16}},
    {{54993, 48482, 18151}, {17, 15, 20}},
    {{49343, 42696, 17670}, {20, 21, 21}},
    {{44804, 39405, 16991}, {23, 23, 23}},
    {{40348, 35122, 16598}, {26, 29, 23}},
    {{36481, 31597, 16106}, {25, 31, 28}},
    {{32265, 28208, 15932}, {30, 32, 32}},
    {{26016, 23939, 15605}, {40, 35, 40}},
    {{18989, 17165, 15829}, {56, 44, 42}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{114263, 101092, 19138}, {10, 10, 18}},
    {{99750, 88951, 18974}, {12, 13, 15}},
    {{89648, 79116, 18452}, {16, 14, 18}},
    {{78202, 68737, 17922}, {19, 19, 18}},
    {{72335, 63335, 17575}, {20, 21, 20}},
    {{66544, 57720, 17007}, {23, 24, 21}},
    {{60072, 52686, 16348}, {25, 27, 22}},
    {{54785, 47896, 15986}, {28, 28, 24}},
    {{48998, 43660, 15901}, {31, 33, 29}},
    {{44483, 38902, 15915}, {34, 37, 30}},
    {{39609, 35302, 15702}, {38, 40, 30}},
    {{33415, 29836, 15605}, {49, 41, 43}},
    {{24048, 21680, 15605}, {48, 56, 49}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{95098, 83880, 18851}, {-419, -279, 19}},
    {{85831, 74761, 18588}, {-376, -221, 18}},
    {{74269, 65097, 18079}, {-313, -169, 18}},
    {{65773, 56912, 17745}, {-73, 10, 20}},
    {{59707, 52724, 17431}, {18, 19, 20}},
    {{54193, 47529, 16773}, {21, 20, 21}},
    {{50198, 43964, 16293}, {22, 26, 25}},
    {{46328, 40533, 15935}, {27, 29, 25}},
    {{41442, 35617, 15647}, {31, 33, 28}},
    {{36973, 32889, 15605}, {35, 33, 30}},
    {{33248, 29716, 15605}, {33, 40, 31}},
    {{28155, 25063, 15605}, {39, 39, 47}},
    {{20518, 17990, 15575}, {46, 46, 55}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{100035, 85831, 21093}, {966, 675, 13}},
    {{87781, 77621, 20658}, {939, 662, 15}},
    {{77101, 68679, 20157}, {825, 567, 16}},
    {{67068, 58727, 19251}, {688, 468, 16}},
    {{62084, 54708, 18623}, {525, 319, 16}},
    {{56982, 49273, 17914}, {294, 135, 18}},
    {{52371, 46246, 16934}, {194, 71, 22}},
    {{48218, 42430, 16637}, {50, 33, 24}},
    {{43646, 39144, 16270}, {28, 29, 30}},
    {{39576, 34986, 16254}, {35, 37, 28}},
    {{35435, 31265, 16210}, {34, 38, 31}},
    {{29640, 26184, 15911}, {50, 50, 44}},
    {{21945, 19729, 16449}, {50, 47, 48}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{94443, 82710, 19874}, {9, 10, 17}},
    {{83190, 74213, 19200}, {11, 11, 16}},
    {{73150, 65592, 18972}, {11, 12, 16}},
    {{64381, 55984, 18412}, {20, 15, 17}},
    {{57743, 51321, 17870}, {21, 17, 19}},
    {{53499, 47029, 17361}, {20, 18, 20}},
    {{50317, 43111, 16773}, {23, 25, 21}},
    {{44267, 38892, 16348}, {26, 29, 22}},
    {{40306, 35478, 16119}, {26, 29, 24}},
    {{36351, 31958, 15605}, {28, 34, 27}},
    {{32605, 28790, 15669}, {40, 30, 35}},
    {{27473, 24321, 15423}, {45, 43, 47}},
    {{20172, 17904, 15952}, {39, 47, 46}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{118021, 101263, 18852}, {-937, -800, 17}},
    {{103732, 89966, 18326}, {-896, -761, 18}},
    {{90848, 79048, 18357}, {-830, -660, 19}},
    {{78528, 69967, 17904}, {-914, -678, 17}},
    {{71062, 63018, 17754}, {-741, -484, 18}},
    {{65085, 58534, 17397}, {-500, -330, 21}},
    {{57737, 50643, 16628}, {-421, -227, 22}},
    {{52306, 46109, 16250}, {-77, 11, 24}},
    {{46873, 41469, 16140}, {24, 24, 23}},
    {{41732, 36836, 15605}, {26, 30, 23}},
    {{37666, 33185, 15605}, {33, 29, 26}},
    {{31611, 27877, 15163}, {36, 35, 35}},
    {{23120, 20574, 15605}, {40, 41, 45}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{121081, 105182, 20973}, {1449, 1180, 13}},
    {{106352, 90903, 20181}, {1501, 1191, 14}},
    {{92463, 81484, 20070}, {1417, 1146, 16}},
    {{83271, 71385, 18909}, {1326, 1056, 15}},
    {{74388, 64653, 18584}, {1124, 794, 16}},
    {{67978, 59497, 17840}, {857, 607, 19}},
    {{61936, 55532, 17222}, {789, 532, 21}},
    {{55865, 49324, 16755}, {479, 276, 25}},
    {{51446, 45760, 16300}, {261, 112, 27}},
    {{46642, 40924, 16001}, {67, 31, 31}},
    {{41325, 36423, 16032}, {33, 36, 35}},
    {{34746, 30536, 15605}, {39, 42, 56}},
    {{25550, 22727, 15917}, {47, 51, 51}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{132818, 115066, 19067}, {-895, -748, 18}},
    {{117959, 103455, 18852}, {-876, -735, 17}},
    {{103257, 91635, 18610}, {-789, -632, 17}},
    {{88561, 76963, 18203}, {-839, -565, 19}},
    {{80117, 70523, 17696}, {-812, -542, 23}},
    {{72626, 63924, 17639}, {-546, -334, 22}},
    {{64521, 56681, 16653}, {-436, -229, 26}},
    {{58355, 50935, 16529}, {-337, -161, 21}},
    {{53410, 45814, 16107}, {-150, -12, 21}},
    {{46813, 41444, 16007}, {-52, 19, 27}},
    {{42090, 37189, 15605}, {26, 30, 25}},
    {{35419, 31159, 15605}, {34, 41, 38}},
    {{25727, 22877, 15605}, {40, 36, 45}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{138873, 119412, 19440}, {-1283, -1064, 17}},
    {{120879, 104818, 18981}, {-1237, -981, 15}},
    {{105947, 92559, 18699}, {-1134, -896, 17}},
    {{92073, 79255, 18362}, {-1293, -1015, 21}},
    {{82806, 72787, 17745}, {-1093, -812, 20}},
    {{76534, 66734, 17247}, {-1043, -688, 21}},
    {{69011, 61440, 16872}, {-908, -624, 19}},
    {{62652, 54565, 16433}, {-575, -332, 24}},
    {{57092, 48882, 15806}, {-325, -118, 27}},
    {{50643, 44828, 15819}, {-72, 17, 28}},
    {{44729, 39398, 15605}, {32, 30, 34}},
    {{37792, 32706, 15226}, {39, 43, 37}},
    {{27322, 24595, 15605}, {48, 39, 47}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{106457, 93361, 20055}, {9, 10, 16}},
    {{91683, 80929, 19468}, {14, 11, 18}},
    {{81084, 71005, 19159}, {15, 15, 15}},
    {{72429, 62782, 18686}, {17, 19, 14}},
    {{65473, 57108, 17951}, {21, 24, 19}},
    {{59098, 51660, 17501}, {22, 25, 18}},
    {{55682, 48543, 16845}, {25, 23, 22}},
    {{50610, 43592, 16307}, {24, 27, 20}},
    {{46579, 40899, 15886}, {31, 29, 29}},
    {{41017, 36656, 15892}, {37, 33, 31}},
    {{36783, 33214, 15773}, {34, 34, 31}},
    {{30683, 27220, 15837}, {35, 42, 46}},
    {{22681, 20205, 15782}, {44, 53, 45}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{148735, 130352, 20979}, {1485, 1337, 17}},
    {{131882, 115746, 20254}, {1370, 1232, 12}},
    {{116071, 99926, 19870}, {1339, 1205, 16}},
    {{98224, 86372, 19422}, {1353, 1136, 15}},
    {{88171, 77618, 19175}, {1105, 860, 15}},
    {{80209, 70554, 18598}, {884, 676, 17}},
    {{71315, 61835, 17880}, {698, 447, 18}},
    {{63281, 56566, 17377}, {328, 190, 19}},
    {{56618, 49898, 17238}, {55, 27, 16}},
    {{51058, 44343, 17030}, {21, 19, 15}},
    {{44835, 39214, 16390}, {23, 27, 24}},
    {{38250, 33360, 15190}, {31, 31, 31}},
    {{26962, 24182, 15059}, {39, 37, 38}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{149091, 129065, 19482}, {-1610, -1373, 13}},
    {{130311, 113687, 19219}, {-1729, -1485, 13}},
    {{115455, 99598, 18907}, {-1779, -1497, 15}},
    {{99530, 87001, 18644}, {-1998, -1734, 17}},
    {{90123, 78456, 17872}, {-1953, -1717, 19}},
    {{82740, 72384, 17777}, {-1984, -1720, 18}},
    {{74839, 64119, 16929}, {-2076, -1913, 20}},
    {{67913, 58363, 16811}, {-2101, -1865, 22}},
    {{60497, 52426, 16141}, {-2157, -1945, 25}},
    {{52373, 47025, 15974}, {-2000, -1746, 24}},
    {{46742, 41727, 15605}, {-1968, -1702, 27}},
    {{39487, 34757, 15605}, {-1995, -1776, -67}},
    {{27948, 25105, 15135}, {-1456, -1088, -109}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{148190, 129885, 19440}, {-1352, -1166, 17}},
    {{131682, 113169, 18981}, {-1336, -1105, 15}},
    {{113538, 99917, 18699}, {-1268, -1046, 17}},
    {{99616, 88561, 18362}, {-1410, -1184, 21}},
    {{91683, 80702, 17745}, {-1287, -1025, 20}},
    {{83554, 72420, 17247}, {-1209, -909, 21}},
    {{74705, 65861, 16872}, {-1066, -762, 19}},
    {{67008, 59143, 16433}, {-709, -466, 24}},
    {{61458, 54110, 15806}, {-450, -239, 27}},
    {{54114, 47577, 15819}, {-134, -22, 28}},
    {{48362, 42749, 15605}, {19, 34, 34}},
    {{40533, 36332, 15226}, {45, 38, 37}},
    {{29418, 26030, 15605}, {48, 50, 47}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{92100, 79978, 20597}, {-662, -468, 14}},
    {{79131, 70724, 20291}, {-596, -395, 15}},
    {{69479, 61514, 19166}, {-450, -257, 14}},
    {{61521, 55369, 18861}, {-412, -238, 17}},
    {{57100, 49707, 18424}, {-264, -107, 15}},
    {{50410, 45748, 18033}, {-78, -4, 18}},
    {{47692, 41862, 16996}, {-16, 27, 20}},
    {{42779, 37352, 16678}, {27, 27, 22}},
    {{38736, 33833, 16224}, {33, 31, 28}},
    {{34119, 30733, 15868}, {33, 30, 33}},
    {{30279, 27237, 15963}, {37, 33, 31}},
    {{25758, 23141, 15605}, {38, 46, 47}},
    {{18448, 16271, 15605}, {48, 49, 50}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{90042, 80758, 20055}, {11, 11, 16}},
    {{79420, 70511, 19468}, {11, 12, 18}},
    {{69466, 62649, 19159}, {14, 13, 15}},
    {{62782, 54843, 18686}, {19, 18, 14}},
    {{57108, 50296, 17951}, {24, 20, 19}},
    {{52612, 46696, 17501}, {20, 19, 18}},
    {{47885, 42688, 16845}, {25, 23, 22}},
    {{43202, 37914, 16307}, {29, 24, 20}},
    {{38633, 34730, 15886}, {31, 31, 29}},
    {{34779, 31194, 15892}, {36, 32, 31}},
    {{31364, 27761, 15773}, {34, 36, 31}},
    {{26671, 23211, 15837}, {40, 42, 46}},
    {{18770, 17291, 15782}, {45, 45, 45}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{95440, 83898, 21153}, {9, 8, 14}},
    {{83490, 73625, 20595}, {11, 10, 11}},
    {{72377, 64695, 20020}, {14, 12, 14}},
    {{63633, 57154, 19213}, {16, 17, 16}},
    {{57321, 50644, 18795}, {16, 19, 17}},
    {{53646, 47180, 18016}, {22, 18, 17}},
    {{49005, 42787, 17418}, {23, 24, 18}},
    {{44020, 39930, 16572}, {28, 27, 22}},
    {{39495, 35539, 16434}, {30, 30, 26}},
    {{35954, 31954, 16296}, {36, 36, 27}},
    {{31958, 28409, 15893}, {34, 33, 27}},
    {{26163, 23428, 15605}, {45, 42, 55}},
    {{19181, 16981, 15770}, {48, 42, 49}},
    {{87366, 43749, 13217}, {6, 12, 32}},
    {{92443, 81930, 19138}, {398, 242, 18}},
    {{80847, 71977, 18974}, {347, 208, 15}},
    {{70960, 63147, 18452}, {309, 164, 18}},
    {{65421, 57076, 17922}, {224, 100, 18}},
    {{59238, 51176, 17575}, {68, 21, 20}},
    {{53110, 47266, 17007}, {34, 23, 21}},
    {{49637, 44148, 16348}, {28, 27, 22}},
    {{45031, 39325, 15986}, {28, 29, 24}},
    {{40712, 35783, 15901}, {33, 37, 29}},
    {{36865, 32410, 15915}, {34, 31, 30}},
    {{32560, 28633, 15702}, {41, 41, 30}},
    {{27043, 23760, 15605}, {40, 41, 43}},
    {{19456, 17496, 15605}, {48, 45, 49}},
    {{87366, 43749, 13217}, {6, 12, 32}},
};

const ClubParameters *GetClubParameters(ClubIndex clubIndex) {
    return &f_ClubParameters[clubIndex];
}

void DoNothingWithClubs(void) {
}

s32 GetShotDistance(Club *club, ShotMode shotMode) {
    s32 distance = 0;

    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        switch ((u32)shotMode) {
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
    }
    else {
        switch ((u32)shotMode) {
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

s32 GetShotWorldDistance(Club *club, ShotMode shotMode) {
    s32 distance = GetShotDistance(club, shotMode);

    if (club->surfaceMask & CLUB_SURFACE_GREEN) {
        distance = distance * UNITS_PER_FOOT;
    }
    else {
        distance = distance * UNITS_PER_YARD;
    }

    return distance;
}

void WriteClubParameters(void) {
    char line[CLUB_SOURCE_LINE_SIZE];
    char *cursor;
    char *end;
    char *source;
    s32 index;
    s32 step;
    Club *club;

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

    source = AllocMemory(CLUB_SOURCE_SIZE);
    cursor = source;
    end = source + CLUB_SOURCE_SIZE;
    if (source == NULL) {
        ReportFailedAssertion("EX", "c:/usgolf64/club.c", CLUB_ASSERT_LINE);
    }

    sprintf(
        line,
        "/* Caution! This file is automatically generated by club.c*/\n"
        "static const CPARA cp[MAXCLUB] = {\n"
    );
    AppendText(line);
    for (index = 0; index < CLUB_COUNT; index++) {
        club = &g_Clubs[index];
        sprintf(
            line,
            "{{%d, %d, %d}, {%d, %d, %d}},\n",
            (int)club->power[0],
            (int)club->power[1],
            (int)club->power[2],
            club->deflection[0],
            club->deflection[1],
            club->deflection[2]
        );
        AppendText(line);
    }

    sprintf(
        line,
        "};\nextern CPARA *kGetCparaInfo (LONG clubno) { return &cp[clubno]; "
        "}\n"
    );
    AppendText(line);
    AppendText("LONG lkPutterPower[3][30] = {");
    for (index = 0; index < SHOT_MODE_COUNT; index++) {
        AppendText("{");
        for (step = 1; step <= PUTT_STEP_COUNT; step++) {
            sprintf(
                line,
                "%d,\n",
                (int)(GetPuttPower((step * LAUNCH_POWER_MAX) / PUTT_STEP_COUNT, index))
            );
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

void GenerateClub(ClubIndex clubIndex) {
    BallSimulation simulation;
    Club *club;
    ShotMode shotMode;
    s32 targetDistance;
    s32 distance;
    s32 shortDistance;
    s32 shortPower;
    s32 longDistance;
    s32 longPower;
    s32 tryIndex;
    s32 step;
    f32 error;

    s32 InterpolatePower(s32 from, s32 to, f32 fraction) {
        return from + (to - from) * (f64)fraction;
    }

    club = &g_Clubs[clubIndex];
    f_ClubTestShot.clubSlot = clubIndex;
    for (shotMode = 0; shotMode < SHOT_MODE_COUNT; shotMode++) {
        f_ClubTestShot.shotMode = shotMode;

        targetDistance = GetShotDistance(club, shotMode);
        osSyncPrintf(
            "Generation Club%d PowerMode%d TargetDist %d\n",
            clubIndex,
            shotMode,
            targetDistance
        );
        club->power[shotMode] = targetDistance * 260.096f;
        shortDistance = 0;
        shortPower = 0;
        longDistance = 0;
        longPower = 0;
        if (club->surfaceMask & CLUB_SURFACE_GREEN) {
            targetDistance = targetDistance * 0x3C00;
            g_ClubTestMode = CLUB_TEST_PUTT;
        }
        else {
            g_ClubTestMode = CLUB_TEST_SHOT;
            targetDistance = targetDistance * UNITS_PER_YARD;
        }

        osSyncPrintf("Generation Club%d PowerMode%d\n", clubIndex, shotMode);
        for (tryIndex = 0; tryIndex < CLUB_TRY_COUNT_MAX; tryIndex++) {
            simulation.position[AXIS_X] = 0;
            simulation.position[AXIS_Y] = 0;
            simulation.position[AXIS_Z] = 0;
            LaunchBallSimulation(&simulation, LAUNCH_PURPOSE_CLUB_TEST, &f_ClubTestShot);
            for (step = 0; step < TEST_SHOT_STEP_COUNT_MAX; step++) {
                if (simulation.state == BALL_STATE_STOPPED) {
                    break;
                }

                StepBallSimulation(&simulation);
            }

            distance = IntLength2D(simulation.position[AXIS_X], simulation.position[AXIS_Z]);
            error = distance - targetDistance;
            osSyncPrintf(
                "Try %d: dist %fYd:  Error %f%\n",
                tryIndex + 1,
                distance * YARDS_PER_UNIT,
                error / targetDistance * (f32)PERCENT_PER_WHOLE
            );
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
                    club->power[shotMode] = longPower * 3 / 4;
                }
                else {
                    error =
                        (f32)(targetDistance - shortDistance) / (f32)(longDistance - shortDistance);
                    club->power[shotMode] = InterpolatePower(shortPower, longPower, error);
                }
            }
            else {
                shortDistance = distance;
                shortPower = club->power[shotMode];
                if (longDistance == shortDistance) {
                    break;
                }

                if (longPower == 0) {
                    club->power[shotMode] = shortPower * 3 / 2;
                }
                else {
                    error =
                        (f32)(targetDistance - shortDistance) / (f32)(longDistance - shortDistance);
                    club->power[shotMode] = InterpolatePower(shortPower, longPower, error);
                }
            }
        }

        club->deflection[shotMode] = STRAIGHT_ANGLE -
            (s32)(atan2(
                      simulation.position[AXIS_Z] - simulation.startPosition[AXIS_Z],
                      simulation.position[AXIS_X] - simulation.startPosition[AXIS_X]
                  ) *
                RADIANS_TO_BINARY_ANGLE);
    }

    g_ClubTestMode = CLUB_TEST_NONE;
}

#define CLUB_DISTANCE_MAX 400
#define FLATTEST_CLUB_PITCH (-7)
#define STEEPEST_CLUB_PITCH (-45)
#define CLUB_SIDE_SPIN_MAX 100
#define CLUB_CURVE_MAX 4
#define BASE_MISHIT_SCALE 110
#define MISHIT_SCALE_PER_RESISTANCE 9
#define MISHIT_SCALE_MIN 70
#define MISHIT_SCALE_MAX 145

void GenerateClubSet(s32 setIndex, GameBoyCharacter *gameBoyCharacter) {
    ClubType type;
    Club *club;

    for (type = 0; type < CLUB_SET_SIZE; type++) {
        club = GetClub((setIndex * CLUB_SET_SIZE) + type);
        club->distance = gameBoyCharacter->clubs[type].distance;
        if (club->distance > CLUB_DISTANCE_MAX) {
            club->distance = CLUB_DISTANCE_MAX;
        }

        osSyncPrintf("TargetDist %d\n", club->distance);
        club->pitch =
            -((gameBoyCharacter->clubs[type].pitch * DEGREES_PER_TURN / BINARY_ANGLE_TURN) +
                gameBoyCharacter->clubPitch);
        osSyncPrintf("Xang %d\n", club->pitch);
        if (club->pitch > FLATTEST_CLUB_PITCH) {
            club->pitch = FLATTEST_CLUB_PITCH;
        }

        if (club->pitch < STEEPEST_CLUB_PITCH) {
            club->pitch = STEEPEST_CLUB_PITCH;
        }

        club->sideSpin = gameBoyCharacter->clubs[type].sideSpin;
        if (club->sideSpin > CLUB_SIDE_SPIN_MAX) {
            club->sideSpin = CLUB_SIDE_SPIN_MAX;
        }

        if (club->sideSpin < -CLUB_SIDE_SPIN_MAX) {
            club->sideSpin = -CLUB_SIDE_SPIN_MAX;
        }

        if ((u8)(gameBoyCharacter->lieRatingLevel + LIE_RATING_LEVEL_MAX) >=
            ARRAY_COUNT(f_LieRatingPercents)) {
            club->lieRatingPercent = DEFAULT_LIE_RATING_PERCENT;
        }
        else {
            club->lieRatingPercent =
                f_LieRatingPercents[gameBoyCharacter->lieRatingLevel + LIE_RATING_LEVEL_MAX];
        }

        club->mishitScale =
            BASE_MISHIT_SCALE - (gameBoyCharacter->mishitResistance * MISHIT_SCALE_PER_RESISTANCE);
        club->curve = gameBoyCharacter->shotCurve;
        if (club->curve < -CLUB_CURVE_MAX) {
            club->curve = -CLUB_CURVE_MAX;
        }

        if (club->curve > CLUB_CURVE_MAX) {
            club->curve = CLUB_CURVE_MAX;
        }

        if (club->mishitScale > MISHIT_SCALE_MAX) {
            club->mishitScale = MISHIT_SCALE_MAX;
        }

        if (club->mishitScale < MISHIT_SCALE_MIN) {
            club->mishitScale = MISHIT_SCALE_MIN;
        }

        g_GameState.isMiniGolf = FALSE;
        GenerateClub((setIndex * CLUB_SET_SIZE) + type);
    }
}

void InitClubs(void) {
    ClubIndex clubIndex;
    ClubParameters *parameters;
    Club *club;

    s8 GetStockCurve(ClubIndex clubIndex) {
        u32 setIndex = clubIndex / CLUB_SET_SIZE;

        if (setIndex >= CLUB_SET_COUNT) {
            setIndex = 0;
        }

        return f_StockCurves[setIndex];
    }

    if (!IsGameFlagSet(GAME_FLAG_GENERATE_CLUBS)) {
        for (clubIndex = 0; clubIndex < STOCK_CLUB_SET_COUNT * CLUB_SET_SIZE; clubIndex++) {
            club = &g_Clubs[clubIndex];

            parameters = (ClubParameters *)GetClubParameters(clubIndex);
            club->power[0] = parameters->power[0];
            club->power[1] = parameters->power[1];
            club->power[2] = parameters->power[2];
            club->deflection[0] = parameters->deflection[0];
            club->deflection[1] = parameters->deflection[1];
            club->deflection[2] = parameters->deflection[2];
            club->curve = GetStockCurve(clubIndex);
        }
    }
    else if (IsGameFlagSet(GAME_FLAG_GENERATE_CLUBS)) {
        for (clubIndex = 0; clubIndex < CLUB_COUNT; clubIndex++) {
            GenerateClub(clubIndex);
        }

        WriteClubParameters();
    }
}

s32 GetPuttPower(s32 fraction, ShotMode shotMode) {
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
    f32 distancePerPower;
    s32 initialPowerNumerator;
    f32 error;
    s32 power;
    ShotMode testShotMode;

    s32 InterpolatePuttPower(s32 from, s32 to, f32 fraction) {
        return from + (to - from) * (f64)fraction;
    }

    if (!IsGameFlagSet(GAME_FLAG_GENERATE_CLUBS)) {
        index = fraction * PUTT_STEP_COUNT / LAUNCH_POWER_MAX;
        if (index > PUTT_STEP_COUNT) {
            index = PUTT_STEP_COUNT;
        }

        if (index <= 0) {
            index = 1;
        }

        index--;
        return f_PutterPowers[shotMode][index];
    }

    testShotMode = shotMode;
    f_PutterTestShot.clubSlot = CLUB_TYPE_PUTTER;
    f_PutterTestShot.shotMode = testShotMode;
    putter = &g_Clubs[CLUB_TYPE_PUTTER];
    reach = GetShotDistance(putter, testShotMode);
    distancePerPower = reach * (UNITS_PER_FOOT / LAUNCH_POWER_MAX);
    initialPowerNumerator = (s32)(reach * PUTT_POWER_ESTIMATE_SCALE);
    shortDistance = 0;
    shortPower = 0;
    longDistance = 0;
    longPower = 0;
    tryIndex = 0;
    g_ClubTestMode = CLUB_TEST_PUTT;
    targetDistance = (s32)(distancePerPower * fraction);
    targetDistance += PUTT_OVERSHOOT;
    putter->power[testShotMode] = initialPowerNumerator / PUTT_POWER_ESTIMATE_DIVISOR;
    for (; tryIndex < PUTT_TRY_COUNT_MAX; tryIndex++) {
        simulation.position[AXIS_X] = 0;
        simulation.position[AXIS_Y] = 0;
        simulation.position[AXIS_Z] = 0;
        simulation.surface = PUTT_TEST_SURFACE;
        LaunchBallSimulation(&simulation, LAUNCH_PURPOSE_CLUB_TEST, &f_PutterTestShot);
        for (step = 0; step < TEST_SHOT_STEP_COUNT_MAX; step++) {
            if (simulation.state == BALL_STATE_STOPPED) {
                break;
            }

            StepBallSimulation(&simulation);
        }

        distance = IntLength2D(simulation.position[AXIS_X], simulation.position[AXIS_Z]);
        error = fabsf(distance - targetDistance);
        if (error < targetDistance * GENERATION_TOLERANCE) {
            break;
        }

        if (distance > targetDistance) {
            IN_OWN_BLOCK(longPower = putter->power[testShotMode]);
            longDistance = distance;
            if (longDistance == shortDistance) {
                break;
            }

            if (shortPower == 0) {
                power = longPower * 3 / 4;
            }
            else {
                power = InterpolatePuttPower(
                    shortPower,
                    longPower,
                    (f32)(targetDistance - shortDistance) / (f32)(longDistance - shortDistance)
                );
            }

            putter->power[testShotMode] = power;
        }
        else {
            shortDistance = distance;
            shortPower = putter->power[testShotMode];
            if (longDistance == shortDistance) {
                break;
            }

            if (longPower == 0) {
                power = shortPower * 3 / 2;
                putter->power[testShotMode] = power;
            }
            else {
                power = InterpolatePuttPower(
                    shortPower,
                    longPower,
                    (f32)(targetDistance - shortDistance) / (f32)(longDistance - shortDistance)
                );
                putter->power[testShotMode] = power;
            }
        }
    }

    g_ClubTestMode = CLUB_TEST_NONE;
    return putter->power[testShotMode];
}

s32 GetClubPower(Club *club, ShotMode shotMode) {
    return club->power[shotMode];
}

Club *GetClub(UClubIndex clubIndex) {
    return (clubIndex < CLUB_COUNT) ? &g_Clubs[clubIndex] : &f_NoClub;
}

Club *GetSelectedClub(void) {
    return &g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[g_CurrentShot.clubSlot]];
}

s32 GetSelectedShotDistance(void) {
    return GetShotDistance(GetSelectedClub(), g_CurrentShot.shotMode);
}

s32 GetSelectedShotWorldDistance(void) {
    return GetShotWorldDistance(GetSelectedClub(), g_CurrentShot.shotMode);
}

ClubSurfaceMask GetSurfaceClubMask(Boolean distinguishesKinds, SurfaceTraits *surfaceTraits) {
    SurfaceKind surfaceKind = surfaceTraits->kind;
    ClubSurfaceMask mask;

    if (IsGameFlagSet(GAME_FLAG_ANY_CLUB_ANYWHERE)) {
        return CLUB_SURFACE_ALL;
    }

    if (g_GameState.isMiniGolf) {
        return CLUB_SURFACE_GREEN;
    }

    if (!distinguishesKinds) {
        return (surfaceKind != SURFACE_KIND_GREEN) ? CLUB_SURFACE_ALL : CLUB_SURFACE_GREEN;
    }

    switch (surfaceKind) {
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_BARE_GROUND:
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_WASTE_AREA:
            mask = CLUB_SURFACE_ROUGH;
            break;
        case SURFACE_KIND_GREEN:
            mask = CLUB_SURFACE_GREEN;
            break;
        case SURFACE_KIND_ROUGH_POOR_LIE:
        case SURFACE_KIND_BUNKER:
        case SURFACE_KIND_BUNKER_FAIR_LIE:
        case SURFACE_KIND_BUNKER_POOR_LIE:
            mask = CLUB_SURFACE_BUNKER;
            break;
        case SURFACE_KIND_TEE_GROUND:
            mask = CLUB_SURFACE_TEE_GROUND;
            break;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
            mask = CLUB_SURFACE_FAIRWAY;
            break;
        case SURFACE_KIND_SEMI_ROUGH:
            mask = CLUB_SURFACE_SEMI_ROUGH;
            break;
        default:
            mask = CLUB_SURFACE_ALL;
            break;
    }

    return mask;
}

ClubSurfaceMask GetCurrentSurfaceClubMask(Boolean distinguishesKinds) {
    return GetSurfaceClubMask(distinguishesKinds, GetSurfaceTraits(g_CurrentShot.surface));
}

LieRating GetLieRating(Club *club, Surface surface, ShotMode shotMode) {
    const char *ratings;
    const char *approachRatings;
    LieRating rating;

    switch (GetSurfaceTraits(surface)->kind) {
        case SURFACE_KIND_TEE_UP:
            ratings = "3444444555555";
            approachRatings = "5555555555555";
            break;
        case SURFACE_KIND_FAIRWAY:
        case SURFACE_KIND_GREEN_EDGE:
        case SURFACE_KIND_TEE_GROUND:
            ratings = "3334444445555";
            approachRatings = "4444555555555";
            break;
        case SURFACE_KIND_FAIRWAY_FAIR_LIE:
        case SURFACE_KIND_SEMI_ROUGH:
            ratings = "2333344444455";
            approachRatings = "3344445555555";
            break;
        case SURFACE_KIND_FAIRWAY_POOR_LIE:
            ratings = "1222333344445";
            approachRatings = "2333334444455";
            break;
        case SURFACE_KIND_ROUGH:
        case SURFACE_KIND_ROCK:
        case SURFACE_KIND_BARE_GROUND:
            ratings = "1223333444455";
            approachRatings = "2233344445555";
            break;
        case SURFACE_KIND_ROUGH_POOR_LIE:
            ratings = "1112223334445";
            approachRatings = "2222333444455";
            break;
        case SURFACE_KIND_DEEP_ROUGH:
        case SURFACE_KIND_WASTE_AREA:
            ratings = "1111222233334";
            approachRatings = "1112222333344";
            break;
        case SURFACE_KIND_BUNKER:
            ratings = "1112223333445";
            approachRatings = "2223333444455";
            break;
        case SURFACE_KIND_BUNKER_FAIR_LIE:
            ratings = "1111122222334";
            approachRatings = "1112223333444";
            break;
        case SURFACE_KIND_BUNKER_POOR_LIE:
            ratings = "1111111222223";
            approachRatings = "1111112223334";
            break;
        case SURFACE_KIND_CART_WAY:
            ratings = "1112223333344";
            approachRatings = "2223333333444";
            break;
        default:
            ratings = "3333333333333";
            approachRatings = ratings;
            break;
    }

    if (club->type != CLUB_TYPE_PUTTER) {
        if (shotMode == SHOT_MODE_APPROACH) {
            rating = approachRatings[club->type] - '0';
        }
        else {
            rating = ratings[club->type] - '0';
        }

        rating = ((rating * club->lieRatingPercent) + (PERCENT_PER_WHOLE / 2)) / PERCENT_PER_WHOLE;
        if (rating <= NO_LIE_RATING) {
            rating = LIE_RATING_MIN;
        }

        if (rating > LIE_RATING_MAX) {
            rating = LIE_RATING_MAX;
        }
    }
    else {
        rating = NO_LIE_RATING;
    }

    return rating;
}

void GetLieSpan(Club *club, Surface surface, s32 *lieSpan, ShotMode shotMode) {
    LieRating rating = GetLieRating(club, surface, shotMode);

    if (rating > NO_LIE_RATING) {
        lieSpan[0] = (rating * 2) - 1;
        lieSpan[1] = rating - 1;
    }
    else {
        lieSpan[0] = 0;
        lieSpan[1] = 0;
    }
}

Boolean StepToClubChoice(CurrentShot *targetShot) {
    Boolean hasStepped = FALSE;
    ClubSlot clubSlot;

    if (g_CurrentShot.shotMode == targetShot->shotMode &&
        g_CurrentShot.clubSlot == targetShot->clubSlot) {
        return TRUE;
    }

    if (f_ClubChoiceDelay > CLUB_CHOICE_STEP_DELAY) {
        f_ClubChoiceDelay = CLUB_CHOICE_STEP_DELAY;
    }

    if (f_ClubChoiceDelay != 0) {
        f_ClubChoiceDelay--;
        return FALSE;
    }

    f_ClubChoiceDelay = CLUB_CHOICE_STEP_DELAY;
    if (!hasStepped) {
        if (g_CurrentShot.shotMode != targetShot->shotMode) {
            PlaySound(SOUND_LOOK_DOWN_RANGE, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
            hasStepped = TRUE;
            g_CurrentShot.shotMode = targetShot->shotMode;
        }
    }

    if (!hasStepped) {
        if (targetShot->clubSlot > g_CurrentShot.clubSlot) {
            PlaySound(SOUND_LOOK_DOWN_RANGE, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
            for (clubSlot = g_CurrentShot.clubSlot + 1; clubSlot < CLUB_SLOT_COUNT; clubSlot++) {
                if (g_Players[g_CurrentShot.playerIndex].clubs[clubSlot] != NO_CLUB &&
                    (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[clubSlot]].surfaceMask &
                        GetCurrentSurfaceClubMask(FALSE))) {
                    g_CurrentShot.clubSlot = clubSlot;
                    hasStepped = TRUE;
                    break;
                }
            }

            if (!hasStepped || g_CurrentShot.clubSlot > targetShot->clubSlot) {
                g_CurrentShot.clubSlot = targetShot->clubSlot;
            }
        }
        else if (targetShot->clubSlot < g_CurrentShot.clubSlot) {
            PlaySound(SOUND_LOOK_DOWN_RANGE, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
            for (clubSlot = g_CurrentShot.clubSlot - 1; clubSlot >= 0; clubSlot--) {
                if (g_Players[g_CurrentShot.playerIndex].clubs[clubSlot] != NO_CLUB &&
                    (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[clubSlot]].surfaceMask &
                        GetCurrentSurfaceClubMask(FALSE))) {
                    g_CurrentShot.clubSlot = clubSlot;
                    hasStepped = TRUE;
                    break;
                }
            }

            if (!hasStepped || g_CurrentShot.clubSlot < targetShot->clubSlot) {
                g_CurrentShot.clubSlot = targetShot->clubSlot;
            }
        }
    }

    return !hasStepped;
}

void UpdateClubControls(void) {
    ClubSlot clubSlot;
    u16 buttons;

    if (IsShotModeChangeDone()) {
        buttons = g_MenuControllerData[g_CurrentShot.controllerIndex].button;
        if (buttons & D_JPAD) {
            for (clubSlot = g_CurrentShot.clubSlot + 1; clubSlot < CLUB_SLOT_COUNT; clubSlot++) {
                if (g_Players[g_CurrentShot.playerIndex].clubs[clubSlot] != NO_CLUB &&
                    (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[clubSlot]].surfaceMask &
                        GetCurrentSurfaceClubMask(FALSE))) {
                    g_CurrentShot.clubSlot = clubSlot;
                    PlaySound(SOUND_LOOK_DOWN_RANGE, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    return;
                }
            }
        }
        else if (buttons & U_JPAD) {
            for (clubSlot = g_CurrentShot.clubSlot - 1; clubSlot >= 0; clubSlot--) {
                if (g_Players[g_CurrentShot.playerIndex].clubs[clubSlot] != NO_CLUB &&
                    (g_Clubs[g_Players[g_CurrentShot.playerIndex].clubs[clubSlot]].surfaceMask &
                        GetCurrentSurfaceClubMask(FALSE))) {
                    g_CurrentShot.clubSlot = clubSlot;
                    PlaySound(SOUND_LOOK_DOWN_RANGE, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
                    return;
                }
            }
        }
    }
}

static inline Boolean IsFartherThan(s32 distance, s32 limit) {
    return distance > limit;
}

void UpdateShotModeControls(void) {
    Club *club = GetSelectedClub();
    s32 cupDistance = GetCupDistance(
                          g_CurrentShot.ballPosition[AXIS_X] - g_HoleSetup.cupPosition[AXIS_X],
                          g_CurrentShot.ballPosition[AXIS_Z] - g_HoleSetup.cupPosition[AXIS_Z]
                      ) *
        YARDS_PER_UNIT;
    SoundId soundId;

    if ((g_MenuControllerData[g_CurrentShot.controllerIndex].button & B_BUTTON) &&
        IsShotModeChangeDone()) {
        if (club->surfaceMask & CLUB_SURFACE_GREEN) {
            if (g_CurrentShot.shotMode == PUTT_MODE_MIDDLE) {
                g_CurrentShot.shotMode = PUTT_MODE_LONG;
            }
            else if (g_CurrentShot.shotMode == PUTT_MODE_LONG) {
                g_CurrentShot.shotMode = PUTT_MODE_SHORT;
            }
            else {
                g_CurrentShot.shotMode = PUTT_MODE_MIDDLE;
            }

            soundId = SOUND_LOOK_DOWN_RANGE;
            PlaySound(soundId, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else if (IsFartherThan(cupDistance, APPROACH_SHOT_DISTANCE)) {
            if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                soundId = SOUND_POWER_SHOT_REFUSED;
                if (g_CurrentShot.powerShotCount != 0) {
                    g_CurrentShot.shotMode = SHOT_MODE_POWER;
                    soundId = SOUND_LOOK_DOWN_RANGE;
                }
            }
            else {
                g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
                soundId = SOUND_LOOK_DOWN_RANGE;
            }

            PlaySound(soundId, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
        }
        else {
            PlaySound(SOUND_LOOK_DOWN_RANGE, CLUB_SOUND_CHANNEL, SOUND_FULL_SCALE);
            if (g_CurrentShot.shotMode == SHOT_MODE_NORMAL) {
                g_CurrentShot.shotMode = SHOT_MODE_APPROACH;
            }
            else {
                g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
            }
        }
    }

    if (!(club->surfaceMask & CLUB_SURFACE_GREEN)) {
        if ((g_CurrentShot.shotMode == SHOT_MODE_POWER) & (cupDistance <= APPROACH_SHOT_DISTANCE)) {
            g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
        }

        if ((g_CurrentShot.shotMode == SHOT_MODE_APPROACH) &
            (cupDistance > APPROACH_SHOT_DISTANCE)) {
            g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
        }

        if (g_CurrentShot.shotMode == SHOT_MODE_POWER && g_CurrentShot.powerShotCount == 0) {
            g_CurrentShot.shotMode = SHOT_MODE_NORMAL;
        }
    }
}
