/*
 * Singly-linked thread-queue removal.
 *
 * Unlinks a specific thread from a priority/wait queue by walking the chain
 * and splicing it out. Shared by the scheduler and the message primitives,
 * which use the same intrusive `next`-linked thread lists.
 */

#include "PR/os_internal.h"

void __osDequeueThread(register OSThread** queue, register OSThread* t) {
  register OSThread* pred;
  register OSThread* succ;

  // Treat the queue head pointer as a pseudo-node so the first real entry can
  // be unlinked with the same predecessor/successor splice as any other.
  pred = (OSThread*)queue;
  succ = pred->next;
  while (succ != NULL) {
    if (succ == t) {
      pred->next = t->next;
      return;
    }
    pred = succ;
    succ = pred->next;
  }
}
