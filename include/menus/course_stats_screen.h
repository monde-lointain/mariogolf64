/*
 * The clubhouse's course stats, in ovl_01/course_menu.c
 */

#ifndef COURSE_STATS_SCREEN_H
#define COURSE_STATS_SCREEN_H

#include <ultra64.h>

#include "course/course.h"
#include "system/asset.h"

// The buffers that LoadCourseMenuBuffer keeps, by the kind its caller gives
enum CourseMenuBuffer {
    // Neither kept nor read
    COURSE_MENU_BUFFER_UNUSED = 1,
    COURSE_MENU_BUFFER_HOLE_IN_ONE_BADGE,
    COURSE_MENU_BUFFER_NAME_PLATE,
    // Kept in the hole-in-one badge's place
    COURSE_MENU_BUFFER_SPINNING_BADGE,
    COURSE_MENU_BUFFER_DIGITS_AND_ARROWS,
    COURSE_MENU_BUFFER_LARGE_DIGITS,
    COURSE_MENU_BUFFER_GREEN_ARROWS
};

typedef s16 CourseMenuBuffer;

// The buffers LoadCourseTrophyImages keeps, by the kind its caller gives
enum CourseBuffer {
    COURSE_BUFFER_FIRST = 1,
    COURSE_BUFFER_SECOND,
    COURSE_BUFFER_LAST
};

typedef s32 CourseBuffer;
typedef s8 CourseBuffer8;
void LoadCourseStatsIcons(ImageAsset **buffers);
void LoadCourseTrophyImages(ImageAsset **buffers, CourseBuffer requestedKind);
void LoadCourseMenuBuffer(void *buffer, CourseMenuBuffer bufferKind);
void UpdateCourseStats(void);
void DrawCourseStats(Gfx **displayListHead);
void DrawCourseBanner(Gfx **displayListHead, u8 useCourseNames, CourseIndex courseIndex);

#endif
