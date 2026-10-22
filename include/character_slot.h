#ifndef CHARACTER_SLOT_H
#define CHARACTER_SLOT_H

#include <ultra64.h>

#define CHARACTER_MODEL_KIND_COUNT 4
#define CHARACTER_MODELS_PER_KIND 36
#define OTHER_CHARACTER_MODEL_COUNT 18
#define CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT 15

/* Where a character model's files are in ROM and where its parts are in segment 2 */
typedef struct CharacterModel {
    /* 0x00 */ u32 romStart;
    /* 0x04 */ u32 romEnd;
    /* 0x08 */ u32 segmentAddresses[CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT];
    /* 0x44 */ s16 unk44;
    /* 0x46 */ s16 unk46;
    /* The model loads this file first, then patches it with romStart's when the two differ */
    /* 0x48 */ u32 baseRomStart;
    /* 0x4C */ u32 baseRomEnd;
    /* 0x50 */ u32 unk50;
    /* 0x54 */ u32 unk54;
} CharacterModel; /* size = 0x58 */

#define CHARACTER_SLOT_COUNT 4
/* A slot's model index when no character is loaded into it */
#define NO_CHARACTER_MODEL -1

/* A character loaded for drawing */
typedef struct CharacterSlot {
    /* 0x000 */ const CharacterModel *model;
    /* 0x004 */ void *unk4;
    /* 0x008 */ s32 modelIndex;
    /* 0x00C */ u8 unkC[0x34 - 0xC];
    /* 0x034 */ u32 lastDrawnFrame;
    /* 0x038 */ u8 unk38[0x18C - 0x38];
} CharacterSlot; /* size = 0x18C */

extern CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];

extern const CharacterModel g_CharacterModels[CHARACTER_MODEL_KIND_COUNT][CHARACTER_MODELS_PER_KIND];
extern const CharacterModel g_OtherCharacterModels[OTHER_CHARACTER_MODEL_COUNT];

s32 FreeCharacterSlot(u32 slotIndex);
s32 IsCharacterSlotBusy(u32 slotIndex);
s32 IsAnyCharacterSlotBusy(void);
CharacterSlot *GetUsedCharacterSlot(u32 slotIndex);
void FreeAllCharacterSlots(void);

#endif
