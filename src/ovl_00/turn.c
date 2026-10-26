#include "common.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "current_shot.h"
#include "reaction.h"
#include "save.h"
#include "sound.h"

/* The loop played while the stick sets the impact point */
#define IMPACT_POINT_SOUND 0x26
#define IMPACT_POINT_SOUND_CHANNEL 8
#define IMPACT_POINT_SOUND_VOLUME 60
#define IMPACT_POINT_SOUND_REVERB 20
#define IMPACT_POINT_SOUND_PAN 230

/* The buttons that hold the impact point mode */
#define IMPACT_POINT_BUTTONS (Z_TRIG | L_TRIG)

/* Bit 1 of D_800C3084 keeps the impact point from being set */
#define SHOT_UNK3084_NO_IMPACT_POINT 2

extern s32 D_800C3084;
extern s32 D_80227194_ovl_00;

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202A00_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202A20_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80202D04_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802038D0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203B4C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203DAC_ovl_00);

void UpdateImpactPointMode(void) {
    if ((g_TurnController.buttons & IMPACT_POINT_BUTTONS) && !(D_800C3084 & SHOT_UNK3084_NO_IMPACT_POINT)) {
        if (!g_IsSettingImpactPoint && D_80227194_ovl_00 == 0) {
            PlaySoundWithEffects(IMPACT_POINT_SOUND, IMPACT_POINT_SOUND_CHANNEL, IMPACT_POINT_SOUND_VOLUME,
                                 IMPACT_POINT_SOUND_REVERB, IMPACT_POINT_SOUND_PAN);
        }
        g_IsSettingImpactPoint = TRUE;
    } else {
        StopSound(IMPACT_POINT_SOUND_CHANNEL);
        g_IsSettingImpactPoint = FALSE;
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203EEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80203F30_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204104_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802041C8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204210_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020439C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204460_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204568_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802045BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020466C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80204910_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802054CC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802055F8_ovl_00);

/* Always FALSE; maybe once a debug check */
s32 func_8020561C_ovl_00(s32 arg0) {
    return FALSE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205624_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802059C0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205AE4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205BB8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205C78_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205D44_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80205DE8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206354_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206BD0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206CC8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206CF0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80206E88_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207014_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207048_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207080_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802071B8_ovl_00);

/*
 * The first of the club kind's animations; shotMode is unused. The staff
 * roll overlay has its own copies of these four routines, with the same
 * names, so these are static to link beside them.
 */
static s32 GetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    s32 animation;

    switch (GetCharacterClubKind(slotIndex, club)) {
        case CLUB_KIND_WOOD:
            animation = WOOD_ANIMATIONS;
            break;
        case CLUB_KIND_IRON:
            animation = IRON_ANIMATIONS;
            break;
        case CLUB_KIND_WEDGE:
            animation = WEDGE_ANIMATIONS;
            break;
        case CLUB_KIND_PUTTER:
            animation = PUTTER_ANIMATIONS;
            break;
        default:
            animation = WOOD_ANIMATIONS;
            break;
    }
    return animation;
}

/* The second of the club kind's animations, or the third for an approach shot */
static s32 GetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    s32 animation;

    switch (GetCharacterClubKind(slotIndex, club)) {
        case CLUB_KIND_WOOD:
            animation = WOOD_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_KIND_IRON:
            animation = IRON_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_KIND_WEDGE:
            animation = WEDGE_ANIMATIONS + SWING_ANIMATION;
            break;
        case CLUB_KIND_PUTTER:
            animation = PUTTER_ANIMATIONS + SWING_ANIMATION;
            break;
        default:
            animation = 0;
            break;
    }
    return animation + (shotMode == SHOT_MODE_APPROACH);
}

static void SetStanceAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    SetCharacterAnimation(slotIndex, GetStanceAnimation(slotIndex, club, shotMode));
}

static void SetSwingAnimation(u32 slotIndex, s32 club, s32 shotMode) {
    SetCharacterAnimation(slotIndex, GetSwingAnimation(slotIndex, club, shotMode));
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207388_ovl_00);

void func_80207660_ovl_00(UnkB8 *player) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207668_ovl_00);

/* Hides the rings the ball passed, so that a new shot can pass them again */
void ClearPassedRings(UnkB8 *player) {
    s32 i;

    for (i = 0; i < RING_COUNT; i++) {
        if (player->ringStates[i] >= RING_STATE_PASSED) {
            player->ringStates[i] = RING_STATE_NONE;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802078F4_ovl_00);

void FreeCharacterAndReactionLight(void) {
    FreeCharacterSlot(0);
    TurnOffReactionLight();
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207A34_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207B8C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207BEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207CAC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207E88_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80207F70_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802080B4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802080D4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208120_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020816C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802081F8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208354_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_8020845C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802087D0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208810_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_80208850_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/turn", func_802088B0_ovl_00);
