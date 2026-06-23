/*
 * PAL LAN1 video mode: the register table osViSetMode() loads to drive the
 * Video Interface for a PAL display.
 *
 * The four-letter suffix encodes the framebuffer geometry:
 *   L = Low resolution (320 wide)
 *   A = Anti-aliased (filtered, vs. P = point-sampled)
 *   N = Non-interlaced (single field, vs. interlaced)
 *   1 = 16-bit color framebuffer (vs. 2 = 32-bit)
 *
 * PAL differs from NTSC in line count and burst/sync timing (625 half-lines per
 * frame vs. 525). Each entry is a packed VI register value built by a timing
 * macro; the trailing comment names the OSViMode field it initializes. The two
 * fldRegs entries are identical because the mode is non-interlaced.
 */
#include "PR/os.h"
#include "PR/rcp.h"
#include "viint.h"

// clang-format off: hand-aligned register table, one column of OSViMode field names
OSViMode osViModePalLan1 = {
    OS_VI_PAL_LAN1,                                          // type
    {
        // comRegs: registers shared by both fields
        VI_CTRL_TYPE_16 | VI_CTRL_GAMMA_DITHER_ON | VI_CTRL_GAMMA_ON |
            VI_CTRL_DIVOT_ON | VI_CTRL_ANTIALIAS_MODE_1 | VI_CTRL_PIXEL_ADV_3,  // ctrl
        WIDTH(320),                                          // width
        BURST(58, 30, 4, 69),                                // burst
        VSYNC(625),                                          // vSync: 625 half-lines per PAL frame
        HSYNC(3177, 23),                                     // hSync
        LEAP(3183, 3181),                                    // leap
        HSTART(128, 768),                                    // hStart: active-region horizontal start/end
        SCALE(2, 0),                                         // xScale: horizontal scale (1/2 -> 320 from 640)
        VCURRENT(0),                                         // vCurrent
    },
    {{
         // fldRegs[0]: field 1
         ORIGIN(640),                                        // origin: framebuffer stride in bytes
         SCALE(1, 0),                                        // yScale
         HSTART(95, 569),                                    // vStart: active-region vertical start/end
         BURST(107, 2, 9, 0),                                // vBurst
         VINTR(2),                                           // vIntr: scanline that raises the VI interrupt
     },
     {
         // fldRegs[1]: field 2 (same as field 1; non-interlaced)
         ORIGIN(640),                                        // origin
         SCALE(1, 0),                                        // yScale
         HSTART(95, 569),                                    // vStart
         BURST(107, 2, 9, 0),                                // vBurst
         VINTR(2),                                           // vIntr
     }},
};
// clang-format on
