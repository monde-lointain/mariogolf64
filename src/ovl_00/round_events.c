#include "common.h"
#include "asset.h"
#include "heap.h"
#include "movie.h"
#include "round_events.h"

/* The buffer that a movie's images, or the wind Fly Guy's, are read into */
#define MOVIE_BUFFER_SIZE 0x6270
/* The movie frame after which the hole intro goes on */
#define MOVIE_CUE_FRAME 12

#define FLY_GUY_IMAGE_ASSET 0x6CF
#define WIND_STREAK_IMAGE_ASSET 0x6AF
#define FLY_GUY_IMAGE_SIZE 0x808 /* With its header; the wind streak follows it */

extern s32 sMovieUnk14;
extern s32 sMovieUnk20;
extern s32 sIsMovieBufferAllocated;
extern u8 *sMovieBuffer;
extern s32 sCurrentMovie;
extern s32 sMovieFrame;
extern s32 sWindFlyGuyFrame;
extern s32 sWindFlyGuyAsset;

void ResetPath(void);

void AllocMovieBuffer(void) {
    EncodedAsset unused;

    sMovieUnk14 = -1;
    sIsMovieBufferAllocated = TRUE;
    sMovieBuffer = AllocMemory(MOVIE_BUFFER_SIZE);
    sMovieUnk20 = -1;
}

void FreeMovieBuffer(void) {
    sIsMovieBufferAllocated = FALSE;
    FreeMemory(&sMovieBuffer);
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartBanner);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020EBBC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020ED54_ovl_00);

void func_8020EEEC_ovl_00(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020EEF8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020FAE8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8020FC10_ovl_00);

s32 IsMovieFinished(void) {
    return sCurrentMovie == MOVIE_NONE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartMovie);

s32 IsMoviePastCue(void) {
    return sMovieFrame >= MOVIE_CUE_FRAME;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80210370_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802104BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80210F40_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8021169C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80211D60_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80212590_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802129C0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80212CE0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80213038_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80213634_ovl_00);

s32 IsContactEffectShown(void) {
    return g_ContactEffect != CONTACT_EFFECT_NONE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80213C78_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", StartContactEffect);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_8021419C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_80214250_ovl_00);

/* Shows the Fly Guy that warns of a strong wind, or hides it given FLY_GUY_HIDDEN */
void ShowWindFlyGuy(s32 show) {
    EncodedAsset asset;

    if (sIsMovieBufferAllocated == TRUE) {
        if (show == FLY_GUY_HIDDEN) {
            sWindFlyGuyFrame = FLY_GUY_HIDDEN;
            return;
        }
        sWindFlyGuyFrame = 0;
        sWindFlyGuyAsset = FLY_GUY_IMAGE_ASSET;
        OpenEncodedAsset(FLY_GUY_IMAGE_ASSET, &asset);
        ReadEncodedAsset(sMovieBuffer, &asset);
        OpenEncodedAsset(WIND_STREAK_IMAGE_ASSET, &asset);
        ReadEncodedAsset(sMovieBuffer + FLY_GUY_IMAGE_SIZE, &asset);
        sMovieUnk20 = -1;
        sMovieUnk14 = -1;
        ResetPath();
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round_events", func_802161D4_ovl_00);
