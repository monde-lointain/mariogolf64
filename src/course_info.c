#include "common.h"

#include "billboard.h"
#include "course.h"
#include "save.h"
#include "scene.h"
#include "variables.h"

/* The radius used for holes not listed in GetHoleRadius */
#define DEFAULT_HOLE_RADIUS 200
#define MINI_GOLF_HOLE_RADIUS 30

extern RingShotHole g_RingShotHoles[RING_SHOT_COURSE_COUNT]
                                   [RING_SHOT_HOLE_COUNT];

/*
 * The one caller scales the sine and cosine of an angle by this, so it is
 * taken to be a radius
 */
s32 GetHoleRadius(s32 courseRecordIndex, s32 holeIndex) {
  switch (courseRecordIndex) {
    case 0:
      if (holeIndex == 10) {
        return 400;
      }
      if (holeIndex == 16) {
        return 400;
      }
      break;
    case 1:
      if (holeIndex == 1) {
        return 400;
      }
      if (holeIndex == 5) {
        return 600;
      }
      if (holeIndex == 7) {
        return 150;
      }
      if (holeIndex == 16) {
        return 300;
      }
      break;
    case 2:
      if (holeIndex == 2) {
        return 300;
      }
      if (holeIndex == 12) {
        return 600;
      }
      if (holeIndex == 16) {
        return 550;
      }
      break;
    case 3:
      if (holeIndex == 2) {
        return 300;
      }
      if (holeIndex == 4) {
        return 1000;
      }
      if (holeIndex == 9) {
        return 700;
      }
      if (holeIndex == 10) {
        return 400;
      }
      if (holeIndex == 11) {
        return 450;
      }
      if (holeIndex == 12) {
        return 400;
      }
      if (holeIndex == 13) {
        return 600;
      }
      if (holeIndex == 16) {
        return 900;
      }
      break;
    case 4:
      if (holeIndex == 10) {
        return 400;
      }
      if (holeIndex == 15) {
        return 300;
      }
      break;
    case 5:
      if (holeIndex == 15) {
        return 400;
      }
      if (holeIndex == 16) {
        return 700;
      }
      if (holeIndex == 17) {
        return 400;
      }
      break;
    case COURSE_RECORD_LUIGIS_GARDEN:
    case COURSE_RECORD_PEACHS_CASTLE:
      return MINI_GOLF_HOLE_RADIUS;
  }
  return DEFAULT_HOLE_RADIUS;
}

s32 GetCourseRecordIndex(void) {
  if (g_GameMode == GAME_MODE_RING_SHOT) {
    return g_RingShotHoles[g_CourseIndex][g_HoleIndex].courseRecordIndex;
  }
  if (g_IsMiniGolf) {
    return (((g_CourseIndex - MINI_GOLF_COURSE_INDEX) /
                MINI_GOLF_COURSES_PER_RECORD) &
               1) |
        COURSE_RECORD_LUIGIS_GARDEN;
  }
  if (g_CourseIndex >= DRIVING_RANGE_COURSE_INDEX) {
    return COURSE_RECORD_DRIVING_RANGE;
  }
  return g_CourseIndex;
}

s32 GetHoleRecordIndex(void) {
  if (g_GameMode == GAME_MODE_RING_SHOT) {
    return g_RingShotHoles[g_CourseIndex][g_HoleIndex].holeIndex;
  }
  if (g_CourseIndex >= DRIVING_RANGE_COURSE_INDEX) {
    return 0;
  }
  return g_HoleIndex;
}

/* GetHoleRecordIndex as if outside Ring Shot */
s32 GetHoleIndexInRecord(void) {
  if (g_CourseIndex >= DRIVING_RANGE_COURSE_INDEX) {
    return 0;
  }
  return g_HoleIndex;
}

/* With g_IsDebugEnabled set, the first hole counts too */
s32 IsMarkedParFive(s32 courseRecordIndex, s32 holeIndex) {
  if (g_IsMiniGolf) {
    return FALSE;
  }
  if (g_IsDebugEnabled && holeIndex == 0) {
    return TRUE;
  }
  return g_CourseRecords[courseRecordIndex].holes[holeIndex].flags &
      HOLE_FLAG_MARKED_PAR_FIVE;
}

s32 IsParThree(s32 courseRecordIndex, s32 holeIndex) {
  if (g_IsMiniGolf) {
    return FALSE;
  }
  return g_CourseRecords[courseRecordIndex].holes[holeIndex].flags &
      HOLE_FLAG_PAR_THREE;
}
