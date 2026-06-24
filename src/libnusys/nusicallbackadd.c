/*
 * Append an SI (serial-interface / controller) message-callback entry to the
 * SI manager's dispatch list.
 */

#include <nusys.h>

/*
 * Register the callback list `list`, keyed by its major number. The list is
 * rejected if an entry with the same major number is already present, so each
 * major number maps to exactly one handler set.
 */
void nuSiCallBackAdd(NUCallBackList* list) {
  OSIntMask mask;
  NUCallBackList** siCallBackListPtr;
  s32 cnt;

  // Walk to the list tail, bailing out if this major number already exists.
  siCallBackListPtr = &nuSiCallBackList;
  while (*siCallBackListPtr) {
    if ((*siCallBackListPtr)->majorNo == list->majorNo) {
#ifdef NU_DEBUG
      osSyncPrintf(
          "nuSiCallBackAdd: CallBack is already added(major no = %#X!!\n",
          list->majorNo);
#endif
      return;
    }
    siCallBackListPtr = &(*siCallBackListPtr)->next;
  }

  // Count the minor-number handler slots so the dispatcher can range-check the
  // minor number later (func[0] is the retrace handler, so counting starts at
  // 1 and stops at the first NULL).
  for (cnt = 1; list->func[cnt] != NULL; cnt++) {
    ;
  }

  // Link the entry in under an interrupt mask so the SI manager thread can't
  // traverse a partially linked node.
  mask = osSetIntMask(OS_IM_NONE);
  *siCallBackListPtr = list;
  list->next = NULL;
  list->funcNum = cnt;
  osSetIntMask(mask);
}
