/*
 * What the interpreter records for the GPU between two flushes: the
 * vertices of every triangle in one stream, the table of materials that the
 * triangles' fragments read, the texels of new textures for the atlas, and
 * the draws, grouped into passes over the color images that they render to.
 *
 * Everything that varies from one triangle to the next without changing
 * the GPU's fixed-function state lives in the material table, so a draw
 * covers every triangle between two changes of blending, depth or scissor.
 */

#ifndef PORT_GFX_FRAME_H
#define PORT_GFX_FRAME_H

#include <stdbool.h>
#include <stdint.h>

// The layout of a vertex in the GPU's vertex buffer
typedef struct GfxVertex {
    float position[4]; // Clip space over the color image
    float texCoord[2]; // Texels, before the tile's shift and offset
    uint32_t color; // The shade color, RGBA8 with red lowest
    uint32_t material; // An index into the frame's materials
} GfxVertex;

// How a tile maps texture coordinates to the atlas, matching the shader's
// std430 layout
typedef struct GfxTileSampler {
    float scale[2]; // The shift, as a factor
    float offset[2]; // The tile's upper-left corner, in texels
    int32_t atlasOrigin[2];
    int32_t atlasLayer;
    uint32_t axisModes; // TILE_AXIS_* for s, then the same shifted 8 for t
    int32_t size[2]; // The texels decoded each way, or 0 for a tile not read
    int32_t clampLimit[2];
} GfxTileSampler;

#define TILE_AXIS_CLAMP 0x10
#define TILE_AXIS_MIRROR 0x20
#define TILE_AXIS_MASK_BITS 0x0F

// The combiner's inputs as the shader numbers them, for both color and alpha
typedef enum CombinerInput {
    COMBINER_COMBINED,
    COMBINER_TEXEL0,
    COMBINER_TEXEL1,
    COMBINER_PRIMITIVE,
    COMBINER_SHADE,
    COMBINER_ENVIRONMENT,
    COMBINER_ONE,
    COMBINER_ZERO,
    COMBINER_COMBINED_ALPHA,
    COMBINER_TEXEL0_ALPHA,
    COMBINER_TEXEL1_ALPHA,
    COMBINER_PRIMITIVE_ALPHA,
    COMBINER_SHADE_ALPHA,
    COMBINER_ENVIRONMENT_ALPHA,
    COMBINER_LOD_FRACTION,
    COMBINER_PRIMITIVE_LOD_FRACTION,
    COMBINER_NOISE,
} CombinerInput;

// GfxMaterial.modes
#define MATERIAL_TWO_CYCLE 0x0001
#define MATERIAL_PERSPECTIVE 0x0002
#define MATERIAL_FILTERED 0x0004
#define MATERIAL_ALPHA_THRESHOLD 0x0008
#define MATERIAL_ALPHA_COVERAGE 0x0010
#define MATERIAL_ALPHA_DITHER 0x0020
// The blender's shape: how its last cycle's terms reach the GPU's blending
#define MATERIAL_BLEND_SHIFT 8
#define MATERIAL_BLEND_MASK 0x0700

// The blender's shapes, as the shader composes its output for each
typedef enum BlendShape {
    BLEND_FIRST_ONLY, // No blending: the first color passes
    BLEND_FIRST_OVER_MEMORY, // first * a + memory * b, with b in the GPU
    BLEND_SECOND_OVER_MEMORY, // memory * a + second * b, with a in the GPU
    BLEND_NO_MEMORY, // first * a + second * b, all in the shader
    BLEND_KEEP_MEMORY, // Both colors are memory's
} BlendShape;

typedef struct GfxMaterial {
    // Each word holds a cycle's a, b, c and d selectors, a byte each, as
    // CombinerInputs: color of cycle 0, alpha of cycle 0, then cycle 1's
    uint32_t combine[4];
    float primitiveColor[4];
    float environmentColor[4];
    float fogColor[4];
    float blendColor[4];
    float primitiveLodFraction;
    // Below which a fragment's alpha leaves it uncovered, when
    // MATERIAL_ALPHA_COVERAGE is set
    float coverageThreshold;
    float pad[2];
    uint32_t modes;
    // Each cycle's blender selectors, 2 bits each: first color, its factor,
    // second color, its factor; cycle 1's in the high byte
    uint32_t blender;
    uint32_t pad2[2];
    GfxTileSampler tiles[2];
} GfxMaterial;

// The GPU state that a draw needs, as bits
#define PIPELINE_DEPTH_TEST 0x01
#define PIPELINE_DEPTH_WRITE 0x02
#define PIPELINE_DEPTH_DECAL 0x04
#define PIPELINE_BLEND 0x08
// The destination factor of BLEND_FIRST_OVER_MEMORY and
// BLEND_SECOND_OVER_MEMORY
#define PIPELINE_DESTINATION_SHIFT 4
#define PIPELINE_DESTINATION_MASK 0x70
// Fills and copies write alpha, which holds the coverage of 16-bit images;
// other draws leave it as it was
#define PIPELINE_WRITE_ALPHA 0x80
// Writes depth and no color, as a fill of part of the depth image does
#define PIPELINE_DEPTH_ONLY 0x100
#define PIPELINE_KEY_COUNT 0x200

typedef enum DestinationFactor {
    DESTINATION_ZERO,
    DESTINATION_ONE,
    DESTINATION_ONE_MINUS_SOURCE_ALPHA,
    DESTINATION_SOURCE_ALPHA,
    DESTINATION_DESTINATION_ALPHA,
} DestinationFactor;

// A rectangle of the color image, in its pixels
typedef struct GfxRect {
    uint16_t left;
    uint16_t top;
    uint16_t right; // Exclusive
    uint16_t bottom;
} GfxRect;

typedef struct GfxDraw {
    uint32_t firstVertex;
    uint32_t vertexCount;
    uint16_t pipelineKey;
    GfxRect scissor;
} GfxDraw;

// A color image of the game's, which the GPU renders into
typedef struct GfxTarget GfxTarget;

// Pixels of another target to copy into a pass's, at the same scale: what
// the game copies between its images in the RDP's copy mode
typedef struct GfxCopy {
    GfxTarget *source;
    uint16_t sourceX;
    uint16_t sourceY;
    uint16_t x; // Where they go in the pass's target
    uint16_t y;
    uint16_t width;
    uint16_t height;
} GfxCopy;

// The copies of a pass happen before its draws
typedef struct GfxPass {
    GfxTarget *target;
    GfxTarget *depth; // NULL when no draw tests or writes depth
    bool clearsDepth;
    float depthClearValue;
    uint32_t firstDraw;
    uint32_t drawCount;
    uint32_t firstCopy;
    uint32_t copyCount;
} GfxPass;

// A texture of the atlas to fill from the frame's texels
typedef struct GfxTextureUpload {
    uint32_t texelOffset;
    uint16_t x;
    uint16_t y;
    uint16_t layer;
    uint16_t width;
    uint16_t height;
} GfxTextureUpload;

typedef struct GfxFrame {
    GfxVertex *vertices;
    uint32_t vertexCount;
    GfxMaterial *materials;
    uint32_t materialCount;
    GfxDraw *draws;
    uint32_t drawCount;
    GfxPass *passes;
    uint32_t passCount;
    GfxTextureUpload *uploads;
    uint32_t uploadCount;
    uint32_t *texels;
    uint32_t texelCount;
    GfxCopy *copies;
    uint32_t copyCount;
} GfxFrame;

// How much a frame holds before it must be flushed
#define FRAME_VERTEX_CAPACITY (192 * 1024)
#define FRAME_MATERIAL_CAPACITY (16 * 1024)
#define FRAME_DRAW_CAPACITY (32 * 1024)
#define FRAME_PASS_CAPACITY 256
#define FRAME_UPLOAD_CAPACITY 4096
#define FRAME_TEXEL_CAPACITY (4 * 1024 * 1024)
#define FRAME_COPY_CAPACITY 256

void InitFrame(GfxFrame *frame);
void ResetFrame(GfxFrame *frame);

// Starts a pass over target, unless the frame's last pass is over it and
// can go on
void BeginFramePass(GfxFrame *frame, GfxTarget *target);
// Makes the current pass use depth, clearing it first when clearValue isn't
// negative. A pass can clear only before its first draw, so clearing later
// starts a new pass.
void UseFrameDepth(GfxFrame *frame, GfxTarget *depth, float clearValue);

// Whether the frame has room for so many more of each; when it hasn't, the
// caller flushes it first
bool HasFrameRoom(const GfxFrame *frame, uint32_t vertexCount, uint32_t texelCount);

// Adds a material, or finds it as the last one added, and gives its index
uint32_t AddFrameMaterial(GfxFrame *frame, const GfxMaterial *material);

// Appends vertices to the current pass, extending its last draw when the
// state is the same; returns where they go for the caller to fill
GfxVertex *AddFrameVertices(
    GfxFrame *frame,
    uint32_t vertexCount,
    uint16_t pipelineKey,
    const GfxRect *scissor
);

// Copies pixels into target ahead of any draw into it that follows. A copy
// that carries on the rows of the pass's last copy, from the same source,
// lengthens that copy.
void AddFrameCopy(GfxFrame *frame, GfxTarget *target, const GfxCopy *copy);

// Reserves texels for a texture of the atlas, to be uploaded at its place
uint32_t *AddFrameTextureUpload(
    GfxFrame *frame,
    uint16_t layer,
    uint16_t x,
    uint16_t y,
    uint16_t width,
    uint16_t height
);

#endif
