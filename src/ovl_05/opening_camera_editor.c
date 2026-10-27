#include "common.h"
#include "math_util.h"

/* The points of the camera that the editor moves */
#define EDITOR_POINT_EYE 0
#define EDITOR_POINT_LOOK_AT 1
#define EDITOR_POINT_COUNT 2

/* The state of the opening's camera editor, starting with the camera it moves */
typedef struct CameraEditor {
    /* 0x00 */ Vec3 points[EDITOR_POINT_COUNT]; /* The eye, then the look-at point */
    /* 0x18 */ f32 heading;  /* In radians, of the look direction across the ground */
    /* 0x1C */ f32 pitch;    /* In radians, of the look direction above the ground */
    /* 0x20 */ f32 distance; /* From the eye to the look-at point */
    /* 0x24 */ f32 unk24;
    /* 0x28 */ Vec3 targets[EDITOR_POINT_COUNT]; /* Where func_8020370C moves the points to */
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
#define EDITOR_FLAG_UNK8 0x8
#define EDITOR_FLAG_UNK80 0x80

extern CameraEditor g_CameraEditor;

void func_80203620_ovl_05(void);
void func_8020370C_ovl_05(s32 index, f32 x, f32 y, f32 z, s32 frameCount);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_802033B0_ovl_05);

void func_8020356C_ovl_05(f32 arg0, f32 arg1, f32 arg2, f32 arg3, f32 arg4, f32 arg5) {
    g_CameraEditor.points[EDITOR_POINT_EYE][0] = arg0;
    g_CameraEditor.points[EDITOR_POINT_EYE][1] = arg1;
    g_CameraEditor.points[EDITOR_POINT_EYE][2] = arg2;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] = arg3;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] = arg4;
    g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] = arg5;
    func_80203620_ovl_05();
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_802035BC_ovl_05);

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

/* Works out the camera's heading, pitch and distance from its eye and look-at point */
void func_80203620_ovl_05(void) {
    f32 offsetX;
    f32 offsetY;
    f32 offsetZ;

    offsetX = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] - g_CameraEditor.points[EDITOR_POINT_EYE][0];
    g_CameraEditor.pitch =
        atan2f(g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] - g_CameraEditor.points[EDITOR_POINT_EYE][1],
               sqrtf(offsetX * offsetX +
                     (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] - g_CameraEditor.points[EDITOR_POINT_EYE][2]) *
                         (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] -
                          g_CameraEditor.points[EDITOR_POINT_EYE][2])));
    g_CameraEditor.heading =
        atan2f(g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] - g_CameraEditor.points[EDITOR_POINT_EYE][2],
               g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] - g_CameraEditor.points[EDITOR_POINT_EYE][0]);
    offsetY = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][1] - g_CameraEditor.points[EDITOR_POINT_EYE][1];
    offsetZ = g_CameraEditor.points[EDITOR_POINT_LOOK_AT][2] - g_CameraEditor.points[EDITOR_POINT_EYE][2];
    g_CameraEditor.distance =
        sqrtf((g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] - g_CameraEditor.points[EDITOR_POINT_EYE][0]) *
                  (g_CameraEditor.points[EDITOR_POINT_LOOK_AT][0] - g_CameraEditor.points[EDITOR_POINT_EYE][0]) +
              offsetY * offsetY + offsetZ * offsetZ);
}

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_8020370C_ovl_05);

/* Sets two flags in entry index and clears them in the other before calling func_8020370C */
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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203A30_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203B60_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203C90_ovl_05);

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

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203E14_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80203F70_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_80204EA8_ovl_05);

INCLUDE_ASM("asm/nonmatchings/ovl_05/opening_camera_editor", func_802065FC_ovl_05);

/* Ground kind names in half-width katakana; the data block points at the last four */
const char D_8020C190_ovl_05[] = "\332\257\304\336";
const char D_8020C198_ovl_05[] = "\314\336\331\260";
const char D_8020C1A0_ovl_05[] = "\270\336\330\260\335";
const char D_8020C1A8_ovl_05[] = "TEE UP";
const char D_8020C1B0_ovl_05[] = "\261\263\304 \265\314\336 \272\260\275";
const char D_8020C1BC_ovl_05[] = "            ";
const char D_8020C1CC_ovl_05[] = "\263\264\275\304\264\330\261";
const char D_8020C1D4_ovl_05[] = "\303\336\250\260\314\337\327\314";
const char D_8020C1E0_ovl_05[] = "\315\336\261\270\336\327\263\335\304\336";
const char D_8020C1EC_ovl_05[] = "\276\320\327\314";
const char D_8020C1F4_ovl_05[] = "\270\336\330\260\335 \264\257\274\336";
const char D_8020C200_ovl_05[] = "\333\257\270";
const char D_8020C204_ovl_05[] = "\303\250\260\270\336\327\263\335\304\336";
const char D_8020C210_ovl_05[] = "\266\260\304\263\252\262";
const char D_8020C218_ovl_05[] = "\263\253\260\300\260\312\273\336\260\304\336";
const char D_8020C224_ovl_05[] = "\265\335 \270\336\330\260\335";
const char D_8020C230_ovl_05[] = "\312\336\335\266\260";
const char D_8020C238_ovl_05[] = "\327\314";
const char D_8020C23C_ovl_05[] = "\314\252\261\263\252\262";
const char D_8020C244_ovl_05[] = "O.B.";
