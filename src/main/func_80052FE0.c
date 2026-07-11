#include "common.h"

extern s32 func_80054550(s32 id, s32 tag, s32 arg2);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_80052FE0);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800530FC);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_8005342C);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", calculate_bone_matrices);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", clear_animation_slot);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800542A0);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_80054310);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", get_character_state);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800543A4);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800543DC);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_800544B4);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_80054550);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_8005470C);

INCLUDE_ASM("asm/nonmatchings/main/func_80052FE0", func_8005483C);

s32 func_800548DC(s32 arg0, s32 arg1) { return func_80054550(arg0, 2, arg1); }
