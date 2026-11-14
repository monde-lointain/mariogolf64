#ifndef PIN_H
#define PIN_H

#include <ultra64.h>

#include "camera.h"
#include "system/c_language.h"

#define FLAGSTICK_HEIGHT 0xB400

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
