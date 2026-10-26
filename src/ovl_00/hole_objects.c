#include "common.h"
#include "camera.h"
#include "math_util.h"

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020D6F0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020D784_ovl_00);

/* The yaw the camera looks along, or 0 when it looks straight up or down */
f32 GetCameraYaw(void) {
    f32 x = g_Cameras->view.lookAt[0] - g_Cameras->view.eye[0];
    f32 yaw = 0.0f;
    f32 z = g_Cameras->view.lookAt[2] - g_Cameras->view.eye[2];

    if ((s32)x != 0 || (s32)z != 0) {
        yaw = atan2f(x, z);
    }
    return yaw;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020D838_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020D874_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020D9F0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020DA18_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/hole_objects", func_8020DCA4_ovl_00);
