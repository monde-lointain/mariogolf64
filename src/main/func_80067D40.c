#include "common.h"

typedef struct {
  /* 0x0 */ s8 f0;
  /* 0x1 */ s8 pad[3];
} Hf0; /* stride 0x4, based at D_800C3604 */

typedef struct {
  /* 0x0 */ s8 f3;
  /* 0x1 */ s8 pad[3];
} Hf3; /* stride 0x4, based at D_800C3607 */

typedef struct {
  /* 0x0 */ s8 id;
  /* 0x1 */ s8 pad0;
  /* 0x2 */ s16 pad1;
  /* 0x4 */ s16 pad2;
} Eid; /* stride 0x6, align 2, based at D_801B7118 */

typedef struct {
  /* 0x0 */ s16 score;
  /* 0x2 */ s8 pad[4];
} Esc; /* stride 0x6, based at D_801B711A */

typedef struct {
  /* 0x0 */ s8 hole;
  /* 0x1 */ s8 pad[5];
} Eho; /* stride 0x6, based at D_801B711C */

typedef struct {
  /* 0x0 */ u8 rank;
  /* 0x1 */ s8 pad[5];
} Erank; /* stride 0x6, based at D_801B711D */

extern s32 scenario_mode_id;
extern s32 D_800C3600;
extern Hf0 D_800C3604[];
extern Hf3 D_800C3607[];
extern char D_800C3678[];
extern f64 D_800D1370;
extern s16 D_80132CB0[];
extern Eid D_801B7118[];
extern Esc D_801B711A[];
extern Eho D_801B711C[];
extern Erank D_801B711D[];
extern char D_801B71D0[];

extern f64 sin(f64);
extern s32 strcmp(const char*, const char*);
extern void rank_scores_descending(s32* keys, s32* order, s32 count);

s32 func_80067D4C(void);
void func_80068308(void);

void func_80067D40(s32 arg0) { D_800C3600 = arg0; }

s32 func_80067D4C(void) {
  D_800C3600 = D_800C3600 * 0x5D588B65 + 1;
  return D_800C3600;
}

s32 func_80067D74(s32 arg0, s32 arg1, s32 start, s32 end, s32 arg4) {
  s32 hi;
  s32 i;
  s32 mod3;
  s32 factor;
  s32 hole;
  s32 scoreDelta;
  f64 rad;
  s32 val;
  if (arg0 > 0x12) {
    arg0 = 0x12;
  }
  i = start;
  if (i < end) {
    hi = (u32)arg1 >> 31;
    rad = D_800D1370;
    do {
      mod3 = i % 3;
      factor = i % 10;
      if (scenario_mode_id < 2) {
        factor = 0xD - factor;
        if (factor > 9) {
          factor = 9;
        }
      }
      {
        s32 f = D_800C3607[i - 1].f3;
        s32 carry = hi;
        carry |= arg1 >= i;
        hole = arg0 + (f + carry);
      }
      if (hole > 0x12) {
        hole = 0x12;
      }
      scoreDelta = D_800C3604[i - 1].f0 - scenario_mode_id * 2;
      {
        s32 a = (s32)((u32)func_80067D4C() >> 28 & 5) - 2;
        s32 b = (s32)((u32)func_80067D4C() >> 28 & 5) - 2;
        scoreDelta += (a + b) / 2;
      }
      if (hole == 0x12) {
        s32 lo = scenario_mode_id * -2 + 1;
        if (scoreDelta <= lo) {
          scoreDelta = lo;
        }
      }
      if (scoreDelta < -0x12) {
        scoreDelta = -0x12;
      } else if (scoreDelta > 0x12) {
        scoreDelta = 0x12;
      }
      switch (mod3) {
        case 0:
          val = (scoreDelta * hole) / 18;
          break;
        case 1:
          val = (s32)((f64)((scoreDelta * hole) / 18) +
                      sin((f64)hole * rad) * (f64)factor);
          break;
        case 2:
          val = (s32)((f64)((scoreDelta * hole) / 18) -
                      sin((f64)hole * rad) * (f64)factor);
          break;
      }
      D_801B7118[i].id = i - 1;
      D_801B711A[i].score = val;
      D_801B711C[i].hole = hole;
      i++;
    } while (i < end);
  }
}

u8 func_800680FC(s32 arg0, s32 arg1, s32 arg2, s32 arg3) {
  s32 keys[30];
  s32 order[30];
  Eid saved[30];
  s32 i;
  s32 j;
  u8 result;

  result = 1;
  func_80067D40(arg3);
  func_80067D74(arg0, arg1, 1, 30, arg2);
  func_80068308();

  D_801B711A[0].score = arg2;
  D_801B7118[0].id = -1;
  D_801B711C[0].hole = arg0;
  for (i = 0; i < 30; i++) {
    keys[i] = -D_801B711A[i].score;
  }
  rank_scores_descending(keys, order, 30);
  for (i = 0; i < 30; i++) {
    saved[i] = D_801B7118[i];
  }
  for (i = 0; i < 30; i++) {
    D_801B7118[i] = saved[order[i]];
  }

  for (i = 0; i < 30; i++) {
    u8 rank = 1;
    u8 flag = 0;
    j = 0;
    if (j < i) {
      Erank* rp = D_801B711D;
      do {
        if (D_801B711A[i].score > D_801B711A[j].score) {
          rank++;
        } else {
          flag = 0x80;
          rp[j].rank |= 0x80;
        }
        j++;
      } while (j < i);
    }
    D_801B711D[i].rank = flag | rank;
    if (D_801B7118[i].id == -1) {
      result = flag | rank;
    }
  }
  return result;
}

void func_80068308(void) {
  s16* p;
  s16* limit;
  char* names;
  s32 lo;
  s32 i;
  s32 idx;
  s32 found;
  s32 pl;
  s16* base;
  s32 sc;

  sc = scenario_mode_id;
  i = 0x1C;
  idx = 0x1C;
  found = 0;
  names = D_800C3678;
  base = D_80132CB0;
  p = base + 0x1C;
  limit = base + 1;
  lo = (sc + 1) * 30 - 2;
  for (; i >= 0; i--) {
    if (!found) {
      if (strcmp(D_801B71D0, (char*)((lo - idx) * 13 + (s32)names)) == 0) {
        found = 1;
        idx--;
      }
    }
    pl = p < limit;
    if (found & pl) {
      *p = (sc + 1) * 30 - 1;
    } else {
      *p = lo - idx;
      idx--;
    }
    p--;
  }
}
