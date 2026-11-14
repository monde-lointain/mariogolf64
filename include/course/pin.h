/*
 * The pin: the cup, the flagstick and the flag.
 */

#ifndef PIN_H
#define PIN_H

#include <ultra64.h>

#include "camera/camera.h"
#include "system/c_language.h"
// In 1/1024ths of a unit
#define FLAGSTICK_HEIGHT 0xB400

// How many times its model's size the pin, the cup and the ball's trail are
// drawn
extern s32 g_PinVertexScale;
extern s32 g_CupViewDepth;

s32 GetCupDistance(s32 x, s32 z);
void BuildPinShadow(void);
void LoadPinTextures(void);
void FreePinTextures(void);
void TakePinOut(void);
void SkipNextPinDraw(void);
Boolean IsPinInCup(void);
void DrawPin(CameraMatrices *matrices, CameraView *view, s32 cameraIndex);

#endif
