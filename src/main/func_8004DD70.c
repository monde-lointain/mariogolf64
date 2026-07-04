#include "common.h"

/* A circular doubly-linked list of sized nodes, one list per slot index.
 * The array element at D_800DC6E0[i] doubles as the list head (self-linked
 * when empty) and the running-total accumulator for slot i. */
typedef struct Slot {
  /* 0x00 */ s32 size; /* byte length of this node's region */
  /* 0x04 */ s32 unk_04;
  /* 0x08 */ struct Slot* next;
  /* 0x0C */ struct Slot* prev;
  /* 0x10 */ s32 total; /* accumulated size across the slot's list */
  /* 0x14 */ s32 unk_14;
} Slot; /* 0x18 */

extern Slot D_800DC6E0[];

void func_8004DD70(s32 i, Slot* node, void* end) {
  s32 len = (u8*)end - (u8*)node;

  node->size = len;
  D_800DC6E0[i].total += len;
  node->unk_04 = 0;
  node->next = &D_800DC6E0[i];
  node->prev = D_800DC6E0[i].prev;
  D_800DC6E0[i].prev->next = node;
  D_800DC6E0[i].prev = node;
}

void func_8004DDE4(s32 i) {
  Slot* s = &D_800DC6E0[i];

  D_800DC6E0[i].total = 0;
  D_800DC6E0[i].size = 0;
  D_800DC6E0[i].unk_04 = -1;
  s->prev = s;
  D_800DC6E0[i].next = s;
  D_800DC6E0[i].unk_14 = -1;
}

s32 func_8004DE44(s32 i) { return D_800DC6E0[i].total; }
