/*
 * Push the pending VI context into the hardware registers (called at retrace).
 *
 * Resolves the staged update flags into the field-specific register values,
 * writes every VI register, then swaps the current and next contexts so the
 * just-programmed one becomes current.
 */

#include "PR/os_internal.h"
#include "PR/rcp.h"
#include "viint.h"

#ident "$Revision: 1.17 $"

void __osViSwapContext(void) {
  register OSViMode* vm;
  register __OSViContext* vc;
  u32 origin;
  u32 hStart;
#if BUILD_VERSION >= VERSION_J
  u32 vStart;
#endif
  u32 nomValue;
  u32 field;

  field = 0;
  vc = __osViNext;
  vm = vc->modep;

  // The VI alternates even/odd fields each frame; the low CURRENT bit selects
  // which field's register set to program.
  field = IO_READ(VI_CURRENT_REG) & 1;
  origin = osVirtualToPhysical(vc->framep) + (vm->fldRegs[field].origin);

  // X scale: keep the caller's staged value but re-merge the mode's subpixel
  // bits; otherwise take the mode default.
  if (vc->state & VI_STATE_XSCALE_UPDATED) {
    vc->x.scale |= (vm->comRegs.xScale & ~VI_SCALE_MASK);
  } else {
    vc->x.scale = vm->comRegs.xScale;
  }

  // Y scale: when set via osViSetYScale, multiply the mode's nominal scale by
  // the factor; otherwise take the mode default.
  if (vc->state & VI_STATE_YSCALE_UPDATED) {
    nomValue = vm->fldRegs[field].yScale & VI_SCALE_MASK;
    vc->y.scale = vc->y.factor * nomValue;
    vc->y.scale |= vm->fldRegs[field].yScale & ~VI_SCALE_MASK;
  } else {
    vc->y.scale = vm->fldRegs[field].yScale;
  }

#if BUILD_VERSION >= VERSION_J
  // Shift the active region down by the extra-scanline count (FPAL).
  vStart =
      (vm->fldRegs[field].vStart - (__additional_scanline << VI_SUBPIXEL_SH)) +
      __additional_scanline;
#endif

  hStart = vm->comRegs.hStart;

  // Black: suppress horizontal sync so nothing is displayed.
  if (vc->state & VI_STATE_BLACK) {
    hStart = 0;
  }

  // Repeat-line: tile the first frame-buffer line over the whole screen by
  // zeroing the y scale and pointing origin at the buffer start.
  if (vc->state & VI_STATE_REPEATLINE) {
    vc->y.scale = 0;
    origin = osVirtualToPhysical(vc->framep);
  }

  // Fade: scroll the display vertically by encoding the offset into the y scale
  // fractional field.
  if (vc->state & VI_STATE_FADE) {
    vc->y.scale = (vc->y.offset << VI_SUBPIXEL_SH) &
                  (VI_2_10_FPART_MASK << VI_SUBPIXEL_SH);
    origin = osVirtualToPhysical(vc->framep);
  }

  // Commit the resolved values to the VI hardware registers.
  IO_WRITE(VI_ORIGIN_REG, origin);
  IO_WRITE(VI_WIDTH_REG, vm->comRegs.width);
  IO_WRITE(VI_BURST_REG, vm->comRegs.burst);
  IO_WRITE(VI_V_SYNC_REG, vm->comRegs.vSync);
  IO_WRITE(VI_H_SYNC_REG, vm->comRegs.hSync);
  IO_WRITE(VI_LEAP_REG, vm->comRegs.leap);
  IO_WRITE(VI_H_START_REG, hStart);
#if BUILD_VERSION >= VERSION_J
  IO_WRITE(VI_V_START_REG, vStart);
#else
  IO_WRITE(VI_V_START_REG, vm->fldRegs[field].vStart);
#endif
  IO_WRITE(VI_V_BURST_REG, vm->fldRegs[field].vBurst);
  IO_WRITE(VI_INTR_REG, vm->fldRegs[field].vIntr);
  IO_WRITE(VI_X_SCALE_REG, vc->x.scale);
  IO_WRITE(VI_Y_SCALE_REG, vc->y.scale);
  IO_WRITE(VI_CONTROL_REG, vc->control);

  // Swap the buffers: the context just programmed becomes current, and the old
  // current is reused as next, seeded with a copy of the new current.
  __osViNext = __osViCurr;
  __osViCurr = vc;
  *__osViNext = *__osViCurr;
}
