/*
 * Unlink an SI message-callback entry from the SI manager's dispatch list.
 */

#include <nusys.h>

/*
 * Remove `list` from the callback chain by finding its predecessor and
 * splicing it out. The scan matches the node whose `next` is `list`, so a list
 * that is the chain head (no predecessor) is left in place.
 */
void nuSiCallBackRemove(NUCallBackList* list) {
  OSIntMask mask;
  NUCallBackList** siCallBackListPtr;

  siCallBackListPtr = &nuSiCallBackList;
  while (*siCallBackListPtr) {
    if ((*siCallBackListPtr)->next == list) {
      // Splice the node out under an interrupt mask so the SI manager thread
      // never sees a dangling link.
      mask = osSetIntMask(OS_IM_NONE);
      (*siCallBackListPtr)->next = list->next;
      list->next = NULL;
      osSetIntMask(mask);
      break;
    }
    siCallBackListPtr = &(*siCallBackListPtr)->next;
  }
}
