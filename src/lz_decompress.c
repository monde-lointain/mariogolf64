#include "common.h"

#ifdef NONMATCHING
// m2c output, permuter-improved (score 2225)
s32 lz_decompress_simple(LzDecompressState *state) {
    s32 var_a2;
    u16 *temp_a1;
    u16 *var_a1;
    u16 temp_a2;
    u16 temp_v0;
    u16 temp_v0_2;
    u16 temp_v1;
    u16 temp_v1_2;
    u32 temp_v0_3;
    u8 *temp_t3;
    u8 *new_var;
    u8 *var_a3;
    u8 *var_t0;
    u8 *var_t1;
    u8 *var_t3;
    u8 *var_v0;
    u8 *var_v1;
    u8 *var_v1_2;

    if (state->flags & 1) {
        var_a3 = state->dst_start;
        var_t1 = state->src_cur + 4;
    } else {
        var_t1 = state->src_cur;
        var_a3 = state->dst_alt;
    }
    var_a2 = state->flags >> 1;
    temp_t3 = state->src_end;
    var_t0 = var_t1 + 0x1E;
loop_4:
    var_v0 = temp_t3 - 0x20;
loop_5:
    if (((u32)var_t1 >= (u32)var_v0) && ((var_a2 ^ 1) & 1)) {
        var_v1 = state->dst_cur;
        if ((u32)var_t1 < (u32)temp_t3) {
            var_t3 = temp_t3 - 2;
            do {
                var_v1 -= 2;
                *(u16*)var_v1 = *(u16*)var_t3;
                var_t3 -= 2;
            } while ((u32)var_t1 < (u32)var_t3);
        }
        state->src_cur = var_v1;
        state->dst_alt = var_a3;
        return -1;
    }
    temp_a2 = *(u16*)var_t1;
    var_t0 += 2;
    var_t1 += 2;
    if (temp_a2 == 0) {
        ((u16*)var_a3)[0] = *(u16*)var_t1;
        ((u16*)var_a3)[1] = ((u16*)var_t0)[-0xE];
        ((u16*)var_a3)[2] = ((u16*)var_t0)[-0xD];
        ((u16*)var_a3)[3] = ((u16*)var_t0)[-0xC];
        ((u16*)var_a3)[6] = ((u16*)var_t0)[-0x9];
        ((u16*)var_a3)[4] = ((u16*)var_t0)[-0xB];
        ((u16*)var_a3)[5] = ((u16*)var_t0)[-0xA];
        ((u16*)var_a3)[7] = ((u16*)var_t0)[-0x8];
        ((u16*)var_a3)[8] = ((u16*)var_t0)[-0x7];
        ((u16*)var_a3)[9] = ((u16*)var_t0)[-0x6];
        ((u16*)var_a3)[10] = ((u16*)var_t0)[-0x5];
        ((u16*)var_a3)[11] = ((u16*)var_t0)[-0x4];
        ((u16*)var_a3)[12] = ((u16*)var_t0)[-0x3];
        temp_v0 = ((u16*)var_t0)[-1];
        ((u16*)var_a3)[13] = ((u16*)var_t0)[-2];
        temp_v1 = ((u16*)var_t0)[0];
        var_t1 += 0x20;
        var_t0 += 0x20;
        ((u16*)var_a3)[14] = temp_v0;
        ((u16*)var_a3)[15] = temp_v1;
        var_a3 += 0x20;
        goto loop_4;
    }
    var_a2 = (temp_a2 << 0x10) | 0x8000;
loop_16:
    if (var_a2 >= 0) {
        do {
            var_t0 += 2;
            temp_v0_2 = *(u16*)var_t1;
            var_t1 += 2;
            var_a2 *= 2;
            *(u16*)var_a3 = temp_v0_2;
            var_a3 += 2;
        } while (var_a2 >= 0);
    }
    var_a2 *= 2;
    var_v0 = temp_t3 - 0x20;
    if (var_a2 != 0) {
        temp_v1_2 = *(u16*)var_t1;
        new_var = var_a3 + (((temp_v1_2 & 0x1F) * 2) + 4);
        var_t0 += 2;
        temp_v0_3 = temp_v1_2 & 0xFFFF;
        var_t1 += 2;
        if (temp_v0_3 != 0) {
            temp_a1 = (u16*)var_a3 - ((temp_v0_3 >> 5) * 2);
            var_a1 = temp_a1 + 2;
            var_v1_2 = var_a3 + 2;
            *(u16*)var_a3 = *temp_a1;
            do {
                *(u16*)var_v1_2 = *var_a1;
                var_v1_2 += 2;
                var_a1 += 2;
            } while (var_v1_2 != new_var);
            var_a3 = var_v1_2;
            goto loop_16;
        }
        return var_a3 - state->dst_start;
    }
    goto loop_5;
}
#else
INCLUDE_ASM("asm/nonmatchings/lz_decompress", lz_decompress_simple);
#endif

INCLUDE_ASM("asm/nonmatchings/lz_decompress", lz_decompress_extended);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", lz_decompress_dma);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", lz_decompress_extended_dma);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80068F00);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80068F18);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80068F4C);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80068F98);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_800690C0);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069124);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_8006955C);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_800695F8);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069BCC);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069CE4);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069D08);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069F28);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069F38);

INCLUDE_ASM("asm/nonmatchings/lz_decompress", func_80069FBC);
