#include "common.h"

extern u8 D_80451A60[];

void *func_80400000_hole_editor(void) {
    return D_80451A60;
}

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040000C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804002C0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80400FBC_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80402428_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80402E1C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80403A04_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804045C0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80404850_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80404B04_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80404B84_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040582C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405870_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405AF4_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405B80_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405C94_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80405DB0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406570_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804066C8_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406818_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80406C3C_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804077B0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804079B8_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80407D64_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80407EC4_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80407F08_hole_editor);

/*
 * Doesn't match: the original opens with an 8-byte frame and stores an unset
 * $v0 into it, and it clears the flag only after loading ob[2].
 */
#ifdef NON_MATCHING
void GetMidpointVertex(Vtx_t *first, Vtx_t *second, Vtx_t *midpoint) {
    s32 unused;

    midpoint->ob[0] = (first->ob[0] + second->ob[0]) / 2;
    midpoint->ob[1] = (first->ob[1] + second->ob[1]) / 2;
    midpoint->flag = 0;
    midpoint->ob[2] = (first->ob[2] + second->ob[2]) / 2;
    midpoint->tc[0] = (first->tc[0] + second->tc[0]) / 2;
    midpoint->tc[1] = (first->tc[1] + second->tc[1]) / 2;
    midpoint->cn[0] = (first->cn[0] + second->cn[0]) / 2;
    midpoint->cn[1] = (first->cn[1] + second->cn[1]) / 2;
    midpoint->cn[2] = (first->cn[2] + second->cn[2]) / 2;
    midpoint->cn[3] = (first->cn[3] + second->cn[3]) / 2;
}
#else
INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", GetMidpointVertex);
#endif

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_80408170_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_804083E0_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040BF64_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040BFC8_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040C124_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040C260_hole_editor);

INCLUDE_ASM("asm/nonmatchings/hole_editor/hole_editor", func_8040C360_hole_editor);
