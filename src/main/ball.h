#ifndef BALL_H
#define BALL_H

#include "common.h"

/* Ball object used by the shot and meter code. unkA5 is the owning player index. */
typedef struct {
  u8 pad00[0x18];
  s32 unk18;
  s32 unk1C;
  s32 unk20;
  u8 pad24[0x68];
  s32 unk8C;
  s32 unk90;
  u8 pad94[0xC];
  u8 unkA0;
  u8 unkA1;
  u8 padA2;
  s8 unkA3;
  u8 padA4;
  s8 unkA5;
} BallObject;

#endif /* BALL_H */
