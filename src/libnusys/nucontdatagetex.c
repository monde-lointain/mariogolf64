/*
 * Controller read with edge-triggered button bits.
 *
 * Returns the latest pad state for one port plus a derived "trigger" field that
 * marks the buttons newly pressed since the caller last read this pad. The
 * extra field is what distinguishes NUContData from the bare OSContPad.
 */
#include <nusys.h>

/*
 * Copy port padno's current state into contdata, filling in trigger bits.
 *
 * The caller's existing button word (the previous frame's held buttons) is
 * captured before the copy, then the freshly read buttons are ANDed with its
 * complement so trigger holds only the just-pressed edges. The copy runs inside
 * a lock/unlock pair so the snapshot is consistent against the retrace updater.
 */
void nuContDataGetEx(NUContData* contdata, u32 padno) {
  u16 button;

  button = contdata->button;
  nuContDataClose();
  bcopy(&nuContData[padno], contdata, sizeof(OSContPad));
  contdata->trigger = nuContData[padno].button & (~button);
  nuContDataOpen();
}
