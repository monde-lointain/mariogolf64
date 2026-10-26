#include "common.h"
#include "camera.h"
#include "character_slot.h"
#include "club.h"
#include "controller.h"
#include "current_shot.h"
#include "math_util.h"
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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD5F0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD5F8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD624_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD760_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD798_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD8DC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD9C0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FD9E4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FDA04_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FDA64_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FE31C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_801FE370_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80202A00_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80202A20_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80202D04_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802038D0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80203B4C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80203DAC_ovl_00);

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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80203EEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80203F30_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80204104_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802041C8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80204210_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020439C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80204460_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80204568_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802045BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020466C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80204910_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802054CC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802055F8_ovl_00);

/* Always FALSE; maybe once a debug check */
s32 func_8020561C_ovl_00(s32 arg0) {
    return FALSE;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80205624_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802059C0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80205AE4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80205BB8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80205C78_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80205D44_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80205DE8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80206354_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80206BD0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80206CC8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80206CF0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80206E88_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207014_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207048_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207080_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802071B8_ovl_00);

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

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207388_ovl_00);

void func_80207660_ovl_00(UnkB8 *player) {
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207668_ovl_00);

/* Hides the rings the ball passed, so that a new shot can pass them again */
void ClearPassedRings(UnkB8 *player) {
    s32 i;

    for (i = 0; i < RING_COUNT; i++) {
        if (player->ringStates[i] >= RING_STATE_PASSED) {
            player->ringStates[i] = RING_STATE_NONE;
        }
    }
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802078F4_ovl_00);

void FreeCharacterAndReactionLight(void) {
    FreeCharacterSlot(0);
    TurnOffReactionLight();
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207A34_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207B8C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207BEC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207CAC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207E88_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80207F70_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802080B4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802080D4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80208120_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020816C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802081F8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80208354_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020845C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802087D0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80208810_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80208850_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802088B0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020D6F0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020D784_ovl_00);

/* The yaw the camera looks along, or 0 when it looks straight up or down */
f32 GetCameraYaw(void) {
    f32 x = g_Cameras->view.lookAt[0] - g_Cameras->view.eye[0];
    f32 yaw = 0.0f;
    f32 z = g_Cameras->view.lookAt[2] - g_Cameras->view.eye[2];

    if ((s32)x != 0 || (s32)z != 0) {
        yaw = atan2f(x, z);
    }
    return yaw;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020D838_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020D874_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020D9F0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020DA18_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020DCA4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020E740_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020E78C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020E7B4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020EBBC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020ED54_ovl_00);

void func_8020EEEC_ovl_00(Gfx **displayListHead) {
    Gfx *gfx = *displayListHead;

    *displayListHead = gfx;
}

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020EEF8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020FAE8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8020FC10_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", IsMovieFinished);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", StartMovie);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021035C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80210370_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802104BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80210F40_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021169C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80211D60_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80212590_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802129C0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80212CE0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80213038_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80213634_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80213C64_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80213C78_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80213D70_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021419C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80214250_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216130_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802161D4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216B10_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216B74_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216C18_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216C58_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216CAC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216EDC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216F8C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216FC4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80216FCC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80217164_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80217F44_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80217F4C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80217F54_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80217F5C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80218238_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021857C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80218CC0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80218D1C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80218DB0_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225440_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225450_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_8022545C_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225468_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225474_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225480_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225484_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225488_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225490_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_80225498_ovl_00);

INCLUDE_RODATA("asm/nonmatchings/ovl_00/round", D_802254A0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80218DD0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80218FDC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80219034_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_802199EC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_80219C74_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021A62C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021A690_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021A6BC_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021A724_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021A940_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021B030_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021B52C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021B79C_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021C6B4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021DED0_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021E090_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021E550_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021E5F4_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021E6B8_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021E710_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021E724_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021EA48_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021EA58_ovl_00);

INCLUDE_ASM("asm/nonmatchings/ovl_00/round", func_8021FC20_ovl_00);

