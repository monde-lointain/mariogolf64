/*
 * MPAL LAN1 video mode: the register table osViSetMode() loads to drive the
 * Video Interface for an MPAL display.
 *
 * MPAL (M-PAL, "PAL-M") is the Brazilian standard: PAL color encoding on the
 * NTSC 525-line/60Hz line rate. So this table carries NTSC-style line timing
 * (525 half-lines, the same field-region values) with MPAL-specific burst and
 * sync constants.
 *
 * The four-letter suffix encodes the framebuffer geometry:
 *   L = Low resolution (320 wide)
 *   A = Anti-aliased (filtered, vs. P = point-sampled)
 *   N = Non-interlaced (single field, vs. interlaced)
 *   1 = 16-bit color framebuffer (vs. 2 = 32-bit)
 *
 * Each entry is a packed VI register value built by a timing macro; the
 * trailing comment names the OSViMode field it initializes. The two fldRegs
 * entries are identical because the mode is non-interlaced.
 */
#include "PR/os.h"
#include "PR/rcp.h"
#include "viint.h"

// clang-format off: hand-aligned register table, one column of OSViMode field names
OSViMode osViModeMpalLan1 = {
    OS_VI_MPAL_LAN1,                                         // type
    {
        // comRegs: registers shared by both fields
        VI_CTRL_TYPE_16 | VI_CTRL_GAMMA_DITHER_ON | VI_CTRL_GAMMA_ON |
            VI_CTRL_DIVOT_ON | VI_CTRL_ANTIALIAS_MODE_1 | VI_CTRL_PIXEL_ADV_3,  // ctrl
        WIDTH(320),                                          // width
        BURST(57, 30, 5, 70),                                // burst
        VSYNC(525),                                          // vSync: 525 half-lines (NTSC line rate)
        HSYNC(3089, 4),                                      // hSync
        LEAP(3097, 3098),                                    // leap
        HSTART(108, 748),                                    // hStart: active-region horizontal start/end
        SCALE(2, 0),                                         // xScale: horizontal scale (1/2 -> 320 from 640)
        VCURRENT(0),                                         // vCurrent
    },
    {{
         // fldRegs[0]: field 1
         ORIGIN(640),                                        // origin: framebuffer stride in bytes
         SCALE(1, 0),                                        // yScale
         HSTART(37, 511),                                    // vStart: active-region vertical start/end
         BURST(4, 2, 14, 0),                                 // vBurst
         VINTR(2),                                           // vIntr: scanline that raises the VI interrupt
     },
     {
         // fldRegs[1]: field 2 (same as field 1; non-interlaced)
         ORIGIN(640),                                        // origin
         SCALE(1, 0),                                        // yScale
         HSTART(37, 511),                                    // vStart
         BURST(4, 2, 14, 0),                                 // vBurst
         VINTR(2),                                           // vIntr
     }},
};
// clang-format on
