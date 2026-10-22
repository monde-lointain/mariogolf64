#ifndef CHARACTER_SLOT_H
#define CHARACTER_SLOT_H

#include <ultra64.h>
#include "camera.h"

#define CHARACTER_MODEL_KIND_COUNT 4
#define CHARACTER_MODELS_PER_KIND 36
#define OTHER_CHARACTER_MODEL_COUNT 18
#define CHARACTER_MODEL_SEGMENT_ADDRESS_COUNT 15

/* What some of a CharacterModel's segment addresses hold */
#define CHARACTER_MODEL_CHOSEN_DISPLAY_LIST 1 /* A command calling one of the display list choices */
#define CHARACTER_MODEL_DISPLAY_LIST_CHOICES 2 /* Ended by END_OF_DISPLAY_LIST_CHOICES */
#define CHARACTER_MODEL_POINTS 4 /* CharacterPoints, ended by one with no joint path */

#define END_OF_DISPLAY_LIST_CHOICES -1
/* The display list choice that draws nothing */
#define NO_DISPLAY_LIST_CHOICE -1

/* Where SetMultiTlb maps a slot's model buffer, in TLB_PAGE_MODE_4K pages */
#define CHARACTER_MODEL_ADDRESS 0x02000000
#define CHARACTER_MODEL_PAGE_COUNT 32
#define TLB_PAGE_MODE_4K 0

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

/* A point on a character model, found by moving through a path of joints */
typedef struct CharacterPoint {
    /* 0x00 */ s16 *jointPath; /* Joint matrix indices, ended by END_OF_JOINT_PATH */
    /* 0x04 */ f32 offset[3];  /* From the last joint */
    /* 0x10 */ s16 id;
} CharacterPoint; /* size = 0x14 */

#define END_OF_JOINT_PATH -1

/* A character loaded for drawing */
typedef struct CharacterSlot {
    /* 0x000 */ const CharacterModel *model;
    /* 0x004 */ void *modelBuffer; /* Where the model's files are loaded */
    /* 0x008 */ s32 modelIndex;
    /* 0x00C */ u8 unkC[0x2C - 0xC];
    /* 0x02C */ Mtx *jointMatrices;
    /* 0x030 */ u8 unk30[0x34 - 0x30];
    /* 0x034 */ u32 lastDrawnFrame;
    /* 0x038 */ f32 position[3]; /* In 1024ths of a world unit */
    /* 0x044 */ f32 rotation[3]; /* In radians */
    /* 0x050 */ f32 scale[3];
    /* 0x05C */ u8 unk5C[0x18C - 0x5C];
} CharacterSlot; /* size = 0x18C */

extern CharacterSlot g_CharacterSlots[CHARACTER_SLOT_COUNT];

/* Display lists drawn before the characters, rebuilt by SetCharacterAlpha and SetCharacterHilite */
extern Gfx g_CharacterRenderModeDisplayList[3];
extern Gfx g_CharacterCombineDisplayList[2];
extern Gfx g_CharacterHiliteDisplayList[3];

/* The model index of the slot last set up or asked about */
extern s32 g_CharacterModelIndex;

extern const CharacterModel g_CharacterModels[CHARACTER_MODEL_KIND_COUNT][CHARACTER_MODELS_PER_KIND];
extern const CharacterModel g_OtherCharacterModels[OTHER_CHARACTER_MODEL_COUNT];

s32 FreeCharacterSlot(u32 slotIndex);
s32 IsCharacterSlotBusy(u32 slotIndex);
s32 IsAnyCharacterSlotBusy(void);
CharacterSlot *GetUsedCharacterSlot(u32 slotIndex);
void FreeAllCharacterSlots(void);
s32 SetMultiTlb(s32 firstTlbIndex, s32 pageMode, u32 pageCount, u32 virtualAddress, u32 physicalAddress);
s32 SelectCharacterDisplayList(u32 slotIndex, s32 choiceIndex);
s32 GetCharacterClubKind(u32 slotIndex, s32 clubIndex);
s32 GetCharacterPointModelPosition(u32 slotIndex, s32 pointId, f32 *position);
void GetCharacterSlotMatrix(CharacterSlot *slot, Mat4 matrix);
s32 GetCharacterPointPosition(u32 slotIndex, s32 pointId, f32 *position);

#endif
