#version 450

// Copies depth from another depth target, the game's copy of memory that
// the GPU drew as a depth image into memory used as one. The texture
// coordinates are the source's pixels at the N64's resolution, so at the
// target's scale each fragment reads the source's pixel at the same place
// within the N64's pixel, keeping detail finer than it.

layout(set = 2, binding = 0) uniform sampler2D sourceDepth;

layout(set = 3, binding = 0) uniform FrameConstants {
    uint noiseSeed;
    uint scale;
};

// The texture coordinates over w, then w's reciprocal, which is 1 here
layout(location = 0) noperspective in vec3 inTexCoord;

void main() {
    ivec2 texel = ivec2(floor(inTexCoord.xy * float(scale)));

    gl_FragDepth = texelFetch(sourceDepth, texel, 0).r;
}
