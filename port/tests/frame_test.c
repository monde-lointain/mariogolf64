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

int main(void) {
    InitFrame(&f_Frame);
    TestDrawsMerge();
    TestPassPerTarget();
    TestDepthSplitsPasses();
    TestMaterialsShared();
    TestTextureUploads();
    return ReportTestResults("frame_test");
}
