#ifndef CURRENT_SHOT_H
#define CURRENT_SHOT_H

#include <ultra64.h>

/* The state of the shot being set up by the player whose turn it is */
typedef struct CurrentShot {
  /* 0x00 */ s16 unk0;
  /* 0x04 */ s32 unk4;
  /* 0x08 */ s32 unk8;
  /* 0x0C */ s32 shotMode;
  /* 0x10 */ s32 clubSlot; /* Index into the player's clubs */
  /* 0x14 */ s32 unk14;
  /* 0x18 */ s32 unk18;
  /* 0x1C */ CoursePoint ballPosition;
  /* 0x28 */ s32 surface;
  /* 0x2C */ s32 playerIndex;
  /* 0x30 */ s32 controllerIndex;
  /* 0x34 */ s32 aimCameraDistance; /* From the ball along the aim, so negative
                                       is behind it */
  /* 0x38 */ s32 unk38;
  /* 0x3C */ s32 aimCameraHeight; /* Added to the lower of the ground's and the
                                     ball's heights */
  /* 0x40 */ s32 unk40;
  /* 0x44 */ CoursePoint unk44;
  /* 0x50 */ s32 unk50;
  /* 0x54 */ s8 unk54;
} CurrentShot; /* size = 0x58 */

extern CurrentShot g_CurrentShot;
/* Holding Z or L sets the impact point with the stick, rather than aiming */
extern s32 g_IsSettingImpactPoint;

#endif
