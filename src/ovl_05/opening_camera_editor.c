#include "common.h"

#include "math_util.h"

/* The points of the camera that the editor moves */
#define EDITOR_POINT_EYE 0
#define EDITOR_POINT_LOOK_AT 1
#define EDITOR_POINT_COUNT 2

/* The axis along which func_8020370C moves a point furthest */
#define EDITOR_MOVE_AXIS_X 1
#define EDITOR_MOVE_AXIS_Y 2
#define EDITOR_MOVE_AXIS_Z 3

/* The state of the opening's camera editor, starting with the camera it moves
 */
typedef struct CameraEditor {
  /* 0x00 */ Vec3
      points[EDITOR_POINT_COUNT]; /* The eye, then the look-at point */
  /* 0x18 */ f32
      heading; /* In radians, of the look direction across the ground */
  /* 0x1C */ f32 pitch; /* In radians, of the look direction above the ground */
  /* 0x20 */ f32 distance; /* From the eye to the look-at point */
  /* 0x24 */ f32 unk24;
  /* 0x28 */ Vec3
      targets[EDITOR_POINT_COUNT]; /* Where func_8020370C moves the points to */
  /* 0x40 */ Vec3 unk40[2];
  /* 0x58 */ f32 unk58;
  /* 0x5C */ f32 unk5C;
  /* 0x60 */ f32 unk60;
  /* 0x64 */ f32 unk64;
  /* 0x68 */ f32 unk68;
  /* 0x6C */ f32 unk6C;
  /* 0x70 */ f32 unk70[2];
  /* 0x78 */ f32 unk78[2];
  /* 0x80 */ f32 unk80;
  /* 0x84 */ f32 unk84;
  /* 0x88 */ f32 unk88;
  /* 0x8C */ f32 unk8C;
  /* 0x90 */ f32 unk90;
  /* 0x94 */ f32 unk94;
  /* 0x98 */ f32 unk98[2];
  /* 0xA0 */ f32 unkA0;
  /* 0xA4 */ f32 unkA4;
  /* 0xA8 */ f32 unkA8;
  /* 0xAC */ s32 flags[2]; /* EDITOR_FLAG_* bits */
  /* 0xB4 */ s32 unkB4[2];
} CameraEditor; /* size = 0xBC */

/* CameraEditor flags bits */
#define EDITOR_FLAG_UNK2 0x2
#define EDITOR_FLAG_UNK4 0x4
#define EDITOR_FLAG_UNK8 0x8
#define EDITOR_FLAG_UNK10 0x10
#define EDITOR_FLAG_UNK20 0x20
#define EDITOR_FLAG_UNK40 0x40
#define EDITOR_FLAG_UNK80 0x80

extern CameraEditor g_CameraEditor;
extern f32 D_8020AE3C_ovl_05; /* A scale that the opening passes to guScale */

void func_80203620_ovl_05(void);
void func_8020370C_ovl_05(s32 index, f32 x, f32 y, f32 z, s32 frameCount);

/* Starts the editor with its eye at (0, -100, 100) looking at the origin, and
 * its moves stopped */
void func_802033B0_ovl_05(void) {
  s32 i;

  g_CameraEditor.points[EDITOR_POINT_EYE][0] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] = 0.0f;
  g_CameraEditor.points[EDITOR_POINT_EYE][1] = -100.0f;
  g_CameraEditor.points[EDITOR_POINT_EYE][2] = 100.0f;
  g_CameraEditor.unk24 = 45.0f;
  for (i = 0; i != EDITOR_POINT_COUNT; i++) {
    g_CameraEditor.targets[i][0] = 0.0f;
    g_CameraEditor.targets[i][1] = 0.0f;
    g_CameraEditor.targets[i][2] = 0.0f;
    g_CameraEditor.unk40[i][0] = 0.0f;
    g_CameraEditor.unk40[i][1] = 0.0f;
    g_CameraEditor.unk40[i][2] = 0.0f;
    /* The game clears these twice */
    g_CameraEditor.unk40[i][0] = 0.0f;
    g_CameraEditor.unk40[i][1] = 0.0f;
    g_CameraEditor.unk40[i][2] = 0.0f;
    g_CameraEditor.unk70[i] = 30.0f;
    g_CameraEditor.unk78[i] = 3.0f;
    g_CameraEditor.unk98[i] = 0.0f;
    g_CameraEditor.unkB4[i] = 0;
    g_CameraEditor.flags[i] = 0;
  }
  g_CameraEditor.unkA0 = 0.0f;
  g_CameraEditor.unkA4 = 0.0f;
  g_CameraEditor.unkA8 = 0.0f;
  g_CameraEditor.unk80 = DEGREES_TO_RADIANS(6.0f);
  g_CameraEditor.unk84 = DEGREES_TO_RADIANS(2.0f);
  g_CameraEditor.unk88 = DEGREES_TO_RADIANS(3.0f);
  g_CameraEditor.unk8C = DEGREES_TO_RADIANS(1.0f);
  g_CameraEditor.unk90 = 10.0f;
  g_CameraEditor.unk94 = 1.0f;
  func_80203620_ovl_05();
  D_8020AE3C_ovl_05 = 1.0f;
}

void func_8020356C_ovl_05(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4,
                          f32 arg5) {
  g_CameraEditor.points[EDITOR_POINT_EYE][0] = arg0;
  g_CameraEditor.points[EDITOR_POINT_EYE][1] = arg1;
  g_CameraEditor.points[EDITOR_POINT_EYE][2] = arg2;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] = arg3;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] = arg4;
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] = arg5;
  func_80203620_ovl_05();
}

/* Sets the speeds of point index's moves. Nothing calls it */
void func_802035BC_ovl_05(s32 index, f32 speed, f32 easeSpeed) {
  g_CameraEditor.unk70[index] = speed;
  g_CameraEditor.unk78[index] = easeSpeed;
}

void func_802035E4_ovl_05(s32 arg0, f32 arg1, f32 arg2) {
  g_CameraEditor.unk80 = arg1;
  g_CameraEditor.unk84 = arg2;
}

void func_802035F8_ovl_05(s32 arg0, f32 arg1, f32 arg2) {
  g_CameraEditor.unk88 = arg1;
  g_CameraEditor.unk8C = arg2;
}

void func_8020360C_ovl_05(s32 arg0, f32 arg1, f32 arg2) {
  g_CameraEditor.unk90 = arg1;
  g_CameraEditor.unk94 = arg2;
}

/* Works out the camera's heading, pitch and distance from its eye and look-at
 * point */
void func_80203620_ovl_05(void) {
  f32 offsetX;
  f32 offsetY;
  f32 offsetZ;

  offsetX = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
            g_CameraEditor.points[EDITOR_POINT_EYE][0];
  g_CameraEditor.pitch =
      atan2f(g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] -
                 g_CameraEditor.points[EDITOR_POINT_EYE][1],
             sqrtf(offsetX * offsetX +
                   (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
                    g_CameraEditor.points[EDITOR_POINT_EYE][2]) *
                       (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
                        g_CameraEditor.points[EDITOR_POINT_EYE][2])));
  g_CameraEditor.heading =
      atan2f(g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
                 g_CameraEditor.points[EDITOR_POINT_EYE][2],
             g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
                 g_CameraEditor.points[EDITOR_POINT_EYE][0]);
  offsetY = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] -
            g_CameraEditor.points[EDITOR_POINT_EYE][1];
  offsetZ = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
            g_CameraEditor.points[EDITOR_POINT_EYE][2];
  g_CameraEditor.distance =
      sqrtf((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
             g_CameraEditor.points[EDITOR_POINT_EYE][0]) *
                (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] -
                 g_CameraEditor.points[EDITOR_POINT_EYE][0]) +
            offsetY * offsetY + offsetZ * offsetZ);
}

/* Starts moving point index to (x, y, z) over frameCount frames, or snaps it
 * there when it is close */
void func_8020370C_ovl_05(s32 index, f32 x, f32 y, f32 z, s32 frameCount) {
  f32 distance;
  s32 easeFrameCount;
  f32 speed;

  distance = sqrtf((x - g_CameraEditor.points[index][0]) *
                       (x - g_CameraEditor.points[index][0]) +
                   (y - g_CameraEditor.points[index][1]) *
                       (y - g_CameraEditor.points[index][1]) +
                   (z - g_CameraEditor.points[index][2]) *
                       (z - g_CameraEditor.points[index][2]));
  if (distance < 0.1f) {
    g_CameraEditor.points[index][0] = x;
    g_CameraEditor.points[index][1] = y;
    g_CameraEditor.points[index][2] = z;
    return;
  }
  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = distance / (frameCount - easeFrameCount);
    g_CameraEditor.unk70[index] = speed;
    g_CameraEditor.unk78[index] = speed / (easeFrameCount + 1);
    g_CameraEditor.unk98[index] = distance * 0.25;
    if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK80)) {
      if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK2)) {
        g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK2;
      }
      if (g_CameraEditor.flags[index] & EDITOR_FLAG_UNK4) {
        g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK4;
      }
    }
  } else {
    if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK4)) {
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK4;
    }
    if (g_CameraEditor.flags[index] & EDITOR_FLAG_UNK2) {
      g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK2;
    }
  }
  g_CameraEditor.targets[index][0] = x;
  g_CameraEditor.targets[index][1] = y;
  g_CameraEditor.targets[index][2] = z;
  /* The arguments become the offsets, and x the larger of the ground ones */
  x -= g_CameraEditor.points[index][0];
  y -= g_CameraEditor.points[index][1];
  z -= g_CameraEditor.points[index][2];
  g_CameraEditor.unkB4[index] = EDITOR_MOVE_AXIS_X;
  if (fabsf(x) < fabsf(z)) {
    g_CameraEditor.unkB4[index] = EDITOR_MOVE_AXIS_Z;
    x = z;
  }
  if (fabsf(x) < fabsf(y)) {
    g_CameraEditor.unkB4[index] = EDITOR_MOVE_AXIS_Y;
  }
}

/* Sets two flags in entry index and clears them in the other before calling
 * func_8020370C */
void func_80203984_ovl_05(s32 index, f32 x, f32 y, f32 z, s32 frameCount) {
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK80)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK80;
  }
  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK80) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK80;
  }
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK8)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK8) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK8;
  }
  func_8020370C_ovl_05(index, x, y, z, frameCount);
}

/* Starts turning the camera's heading by angle over frameCount frames, or turns
 * it at once when angle is tiny */
/* Doesn't match: the original loads the threshold after taking the absolute
 * value; GCC schedules it first */
#ifdef NON_MATCHING
void func_80203A30_ovl_05(s32 index, f32 angle, s32 frameCount) {
  s32 easeFrameCount;
  f32 speed;

  if (fabsf(angle) < 0.001f) {
    g_CameraEditor.heading += angle;
    return;
  }
  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = angle / (frameCount - easeFrameCount);
    g_CameraEditor.unkA4 = angle * 0.25;
    g_CameraEditor.unk80 = speed;
    g_CameraEditor.unk84 = speed / (easeFrameCount + 1);
  }
  g_CameraEditor.unk68 = angle;
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK8)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK8) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index] & EDITOR_FLAG_UNK10) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK10;
  }
  if (!(g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK10)) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK10;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203A30_ovl_05);
#endif

/* Starts turning the camera's pitch by angle over frameCount frames, or turns
 * it at once when angle is tiny */
/* Doesn't match: the original loads the threshold after taking the absolute
 * value; GCC schedules it first */
#ifdef NON_MATCHING
void func_80203B60_ovl_05(s32 index, f32 angle, s32 frameCount) {
  s32 easeFrameCount;
  f32 speed;

  if (fabsf(angle) < 0.001f) {
    g_CameraEditor.pitch += angle;
    return;
  }
  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = angle / (frameCount - easeFrameCount);
    g_CameraEditor.unkA0 = angle * 0.25;
    g_CameraEditor.unk88 = speed;
    g_CameraEditor.unk8C = speed / (easeFrameCount + 1);
  }
  g_CameraEditor.unk64 = angle;
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK8)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK8) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index] & EDITOR_FLAG_UNK20) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK20;
  }
  if (!(g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK20)) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK20;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203B60_ovl_05);
#endif

/* Starts moving the camera to distance over frameCount frames, or moves it at
 * once when it is close */
/* Doesn't match: the original loads the threshold after taking the absolute
 * value; GCC schedules it first */
#ifdef NON_MATCHING
void func_80203C90_ovl_05(s32 index, f32 distance, s32 frameCount) {
  s32 easeFrameCount;
  f32 change;
  f32 speed;

  change = distance - g_CameraEditor.distance;
  if (fabsf(change) < 0.1f) {
    g_CameraEditor.distance = distance;
    return;
  }
  if (frameCount != 0) {
    easeFrameCount = frameCount / 4;
    speed = change / (frameCount - easeFrameCount);
    g_CameraEditor.unk90 = speed;
    g_CameraEditor.unk94 = speed / (easeFrameCount + 1);
    g_CameraEditor.unkA8 = fabsf(change) * 0.25;
  }
  g_CameraEditor.unk6C = change;
  if (!(g_CameraEditor.flags[index] & EDITOR_FLAG_UNK8)) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK8) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK8;
  }
  if (g_CameraEditor.flags[index] & EDITOR_FLAG_UNK40) {
    g_CameraEditor.flags[index] ^= EDITOR_FLAG_UNK40;
  }
  if (!(g_CameraEditor.flags[index ^ 1] & EDITOR_FLAG_UNK40)) {
    g_CameraEditor.flags[index ^ 1] ^= EDITOR_FLAG_UNK40;
  }
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203C90_ovl_05);
#endif

s32 func_80203DC4_ovl_05(void) {
  if (g_CameraEditor.flags[0] == 0 && g_CameraEditor.flags[1] == 0) {
    return TRUE;
  }
  return FALSE;
}

s32 func_80203DE8_ovl_05(s32 mask) {
  if (!(g_CameraEditor.flags[0] & mask) && !(g_CameraEditor.flags[1] & mask)) {
    return TRUE;
  }
  return FALSE;
}

/* Stops the editor's moves and puts its speeds back to their starting values */
void func_80203E14_ovl_05(void) {
  s32 i;

  for (i = 0; i != EDITOR_POINT_COUNT; i++) {
    g_CameraEditor.targets[i][0] = 0.0f;
    g_CameraEditor.targets[i][1] = 0.0f;
    g_CameraEditor.targets[i][2] = 0.0f;
    g_CameraEditor.unk40[i][0] = 0.0f;
    g_CameraEditor.unk40[i][1] = 0.0f;
    g_CameraEditor.unk40[i][2] = 0.0f;
    /* The game clears these twice */
    g_CameraEditor.unk40[i][0] = 0.0f;
    g_CameraEditor.unk40[i][1] = 0.0f;
    g_CameraEditor.unk40[i][2] = 0.0f;
    g_CameraEditor.unk70[i] = 30.0f;
    g_CameraEditor.unk78[i] = 3.0f;
    g_CameraEditor.unk98[i] = 0.0f;
    g_CameraEditor.unkB4[i] = 0;
    g_CameraEditor.flags[i] = 0;
  }
  g_CameraEditor.unkA0 = 0.0f;
  g_CameraEditor.unkA4 = 0.0f;
  g_CameraEditor.unkA8 = 0.0f;
  g_CameraEditor.unk80 = DEGREES_TO_RADIANS(6.0f);
  g_CameraEditor.unk84 = DEGREES_TO_RADIANS(2.0f);
  g_CameraEditor.unk88 = DEGREES_TO_RADIANS(3.0f);
  g_CameraEditor.unk8C = DEGREES_TO_RADIANS(1.0f);
  g_CameraEditor.unk90 = 10.0f;
  g_CameraEditor.unk94 = 1.0f;
  func_80203620_ovl_05();
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203F70_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80204EA8_ovl_05);

/* Moves the whole camera by offset */
void func_802065FC_ovl_05(Vec3 offset) {
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] += offset[0];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] += offset[1];
  g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] += offset[2];
  g_CameraEditor.points[EDITOR_POINT_EYE][0] += offset[0];
  g_CameraEditor.points[EDITOR_POINT_EYE][1] += offset[1];
  g_CameraEditor.points[EDITOR_POINT_EYE][2] += offset[2];
}

/* Ground kind names in half-width katakana; the data block points at the last
 * four */
const char D_8020C190_ovl_05[] = "ﾚｯﾄﾞ";
const char D_8020C198_ovl_05[] = "ﾌﾞﾙｰ";
const char D_8020C1A0_ovl_05[] = "ｸﾞﾘｰﾝ";
const char D_8020C1A8_ovl_05[] = "TEE UP";
const char D_8020C1B0_ovl_05[] = "ｱｳﾄ ｵﾌﾞ ｺｰｽ";
const char D_8020C1BC_ovl_05[] = "            ";
const char D_8020C1CC_ovl_05[] = "ｳｴｽﾄｴﾘｱ";
const char D_8020C1D4_ovl_05[] = "ﾃﾞｨｰﾌﾟﾗﾌ";
const char D_8020C1E0_ovl_05[] = "ﾍﾞｱｸﾞﾗｳﾝﾄﾞ";
const char D_8020C1EC_ovl_05[] = "ｾﾐﾗﾌ";
const char D_8020C1F4_ovl_05[] = "ｸﾞﾘｰﾝ ｴｯｼﾞ";
const char D_8020C200_ovl_05[] = "ﾛｯｸ";
const char D_8020C204_ovl_05[] = "ﾃｨｰｸﾞﾗｳﾝﾄﾞ";
const char D_8020C210_ovl_05[] = "ｶｰﾄｳｪｲ";
const char D_8020C218_ovl_05[] = "ｳｫｰﾀｰﾊｻﾞｰﾄﾞ";
const char D_8020C224_ovl_05[] = "ｵﾝ ｸﾞﾘｰﾝ";
const char D_8020C230_ovl_05[] = "ﾊﾞﾝｶｰ";
const char D_8020C238_ovl_05[] = "ﾗﾌ";
const char D_8020C23C_ovl_05[] = "ﾌｪｱｳｪｲ";
const char D_8020C244_ovl_05[] = "O.B.";
