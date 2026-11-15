/*
 * Loads a hole for the tests as the loader thread does: the hole builder
 * decodes the hole's ground and points from the asset archive in ROM, and
 * shades the ground for the scene's light.
 */

#ifndef HOLE_FIXTURE_H
#define HOLE_FIXTURE_H

#include <ultra64.h>

#include "course/course.h"
#include "system/c_language.h"

// Load holeIndex of courseIndex in stroke play, in the round start's light,
// and set its default cup and the tee as a hole's conditions do. Returns
// whether the hole builder's overlay could be loaded; it is unloaded again.
// The game flags stay as the caller set them; GAME_FLAG_UNSHADED_GROUND
// skips the shading, which takes most of a load.
Boolean LoadTestHole(CourseIndex courseIndex, s32 holeIndex);

// The FNV-1a hash of size bytes at data, continuing from hash; start from
// FNV_OFFSET_BASIS. The tests that characterize a large state, such as the
// ground, compare its hash with the one the matching build gives.
#define FNV_OFFSET_BASIS 0x811C9DC5
u32 HashBytes(u32 hash, const void *data, u32 size);

// The hashes of the ground's height and surface at a grid of points over
// the loaded hole, GROUND_POINT_COLUMN_COUNT by GROUND_POINT_ROW_COUNT, and
// over the mini-golf ground's smaller square in mini golf
u32 HashGroundHeights(void);
u32 HashGroundSurfaces(void);

#endif
