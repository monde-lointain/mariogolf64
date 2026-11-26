/*
 * Tests of the frame recorder's batching: how draws merge and passes split.
 */

#include <SDL3/SDL_stdinc.h>

#include "frame.h"
#include "test_support.h"

// Distinct addresses to stand for targets, which the recorder never reads
static char f_TargetA;
static char f_TargetB;
static char f_Depth;
static char f_OtherDepth;

#define TARGET_A ((GfxTarget *)&f_TargetA)
#define TARGET_B ((GfxTarget *)&f_TargetB)
#define DEPTH ((GfxTarget *)&f_Depth)
#define OTHER_DEPTH ((GfxTarget *)&f_OtherDepth)

static GfxFrame f_Frame;
static const GfxRect f_FullRect = {0, 0, 320, 240};
static const GfxRect f_HalfRect = {0, 0, 160, 240};

// Vertices with the same pipeline key and scissor extend one draw; a change
// of either starts another
static void TestDrawsMerge(void) {
    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    AddFrameVertices(&f_Frame, 3, 0, &f_FullRect);
    AddFrameVertices(&f_Frame, 6, 0, &f_FullRect);
    EXPECT(f_Frame.drawCount == 1 && f_Frame.draws[0].vertexCount == 9);

    AddFrameVertices(&f_Frame, 3, PIPELINE_BLEND, &f_FullRect);
    AddFrameVertices(&f_Frame, 3, PIPELINE_BLEND, &f_HalfRect);
    EXPECT(f_Frame.drawCount == 3);
    EXPECT(f_Frame.draws[2].firstVertex == 12);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].drawCount == 3);
}

// A new target starts a pass, and draws don't merge across passes
static void TestPassPerTarget(void) {
    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    AddFrameVertices(&f_Frame, 3, 0, &f_FullRect);
    BeginFramePass(&f_Frame, TARGET_A);
    EXPECT(f_Frame.passCount == 1);

    BeginFramePass(&f_Frame, TARGET_B);
    AddFrameVertices(&f_Frame, 3, 0, &f_FullRect);
    EXPECT(f_Frame.passCount == 2);
    EXPECT(f_Frame.drawCount == 2);
    EXPECT(f_Frame.passes[1].firstDraw == 1 && f_Frame.passes[1].drawCount == 1);
}

// Depth joins the current pass unless the pass has another depth target, or
// a clear would wipe what its draws so far have written
static void TestDepthSplitsPasses(void) {
    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    UseFrameDepth(&f_Frame, DEPTH, 1.0f);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].clearsDepth);

    AddFrameVertices(&f_Frame, 3, PIPELINE_DEPTH_TEST, &f_FullRect);
    UseFrameDepth(&f_Frame, DEPTH, -1.0f);
    EXPECT(f_Frame.passCount == 1);

    UseFrameDepth(&f_Frame, DEPTH, 1.0f);
    EXPECT(f_Frame.passCount == 2 && f_Frame.passes[1].target == TARGET_A);
    EXPECT(f_Frame.passes[1].clearsDepth);

    AddFrameVertices(&f_Frame, 3, PIPELINE_DEPTH_TEST, &f_FullRect);
    UseFrameDepth(&f_Frame, OTHER_DEPTH, -1.0f);
    EXPECT(f_Frame.passCount == 3 && !f_Frame.passes[2].clearsDepth);
    EXPECT(f_Frame.passes[2].depth == OTHER_DEPTH);
}

// An equal material is stored once; a different one gets the next index
static void TestMaterialsShared(void) {
    GfxMaterial material;

    ResetFrame(&f_Frame);
    SDL_zero(material);
    EXPECT(AddFrameMaterial(&f_Frame, &material) == 0);
    EXPECT(AddFrameMaterial(&f_Frame, &material) == 0);
    material.primitiveColor[0] = 1.0f;
    EXPECT(AddFrameMaterial(&f_Frame, &material) == 1);
    EXPECT(f_Frame.materialCount == 2);
}

// Uploads take their texels in order from the frame's texel buffer
static void TestTextureUploads(void) {
    uint32_t *first;
    uint32_t *second;

    ResetFrame(&f_Frame);
    first = AddFrameTextureUpload(&f_Frame, 0, 0, 0, 8, 4);
    second = AddFrameTextureUpload(&f_Frame, 1, 8, 0, 4, 4);
    EXPECT(second == first + 32);
    EXPECT(f_Frame.uploadCount == 2 && f_Frame.texelCount == 48);
    EXPECT(f_Frame.uploads[1].texelOffset == 32 && f_Frame.uploads[1].layer == 1);
    EXPECT(HasFrameRoom(&f_Frame, FRAME_VERTEX_CAPACITY, 0));
    EXPECT(!HasFrameRoom(&f_Frame, 0, FRAME_TEXEL_CAPACITY));
}

// A strip of six rows from source, at the same place in both targets
static GfxCopy Strip(GfxTarget *source, uint16_t top) {
    GfxCopy copy;

    SDL_zero(copy);
    copy.source = source;
    copy.sourceX = 8;
    copy.sourceY = top;
    copy.x = 8;
    copy.y = top;
    copy.width = 304;
    copy.height = 6;
    return copy;
}

// A copy goes before the draws of its pass, so one that follows a draw
// starts another pass over the same target, and lengthens no copy of the
// pass before; draws may follow copies
static void TestCopiesPrecedeDraws(void) {
    GfxCopy copy = Strip(TARGET_B, 8);

    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    AddFrameCopy(&f_Frame, TARGET_A, &copy);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].copyCount == 1);
    EXPECT(f_Frame.passes[0].firstCopy == 0 && f_Frame.copyCount == 1);
    EXPECT(f_Frame.copies[0].source == TARGET_B && f_Frame.copies[0].height == 6);

    AddFrameVertices(&f_Frame, 3, 0, &f_FullRect);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].drawCount == 1);

    // Even carrying on the first copy's rows, which it mustn't join
    copy = Strip(TARGET_B, 14);
    AddFrameCopy(&f_Frame, TARGET_A, &copy);
    EXPECT(f_Frame.passCount == 2 && f_Frame.passes[1].target == TARGET_A);
    EXPECT(f_Frame.passes[1].firstCopy == 1 && f_Frame.passes[1].copyCount == 1);
    EXPECT(f_Frame.passes[1].drawCount == 0 && f_Frame.copies[0].height == 6);
}

// A copy into another target than the current pass's starts a pass, as a
// draw does, and so does one with no pass before it
static void TestCopyStartsItsTargetsPass(void) {
    GfxCopy copy = Strip(TARGET_A, 8);

    ResetFrame(&f_Frame);
    AddFrameCopy(&f_Frame, TARGET_B, &copy);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].target == TARGET_B);
    EXPECT(f_Frame.passes[0].depth == NULL && f_Frame.passes[0].copyCount == 1);

    copy = Strip(TARGET_B, 8);
    AddFrameCopy(&f_Frame, TARGET_A, &copy);
    EXPECT(f_Frame.passCount == 2 && f_Frame.passes[1].target == TARGET_A);
    EXPECT(f_Frame.passes[1].firstCopy == 1);
}

// The game copies an image a strip of rows at a time: a strip that carries
// on where the last copy of the pass ended, in both targets, lengthens it
static void TestCopiedStripsJoin(void) {
    GfxCopy copy = Strip(TARGET_B, 8);

    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    AddFrameCopy(&f_Frame, TARGET_A, &copy);
    copy = Strip(TARGET_B, 14);
    AddFrameCopy(&f_Frame, TARGET_A, &copy);
    copy = Strip(TARGET_B, 20);
    copy.height = 2;
    AddFrameCopy(&f_Frame, TARGET_A, &copy);
    EXPECT(f_Frame.copyCount == 1 && f_Frame.passes[0].copyCount == 1);
    EXPECT(f_Frame.copies[0].sourceY == 8 && f_Frame.copies[0].y == 8);
    EXPECT(f_Frame.copies[0].height == 14 && f_Frame.copies[0].width == 304);
}

// Strips that don't carry on the last copy stay apart: a gap or an overlap
// of rows, other columns, another source, or other rows in either target
static void TestCopiesThatDontAdjoinStayApart(void) {
    GfxCopy copies[8];
    int i;

    for (i = 0; i < 8; i++) {
        copies[i] = Strip(TARGET_B, 8);
    }

    copies[1] = Strip(TARGET_B, 15); // A gap
    copies[2] = Strip(TARGET_B, 13); // An overlap
    copies[3] = Strip(TARGET_B, 14);
    copies[3].sourceX = 9;
    copies[4] = Strip(TARGET_B, 14);
    copies[4].x = 9;
    copies[5] = Strip(TARGET_B, 14);
    copies[5].width = 300;
    copies[6] = Strip(OTHER_DEPTH, 14);
    copies[7] = Strip(TARGET_B, 14);
    copies[7].y = 30;
    for (i = 1; i < 8; i++) {
        ResetFrame(&f_Frame);
        BeginFramePass(&f_Frame, TARGET_A);
        AddFrameCopy(&f_Frame, TARGET_A, &copies[0]);
        AddFrameCopy(&f_Frame, TARGET_A, &copies[i]);
        EXPECT(f_Frame.copyCount == 2 && f_Frame.passes[0].copyCount == 2);
        EXPECT(f_Frame.copies[0].height == 6);
    }

    // Only the source's rows lead on
    ResetFrame(&f_Frame);
    copies[1] = Strip(TARGET_B, 14);
    copies[1].sourceY = 30;
    AddFrameCopy(&f_Frame, TARGET_A, &copies[0]);
    AddFrameCopy(&f_Frame, TARGET_A, &copies[1]);
    EXPECT(f_Frame.copyCount == 2);
}

// A frame with no room for another copy needs flushing
static void TestCopiesFillTheFrame(void) {
    uint16_t i;

    ResetFrame(&f_Frame);
    for (i = 0; i < FRAME_COPY_CAPACITY - 1; i++) {
        GfxCopy copy = Strip(TARGET_B, (uint16_t)(i * 7));

        AddFrameCopy(&f_Frame, TARGET_A, &copy);
    }

    EXPECT(f_Frame.copyCount == FRAME_COPY_CAPACITY - 1);
    EXPECT(HasFrameRoom(&f_Frame, 0, 0));
    {
        GfxCopy copy = Strip(TARGET_B, 0);

        AddFrameCopy(&f_Frame, TARGET_A, &copy);
    }

    EXPECT(!HasFrameRoom(&f_Frame, 0, 0));
}

// A depth copy goes into the pass's depth ahead of its draws, as a color
// copy goes into its target, so one that follows a draw starts another
// pass over the same target and depth. A color copy joins the depth copies
// of a pass without draws.
static void TestDepthCopiesPrecedeDraws(void) {
    GfxCopy depthCopy = Strip(OTHER_DEPTH, 8);
    GfxCopy colorCopy = Strip(TARGET_B, 8);

    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    UseFrameDepth(&f_Frame, DEPTH, -1.0f);
    AddFrameDepthCopy(&f_Frame, &depthCopy);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].copyCount == 1);
    EXPECT(f_Frame.passes[0].copiesDepth);
    EXPECT(f_Frame.copyCount == 1 && f_Frame.copies[0].source == OTHER_DEPTH);
    EXPECT(f_Frame.copies[0].y == 8 && f_Frame.copies[0].height == 6);

    AddFrameVertices(&f_Frame, 3, PIPELINE_DEPTH_TEST, &f_FullRect);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].drawCount == 1);

    depthCopy = Strip(OTHER_DEPTH, 14);
    AddFrameDepthCopy(&f_Frame, &depthCopy);
    EXPECT(f_Frame.passCount == 2);
    EXPECT(f_Frame.passes[1].target == TARGET_A && f_Frame.passes[1].depth == DEPTH);
    EXPECT(!f_Frame.passes[1].clearsDepth && f_Frame.passes[1].copiesDepth);
    EXPECT(f_Frame.passes[1].firstCopy == 1 && f_Frame.passes[1].copyCount == 1);
    EXPECT(f_Frame.passes[1].firstDraw == 1 && f_Frame.passes[1].drawCount == 0);

    AddFrameCopy(&f_Frame, TARGET_A, &colorCopy);
    EXPECT(f_Frame.passCount == 2 && f_Frame.passes[1].copyCount == 2);
    EXPECT(f_Frame.copies[2].source == TARGET_B);
}

// A pass's copies come before its depth's clear, so a pass never both
// clears its depth and copies into it: a depth copy after a clear, or a
// clear after a depth copy, starts another pass. A clear after a color
// copy stays in its pass, as the two write different targets.
static void TestDepthCopiesFollowClears(void) {
    GfxCopy depthCopy = Strip(OTHER_DEPTH, 8);
    GfxCopy colorCopy = Strip(TARGET_B, 8);

    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    UseFrameDepth(&f_Frame, DEPTH, 1.0f);
    AddFrameDepthCopy(&f_Frame, &depthCopy);
    EXPECT(f_Frame.passCount == 2);
    EXPECT(f_Frame.passes[0].clearsDepth && f_Frame.passes[0].depthClearValue == 1.0f);
    EXPECT(f_Frame.passes[0].copyCount == 0 && !f_Frame.passes[0].copiesDepth);
    EXPECT(!f_Frame.passes[1].clearsDepth && f_Frame.passes[1].copiesDepth);
    EXPECT(f_Frame.passes[1].depth == DEPTH && f_Frame.passes[1].copyCount == 1);

    UseFrameDepth(&f_Frame, DEPTH, 0.5f);
    EXPECT(f_Frame.passCount == 3 && f_Frame.passes[2].clearsDepth);
    EXPECT(f_Frame.passes[2].depthClearValue == 0.5f && f_Frame.passes[2].copyCount == 0);
    EXPECT(!f_Frame.passes[1].clearsDepth);

    ResetFrame(&f_Frame);
    BeginFramePass(&f_Frame, TARGET_A);
    AddFrameCopy(&f_Frame, TARGET_A, &colorCopy);
    UseFrameDepth(&f_Frame, DEPTH, 1.0f);
    EXPECT(f_Frame.passCount == 1 && f_Frame.passes[0].clearsDepth);
    EXPECT(f_Frame.passes[0].copyCount == 1 && !f_Frame.passes[0].copiesDepth);
}

int main(void) {
    InitFrame(&f_Frame);
    TestDrawsMerge();
    TestPassPerTarget();
    TestDepthSplitsPasses();
    TestMaterialsShared();
    TestTextureUploads();
    TestCopiesPrecedeDraws();
    TestCopyStartsItsTargetsPass();
    TestCopiedStripsJoin();
    TestCopiesThatDontAdjoinStayApart();
    TestCopiesFillTheFrame();
    TestDepthCopiesPrecedeDraws();
    TestDepthCopiesFollowClears();
    return ReportTestResults("frame_test");
}
