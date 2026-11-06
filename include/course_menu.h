#ifndef COURSE_MENU_H
#define COURSE_MENU_H

#include <ultra64.h>

/* The clubhouse's course stats, in ovl_01/course_menu.c */

/* The buffers that LoadCourseMenuBuffer keeps, by the kind its caller gives */
enum CourseMenuBuffer {
  /* Neither kept nor read */
  COURSE_MENU_BUFFER_UNUSED = 1,
  COURSE_MENU_BUFFER_HOLE_IN_ONE_BADGE,
  COURSE_MENU_BUFFER_NAME_PLATE,
  /* Kept in the hole-in-one badge's place */
  COURSE_MENU_BUFFER_SPINNING_BADGE,
  COURSE_MENU_BUFFER_DIGITS_AND_ARROWS,
  COURSE_MENU_BUFFER_LARGE_DIGITS,
  COURSE_MENU_BUFFER_GREEN_ARROWS
};

/* The buffers LoadCourseTrophyImages keeps, by the kind its caller gives */
enum CourseBuffer {
  COURSE_BUFFER_FIRST = 1,
  COURSE_BUFFER_SECOND,
  COURSE_BUFFER_LAST
};

void LoadCourseStatsIcons(void** buffers);
void LoadCourseTrophyImages(void** buffers, s32 requestedKind);
void LoadCourseMenuBuffer(void* buffer, s16 bufferKind);
void UpdateCourseStats(void);
void DrawCourseStats(Gfx** displayListHead);
void DrawCourseBanner(
    Gfx** displayListHead, u8 useCourseNames, s32 courseIndex);

#endif
