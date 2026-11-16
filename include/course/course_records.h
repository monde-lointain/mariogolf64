/*
 * Each course's name, its tournament's name and its holes' records: each
 * hole's par, handicap, yardage and flags, in the order the holes are
 * played. A build changes them by putting its own copy of this header before
 * the original on the include path.
 */

#ifndef COURSE_RECORDS_H
#define COURSE_RECORDS_H

// A hole's record, whose yardage is the same from each of its tees
#define HOLE_RECORD(par, handicap, yardage, flags) \
    {(par), (handicap), {(yardage), (yardage), (yardage)}, (flags)}

// A copy of this header that replaces courses may also define
// CUSTOM_HOLE_RADII, giving for each such course its CourseRecordIndex and
// its 18 holes' radii for the flyover camera, which circles a hole's cup at
// a distance that grows with the radius. The courses' entries are separated
// by commas, as in:
//
//     #define CUSTOM_HOLE_RADII {COURSE_RECORD_KOOPA_PARK, {200, 400, 200, ...}}
//
// The game's own holes use DEFAULT_HOLE_RADIUS, 200, or as much as 1000 for
// the longest.

// clang-format off
#define TOAD_HIGHLANDS_NAME "Toad HighLands"
#define TOAD_HIGHLANDS_TOURNAMENT_NAME "Toad Tournament"
#define TOAD_HIGHLANDS_HOLE_RECORDS \
    HOLE_RECORD(4, 11, 363, 0), \
    HOLE_RECORD(3, 1, 161, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 5, 343, 0), \
    HOLE_RECORD(5, 13, 477, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 15, 355, 0), \
    HOLE_RECORD(3, 7, 138, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 3, 387, 0), \
    HOLE_RECORD(5, 17, 511, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 9, 409, 0), \
    HOLE_RECORD(4, 16, 304, 0), \
    HOLE_RECORD(4, 4, 380, 0), \
    HOLE_RECORD(5, 12, 466, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 2, 165, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 10, 388, 0), \
    HOLE_RECORD(4, 8, 369, 0), \
    HOLE_RECORD(5, 18, 448, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 14, 149, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 6, 400, 0)

#define KOOPA_PARK_NAME "Koopa Park"
#define KOOPA_PARK_TOURNAMENT_NAME "Koopa Cup"
#define KOOPA_PARK_HOLE_RECORDS \
    HOLE_RECORD(4, 11, 372, 0), \
    HOLE_RECORD(5, 15, 527, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 5, 375, 0), \
    HOLE_RECORD(3, 7, 169, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 13, 400, 0), \
    HOLE_RECORD(5, 17, 545, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 9, 380, 0), \
    HOLE_RECORD(3, 3, 140, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 1, 362, 0), \
    HOLE_RECORD(4, 8, 381, 0), \
    HOLE_RECORD(4, 16, 366, 0), \
    HOLE_RECORD(5, 6, 512, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 14, 174, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 2, 392, 0), \
    HOLE_RECORD(4, 10, 436, 0), \
    HOLE_RECORD(3, 4, 154, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 12, 360, 0), \
    HOLE_RECORD(5, 18, 520, HOLE_FLAG_MARKED_PAR_FIVE)

#define SHY_GUY_DESERT_NAME "Shy Guy Desert"
#define SHY_GUY_DESERT_TOURNAMENT_NAME "Shy Guy International"
#define SHY_GUY_DESERT_HOLE_RECORDS \
    HOLE_RECORD(4, 11, 360, 0), \
    HOLE_RECORD(3, 13, 153, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(5, 15, 520, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 1, 390, 0), \
    HOLE_RECORD(4, 5, 400, 0), \
    HOLE_RECORD(4, 9, 420, 0), \
    HOLE_RECORD(3, 7, 170, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 3, 403, 0), \
    HOLE_RECORD(5, 17, 550, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 2, 380, 0), \
    HOLE_RECORD(4, 8, 412, 0), \
    HOLE_RECORD(3, 4, 177, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 12, 370, 0), \
    HOLE_RECORD(5, 18, 535, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 6, 164, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 14, 340, 0), \
    HOLE_RECORD(5, 10, 570, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 16, 398, 0)

#define YOSHIS_ISLAND_NAME "Yoshi's Island"
#define YOSHIS_ISLAND_TOURNAMENT_NAME "Yoshi Championship"
#define YOSHIS_ISLAND_HOLE_RECORDS \
    HOLE_RECORD(4, 9, 350, 0), \
    HOLE_RECORD(4, 15, 400, 0), \
    HOLE_RECORD(3, 5, 170, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(5, 13, 470, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 17, 382, 0), \
    HOLE_RECORD(3, 3, 155, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(5, 1, 544, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 11, 420, 0), \
    HOLE_RECORD(4, 7, 365, 0), \
    HOLE_RECORD(4, 6, 375, 0), \
    HOLE_RECORD(5, 18, 507, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 10, 140, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 16, 415, 0), \
    HOLE_RECORD(4, 12, 395, 0), \
    HOLE_RECORD(3, 2, 190, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 8, 398, 0), \
    HOLE_RECORD(4, 4, 389, 0), \
    HOLE_RECORD(5, 14, 530, HOLE_FLAG_MARKED_PAR_FIVE)

#define BOO_VALLEY_NAME "Boo Valley"
// Misspelled so in the game
#define BOO_VALLEY_TOURNAMENT_NAME "Boo Clasic"
#define BOO_VALLEY_HOLE_RECORDS \
    HOLE_RECORD(4, 17, 373, 0), \
    HOLE_RECORD(4, 5, 400, 0), \
    HOLE_RECORD(5, 11, 534, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 3, 199, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 7, 335, 0), \
    HOLE_RECORD(4, 13, 411, 0), \
    HOLE_RECORD(5, 15, 546, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 9, 136, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 1, 402, 0), \
    HOLE_RECORD(5, 16, 562, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 4, 357, 0), \
    HOLE_RECORD(3, 6, 152, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 14, 396, 0), \
    HOLE_RECORD(4, 8, 391, 0), \
    HOLE_RECORD(3, 18, 170, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 2, 407, 0), \
    HOLE_RECORD(5, 12, 484, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 10, 380, 0)

#define MARIOS_STAR_NAME "Mario's Star"
#define MARIOS_STAR_TOURNAMENT_NAME "Mario Open"
#define MARIOS_STAR_HOLE_RECORDS \
    HOLE_RECORD(4, 17, 338, 0), \
    HOLE_RECORD(4, 1, 414, 0), \
    HOLE_RECORD(5, 11, 510, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(3, 3, 194, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 13, 410, 0), \
    HOLE_RECORD(3, 15, 149, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(5, 9, 500, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 7, 346, 0), \
    HOLE_RECORD(4, 5, 416, 0), \
    HOLE_RECORD(4, 12, 400, 0), \
    HOLE_RECORD(5, 8, 515, 0), \
    HOLE_RECORD(3, 4, 172, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(4, 2, 400, 0), \
    HOLE_RECORD(4, 16, 380, 0), \
    HOLE_RECORD(4, 18, 419, 0), \
    HOLE_RECORD(3, 14, 195, HOLE_FLAG_PAR_THREE), \
    HOLE_RECORD(5, 10, 520, HOLE_FLAG_MARKED_PAR_FIVE), \
    HOLE_RECORD(4, 6, 395, 0)

#define LUIGIS_GARDEN_NAME "Luigi's Garden"
#define LUIGIS_GARDEN_HOLE_RECORDS \
    HOLE_RECORD(3, 17, 363, 0), \
    HOLE_RECORD(3, 7, 165, 0), \
    HOLE_RECORD(3, 11, 343, 0), \
    HOLE_RECORD(3, 9, 477, 0), \
    HOLE_RECORD(3, 3, 355, 0), \
    HOLE_RECORD(3, 13, 145, 0), \
    HOLE_RECORD(3, 15, 387, 0), \
    HOLE_RECORD(3, 1, 510, 0), \
    HOLE_RECORD(3, 5, 410, 0), \
    HOLE_RECORD(3, 12, 338, 0), \
    HOLE_RECORD(3, 10, 380, 0), \
    HOLE_RECORD(3, 6, 466, 0), \
    HOLE_RECORD(3, 14, 170, 0), \
    HOLE_RECORD(3, 8, 395, 0), \
    HOLE_RECORD(3, 18, 369, 0), \
    HOLE_RECORD(3, 4, 448, 0), \
    HOLE_RECORD(3, 16, 150, 0), \
    HOLE_RECORD(3, 2, 400, 0)

#define PEACHS_CASTLE_NAME "Peach's Castle"
#define PEACHS_CASTLE_HOLE_RECORDS \
    HOLE_RECORD(3, 17, 363, 0), \
    HOLE_RECORD(3, 7, 165, 0), \
    HOLE_RECORD(3, 11, 343, 0), \
    HOLE_RECORD(3, 9, 477, 0), \
    HOLE_RECORD(3, 3, 355, 0), \
    HOLE_RECORD(3, 13, 145, 0), \
    HOLE_RECORD(3, 15, 387, 0), \
    HOLE_RECORD(3, 5, 510, 0), \
    HOLE_RECORD(3, 1, 410, 0), \
    HOLE_RECORD(3, 16, 338, 0), \
    HOLE_RECORD(3, 14, 380, 0), \
    HOLE_RECORD(3, 8, 466, 0), \
    HOLE_RECORD(3, 10, 170, 0), \
    HOLE_RECORD(3, 6, 395, 0), \
    HOLE_RECORD(3, 18, 369, 0), \
    HOLE_RECORD(3, 4, 448, 0), \
    HOLE_RECORD(3, 12, 150, 0), \
    HOLE_RECORD(3, 2, 400, 0)

#define DRIVING_RANGE_NAME "Driving Range"
#define DRIVING_RANGE_HOLE_RECORDS \
    HOLE_RECORD(3, 1, 363, 0), \
    HOLE_RECORD(3, 1, 165, 0), \
    HOLE_RECORD(3, 1, 343, 0), \
    HOLE_RECORD(3, 1, 477, 0), \
    HOLE_RECORD(3, 1, 355, 0), \
    HOLE_RECORD(3, 1, 145, 0), \
    HOLE_RECORD(3, 1, 387, 0), \
    HOLE_RECORD(3, 1, 510, 0), \
    HOLE_RECORD(3, 1, 410, 0), \
    HOLE_RECORD(3, 1, 338, 0), \
    HOLE_RECORD(3, 1, 380, 0), \
    HOLE_RECORD(3, 1, 466, 0), \
    HOLE_RECORD(3, 1, 170, 0), \
    HOLE_RECORD(3, 1, 395, 0), \
    HOLE_RECORD(3, 1, 369, 0), \
    HOLE_RECORD(3, 1, 448, 0), \
    HOLE_RECORD(3, 1, 150, 0), \
    HOLE_RECORD(3, 1, 400, 0)
// clang-format on

#endif
