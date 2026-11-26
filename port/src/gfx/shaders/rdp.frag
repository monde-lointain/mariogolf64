#version 450

// The RDP's texture units, color combiner and the blender's terms that don't
// read memory. Every fragment reads its material from the frame's table, so
// one pipeline serves every combination of combiner and textures.
//
// The constants here mirror frame.h.

#define COMBINER_COMBINED 0u
#define COMBINER_TEXEL0 1u
#define COMBINER_TEXEL1 2u
#define COMBINER_PRIMITIVE 3u
#define COMBINER_SHADE 4u
#define COMBINER_ENVIRONMENT 5u
#define COMBINER_ONE 6u
#define COMBINER_ZERO 7u
#define COMBINER_COMBINED_ALPHA 8u
#define COMBINER_TEXEL0_ALPHA 9u
#define COMBINER_TEXEL1_ALPHA 10u
#define COMBINER_PRIMITIVE_ALPHA 11u
#define COMBINER_SHADE_ALPHA 12u
#define COMBINER_ENVIRONMENT_ALPHA 13u
#define COMBINER_LOD_FRACTION 14u
#define COMBINER_PRIMITIVE_LOD_FRACTION 15u
#define COMBINER_NOISE 16u

#define MATERIAL_TWO_CYCLE 0x0001u
#define MATERIAL_FILTERED 0x0004u
#define MATERIAL_ALPHA_THRESHOLD 0x0008u
#define MATERIAL_ALPHA_COVERAGE 0x0010u
#define MATERIAL_ALPHA_DITHER 0x0020u
#define MATERIAL_BLEND_SHIFT 8u
#define MATERIAL_BLEND_MASK 0x0700u

#define BLEND_FIRST_ONLY 0u
#define BLEND_FIRST_OVER_MEMORY 1u
#define BLEND_SECOND_OVER_MEMORY 2u
#define BLEND_NO_MEMORY 3u
#define BLEND_KEEP_MEMORY 4u

// Steps a texel that the RDP counts texture coordinates in, its deltas'
// ten bits of fraction
#define TEXCOORD_GRID 1024.0

#define TILE_AXIS_CLAMP 0x10u
#define TILE_AXIS_MIRROR 0x20u
#define TILE_AXIS_MASK_BITS 0x0Fu

// The blender's selectors
#define BLENDER_COLOR_PIXEL 0u
#define BLENDER_COLOR_BLEND 2u
#define BLENDER_COLOR_FOG 3u
#define BLENDER_ALPHA_PIXEL 0u
#define BLENDER_ALPHA_FOG 1u
#define BLENDER_ALPHA_SHADE 2u
#define BLENDER_FACTOR_ONE_MINUS_A 0u
#define BLENDER_FACTOR_ONE 2u

struct TileSampler {
    vec2 scale;
    vec2 offset;
    ivec2 atlasOrigin;
    int atlasLayer;
    uint axisModes;
    ivec2 size;
    ivec2 clampLimit;
};

struct Material {
    uvec4 combine;
    vec4 primitiveColor;
    vec4 environmentColor;
    vec4 fogColor;
    vec4 blendColor;
    float primitiveLodFraction;
    float coverageThreshold;
    vec2 pad;
    uint modes;
    uint blender;
    uvec2 pad2;
    TileSampler tiles[2];
};

layout(set = 2, binding = 0) uniform sampler2DArray atlas;
layout(std430, set = 2, binding = 1) readonly buffer Materials {
    Material materials[];
};
// As depth_copy.frag has them, which reads the scale
layout(set = 3, binding = 0) uniform FrameConstants {
    uint noiseSeed;
    uint scale;
};

// The texture coordinates over w, then w's reciprocal
layout(location = 0) noperspective in vec3 inTexCoord;
layout(location = 1) noperspective in vec4 inShade;
layout(location = 2) flat in uint inMaterial;

layout(location = 0) out vec4 outColor;

// A random value in [0, 1) for the pixel, new each frame
float Noise() {
    uvec2 pixel = uvec2(gl_FragCoord.xy);
    uint hash = (pixel.x * 0x8DA6B343u) ^ (pixel.y * 0xD8163841u) ^ (noiseSeed * 0xCB1AB31Fu);

    hash ^= hash >> 15;
    hash *= 0x2C1B3C6Du;
    hash ^= hash >> 12;
    return float(hash >> 8) / 16777216.0;
}

// One axis of a texel's coordinate in the tile, as the tile clamps, masks
// and mirrors it
int WrapAxis(int coordinate, uint modes, int size, int clampLimit) {
    uint maskBits = modes & TILE_AXIS_MASK_BITS;

    if ((modes & TILE_AXIS_CLAMP) != 0u) {
        coordinate = clamp(coordinate, 0, clampLimit);
    }

    if (maskBits != 0u) {
        int mask = (1 << maskBits) - 1;
        bool isMirrored = (modes & TILE_AXIS_MIRROR) != 0u && ((coordinate >> maskBits) & 1) != 0;

        coordinate = isMirrored ? mask - (coordinate & mask) : coordinate & mask;
    }

    return clamp(coordinate, 0, size - 1);
}

vec4 FetchTexel(TileSampler tile, ivec2 texel) {
    ivec2 wrapped = ivec2(
        WrapAxis(texel.x, tile.axisModes & 0xFFu, tile.size.x, tile.clampLimit.x),
        WrapAxis(texel.y, tile.axisModes >> 8, tile.size.y, tile.clampLimit.y)
    );

    return texelFetch(atlas, ivec3(tile.atlasOrigin + wrapped, tile.atlasLayer), 0);
}

// The RDP filters between a texel and the next ones, with no half-texel
// offset
vec4 SampleTile(TileSampler tile, vec2 texCoord, bool isFiltered) {
    vec2 coordinate = (texCoord * tile.scale) - tile.offset;
    ivec2 base = ivec2(floor(coordinate));

    // A tile that the material doesn't read has size 0. Wrapping into it
    // would clamp to an empty range, which GLSL leaves undefined, and the
    // fetch could land outside the atlas.
    if (tile.size.x <= 0 || tile.size.y <= 0) {
        return vec4(0.0);
    }

    if (!isFiltered) {
        return FetchTexel(tile, base);
    }

    vec2 fraction = coordinate - floor(coordinate);
    vec4 top = mix(FetchTexel(tile, base), FetchTexel(tile, base + ivec2(1, 0)), fraction.x);
    vec4 bottom = mix(
        FetchTexel(tile, base + ivec2(0, 1)),
        FetchTexel(tile, base + ivec2(1, 1)),
        fraction.x
    );

    return mix(top, bottom, fraction.y);
}

struct CombinerInputs {
    vec4 combined;
    vec4 texel0;
    vec4 texel1;
    vec4 primitive;
    vec4 shade;
    vec4 environment;
    float lodFraction;
    float primitiveLodFraction;
    float noise;
};

vec3 SelectColor(uint selector, CombinerInputs inputs) {
    switch (selector) {
        case COMBINER_COMBINED: return inputs.combined.rgb;
        case COMBINER_TEXEL0: return inputs.texel0.rgb;
        case COMBINER_TEXEL1: return inputs.texel1.rgb;
        case COMBINER_PRIMITIVE: return inputs.primitive.rgb;
        case COMBINER_SHADE: return inputs.shade.rgb;
        case COMBINER_ENVIRONMENT: return inputs.environment.rgb;
        case COMBINER_ONE: return vec3(1.0);
        case COMBINER_COMBINED_ALPHA: return vec3(inputs.combined.a);
        case COMBINER_TEXEL0_ALPHA: return vec3(inputs.texel0.a);
        case COMBINER_TEXEL1_ALPHA: return vec3(inputs.texel1.a);
        case COMBINER_PRIMITIVE_ALPHA: return vec3(inputs.primitive.a);
        case COMBINER_SHADE_ALPHA: return vec3(inputs.shade.a);
        case COMBINER_ENVIRONMENT_ALPHA: return vec3(inputs.environment.a);
        case COMBINER_LOD_FRACTION: return vec3(inputs.lodFraction);
        case COMBINER_PRIMITIVE_LOD_FRACTION: return vec3(inputs.primitiveLodFraction);
        case COMBINER_NOISE: return vec3(inputs.noise);
        default: return vec3(0.0);
    }
}

float SelectAlpha(uint selector, CombinerInputs inputs) {
    switch (selector) {
        case COMBINER_COMBINED: return inputs.combined.a;
        case COMBINER_TEXEL0: return inputs.texel0.a;
        case COMBINER_TEXEL1: return inputs.texel1.a;
        case COMBINER_PRIMITIVE: return inputs.primitive.a;
        case COMBINER_SHADE: return inputs.shade.a;
        case COMBINER_ENVIRONMENT: return inputs.environment.a;
        case COMBINER_ONE: return 1.0;
        case COMBINER_LOD_FRACTION: return inputs.lodFraction;
        case COMBINER_PRIMITIVE_LOD_FRACTION: return inputs.primitiveLodFraction;
        default: return 0.0;
    }
}

// One cycle of the combiner: (a - b) * c + d, for color and for alpha
vec4 CombineCycle(uint colorSelectors, uint alphaSelectors, CombinerInputs inputs) {
    vec3 color = (SelectColor(colorSelectors & 0xFFu, inputs) -
                  SelectColor((colorSelectors >> 8) & 0xFFu, inputs)) *
            SelectColor((colorSelectors >> 16) & 0xFFu, inputs) +
        SelectColor(colorSelectors >> 24, inputs);
    float alpha = (SelectAlpha(alphaSelectors & 0xFFu, inputs) -
                   SelectAlpha((alphaSelectors >> 8) & 0xFFu, inputs)) *
            SelectAlpha((alphaSelectors >> 16) & 0xFFu, inputs) +
        SelectAlpha(alphaSelectors >> 24, inputs);

    return clamp(vec4(color, alpha), 0.0, 1.0);
}

vec3 BlenderColor(uint selector, vec3 pixel, Material material) {
    switch (selector) {
        case BLENDER_COLOR_BLEND: return material.blendColor.rgb;
        case BLENDER_COLOR_FOG: return material.fogColor.rgb;
        // Memory, where the shader sees it, is taken as the pixel
        default: return pixel;
    }
}

float BlenderAlpha(uint selector, float pixelAlpha, Material material) {
    switch (selector) {
        case BLENDER_ALPHA_PIXEL: return pixelAlpha;
        case BLENDER_ALPHA_FOG: return material.fogColor.a;
        case BLENDER_ALPHA_SHADE: return inShade.a;
        default: return 0.0;
    }
}

float BlenderFactor(uint selector, float alpha) {
    switch (selector) {
        case BLENDER_FACTOR_ONE_MINUS_A: return 1.0 - alpha;
        case BLENDER_FACTOR_ONE: return 1.0;
        // Memory's alpha, where the shader sees it, is taken as opaque
        case 1u: return 1.0;
        default: return 0.0;
    }
}

// A cycle of the blender whose inputs are all in the shader
vec3 BlendCycle(uint selectors, vec3 pixel, float pixelAlpha, Material material) {
    vec3 first = BlenderColor(selectors & 3u, pixel, material);
    float alpha = BlenderAlpha((selectors >> 2) & 3u, pixelAlpha, material);
    vec3 second = BlenderColor((selectors >> 4) & 3u, pixel, material);
    float factor = BlenderFactor((selectors >> 6) & 3u, alpha);

    return clamp((first * alpha) + (second * factor), 0.0, 1.0);
}

void main() {
    Material material = materials[inMaterial];
    bool isTwoCycle = (material.modes & MATERIAL_TWO_CYCLE) != 0u;
    bool isFiltered = (material.modes & MATERIAL_FILTERED) != 0u;
    // The RDP samples a pixel's texture at its top left corner, where the
    // host interpolates at its center. Upscaled, each target pixel does so.
    vec2 centerTexCoord = inTexCoord.xy / inTexCoord.z;
    vec2 cornerTexCoord = centerTexCoord - (0.5 * (dFdx(centerTexCoord) + dFdy(centerTexCoord)));
    // A corner often falls on a texel's edge, where the host's rounding
    // would take the texel before as often as the one after. Snapping it
    // to the RDP's steps puts it where the RDP's exact arithmetic would.
    vec2 texCoord = round(cornerTexCoord * TEXCOORD_GRID) / TEXCOORD_GRID;
    CombinerInputs inputs;

    inputs.combined = vec4(0.0);
    inputs.texel0 = SampleTile(material.tiles[0], texCoord, isFiltered);
    inputs.texel1 = isTwoCycle ? SampleTile(material.tiles[1], texCoord, isFiltered) : inputs.texel0;
    inputs.primitive = material.primitiveColor;
    inputs.shade = inShade;
    inputs.environment = material.environmentColor;
    inputs.lodFraction = 0.0;
    inputs.primitiveLodFraction = material.primitiveLodFraction;
    inputs.noise = Noise();

    // One-cycle mode runs the second cycle's settings alone. In two, the
    // texels move down the pipeline between the cycles, so the second
    // cycle's TEXEL0 is the first's TEXEL1 and the reverse.
    if (isTwoCycle) {
        vec4 firstTexel = inputs.texel0;

        inputs.combined = CombineCycle(material.combine.x, material.combine.y, inputs);
        inputs.texel0 = inputs.texel1;
        inputs.texel1 = firstTexel;
    }

    vec4 combined = CombineCycle(material.combine.z, material.combine.w, inputs);

    // The alpha compare tests the first cycle's alpha
    if ((material.modes & MATERIAL_ALPHA_THRESHOLD) != 0u) {
        float threshold = (material.modes & MATERIAL_ALPHA_DITHER) != 0u ? Noise() :
                                                                         material.blendColor.a;
        float comparedAlpha = isTwoCycle ? inputs.combined.a : combined.a;

        if (comparedAlpha < threshold) {
            discard;
        }
    }

    if ((material.modes & MATERIAL_ALPHA_COVERAGE) != 0u &&
        combined.a < material.coverageThreshold) {
        discard;
    }

    vec3 pixel = combined.rgb;
    uint lastCycle = material.blender & 0xFFu;

    if (isTwoCycle) {
        pixel = BlendCycle(material.blender & 0xFFu, pixel, combined.a, material);
        lastCycle = material.blender >> 8;
    }

    // The GPU adds memory times its factor to what the shader gives, so
    // the shader gives its color already times its own factor, and the
    // factor that memory's color takes as alpha
    float alpha = BlenderAlpha((lastCycle >> 2) & 3u, combined.a, material);
    float secondFactor = BlenderFactor(lastCycle >> 6, alpha);

    switch ((material.modes & MATERIAL_BLEND_MASK) >> MATERIAL_BLEND_SHIFT) {
        case BLEND_FIRST_OVER_MEMORY:
            outColor = vec4(BlenderColor(lastCycle & 3u, pixel, material) * alpha, alpha);
            break;
        case BLEND_SECOND_OVER_MEMORY:
            outColor = vec4(
                BlenderColor((lastCycle >> 4) & 3u, pixel, material) * secondFactor,
                alpha
            );
            break;
        case BLEND_NO_MEMORY:
            outColor = vec4(BlendCycle(lastCycle, pixel, combined.a, material), 1.0);
            break;
        case BLEND_KEEP_MEMORY:
            outColor = vec4(0.0);
            break;
        default:
            outColor = vec4(pixel, combined.a);
            break;
    }
}
