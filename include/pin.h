#ifndef PIN_H
#define PIN_H

#include <ultra64.h>

#include "camera.h"

/* How many times its model's size the pin, the cup and the ball's trail are
 * drawn */
extern s32 g_PinVertexScale;
extern s32 g_CupViewDepth;

s32 GetCupDistance(s32 x, s32 z);
void BuildPinShadow(void);
void LoadPinTextures(void);
void FreePinTextures(void);
void TakePinOut(void);
void SkipNextPinDraw(void);
s32 IsPinInCup(void);
void DrawPin(CameraMatrices* matrices, CameraView* view, s32 cameraIndex);

#endif
