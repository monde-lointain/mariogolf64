#ifndef SEGMENT_SYMBOLS_H
#define SEGMENT_SYMBOLS_H

#include <ultra64.h>

#define DECLARE_SEGMENT(name) \
    extern u8 name##_ROM_START[]; \
    extern u8 name##_ROM_END[]; \
    extern u8 name##_VRAM[]; \
    extern u8 name##_TEXT_END[]; \
    extern u8 name##_BSS_START[]; \
    extern u8 name##_BSS_END[]

DECLARE_SEGMENT(main);
DECLARE_SEGMENT(build_date);
DECLARE_SEGMENT(ovl_00);
DECLARE_SEGMENT(ovl_01);
DECLARE_SEGMENT(ovl_02);
DECLARE_SEGMENT(ovl_03);
DECLARE_SEGMENT(ovl_04);
DECLARE_SEGMENT(ovl_05);
DECLARE_SEGMENT(ovl_06);
DECLARE_SEGMENT(ovl_07);
DECLARE_SEGMENT(ovl_08);
DECLARE_SEGMENT(ovl_09);
DECLARE_SEGMENT(ovl_10);
DECLARE_SEGMENT(ovl_11);
DECLARE_SEGMENT(ovl_12);
DECLARE_SEGMENT(ovl_13);
DECLARE_SEGMENT(ovl_14);

#endif
