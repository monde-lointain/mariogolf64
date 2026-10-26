#include "common.h"
#include "asset.h"
#include "billboard.h"
#include "controller.h"
#include "course.h"
#include "heap.h"
#include "hole_display.h"
#include "round_hud.h"
#include "scene.h"
#include "screen.h"
#include "sound.h"
#include "text.h"
#include "variables.h"

/*
 * The pause menu over a round, and the choice box that asks the player to
 * pick between a few options during a turn.
 */

#define PAUSE_CURSOR_IMAGE_ASSET 0x70F /* A pointing glove and two tees */
#define PAUSE_CLOUD_IMAGE_ASSET 0x70B  /* The wide cloud behind the pause menu */
#define PAUSE_ARROWS_IMAGE_ASSET 0x71E
#define CHOICE_BOX_CLOUD_IMAGE_ASSET 0x71F /* The small cloud behind a choice box */

#define ROUND_FRAMEBUFFER_COUNT 3
#define FULL_SONG_VOLUME 0x7F

/* Values of sPauseMenuRequest, which the pause menu's drawing carries out */
#define PAUSE_MENU_REQUEST_FREE (-1)
#define PAUSE_MENU_REQUEST_NONE 0
#define PAUSE_MENU_REQUEST_LOAD 1

/* The pause menu's option lists, which the game mode picks */
#define PAUSE_LAYOUT_NONE (-1) /* For a record being played back */
#define PAUSE_LAYOUT_SPEED_GOLF 0
#define PAUSE_LAYOUT_RING_SHOT 1
#define PAUSE_LAYOUT_TRAINING 2
#define PAUSE_LAYOUT_TRAINING_MINI_GOLF 3
#define PAUSE_LAYOUT_DRIVING_RANGE 4
#define PAUSE_LAYOUT_VERSUS 5 /* Match Game, Get Character, Skins Match and Club Slots */
#define PAUSE_LAYOUT_STROKE 6 /* Stroke, Tournament and the modes like them */
/* The layouts used while D_800C42D0 is set */
#define PAUSE_LAYOUT_UNK42D0_SPEED_GOLF 7
#define PAUSE_LAYOUT_UNK42D0 8
/* The layouts used while g_IsUnk3B is set */
#define PAUSE_LAYOUT_UNK3B_SPEED_GOLF 9
#define PAUSE_LAYOUT_UNK3B 10

#define PAUSE_MENU_SONG_VOLUME 0x60

extern s32 D_800C42D0; /* Set once a Game Boy character has been brought over */
extern s32 sIsPauseMenuLoaded;
extern s32 sPauseSelection;
extern s32 sPauseLayout;
extern s32 sPauseMenuRequest;
extern s32 sPauseMenuUnkD678;
extern f32 sPauseMenuUnkD67C;
extern f32 sPauseMenuUnkD684;
extern f32 sPauseMenuUnkD688;
extern u8 *sPauseCursorImage;
extern u8 *sMenuCloudImage; /* The pause menu's cloud, or the choice box's */
extern u8 *sPauseArrowsImage;
extern u8 *sChoiceBoxCursorImage;

#define CHOICE_BOX_MAX_OPTIONS 4
#define CHOICE_BOX_OPEN_SOUND 0x64
#define CHOICE_BOX_CLOSE_SOUND 0x65
#define CHOICE_BOX_MOVE_SOUND 0x5C
#define CHOICE_BOX_CHOOSE_SOUND 0x5D
#define MENU_SOUND_VOLUME 100

/* Values of sChoiceBoxState */
#define CHOICE_BOX_CLOSED (-1)
#define CHOICE_BOX_CHOOSING 0
#define CHOICE_BOX_CHOSEN 1   /* Waits a frame before the box closes */
#define CHOICE_BOX_CLOSING 2

typedef struct ChoiceBox {
    /* 0x00 */ s32 optionCount;
    /* 0x04 */ s32 halfWidth; /* Widened to fit the longest label */
    /* 0x08 */ s32 isVertical;
    /* 0x0C */ u8 labels[CHOICE_BOX_MAX_OPTIONS]; /* Indexes into sChoiceBoxLabels */
    /* 0x10 */ u8 results[CHOICE_BOX_MAX_OPTIONS];
} ChoiceBox; /* size = 0x14 */

extern ChoiceBox sChoiceBoxes[];
extern char *sChoiceBoxLabels[];
extern s32 sChoiceBoxState;
extern s32 sChoiceBoxIndex;
extern s32 sChoiceBoxSelection;
extern f32 sChoiceGlowAngle;  /* Cycles the selected label's colour */
extern f32 sChoiceCursorAngle; /* Bobs the cursor from side to side */

#define CHOICE_BOX_CENTER_X (SCREEN_WIDTH / 2)
#define CHOICE_BOX_MARGIN 32
#define CHOICE_BOX_TOP 72
#define CHOICE_BOX_BOTTOM 164
#define CHOICE_BOX_CLOUD_WIDTH 128
#define CHOICE_BOX_CLOUD_HEIGHT 64
#define CHOICE_BOX_CLOUD_PART_COUNT 1
#define CHOICE_BOX_CLOUD_PART_SIZE 0x1000
/* The cloud's height scaled to the box's, as a 5.10 texel step */
#define CHOICE_BOX_CLOUD_DTDY 0x2C8
#define CHOICE_LABELS_MIDDLE_Y 113
#define CHOICE_LABEL_SPACING 16
#define CHOICE_CURSOR_SIZE 24
#define CHOICE_CURSOR_LOADED_HEIGHT 76
/* The cursor bobs about this x, left of the selected label's middle */
#define CHOICE_CURSOR_X 124
#define CHOICE_CURSOR_SWAY 8.0f
#define COLOR_WAVE_MIDDLE 127
#define COLOR_WAVE_AMPLITUDE 127.0f

void SetFramebufferCount(s32 count);

void HideRoundHud(void) {
    g_RoundHud.isLetterboxShown = FALSE;
    g_RoundHud.unkC = 0;
    g_RoundHud.unk7 = 0;
    g_RoundHud.unk89 = 0;
    g_RoundHud.unk8A = 0;
    g_RoundHud.unk8B = 0;
    g_RoundHud.unk8 = 0;
    g_RoundHud.unk9 = 0;
    g_RoundHud.unkA = 0;
    g_RoundHud.unkB = 0;
    g_RoundHud.unkD = 0;
}

void ResumeRound(void) {
    g_RoundHud.isLetterboxShown = TRUE;
    g_RoundHud.unkC = 1;
    g_RoundHud.unk7 = 1;
    g_RoundHud.unk89 = 0;
    g_RoundHud.unk8A = 0;
    g_RoundHud.unk8B = 0;
    g_RoundHud.unk8 = 1;
    g_RoundHud.unk9 = 1;
    g_RoundHud.unkA = 1;
    g_RoundHud.unkB = 1;
    g_RoundHud.unkD = 1;
    SetMenuBackdrop(MENU_BACKDROP_NONE);
    g_PauseScreen = PAUSE_SCREEN_NONE;
    SetFramebufferCount(ROUND_FRAMEBUFFER_COUNT);
    SetScreenEffect(SCREEN_EFFECT_NONE, 0);
}

void RequestPauseMenu(s32 unkD678, s32 isClosing) {
    sPauseMenuRequest = isClosing == FALSE ? PAUSE_MENU_REQUEST_LOAD : PAUSE_MENU_REQUEST_FREE;
    sPauseMenuUnkD678 = unkD678;
}

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225440_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225450_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_8022545C_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225468_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225474_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225480_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225484_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225488_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225490_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_80225498_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/pause_menu", D_802254A0_ovl_00);

void LoadPauseMenu(void) {
    EncodedAsset asset;

    if (!sIsPauseMenuLoaded) {
        sIsPauseMenuLoaded = TRUE;
        sPauseSelection = 0;
        if (g_IsUnk3B) {
            if (g_GameMode == GAME_MODE_SPEED_GOLF) {
                sPauseLayout = PAUSE_LAYOUT_UNK3B_SPEED_GOLF;
            } else {
                sPauseLayout = PAUSE_LAYOUT_UNK3B;
            }
        } else if (D_800C42D0) {
            if (g_GameMode == GAME_MODE_SPEED_GOLF) {
                sPauseLayout = PAUSE_LAYOUT_UNK42D0_SPEED_GOLF;
            } else if (g_GameMode == GAME_MODE_RING_SHOT) {
                sPauseLayout = PAUSE_LAYOUT_RING_SHOT;
            } else if (g_GameMode == GAME_MODE_TRAINING) {
                if (g_IsMiniGolf == TRUE) {
                    sPauseLayout = PAUSE_LAYOUT_TRAINING_MINI_GOLF;
                } else if (GetCourseRecordIndex() == COURSE_RECORD_DRIVING_RANGE) {
                    sPauseLayout = PAUSE_LAYOUT_DRIVING_RANGE;
                } else {
                    sPauseLayout = PAUSE_LAYOUT_TRAINING;
                }
            } else {
                sPauseLayout = PAUSE_LAYOUT_UNK42D0;
            }
        } else {
            switch (g_GameMode) {
                case GAME_MODE_SPEED_GOLF:
                    sPauseLayout = PAUSE_LAYOUT_SPEED_GOLF;
                    break;
                case GAME_MODE_TRAINING:
                    if (g_IsMiniGolf == TRUE) {
                        sPauseLayout = PAUSE_LAYOUT_TRAINING_MINI_GOLF;
                    } else if (GetCourseRecordIndex() == COURSE_RECORD_DRIVING_RANGE) {
                        sPauseLayout = PAUSE_LAYOUT_DRIVING_RANGE;
                    } else {
                        sPauseLayout = PAUSE_LAYOUT_TRAINING;
                    }
                    break;
                case GAME_MODE_RING_SHOT:
                    sPauseLayout = PAUSE_LAYOUT_RING_SHOT;
                    break;
                case GAME_MODE_MATCH_GAME:
                case GAME_MODE_GET_CHARACTER:
                case GAME_MODE_SKINS_MATCH:
                case GAME_MODE_CLUB_SLOTS:
                    sPauseLayout = PAUSE_LAYOUT_VERSUS;
                    break;
                case GAME_MODE_UNK8:
                    sPauseLayout = PAUSE_LAYOUT_NONE;
                    break;
                case GAME_MODE_STROKE:
                case GAME_MODE_UNK1:
                case GAME_MODE_TOURNAMENT:
                case GAME_MODE_UNK6:
                    sPauseLayout = PAUSE_LAYOUT_STROKE;
                    break;
            }
        }
        sPauseCursorImage = AllocMemory(OpenEncodedAsset(PAUSE_CURSOR_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sPauseCursorImage, &asset);
        sMenuCloudImage = AllocMemory(OpenEncodedAsset(PAUSE_CLOUD_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sMenuCloudImage, &asset);
        sPauseMenuUnkD684 = SCREEN_WIDTH;
        sPauseMenuUnkD688 = SCREEN_HEIGHT;
        sPauseMenuUnkD67C = SCREEN_HEIGHT;
        sPauseArrowsImage = AllocMemory(OpenEncodedAsset(PAUSE_ARROWS_IMAGE_ASSET, &asset));
        ReadEncodedAsset(sPauseArrowsImage, &asset);
        SetSongVolume(PAUSE_MENU_SONG_VOLUME);
    }
}

void FreePauseMenu(void) {
    if (sIsPauseMenuLoaded == TRUE) {
        sIsPauseMenuLoaded = FALSE;
        FreeMemory(&sPauseCursorImage);
        FreeMemory(&sMenuCloudImage);
        FreeMemory(&sPauseArrowsImage);
    }
    SetSongVolume(FULL_SONG_VOLUME);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_80219034_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_802199EC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", func_80219C74_ovl_00);

void LoadChoiceBoxImages(void) {
    EncodedAsset asset;

    sChoiceBoxCursorImage = AllocMemory(OpenEncodedAsset(PAUSE_CURSOR_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sChoiceBoxCursorImage, &asset);
    sMenuCloudImage = AllocMemory(OpenEncodedAsset(CHOICE_BOX_CLOUD_IMAGE_ASSET, &asset));
    ReadEncodedAsset(sMenuCloudImage, &asset);
}

void FreeChoiceBoxImages(void) {
    FreeMemory(&sChoiceBoxCursorImage);
    FreeMemory(&sMenuCloudImage);
}

/* Opens the choice box of the given index, or closes the box with CHOICE_BOX_CLOSED */
void OpenChoiceBox(s32 choiceBoxIndex) {
    sChoiceBoxIndex = choiceBoxIndex;
    if (choiceBoxIndex == CHOICE_BOX_CLOSED) {
        sChoiceBoxState = choiceBoxIndex;
        FreeChoiceBoxImages();
        PlaySound(CHOICE_BOX_CLOSE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
    } else {
        sChoiceBoxSelection = 0;
        sChoiceBoxState = CHOICE_BOX_CHOOSING;
        LoadChoiceBoxImages();
        PlaySound(CHOICE_BOX_OPEN_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
    }
}

/* Moves the choice box's selection, and returns the chosen option's result once the box closes */
s32 UpdateChoiceBox(ControllerState *controller) {
    ChoiceBox *box = &sChoiceBoxes[sChoiceBoxIndex];

    if (sChoiceBoxIndex == CHOICE_BOX_CLOSED) {
        return -1;
    }
    switch (sChoiceBoxState) {
        case CHOICE_BOX_CHOSEN:
            sChoiceBoxState = CHOICE_BOX_CLOSING;
            return -1;
        case CHOICE_BOX_CLOSING:
            OpenChoiceBox(CHOICE_BOX_CLOSED);
            return box->results[sChoiceBoxSelection];
        case CHOICE_BOX_CHOOSING:
            if (controller->pressedButtons == A_BUTTON) {
                PlaySound(CHOICE_BOX_CHOOSE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
                sChoiceBoxState = CHOICE_BOX_CHOSEN;
                return -1;
            }
            if (!box->isVertical) {
                if (controller->repeatedButtons & L_JPAD) {
                    sChoiceBoxSelection--;
                    PlaySound(CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
                }
                if (controller->repeatedButtons & R_JPAD) {
                    sChoiceBoxSelection++;
                    PlaySound(CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
                }
            } else {
                if (controller->repeatedButtons & U_JPAD) {
                    PlaySound(CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
                    sChoiceBoxSelection--;
                }
                if (controller->repeatedButtons & D_JPAD) {
                    PlaySound(CHOICE_BOX_MOVE_SOUND, g_NextSoundChannel, MENU_SOUND_VOLUME);
                    sChoiceBoxSelection++;
                }
            }
            if (sChoiceBoxSelection > box->optionCount - 1) {
                sChoiceBoxSelection = 0;
            }
            if (sChoiceBoxSelection < 0) {
                sChoiceBoxSelection = box->optionCount - 1;
            }
            return -1;
    }
    return -1;
}

#ifdef NON_MATCHING
void DrawChoiceBox(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;
    ChoiceBox *box = &sChoiceBoxes[sChoiceBoxIndex];
    s32 halfWidth;
    s32 red;
    s32 green;
    s32 blue;
    s32 textWidth;
    s32 labelWidth;
    s32 i;
    s32 cursorX;
    s32 cursorY;
    s32 labelsTop;
    s16 labelY;

    sChoiceGlowAngle += M_PI / 18;
    halfWidth = box->halfWidth + CHOICE_BOX_MARGIN;
    red = (s32)(sinf(sChoiceGlowAngle) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    blue = (s32)(sinf(sChoiceGlowAngle + M_PI / 2) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    green = (s32)(sinf(sChoiceGlowAngle + M_PI) * COLOR_WAVE_AMPLITUDE) + COLOR_WAVE_MIDDLE;
    textWidth = 0;
    if (sChoiceBoxIndex != CHOICE_BOX_CLOSED) {
        gDPPipeSync(gfx++);
        gDPSetPrimColor(gfx++, 0, 0, 0x1F, 0x1F, 0x1F, 0xBF);
        gDPSetPrimColor(gfx++, 0, 0, 0xFF, 0xFF, 0xFF, 0xBF);
        gDPPipeSync(gfx++);
        gDPSetRenderMode(gfx++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
        gDPSetCombineLERP(gfx++, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0, 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0);
        gDPPipeSync(gfx++);
        gDPSetTextureFilter(gfx++, G_TF_BILERP);
        for (i = 0; i != box->optionCount; i++) {
            labelWidth = MeasureText(sChoiceBoxLabels[box->labels[i]], FONT_SIZE_LARGE, MEASURE_TEXT_PROPORTIONAL);
            if (textWidth < labelWidth) {
                textWidth = labelWidth;
            }
            halfWidth = textWidth / 2 + CHOICE_BOX_MARGIN;
        }
        for (i = 0; i != CHOICE_BOX_CLOUD_PART_COUNT; i++) {
            LoadNibbleTextureBlock(&gfx, sMenuCloudImage + IMAGE_HEADER_SIZE + i * CHOICE_BOX_CLOUD_PART_SIZE,
                                   G_IM_FMT_I, CHOICE_BOX_CLOUD_WIDTH, CHOICE_BOX_CLOUD_HEIGHT, 0, 0, 0, 0, 0, 0, 0);
            DrawTextureRect(&gfx, (CHOICE_BOX_CENTER_X - halfWidth) << 2, CHOICE_BOX_TOP << 2,
                            (CHOICE_BOX_CENTER_X + halfWidth) << 2, CHOICE_BOX_BOTTOM << 2, G_TX_RENDERTILE, 0, 0,
                            (CHOICE_BOX_CLOUD_WIDTH << 10) / 2 / halfWidth, CHOICE_BOX_CLOUD_DTDY);
        }
        gDPPipeSync(gfx++);
        SetUpTextDrawing(&gfx);
        if (box->isVertical) {
            labelsTop = CHOICE_LABELS_MIDDLE_Y - (box->optionCount - 1) * (CHOICE_LABEL_SPACING / 2);
            for (i = 0, labelY = labelsTop; i != box->optionCount; i++, labelY += CHOICE_LABEL_SPACING) {
                if (i == sChoiceBoxSelection) {
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, red, green, blue, 0xFF);
                    gDPPipeSync(gfx++);
                } else {
                    gDPPipeSync(gfx++);
                    gDPSetPrimColor(gfx++, 0, 0, 0x5F, 0x3F, 0xFF, 0xFF);
                    gDPPipeSync(gfx++);
                }
                DrawCenteredLargeText(&gfx, CHOICE_BOX_CENTER_X, labelY, sChoiceBoxLabels[box->labels[i]]);
            }
            gDPPipeSync(gfx++);
            gDPSetTextureLUT(gfx++, G_TT_RGBA16);
            gDPPipeSync(gfx++);
            gDPSetRenderMode(gfx++, G_RM_TEX_EDGE, G_RM_TEX_EDGE2);
            gDPSetCombineMode(gfx++, G_CC_DECALRGBA, G_CC_DECALRGBA);
            sChoiceCursorAngle += M_PI / 12;
            if (sChoiceCursorAngle > M_PI) {
                sChoiceCursorAngle -= M_PI;
            }
            labelWidth = MeasureText(sChoiceBoxLabels[box->labels[sChoiceBoxSelection]], FONT_SIZE_LARGE,
                                     MEASURE_TEXT_PROPORTIONAL);
            cursorX = (s32)(sinf(sChoiceCursorAngle) * CHOICE_CURSOR_SWAY) + CHOICE_CURSOR_X;
        cursorX -= textWidth / 2;
            LoadSmallPalette(&gfx, 0, sChoiceBoxCursorImage + IMAGE_HEADER_SIZE);
            cursorY = labelsTop + sChoiceBoxSelection * CHOICE_LABEL_SPACING;
            gDPLoadTextureTile_4b(gfx++, TEXTURE_ADDRESS(sChoiceBoxCursorImage + PALETTED_TEXELS_OFFSET), G_IM_FMT_CI,
                                  CHOICE_CURSOR_SIZE, 0, 0, 0, CHOICE_CURSOR_SIZE - 1, CHOICE_CURSOR_LOADED_HEIGHT - 1, 0,
                                  0, 0, 0, 0, 0, 0);
            DrawTextureRect(&gfx, cursorX << 2, (cursorY - 2) << 2, (cursorX + CHOICE_CURSOR_SIZE) << 2,
                            (cursorY + CHOICE_CURSOR_SIZE - 2) << 2, G_TX_RENDERTILE, 0, 0, 1 << 10, 1 << 10);
        }
    }
    *displayListHead = gfx;
}
#else
INCLUDE_ASM("asm/nonmatchings/ovl_00/pause_menu", DrawChoiceBox);
#endif
