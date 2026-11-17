/*
 * Tests of CharacterModelAt, which finds a model by its index across all the
 * kinds of model, a row of g_CharacterModels at a time.
 */

#include <ultra64.h>

#include "characters/character_model.h"
#include "system/c_language.h"
#include "test_support.h"

// A table of its own, linked in place of the game's
const CharacterModel g_CharacterModels[CHARACTER_MODEL_KIND_COUNT][CHARACTER_MODELS_PER_KIND];

static const CharacterModel *FindModel(s32 modelIndex) {
    volatile s32 volatileIndex = modelIndex;

    return CharacterModelAt(volatileIndex);
}

// The first kind's models are its own row
static void TestFindsTheFirstKindsModels(void) {
    EXPECT(FindModel(0) == &g_CharacterModels[0][0]);
    EXPECT(
        FindModel(CHARACTER_MODELS_PER_KIND - 1) ==
        &g_CharacterModels[0][CHARACTER_MODELS_PER_KIND - 1]
    );
}

// The next index after a row starts the next, as 58 and 100 do in the game
static void TestFindsTheLaterKindsModels(void) {
    EXPECT(FindModel(CHARACTER_MODELS_PER_KIND) == &g_CharacterModels[1][0]);
    EXPECT(FindModel(58) == &g_CharacterModels[1][22]);
    EXPECT(FindModel(100) == &g_CharacterModels[2][28]);
    EXPECT(
        FindModel((CHARACTER_MODEL_KIND_COUNT * CHARACTER_MODELS_PER_KIND) - 1) ==
        &g_CharacterModels[CHARACTER_MODEL_KIND_COUNT - 1][CHARACTER_MODELS_PER_KIND - 1]
    );
}

int main(void) {
    TestFindsTheFirstKindsModels();
    TestFindsTheLaterKindsModels();
    return ReportTestResults("character_model_index_test");
}
